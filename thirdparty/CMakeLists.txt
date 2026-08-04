include(${CMAKE_SOURCE_DIR}/cmake/CPM.cmake)

CPMAddPackage(
    NAME ads
    GITHUB_REPOSITORY githubuser0xFFFF/Qt-Advanced-Docking-System
    GIT_TAG 34b68d6eab1556cf851d24e033909332771f3dfe
    OPTIONS
        "BUILD_STATIC ON"
        "BUILD_EXAMPLES OFF"
)

CPMAddPackage(
    NAME QSimpleUpdater
    GITHUB_REPOSITORY alex-spataru/QSimpleUpdater
    GIT_TAG 8e7017f7fbdc2b4b1a26ed1eef9ebcba6a50639c
    OPTIONS
        "QSIMPLE_UPDATER_BUILD_TESTS OFF"
)

set(QAPPLICATION_CLASS QApplication CACHE STRING "Inheritance class for SingleApplication" FORCE)
CPMAddPackage(
    NAME SingleApplication
    GITHUB_REPOSITORY itay-grudev/SingleApplication
    GIT_TAG 494772e98cef0aa88124f154feb575cc60b08b38
)

CPMAddPackage(
    NAME editorconfig_core_qt
    GITHUB_REPOSITORY editorconfig/editorconfig-core-qt
    GIT_TAG ab62f0554abf2bbe4d45427b36a8b2f81ca7b4ab
)

CPMAddPackage(
    NAME yaml-cpp
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG yaml-cpp-0.9.0
    OPTIONS
        "YAML_CPP_BUILD_TESTS OFF"
        "YAML_CPP_BUILD_TOOLS OFF"
        "YAML_BUILD_SHARED_LIBS OFF"
)

CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    GIT_TAG v3.4.0
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME libgit2
    GITHUB_REPOSITORY libgit2/libgit2
    GIT_TAG 26055f5af74ab1cf636d272e8a34315496d3f06f
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "BUILD_TESTS OFF"
        "BUILD_CLI OFF"
        "BUILD_EXAMPLES OFF"
        "USE_SSH OFF"
        "USE_HTTPS OFF"
        "USE_GSSAPI OFF"
        "USE_NTLMCLIENT OFF"
        "USE_BUNDLED_ZLIB ON"
        "REGEX_BACKEND builtin"
)

if(WIN32)
    set(NOTEPADNEXT_LIBSSH2_CRYPTO_BACKEND WinCNG)
else()
    set(NOTEPADNEXT_LIBSSH2_CRYPTO_BACKEND OpenSSL)
endif()

CPMAddPackage(
    NAME libssh2
    GITHUB_REPOSITORY libssh2/libssh2
    GIT_TAG 3a735286108ad19e3b49c64ebcb66342f1f21df7
    OPTIONS
        "BUILD_STATIC_LIBS ON"
        "BUILD_SHARED_LIBS OFF"
        "BUILD_TESTING OFF"
        "BUILD_EXAMPLES OFF"
        "CRYPTO_BACKEND ${NOTEPADNEXT_LIBSSH2_CRYPTO_BACKEND}"
)

add_subdirectory(lua)
add_subdirectory(scintilla)
add_subdirectory(lexilla)
add_subdirectory(uchardet)

target_link_libraries(lexilla PRIVATE scintilla)

if(NOTEPADNEXT_ENABLE_TREE_SITTER)
    CPMAddPackage(
        NAME tree_sitter
        GITHUB_REPOSITORY tree-sitter/tree-sitter
        GIT_TAG v0.26.9
        OPTIONS
            "BUILD_SHARED_LIBS OFF"
            "TREE_SITTER_FEATURE_WASM OFF"
            "AMALGAMATED ON"
    )

    CPMAddPackage(
        NAME tree_sitter_json
        GITHUB_REPOSITORY tree-sitter/tree-sitter-json
        GIT_TAG v0.24.8
        DOWNLOAD_ONLY YES
    )

    add_library(tree-sitter-json STATIC
        ${tree_sitter_json_SOURCE_DIR}/src/parser.c
    )
    set_target_properties(tree-sitter-json PROPERTIES C_STANDARD 11)
    target_link_libraries(tree-sitter-json PUBLIC tree-sitter)
endif()
