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
