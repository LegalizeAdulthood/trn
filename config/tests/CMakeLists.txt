include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(config-test
    test_env.cpp
    test_file_ref.cpp
    test_generated_config.cpp
    test_string_case_compare.cpp
)
target_link_libraries(config-test PUBLIC config GTest::gmock_main GTest::gtest)
target_include_directories(config-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_folder(config-test "Tests")
gtest_discover_tests(config-test PROPERTIES LABELS trn)

set(posting_host_match_segments_validation_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_posting_host_match_segments_validation.cmake")
set(posting_host_match_segments_configure_script
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/configure_trn.cmake")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsAcceptsZero
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=0"
        -P "${posting_host_match_segments_validation_script}")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsAcceptsLargeCount
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=15"
        -P "${posting_host_match_segments_validation_script}")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsRejectsText
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=bogus"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${posting_host_match_segments_validation_script}")
add_test(NAME ConfigCMakeTest.postingHostMatchSegmentsRejectsNegative
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_POSTING_HOST_MATCH_SEGMENTS=-1"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${posting_host_match_segments_validation_script}")
set_tests_properties(
    ConfigCMakeTest.postingHostMatchSegmentsAcceptsZero
    ConfigCMakeTest.postingHostMatchSegmentsAcceptsLargeCount
    ConfigCMakeTest.postingHostMatchSegmentsRejectsText
    ConfigCMakeTest.postingHostMatchSegmentsRejectsNegative
    PROPERTIES LABELS trn)

set(unsubscribed_char_validation_script
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_unsubscribed_char_validation.cmake")
add_test(NAME ConfigCMakeTest.unsubscribedCharAcceptsSingleCharacter
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_UNSUBSCRIBED_CHAR=?"
        -P "${unsubscribed_char_validation_script}")
add_test(NAME ConfigCMakeTest.unsubscribedCharRejectsEmpty
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_UNSUBSCRIBED_CHAR="
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${unsubscribed_char_validation_script}")
add_test(NAME ConfigCMakeTest.unsubscribedCharRejectsMultipleCharacters
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_UNSUBSCRIBED_CHAR=!!"
        "-DTRN_TEST_EXPECT_FAILURE=ON"
        -P "${unsubscribed_char_validation_script}")
set_tests_properties(
    ConfigCMakeTest.unsubscribedCharAcceptsSingleCharacter
    ConfigCMakeTest.unsubscribedCharRejectsEmpty
    ConfigCMakeTest.unsubscribedCharRejectsMultipleCharacters
    PROPERTIES LABELS trn)

set(save_path_defaults_test_script "${CMAKE_CURRENT_SOURCE_DIR}/cmake/test_save_path_defaults.cmake")
add_test(NAME ConfigCMakeTest.savePathDefaultsUseFlatMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_MAKE_SAVE_DIRECTORIES=OFF"
        -P "${save_path_defaults_test_script}")
add_test(NAME ConfigCMakeTest.savePathDefaultsUseCategoryMode
    COMMAND "${CMAKE_COMMAND}"
        "-DTRN_CONFIGURE_TRN_CMAKE:FILEPATH=${posting_host_match_segments_configure_script}"
        "-DTRN_TEST_MAKE_SAVE_DIRECTORIES=ON"
        -P "${save_path_defaults_test_script}")
set_tests_properties(
    ConfigCMakeTest.savePathDefaultsUseFlatMode
    ConfigCMakeTest.savePathDefaultsUseCategoryMode
    PROPERTIES LABELS trn)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    input_signal_probes.h
    @ONLY
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    wait_status_probe.h
    @ONLY
)

set(ACTIVE_TIMES "none")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_times_default.h
    @ONLY
)

set(ACTIVE_TIMES "%x/active.times")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_times_configured.h
    @ONLY
)

set(ACTIVE "${DEFAULT_ACTIVE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_default.h
    @ONLY
)

set(ACTIVE "%x/test-active")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    active_configured.h
    @ONLY
)

