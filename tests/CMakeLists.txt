find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOTEPADNEXT_ENABLE_TREE_SITTER)
    set(NOTEPADNEXT_TREE_SITTER_STATE on)
else()
    set(NOTEPADNEXT_TREE_SITTER_STATE off)
endif()

if(NOTEPADNEXT_HAS_WEBENGINE)
    set(NOTEPADNEXT_WEBENGINE_STATE on)
else()
    set(NOTEPADNEXT_WEBENGINE_STATE off)
endif()

add_test(NAME NotepadNextDependencyInventoryTests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_SOURCE_DIR}/scripts/dependency_inventory.py
        --check
        --tree-sitter ${NOTEPADNEXT_TREE_SITTER_STATE}
        --webengine ${NOTEPADNEXT_WEBENGINE_STATE}
        --output ${CMAKE_BINARY_DIR}/dependency-inventory.json
        --sbom ${CMAKE_BINARY_DIR}/dependency-sbom.json
)

qt_add_executable(NotepadNextSessionJournalTests
    SessionJournalTests.cpp
    ${CMAKE_SOURCE_DIR}/src/SessionJournal.cpp
)

target_include_directories(NotepadNextSessionJournalTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextSessionJournalTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextSessionJournalTests COMMAND NotepadNextSessionJournalTests)

qt_add_executable(NotepadNextAtomicFileWriterTests
    AtomicFileWriterTests.cpp
    ${CMAKE_SOURCE_DIR}/src/AtomicFileWriter.cpp
)

target_include_directories(NotepadNextAtomicFileWriterTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextAtomicFileWriterTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextAtomicFileWriterTests COMMAND NotepadNextAtomicFileWriterTests)

qt_add_executable(NotepadNextCapabilityTrustTests
    CapabilityTrustTests.cpp
    ${CMAKE_SOURCE_DIR}/src/CapabilityTrust.cpp
)

target_include_directories(NotepadNextCapabilityTrustTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextCapabilityTrustTests
    PRIVATE
        Qt6::Core
        Qt6::Test
        Qt6::Widgets
)

add_test(NAME NotepadNextCapabilityTrustTests COMMAND NotepadNextCapabilityTrustTests)

qt_add_executable(NotepadNextCommandPaletteTests
    CommandPaletteTests.cpp
    ${CMAKE_SOURCE_DIR}/src/CommandPaletteFilter.cpp
)

target_include_directories(NotepadNextCommandPaletteTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextCommandPaletteTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextCommandPaletteTests COMMAND NotepadNextCommandPaletteTests)

qt_add_executable(NotepadNextQRegexSearchTests
    QRegexSearchTests.cpp
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.cpp
)

target_include_directories(NotepadNextQRegexSearchTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextQRegexSearchTests
    PRIVATE
        Qt6::Core
        Qt6::Test
        scintilla
)

add_test(NAME NotepadNextQRegexSearchTests COMMAND NotepadNextQRegexSearchTests)

qt_add_executable(NotepadNextRegexBuilderTests
    RegexBuilderTests.cpp
    ${CMAKE_SOURCE_DIR}/src/RegexBuilder.cpp
)

target_include_directories(NotepadNextRegexBuilderTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextRegexBuilderTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextRegexBuilderTests COMMAND NotepadNextRegexBuilderTests)

qt_add_executable(NotepadNextHexDocumentTests
    HexDocumentTests.cpp
    ${CMAKE_SOURCE_DIR}/src/HexDocument.cpp
    ${CMAKE_SOURCE_DIR}/src/HexTableModel.cpp
    ${CMAKE_SOURCE_DIR}/src/AtomicFileWriter.cpp
)

target_include_directories(NotepadNextHexDocumentTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextHexDocumentTests
    PRIVATE
        Qt6::Core
        Qt6::Core5Compat
        Qt6::Test
)

add_test(NAME NotepadNextHexDocumentTests COMMAND NotepadNextHexDocumentTests)

qt_add_executable(NotepadNextSnippetEngineTests
    SnippetEngineTests.cpp
    ${CMAKE_SOURCE_DIR}/src/SnippetEngine.cpp
)

target_include_directories(NotepadNextSnippetEngineTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextSnippetEngineTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextSnippetEngineTests COMMAND NotepadNextSnippetEngineTests)

