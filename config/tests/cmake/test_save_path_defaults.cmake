# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

include("${TRN_CONFIGURE_TRN_CMAKE}")
configure_save_path_defaults("${TRN_TEST_MAKE_SAVE_DIRECTORIES}")

if(TRN_TEST_MAKE_SAVE_DIRECTORIES)
    if(NOT DEFAULT_SAVEDIR STREQUAL "%p/%c")
        message(FATAL_ERROR "Category save mode did not set SAVEDIR to %p/%c.")
    endif()
    if(NOT DEFAULT_SAVENAME STREQUAL "%a")
        message(FATAL_ERROR "Category save mode did not set SAVENAME to %a.")
    endif()
else()
    if(NOT DEFAULT_SAVEDIR STREQUAL "%p")
        message(FATAL_ERROR "Flat save mode did not set SAVEDIR to %p.")
    endif()
    if(NOT DEFAULT_SAVENAME STREQUAL "%^C")
        message(FATAL_ERROR "Flat save mode did not set SAVENAME to %^C.")
    endif()
endif()
