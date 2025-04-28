function(find_program_default name)
    find_program(PATH ${name})
    if(NOT ${PATH})
        set(PATH "${name}")
    endif()
    set("${name}_DEFAULT" "${PATH}" PARENT_SCOPE)
endfunction()

find_program_default(diff)
find_program_default(ed)
find_program_default(ispell)
find_program_default(spell)

set(SHELL_START "#!/bin/sh" CACHE STRING "Text to use for the first line bash shell scripts.")
set(DIFF_PROGRAM "${diff_DEFAULT}" CACHE STRING "Program to use for differencing files.")
set(ED_PROGRAM "${ed_DEFAULT}" CACHE STRING "Program to use for automated line editing.")
set(ISPELL_PROGRAM "${ispell_DEFAULT}" CACHE STRING "ispell program to use for spell checking.")
set(ISPELL_OPTIONS "" CACHE STRING "Options to pass to ISPELL_PROGRAM for spell checking.")
set(SPELL_PROGRAM "${spell_DEFAULT}" CACHE STRING "Program to use for spell checking if ispell not found.")

configure_file(access_def.in access_def @ONLY)
configure_file(environment.in environment @ONLY)
configure_file(filexp.in filexp @ONLY)
configure_file(newsnews.in newsnews @ONLY)
configure_file(Speller.in Speller @ONLY)

add_library(data INTERFACE
    access_def.in
    environment.in
    filexp.in
    newsnews.in
    Speller.in
    "${CMAKE_CURRENT_BINARY_DIR}/access_def"
    "${CMAKE_CURRENT_BINARY_DIR}/environment"
    "${CMAKE_CURRENT_BINARY_DIR}/filexp"
    "${CMAKE_CURRENT_BINARY_DIR}/newsnews"
    "${CMAKE_CURRENT_BINARY_DIR}/Speller"
)
source_group("CMake Templates" REGULAR_EXPRESSION ".*\\.in$")
source_group("Data Files" REGULAR_EXPRESSION "${CMAKE_CURRENT_BINARY_DIR}/.*")
target_folder(data "Libraries")
