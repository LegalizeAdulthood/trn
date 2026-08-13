# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")
if(TRN_TEST_EXPECT_FAILURE)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${TRN_CONFIGURE_TRN_CMAKE}"
            "-DTRN_TEST_UNSUBSCRIBED_CHAR=${TRN_TEST_UNSUBSCRIBED_CHAR}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    string(CONCAT combined_output "${output}" "${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "Expected UNSUBSCRIBED_CHAR validation to fail.")
    endif()
    if(NOT combined_output MATCHES "UNSUBSCRIBED_CHAR" OR NOT combined_output MATCHES "single character")
        message(FATAL_ERROR "UNSUBSCRIBED_CHAR validation did not report the expected diagnostic.")
    endif()
    return()
endif()

validate_unsubscribed_char("${TRN_TEST_UNSUBSCRIBED_CHAR}")
