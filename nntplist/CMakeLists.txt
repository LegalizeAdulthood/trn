add_executable(nntplist nntplist.cpp)
target_include_directories(nntplist PRIVATE ../libtrn)
target_link_libraries(nntplist PUBLIC nntp tool)
target_folder(nntplist "Tools")