qt_add_executable(NotepadNextAccessibilityTests
    AccessibilityTests.cpp
    ${CMAKE_SOURCE_DIR}/src/CommandPaletteDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/CommandPaletteFilter.cpp
    ${CMAKE_SOURCE_DIR}/src/FindInFilesDock.cpp
    ${CMAKE_SOURCE_DIR}/src/RipgrepSearch.cpp
    ${CMAKE_SOURCE_DIR}/src/TerminalDock.cpp
    ${CMAKE_SOURCE_DIR}/src/TerminalProcess.cpp
    ${CMAKE_SOURCE_DIR}/src/CapabilityTrust.cpp
)

target_include_directories(NotepadNextAccessibilityTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/dialogs
)

target_link_libraries(NotepadNextAccessibilityTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
)

add_test(NAME NotepadNextAccessibilityTests COMMAND NotepadNextAccessibilityTests)
set_tests_properties(NotepadNextAccessibilityTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(NotepadNextLanguageDefinitionTests
    LanguageDefinitionTests.cpp
    ${CMAKE_SOURCE_DIR}/src/LuaState.cpp
    ${CMAKE_SOURCE_DIR}/src/scripts.qrc
)

target_include_directories(NotepadNextLanguageDefinitionTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextLanguageDefinitionTests
    PRIVATE
        Qt6::Core
        Qt6::Test
        lua
        lexilla
        scintilla
)

add_test(NAME NotepadNextLanguageDefinitionTests COMMAND NotepadNextLanguageDefinitionTests)

if(NOTEPADNEXT_ENABLE_TREE_SITTER)
    qt_add_executable(NotepadNextTreeSitterTests
        TreeSitterJsonTests.cpp
        ${CMAKE_SOURCE_DIR}/src/TreeSitterJsonLexer.cpp
    )

    target_include_directories(NotepadNextTreeSitterTests
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_SOURCE_DIR}/thirdparty/scintilla/include
    )

    target_link_libraries(NotepadNextTreeSitterTests
        PRIVATE
            Qt6::Core
            Qt6::Test
            tree-sitter-json
    )

    add_test(NAME NotepadNextTreeSitterTests COMMAND NotepadNextTreeSitterTests)
endif()

qt_add_executable(NotepadNextLspProtocolTests
    LspProtocolTests.cpp
    ${CMAKE_SOURCE_DIR}/src/LspClient.cpp
)

target_include_directories(NotepadNextLspProtocolTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextLspProtocolTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextLspProtocolTests COMMAND NotepadNextLspProtocolTests)

qt_add_executable(NotepadNextThemeManagerTests
    ThemeManagerTests.cpp
    ${CMAKE_SOURCE_DIR}/src/ThemeManager.cpp
)

target_include_directories(NotepadNextThemeManagerTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextThemeManagerTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME NotepadNextThemeManagerTests COMMAND NotepadNextThemeManagerTests)

qt_add_executable(NotepadNextIconThemeManagerTests
    IconThemeManagerTests.cpp
    ${CMAKE_SOURCE_DIR}/src/IconThemeManager.cpp
)

target_include_directories(NotepadNextIconThemeManagerTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextIconThemeManagerTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME NotepadNextIconThemeManagerTests COMMAND NotepadNextIconThemeManagerTests)
set_tests_properties(NotepadNextIconThemeManagerTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(NotepadNextFontLigatureTests
    FontLigatureTests.cpp
)

target_link_libraries(NotepadNextFontLigatureTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Test
)

add_test(NAME NotepadNextFontLigatureTests COMMAND NotepadNextFontLigatureTests)

add_test(
    NAME NotepadNextShellIntegrationTests
    COMMAND ${CMAKE_COMMAND}
        -DNOTEPADNEXT_SOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/cmake/ValidateShellIntegration.cmake
)

qt_add_executable(NotepadNextSchemaValidatorTests
    SchemaValidatorTests.cpp
    ${CMAKE_SOURCE_DIR}/src/SchemaValidator.cpp
)

target_include_directories(NotepadNextSchemaValidatorTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CPM_PACKAGE_tomlplusplus_SOURCE_DIR}/include
)

target_link_libraries(NotepadNextSchemaValidatorTests
    PRIVATE
        Qt6::Core
        Qt6::Test
        yaml-cpp::yaml-cpp
)