set(MAIL_FILE "${DEFAULT_MAIL_FILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    mail_file_default.h
    @ONLY
)

set(MAIL_FILE "%./mailbox")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    mail_file_configured.h
    @ONLY
)

set(NEWS_LIB "${DEFAULT_NEWS_LIB}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    news_lib_default.h
    @ONLY
)

set(NEWS_LIB "/var/lib/trn/news")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    news_lib_configured.h
    @ONLY
)

set(GROUP_DESC "${DEFAULT_GROUP_DESC}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    group_desc_default.h
    @ONLY
)

set(GROUP_DESC "/var/lib/trn/newsgroups")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    group_desc_configured.h
    @ONLY
)

set(SUBSCRIPTIONS "${DEFAULT_SUBSCRIPTIONS}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    subscriptions_default.h
    @ONLY
)

set(SUBSCRIPTIONS "/var/lib/trn/subscriptions")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    subscriptions_configured.h
    @ONLY
)

set(OVERVIEW_DIR "${DEFAULT_OVERVIEW_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    overview_dir_default.h
    @ONLY
)

set(OVERVIEW_DIR "/var/spool/news/overview")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    overview_dir_configured.h
    @ONLY
)

set(OVERVIEW_FMT "${DEFAULT_OVERVIEW_FMT}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    overview_fmt_default.h
    @ONLY
)

set(OVERVIEW_FMT "/var/lib/trn/overview.fmt")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    overview_fmt_configured.h
    @ONLY
)

set(POSTING_HOST_MATCH_SEGMENTS "${DEFAULT_POSTING_HOST_MATCH_SEGMENTS}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_host_match_segments_default.h
    @ONLY
)

set(POSTING_HOST_MATCH_SEGMENTS "4")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    posting_host_match_segments_configured.h
    @ONLY
)

set(PASS_NAMES "${DEFAULT_PASS_NAMES}")
set(BERKELEY_NAMES "${DEFAULT_BERKELEY_NAMES}")
set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}")
set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}")
set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_default.h
    @ONLY
)

set(PASS_NAMES OFF)
set(BERKELEY_NAMES OFF)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_fullname_file.h
    @ONLY
)

set(PASS_NAMES ON)
set(BERKELEY_NAMES OFF)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_usg.h
    @ONLY
)

set(PASS_NAMES ON)
set(BERKELEY_NAMES ON)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    name_parsing_berkeley.h
    @ONLY
)

set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}")
set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}")
set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    account_name_files_default.h
    @ONLY
)

set(PASSWORD_FILE "/var/lib/trn/passwd")
set(LOGIN_DIR_FIELD "7")
set(FULLNAMEFILE "%./profile-name")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    account_name_files_configured.h
    @ONLY
)

set(TRNDIR "${DEFAULT_TRNDIR}")
set(TRNMACRO "${DEFAULT_TRNMACRO}")
set(RNMACRO "${DEFAULT_RNMACRO}")
set(RCNAME "${DEFAULT_RCNAME}")
set(LOCKNAME "${DEFAULT_LOCKNAME}")
set(LASTNAME "${DEFAULT_LASTNAME}")
set(SIGNATURE_FILE "${DEFAULT_SIGNATURE_FILE}")
set(NNTP_AUTH_FILE "${DEFAULT_NNTP_AUTH_FILE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    user_state_paths_default.h
    @ONLY
)

set(TRNDIR "%./trn-test")
set(TRNMACRO "%+/test-macros")
set(RNMACRO "%./test-rnmac")
set(RCNAME "%./test-newsrc")
set(LOCKNAME "%./test-rnlock")
set(LASTNAME "%./test-rnlast")
set(SIGNATURE_FILE "%./test-signature")
set(NNTP_AUTH_FILE "%./test-nntpauth")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    user_state_paths_configured.h
    @ONLY
)

