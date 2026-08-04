# Notepad Next

![Build Notepad Next](https://github.com/dail8859/NotepadNext/workflows/Build%20Notepad%20Next/badge.svg)

A cross-platform, reimplementation of Notepad++.

Though the application overall is stable and usable, it should not be considered safe for critically important work.

There are numerous bugs and half working implementations. Pull requests are greatly appreciated.

![screenshot](/doc/screenshot.png)

# Installation

Packages are available for Windows, Linux, and MacOS.

Below are the supported distribution mechanisms. There may be other ways to download/install the application, but this project will likely not be able to offer any support for those since they are made available by other individuals.

## Windows
Windows packages are available as an installer or a stand-alone zip file on the [release](https://github.com/dail8859/NotepadNext/releases) page. The installer provides additional components such as an auto-updater and Windows context menu integration. You can easily install it with Winget:

```powershell
winget install dail8859.NotepadNext
```

## Linux
Linux packages can be obtained by downloading the stand-alone AppImage on the [release](https://github.com/dail8859/NotepadNext/releases) page or by installing the [flatpak](https://flathub.org/apps/details/com.github.dail8859.NotepadNext) by executing:

```bash
flatpak install flathub com.github.dail8859.NotepadNext
```

The maintained Flatpak manifest is [`deploy/flatpak/com.github.dail8859.NotepadNext.yml`](deploy/flatpak/com.github.dail8859.NotepadNext.yml). It forwards files through the desktop portal and keeps settings and session data in the Flatpak-scoped portable profile. Build it locally with `flatpak-builder --user --install --force-clean build-flatpak deploy/flatpak/com.github.dail8859.NotepadNext.yml`.

The Linux desktop entry registers Notepad Next as a text-file handler and accepts multiple files from the file manager. The Windows installer adds both an “Open with” registration and an optional context-menu command, while the macOS application bundle registers itself as an editor for text documents.

If you are using Ubuntu and prefer an up-to-date deb version, you can use the [PPA supporting Ubuntu 22.04 and newer](https://launchpad.net/~quentiumyt/+archive/ubuntu/notepadnext) provided by
[Quentin Lienhardt](https://github.com/QuentiumYT). You can add it by executing:

```bash
sudo add-apt-repository ppa:quentiumyt/notepadnext
sudo apt update
sudo apt install notepadnext
```

## MacOS
MacOS disk images can be downloaded from the [release](https://github.com/dail8859/NotepadNext/releases) page.

It can also be installed using brew:
```bash
brew tap dail8859/notepadnext
brew install --no-quarantine notepadnext
```

The maintained cask mirror is available in [`Casks/notepadnext.rb`](Casks/notepadnext.rb).

#### MacOS Tweaks

By default, MacOS enables font smoothing which causes text to appear quite differently from the Windows version. This can be disabled system-wide using the following command:

```bash
defaults -currentHost write -g AppleFontSmoothing -int 0
```

A restart is required for this to take effect.

# Translations
Translations are contributed by the community. All translations are managed using Crowdin at `https://crowdin.com/project/notepadnext`. If there is a language missing you would like to contribute, feel free to start a discussion on Crowdin.

# Development
Current development is done using QtCreator with the Microsoft Visual C++ (msvc) compiler. Qt 6.5 is the currently supported Qt version. Older versions of Qt are likely to work but are not tested. Any fixes for older versions will be accepted as long as they do not introduce complex fixes. This application is also known to build successfully on various Linux distributions and macOS. Other platforms/compilers should be usable with minor modifications.

If you are familiar with building C++ Qt desktop applications with Qt Creator, then this should be as simple as opening `CMakeLists` and build/run the project.

The build includes the optional Tree-sitter JSON lexer by default. Disable it with `-DNOTEPADNEXT_ENABLE_TREE_SITTER=OFF` when a minimal build without the parser dependency is preferred.

Editor language services are available when their server is installed on `PATH`: `clangd` for C/C++, `pyright-langserver` for Python, `rust-analyzer` for Rust, and `gopls` for Go. Diagnostics are shown as editor squiggles, hover information appears after briefly resting the pointer over a symbol, and `F12` requests go-to-definition. The editor continues to work normally when a server is not installed.

JSON, YAML, and TOML documents can be validated against JSON Schema documents from the Search menu or command palette. Choose **Validate Document Against Schema...** to attach a schema to the current document, or use **Clear Document Schema** to return to automatic discovery. The editor also checks `<document>.schema.json`, `<document-name>.schema.json`, `.notepadnext-schema.json`, local `schema:`/`yaml-language-server` directives, and local `$schema` paths. Validation is debounced while editing and marks failures with squiggles; remote schema URLs are ignored so validation remains offline.

Markdown and MDX documents can be previewed live from **View > Markdown Preview**. The preview opens as a docked split pane, follows the active editor, refreshes while typing, and resolves local images and links relative to the document. Qt WebEngine is used when the installed Qt kit provides it; otherwise the preview uses Qt's built-in Markdown renderer with scripting disabled.

The View menu provides Fusion, Material, and Fluent theme variants, plus Default, Nord, Catppuccin, and GitHub Dark icon packs. Both selections are stored with the application settings and apply immediately; a `custom.css` file beside the settings file is still loaded last so local overrides take precedence.

On Windows, the main window uses a custom frameless title bar with native resize/maximize behavior and requests Mica backdrop and rounded-corner effects when the installed DWM version supports them.

Programming fonts use Qt’s HarfBuzz-backed shaping with standard (`liga`/`clig`) and contextual (`calt`) OpenType features enabled when the Qt version exposes explicit feature control.

If you are new to building C++ Qt desktop applications, there is a more detailed guide [here](/doc/Building.md).


# License
This code is released under the [GNU General Public License version 3](https://www.gnu.org/licenses/gpl-3.0.txt).
