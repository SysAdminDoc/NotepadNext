# Changelog

All notable changes to NotepadNext will be documented in this file.

## Unreleased

- Fixed: Session recovery now commits unsaved buffers through an atomic, generation-based journal with a backward-compatible legacy loader.
- Added: QtTest coverage for session journal round-tripping and buffer-path traversal protection.
- Fixed: File loading now decodes UTF-8/16/32 BOMs and uchardet-detected legacy encodings before sending text to Scintilla, preserving the codec for atomic saves.
- Added: Ctrl+D expands the word under the caret before selecting the next instance, and Alt-click adds a caret without sacrificing Alt-drag rectangular selection.
- Added: Ctrl+Shift+P opens a fuzzy command palette covering enabled menu actions.
- Fixed: Recorded macros now persist edits and shortcuts immediately, reject duplicate names/shortcuts, and restore keyboard actions after restart.
- Fixed: Regex search enumeration and Replace All now make progress through zero-length matches without repeating or hanging.

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
