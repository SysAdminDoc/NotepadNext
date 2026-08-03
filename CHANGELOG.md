# Changelog

All notable changes to NotepadNext will be documented in this file.

## Unreleased

- Fixed: Session recovery now commits unsaved buffers through an atomic, generation-based journal with a backward-compatible legacy loader.
- Added: QtTest coverage for session journal round-tripping and buffer-path traversal protection.

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
