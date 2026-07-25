cmake_minimum_required(VERSION 3.16)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 尋找 Qt（支援 Qt5 和 Qt6）
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

# 建立插件共享庫
add_library(gitplugin SHARED
    gitplugin.cpp
    gitplugin.h
)

# 設定目標屬性
set_target_properties(gitplugin PROPERTIES
    OUTPUT_NAME gitplugin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/git
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins/git
)

# 連結 Qt 庫
target_link_libraries(gitplugin PRIVATE
    scriptura_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

# On Windows, the plugin DLL must link the Rust backend directly to resolve
# FFI symbols from rust_adapter (compiled into scriptura_core).
# The Rust backend is a staticlib (scriptura_backend.lib).
if(WIN32)
    # Ensure the Rust backend is built before the plugin links
    add_dependencies(gitplugin rust_backend)
    # RUST_LIB_REL_PATH is inherited from the parent CMakeLists.txt scope
    target_link_libraries(gitplugin PRIVATE
        "${RUST_LIB_REL_PATH}"
        bcrypt dwmapi user32 advapi32 ws2_32 crypt32 userenv ntdll
    )
endif()

# Ensure ScripturaPlugin symbols are visible on macOS
if(APPLE)
    set_target_properties(gitplugin PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif()

# 包含目錄（包含 Scriptura 標頭）
target_include_directories(gitplugin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/plugins/git
)

# 安裝規則
install(TARGETS gitplugin
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/scriptura/plugins/git
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBDIR}/scriptura/plugins/git
)

# 安裝元數據
install(FILES git.json
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/scriptura/plugins/git
)

# 安裝資源（如果有）
# install(DIRECTORY resources/
#     DESTINATION ${CMAKE_INSTALL_DATADIR}/scriptura/plugins/git/resources
# )

# Windows 特定設定
if(WIN32)
    set_target_properties(gitplugin PROPERTIES
        SUFFIX ".dll"
    )
endif()

# macOS 特定設定
if(APPLE)
    set_target_properties(gitplugin PROPERTIES
        SUFFIX ".dylib"
    )
endif()

# Linux 特定設定
if(UNIX AND NOT APPLE)
    set_target_properties(gitplugin PROPERTIES
        SUFFIX ".so"
    )
endif()