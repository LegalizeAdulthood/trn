find_package(BoostAsio REQUIRED)

add_library(nntp
    nntpauth.cpp nntpauth.h
    nntpclient.cpp nntpclient.h
    nntpinit.cpp nntpinit.h
)
target_include_directories(nntp PUBLIC .)
target_include_directories(nntp PRIVATE ../libtrn)
target_link_libraries(nntp PUBLIC boost::asio util)
set_property(TARGET nntp PROPERTY FOLDER Libraries)
