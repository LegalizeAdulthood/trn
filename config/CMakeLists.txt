list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(configure_trn)
configure_trn()

add_library(config
    common.cpp common.h
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
    config2.h
    "${CMAKE_CURRENT_BINARY_DIR}/include/fdio.h"
    msdos.h
    "${CMAKE_CURRENT_BINARY_DIR}/include/pipe_io.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/string_case_compare.h" ${STRING_CASE_COMPARE_SOURCES}
    typedef.h
    user_id.h
    cmake/config.h.in
    cmake/configure_trn.cmake
    cmake/fdio.Linux.h.in
    cmake/fdio.Windows.h.in
    cmake/pipe_io.Linux.h.in
    cmake/pipe_io.Windows.h.in
    cmake/string_case_compare.Linux.h.in
    cmake/string_case_compare.Windows.h.in
)
target_include_directories(config PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include" .)
set_target_properties(config PROPERTIES FOLDER "Libraries")
source_group("CMake Templates" REGULAR_EXPRESSION "^.*\\.in$")
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
