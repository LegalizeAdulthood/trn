find_package(strong_type CONFIG REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(configure_trn)
configure_trn()

add_library(config
    common.cpp include/config/common.h
    include/config/config2.h
    include/config/msdos.h
    include/config/typedef.h
    include/config/user_id.h
    cmake/config.h.in
    cmake/configure_trn.cmake
    cmake/fdio.fcntl.h.in
    cmake/fdio.io.h.in
    cmake/pipe_io.standard.h.in
    cmake/pipe_io.underscore.h.in
    cmake/string_case_compare.strings.h.in
    cmake/string_case_compare.manual.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/config.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/fdio.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/pipe_io.h"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config/string_case_compare.h" ${STRING_CASE_COMPARE_SOURCES}
)
target_include_directories(config PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include" include)
target_link_libraries(config PUBLIC strong_type::strong_type)
target_folder(config "Libraries")
source_group("CMake Templates" REGULAR_EXPRESSION "^.*\\.in$")
source_group("CMake Scripts" REGULAR_EXPRESSION "^.*\\.cmake$")

add_subdirectory(data)
add_subdirectory(legacy)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
