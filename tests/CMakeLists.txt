find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Core Network)

file(GLOB_RECURSE TEST_SOURCES
    "test_*.cpp" "test_*.h"
)

qt_add_executable(scriptura_tests
    ${TEST_SOURCES}
)

set_target_properties(scriptura_tests PROPERTIES
    EXCLUDE_FROM_ALL TRUE
    FOLDER "Tests"
)

# Rust backend static library (scriptura_core's rust_adapter.cpp calls into it via FFI).
# RUST_LIB_REL_PATH is inherited from the parent CMakeLists.txt scope.
target_link_libraries(scriptura_tests PRIVATE
    scriptura_core
    "${RUST_LIB_REL_PATH}"
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

if(WIN32)
    target_link_libraries(scriptura_tests PRIVATE user32 advapi32 ws2_32 crypt32 userenv ntdll)
endif()

if(APPLE)
    find_library(_COREFOUNDATION CoreFoundation)
    find_library(_SECURITY Security)
    find_library(_SYSTEMCONFIG SystemConfiguration)
    target_link_libraries(scriptura_tests PRIVATE "${_COREFOUNDATION}" "${_SECURITY}" "${_SYSTEMCONFIG}")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(scriptura_tests PRIVATE pthread dl rt m)
endif()

# Ensure the Rust library is built before the test executable links
add_dependencies(scriptura_tests rust_backend)

target_include_directories(scriptura_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/internals
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

add_test(NAME scriptura_unit_tests COMMAND $<TARGET_FILE:scriptura_tests>)
