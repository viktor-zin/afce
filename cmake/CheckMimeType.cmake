# Checks the shared-mime-info package file (afce.xml) with update-mime-database:
# the file must be accepted without errors and register both the *.afc glob
# and the <algorithm> root element sniffing rule.
#
# cmake -DUPDATE_MIME_DATABASE=<tool> -DMIME_FILE=<afce.xml> -DWORK_DIR=<dir> -P CheckMimeType.cmake

foreach(var UPDATE_MIME_DATABASE MIME_FILE WORK_DIR)
    if(NOT ${var})
        message(FATAL_ERROR "${var} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/packages")
configure_file("${MIME_FILE}" "${WORK_DIR}/packages/afce.xml" COPYONLY)

execute_process(
    COMMAND "${UPDATE_MIME_DATABASE}" -V "${WORK_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "update-mime-database failed (${result}):\n${output}")
endif()
if(output MATCHES "Error|error:")
    message(FATAL_ERROR "update-mime-database reported errors:\n${output}")
endif()

file(READ "${WORK_DIR}/globs2" globs)
if(NOT globs MATCHES "application/x-afce:\\*\\.afc")
    message(FATAL_ERROR "The *.afc glob is not registered:\n${globs}")
endif()
file(READ "${WORK_DIR}/XMLnamespaces" namespaces)
if(NOT namespaces MATCHES "algorithm application/x-afce")
    message(FATAL_ERROR "The <algorithm> root-XML rule is not registered:\n'${namespaces}'")
endif()
message(STATUS "afce.xml OK")
