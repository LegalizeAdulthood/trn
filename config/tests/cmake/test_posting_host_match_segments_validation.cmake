# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")
if(TRN_TEST_EXPECT_FAILURE)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${TRN_CONFIGURE_TRN_CMAKE}"
            "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=${TRN_TEST_POSTING_HOST_MATCH_SEGMENTS}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    string(CONCAT combined_output "${output}" "${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "Expected POSTING_HOST_MATCH_SEGMENTS validation to fail.")
    endif()
    if(NOT combined_output MATCHES "POSTING_HOST_MATCH_SEGMENTS"
        OR NOT combined_output MATCHES "non-negative decimal")
        message(FATAL_ERROR
            "POSTING_HOST_MATCH_SEGMENTS validation did not report the expected diagnostic.")
    endif()
    return()
endif()

validate_posting_host_match_segments("${TRN_TEST_POSTING_HOST_MATCH_SEGMENTS}")
