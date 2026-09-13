cmake_minimum_required(VERSION 3.16)

project(scriptura LANGUAGES CXX)

if(DEFINED ENV{GITHUB_REF})
    string(REGEX MATCH "refs/tags/v([0-9.]+)" _ $ENV{GITHUB_REF})
    set(SCRIPTURA_VERSION "${CMAKE_MATCH_1}")
else()
    set(SCRIPTURA_VERSION "0.0.0-dev")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
)

if(APPLE)
    set(INFO_PLIST_IN "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in")
    set(INFO_PLIST_OUT "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
    configure_file("${INFO_PLIST_IN}" "${INFO_PLIST_OUT}" @ONLY)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32 AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network Sql Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network Sql Test)

enable_testing()

file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")

set(RUST_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/rust_backend")
set(RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rust_backend_target")

if(WIN32)
    set(RUST_BIN_REL_PATH "${RUST_TARGET_DIR}/release/scriptura_backend.exe")
    set(RUST_LIB_REL_PATH "${RUST_TARGET_DIR}/release/scriptura_backend.lib")
else()
    set(RUST_BIN_REL_PATH "${RUST_TARGET_DIR}/release/scriptura-backend")
    set(RUST_LIB_REL_PATH "${RUST_TARGET_DIR}/release/libscriptura_backend.a")
endif()

file(GLOB_RECURSE RUST_SOURCES CONFIGURE_DEPENDS
    "${RUST_BACKEND_DIR}/src/*.rs"
    "${RUST_BACKEND_DIR}/Cargo.toml"
    "${RUST_BACKEND_DIR}/Cargo.lock"
)

if(WIN32)
    add_custom_command(
        OUTPUT "${RUST_BIN_REL_PATH}"
        COMMAND cargo build --release --target-dir "${RUST_TARGET_DIR}" --bin scriptura-backend --verbose
        WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
        COMMENT "Building Rust binary"
        DEPENDS ${RUST_SOURCES}
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT "${RUST_BIN_REL_PATH}"
        COMMAND cargo build --release --target-dir "${RUST_TARGET_DIR}" --bin scriptura-backend
        WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
        COMMENT "Building Rust binary"
        DEPENDS ${RUST_SOURCES}
        VERBATIM
    )
endif()
add_custom_target(rust_binary DEPENDS "${RUST_BIN_REL_PATH}")

# Also build staticlib for tests/linking
if(WIN32)
    add_custom_command(
        OUTPUT "${RUST_LIB_REL_PATH}"
        COMMAND cargo build --release --target-dir "${RUST_TARGET_DIR}" --verbose
        WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
        COMMENT "Building Rust staticlib"
        DEPENDS ${RUST_SOURCES}
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT "${RUST_LIB_REL_PATH}"
        COMMAND cargo build --release --target-dir "${RUST_TARGET_DIR}"
        WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
        COMMENT "Building Rust staticlib"
        DEPENDS ${RUST_SOURCES}
        VERBATIM
    )
endif()
add_custom_target(rust_backend DEPENDS "${RUST_LIB_REL_PATH}")
add_dependencies(rust_binary rust_backend)

# Discover UI-layer C++ sources (excluding backend, qt_bridge, and main)
file(GLOB_RECURSE SCRIPTURA_CORE_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp" "src/*.h" "src/*.ui" "src/*.qrc" "src/*.h.in"
)
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/plugins/git/")
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/rust_backend/")
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/qt_bridge\\.cpp$")
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

list(APPEND SCRIPTURA_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scriptura/plugininterface.h"
)

# Compile qt_bridge as a separate static library
add_library(qt_bridge STATIC src/qt_bridge.cpp)
target_include_directories(qt_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/lsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/dap
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internals
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptura
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include/scriptura
    ${CMAKE_CURRENT_BINARY_DIR}
    /usr/include/x86_64-linux-gnu/qt6
    /usr/include/x86_64-linux-gnu/qt6/QtCore
    /usr/include/x86_64-linux-gnu/qt6/QtWidgets
    /usr/include/x86_64-linux-gnu/qt6/QtGui
    /usr/include/x86_64-linux-gnu/qt6/QtNetwork
)
target_link_libraries(qt_bridge PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Gui
)
if(UNIX AND NOT APPLE)
    target_link_libraries(qt_bridge PRIVATE pthread dl rt m)
endif()

add_library(scriptura_core STATIC ${SCRIPTURA_CORE_SOURCES})
set_target_properties(scriptura_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Internal"
)

find_package(ZLIB REQUIRED)
target_link_libraries(scriptura_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    ZLIB::ZLIB
)

if(WIN32)
    target_link_libraries(scriptura_core PRIVATE dwmapi)
    target_link_libraries(scriptura_core PRIVATE bcrypt user32 advapi32 ws2_32 crypt32 userenv ntdll)
endif()

if(APPLE)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(SECURITY_LIBRARY Security)
    find_library(SYSTEMCONFIG_LIBRARY SystemConfiguration)
    target_link_libraries(scriptura_core PRIVATE
        "${COREFOUNDATION_LIBRARY}"
        "${SECURITY_LIBRARY}"
        "${SYSTEMCONFIG_LIBRARY}"
    )
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(scriptura_core PRIVATE pthread dl rt m)
endif()

target_include_directories(scriptura_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/lsp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/dap
    ${CMAKE_CURRENT_SOURCE_DIR}/src/panels
    ${CMAKE_CURRENT_SOURCE_DIR}/src/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internals
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/scriptura
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk/include/scriptura
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Create a wrapper scriptura target that depends on rust_binary
add_custom_target(scriptura ALL DEPENDS rust_binary)

# Copy Rust binary as the final executable
add_custom_command(
    TARGET rust_binary POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${RUST_BIN_REL_PATH}"
        "${CMAKE_BINARY_DIR}/scriptura${CMAKE_EXECUTABLE_SUFFIX}"
)

add_subdirectory(src/plugins/git)
add_subdirectory(tests)

include(GNUInstallDirs)
install(PROGRAMS "${RUST_BIN_REL_PATH}" DESTINATION bin RENAME scriptura)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deployment/linux/scriptura.desktop")
    install(FILES deployment/linux/scriptura.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
endif()

set(CPACK_PACKAGE_NAME "scriptura")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple Qt-based text editor with project file browsing")
set(CPACK_PACKAGE_VENDOR "Scriptura")
set(CPACK_PACKAGE_CONTACT "maintainer@example.com")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/snap/snapcraft.yaml")
    set(CPACK_GENERATOR "TGZ")
endif()

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Scriptura")
    set(CPACK_DMG_FORMAT "UDBZ")
endif()

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/deployment/windows/icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/deployment/windows/icon.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "
        CreateShortCut \"\$SMPROGRAMS\\\$STARTMENU_FOLDER\\${CPACK_PACKAGE_NAME}.lnk\" \"\$INSTDIR\\scriptura.exe\"
        CreateShortCut \"\$SMPROGRAMS\\\$STARTMENU_FOLDER\\Uninstall.lnk\" \"\$INSTDIR\\Uninstall.exe\"
    ")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "
        Delete \"\$SMPROGRAMS\\\$STARTMENU_FOLDER\\${CPACK_PACKAGE_NAME}.lnk\"
        Delete \"\$SMPROGRAMS\\\$STARTMENU_FOLDER\\Uninstall.lnk\"
    ")
    set(CPACK_NSIS_ENABLE_AUTO_DIRECTORY true)
    set(CPACK_NSIS_MODIFY_PATH "ON")
endif()

include(CPack)
