list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(configure_trn)
configure_trn()

add_library(config
    common.cpp common.h
    config2.h
    msdos.h
    typedef.h
    config.h.in
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h")
target_include_directories(config PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include" .)
set_target_properties(config PROPERTIES FOLDER "Libraries")
source_group("CMake Templates" REGULAR_EXPRESSION "^.*\\.in$")
