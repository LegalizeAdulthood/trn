find_package(BoostAsio REQUIRED)

add_library(nntp
    nntpauth.cpp include/nntp/nntpauth.h
    nntpclient.cpp include/nntp/nntpclient.h
    nntpinit.cpp include/nntp/nntpinit.h
)
target_include_directories(nntp PUBLIC include)
target_include_directories(nntp PRIVATE ../libtrn)
target_link_libraries(nntp PUBLIC boost::asio util)
target_folder(nntp "Libraries")
