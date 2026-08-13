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

configure_file(cmake/test_generated_config.h.in test_generated_config.h @ONLY)
