set(trn_artchk_test_dir "${CMAKE_CURRENT_BINARY_DIR}/test_data")
set(trn_artchk_article "${trn_artchk_test_dir}/valid.article")
set(trn_artchk_long_body_article
    "${trn_artchk_test_dir}/long-body.article")
set(trn_artchk_missing_newline_article
    "${trn_artchk_test_dir}/missing-newline.article")
set(trn_artchk_invalid_header_article
    "${trn_artchk_test_dir}/invalid-header.article")
set(trn_artchk_header_no_space_article
    "${trn_artchk_test_dir}/header-no-space.article")
set(trn_artchk_newsgroups_spaces_article
    "${trn_artchk_test_dir}/newsgroups-spaces.article")
set(trn_artchk_empty_newsgroups_article
    "${trn_artchk_test_dir}/empty-newsgroups.article")
set(trn_artchk_missing_newsgroups_article
    "${trn_artchk_test_dir}/missing-newsgroups.article")
set(trn_artchk_invalid_newsgroup_article
    "${trn_artchk_test_dir}/invalid-newsgroup.article")
set(trn_artchk_no_description_article
    "${trn_artchk_test_dir}/no-description.article")
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
file(WRITE "${trn_artchk_invalid_header_article}"
"Bad header

Body
")
file(WRITE "${trn_artchk_header_no_space_article}"
"Newsgroups: comp.lang.apl
Subject:No space

Body
")
file(WRITE "${trn_artchk_newsgroups_spaces_article}"
"Newsgroups: comp.lang.apl comp.lang.c++
Subject: Spaces

Body
")
file(WRITE "${trn_artchk_empty_newsgroups_article}"
    "Newsgroups: \nSubject: Empty\n\nBody\n")
file(WRITE "${trn_artchk_missing_newsgroups_article}"
"Subject: Missing newsgroups

Body
")
file(WRITE "${trn_artchk_invalid_newsgroup_article}"
"Newsgroups: comp.lang.missing
Subject: Invalid newsgroup

Body
")
file(WRITE "${trn_artchk_no_description_article}"
"Newsgroups: comp.lang.nodesc
Subject: No description

Body
")
file(WRITE "${trn_artchk_active}"
"comp.lang.apl 2 1 y
comp.lang.c++ 2 1 y
comp.lang.nodesc 2 1 y
")
file(WRITE "${trn_artchk_newsgroups}"
"comp.lang.apl APL discussion
comp.lang.c++ C++ discussion
")

add_test(NAME trn-artchk-invalid-max-line-length
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_COMMAND=$<TARGET_FILE:trn-artchk>"
        "-DTEST_ARGS=${trn_artchk_article};not-a-number;${trn_artchk_newsgroups};${trn_artchk_active}"
        "-DTEST_REGEX=Usage: trn-artchk"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectFailure.cmake"
)
set_tests_properties(trn-artchk-invalid-max-line-length PROPERTIES
    LABELS trn
)

add_test(NAME trn-artchk-zero-max-line-length
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_COMMAND=$<TARGET_FILE:trn-artchk>"
        "-DTEST_ARGS=${trn_artchk_article};0;${trn_artchk_newsgroups};${trn_artchk_active}"
        "-DTEST_REGEX=Usage: trn-artchk"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectFailure.cmake"
)
set_tests_properties(trn-artchk-zero-max-line-length PROPERTIES
    LABELS trn
)

add_test(NAME trn-artchk-trailing-max-line-length
    COMMAND ${CMAKE_COMMAND}
        "-DTEST_COMMAND=$<TARGET_FILE:trn-artchk>"
        "-DTEST_ARGS=${trn_artchk_article};79x;${trn_artchk_newsgroups};${trn_artchk_active}"
        "-DTEST_REGEX=Usage: trn-artchk"
        -P "${CMAKE_SOURCE_DIR}/tests/cmake/ExpectFailure.cmake"
)
set_tests_properties(trn-artchk-trailing-max-line-length PROPERTIES
    LABELS trn
)

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

add_test(NAME trn-artchk-invalid-header
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_invalid_header_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-invalid-header PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "ERROR: line 1 is an invalid header line"
)

add_test(NAME trn-artchk-header-no-space
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_header_no_space_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-header-no-space PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "ERROR: header on line 2 does not have a space after the colon"
)

add_test(NAME trn-artchk-newsgroups-spaces
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_newsgroups_spaces_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-newsgroups-spaces PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "ERROR: the \"Newsgroups:\" line has spaces in it"
)

add_test(NAME trn-artchk-empty-newsgroups
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_empty_newsgroups_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-empty-newsgroups PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "ERROR: the \"Newsgroups:\" line lists no newsgroups"
)

add_test(NAME trn-artchk-missing-newsgroups
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_missing_newsgroups_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-missing-newsgroups PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "ERROR: the \"Newsgroups:\" line is missing from the header"
)

add_test(NAME trn-artchk-invalid-newsgroup
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_invalid_newsgroup_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-invalid-newsgroup PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "comp.lang.missing[ ]+\\*\\* invalid news group -- check spelling \\*\\*"
)

add_test(NAME trn-artchk-no-description
    COMMAND ${CMAKE_COMMAND} -E env
        "NNTPSERVER=local"
        "HOME=${trn_artchk_test_dir}"
        "DOTDIR=${trn_artchk_test_dir}"
        $<TARGET_FILE:trn-artchk>
        "${trn_artchk_no_description_article}"
        79
        "${trn_artchk_newsgroups}"
        "${trn_artchk_active}"
)
set_tests_properties(trn-artchk-no-description PROPERTIES
    LABELS trn
    PASS_REGULAR_EXPRESSION
        "comp.lang.nodesc[ ]+\\[no description available\\]"
)
