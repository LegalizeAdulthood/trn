add_executable(trn-artchk trn-artchk.cpp)
target_include_directories(trn-artchk PRIVATE ../libtrn/include)
target_link_libraries(trn-artchk PUBLIC nntp tool)
target_folder(trn-artchk "Tools")
