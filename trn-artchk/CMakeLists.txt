add_executable(trn-artchk trn-artchk.cpp)
target_include_directories(trn-artchk PRIVATE ../libtrn/include)
target_link_libraries(trn-artchk PUBLIC nntp tool PRIVATE fmt::fmt)
target_folder(trn-artchk "Tools")

if(BUILD_TESTING)
    set(trn_artchk_test_dir "${CMAKE_CURRENT_BINARY_DIR}/test_data")
    set(trn_artchk_article "${trn_artchk_test_dir}/valid.article")
    set(trn_artchk_long_body_article
        "${trn_artchk_test_dir}/long-body.article")
    set(trn_artchk_missing_newline_article
        "${trn_artchk_test_dir}/missing-newline.article")
    set(trn_artchk_active "${trn_artchk_test_dir}/active")
    set(trn_artchk_newsgroups "${trn_artchk_test_dir}/newsgroups")

    file(MAKE_DIRECTORY "${trn_artchk_test_dir}")
    string(REPEAT "x" 90 trn_artchk_long_body_line)
    string(CONCAT trn_artchk_missing_newline_contents
        "Newsgroups: comp.lang.apl\n"
        "Subject: Missing newline\n"
        "\n"
        "unterminated body")
    file(WRITE "${trn_artchk_article}"
"Newsgroups: comp.lang.apl,comp.lang.c++
Subject: Valid article

Short body line
")
    file(WRITE "${trn_artchk_long_body_article}"
"Newsgroups: comp.lang.apl
Subject: Long body

${trn_artchk_long_body_line}
")
    file(WRITE "${trn_artchk_missing_newline_article}"
        "${trn_artchk_missing_newline_contents}")
    file(WRITE "${trn_artchk_active}"
"comp.lang.apl 2 1 y
comp.lang.c++ 2 1 y
")
    file(WRITE "${trn_artchk_newsgroups}"
"comp.lang.apl APL discussion
comp.lang.c++ C++ discussion
")

    add_test(NAME trn-artchk-valid-newsgroups
        COMMAND ${CMAKE_COMMAND} -E env
            "NNTPSERVER=local"
            "HOME=${trn_artchk_test_dir}"
            "DOTDIR=${trn_artchk_test_dir}"
            $<TARGET_FILE:trn-artchk>
            "${trn_artchk_article}"
            79
            "${trn_artchk_newsgroups}"
            "${trn_artchk_active}"
    )
    set_tests_properties(trn-artchk-valid-newsgroups PROPERTIES
        LABELS trn
        PASS_REGULAR_EXPRESSION
            "Your article's newsgroups:[\r\n]+comp.lang.apl[ ]+APL discussion[\r\n]+comp.lang.c\\+\\+[ ]+C\\+\\+ discussion"
    )

    add_test(NAME trn-artchk-long-body-warning
        COMMAND ${CMAKE_COMMAND} -E env
            "NNTPSERVER=local"
            "HOME=${trn_artchk_test_dir}"
            "DOTDIR=${trn_artchk_test_dir}"
            $<TARGET_FILE:trn-artchk>
            "${trn_artchk_long_body_article}"
            79
            "${trn_artchk_newsgroups}"
            "${trn_artchk_active}"
    )
    set_tests_properties(trn-artchk-long-body-warning PROPERTIES
        LABELS trn
        PASS_REGULAR_EXPRESSION
            "Warning: posting exceeds 79 columns.  Line 4 is the first long one:"
    )

    add_test(NAME trn-artchk-missing-final-newline-warning
        COMMAND ${CMAKE_COMMAND} -E env
            "NNTPSERVER=local"
            "HOME=${trn_artchk_test_dir}"
            "DOTDIR=${trn_artchk_test_dir}"
            $<TARGET_FILE:trn-artchk>
            "${trn_artchk_missing_newline_article}"
            79
            "${trn_artchk_newsgroups}"
            "${trn_artchk_active}"
    )
    set_tests_properties(trn-artchk-missing-final-newline-warning PROPERTIES
        LABELS trn
        PASS_REGULAR_EXPRESSION
            "Warning: line 4 has no trailing newline character and may get lost"
    )
endif()
