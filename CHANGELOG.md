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
