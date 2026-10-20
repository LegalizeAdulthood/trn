# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

function(expect_exists path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "Expected packaged file '${path}'.")
    endif()
endfunction()

function(expect_not_exists path)
    if(EXISTS "${path}")
        message(FATAL_ERROR "Did not expect packaged file '${path}'.")
    endif()
endfunction()

function(expect_executable path)
    expect_exists("${path}")
    if(NOT WIN32)
        execute_process(COMMAND test -x "${path}" RESULT_VARIABLE test_result)
        if(NOT test_result EQUAL 0)
            message(FATAL_ERROR "Expected packaged program '${path}' to be executable.")
        endif()
    endif()
endfunction()

if("${TRN_TEST_CPACK_COMMAND}" STREQUAL "")
    message(FATAL_ERROR "TRN_TEST_CPACK_COMMAND is not set.")
endif()

file(REMOVE_RECURSE "${TRN_TEST_PACKAGE_DIR}")
file(MAKE_DIRECTORY "${TRN_TEST_PACKAGE_DIR}")

set(package_command
    "${TRN_TEST_CPACK_COMMAND}"
    -G ZIP
    --config "${TRN_TEST_BUILD_DIR}/CPackConfig.cmake"
    -B "${TRN_TEST_PACKAGE_DIR}")
if(NOT "${TRN_TEST_CONFIG}" STREQUAL "")
    list(APPEND package_command -C "${TRN_TEST_CONFIG}")
endif()

execute_process(
    COMMAND ${package_command}
    RESULT_VARIABLE package_result
    OUTPUT_VARIABLE package_stdout
    ERROR_VARIABLE package_stderr)
if(NOT package_result EQUAL 0)
    message(FATAL_ERROR
        "Package failed:\n${package_stdout}\n${package_stderr}")
endif()

file(GLOB package_files "${TRN_TEST_PACKAGE_DIR}/*.zip")
list(LENGTH package_files package_count)
if(NOT package_count EQUAL 1)
    message(FATAL_ERROR
        "Expected one ZIP package in '${TRN_TEST_PACKAGE_DIR}'.")
endif()
list(GET package_files 0 package_file)

set(extract_dir "${TRN_TEST_PACKAGE_DIR}/extract")
file(REMOVE_RECURSE "${extract_dir}")
file(MAKE_DIRECTORY "${extract_dir}")
file(ARCHIVE_EXTRACT INPUT "${package_file}" DESTINATION "${extract_dir}")

file(GLOB package_roots RELATIVE "${extract_dir}" "${extract_dir}/*")
list(LENGTH package_roots package_root_count)
if(NOT package_root_count EQUAL 1)
    message(FATAL_ERROR
        "Expected one package root in '${extract_dir}'.")
endif()
list(GET package_roots 0 package_root_name)
set(package_root "${extract_dir}/${package_root_name}")

set(bin_dir "${package_root}/${TRN_TEST_BINDIR}")
set(private_lib_dir "${package_root}/${TRN_TEST_PRIVATE_LIBDIR}")
set(man_dir "${package_root}/${TRN_TEST_MANDIR}/man1")

expect_exists("${package_root}/ReadMe.md")
expect_exists("${package_root}/INSTALL.md")
expect_exists("${package_root}/LICENSE.txt")
expect_exists("${package_root}/HACKERSGUIDE.txt")
expect_not_exists("${package_root}/INIT.txt")
expect_not_exists("${package_root}/default_topic")

expect_executable("${bin_dir}/trn${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/inews${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/nntplist${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/trn-artchk${TRN_TEST_EXECUTABLE_SUFFIX}")
expect_executable("${bin_dir}/${TRN_TEST_PNEWS_HELPER_NAME}")
expect_executable("${bin_dir}/${TRN_TEST_RNMAIL_HELPER_NAME}")

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
    "${TRN_TEST_PACKAGE_DIR}/norm-saver-check" full)

include("${CMAKE_CURRENT_LIST_DIR}/test_mbox_saver.cmake")
check_mbox_saver("${private_lib_dir}/${TRN_TEST_MBOX_SAVER_HELPER_NAME}"
    "${TRN_TEST_PACKAGE_DIR}/mbox-saver-check" full "${TRN_TEST_MBOX_FORMAT}")

include("${CMAKE_CURRENT_LIST_DIR}/test_speller.cmake")
check_speller("${private_lib_dir}/${TRN_TEST_SPELLER_HELPER_NAME}"
    "${TRN_TEST_PACKAGE_DIR}/speller-check" correct)

if(WIN32)
    include("${CMAKE_CURRENT_LIST_DIR}/test_pnews.cmake")
    check_pnews("${bin_dir}/${TRN_TEST_PNEWS_HELPER_NAME}" "${TRN_TEST_PNEWS_TOOL}"
        "${TRN_TEST_PACKAGE_DIR}/pnews-check" authorcopy)
endif()
