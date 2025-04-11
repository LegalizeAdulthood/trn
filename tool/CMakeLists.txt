add_library(tool
    util3.cpp util3.h
)
target_include_directories(tool PUBLIC .)
target_include_directories(tool PRIVATE ../libtrn)
target_link_libraries(tool PUBLIC nntp)
target_folder(tool "Libraries")
