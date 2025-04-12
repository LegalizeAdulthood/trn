add_library(tool
    util3.cpp include/tool/util3.h
)
target_include_directories(tool PUBLIC include)
target_include_directories(tool PRIVATE ../libtrn/include)
target_link_libraries(tool PUBLIC nntp)
target_folder(tool "Libraries")