add_test(NAME NotepadNextSchemaValidatorTests COMMAND NotepadNextSchemaValidatorTests)

qt_add_executable(NotepadNextMarkdownPreviewTests
    MarkdownPreviewTests.cpp
    ${CMAKE_SOURCE_DIR}/src/MarkdownRenderer.cpp
)

target_include_directories(NotepadNextMarkdownPreviewTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextMarkdownPreviewTests
    PRIVATE
        Qt6::Gui
        Qt6::Test
)

add_test(NAME NotepadNextMarkdownPreviewTests COMMAND NotepadNextMarkdownPreviewTests)
set_tests_properties(NotepadNextMarkdownPreviewTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(NotepadNextPortableModeTests
    PortableModeTests.cpp
    ${CMAKE_SOURCE_DIR}/src/PortableMode.cpp
)

target_include_directories(NotepadNextPortableModeTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextPortableModeTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextPortableModeTests COMMAND NotepadNextPortableModeTests)

qt_add_executable(NotepadNextGitRepositoryTests
    GitRepositoryTests.cpp
    ${CMAKE_SOURCE_DIR}/src/GitRepository.cpp
)

target_include_directories(NotepadNextGitRepositoryTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CPM_PACKAGE_libgit2_SOURCE_DIR}/include
)

target_link_libraries(NotepadNextGitRepositoryTests
    PRIVATE
        Qt6::Core
        Qt6::Test
        libgit2package
)

add_test(NAME NotepadNextGitRepositoryTests COMMAND NotepadNextGitRepositoryTests)

qt_add_executable(NotepadNextTerminalProcessTests
    TerminalProcessTests.cpp
    ${CMAKE_SOURCE_DIR}/src/TerminalProcess.cpp
)

target_include_directories(NotepadNextTerminalProcessTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextTerminalProcessTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextTerminalProcessTests COMMAND NotepadNextTerminalProcessTests)

qt_add_executable(NotepadNextRipgrepSearchTests
    RipgrepSearchTests.cpp
    ${CMAKE_SOURCE_DIR}/src/RipgrepSearch.cpp
)

target_include_directories(NotepadNextRipgrepSearchTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextRipgrepSearchTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextRipgrepSearchTests COMMAND NotepadNextRipgrepSearchTests)

qt_add_executable(NotepadNextSftpClientTests
    SftpClientTests.cpp
    ${CMAKE_SOURCE_DIR}/src/SftpClient.cpp
)

target_include_directories(NotepadNextSftpClientTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CPM_PACKAGE_libssh2_SOURCE_DIR}/include
)

target_link_libraries(NotepadNextSftpClientTests
    PRIVATE
        Qt6::Core
        Qt6::Network
        Qt6::Test
        libssh2::libssh2
)

add_test(NAME NotepadNextSftpClientTests COMMAND NotepadNextSftpClientTests)

qt_add_executable(NotepadNextPluginTrustTests
    PluginTrustTests.cpp
    ${CMAKE_SOURCE_DIR}/src/NppPluginTrust.cpp
)

target_include_directories(NotepadNextPluginTrustTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextPluginTrustTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextPluginTrustTests COMMAND NotepadNextPluginTrustTests)

qt_add_executable(NotepadNextExternalFileChangeTests
    ExternalFileChangeTests.cpp
    ${CMAKE_SOURCE_DIR}/src/AtomicFileWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaNext.cpp
    ${CMAKE_SOURCE_DIR}/src/Finder.cpp
    ${CMAKE_SOURCE_DIR}/src/ScintillaCommenter.cpp
    ${CMAKE_SOURCE_DIR}/src/SelectionTracker.cpp
    ${CMAKE_SOURCE_DIR}/src/UndoAction.cpp
    ${CMAKE_SOURCE_DIR}/src/RangeAllocator.cpp
    ${CMAKE_SOURCE_DIR}/src/QRegexSearch.cpp
)

target_include_directories(NotepadNextExternalFileChangeTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextExternalFileChangeTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Core5Compat
        Qt6::Test
        scintilla
        uchardet
)

