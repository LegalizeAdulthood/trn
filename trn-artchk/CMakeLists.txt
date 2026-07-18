add_executable(trn-artchk trn-artchk.cpp)
target_include_directories(trn-artchk PRIVATE ../libtrn/include)
target_link_libraries(trn-artchk PUBLIC nntp tool)
target_folder(trn-artchk "Tools")

if(BUILD_TESTING)
    set(trn_artchk_test_dir "${CMAKE_CURRENT_BINARY_DIR}/test_data")
    set(trn_artchk_article "${trn_artchk_test_dir}/valid.article")
    set(trn_artchk_active "${trn_artchk_test_dir}/active")
    set(trn_artchk_newsgroups "${trn_artchk_test_dir}/newsgroups")

    file(MAKE_DIRECTORY "${trn_artchk_test_dir}")
    file(WRITE "${trn_artchk_article}"
"Newsgroups: comp.lang.apl,comp.lang.c++
Subject: Valid article

Short body line
")
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
endif()
