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
