find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test Widgets Core Network)

set(TEST_SOURCES
    test_main.cpp
    # Existing tests
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
    # New tests — Internals
    test_crashhandler.h
    test_crashhandler.cpp
    test_plugincrashhandler.h
    test_plugincrashhandler.cpp
    test_plugincontext.h
    test_plugincontext.cpp
    test_servicelocator.h
    test_servicelocator.cpp
    test_versionfetcher.h
    test_versionfetcher.cpp
    test_updater.h
    test_updater.cpp
    # New tests — Panels
    test_findreplace.h
    test_findreplace.cpp
    test_problempanel.h
    test_problempanel.cpp
    test_terminalpanel.h
    test_terminalpanel.cpp
    test_todopanel.h
    test_todopanel.cpp
    test_gitpanel.h
    test_gitpanel.cpp
    # New tests — Plugins
    test_pluginregistry.h
    test_pluginregistry.cpp
    test_codeactionui.h
    test_codeactionui.cpp
    # New tests — Services
    test_debugconfiguration.h
    test_debugconfiguration.cpp
    test_debuggergutter.h
    test_debuggergutter.cpp
    test_projectsearch.h
    test_projectsearch.cpp
    # New tests — Widgets
    test_breadcrumb.h
    test_breadcrumb.cpp
    test_splitmanager.h
    test_splitmanager.cpp
    test_commandpalette.h
    test_commandpalette.cpp
    test_minimap.h
    test_minimap.cpp
    # New tests — Panels (git)
    test_gitbranchwidget.h
    test_gitbranchwidget.cpp
    test_gitdiffwidget.h
    test_gitdiffwidget.cpp
    test_gitmergewidget.h
    test_gitmergewidget.cpp
    # New tests — Plugins
    test_aiinlinecompletion.h
    test_aiinlinecompletion.cpp
    test_sqliteviewer.h
    test_sqliteviewer.cpp
    # New tests — Core
    test_themeicons.h
    test_themeicons.cpp
    test_thememanager.h
    test_thememanager.cpp
    test_windowanimator.h
    test_windowanimator.cpp
    # New tests — Internals (continued)
    test_pluginupdater.h
    test_pluginupdater.cpp
    test_rundialog.h
    test_rundialog.cpp
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
