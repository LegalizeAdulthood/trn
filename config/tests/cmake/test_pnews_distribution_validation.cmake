# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")
if(TRN_TEST_EXPECT_FAILURE)
    set(validation_args "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${TRN_CONFIGURE_TRN_CMAKE}")
    if(DEFINED TRN_TEST_NAME_TYPE)
        list(APPEND validation_args "-DTRN_TEST_NAME_TYPE=${TRN_TEST_NAME_TYPE}")
    endif()
    if(DEFINED TRN_TEST_DISTRIBUTION_NAME)
        list(APPEND validation_args "-DTRN_TEST_DISTRIBUTION_NAME=${TRN_TEST_DISTRIBUTION_NAME}")
        list(APPEND validation_args "-DTRN_TEST_DISTRIBUTION_VALUE=${TRN_TEST_DISTRIBUTION_VALUE}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            ${validation_args}
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    string(CONCAT combined_output "${output}" "${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "Expected Pnews distribution validation to fail.")
    endif()
    if(DEFINED TRN_TEST_NAME_TYPE)
        if(NOT combined_output MATCHES "NAME_TYPE"
            OR NOT combined_output MATCHES "bsd, usg, or other")
            message(FATAL_ERROR "NAME_TYPE validation did not report the expected diagnostic.")
        endif()
    endif()
    if(DEFINED TRN_TEST_DISTRIBUTION_NAME)
        if(NOT combined_output MATCHES "${TRN_TEST_DISTRIBUTION_NAME}"
            OR NOT combined_output MATCHES "non-empty")
            message(FATAL_ERROR "Distribution code validation did not report the expected diagnostic.")
        endif()
    endif()
    return()
endif()

if(DEFINED TRN_TEST_NAME_TYPE)
    validate_name_type("${TRN_TEST_NAME_TYPE}")
endif()
if(DEFINED TRN_TEST_DISTRIBUTION_NAME)
    validate_distribution_code("${TRN_TEST_DISTRIBUTION_NAME}" "${TRN_TEST_DISTRIBUTION_VALUE}")
endif()
