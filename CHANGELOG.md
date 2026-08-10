# Changelog

All notable changes to NotepadNext will be documented in this file.

## [Unreleased]

- Security: Added a dependency provenance/SBOM gate for immutable revisions, license metadata, source hashes, feature matrices, update reporting, and OSV advisory checks; libssh2 now tracks the upstream post-1.11.1 security-fix commit.
- Security: Added workspace-scoped capability trust for JavaScript, Lua, script-file access, document automation, terminal processes, and optional workspace startup Lua, with session/persistent grants, independent revocation, and redacted audit records.
- Security: Windows Notepad++ DLL plugins now require an explicit, persisted SHA-256 trust decision; profile plugins are opt-in and `--safe-mode` disables native plugins for the session.
- Reliability: External file changes now preserve the in-memory document on reload failure, distinguish deleted/restored/conflicted states, and block ordinary saves until the change is resolved explicitly.
- Reliability: SFTP saves now verify remote size/mtime, upload to a same-directory temporary path, validate the transfer, and atomically replace the destination without silently overwriting concurrent edits.
- Reliability: Recursive directory drops now avoid symlinks, de-duplicate canonical paths, enforce depth/file/byte/time limits, and provide cancelable background scanning with skipped-item reporting.
- Reliability: Document, export, and session writes now share checked atomic replacement, preserve previous output on failure, and surface actionable write errors.
- Added: The Encoding menu now exposes UTF-8/16/32 BOM choices and available legacy codecs, marks auto-detected no-BOM files, keeps the status bar synchronized, and refuses lossy saves with an actionable error.
- Added: Text files from 50 MiB through the 128 MiB safety limit now use read-only large-file mode with visible-range decorations and explicit status messages; larger files are rejected with guidance to use the Hex Editor or an external tool.
- Reliability: LSP servers are now shared by workspace root and compatible configuration, track multiple documents, version-gate diagnostics and language replies, cancel superseded requests, time out stalled requests, and expose lifecycle status in the main window.

## [v0.15] - 2026-08-03

