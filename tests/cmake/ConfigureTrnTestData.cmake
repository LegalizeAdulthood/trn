function(make_dir dir)
    file(MAKE_DIRECTORY "${dir}")
endfunction()

function(configure_trn_test_data)
    # Directory and environment related content.
    set(TRN_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/test_data")
    set(TRN_TEST_HOME_DIR "${TRN_TEST_DATA_DIR}/home")
    set(TRN_TEST_HOME_DIR_CAPITALIZED "${TRN_TEST_DATA_DIR}/Home")
    make_dir("${TRN_TEST_HOME_DIR}")
    make_dir("${TRN_TEST_HOME_DIR}/News")
    set(TRN_TEST_TMP_DIR  "${TRN_TEST_DATA_DIR}/tmp")
    make_dir("${TRN_TEST_TMP_DIR}")
    set(TRN_TEST_LOGIN_NAME "bozo")
    set(TRN_TEST_REAL_NAME "Bozo the Clown")
    set(TRN_TEST_DOT_DIR  "${TRN_TEST_DATA_DIR}/dot")
    make_dir("${TRN_TEST_DOT_DIR}")
    set(TRN_TEST_TRN_DIR  "${TRN_TEST_DATA_DIR}/trn")
    make_dir("${TRN_TEST_TRN_DIR}")
    set(TRN_TEST_LIB_DIR  "${TRN_TEST_DATA_DIR}/lib")
    make_dir("${TRN_TEST_LIB_DIR}")
    set(TRN_TEST_RN_LIB_DIR "${TRN_TEST_DATA_DIR}/rn_lib")
    make_dir("${TRN_TEST_RN_LIB_DIR}")
    set(TRN_TEST_LOCAL_HOST "huey.example.org")
    set(TRN_TEST_P_HOST_NAME "duey.example.org")
    set(TRN_TEST_NNTPSERVER "news.example.org")
    set(TRN_TEST_ORGANIZATION "Multi-cellular, biological")
    set(TRN_TEST_ORGFILE "${TRN_TEST_DATA_DIR}/organization")
    set(TRN_TEST_LOCAL_SPOOL_DIR "${TRN_TEST_DATA_DIR}/spool")
    make_dir("${TRN_TEST_LOCAL_SPOOL_DIR}")

    # Newsgroup related content.
    set(TRN_TEST_SPOOL_DIR "${TRN_TEST_DATA_DIR}/spool")
    set(TRN_TEST_NEWSGROUP "alt.binaries.fractals")
    string(REPLACE "." "/" TRN_TEST_NEWSGROUP_SUBDIR "${TRN_TEST_NEWSGROUP}")
    set(TRN_TEST_NEWSGROUP_DIR "${TRN_TEST_SPOOL_DIR}/${TRN_TEST_NEWSGROUP_SUBDIR}")
    make_dir("${TRN_TEST_NEWSGROUP_DIR}")

    # Article contents.
    set(TRN_TEST_ARTICLE_NUM 623)
    set(TRN_TEST_ARTICLE_FILE "${TRN_TEST_NEWSGROUP_DIR}/${TRN_TEST_ARTICLE_NUM}")
    set(TRN_TEST_HEADER_PATH "foo!bar!goink!not-for-mail")
    set(TRN_TEST_HEADER_FROM_ADDRESS "bozo@clown-world.org")
    set(TRN_TEST_HEADER_FROM_NAME "Bozo the Clown")
    set(TRN_TEST_HEADER_FROM "${TRN_TEST_HEADER_FROM_NAME} <${TRN_TEST_HEADER_FROM_ADDRESS}>")
    set(TRN_TEST_HEADER_NEWSGROUPS "${TRN_TEST_NEWSGROUP},alt.flame")
    set(TRN_TEST_HEADER_FOLLOWUP_TO "alt.flame")
    set(TRN_TEST_HEADER_STRIPPED_SUBJECT "the best red nose")
    set(TRN_TEST_HEADER_ONE_RE_SUBJECT "Re: ${TRN_TEST_HEADER_STRIPPED_SUBJECT}")
    set(TRN_TEST_HEADER_SUBJECT "Re: ${TRN_TEST_HEADER_ONE_RE_SUBJECT}")
    set(TRN_TEST_HEADER_REPLY_TO_ADDRESS "clongworth@dark-shadows.example.org")
    set(TRN_TEST_HEADER_REPLY_TO "Cyrus Longworth <${TRN_TEST_HEADER_REPLY_TO_ADDRESS}>")
    set(TRN_TEST_HEADER_DATE "Tue, 21 Jan 2003 01:21:09 +0000 (UTC)")
    set(TRN_TEST_HEADER_MESSAGE_ID "<b0i7a5$aoq$1@terabinaries.xmission.com>")
    set(TRN_TEST_REFERENCE_IDS
            "<u0fn0g$34scf$1@dont-email.me>"
            "<874jpv84uv.fsf@bsb.me.uk>"
            "<u0jt3l$cmu$1@reader2.panix.com>")
    string(JOIN " " TRN_TEST_HEADER_REFERENCES ${TRN_TEST_REFERENCE_IDS})
    list(GET TRN_TEST_REFERENCE_IDS -1 TRN_TEST_HEADER_LAST_REFERENCE)
    set(TRN_TEST_HEADER_DISTRIBUTION "na")
    set(TRN_TEST_HEADER_X_BOOGIE_NIGHTS "oh what a night")
    set(TRN_TEST_HEADER_BYTES "2132")
    set(TRN_TEST_HEADER_LINES "16")
    set(TRN_TEST_BODY "Pneumatic tubes are killing the Internet.")
    set(TRN_TEST_SIGNATURE "XMission Internet Access - http://www.xmission.com - Voice: 801 539 0852")

    # Article with no fallbacks for other fields, e.g. no Reply-To: for From:.
    set(TRN_TEST_ARTICLE_NO_FALLBACKS_NUM 624)

    # Active file contents.
    set(TRN_TEST_NEWSGROUP_HIGH "${TRN_TEST_ARTICLE_NO_FALLBACKS_NUM}")
    set(TRN_TEST_NEWSGROUP_LOW "${TRN_TEST_ARTICLE_NUM}")
    set(TRN_TEST_NEWSGROUP_PERM "y")

    # Access file contents
    set(TRN_TEST_ACCESS_LOCAL_ACTIVE_FILE   "${TRN_TEST_LOCAL_SPOOL_DIR}/active")
    set(TRN_TEST_ACCESS_LOCAL_ADD_GROUPS    "yes")
    set(TRN_TEST_ACCESS_LOCAL_NEWSRC        "${TRN_TEST_DOT_DIR}/local-newsrc")
    set(TRN_TEST_ACCESS_LOCAL_SPOOL_DIR     "${TRN_TEST_LOCAL_SPOOL_DIR}")
    set(TRN_TEST_ACCESS_NNTP_ACTIVE_FILE    "${TRN_TEST_HOME_DIR}/nntp-active")
    set(TRN_TEST_ACCESS_NNTP_ADD_GROUPS     "yes")
    set(TRN_TEST_ACCESS_NNTP_NEWSRC         "${TRN_TEST_DOT_DIR}/nntp-newsrc")
    set(TRN_TEST_ACCESS_NNTP_SERVER         "nntp.example.org")

    # Local newsrc contents
    math(EXPR _prev_article                     "${TRN_TEST_ARTICLE_NUM}-1")
    set(TRN_TEST_LOCAL_NEWSGROUP                "${TRN_TEST_NEWSGROUP}")
    set(TRN_TEST_LOCAL_NEWSGROUP_READ_ARTICLES  "1-${_prev_article}")

    # NNTP newsrc contents
    set(TRN_TEST_NNTP_NEWSGROUP                 "${TRN_TEST_NEWSGROUP}")
    set(TRN_TEST_NNTP_NEWSGROUP_READ_ARTICLES   "")

    # mimecap file contents
    set(TRN_TEST_MIMECAP_FILE                   "${TRN_TEST_DOT_DIR}/mimecap")
    set(TRN_TEST_MIME_IMAGE_GIF_CONTENT_TYPE    "image/gif")
    set(TRN_TEST_MIME_IMAGE_GIF_COMMAND         "xgif %s")
    set(TRN_TEST_MIME_IMAGE_GIF_DESCRIPTION     "A gif image")
    set(TRN_TEST_MIME_IMAGE_ANY_CONTENT_TYPE    "image/*")
    set(TRN_TEST_MIME_IMAGE_ANY_COMMAND         "xv %s")
    set(TRN_TEST_MIME_IMAGE_ANY_DESCRIPTION     "Other image")
    set(TRN_TEST_MIME_APPLEFILE_CONTENT_TYPE    "application/applefile")
    set(TRN_TEST_MIME_APPLEFILE_COMMAND         "recieveAppleSingle %s")
    set(TRN_TEST_MIME_APPLEFILE_DESCRIPTION     "An Apple Macintosh file in AppleSingle format")
    set(TRN_TEST_MIME_TEXT_PLAIN_CONTENT_TYPE   "text/plain")
    set(TRN_TEST_MIME_TEXT_PLAIN_COMMAND        "cat %s")
    set(TRN_TEST_MIME_TEXT_PLAIN_DESCRIPTION    "A text file")
    set(TRN_TEST_MIME_PDF_CONTENT_TYPE          "application/pdf")
    set(TRN_TEST_MIME_PDF_COMMAND               "SumatraPDF %s")
    set(TRN_TEST_MIME_PDF_TEST_COMMAND          "check-content-type %t %s %{content-disposition}")
    set(TRN_TEST_MIME_PDF_DESCRIPTION           "Adobe PDF document")
    set(TRN_TEST_MIME_PDF_DECODE_FILE           "decoded_file.pdf")
    set(TRN_TEST_MIME_PDF_CONTENT_DISPOSITION   "none")
    set(TRN_TEST_MIME_PDF_SECTION_PARAMS        "X-Foo=bar; Content-Disposition=${TRN_TEST_MIME_PDF_CONTENT_DISPOSITION}; X-Bar=foo")
    set(TRN_TEST_MIME_PDF_TEST_EXEC_COMMAND     "check-content-type '${TRN_TEST_MIME_PDF_CONTENT_TYPE}' ${TRN_TEST_MIME_PDF_DECODE_FILE} '${TRN_TEST_MIME_PDF_CONTENT_DISPOSITION}'")

    # Generate test data files.
    configure_file(cmake/test_access_file.in            "${TRN_TEST_TRN_DIR}/access")
    configure_file(cmake/test_access_file.in            "${TRN_TEST_RN_LIB_DIR}/access.def")
    configure_file(cmake/test_active_file.in            "${TRN_TEST_SPOOL_DIR}/active")
    configure_file(cmake/test_article.in                "${TRN_TEST_ARTICLE_FILE}")
    configure_file(cmake/test_article_no_fallbacks.in   "${TRN_TEST_NEWSGROUP_DIR}/${TRN_TEST_ARTICLE_NO_FALLBACKS_NUM}")
    configure_file(cmake/test_local_newsrc.in           "${TRN_TEST_ACCESS_LOCAL_NEWSRC}")
    configure_file(cmake/test_mimecap_file.in           "${TRN_TEST_MIMECAP_FILE}")
    configure_file(cmake/test_nntp_auth_file.in         "${TRN_TEST_DOT_DIR}/.nntpauth")
    configure_file(cmake/test_nntp_newsrc.in            "${TRN_TEST_ACCESS_NNTP_NEWSRC}")
    configure_file(cmake/test_organization_file.in      "${TRN_TEST_ORGFILE}")

    # Generate test source files.
    # test_config.h needs the size of the generated article.
    file(SIZE "${TRN_TEST_ARTICLE_FILE}" TRN_TEST_ARTICLE_SIZE)
    configure_file(cmake/test_config.h.in               test_config.h)
    configure_file(cmake/test_data.cpp.in               test_data.cpp)
    configure_file(cmake/test_mime.h.in                 test_mime.h)

    set(TEST_MAKEDIR_BASE                   "${TRN_TEST_DATA_DIR}/make_dir")
    configure_file(cmake/test_makedir.h.in  test_makedir.h @ONLY)
    make_dir("${TEST_MAKEDIR_BASE}")
endfunction()
