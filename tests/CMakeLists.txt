find_package(Qt6 REQUIRED COMPONENTS Test)

# Generator engine / rules
qt_add_executable(tst_generator tst_generator.cpp)
set_target_properties(tst_generator PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
target_link_libraries(tst_generator PRIVATE afce_generator Qt6::Test)
target_compile_definitions(tst_generator PRIVATE
    AFCE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AFCE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME tst_generator COMMAND tst_generator)

# Flowchart model / rendering / export (offscreen)
qt_add_executable(tst_flowchart tst_flowchart.cpp)
set_target_properties(tst_flowchart PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
target_link_libraries(tst_flowchart PRIVATE afce_model Qt6::Test)
target_compile_definitions(tst_flowchart PRIVATE
    AFCE_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    AFCE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME tst_flowchart COMMAND tst_flowchart)
set_tests_properties(tst_flowchart PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Main window: languages, opening files, generators, wheel (offscreen)
qt_add_executable(tst_mainwindow tst_mainwindow.cpp)
set_target_properties(tst_mainwindow PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
target_link_libraries(tst_mainwindow PRIVATE afce_app Qt6::Test)
target_compile_definitions(tst_mainwindow PRIVATE AFCE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME tst_mainwindow COMMAND tst_mainwindow)
set_tests_properties(tst_mainwindow PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# "Import from source code" dialog and the code editor (offscreen)
qt_add_executable(tst_importdialog tst_importdialog.cpp)
set_target_properties(tst_importdialog PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
target_link_libraries(tst_importdialog PRIVATE afce_app Qt6::Test)
target_compile_definitions(tst_importdialog PRIVATE AFCE_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME tst_importdialog COMMAND tst_importdialog)
set_tests_properties(tst_importdialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# ---------------------------------------------------------------------------
# Command line smoke tests (afce-cli and the headless modes of afce)
set(AFCE_SAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/samples)
set(AFCE_ROBUSTNESS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/robustness)
set(AFCE_TEST_OUT ${CMAKE_CURRENT_BINARY_DIR}/out)
file(MAKE_DIRECTORY ${AFCE_TEST_OUT})

add_test(NAME cli_languages COMMAND afce-cli languages)
set_tests_properties(cli_languages PROPERTIES PASS_REGULAR_EXPRESSION "(^|\n)cpp\n")
add_test(NAME cli_usage COMMAND afce-cli)
set_tests_properties(cli_usage PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_unknown_command COMMAND afce-cli frobnicate)
set_tests_properties(cli_unknown_command PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_generate_c COMMAND afce-cli generate ${AFCE_SAMPLES_DIR}/all_blocks.afc --lang c)
add_test(NAME cli_generate_old_format COMMAND afce-cli generate ${AFCE_SAMPLES_DIR}/old_format.afc --lang py)
set_tests_properties(cli_generate_old_format PROPERTIES PASS_REGULAR_EXPRESSION "a \\+ b")
add_test(NAME cli_generate_rule_file COMMAND afce-cli generate ${AFCE_SAMPLES_DIR}/gcd.afc
         --lang ${PROJECT_SOURCE_DIR}/generators/pas.json --output ${AFCE_TEST_OUT}/gcd.pas)
add_test(NAME cli_generate_unknown_language COMMAND afce-cli generate ${AFCE_SAMPLES_DIR}/gcd.afc --lang klingon)
set_tests_properties(cli_generate_unknown_language PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_generate_invalid_file COMMAND afce-cli generate ${AFCE_ROBUSTNESS_DIR}/broken.afc --lang c)
set_tests_properties(cli_generate_invalid_file PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_import_missing_file COMMAND afce-cli import ${AFCE_TEST_OUT}/does_not_exist.c)
set_tests_properties(cli_import_missing_file PROPERTIES WILL_FAIL TRUE)
add_test(NAME cli_import_bad_option COMMAND afce-cli import ${AFCE_SAMPLES_DIR}/gcd.afc --for-style sideways)
set_tests_properties(cli_import_bad_option PROPERTIES WILL_FAIL TRUE)

set(AFCE_SOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data/sources)
add_test(NAME cli_functions COMMAND afce-cli functions ${AFCE_SOURCES_DIR}/max_of_two.c --json)
set_tests_properties(cli_functions PROPERTIES PASS_REGULAR_EXPRESSION "\"name\": \"main\"")
add_test(NAME cli_import COMMAND afce-cli import ${AFCE_SOURCES_DIR}/max_of_two.c --output ${AFCE_TEST_OUT}/max_of_two.afc)
add_test(NAME cli_import_generate COMMAND afce-cli import ${AFCE_SOURCES_DIR}/max_of_two.c --function max --generate py)
add_test(NAME cli_import_unknown_function COMMAND afce-cli import ${AFCE_SOURCES_DIR}/max_of_two.c --function nosuch)
set_tests_properties(cli_import_unknown_function PROPERTIES WILL_FAIL TRUE)
add_test(NAME gui_import_export
         COMMAND afce --import ${AFCE_SOURCES_DIR}/max_of_two.c --export ${AFCE_TEST_OUT}/max_of_two.png)

foreach(format png svg pdf)
    add_test(NAME gui_export_${format}
             COMMAND afce ${AFCE_SAMPLES_DIR}/all_blocks.afc --export ${AFCE_TEST_OUT}/all_blocks.${format} --zoom 1.5)
endforeach()
add_test(NAME gui_export_invalid_file
         COMMAND afce ${AFCE_ROBUSTNESS_DIR}/not_afc.xml --export ${AFCE_TEST_OUT}/invalid.png)
set_tests_properties(gui_export_invalid_file PROPERTIES WILL_FAIL TRUE)
add_test(NAME gui_screenshot
         COMMAND afce ${AFCE_SAMPLES_DIR}/max_of_two.afc --screenshot ${AFCE_TEST_OUT}/screenshot.png --language en_US)
set_tests_properties(gui_export_png gui_export_svg gui_export_pdf gui_export_invalid_file gui_screenshot
                     gui_import_export PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Runs that show no window must work without a display (SSH, CI): afce selects
# the offscreen platform itself. (Not on Windows: there a GUI program shows
# --help / --version in a message box.)
if(NOT WIN32)
    set(AFCE_NO_DISPLAY ${CMAKE_COMMAND} -E env --unset=DISPLAY --unset=WAYLAND_DISPLAY --unset=QT_QPA_PLATFORM)
    add_test(NAME gui_version_without_display COMMAND ${AFCE_NO_DISPLAY} $<TARGET_FILE:afce> --version)
    set_tests_properties(gui_version_without_display PROPERTIES
        PASS_REGULAR_EXPRESSION "afce ${PROJECT_VERSION}")
    add_test(NAME gui_help_without_display COMMAND ${AFCE_NO_DISPLAY} $<TARGET_FILE:afce> --help --language en_US)
    set_tests_properties(gui_help_without_display PROPERTIES PASS_REGULAR_EXPRESSION "--export")
    add_test(NAME gui_bad_arguments_without_display
             COMMAND ${AFCE_NO_DISPLAY} $<TARGET_FILE:afce> a.afc b.afc --language en_US)
    set_tests_properties(gui_bad_arguments_without_display PROPERTIES PASS_REGULAR_EXPRESSION "Too many arguments")
    add_test(NAME gui_bad_option_without_display COMMAND ${AFCE_NO_DISPLAY} $<TARGET_FILE:afce> --frobnicate)
    set_tests_properties(gui_bad_option_without_display PROPERTIES PASS_REGULAR_EXPRESSION "frobnicate")
    # The command line is parsed after the translators are installed: the help
    # (Qt's strings, from the embedded qtbase_ru.qm) follows --language.
    if(AFCE_QT_QM_FILES MATCHES "qtbase_ru\\.qm")
        add_test(NAME gui_help_translated COMMAND ${AFCE_NO_DISPLAY} $<TARGET_FILE:afce> --help --language ru_RU)
        set_tests_properties(gui_help_translated PROPERTIES PASS_REGULAR_EXPRESSION "справк")
    endif()
endif()

# afce and afce-cli share the user data directory (here: XDG_DATA_HOME on
# Linux; macOS and Windows do not allow redirecting it for a test).
if(UNIX AND NOT APPLE)
    set(AFCE_TEST_XDG ${AFCE_TEST_OUT}/xdg)
    configure_file(${PROJECT_SOURCE_DIR}/generators/py.json
                   ${AFCE_TEST_XDG}/afce/generators/afcetestlang.json COPYONLY)
    add_test(NAME cli_user_generators COMMAND afce-cli languages)
    add_test(NAME cli_user_generators_generate
             COMMAND afce-cli generate ${AFCE_SAMPLES_DIR}/gcd.afc --lang afcetestlang)
    set_tests_properties(cli_user_generators cli_user_generators_generate PROPERTIES
        ENVIRONMENT "XDG_DATA_HOME=${AFCE_TEST_XDG}")
    set_tests_properties(cli_user_generators PROPERTIES PASS_REGULAR_EXPRESSION "(^|\n)afcetestlang\n")
endif()

# Desktop integration files (Linux packages); checked wherever the tools exist.
find_program(AFCE_UPDATE_MIME_DATABASE update-mime-database)
if(AFCE_UPDATE_MIME_DATABASE)
    add_test(NAME mime_type
             COMMAND ${CMAKE_COMMAND} -DUPDATE_MIME_DATABASE=${AFCE_UPDATE_MIME_DATABASE}
                     -DMIME_FILE=${PROJECT_SOURCE_DIR}/afce.xml -DWORK_DIR=${AFCE_TEST_OUT}/mime
                     -P ${PROJECT_SOURCE_DIR}/cmake/CheckMimeType.cmake)
endif()
find_program(AFCE_DESKTOP_FILE_VALIDATE desktop-file-validate)
if(AFCE_DESKTOP_FILE_VALIDATE)
    add_test(NAME desktop_file COMMAND ${AFCE_DESKTOP_FILE_VALIDATE} ${PROJECT_SOURCE_DIR}/afce.desktop)
    # desktop-file-validate returns 0 for warnings as well
    set_tests_properties(desktop_file PROPERTIES FAIL_REGULAR_EXPRESSION "error|warning")
endif()

# Script based suites of the other components register themselves here.
foreach(suite generators roundtrip)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${suite}/CMakeLists.txt)
        add_subdirectory(${suite})
    endif()
endforeach()
