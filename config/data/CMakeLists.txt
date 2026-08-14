include("${CMAKE_CURRENT_LIST_DIR}/configure_data.cmake")

find_program_default(diff DEFAULT_DIFF_PROGRAM)
find_program_default(ed DEFAULT_ED_PROGRAM)
configure_ispell_defaults()
find_program_default(spell DEFAULT_SPELL_PROGRAM)
find_program(sh_path "sh" NO_CACHE)
configure_shell_start_default("${sh_path}")

set(DEFAULT_SHELL_START "${DEFAULT_SHELL_START}" PARENT_SCOPE)
set(DEFAULT_DIFF_PROGRAM "${DEFAULT_DIFF_PROGRAM}" PARENT_SCOPE)
set(DEFAULT_ED_PROGRAM "${DEFAULT_ED_PROGRAM}" PARENT_SCOPE)
set(DEFAULT_ISPELL_PROGRAM "${DEFAULT_ISPELL_PROGRAM}" PARENT_SCOPE)
set(DEFAULT_ISPELL_OPTIONS "${DEFAULT_ISPELL_OPTIONS}" PARENT_SCOPE)
set(DEFAULT_SPELL_PROGRAM "${DEFAULT_SPELL_PROGRAM}" PARENT_SCOPE)
configure_helper_tool_cache_defaults()
# MMDF mailbox magic separator
string(ASCII 1 CTRL_A)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../../INIT.txt" INIT COPYONLY)
configure_file(access_def.in access.def @ONLY)
configure_file(environment.in environment @ONLY)
configure_file(filexp.in filexp @ONLY)
configure_file(mbox.saver.in mbox.saver @ONLY)
configure_file(newsnews.in newsnews @ONLY)
configure_file(norm.saver.in norm.saver @ONLY)
configure_file(Pnews.in Pnews @ONLY)
configure_file(Pnews.header.in Pnews.header @ONLY)
configure_file(Rnmail.in Rnmail @ONLY)
configure_file(Speller.in Speller @ONLY)

add_library(data INTERFACE
    configure_data.cmake
    "${CMAKE_CURRENT_SOURCE_DIR}/../../INIT.txt"
    access_def.in
    environment.in
    filexp.in
    mbox.saver.in
    newsnews.in
    norm.saver.in
    Pnews.in
    Pnews.header.in
    Rnmail.in
    Speller.in
    "${CMAKE_CURRENT_BINARY_DIR}/INIT"
    "${CMAKE_CURRENT_BINARY_DIR}/access.def"
    "${CMAKE_CURRENT_BINARY_DIR}/environment"
    "${CMAKE_CURRENT_BINARY_DIR}/filexp"
    "${CMAKE_CURRENT_BINARY_DIR}/mbox.saver"
    "${CMAKE_CURRENT_BINARY_DIR}/newsnews"
    "${CMAKE_CURRENT_BINARY_DIR}/norm.saver"
    "${CMAKE_CURRENT_BINARY_DIR}/Pnews"
    "${CMAKE_CURRENT_BINARY_DIR}/Pnews.header"
    "${CMAKE_CURRENT_BINARY_DIR}/Rnmail"
    "${CMAKE_CURRENT_BINARY_DIR}/Speller"
)
source_group("CMake Scripts" REGULAR_EXPRESSION ".*\\.cmake$")
source_group("CMake Templates" REGULAR_EXPRESSION ".*\\.in$")
source_group("Data Files" REGULAR_EXPRESSION "${CMAKE_CURRENT_BINARY_DIR}/.*")
target_folder(data "Libraries")
