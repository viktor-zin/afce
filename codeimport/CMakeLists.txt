# C / C++ -> AFC flowchart importer (QtCore + QtXml only).
# Can be configured standalone for fast iteration: cmake -S codeimport -B build-import
cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(afce_codeimport LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Core Xml)
    qt_standard_project_setup()
    include(CTest)
    add_library(afce_util STATIC ../afceutil.cpp ../afceutil.h)
    target_include_directories(afce_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
    target_link_libraries(afce_util PUBLIC Qt6::Core)
endif()

add_library(afce_codeimport STATIC
    codeimporter.cpp
    codeimporter.h
    importtokenizer.cpp
    importtokenizer.h
    importscanner.cpp
    importscanner.h
    importexpr.cpp
    importexpr.h
    importparser.cpp
    importparser.h
    importbuilder.cpp
    importbuilder.h
)
target_include_directories(afce_codeimport PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afce_codeimport PUBLIC Qt6::Core Qt6::Xml afce_util)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