set(GLOBAL_INIT "${DEFAULT_GLOBAL_INIT}")
set(DEFACCESS "${DEFAULT_DEFACCESS}")
set(TRNACCESS "${DEFAULT_TRNACCESS}")
set(NEWSNEWSNAME "${DEFAULT_NEWSNEWSNAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    data_file_paths_default.h
    @ONLY
)

set(GLOBAL_INIT "%X/test-INIT")
set(DEFACCESS "%X/test-access.def")
set(TRNACCESS "%+/test-access")
set(NEWSNEWSNAME "%X/test-newsnews")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    data_file_paths_configured.h
    @ONLY
)

set(OV_FILE_NAME "${DEFAULT_OV_FILE_NAME}")
set(VARYNAME "${DEFAULT_VARYNAME}")
set(HEADNAME "${DEFAULT_HEADNAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    article_path_templates_default.h
    @ONLY
)

set(OV_FILE_NAME "/test-overview")
set(VARYNAME "%Y/test-vary.%$")
set(HEADNAME "%./test-head.%$")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    article_path_templates_configured.h
    @ONLY
)

set(SAVEDIR "${DEFAULT_SAVEDIR}")
set(SAVENAME "${DEFAULT_SAVENAME}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    save_path_templates_default.h
    @ONLY
)

set(SAVEDIR "%p/test-save")
set(SAVENAME "test-%a")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    save_path_templates_configured.h
    @ONLY
)

set(UNSUBSCRIBED_CHAR "${DEFAULT_UNSUBSCRIBED_CHAR}")
set(SELECTION_CHARS "${DEFAULT_SELECTION_CHARS}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    selection_policy_default.h
    @ONLY
)

set(UNSUBSCRIBED_CHAR "?")
set(SELECTION_CHARS "xyz789")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/config.h.in"
    selection_policy_configured.h
    @ONLY
)

set(NAME_TYPE "${DEFAULT_NAME_TYPE}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../data/Pnews.in"
    pnews_name_type_default
    @ONLY
)

set(PASS_NAMES "${DEFAULT_PASS_NAMES}")
set(BERKELEY_NAMES "${DEFAULT_BERKELEY_NAMES}")
set(PASSWORD_FILE "${DEFAULT_PASSWORD_FILE}")
set(LOGIN_DIR_FIELD "${DEFAULT_LOGIN_DIR_FIELD}")
set(FULLNAMEFILE "${DEFAULT_FULLNAMEFILE}")
set(TRNDIR "${DEFAULT_TRNDIR}")
set(TRNMACRO "${DEFAULT_TRNMACRO}")
set(RNMACRO "${DEFAULT_RNMACRO}")
set(RCNAME "${DEFAULT_RCNAME}")
set(LOCKNAME "${DEFAULT_LOCKNAME}")
set(LASTNAME "${DEFAULT_LASTNAME}")
set(SIGNATURE_FILE "${DEFAULT_SIGNATURE_FILE}")
set(NNTP_AUTH_FILE "${DEFAULT_NNTP_AUTH_FILE}")
set(GLOBAL_INIT "${DEFAULT_GLOBAL_INIT}")
set(DEFACCESS "${DEFAULT_DEFACCESS}")
set(TRNACCESS "${DEFAULT_TRNACCESS}")
set(NEWSNEWSNAME "${DEFAULT_NEWSNEWSNAME}")
set(OV_FILE_NAME "${DEFAULT_OV_FILE_NAME}")
set(VARYNAME "${DEFAULT_VARYNAME}")
set(HEADNAME "${DEFAULT_HEADNAME}")
set(SAVEDIR "${DEFAULT_SAVEDIR}")
set(SAVENAME "${DEFAULT_SAVENAME}")
set(UNSUBSCRIBED_CHAR "${DEFAULT_UNSUBSCRIBED_CHAR}")
set(SELECTION_CHARS "${DEFAULT_SELECTION_CHARS}")

configure_file(cmake/test_generated_config.h.in test_generated_config.h @ONLY)
