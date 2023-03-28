find_package(GTest CONFIG REQUIRED)

configure_file(test_config.h.in test_config.h)
add_executable(trn-test
    test_enum_order.cpp
    test_flags.cpp
    test_init.cpp
    test_interp.cpp
    test_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
)
target_include_directories(trn-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(trn-test PUBLIC libtrn GTest::gmock_main GTest::gtest)
set_property(TARGET trn-test PROPERTY FOLDER Tests)
