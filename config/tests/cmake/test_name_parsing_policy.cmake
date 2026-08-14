# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")

if(TRN_TEST_EXPECT_PASS_NAMES)
    set(legacy_pass_names OFF)
else()
    set(legacy_pass_names ON)
endif()
if(TRN_TEST_EXPECT_BERKELEY_NAMES)
    set(legacy_berkeley_names OFF)
else()
    set(legacy_berkeley_names ON)
endif()
set(PASS_NAMES "${legacy_pass_names}" CACHE BOOL "Legacy cache value.")
set(BERKELEY_NAMES "${legacy_berkeley_names}" CACHE BOOL
    "Legacy cache value.")

configure_name_parsing_policy("${TRN_TEST_NAME_TYPE}")

if(DEFINED CACHE{PASS_NAMES})
    message(FATAL_ERROR "PASS_NAMES must not remain in the cache.")
endif()
if(DEFINED CACHE{BERKELEY_NAMES})
    message(FATAL_ERROR "BERKELEY_NAMES must not remain in the cache.")
endif()
if(NOT "${PASS_NAMES}" STREQUAL "${TRN_TEST_EXPECT_PASS_NAMES}")
    message(FATAL_ERROR "PASS_NAMES was not derived from NAME_TYPE.")
endif()
if(NOT "${BERKELEY_NAMES}" STREQUAL "${TRN_TEST_EXPECT_BERKELEY_NAMES}")
    message(FATAL_ERROR "BERKELEY_NAMES was not derived from NAME_TYPE.")
endif()
