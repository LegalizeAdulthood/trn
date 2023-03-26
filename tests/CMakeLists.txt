find_package(GTest CONFIG REQUIRED)

add_executable(trn-test
    test_enum_order.cpp
    test_interp.cpp
)
target_link_libraries(trn-test PUBLIC libtrn GTest::gtest_main)
set_property(TARGET trn-test PROPERTY FOLDER Tests)
