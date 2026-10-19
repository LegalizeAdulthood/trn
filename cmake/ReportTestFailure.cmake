# Copyright (c) 2026, Richard Thomson

if(NOT EXISTS "${TRN_TEST_LOG}" OR NOT EXISTS "${TRN_TEST_EXECUTABLE}")
    message(STATUS "No failed test log or test executable available for a backtrace")
    return()
endif()
file(STRINGS "${TRN_TEST_LOG}" failed_test LIMIT_COUNT 1)
if(NOT failed_test MATCHES "^[0-9]+:(.+)$")
    message(STATUS "No failed test name available for a backtrace")
    return()
endif()
set(failed_test "${CMAKE_MATCH_1}")
find_program(TRN_DEBUGGER NAMES egdb gdb)
if(NOT TRN_DEBUGGER)
    message(STATUS "No debugger available for ${failed_test}")
    return()
endif()
get_filename_component(test_executable "${TRN_TEST_EXECUTABLE}" ABSOLUTE)
get_filename_component(test_directory "${test_executable}" DIRECTORY)
message(STATUS "Capturing backtrace for ${failed_test}")
execute_process(
    COMMAND "${TRN_DEBUGGER}" --batch --nx
        -ex "set pagination off"
        -ex run
        -ex "thread apply all bt"
        -ex "info sharedlibrary"
        --args "${test_executable}" "--gtest_filter=${failed_test}"
    WORKING_DIRECTORY "${test_directory}"
    TIMEOUT 60
    RESULT_VARIABLE debugger_status)
message(STATUS "Debugger status: ${debugger_status}")
