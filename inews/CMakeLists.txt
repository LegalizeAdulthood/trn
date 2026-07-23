add_executable(inews inews.cpp)
target_include_directories(inews PRIVATE ../libtrn/include)
target_link_libraries(inews PUBLIC nntp tool PRIVATE fmt::fmt)
target_folder(inews "Tools")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
