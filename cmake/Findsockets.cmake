include(FindPackageHandleStandardArgs)

find_library(SOCKETS_LIB ws2_32.lib)
find_path(SOCKETS_INCLUDE winsock.h)

find_package_handle_standard_args(sockets REQUIRED_VARS SOCKETS_LIB SOCKETS_INCLUDE)

if(sockets_FOUND)
    add_library(sockets STATIC IMPORTED)
    target_include_directories(sockets INTERFACE ${SOCKETS_INCLUDE})
    set_target_properties(sockets PROPERTIES IMPORTED_LOCATION ${SOCKETS_LIB})
endif()
