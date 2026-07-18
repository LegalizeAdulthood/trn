add_executable(inews inews.cpp)
target_include_directories(inews PRIVATE ../libtrn/include)
target_link_libraries(inews PUBLIC nntp tool PRIVATE fmt::fmt)
target_folder(inews "Tools")

if(BUILD_TESTING)
    set(inews_test_dir "${CMAKE_CURRENT_BINARY_DIR}/test_data")
    set(inews_invalid_header_article
        "${inews_test_dir}/invalid-header.article")
    set(inews_all_header_article
        "${inews_test_dir}/all-header.article")
    set(inews_failure_runner
        "${CMAKE_CURRENT_BINARY_DIR}/run_inews_failure.cmake")

    file(MAKE_DIRECTORY "${inews_test_dir}")
    file(WRITE "${inews_failure_runner}"
"execute_process(
    COMMAND \"${CMAKE_COMMAND}\" -E env
        \"NNTPSERVER=local\"
        \"HOME=\${INEWS_TEST_HOME}\"
        \"DOTDIR=\${INEWS_TEST_HOME}\"
        \"\${INEWS_EXE}\"
        \"\${INEWS_ARTICLE}\"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)
set(output \"\${stdout}\${stderr}\")
if(result EQUAL 0)
    message(FATAL_ERROR \"inews was expected to fail\")
endif()
if(NOT output MATCHES \"\${INEWS_EXPECTED}\")
    message(FATAL_ERROR
        \"inews output did not match: \${INEWS_EXPECTED}\n\${output}\")
endif()
")
    string(REPEAT "x" 9000 inews_long_subject)
    file(WRITE "${inews_invalid_header_article}"
"Bad header

Body
")
    file(WRITE "${inews_all_header_article}"
"Subject: ${inews_long_subject}
")

    add_test(NAME inews-invalid-header
        COMMAND ${CMAKE_COMMAND}
            "-DINEWS_EXE=$<TARGET_FILE:inews>"
            "-DINEWS_ARTICLE=${inews_invalid_header_article}"
            "-DINEWS_TEST_HOME=${inews_test_dir}"
            "-DINEWS_EXPECTED=Invalid header:"
            -P "${inews_failure_runner}"
    )
    set_tests_properties(inews-invalid-header PROPERTIES
        LABELS trn
    )

    add_test(NAME inews-all-header
        COMMAND ${CMAKE_COMMAND}
            "-DINEWS_EXE=$<TARGET_FILE:inews>"
            "-DINEWS_ARTICLE=${inews_all_header_article}"
            "-DINEWS_TEST_HOME=${inews_test_dir}"
            "-DINEWS_EXPECTED=Article was all header"
            -P "${inews_failure_runner}"
    )
    set_tests_properties(inews-all-header PROPERTIES
        LABELS trn
    )
endif()
