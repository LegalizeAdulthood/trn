function(_mkdir dir)
    file(MAKE_DIRECTORY ${dir})
endfunction()

function(configure_trn_test_data)
    set(TRN_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_data")
    set(TRN_TEST_HOME_DIR "${TRN_TEST_DATA_DIR}/home")
    _mkdir(${TRN_TEST_HOME_DIR})
    set(TRN_TEST_TMP_DIR  "${TRN_TEST_DATA_DIR}/tmp")
    _mkdir(${TRN_TEST_TMP_DIR})
    set(TRN_TEST_LOGIN_NAME "bozo")
    set(TRN_TEST_REAL_NAME "Bozo the Clown")
    set(TRN_TEST_DOT_DIR  "${TRN_TEST_DATA_DIR}/dot")
    _mkdir(${TRN_TEST_DOT_DIR})
    set(TRN_TEST_TRN_DIR  "${TRN_TEST_DATA_DIR}/trn")
    _mkdir(${TRN_TEST_TRN_DIR})
    set(TRN_TEST_LIB_DIR  "${TRN_TEST_DATA_DIR}/lib")
    _mkdir(${TRN_TEST_LIB_DIR})
    set(TRN_TEST_RN_LIB_DIR "${TRN_TEST_DATA_DIR}/rn_lib")
    _mkdir(${TRN_TEST_RN_LIB_DIR})
    set(TRN_TEST_LOCAL_HOST "huey.example.org")
    set(TRN_TEST_P_HOST_NAME "duey.example.org")
    configure_file(cmake/test_config.h.in test_config.h)
    configure_file(cmake/test_data.cpp.in test_data.cpp)

    set(TRN_TEST_SPOOL_DIR "${TRN_TEST_DATA_DIR}/spool")
    set(TRN_TEST_NEWSGROUP "alt.binaries.fractals")
    string(REPLACE "." "/" TRN_TEST_NEWSGROUP_SUBDIR ${TRN_TEST_NEWSGROUP})
    set(TRN_TEST_NEWSGROUP_DIR "${TRN_TEST_SPOOL_DIR}/${TRN_TEST_NEWSGROUP_SUBDIR}")
    set(TRN_TEST_ARTICLE_NUM 623)
    set(TRN_TEST_HEADER_FROM "Bozo the Clown <bozo@clown-world.org>")
    set(TRN_TEST_HEADER_SUBJECT "Re: the best red nose")
    set(TRN_TEST_HEADER_DATE "Tue, 21 Jan 2003 01:21:09 +0000 (UTC)")
    set(TRN_TEST_HEADER_MESSAGE_ID "<b0i7a5$aoq$1@terabinaries.xmission.com>")
    set(TRN_TEST_HEADER_REFERENCES "nnrp.xmission xmission.general:21646")
    set(TRN_TEST_HEADER_BYTES "2132")
    set(TRN_TEST_HEADER_LINES "12")
    set(TRN_TEST_BODY "Pneumatic tubes are killing the Internet.\n")
    set(TRN_TEST_SIGNATURE "XMission Internet Access - http://www.xmission.com - Voice: 801 539 0852\n")
    _mkdir(${TRN_TEST_NEWSGROUP_DIR})
    configure_file(cmake/test_article.in "${TRN_TEST_NEWSGROUP_DIR}/${TRN_TEST_ARTICLE_NUM}")

    configure_file(cmake/test_nntp_auth_file.in "${TRN_TEST_DOT_DIR}/.nntpauth")
    configure_file(cmake/test_access_file.in "${TRN_TEST_TRN_DIR}/access")
    configure_file(cmake/test_default_access_file.in "${TRN_TEST_RN_LIB_DIR}/access.def")
endfunction()