- Fixed: Session recovery now commits unsaved buffers through an atomic, generation-based journal with a backward-compatible legacy loader.
- Added: QtTest coverage for session journal round-tripping and buffer-path traversal protection.
- Fixed: File loading now decodes UTF-8/16/32 BOMs and uchardet-detected legacy encodings before sending text to Scintilla, preserving the codec for atomic saves.
- Added: Ctrl+D expands the word under the caret before selecting the next instance, and Alt-click adds a caret without sacrificing Alt-drag rectangular selection.
- Added: Ctrl+Shift+P opens a fuzzy command palette covering enabled menu actions.
- Fixed: Recorded macros now persist edits and shortcuts immediately, reject duplicate names/shortcuts, and restore keyboard actions after restart.
- Fixed: Regex search enumeration and Replace All now make progress through zero-length matches without repeating or hanging.
- Fixed: Regex searches now honor backward ranges, whole-word/word-start boundaries, newline-aware dot matching, and release replacement buffers correctly.
- Added: Headless document-level regex regression coverage for direction, boundaries, newline mode, and capture replacement.
- Fixed: The Qt regex adapter now honors Scintilla's case-sensitivity contract and preserves UTF-8 byte offsets while retaining POSIX character classes.
- Added: Modern language profiles for Nim, Zig, TOML, Svelte, MDX, HCL, and Terraform, backed by the vendored Lexilla lexers and structural fallbacks where no native lexer is available.
- Added: Headless coverage that loads modern language definitions and verifies each configured lexer factory.
- Added: Windows builds now discover Unicode Notepad++ DLL plugins, expose their menu commands, forward Scintilla notifications, and bridge common NPPM path, document, save, and query messages.
- Fixed: Regex search now honors Scintilla's explicit pattern-length contract, clears stale match lengths on failure, and expands Boost/Notepad++-style numbered, named, whole-match, and escaped replacement references.
- Added: Headless regex coverage for explicit pattern lengths, invalid patterns, named and numbered replacement references, and replacement escapes.
- Added: An optional Tree-sitter JSON lexer with parser-backed token styles, pinned MIT runtime/grammar dependencies, and a `NOTEPADNEXT_ENABLE_TREE_SITTER=OFF` minimal-build switch.
- Added: Headless Tree-sitter parser and lexer-style coverage, including a build verification with the feature disabled.
- Added: Fold-aware sticky scroll breadcrumbs that keep the current class/function headers visible while scrolling, with theme-aware elision and split-editor support.
- Added: A pointer-transparent minimap overlay with document-density rendering, viewport highlighting, and unsaved-line diff markers for each editor pane.
- Added: LSP client support for clangd, pyright-langserver, rust-analyzer, and gopls with full-document synchronization, diagnostics squiggles, hover call tips, and F12 go-to-definition navigation.
- Added: Headless JSON-RPC framing and fake-server coverage for LSP initialization, document changes, diagnostics, hover, and definition responses.
- Added: Offline JSON Schema validation for JSON, YAML, and TOML documents, with local schema discovery, explicit Search-menu selection, debounced editor diagnostics, and parser/keyword coverage.
- Added: Live Markdown and MDX preview in a docked split pane, with active-editor tracking, debounced updates, local resource bases, and an optional Qt WebEngine backend.
- Added: Local libgit2 integration with automatic diff markers, file-level stage/unstage actions, and an optional blame gutter for the active document.
- Added: An integrated terminal dock backed by QProcess, with active-document working directories and shell lifecycle controls.
- Added: Asynchronous ripgrep-backed Find in Files with regex/case/hidden-file controls and click-to-open results.
- Added: SFTP remote editing through pinned libssh2 with password/private-key authentication, known-host verification, temporary document mirrors, and remote Save uploads.
- Added: Portable-mode auto-detection for removable launch volumes, with an optional `portable` directory marker and profile-local settings/session data.
- Added: A docked QuickJS-NG JavaScript console with a document automation bridge, script-file execution, output helpers, memory limits, and runaway-script interruption.
- Added: A Regex Builder dock with live match inspection, named and numbered capture-group visualization, sample-text highlighting, and current-document/selection loading.
- Added: Automatic binary-file detection with an editable, atomic-save Hex Editor dock, byte/ASCII inspection, and a 64 MiB safety limit.
- Added: A persisted Snippet Manager with trigger expansion, UTF-8-safe placeholder navigation, and Tab/Shift+Tab movement through `${1:default}` and `${0}` tabstops.
- Added: Accessibility labels, descriptions, explicit focus chains, and keyboard activation paths across the command palette, editor, search, terminal, scripting, inspection, and file-navigation surfaces.
- Added: Persisted Fusion, Material, and Fluent theme variants with immediate View-menu switching, palette/QSS styling, and custom CSS overrides.
- Added: Selectable Default, Nord, Catppuccin, and GitHub Dark icon packs with semantic accent colors and transparency-preserving recoloring.
- Added: Windows custom frameless title bar with native resize hit-testing, window controls, and guarded DWM Mica/rounded-corner attributes.
- Added: Explicit Qt/HarfBuzz programming-font shaping for standard and contextual OpenType ligature features, with a Qt-version compatibility guard.
- Added: An in-tree Homebrew cask mirror for the current macOS release, including release live-checking and deterministic DMG verification.
- Added: An updated CMake-based Flatpak manifest pinned to v0.14, with offline CPM dependency sources, portal file forwarding, and a persistent app-scoped profile.
- Added: Cross-platform “Open with Notepad Next” packaging integration with multi-file Linux/Flatpak forwarding, macOS text-editor registration, and Windows installer coverage checks.
- Fixed: Linux packaging now keeps AppImage-only qmake discovery out of normal and Flatpak CMake configuration.

## [v0.1.0] - %Y->- (HEAD -> master)

