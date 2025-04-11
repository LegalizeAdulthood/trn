add_executable(inews inews.cpp)
target_include_directories(inews PRIVATE ../libtrn)
target_link_libraries(inews PUBLIC nntp tool)
target_folder(inews "Tools")
