add_executable(trn-artchk trn-artchk.cpp)
target_include_directories(trn-artchk PRIVATE ../libtrn)
target_link_libraries(trn-artchk PUBLIC nntp tool)
set_property(TARGET trn-artchk PROPERTY FOLDER Tools)