- Removed: Remove upstream funding/donation content
- Prepend new search results
- Fixed: Fix MacOS dock icon (#988)
- Changed: Update packaging script for Linux (#986)
- Changed: Update translation files
- Fixed: Fix appimage build
- Fixed: Fix Linux install path
- Added: New Crowdin updates (#967)
- Removed: Remove unneeded parameters
- Removed: Remove HexViewer

## Roadmap archive — 2026-08-10 — ROADMAP.md

<details>
<summary>Original roadmap snapshot</summary>

```markdown
# Roadmap

Cross-platform Qt 6 reimplementation of Notepad++ using Scintilla. GPL-3.0. Roadmap captures fork-owner priorities for stabilization, feature parity, and modernization.

## Planned Features

### Stability & Parity

### Editor Core

### UI / Theming

### Platform Integration

### Language & Syntax

### Power User

## Competitive Research
- **Notepad++** — upstream; track plugin ecosystem and Scintilla version gap.
- **Kate (KDE)** — cross-platform Scintilla-free reference with strong LSP support.
- **Zed / VS Code** — modern baseline for multi-cursor, LSP, tree-sitter.
- **Sublime Text** — command-palette reference and fast-startup benchmark.

## Nice-to-Haves

## Open-Source Research (Round 2)

### Related OSS Projects
- https://github.com/martinrotter/textosaurus — Qt + Scintilla cross-platform editor, Notepad++-like workflow, actively maintained
- https://github.com/mavroprovato/qt-scintilla-editor — minimal Qt + Scintilla reference editor
- https://github.com/notepadqq/notepadqq — Qt Notepad++ alternative for Linux (Scintilla-based), archived but still a reference
- https://github.com/rolandoislas/SciTEQt — SciTE ported to Qt QML/Quick (alternative UI layer)
- https://github.com/notepad-plus-plus/notepad-plus-plus — upstream original, Win32/STL/Scintilla
- https://github.com/tomasflouri/FeatherPad — lightweight Qt editor reference
- https://github.com/rohity123/auratext — PyQt6 + QScintilla editor, modern UI reference
- https://github.com/ScintillaOrg/lexilla — official Scintilla lexer library, upstream for syntax support

### Features to Borrow
- Keep up with upstream Scintilla/Lexilla for new language lexers (lexilla releases) — NotepadNext should track lexilla tags so new language support lands free
- Textosaurus-style session restore with multi-workspace support (textosaurus) — NotepadNext has session, textosaurus has named workspaces
- Plugin architecture parity with Notepad++ — many OSS N++ alternatives skip plugins entirely; that's the #1 blocker for N++ users to switch (Textosaurus doc discussion)
- Qt-native tabbed terminal emulator pane (FeatherPad pattern) — useful for editing + running scripts without alt-tab
- Split-editor / multi-pane per-window (textosaurus supports) — N++ has it, NotepadNext partial
- Built-in Markdown/HTML preview pane (Notepadqq) — low cost via Qt WebEngine
- AuraText-style Catppuccin/modern themes preloaded (auratext) — current NotepadNext theme library is sparse
- SciTE Lua scripting port (SciTEQt) — macro system for users coming from SciTE

### Patterns & Architectures Worth Studying
- Qt6 migration path (from Qt5) — upstream Scintilla/QScintilla both support Qt6; NotepadNext on Qt6 gets better HiDPI + Wayland + dark-mode native controls
- Lexilla as separate shared library vs embedded — dynamic lexer loading allows user-supplied lexers without recompile (Scintilla 5.x pattern)
- Flatpak + AppImage + winget/snap distribution matrix (already in NotepadNext) — add MSIX for Windows Store, matches Monitorian/Twinkle Tray distribution reach
- QScintilla vs raw Scintilla+Qt adapter — QScintilla adds Python API cost but enables PyQt plugins; auratext proves the model; Notepad Next uses raw Scintilla, document the tradeoff

## Research-Driven Additions

- [ ] P1 — Build an integration regression harness for lifecycle workflows

  Why: Fit 5/5 and impact 5/5. The current 21-target suite covers useful units, protocols, themes, and isolated components but not the MainWindow/document/session workflows where the highest-risk defects live. A headless harness is the fastest way to make future roadmap work safe.

  Evidence: No tests were found for MainWindow open/save/reload/rename/drop, ScintillaNext file I/O, actual SessionManager restore, plugin discovery, script bridge authority, LSP manager lifecycle, SFTP transfers, update parsing, or packaging artifacts. The baseline CTest trees are healthy when run sequentially, but broad integration coverage is absent. Risk is low; dependencies are Qt offscreen fixtures and deterministic temp files; novelty is low.

  Touches: `tests/CMakeLists.txt`, headless Qt fixtures, fake filesystem/SFTP/LSP/plugin providers, MainWindow and session seams, packaging smoke scripts, and test documentation.

  Acceptance: Tests run pointer-free/offscreen and do not require user settings, network credentials, or an interactive desktop; every P0 item has at least one failure-path test; temp files and processes are cleaned; tests cover cancel vs failure, external changes, partial writes, traversal bounds, plugin trust, script permissions, LSP stale responses, SFTP transaction behavior, and version metadata; baseline and no-Tree-sitter configurations remain green.

  Complexity: M

- [ ] P2 — Add first-class workspace/project state

  Why: Fit 4/5 and impact 4/5. Sessions restore files, but a project/workspace model is needed to make LSP roots, schema selection, search scope, per-project settings, missing files, and multi-root editing coherent. This converts several isolated features into a durable user workflow.

  Evidence: The command line accepts `--workspace`, but the current session model is primarily file/generation state and skips remote files. VS Code multi-root workspaces, Sublime project/workspace separation, Kate projects, Textosaurus workspaces, and Notepad++ session/workspace requests all support this direction. Risk is high; dependencies are path identity, migration, LSP, schemas, and trust state; novelty is medium.

  Touches: `SessionManager`, `SessionJournal`, `EditorManager`, workspace CLI/parser, project file format, settings/search/schema/LSP scope, recent files, and migration tests.

  Acceptance: A versioned project file can contain one or more roots, relative paths, exclusions, per-project settings, and optional task/LSP/schema metadata; transient open-buffer state remains separate; missing files are reported with recovery actions; old sessions migrate without losing data; workspace trust and portable mode are explicit; tests cover rename, root removal, duplicate paths, migration, portable paths, and multiple windows.

  Complexity: XL

- [ ] P2 — Make localization reloadable and testable

  Why: Fit 4/5 and impact 3/5. Runtime locale selection exists, but the translation manager installs translators without a complete UI retranslation lifecycle and translation ordering has an identified TODO. Restart-only language changes make accessibility and support workflows less predictable.

  Evidence: `TranslationManager` discovers embedded files and loads translators; comments and preferences behavior indicate the UI does not fully retranslate at runtime. Qt's `QTranslator` contract requires application widgets to react to `LanguageChange`; Qt accessibility and i18n practice favor stable labels and keyboard navigation. Risk is medium; dependencies are UI retranslation coverage and translation QA; novelty is low.

  Touches: `TranslationManager`, `PreferencesDialog`, all top-level forms/docks, dynamic menus/status text, translation resources, and locale tests.

  Acceptance: Locale changes either retranslate all visible widgets immediately or explicitly restart while preserving session state; system default remains first and locale ordering is deterministic; missing translations fall back safely; accessible names/descriptions update with visible labels; tests switch locales in an offscreen window and detect untranslated/duplicated labels in supported languages.

  Complexity: M

- [ ] P2 — Expand accessibility quality gates

  Why: Fit 4/5 and impact 4/5. Accessibility work has already started and should become a maintained product property rather than a handful of dialog tests. This is especially important as menus, docks, terminals, search panels, and custom title-bar behavior grow.

  Evidence: The repository has accessibility labels/tab-order improvements and tests for only a subset of dialogs. Qt accessibility documentation emphasizes names, descriptions, roles, state, keyboard focus, and assistive technology exposure; current UI inventory still includes generic labels and ambiguous controls such as `...`.

  Touches: all `.ui` forms and docks, custom title bar, menus/toolbars/status bar, translation resources, accessibility test helpers, and visual/keyboard documentation.

  Acceptance: Every interactive control has a meaningful accessible name, role, state, description where needed, and keyboard path; focus order and focus visibility are tested; contrast and disabled-state colors are checked for every theme; screen-reader tree snapshots cover main window, editor, docks, search, terminal, hex, dialogs, and custom title bar; regressions fail the test suite without controlling the user's desktop.

  Complexity: M

- [ ] P2 — Define safe offline preview and schema behavior

  Why: Fit 4/5 and impact 4/5. Markdown preview and schema validation are useful differentiators, but preview/rendering and schema resolution can become hidden network or script execution paths. A clear offline contract protects privacy and makes results deterministic.

  Evidence: README promises offline JSON/YAML/TOML schema validation and Markdown preview with optional WebEngine/built-in renderer. The source intentionally ignores remote schema URLs, but preview/resource policy and sanitization need an explicit tested contract. VS Code workspace trust, OWASP guidance, and community Markdown-editor research support clear boundaries. Risk is medium; dependencies are renderer choice and sanitization tests; novelty is medium.

  Touches: `MarkdownRenderer`, preview dock/WebEngine option, `SchemaValidator`, schema discovery/cache, workspace trust, settings, and tests/docs.

  Acceptance: Default preview and schema validation perform no network access; external links/resources and HTML/script execution follow a documented opt-in policy; WebEngine and built-in renderer have equivalent safe defaults; schema roots are bounded to approved workspace/local paths; diagnostics identify ignored remote/untrusted references; tests cover malicious HTML, path traversal, remote URLs, cache poisoning, malformed schemas, and offline operation.

  Complexity: M

- [ ] P2 — Apply EditorConfig changes without stale editor state

  Why: Fit 4/5 and impact 3/5. EditorConfig support exists, but the decorator records a TODO for editor reload. Applying the correct project settings after a file or workspace change prevents subtle indentation/EOL/encoding drift.

  Evidence: `EditorConfigAppDecorator.cpp` marks reload handling as TODO. The EditorConfig specification defines root inheritance and unset semantics, while Visual Studio documents parent precedence and reload behavior. Risk is low; dependencies are active-editor/document lifecycle wiring; novelty is low.

  Touches: `EditorConfigAppDecorator`, file watcher/editor activation signals, preferences precedence, EOL/encoding UI, and tests.

  Acceptance: Opening, renaming, changing workspace roots, and externally modifying `.editorconfig` deterministically recompute applicable settings; the active editor and newly activated editors agree; explicit user overrides are preserved according to documented precedence; no duplicate signal connections accumulate; tests cover nested roots, `root=true`, unset, rename, reload, and multiple editors.

  Complexity: S

- [ ] P2 — Repair search/replace and multi-selection lifecycle semantics

  Why: Fit 4/5 and impact 4/5. Search is a primary editor workflow, yet several TODOs identify zero-length regex matches, extended replacement escapes, stale editor pointers, and multi-selection sort order. Fixing these contracts improves correctness without adding a new subsystem.

  Evidence: `FindReplaceDialog.cpp`, `ColumnEditorDialog.cpp`, and `MainWindow.cpp` contain the corresponding TODOs; tests currently cover regex search but not the complete dialog/editor lifecycle. Notepad++ and community evidence show search, large-tree search, and selection behavior are core adoption factors. Risk is medium; dependencies are active-editor ownership and explicit regex semantics; novelty is low.

  Touches: `FindReplaceDialog`, `QRegexSearch`, `ColumnEditorDialog`, `MainWindow` editor activation/close connections, search results, and tests/translations.

  Acceptance: Zero-length matches always advance safely and terminate; replacement escape semantics are documented and tested; search dialog follows the active editor after activation/close; multi-selection sort direction is deterministic; no stale pointer or duplicate signal remains; tests cover empty matches, CRLF, Unicode, regex errors, replacement escapes, selections, and close/reopen.

  Complexity: M

- [ ] P2 — Add structured diagnostics and support observability

  Why: Fit 4/5 and impact 3/5. The application already has many asynchronous subsystems, but failures are spread across `qInfo`, message boxes, and silent return paths. Structured, redacted diagnostics will shorten issue resolution and make trust/security decisions auditable.

  Evidence: Plugin, LSP, SFTP, updater, session, and file lifecycle paths emit inconsistent messages; some errors are ignored and some logs contain operation context without a stable schema. The issue template asks for Debug Info, and release/security research favors reproducible support bundles and redacted operational data. Risk is low; dependencies are a log schema and redaction policy; novelty is medium.

  Touches: logging utilities, `DebugLogDock`, plugin/LSP/SFTP/session/updater flows, support/debug export, settings, and tests.

  Acceptance: Each asynchronous operation has a stable category, operation ID, state, duration, result, and user-facing recovery message; paths and credentials are redacted according to documented rules; support export is opt-in and deterministic; logs distinguish cancellation from failure; tests assert redaction and lifecycle correlation without relying on timestamps or network services.

  Complexity: M

- [ ] P2 — Make packaging, metadata, and upgrade artifacts reproducible

  Why: Fit 5/5 and impact 4/5. A cross-platform editor cannot be reliably upgraded or supported while version strings and release metadata disagree. This item deliberately excludes signing while making the unsigned release lane deterministic.

  Evidence: CMake reports 0.15, while Flatpak source metadata, `updates.json`, AppStream, and Cask still contain 0.14-era values; README and stale project docs also diverge. The package targets already cover Windows zip/NSIS, AppImage, Flatpak, and DMG, so validation can be added at the existing boundaries. Risk is medium; dependencies are a single version source and packaging tool availability; novelty is low.

  Touches: root `CMakeLists.txt`, `README.md`/project metadata, Flatpak/AppStream/Cask/updates files, installer scripts, package smoke checks, and release documentation.

  Acceptance: One authoritative version is propagated to executable metadata, installer/zip/DMG/AppImage/Flatpak/AppStream/Cask/update descriptors, About UI, and changelog; package smoke checks validate names, versions, hashes, dependencies, and launch metadata; builds are reproducible from a clean dependency cache policy; unsigned artifacts are explicitly labeled; signed installer/update work remains in `Roadmap_Blocked.md`.

  Complexity: M
```

</details>
