# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

function(expect_exists path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected installed file '${path}'.")
    endif()
endfunction()

function(expect_not_exists path)
    if(EXISTS "${path}")
        message(FATAL_ERROR "Did not expect installed file '${path}'.")
    endif()
endfunction()

function(expect_executable path)
    expect_exists("${path}")
    if(NOT WIN32)
        execute_process(COMMAND test -x "${path}" RESULT_VARIABLE test_result)
        if(NOT test_result EQUAL 0)
            message(FATAL_ERROR "Expected installed program '${path}' to be executable.")
        endif()
    endif()
endfunction()

file(REMOVE_RECURSE "${TRN_TEST_INSTALL_PREFIX}")
set(install_command
    "${CMAKE_COMMAND}" --install "${TRN_TEST_BUILD_DIR}"
    --prefix "${TRN_TEST_INSTALL_PREFIX}")
if(NOT "${TRN_TEST_CONFIG}" STREQUAL "")
    list(APPEND install_command --config "${TRN_TEST_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_stdout
    ERROR_VARIABLE install_stderr)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "Install failed:\n${install_stdout}\n${install_stderr}")
endif()

set(bin_dir "${TRN_TEST_INSTALL_PREFIX}/${TRN_TEST_BINDIR}")
set(private_lib_dir "${TRN_TEST_INSTALL_PREFIX}/${TRN_TEST_PRIVATE_LIBDIR}")
set(man_dir "${TRN_TEST_INSTALL_PREFIX}/${TRN_TEST_MANDIR}/man1")

expect_executable("${bin_dir}/trn${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/inews${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/nntplist${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/trn-artchk${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/${TRN_TEST_PNEWS_HELPER_NAME}")
expect_executable("${bin_dir}/${TRN_TEST_RNMAIL_HELPER_NAME}")
expect_not_exists("${bin_dir}/filexp")
expect_not_exists("${bin_dir}/filexp.cmd")
expect_not_exists("${private_lib_dir}/filexp")
expect_not_exists("${private_lib_dir}/filexp.cmd")

expect_executable("${private_lib_dir}/${TRN_TEST_MBOX_SAVER_HELPER_NAME}")
expect_executable("${private_lib_dir}/${TRN_TEST_NORM_SAVER_HELPER_NAME}")
expect_executable("${private_lib_dir}/${TRN_TEST_SPELLER_HELPER_NAME}")
if(NOT "${TRN_TEST_PNEWS_HEADER_NAME}" STREQUAL "")
    expect_exists("${private_lib_dir}/${TRN_TEST_PNEWS_HEADER_NAME}")
else()
    expect_not_exists("${private_lib_dir}/Pnews.header")
endif()
expect_exists("${private_lib_dir}/INIT")
expect_exists("${private_lib_dir}/access.def")
expect_exists("${private_lib_dir}/environment")
expect_exists("${private_lib_dir}/newsnews")
expect_exists("${private_lib_dir}/HelpFiles/top")
expect_exists("${private_lib_dir}/HelpFiles/samples/access")

expect_exists("${man_dir}/trn.1")
expect_exists("${man_dir}/Pnews.1")
expect_exists("${man_dir}/Rnmail.1")
expect_not_exists("${man_dir}/strn.1")

include("${CMAKE_CURRENT_LIST_DIR}/test_norm_saver.cmake")
check_norm_saver("${private_lib_dir}/${TRN_TEST_NORM_SAVER_HELPER_NAME}"
    "${TRN_TEST_INSTALL_PREFIX}/norm-saver-check" full)

include("${CMAKE_CURRENT_LIST_DIR}/test_mbox_saver.cmake")
check_mbox_saver("${private_lib_dir}/${TRN_TEST_MBOX_SAVER_HELPER_NAME}"
    "${TRN_TEST_INSTALL_PREFIX}/mbox-saver-check" full "${TRN_TEST_MBOX_FORMAT}")

include("${CMAKE_CURRENT_LIST_DIR}/test_speller.cmake")
check_speller("${private_lib_dir}/${TRN_TEST_SPELLER_HELPER_NAME}"
    "${TRN_TEST_INSTALL_PREFIX}/speller-check" correct)

if(WIN32)
    expect_exists("${bin_dir}/Rnmail.ps1")
    expect_exists("${private_lib_dir}/trn-mail.ps1")
    include("${CMAKE_CURRENT_LIST_DIR}/test_rnmail.cmake")
    check_rnmail("${bin_dir}/${TRN_TEST_RNMAIL_HELPER_NAME}" "${TRN_TEST_PNEWS_TOOL}"
        "${TRN_TEST_INSTALL_PREFIX}/rnmail-check" forward)
    execute_process(COMMAND "$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe"
        -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_LIST_DIR}/../test_mail_smtp.ps1"
        -WorkDirectory "${TRN_TEST_INSTALL_PREFIX}/native-mail"
        -Adapter "${private_lib_dir}/trn-mail.ps1"
        -Helper "${bin_dir}/${TRN_TEST_RNMAIL_HELPER_NAME}" -OnlyCase accepted
        RESULT_VARIABLE smtp_result OUTPUT_VARIABLE smtp_out ERROR_VARIABLE smtp_err TIMEOUT 30)
    if(NOT smtp_result EQUAL 0)
        message(FATAL_ERROR "Installed SMTP workflow failed: ${smtp_out}${smtp_err}")
    endif()
    include("${CMAKE_CURRENT_LIST_DIR}/test_pnews.cmake")
    check_pnews("${bin_dir}/${TRN_TEST_PNEWS_HELPER_NAME}" "${TRN_TEST_PNEWS_TOOL}"
        "${TRN_TEST_INSTALL_PREFIX}/pnews-check" authorcopy)
endif()
