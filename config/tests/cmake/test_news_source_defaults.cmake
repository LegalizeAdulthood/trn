# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")

function(expect_equal name actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${name} was '${actual}', expected '${expected}'.")
    endif()
endfunction()

if("${TRN_TEST_SCENARIO}" STREQUAL "empty-default-fallback")
    set(SERVER_NAME "news.example.org")
    set(HAS_LOCAL_SPOOL OFF)
    configure_news_source_defaults()
    expect_equal("DEFAULT_SUBSCRIPTIONS" "${DEFAULT_SUBSCRIPTIONS}" "")

    set(SUBSCRIPTIONS "%x/subscriptions" CACHE FILEPATH "Test subscriptions default." FORCE)
    set_config_cache_default(SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}" FILEPATH
        "Test subscriptions default." "%x/subscriptions")
    expect_equal("SUBSCRIPTIONS" "${SUBSCRIPTIONS}" "")

    set(SUBSCRIPTIONS "/test/subscriptions" CACHE FILEPATH "Test subscriptions override." FORCE)
    set_config_cache_default(SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}" FILEPATH
        "Test subscriptions default." "%x/subscriptions")
    expect_equal("SUBSCRIPTIONS" "${SUBSCRIPTIONS}" "/test/subscriptions")
    return()
endif()

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

if(DEFINED TRN_TEST_PROBE_ROOT)
    file(REMOVE_RECURSE "${TRN_TEST_PROBE_ROOT}")
    set(TRN_NEWS_SOURCE_IGNORE_ENV ON)
    set(TRN_NNTP_SERVER_FILE_CANDIDATES "${TRN_TEST_PROBE_ROOT}/nntpserver")
    set(TRN_NEWS_LIB_CANDIDATES "${TRN_TEST_PROBE_ROOT}/news-lib")
    set(TRN_NEWS_SPOOL_CANDIDATES "${TRN_TEST_PROBE_ROOT}/spool")
    if(TRN_TEST_CREATE_NNTP_SOURCE)
        file(MAKE_DIRECTORY "${TRN_TEST_PROBE_ROOT}")
        file(WRITE "${TRN_TEST_PROBE_ROOT}/nntpserver" "news.example.org\n")
    endif()
    if(TRN_TEST_CREATE_LOCAL_SPOOL)
        file(MAKE_DIRECTORY "${TRN_TEST_PROBE_ROOT}/news-lib")
        file(MAKE_DIRECTORY "${TRN_TEST_PROBE_ROOT}/spool")
        file(WRITE "${TRN_TEST_PROBE_ROOT}/news-lib/active" "")
    endif()
    probe_news_source_defaults()
    expect_equal("DEFAULT_SERVER_NAME" "${DEFAULT_SERVER_NAME}" "${TRN_TEST_EXPECT_SERVER_NAME}")
    expect_equal("DEFAULT_HAS_LOCAL_SPOOL" "${DEFAULT_HAS_LOCAL_SPOOL}"
        "${TRN_TEST_EXPECT_HAS_LOCAL_SPOOL}")
    set(SERVER_NAME "${DEFAULT_SERVER_NAME}")
    set(HAS_LOCAL_SPOOL "${DEFAULT_HAS_LOCAL_SPOOL}")
else()
    set(SERVER_NAME "${TRN_TEST_SERVER_NAME}")
    set(HAS_LOCAL_SPOOL "${TRN_TEST_HAS_LOCAL_SPOOL}")
endif()
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
