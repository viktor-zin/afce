find_package(Qt6 REQUIRED COMPONENTS Test)

# Shared helpers: compact AFC serializer + structural validator.
add_library(afce_importtestutil STATIC importtestutil.cpp importtestutil.h)
target_include_directories(afce_importtestutil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(afce_importtestutil PUBLIC afce_codeimport Qt6::Xml)

# Test classes are declared in headers: moc cannot lex some of the raw string
# literals (C source snippets) used in the test bodies.
function(afce_import_test name)
    qt_add_executable(${name} ${name}.cpp ${name}.h)
    set_target_properties(${name} PROPERTIES MACOSX_BUNDLE OFF WIN32_EXECUTABLE OFF)
    target_link_libraries(${name} PRIVATE afce_importtestutil Qt6::Test)
    target_compile_definitions(${name} PRIVATE IMPORT_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")
    add_test(NAME ${name} COMMAND ${name})
endfunction()

afce_import_test(tst_codeimport)            # statement mapping and options
afce_import_test(tst_codeimport_tokenizer)  # tokenizer / token stream
afce_import_test(tst_codeimport_functions)  # function discovery
afce_import_test(tst_codeimport_corpus)     # realistic programs in data/
afce_import_test(tst_codeimport_fuzz)       # robustness: truncation, mutation, deep nesting, size
set_tests_properties(tst_codeimport_fuzz PROPERTIES TIMEOUT 900)
