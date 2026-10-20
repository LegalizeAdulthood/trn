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

set(trn_private_lib_install_dir "${PRIVATE_LIB}")
foreach(_iteration RANGE 1 2)
    string(REPLACE "%x" "${NEWS_LIB}" trn_private_lib_install_dir "${trn_private_lib_install_dir}")
    string(REPLACE "%X" "${PRIVATE_LIB}" trn_private_lib_install_dir "${trn_private_lib_install_dir}")
endforeach()
string(REPLACE "\\" "/" trn_private_lib_install_dir "${trn_private_lib_install_dir}")
string(REGEX REPLACE "^[A-Za-z]:/" "" trn_private_lib_install_dir "${trn_private_lib_install_dir}")
string(REGEX REPLACE "^/+" "" trn_private_lib_install_dir "${trn_private_lib_install_dir}")
set(TRN_INSTALL_PRIVATE_LIBDIR "${trn_private_lib_install_dir}" PARENT_SCOPE)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../../INIT.txt" INIT COPYONLY)
configure_file(access_def.in access.def @ONLY)
configure_file(environment.in environment @ONLY)
configure_file(newsnews.in newsnews @ONLY)
set(trn_norm_saver_support_output "")

if(WIN32)
    set(trn_mbox_saver_output "${CMAKE_CURRENT_BINARY_DIR}/mbox.saver.cmd")
    set(trn_norm_saver_output "${CMAKE_CURRENT_BINARY_DIR}/norm.saver.cmd")
    set(trn_norm_saver_support_output "${CMAKE_CURRENT_BINARY_DIR}/norm.saver.ps1")
    set(trn_pnews_output "${CMAKE_CURRENT_BINARY_DIR}/Pnews.cmd")
    set(trn_pnews_header_output "")
    set(trn_rnmail_output "${CMAKE_CURRENT_BINARY_DIR}/Rnmail.cmd")
    set(trn_speller_output "${CMAKE_CURRENT_BINARY_DIR}/Speller.cmd")
    set(trn_helper_sources
        mbox.saver.cmd.in
        norm.saver.cmd.in
        norm.saver.ps1.in
        Pnews.cmd.in
        Rnmail.cmd.in
        Speller.cmd.in)
    set(TRN_GENERATED_WINDOWS_HELPERS ON)
    set(TRN_INSTALL_MBOX_SAVER_HELPER_NAME "mbox.saver.cmd" PARENT_SCOPE)
    set(TRN_INSTALL_NORM_SAVER_HELPER_NAME "norm.saver.cmd" PARENT_SCOPE)
    set(TRN_INSTALL_PNEWS_HELPER_NAME "Pnews.cmd" PARENT_SCOPE)
    set(TRN_INSTALL_PNEWS_HEADER_NAME "" PARENT_SCOPE)
    set(TRN_INSTALL_RNMAIL_HELPER_NAME "Rnmail.cmd" PARENT_SCOPE)
    set(TRN_INSTALL_SPELLER_HELPER_NAME "Speller.cmd" PARENT_SCOPE)

    configure_file(mbox.saver.cmd.in "${trn_mbox_saver_output}" COPYONLY)
    configure_file(norm.saver.cmd.in "${trn_norm_saver_output}" COPYONLY)
    configure_file(norm.saver.ps1.in "${trn_norm_saver_support_output}" COPYONLY)
    configure_file(Pnews.cmd.in "${trn_pnews_output}" COPYONLY)
    configure_file(Rnmail.cmd.in "${trn_rnmail_output}" COPYONLY)
    configure_file(Speller.cmd.in "${trn_speller_output}" COPYONLY)
