add_executable(trn-artchk trn-artchk.cpp)
target_include_directories(trn-artchk PRIVATE ../libtrn/include)
target_link_libraries(trn-artchk PUBLIC nntp tool PRIVATE fmt::fmt)
target_folder(trn-artchk "Tools")
install(TARGETS trn-artchk DESTINATION ${CMAKE_INSTALL_BINDIR})

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
