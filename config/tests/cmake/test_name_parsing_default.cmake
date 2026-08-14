# Copyright (c) 2026, trn project

include("${TRN_CONFIGURE_TRN_CMAKE}")

function(expect_name_parsing_policy name_type expected_pass_names expected_berkeley_names)
    validate_name_parsing_support("${name_type}" "${TRN_TEST_HAS_GETPWENT}")
    if(NOT "${PASS_NAMES}" STREQUAL "${expected_pass_names}")
        message(FATAL_ERROR "PASS_NAMES was not derived from the default NAME_TYPE.")
    endif()
    if(NOT "${BERKELEY_NAMES}" STREQUAL "${expected_berkeley_names}")
        message(FATAL_ERROR
            "BERKELEY_NAMES was not derived from the default NAME_TYPE.")
    endif()
endfunction()

if(TRN_TEST_EXPECT_FAILURE)
    set(validation_args
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${TRN_CONFIGURE_TRN_CMAKE}"
        "-DTRN_TEST_NAME_TYPE=${TRN_TEST_NAME_TYPE}"
        "-DTRN_TEST_HAS_GETPWENT=${TRN_TEST_HAS_GETPWENT}"
        -P "${CMAKE_CURRENT_LIST_FILE}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${validation_args}
        RESULT_VARIABLE validation_result
        OUTPUT_VARIABLE validation_output
        ERROR_VARIABLE validation_error)
    string(CONCAT combined_output "${validation_output}" "${validation_error}")
    if(validation_result EQUAL 0)
        message(FATAL_ERROR "NAME_TYPE validation unexpectedly succeeded.")
    endif()
    if(NOT combined_output MATCHES "NAME_TYPE=${TRN_TEST_NAME_TYPE}"
            OR NOT combined_output MATCHES "getpwent")
        message(FATAL_ERROR "NAME_TYPE validation did not report getpwent.")
    endif()
    return()
endif()

if(DEFINED TRN_TEST_NAME_TYPE)
    validate_name_parsing_support("${TRN_TEST_NAME_TYPE}" "${TRN_TEST_HAS_GETPWENT}")
    return()
endif()

configure_name_parsing_default(
    "${TRN_TEST_HAS_GETPWENT}"
    "${TRN_TEST_PROBED_NAME_TYPE}")

if(NOT "${DEFAULT_NAME_TYPE}" STREQUAL "${TRN_TEST_EXPECT_NAME_TYPE}")
    message(FATAL_ERROR "DEFAULT_NAME_TYPE did not match the probed name type.")
endif()

if("${TRN_TEST_EXPECT_NAME_TYPE}" STREQUAL "bsd")
    expect_name_parsing_policy("${DEFAULT_NAME_TYPE}" ON ON)
elseif("${TRN_TEST_EXPECT_NAME_TYPE}" STREQUAL "usg")
    expect_name_parsing_policy("${DEFAULT_NAME_TYPE}" ON OFF)
else()
    expect_name_parsing_policy("${DEFAULT_NAME_TYPE}" OFF OFF)
endif()