add_test(NAME NotepadNextExternalFileChangeTests COMMAND NotepadNextExternalFileChangeTests)
set_tests_properties(NotepadNextExternalFileChangeTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

qt_add_executable(NotepadNextDirectoryDropScannerTests
    DirectoryDropScannerTests.cpp
    ${CMAKE_SOURCE_DIR}/src/DirectoryDropScanner.cpp
)

target_include_directories(NotepadNextDirectoryDropScannerTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(NotepadNextDirectoryDropScannerTests
    PRIVATE
        Qt6::Core
        Qt6::Test
)

add_test(NAME NotepadNextDirectoryDropScannerTests COMMAND NotepadNextDirectoryDropScannerTests)

# Build the real application sources into an offscreen lifecycle harness.  The
# application target is deliberately reused as the source of truth so the
# harness exercises the same MainWindow/session/plugin/LSP wiring as a release
# build instead of a reduced test-only substitute.
get_target_property(NOTEPADNEXT_LIFECYCLE_SOURCES NotepadNext SOURCES)
set(NOTEPADNEXT_LIFECYCLE_SOURCE_FILES)
foreach(source IN LISTS NOTEPADNEXT_LIFECYCLE_SOURCES)
    if(source MATCHES "^\\$<")
        continue()
    endif()

    get_filename_component(source_name "${source}" NAME)
    get_filename_component(source_extension "${source}" EXT)
    if(source_name STREQUAL "main.cpp" OR source_name STREQUAL "version.rc")
        continue()
    endif()
    if(NOT source_extension MATCHES "^\\.(c|cc|cpp|h|hpp|ui|qrc)$")
        continue()
    endif()

    if(IS_ABSOLUTE "${source}")
        list(APPEND NOTEPADNEXT_LIFECYCLE_SOURCE_FILES "${source}")
    else()
        list(APPEND NOTEPADNEXT_LIFECYCLE_SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/${source}")
    endif()
endforeach()

qt_add_executable(NotepadNextLifecycleTests
    LifecycleRegressionTests.cpp
    ${NOTEPADNEXT_LIFECYCLE_SOURCE_FILES}
)

target_include_directories(NotepadNextLifecycleTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/decorators
        ${CMAKE_SOURCE_DIR}/src/dialogs
        ${CMAKE_SOURCE_DIR}/src/docks
        ${CMAKE_SOURCE_DIR}/src/widgets
        ${CPM_PACKAGE_libgit2_SOURCE_DIR}/include
        ${CPM_PACKAGE_libssh2_SOURCE_DIR}/include
        ${CPM_PACKAGE_quickjs_SOURCE_DIR}
        ${CPM_PACKAGE_tomlplusplus_SOURCE_DIR}/include
)

target_link_libraries(NotepadNextLifecycleTests
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::PrintSupport
        Qt6::Test
        qtadvanceddocking-qt6
        QSimpleUpdater
        SingleApplication::SingleApplication
        editorconfig-core-qt
        lua
        lexilla
        scintilla
        uchardet
        yaml-cpp::yaml-cpp
        libgit2package
        libssh2::libssh2
        qjs
)

target_compile_definitions(NotepadNextLifecycleTests
    PRIVATE
        ADS_STATIC
        QSIMPLEUPDATER_STATIC
        QAPPLICATION_CLASS=QApplication
        APP_VERSION="${PROJECT_VERSION}"
        APP_COPYRIGHT="${APP_COPYRIGHT}"
        APP_DISTRIBUTION="${APP_DISTRIBUTION}"
        NOTEPADNEXT_LIFECYCLE_TRACE
)

if (NOTEPADNEXT_HAS_WEBENGINE)
    target_compile_definitions(NotepadNextLifecycleTests PRIVATE NOTEPADNEXT_HAS_WEBENGINE)
    target_link_libraries(NotepadNextLifecycleTests PRIVATE Qt6::WebEngineWidgets)
endif()

if(WIN32)
    target_link_libraries(NotepadNextLifecycleTests PRIVATE dwmapi)
endif()

if(NOTEPADNEXT_ENABLE_TREE_SITTER)
    target_compile_definitions(NotepadNextLifecycleTests PRIVATE NOTEPADNEXT_WITH_TREE_SITTER)
    target_link_libraries(NotepadNextLifecycleTests PRIVATE tree-sitter-json)
endif()

add_test(NAME NotepadNextLifecycleTests COMMAND NotepadNextLifecycleTests)
set_tests_properties(NotepadNextLifecycleTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    RUN_SERIAL TRUE
)
