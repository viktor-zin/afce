cmake_minimum_required(VERSION 3.21)

# The program version lives in version.txt (single source of truth).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt" AFCE_VERSION_STRING LIMIT_COUNT 1)
string(STRIP "${AFCE_VERSION_STRING}" AFCE_VERSION_STRING)
if(NOT AFCE_VERSION_STRING MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "version.txt must contain a version like 1.2.3, got '${AFCE_VERSION_STRING}'")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/version.txt")

project(afce
    VERSION ${AFCE_VERSION_STRING}
    DESCRIPTION "Algorithm Flowchart Editor"
    HOMEPAGE_URL "https://github.com/Heisenberg4441/afce-reimagined"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(GNUInstallDirs)

# Qt 6.2 is the oldest supported version (Ubuntu 22.04 ships 6.2.4, Ubuntu 24.04
# and Debian 12 ship 6.4.2). Newer Qt APIs must be guarded with QT_VERSION_CHECK.
find_package(Qt6 6.2 REQUIRED COMPONENTS Core Gui Widgets Xml Svg PrintSupport LinguistTools)
if(COMMAND qt_standard_project_setup) # Qt 6.3+
    qt_standard_project_setup()
else()
    set(CMAKE_AUTOMOC ON)
endif()

include(CTest)

option(AFCE_DEPLOY_QT "Bundle the Qt libraries on 'cmake --install' (macdeployqt / windeployqt)" OFF)

set(AFCE_BUNDLE_ID "io.github.viktor-zin.afce")

# The same main-thread stack as on Linux and macOS (8 MB instead of Windows' 1 MB): deeply
# nested flowcharts and sources are processed recursively (by AFCE and by QtXml).
if(MSVC)
    add_link_options(/STACK:8388608)
elseif(MINGW)
    add_link_options(-Wl,--stack,8388608)
endif()

# Compiler warnings for all targets of the project (C++ only: not for the Windows resource compiler).
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra>")
elseif(MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/W3;/utf-8>")
endif()

# ---------------------------------------------------------------------------
# afce_util: QtCore-only helpers shared by every target
add_library(afce_util STATIC afceutil.cpp afceutil.h)
target_include_directories(afce_util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afce_util PUBLIC Qt6::Core)
target_compile_definitions(afce_util PRIVATE PROGRAM_VERSION="${PROJECT_VERSION}")
if(UNIX AND NOT APPLE)
    # system-wide overrides of the embedded generators / help (optional)
    target_compile_definitions(afce_util PRIVATE PROGRAM_DATA_DIR="${CMAKE_INSTALL_FULL_DATADIR}/afce/")
endif()

# ---------------------------------------------------------------------------
# afce_generator: flowchart -> source code (JSON rules embedded as :/generators/)
add_library(afce_generator STATIC sourcecodegenerator.cpp sourcecodegenerator.h)
target_include_directories(afce_generator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afce_generator PUBLIC Qt6::Core Qt6::Xml afce_util)
file(GLOB AFCE_GENERATOR_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS generators/*.json)
qt_add_resources(afce_generator "afce_generators" PREFIX "/" FILES ${AFCE_GENERATOR_FILES})

# ---------------------------------------------------------------------------
# afce_codeimport: C / C++ source -> flowchart
add_subdirectory(codeimport)

# ---------------------------------------------------------------------------
# afce_model: flowchart model, rendering and export (QtWidgets)
add_library(afce_model STATIC
    zvflowchart.cpp zvflowchart.h
    qflowchartstyle.cpp qflowchartstyle.h
    flowchartexport.cpp flowchartexport.h
)
file(GLOB AFCE_MODEL_EXTRA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/blockcatalog.cpp ${CMAKE_CURRENT_SOURCE_DIR}/blockcatalog.h)
if(AFCE_MODEL_EXTRA_SOURCES)
    target_sources(afce_model PRIVATE ${AFCE_MODEL_EXTRA_SOURCES})
endif()
target_include_directories(afce_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afce_model PUBLIC Qt6::Widgets Qt6::Xml Qt6::Svg Qt6::PrintSupport afce_util)

# ---------------------------------------------------------------------------
# afce-cli: console tool (list functions / import / generate)
qt_add_executable(afce-cli cli/main.cpp)
set_target_properties(afce-cli PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
target_link_libraries(afce-cli PRIVATE afce_generator afce_codeimport)

# ---------------------------------------------------------------------------
# afce_app: the GUI application except main() (also linked into the GUI tests)
add_library(afce_app OBJECT
    mainwindow.cpp mainwindow.h
    appicons.cpp appicons.h
    thelpwindow.cpp thelpwindow.h
    blockeditdialog.cpp blockeditdialog.h
    importdialog.cpp importdialog.h
    codeeditor.cpp codeeditor.h
)
target_include_directories(afce_app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afce_app PUBLIC afce_model afce_generator afce_codeimport)

# Images (:/images/...): the file list of afce.qrc. qt_add_resources instead of
# AUTORCC, which does not work for OBJECT libraries with the Visual Studio generator.
file(STRINGS afce.qrc AFCE_QRC_LINES REGEX "<file>")
set(AFCE_IMAGE_FILES)
foreach(line IN LISTS AFCE_QRC_LINES)
    string(REGEX REPLACE ".*<file>(.*)</file>.*" "\\1" file "${line}")
    list(APPEND AFCE_IMAGE_FILES "${file}")
endforeach()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS afce.qrc)
qt_add_resources(afce_app "afce_images" PREFIX "/" FILES ${AFCE_IMAGE_FILES})

file(GLOB_RECURSE AFCE_HELP_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS help/*)
qt_add_resources(afce_app "afce_help" PREFIX "/" FILES ${AFCE_HELP_FILES})

# Compiled translations are embedded as :/i18n/afce_<locale>.qm
# (lupdate scans the sources of all targets of the project: update_translations)
set(QT_SKIP_STORE_LANGUAGES_FROM_TS_FILES ON) # CFBundleLocalizations is set in cmake/Info.plist.in
set(AFCE_TS_FILES locale/afce_en_US.ts locale/afce_ru_RU.ts locale/afce_uk_UA.ts)
qt_add_translations(afce_app
    TS_FILES ${AFCE_TS_FILES}
    RESOURCE_PREFIX "/i18n")

# Qt's own translations (standard buttons Save/Discard/Cancel, standard dialogs,
# context menus, command line help) are embedded as :/i18n/qtbase_<language>.qm:
# macdeployqt does not deploy them and many Linux systems do not install them.
# They are optional: without them these strings stay in English.
set(AFCE_QT_TRANSLATIONS_DIR "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}" CACHE PATH
    "Directory with Qt's own translations (qtbase_<language>.qm) to embed into afce")
set(AFCE_QT_QM_FILES "")
set(AFCE_QT_QM_MISSING "")
foreach(ts_file IN LISTS AFCE_TS_FILES)
    if(ts_file MATCHES "afce_([a-z]+)_[A-Z]+\\.ts$" AND NOT CMAKE_MATCH_1 STREQUAL "en")
        set(qm_file "${AFCE_QT_TRANSLATIONS_DIR}/qtbase_${CMAKE_MATCH_1}.qm")
        if(EXISTS "${qm_file}")
            list(APPEND AFCE_QT_QM_FILES "${qm_file}")
        else()
            list(APPEND AFCE_QT_QM_MISSING "qtbase_${CMAKE_MATCH_1}.qm")
        endif()
    endif()
endforeach()
if(AFCE_QT_QM_FILES)
    qt_add_resources(afce_app "afce_qt_translations" PREFIX "/i18n"
        BASE "${AFCE_QT_TRANSLATIONS_DIR}" FILES ${AFCE_QT_QM_FILES})
endif()
if(AFCE_QT_QM_MISSING)
    list(JOIN AFCE_QT_QM_MISSING ", " missing)
    message(STATUS "Qt translations not found in ${AFCE_QT_TRANSLATIONS_DIR}: ${missing}. "
                   "Standard buttons and dialogs will be in English unless Qt's translations are installed "
                   "(Debian/Ubuntu: qt6-translations-l10n, Homebrew: qttranslations) "
                   "or AFCE_QT_TRANSLATIONS_DIR points to them.")
endif()

# ---------------------------------------------------------------------------
# afce: GUI application
qt_add_executable(afce WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(afce PRIVATE afce_app)

if(APPLE)
    set(AFCE_ICNS ${CMAKE_CURRENT_SOURCE_DIR}/afce.icns)
    set_source_files_properties(${AFCE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(afce PRIVATE ${AFCE_ICNS})
    set_target_properties(afce PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in
        MACOSX_BUNDLE_BUNDLE_NAME "AFCE"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${AFCE_BUNDLE_ID}"
        MACOSX_BUNDLE_ICON_FILE afce.icns
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright 2008-2026 Victor Zinkevich and AFCE contributors. GPL.")
elseif(WIN32)
    target_sources(afce PRIVATE afce.rc)
endif()

# ---------------------------------------------------------------------------
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# lupdate (target update_translations) scans every target of the project except these:
# the console tool prints English messages only, tests have nothing to translate.
foreach(target afce-cli tst_generator tst_flowchart tst_mainwindow tst_codeimport)
    if(TARGET ${target})
        set_property(TARGET ${target} PROPERTY QT_EXCLUDE_FROM_TRANSLATION ON)
    endif()
endforeach()

# ---------------------------------------------------------------------------
install(TARGETS afce
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
if(APPLE AND AFCE_DEPLOY_QT)
    # The self-contained bundle carries the console tool as well (it is deployed
    # together with afce below, so that it uses the bundled Qt frameworks):
    # afce.app/Contents/MacOS/afce-cli
    install(TARGETS afce-cli RUNTIME DESTINATION afce.app/Contents/MacOS)
else()
    install(TARGETS afce-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(UNIX AND NOT APPLE)
    install(FILES afce.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES afce.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps)
    install(FILES images/appicon.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps RENAME afce.svg)
    install(FILES afce.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/mimetypes
            RENAME application-x-afce.png)
    install(FILES afce.png DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps)
    install(FILES afce.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
endif()
if(AFCE_DEPLOY_QT AND (APPLE OR WIN32))
    if(NOT QT_DEPLOY_SUPPORT)
        message(FATAL_ERROR "AFCE_DEPLOY_QT needs Qt 6.3 or newer.")
    endif()
    if(APPLE)
        # macdeployqt for the bundle and for the console tool inside it
        # (qt_generate_deploy_app_script cannot deploy additional executables)
        set(AFCE_DEPLOY_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/afce_deploy_$<CONFIG>.cmake")
        file(GENERATE OUTPUT "${AFCE_DEPLOY_SCRIPT}" CONTENT "
include(\"${QT_DEPLOY_SUPPORT}\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"afce.app\"
    ADDITIONAL_EXECUTABLES \"afce.app/Contents/MacOS/$<TARGET_FILE_NAME:afce-cli>\")
")
    else()
        # windeployqt puts the Qt DLLs into bin/, next to afce-cli.exe as well
        qt_generate_deploy_app_script(TARGET afce
            OUTPUT_SCRIPT AFCE_DEPLOY_SCRIPT
            NO_UNSUPPORTED_PLATFORM_ERROR)
    endif()
    install(SCRIPT "${AFCE_DEPLOY_SCRIPT}")
endif()
if(WIN32 AND AFCE_DEPLOY_QT)
    # the MSVC runtime DLLs next to afce.exe: the portable package needs no vc_redist
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
endif()
