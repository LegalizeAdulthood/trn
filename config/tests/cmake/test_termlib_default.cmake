# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")

function(expect_equal name actual expected)
    if(NOT "${actual}" STREQUAL "${expected}")
        message(FATAL_ERROR "${name} was '${actual}', expected '${expected}'.")
    endif()
endfunction()

if(DEFINED TRN_TEST_HAS_TERMLIB_OVERRIDE)
    set(DEFAULT_HAS_TERMLIB "${TRN_TEST_DEFAULT_HAS_TERMLIB}")
    set(HAS_TERMLIB "${TRN_TEST_HAS_TERMLIB_OVERRIDE}" CACHE BOOL
        "Indicates that termlib-style routines are available")
    configure_termlib_cache_default()
    expect_equal("HAS_TERMLIB" "${HAS_TERMLIB}" "${TRN_TEST_EXPECT_HAS_TERMLIB}")
    return()
endif()

configure_termlib_default("${TRN_TEST_PROBED_HAS_TERMLIB}")
expect_equal("DEFAULT_HAS_TERMLIB" "${DEFAULT_HAS_TERMLIB}" "${TRN_TEST_EXPECT_HAS_TERMLIB}")
