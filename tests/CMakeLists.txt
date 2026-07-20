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

target_link_libraries(scriptura_tests PRIVATE
    scriptura_core
    Qt${QT_VERSION_MAJOR}::Test
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
)

target_include_directories(scriptura_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/internals
    ${CMAKE_SOURCE_DIR}/src/plugins
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

add_test(NAME scriptura_unit_tests COMMAND $<TARGET_FILE:scriptura_tests>)
