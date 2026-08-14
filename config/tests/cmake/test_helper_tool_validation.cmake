# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_DATA_CMAKE}")
if(TRN_TEST_EXPECT_FAILURE)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_CONFIGURE_DATA_CMAKE:FILEPATH=${TRN_CONFIGURE_DATA_CMAKE}"
            "-DTRN_TEST_HELPER_TOOL_NAME=${TRN_TEST_HELPER_TOOL_NAME}"
            "-DTRN_TEST_HELPER_TOOL_VALUE=${TRN_TEST_HELPER_TOOL_VALUE}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    string(CONCAT combined_output "${output}" "${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "Expected helper tool validation to fail.")
    endif()
    if(NOT combined_output MATCHES "${TRN_TEST_HELPER_TOOL_NAME}"
        OR NOT combined_output MATCHES "must not be empty")
        message(FATAL_ERROR "Helper tool validation did not report the expected diagnostic.")
    endif()
    return()
endif()

validate_helper_tool_value("${TRN_TEST_HELPER_TOOL_NAME}" "${TRN_TEST_HELPER_TOOL_VALUE}")
