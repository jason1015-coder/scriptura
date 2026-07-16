find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Core Network)

set(TEST_SOURCES
    test_main.cpp
    test_workspace.h
    test_workspace.cpp
    test_configvalidator.h
    test_configvalidator.cpp
    test_httpclientpanel.h
    test_httpclientpanel.cpp
    test_lengthprefixedframer.h
    test_lengthprefixedframer.cpp
    test_dependencyresolver.h
    test_dependencyresolver.cpp
    test_eventbus.h
    test_eventbus.cpp
    test_permissionmanager.h
    test_permissionmanager.cpp
    test_pluginsettings.h
    test_pluginsettings.cpp
    test_taskrunner.h
    test_taskrunner.cpp
    test_archiveextractor.h
    test_archiveextractor.cpp
    test_pluginmanager.h
    test_pluginmanager.cpp
    test_lspclient.h
    test_lspclient.cpp
    test_dapclient.h
    test_dapclient.cpp
)

# Copy zip fixtures next to the test binary so tests can locate them.
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

qt_add_executable(scriptura_tests
    ${TEST_SOURCES}
)

set_target_properties(scriptura_tests PROPERTIES
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
)

add_test(NAME scriptura_unit_tests COMMAND $<TARGET_FILE:scriptura_tests>)
