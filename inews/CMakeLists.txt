add_library(inews-common
    post_response.cpp include/inews/post_response.h
)
target_include_directories(inews-common PUBLIC include)
target_folder(inews-common "Libraries")

add_executable(inews inews.cpp)
target_include_directories(inews PRIVATE ../libtrn/include)
target_link_libraries(inews PUBLIC nntp tool PRIVATE inews-common fmt::fmt)
target_folder(inews "Tools")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
