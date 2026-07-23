add_executable(nntplist nntplist.cpp)
target_include_directories(nntplist PRIVATE ../libtrn/include)
target_link_libraries(nntplist PUBLIC nntp tool PRIVATE fmt::fmt)
target_folder(nntplist "Tools")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
