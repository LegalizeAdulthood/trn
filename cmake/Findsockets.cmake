include(FindPackageHandleStandardArgs)

if(WIN32)
    add_library(sockets INTERFACE)
    target_compile_definitions(sockets INTERFACE WINSOCK)
    target_link_libraries(sockets INTERFACE ws2_32.lib)
    set(SOCKETS_INTRINSIC ON)
    find_package_handle_standard_args(sockets DEFAULT_MSG SOCKETS_INTRINSIC)
    return()
endif()

find_library(SOCKETS_LIB libc.a)
find_path(SOCKETS_INCLUDE sys/socket.h)

find_package_handle_standard_args(sockets REQUIRED_VARS SOCKETS_LIB SOCKETS_INCLUDE)

if(sockets_FOUND)
    add_library(sockets STATIC IMPORTED)
    target_include_directories(sockets INTERFACE ${SOCKETS_INCLUDE})
    set_target_properties(sockets PROPERTIES IMPORTED_LOCATION ${SOCKETS_LIB})
endif()
