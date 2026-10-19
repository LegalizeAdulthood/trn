find_package(BoostAsio REQUIRED)
find_package(OpenSSL 3.2 REQUIRED)

add_library(nntp
    nntpauth.cpp include/nntp/nntpauth.h
    nntpclient.cpp include/nntp/nntpclient.h
    nntpinit.cpp include/nntp/nntpinit.h
)
target_include_directories(nntp PUBLIC include)
target_include_directories(nntp PRIVATE ../libtrn/include)
target_link_libraries(nntp PUBLIC boost::asio util PRIVATE fmt::fmt OpenSSL::SSL)
target_folder(nntp "Libraries")
