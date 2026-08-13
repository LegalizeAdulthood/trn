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

configure_file(cmake/test_generated_config.h.in test_generated_config.h @ONLY)
