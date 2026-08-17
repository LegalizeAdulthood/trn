// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/MockNntpServer.h"

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <string>

namespace
{

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

std::string read_one_line_from_loopback(int port)
{
    asio::io_context context;
    tcp::socket      socket{context};
    socket.connect({asio::ip::address_v4::loopback(), static_cast<unsigned short>(port)});

    std::string line;
    read_until(socket, asio::dynamic_buffer(line), "\r\n");
    return line;
}

} // namespace

TEST(MockNntpServerTest, startsOnLoopbackPort)
{
    MockNntpServer server;
    server.start();

    EXPECT_GT(server.port(), 0);
    server.verify();
}

TEST(MockNntpServerTest, sendsDefaultWelcomeBanner)
{
    MockNntpServer server;
    server.start();

    EXPECT_EQ("200 trn-test NNTP server ready\r\n", read_one_line_from_loopback(server.port()));
    server.verify();
}

TEST(MockNntpServerTest, sendsConfiguredWelcomeBanner)
{
    MockNntpServer server;
    server.set_welcome_banner("201 trn-test NNTP server ready");
    server.start();

    EXPECT_EQ("201 trn-test NNTP server ready\r\n", read_one_line_from_loopback(server.port()));
    server.verify();
}

TEST(MockNntpServerTest, verifyReportsWorkerThreadFailure)
{
    MockNntpServer server;
    server.set_welcome_banner("200 bad\nbanner");
    server.start();

    EXPECT_NONFATAL_FAILURE(server.verify(), "welcome banner must not contain line endings");
}
