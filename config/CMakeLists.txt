list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(configure_trn)
configure_trn()

add_library(config
    common.cpp include/config/common.h
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/config.h"
    include/config/config2.h
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/fdio.h"
    include/config/msdos.h
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/pipe_io.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/string_case_compare.h" ${STRING_CASE_COMPARE_SOURCES}
    include/config/typedef.h
    include/config/user_id.h
    cmake/config.h.in
    cmake/configure_trn.cmake
    cmake/fdio.Linux.h.in
    cmake/fdio.Windows.h.in
    cmake/pipe_io.Linux.h.in
    cmake/pipe_io.Windows.h.in
    cmake/string_case_compare.Linux.h.in
    cmake/string_case_compare.Windows.h.in
)
target_include_directories(config PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include" include)
target_folder(config "Libraries")
source_group("CMake Templates" REGULAR_EXPRESSION "^.*\\.in$")
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
