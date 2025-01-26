include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(config-test
    test_string_case_compare.cpp
)
target_link_libraries(config-test PUBLIC config GTest::gmock_main GTest::gtest)
set_target_properties(config-test PROPERTIES FOLDER "Tests")
