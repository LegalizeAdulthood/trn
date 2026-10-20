# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

function(configure_data_tool_path_default tool_path variable missing_value)
    if(tool_path)
        set("${variable}" "${tool_path}" PARENT_SCOPE)
    else()
        set("${variable}" "${missing_value}" PARENT_SCOPE)
    endif()
endfunction()

function(find_program_default name variable)
    if(ARGC GREATER 2)
        set(missing_value "${ARGV2}")
    else()
        set(missing_value "")
    endif()
    find_program(program_path "${name}" NO_CACHE)
    configure_data_tool_path_default("${program_path}" "${variable}" "${missing_value}")
    set("${variable}" "${${variable}}" PARENT_SCOPE)
endfunction()

function(configure_shell_start_default shell_path)
    if(shell_path)
        set(DEFAULT_SHELL_START "#!${shell_path}" PARENT_SCOPE)
    else()
        set(DEFAULT_SHELL_START "" PARENT_SCOPE)
    endif()
endfunction()

function(configure_ispell_defaults_from_paths ispell_path vspell_path)
    if(ispell_path)
        set(DEFAULT_ISPELL_PROGRAM "${ispell_path}" PARENT_SCOPE)
        set(DEFAULT_ISPELL_OPTIONS "-x" PARENT_SCOPE)
        return()
    endif()

    if(vspell_path)
        set(DEFAULT_ISPELL_PROGRAM "${vspell_path}" PARENT_SCOPE)
    else()
        set(DEFAULT_ISPELL_PROGRAM "none" PARENT_SCOPE)
    endif()
    set(DEFAULT_ISPELL_OPTIONS "" PARENT_SCOPE)
endfunction()

function(configure_ispell_defaults)
    find_program(ispell_path "ispell" NO_CACHE)
    find_program(vspell_path "vspell" NO_CACHE)
    configure_ispell_defaults_from_paths("${ispell_path}" "${vspell_path}")
endfunction()

function(configure_helper_tool_cache_defaults)
    set(SHELL_START "${DEFAULT_SHELL_START}" CACHE STRING
        "Text to use for the first line bash shell scripts.")
    set(DIFF_PROGRAM "${DEFAULT_DIFF_PROGRAM}" CACHE STRING
        "Program to use for differencing files; empty disables the default.")
    set(ED_PROGRAM "${DEFAULT_ED_PROGRAM}" CACHE STRING
        "Program to use for automated line editing; empty disables the default.")
    set(ISPELL_PROGRAM "${DEFAULT_ISPELL_PROGRAM}" CACHE STRING
        "ispell program to use for spell checking, or none.")
    set(ISPELL_OPTIONS "${DEFAULT_ISPELL_OPTIONS}" CACHE STRING
        "Options to pass to ISPELL_PROGRAM for spell checking.")
    set(SPELL_PROGRAM "${DEFAULT_SPELL_PROGRAM}" CACHE STRING
        "Program to use for spell checking if ispell not found; empty disables the default.")
endfunction()
