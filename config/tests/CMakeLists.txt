include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(config-test
    test_env.cpp
    test_string_case_compare.cpp
)
target_link_libraries(config-test PUBLIC config GTest::gmock_main GTest::gtest)
target_folder(config-test "Tests")
gtest_discover_tests(config-test PROPERTIES LABELS trn)
