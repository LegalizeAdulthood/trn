# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")

function(expect_equal name actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${name} was '${actual}', expected '${expected}'.")
    endif()
endfunction()

if(TRN_TEST_EXPECT_FAILURE)
    execute_process(
        COMMAND "${CMAKE_COMMAND}"
            "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${TRN_CONFIGURE_TRN_CMAKE}"
            "-DTRN_TEST_SERVER_NAME=${TRN_TEST_SERVER_NAME}"
            "-DTRN_TEST_HAS_LOCAL_SPOOL=${TRN_TEST_HAS_LOCAL_SPOOL}"
            -P "${CMAKE_CURRENT_LIST_FILE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    string(CONCAT combined_output "${output}" "${error}")
    if(result EQUAL 0)
        message(FATAL_ERROR "Expected news-source default validation to fail.")
    endif()
    if(NOT combined_output MATCHES "SERVER_NAME" OR NOT combined_output MATCHES "HAS_LOCAL_SPOOL")
        message(FATAL_ERROR "News-source validation did not report the expected diagnostic.")
    endif()
    return()
endif()

set(SERVER_NAME "${TRN_TEST_SERVER_NAME}")
set(HAS_LOCAL_SPOOL "${TRN_TEST_HAS_LOCAL_SPOOL}")
configure_news_source_defaults()
set(NEWS_SPOOL "${DEFAULT_NEWS_SPOOL}")
configure_news_overview_dir_default()
set(OVERVIEW_DIR "${DEFAULT_OVERVIEW_DIR}")
configure_news_overview_format_default()

expect_equal("DEFAULT_ACTIVE" "${DEFAULT_ACTIVE}" "${TRN_TEST_EXPECT_ACTIVE}")
expect_equal("DEFAULT_GROUP_DESC" "${DEFAULT_GROUP_DESC}" "${TRN_TEST_EXPECT_GROUP_DESC}")
expect_equal("DEFAULT_SUBSCRIPTIONS" "${DEFAULT_SUBSCRIPTIONS}" "${TRN_TEST_EXPECT_SUBSCRIPTIONS}")
expect_equal("DEFAULT_NEWS_SPOOL" "${DEFAULT_NEWS_SPOOL}" "${TRN_TEST_EXPECT_NEWS_SPOOL}")
expect_equal("DEFAULT_OVERVIEW_DIR" "${DEFAULT_OVERVIEW_DIR}" "${TRN_TEST_EXPECT_OVERVIEW_DIR}")
expect_equal("DEFAULT_OVERVIEW_FMT" "${DEFAULT_OVERVIEW_FMT}" "${TRN_TEST_EXPECT_OVERVIEW_FMT}")
