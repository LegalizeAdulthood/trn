add_executable(inews inews.cpp)
target_include_directories(inews PRIVATE ../libtrn/include)
target_link_libraries(inews PUBLIC nntp tool)
target_folder(inews "Tools")
