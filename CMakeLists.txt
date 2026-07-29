cmake_minimum_required(VERSION 3.16)

project(scriptura LANGUAGES CXX)

# 版本號從 GitHub Release tag 注入，開發時預設為 0.0.0-dev
if(DEFINED ENV{GITHUB_REF})
    string(REGEX MATCH "refs/tags/v([0-9.]+)" _ $ENV{GITHUB_REF})
    set(SCRIPTURA_VERSION "${CMAKE_MATCH_1}")
else()
    set(SCRIPTURA_VERSION "0.0.0-dev")
endif()

# 產生 version.h
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Network Sql Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Network Sql Test)

enable_testing()

# Translation files
file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")

# Main application sources (entry-point + resource file only)
set(PROJECT_SOURCES
        src/main.cpp
        src/resources.qrc
)

# ── Rust Backend Library ──────────────────────────────────────────
# Build the Rust library using Cargo
set(RUST_BACKEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/rust_backend")

# Cargo puts the output in <project>/target/ by default (since it's a workspace)
# We set the target dir explicitly to ${CMAKE_CURRENT_BINARY_DIR}/rust_backend_target
set(RUST_TARGET_DIR "${CMAKE_CURRENT_BINARY_DIR}/rust_backend_target")

if(WIN32)
    set(RUST_LIB_REL_PATH "${RUST_TARGET_DIR}/release/scriptura_backend.lib")
else()
    set(RUST_LIB_REL_PATH "${RUST_TARGET_DIR}/release/libscriptura_backend.a")
endif()

# Build the Rust library via cargo.
# Define a custom command + target for all platforms (Linux, macOS, and Windows).
if(WIN32)
    add_custom_command(
        OUTPUT "${RUST_LIB_REL_PATH}"
        COMMAND cargo build --release --target-dir "${RUST_TARGET_DIR}" --verbose
        WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
        COMMENT "Building Rust backend (cargo build --release) for MSVC"
        VERBATIM
    )
else()
    add_custom_command(
        OUTPUT "${RUST_LIB_REL_PATH}"
        COMMAND cargo build --release --target-dir "${RUST_TARGET_DIR}"
        WORKING_DIRECTORY "${RUST_BACKEND_DIR}"
        COMMENT "Building Rust backend (cargo build --release)"
    )
endif()
add_custom_target(rust_backend
    DEPENDS "${RUST_LIB_REL_PATH}"
)

# On Windows (MSBuild), the rust_backend target will be added as a dependency
# to all targets that need it, ensuring it's built before they're linked.

# Discover only UI-layer C++ sources under src/.
# All backend/infrastructure services have been rewritten in Rust (src/rust_backend/)
# and are excluded from C++ compilation. Only Qt UI widgets, panels, and the
# Rust bridge adapter are compiled as C++.
file(GLOB_RECURSE SCRIPTURA_CORE_SOURCES
    "src/*.cpp" "src/*.h" "src/*.ui" "src/*.qrc" "src/*.h.in"
)

# ── Exclude backend C++ files (replaced by Rust FFI modules) ─────────
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/tests/")
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/plugins/git/")
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/pluginhost/")
list(FILTER SCRIPTURA_CORE_SOURCES EXCLUDE REGEX "src/rust_backend/")

# ── Rust Backend (old C++ backend files deleted) ──────────────────
#
# All backend services have been reimplemented in Rust (src/rust_backend/)
# with complete C FFI exports (133 functions across 21+ modules).
# Qt adapter classes are in src/rust_adapter.h/.cpp (RustBackend singleton).
#
# Adapters exist for: LSP, DAP, EventBus, PluginManager, Workspace,
# TaskRunner, Updater, ConfigValidator, PluginRegistry, Permission,
# ServiceLocator, CrashHandler, DependencyResolver.
#
# Old C++ backend files have been deleted. Remaining exclusions are for
# files that were never migrated or belong to the plugin host.

# The plugin SDK interface headers live outside src/ (in include/ and sdk/include/)
# and contain Q_OBJECT classes (ScripturaPlugin, PluginContext). Their MOC-generated
# code must be compiled into scriptura_core so that plugins (which link scriptura_core
# statically) can resolve those symbols on Windows, where a DLL cannot pull symbols
# from the host executable the way Linux/macOS plugins do at load time.
list(APPEND SCRIPTURA_CORE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/include/scriptura/plugininterface.h"
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(scriptura
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_add_lupdate(scriptura TS_FILES)
    qt_add_lrelease(scriptura TS_FILES ${TS_FILES})
else()
    if(ANDROID)
        add_library(scriptura SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(scriptura
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${PROJECT_SOURCES} ${TS_FILES})
endif()

set(I18N_QRC "${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc")
file(WRITE ${I18N_QRC} "<!DOCTYPE RCC><RCC version=\"1.0\">\n<qresource prefix=\"/i18n\">\n")
foreach(ts_file ${TS_FILES})
    get_filename_component(qm_name ${ts_file} NAME_WE)
    if(QT_VERSION_MAJOR GREATER_EQUAL 6)
        set(qm_path "${CMAKE_CURRENT_BINARY_DIR}/${qm_name}.qm")
    else()
        get_filename_component(qm_dir "${QM_FILES}" DIRECTORY)
        set(qm_path "${qm_dir}/${qm_name}.qm")
    endif()
    if(EXISTS "${qm_path}")
        file(APPEND ${I18N_QRC} "    <file>${qm_path}</file>\n")
    endif()
endforeach()
file(APPEND ${I18N_QRC} "</qresource>\n</RCC>\n")

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_resources(scriptura "i18n" PREFIX "/" ${I18N_QRC})
else()
    qt5_add_resources(scriptura_i18n ${I18N_QRC})
    target_sources(scriptura PRIVATE ${I18N_QRC})
endif()

add_library(scriptura_core STATIC ${SCRIPTURA_CORE_SOURCES})
set_target_properties(scriptura_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Internal"
)

# Build Rust backend before scriptura_core and tests on ALL platforms
add_dependencies(scriptura_core rust_backend)

find_package(ZLIB REQUIRED)
target_link_libraries(scriptura_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::Sql
    ZLIB::ZLIB
)

if(WIN32)
    # DwmSetWindowAttribute (used by MainWindow::enableMicaEffect) lives in dwmapi.lib
    target_link_libraries(scriptura_core PRIVATE dwmapi)
    # Windows needs these Rust runtime libraries
    target_link_libraries(scriptura_core PRIVATE user32 advapi32 ws2_32 crypt32 userenv ntdll)
endif()

if(APPLE)
    # macOS needs these frameworks for Rust std
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
    # Linux needs pthread, dl, etc. for Rust std
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

# IMPORTANT: Rust library MUST come AFTER scriptura_core in the link line.
# GNU/Clang linkers resolve symbols left-to-right: a library only satisfies
# undefined references from objects/libraries that appear BEFORE it.
target_link_libraries(scriptura PRIVATE
    scriptura_core
    "${RUST_LIB_REL_PATH}"
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32)
    # Rust FFI needs bcrypt (BCryptGenRandom from getrandom crate), dwmapi, etc.
    target_link_libraries(scriptura PRIVATE bcrypt dwmapi)
    set_target_properties(scriptura PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
endif()

# Plugin system configuration
set(PLUGIN_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/scriptura/plugins")
set(PLUGIN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/plugins")

set_target_properties(scriptura PROPERTIES
    ENABLE_EXPORTS ON
    QT_PLUGIN_PATH "${PLUGIN_OUTPUT_DIR}"
)

# Plugin host executable for sandboxed plugin isolation
add_executable(scriptura_pluginhost
    src/pluginhost/pluginhost.cpp
    src/pluginhost/pluginhost.h
    # removed: src/internals/lengthprefixedframer.cpp
    # removed: src/internals/lengthprefixedframer.h
)
set_target_properties(scriptura_pluginhost PROPERTIES
    FOLDER "Internal"
    WIN32_EXECUTABLE FALSE
)
target_link_libraries(scriptura_pluginhost PRIVATE
    scriptura_core
    "${RUST_LIB_REL_PATH}"
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# Add plugin subdirectories (each has its own CMakeLists.txt)
add_subdirectory(src/plugins/git)
add_subdirectory(tests)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set(BUNDLE_ID_OPTION "")
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.scriptura)
endif()
set_target_properties(scriptura PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS scriptura
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Desktop file for Linux
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deployment/linux/scriptura.desktop")
    install(FILES deployment/linux/scriptura.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
endif()

# Packaging support
set(CPACK_PACKAGE_NAME "scriptura")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple Qt-based text editor with project file browsing")
set(CPACK_PACKAGE_VENDOR "Scriptura")
set(CPACK_PACKAGE_CONTACT "maintainer@example.com")

# Linux packaging
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/snap/snapcraft.yaml")
    set(CPACK_GENERATOR "TGZ")
endif()

# macOS DMG packaging
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Scriptura")
    set(CPACK_DMG_FORMAT "UDBZ")
endif()

# Windows NSIS packaging
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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(scriptura)
endif()
