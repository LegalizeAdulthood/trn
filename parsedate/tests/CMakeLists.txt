include(GoogleTest)

find_package(GTest CONFIG REQUIRED)

add_executable(parsedate-test
    test.cpp
)
target_link_libraries(parsedate-test PUBLIC parsedate GTest::gtest_main)
set_property(TARGET parsedate-test PROPERTY FOLDER Tests)

gtest_discover_tests(parsedate-test PROPERTIES LABEL parsedate)
