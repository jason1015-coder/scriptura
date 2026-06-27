cmake_minimum_required(VERSION 3.16)

project(scriptura_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Test)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Test)

set(TEST_SOURCES
    test_configvalidator.cpp
    ${CMAKE_SOURCE_DIR}/configvalidator.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(scriptura_tests
        ${TEST_SOURCES}
    )
else()
    add_executable(scriptura_tests
        ${TEST_SOURCES}
    )
endif()

target_link_libraries(scriptura_tests PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Test)

# Include the parent directory for headers
target_include_directories(scriptura_tests PRIVATE ${CMAKE_SOURCE_DIR})