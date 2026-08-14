# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

function(find_program_default name variable)
    find_program(program_path "${name}" NO_CACHE)
    if(program_path)
        set("${variable}" "${program_path}" PARENT_SCOPE)
    else()
        set("${variable}" "${name}" PARENT_SCOPE)
    endif()
endfunction()

function(configure_ispell_defaults)
    find_program(ispell_path "ispell" NO_CACHE)
    if(ispell_path)
        set(DEFAULT_ISPELL_PROGRAM "${ispell_path}" PARENT_SCOPE)
        set(DEFAULT_ISPELL_OPTIONS "-x" PARENT_SCOPE)
        return()
    endif()

    find_program(vspell_path "vspell" NO_CACHE)
    if(vspell_path)
        set(DEFAULT_ISPELL_PROGRAM "${vspell_path}" PARENT_SCOPE)
    else()
        set(DEFAULT_ISPELL_PROGRAM "none" PARENT_SCOPE)
    endif()
    set(DEFAULT_ISPELL_OPTIONS "" PARENT_SCOPE)
endfunction()

function(validate_helper_tool_value name value)
    if("${value}" STREQUAL "")
        message(FATAL_ERROR "Option ${name} must not be empty.")
    endif()
endfunction()