else()
    set(trn_mbox_saver_output "${CMAKE_CURRENT_BINARY_DIR}/mbox.saver")
    set(trn_norm_saver_output "${CMAKE_CURRENT_BINARY_DIR}/norm.saver")
    set(trn_pnews_output "${CMAKE_CURRENT_BINARY_DIR}/Pnews")
    set(trn_pnews_header_output "${CMAKE_CURRENT_BINARY_DIR}/Pnews.header")
    set(trn_rnmail_output "${CMAKE_CURRENT_BINARY_DIR}/Rnmail")
    set(trn_speller_output "${CMAKE_CURRENT_BINARY_DIR}/Speller")
    set(trn_helper_sources
        mbox.saver.in
        norm.saver.in
        Pnews.in
        Pnews.header.in
        Rnmail.in
        Speller.in)
    set(TRN_GENERATED_WINDOWS_HELPERS OFF)
    set(TRN_INSTALL_MBOX_SAVER_HELPER_NAME "mbox.saver" PARENT_SCOPE)
    set(TRN_INSTALL_NORM_SAVER_HELPER_NAME "norm.saver" PARENT_SCOPE)
    set(TRN_INSTALL_PNEWS_HELPER_NAME "Pnews" PARENT_SCOPE)
    set(TRN_INSTALL_PNEWS_HEADER_NAME "Pnews.header" PARENT_SCOPE)
    set(TRN_INSTALL_RNMAIL_HELPER_NAME "Rnmail" PARENT_SCOPE)
    set(TRN_INSTALL_SPELLER_HELPER_NAME "Speller" PARENT_SCOPE)

    configure_file(mbox.saver.in "${trn_mbox_saver_output}" @ONLY)
    configure_file(norm.saver.in "${trn_norm_saver_output}" @ONLY)
    configure_file(Pnews.in "${trn_pnews_output}" @ONLY)
    configure_file(Pnews.header.in "${trn_pnews_header_output}" @ONLY)
    configure_file(Rnmail.in "${trn_rnmail_output}" @ONLY)
    configure_file(Speller.in "${trn_speller_output}" @ONLY)
endif()

set(TRN_GENERATED_MBOX_SAVER_FILE "${trn_mbox_saver_output}" PARENT_SCOPE)
set(TRN_GENERATED_NORM_SAVER_FILE "${trn_norm_saver_output}" PARENT_SCOPE)
set(TRN_GENERATED_PNEWS_FILE "${trn_pnews_output}" PARENT_SCOPE)
set(TRN_GENERATED_PNEWS_HEADER_FILE "${trn_pnews_header_output}" PARENT_SCOPE)
set(TRN_GENERATED_RNMAIL_FILE "${trn_rnmail_output}" PARENT_SCOPE)
set(TRN_GENERATED_SPELLER_FILE "${trn_speller_output}" PARENT_SCOPE)
set(TRN_GENERATED_WINDOWS_HELPERS "${TRN_GENERATED_WINDOWS_HELPERS}" PARENT_SCOPE)

set(trn_public_helper_outputs
    "${trn_pnews_output}"
    "${trn_rnmail_output}")
set(trn_private_helper_outputs
    "${trn_mbox_saver_output}"
    "${trn_norm_saver_output}"
    "${trn_speller_output}")

add_library(data INTERFACE
    configure_data.cmake
    "${CMAKE_CURRENT_SOURCE_DIR}/../../INIT.txt"
    access_def.in
    environment.in
    newsnews.in
    ${trn_helper_sources}
    "${CMAKE_CURRENT_BINARY_DIR}/INIT"
    "${CMAKE_CURRENT_BINARY_DIR}/access.def"
    "${CMAKE_CURRENT_BINARY_DIR}/environment"
    "${CMAKE_CURRENT_BINARY_DIR}/newsnews"
    ${trn_private_helper_outputs}
    ${trn_norm_saver_support_output}
    ${trn_public_helper_outputs}
    ${trn_pnews_header_output}
)
source_group("CMake Scripts" REGULAR_EXPRESSION ".*\\.cmake$")
source_group("CMake Templates" REGULAR_EXPRESSION ".*\\.in$")
source_group("Data Files" REGULAR_EXPRESSION "${CMAKE_CURRENT_BINARY_DIR}/.*")
target_folder(data "Libraries")

install(PROGRAMS ${trn_public_helper_outputs} DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(PROGRAMS ${trn_private_helper_outputs} DESTINATION "${trn_private_lib_install_dir}")
set(trn_private_data_outputs
    "${CMAKE_CURRENT_BINARY_DIR}/INIT"
    "${CMAKE_CURRENT_BINARY_DIR}/access.def"
    "${CMAKE_CURRENT_BINARY_DIR}/environment"
    "${CMAKE_CURRENT_BINARY_DIR}/newsnews")
if(trn_norm_saver_support_output)
    list(APPEND trn_private_data_outputs "${trn_norm_saver_support_output}")
endif()
if(trn_pnews_header_output)
    list(APPEND trn_private_data_outputs "${trn_pnews_header_output}")
endif()
install(FILES
    ${trn_private_data_outputs}
    DESTINATION "${trn_private_lib_install_dir}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/HelpFiles"
    DESTINATION "${trn_private_lib_install_dir}")
