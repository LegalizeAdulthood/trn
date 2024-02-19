add_library(tool
    util3.cpp util3.h
)
target_include_directories(tool PUBLIC .)
target_include_directories(tool PRIVATE ..)
target_link_libraries(tool PUBLIC nntp)
set_property(TARGET tool PROPERTY FOLDER Libraries)
