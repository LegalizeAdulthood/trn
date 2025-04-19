include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(config-test
    test_string_case_compare.cpp
    test_strong_int.cpp
)
target_link_libraries(config-test PUBLIC config GTest::gmock_main GTest::gtest)
target_folder(config-test "Tests")
