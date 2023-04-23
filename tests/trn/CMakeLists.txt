find_package(GTest CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(ConfigureTrnTestData)

configure_trn_test_data()

add_executable(trn-test
    mock_env.h
    test_cache.cpp
    test_enum_order.cpp
    test_flags.cpp
    test_init.cpp
    test_interp.cpp
    test_nntp.cpp
    test_rt-util.cpp
    test_utf.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/test_config.h
    ${CMAKE_CURRENT_BINARY_DIR}/test_data.cpp
    cmake/ConfigureTrnTestData.cmake
    cmake/test_access_file.in
    cmake/test_active_file.in
    cmake/test_article.in
    cmake/test_article_no_fallbacks.in
    cmake/test_config.h.in
    cmake/test_data.cpp.in
    cmake/test_local_newsrc.in
    cmake/test_nntp_auth_file.in
    cmake/test_nntp_newsrc.in
    cmake/test_organization_file.in
)
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")
source_group("Template Files" REGULAR_EXPRESSION "^.*\\.in$")
target_include_directories(trn-test PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" .)
target_link_libraries(trn-test PUBLIC libtrn GTest::gmock_main GTest::gtest)
set_property(TARGET trn-test PROPERTY FOLDER Tests)
