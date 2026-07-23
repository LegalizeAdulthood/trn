add_executable(nntplist nntplist.cpp)
target_include_directories(nntplist PRIVATE ../libtrn/include)
target_link_libraries(nntplist PUBLIC nntp tool PRIVATE fmt::fmt)
target_folder(nntplist "Tools")

if(BUILD_TESTING)
    add_test(NAME nntplist-usage-bad-option
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_COMMAND=$<TARGET_FILE:nntplist>"
            "-DTEST_ARGS=-z"
            "-DTEST_REGEX=Usage: nntplist"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectFailure.cmake"
    )
    set_tests_properties(nntplist-usage-bad-option PROPERTIES LABELS trn)

    add_test(NAME nntplist-unknown-local-list
        COMMAND ${CMAKE_COMMAND}
            "-DTEST_COMMAND=$<TARGET_FILE:nntplist>"
            "-DTEST_ARGS=not-a-list"
            "-DTEST_ENV_NAME=NNTPSERVER"
            "-DTEST_ENV_VALUE=local"
            "-DTEST_REGEX=Don't know how to list.*not-a-list.*local system"
            -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectFailure.cmake"
    )
    set_tests_properties(nntplist-unknown-local-list PROPERTIES LABELS trn)
endif()
