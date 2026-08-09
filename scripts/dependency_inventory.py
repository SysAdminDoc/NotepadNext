#!/usr/bin/env python3
"""Validate pinned dependencies and emit a small SPDX-style SBOM.

The normal --check path is offline and verifies the checked-in manifest against
the source cache populated by CMake.  --check-updates and --refresh-advisories
are explicit network operations for maintenance or CI security gates.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
SHA256_RE = re.compile(r"^[0-9a-f]{40}$")
VERSION_RE = re.compile(r"(?<!\d)(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:[-+.]|$)")


def fail(message: str, failures: list[str]) -> None:
    failures.append(message)
    print(f"ERROR: {message}", file=sys.stderr)


def run_git(source: Path, *arguments: str) -> str | None:
    try:
        result = subprocess.run(
            ["git", "-C", str(source), *arguments],
            check=True,
            capture_output=True,
            text=True,
            encoding="utf-8",
        )
    except (OSError, subprocess.CalledProcessError):
        return None
    return result.stdout.strip()


def is_git_checkout(path: Path) -> bool:
    if not path.is_dir():
        return False
    top_level = run_git(path, "rev-parse", "--show-toplevel")
    return bool(top_level) and Path(top_level).resolve() == path.resolve()


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_tree_hash(path: Path) -> str:
    if path.is_file():
        return file_hash(path)

    digest = hashlib.sha256()
    for directory, child_directories, file_names in os.walk(path):
        child_directories[:] = sorted(directory for directory in child_directories if directory != ".git")
        for file_name in sorted(file_names):
            child = Path(directory) / file_name
            relative = child.relative_to(path).as_posix().encode("utf-8")
            digest.update(relative)
            digest.update(b"\0")
            with child.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
    return digest.hexdigest()


def resolve_path(relative: str) -> Path:
    path = (ROOT / relative).resolve()
    try:
        path.relative_to(ROOT)
    except ValueError as exc:
        raise ValueError(f"path escapes repository: {relative}") from exc
    return path


def locate_source(manifest_path: Path, expected_commit: str | None) -> Path:
    """Resolve a CPM package root or a specific source-cache checkout."""
    if not manifest_path.is_dir() or is_git_checkout(manifest_path):
        return manifest_path
    if expected_commit:
        for candidate in sorted(manifest_path.iterdir()):
            if is_git_checkout(candidate) and run_git(candidate, "rev-parse", "HEAD") == expected_commit:
                return candidate
    return manifest_path


def load_manifest(path: Path) -> dict[str, Any]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ValueError(f"unable to read dependency manifest {path}: {exc}") from exc
    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        raise ValueError("dependency manifest schema_version must be 1")
    if not isinstance(manifest.get("dependencies"), list):
        raise ValueError("dependency manifest must contain a dependencies list")
    return manifest


def cmake_declarations() -> dict[str, str]:
    text = (ROOT / "thirdparty" / "CMakeLists.txt").read_text(encoding="utf-8")
    declarations: dict[str, str] = {}
    for block in re.findall(r"CPMAddPackage\((.*?)\)", text, flags=re.DOTALL):
        name_match = re.search(r"\bNAME\s+([^\s\)]+)", block)
        tag_match = re.search(r"\bGIT_TAG\s+([^\s\)]+)", block)
        if name_match and tag_match:
            declarations[name_match.group(1)] = tag_match.group(1)
    return declarations


def active_dependency(dependency: dict[str, Any], tree_sitter: str) -> bool:
    return dependency.get("feature") != "tree-sitter" or tree_sitter == "on"


def validate_manifest(
    manifest: dict[str, Any],
    tree_sitter: str,
    failures: list[str],
) -> list[dict[str, Any]]:
    dependencies = manifest["dependencies"]
    exceptions = {
        item.get("dependency"): item
        for item in manifest.get("advisory_policy", {}).get("exceptions", [])
        if isinstance(item, dict)
    }
    names: set[str] = set()
    declarations = cmake_declarations()
    inventory: list[dict[str, Any]] = []

    for dependency in dependencies:
        if not isinstance(dependency, dict):
            fail("dependency entries must be objects", failures)
            continue

        name = dependency.get("name")
        if not isinstance(name, str) or not name:
            fail("dependency is missing a name", failures)
            continue
        if name in names:
            fail(f"duplicate dependency: {name}", failures)
        names.add(name)

        for field in ("upstream_url", "revision", "version", "license", "license_file"):
            if not isinstance(dependency.get(field), str) or not dependency[field]:
                fail(f"{name} is missing {field}", failures)
        if not str(dependency.get("upstream_url", "")).startswith(("https://", "http://")):
            fail(f"{name} upstream_url must be an explicit URL", failures)

        revision = str(dependency.get("revision", ""))
        if dependency.get("kind") in ("source", "optional-source") and not (
            SHA256_RE.fullmatch(revision) or revision.startswith("vendored:")
        ):
            fail(f"{name} revision is not an immutable commit or vendored revision: {revision}", failures)

        cmake_name = dependency.get("cmake_name")
        if cmake_name:
            actual_tag = declarations.get(cmake_name)
            if actual_tag != revision:
                fail(
                    f"{name} manifest revision {revision} does not match thirdparty/CMakeLists.txt ({actual_tag})",
                    failures,
                )

        if name not in exceptions and dependency.get("kind") in ("system", "build-tool"):
            fail(f"{name} needs an explicit advisory exception with owner and rationale", failures)
        if name in exceptions and not all(
            isinstance(exceptions[name].get(field), str) and exceptions[name][field].strip()
            for field in ("owner", "rationale")
        ):
            fail(f"{name} advisory exception must name an owner and rationale", failures)

        entry: dict[str, Any] = {
            "name": name,
            "version": dependency.get("version"),
            "revision": revision,
            "upstream_url": dependency.get("upstream_url"),
            "license": dependency.get("license"),
            "kind": dependency.get("kind"),
            "feature": dependency.get("feature"),
            "active": active_dependency(dependency, tree_sitter),
        }

        source_path_value = dependency.get("source_path")
        if source_path_value:
            try:
                manifest_source_path = resolve_path(source_path_value)
            except ValueError as exc:
                fail(f"{name}: {exc}", failures)
                manifest_source_path = ROOT / "__missing_dependency_source__"

            source_path = locate_source(manifest_source_path, dependency.get("resolved_commit"))

            if entry["active"] and not source_path.exists():
                fail(f"{name} source is not populated: {source_path_value}", failures)
            if source_path.exists():
                license_file = dependency.get("license_file", "")
                license_path = ROOT / license_file if source_path.is_file() else source_path / license_file
                if not license_path.exists():
                    fail(f"{name} license file is not present: {license_file}", failures)
                entry["source_hash"] = source_tree_hash(source_path)
                entry["source_path"] = source_path_value
                try:
                    entry["resolved_source_path"] = str(source_path.relative_to(ROOT).as_posix())
                except ValueError:
                    entry["resolved_source_path"] = str(source_path)

                if is_git_checkout(source_path) and SHA256_RE.fullmatch(revision):
                    actual_commit = run_git(source_path, "rev-parse", "HEAD")
                    entry["resolved_commit"] = actual_commit
                    if entry["active"] and actual_commit != dependency.get("resolved_commit"):
                        fail(
                            f"{name} cache commit is {actual_commit}, expected {dependency.get('resolved_commit')}",
                            failures,
                        )
        elif dependency.get("kind") not in ("system",):
            fail(f"{name} must identify a source_path", failures)

        if not entry["active"]:
            entry["status"] = "disabled-by-feature"
        else:
            entry["status"] = "present"
        inventory.append(entry)

    return inventory


def parse_version(value: str) -> tuple[int, int, int] | None:
    match = VERSION_RE.search(value.lstrip("v"))
    if not match:
        return None
    return tuple(int(part or 0) for part in match.groups())


def github_repository(url: str) -> str | None:
    match = re.match(r"https://github\.com/([^/]+/[^/#]+?)(?:\.git)?/?$", url)
    return match.group(1) if match else None


def check_updates(
    manifest: dict[str, Any],
    failures: list[str],
) -> list[dict[str, Any]]:
    report: list[dict[str, Any]] = []
    for dependency in manifest["dependencies"]:
        repository = github_repository(str(dependency.get("upstream_url", "")))
        if not repository:
            continue
        request = urllib.request.Request(
            f"https://api.github.com/repos/{repository}/releases/latest",
            headers={"Accept": "application/vnd.github+json", "User-Agent": "NotepadNext-dependency-gate"},
        )
        item: dict[str, Any] = {"name": dependency["name"], "current": dependency.get("version")}
        try:
            with urllib.request.urlopen(request, timeout=15) as response:
                payload = json.load(response)
            latest = payload.get("tag_name", "")
            item["latest"] = latest
            current_version = parse_version(str(dependency.get("version", "")))
            latest_version = parse_version(str(latest))
            has_post_release_fix = "+" in str(dependency.get("version", ""))
            item["status"] = (
                "outdated"
                if current_version and latest_version and latest_version > current_version and not has_post_release_fix
                else "current-or-unversioned"
            )
            if item["status"] == "outdated":
                print(f"UPDATE: {dependency['name']} {dependency['version']} -> {latest}")
        except urllib.error.HTTPError as exc:
            item["status"] = "no-release-metadata" if exc.code == 404 else "unavailable"
            item["error"] = str(exc)
            if exc.code != 404:
                fail(f"unable to check latest release for {dependency['name']}: {exc}", failures)
        except (OSError, urllib.error.URLError, json.JSONDecodeError) as exc:
            item["status"] = "unavailable"
            item["error"] = str(exc)
            fail(f"unable to check latest release for {dependency['name']}: {exc}", failures)
        report.append(item)
    return report


def refresh_advisories(
    manifest: dict[str, Any],
    inventory: list[dict[str, Any]],
) -> dict[str, Any]:
    exceptions = {
        item.get("dependency"): item
        for item in manifest.get("advisory_policy", {}).get("exceptions", [])
        if isinstance(item, dict)
    }
    results: list[dict[str, Any]] = []
    for dependency, entry in zip(manifest["dependencies"], inventory):
        name = dependency["name"]
        if not entry.get("active"):
            results.append({"name": name, "status": "disabled-by-feature"})
            continue
        if name in exceptions:
            results.append({
                "name": name,
                "status": "manual-exception",
                "owner": exceptions[name]["owner"],
            })
            continue

        commit = entry.get("resolved_commit") or dependency.get("resolved_commit")
        if not commit:
            results.append({"name": name, "status": "manual-review-required"})
            continue

        request = urllib.request.Request(
            "https://api.osv.dev/v1/query",
            data=json.dumps({"commit": commit}).encode("utf-8"),
            headers={"Content-Type": "application/json", "User-Agent": "NotepadNext-dependency-gate"},
            method="POST",
        )
        try:
            with urllib.request.urlopen(request, timeout=20) as response:
                payload = json.load(response)
            vulnerabilities = payload.get("vulns", [])
            results.append({
                "name": name,
                "commit": commit,
                "status": "affected" if vulnerabilities else "clear",
                "vulnerabilities": [item.get("id") for item in vulnerabilities],
            })
        except (OSError, urllib.error.URLError, json.JSONDecodeError) as exc:
            results.append({"name": name, "commit": commit, "status": "unavailable", "error": str(exc)})
    return {
        "provider": "OSV.dev",
        "queried_at": datetime.now(timezone.utc).isoformat(),
        "results": results,
    }


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(payload, indent=2, sort_keys=True) + "\n")


def write_sbom(path: Path, inventory: list[dict[str, Any]]) -> None:
    packages: list[dict[str, Any]] = []
    for index, dependency in enumerate(inventory, start=1):
        package: dict[str, Any] = {
            "SPDXID": f"SPDXRef-Package-{index}",
            "name": dependency["name"],
            "versionInfo": dependency["version"],
            "downloadLocation": dependency["upstream_url"],
            "licenseConcluded": dependency["license"],
            "licenseDeclared": dependency["license"],
            "filesAnalyzed": False,
        }
        if dependency.get("source_hash"):
            package["checksums"] = [{"algorithm": "SHA256", "checksumValue": dependency["source_hash"]}]
        packages.append(package)

    created = "1970-01-01T00:00:00Z"
    if "SOURCE_DATE_EPOCH" in __import__("os").environ:
        created = datetime.fromtimestamp(
            int(__import__("os").environ["SOURCE_DATE_EPOCH"]), timezone.utc
        ).isoformat().replace("+00:00", "Z")
    write_json(path, {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": "NotepadNext dependency SBOM",
        "documentNamespace": "https://github.com/SysAdminDoc/NotepadNext/sbom/dependencies",
        "creationInfo": {
            "created": created,
            "creators": ["Tool: NotepadNext dependency_inventory.py"],
        },
        "packages": packages,
    })


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "thirdparty" / "dependencies.json")
    parser.add_argument("--tree-sitter", choices=("on", "off"), default="on")
    parser.add_argument("--webengine", choices=("on", "off"), default="off")
    parser.add_argument("--check", action="store_true", help="run the offline manifest/cache gate")
    parser.add_argument("--check-updates", action="store_true", help="query GitHub release metadata")
    parser.add_argument("--fail-on-outdated", action="store_true")
    parser.add_argument("--refresh-advisories", action="store_true", help="query OSV by pinned commit")
    parser.add_argument("--security-gate", action="store_true", help="fail on advisory errors or affected pins")
    parser.add_argument("--advisory-report", type=Path)
    parser.add_argument("--output", type=Path, help="write the resolved inventory JSON")
    parser.add_argument("--sbom", type=Path, help="write an SPDX 2.3 JSON SBOM")
    args = parser.parse_args()

    if not any((args.check, args.check_updates, args.refresh_advisories, args.security_gate)):
        parser.error("one of --check, --check-updates, --refresh-advisories, or --security-gate is required")

    failures: list[str] = []
    try:
        manifest = load_manifest(args.manifest.resolve())
        matrix = manifest.get("feature_matrix", {})
        if args.tree_sitter not in matrix.get("tree-sitter", []):
            fail(f"tree-sitter feature state is missing from the manifest matrix: {args.tree_sitter}", failures)
        if args.webengine not in matrix.get("webengine", []):
            fail(f"webengine feature state is missing from the manifest matrix: {args.webengine}", failures)
        inventory = validate_manifest(manifest, args.tree_sitter, failures)
    except (OSError, ValueError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if args.check_updates:
        update_report = check_updates(manifest, failures)
        if args.fail_on_outdated and any(item.get("status") == "outdated" for item in update_report):
            fail("one or more dependencies have newer releases", failures)

    advisory_report: dict[str, Any] | None = None
    if args.refresh_advisories:
        advisory_report = refresh_advisories(manifest, inventory)
        if args.advisory_report:
            write_json(args.advisory_report.resolve(), advisory_report)

    if args.security_gate:
        if advisory_report is None:
            fail("--security-gate requires --refresh-advisories for a current advisory result", failures)
        else:
            for item in advisory_report.get("results", []):
                if item.get("status") in ("affected", "unavailable", "manual-review-required"):
                    fail(f"advisory gate for {item.get('name')} is {item.get('status')}", failures)

    resolved = {
        "schema_version": 1,
        "manifest": str(args.manifest.resolve().relative_to(ROOT)),
        "tree_sitter": args.tree_sitter,
        "webengine": args.webengine,
        "dependencies": inventory,
    }
    if args.output:
        write_json(args.output.resolve(), resolved)
    if args.sbom:
        write_sbom(args.sbom.resolve(), inventory)

    active = sum(1 for item in inventory if item.get("active"))
    print(f"dependency inventory: {active}/{len(inventory)} active; tree-sitter={args.tree_sitter}; webengine={args.webengine}")
    if failures:
        print(f"dependency inventory: {len(failures)} failure(s)", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
