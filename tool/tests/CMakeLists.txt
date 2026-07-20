include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(tool-test
    test_util3.cpp
)
target_link_libraries(tool-test PUBLIC tool GTest::gmock_main GTest::gtest)
target_folder(tool-test "Tests")
gtest_discover_tests(tool-test PROPERTIES LABELS trn)
