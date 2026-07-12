# This software is copyrighted as detailed in the LICENSE file.
# Copyright (c) 2026, Richard Thomson

set(command "${TEST_COMMAND}" ${TEST_ARGS})
if(DEFINED TEST_ENV_NAME)
    set(command "${CMAKE_COMMAND}" -E env "${TEST_ENV_NAME}=${TEST_ENV_VALUE}" ${command})
endif()

execute_process(
    COMMAND ${command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)

string(CONCAT combined_output "${output}" "${error}")

if(result EQUAL 0)
    message(FATAL_ERROR "Expected command to fail.")
endif()

if(NOT combined_output MATCHES "${TEST_REGEX}")
    message(FATAL_ERROR
        "Command output did not match expected expression.\n"
        "Expression: ${TEST_REGEX}\n"
        "Output:\n${combined_output}"
    )
endif()
