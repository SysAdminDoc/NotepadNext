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
