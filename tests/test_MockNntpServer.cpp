// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/MockNntpServer.h"

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <string>
#include <string_view>

namespace
{

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

tcp::socket connect_to_loopback(asio::io_context &context, int port)
{
    tcp::socket socket{context};
    socket.connect({asio::ip::address_v4::loopback(), static_cast<unsigned short>(port)});
    return socket;
}

std::string read_line(tcp::socket &socket)
{
    std::string line;
    read_until(socket, asio::dynamic_buffer(line), "\r\n");
    return line;
}

std::string read_one_line_from_loopback(int port)
{
    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, port);

    return read_line(socket);
}

void write_line(tcp::socket &socket, std::string_view line)
{
    std::string buffer{line};
    buffer += "\r\n";
    write(socket, asio::buffer(buffer));
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

TEST(MockNntpServerTest, sendsExpectedListResponse)
{
    MockNntpServer server;
    server.expect_mode_reader();
    server.expect_list({"comp.lang.apl 2 1 y", "comp.lang.c++ 3 1 y"});
    server.expect_quit();
    server.start();

    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, server.port());

    EXPECT_EQ("200 trn-test NNTP server ready\r\n", read_line(socket));
    write_line(socket, "MODE READER");
    EXPECT_EQ("200 trn-test reader mode\r\n", read_line(socket));
    write_line(socket, "LIST");
    EXPECT_EQ("215 list follows\r\n", read_line(socket));
    EXPECT_EQ("comp.lang.apl 2 1 y\r\n", read_line(socket));
    EXPECT_EQ("comp.lang.c++ 3 1 y\r\n", read_line(socket));
    EXPECT_EQ(".\r\n", read_line(socket));
    write_line(socket, "QUIT");
    EXPECT_EQ("205 closing connection\r\n", read_line(socket));
    server.verify();
}

TEST(MockNntpServerTest, sendsExpectedGroupAndArticleReadResponses)
{
    MockNntpServer server;
    server.expect_group("comp.lang.apl", 1, 623, 623);
    server.expect_listgroup({623});
    server.expect_head(623, {"From: reader@example.test", "Subject: hello"});
    server.expect_body(623, {"body line"});
    server.expect_quit();
    server.start();

    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, server.port());

    EXPECT_EQ("200 trn-test NNTP server ready\r\n", read_line(socket));
    write_line(socket, "GROUP comp.lang.apl");
    EXPECT_EQ("211 1 623 623 comp.lang.apl\r\n", read_line(socket));
    write_line(socket, "LISTGROUP");
    EXPECT_EQ("211 list follows\r\n", read_line(socket));
    EXPECT_EQ("623\r\n", read_line(socket));
    EXPECT_EQ(".\r\n", read_line(socket));
    write_line(socket, "HEAD 623");
    EXPECT_EQ("221 623 <message@example.test> header follows\r\n", read_line(socket));
    EXPECT_EQ("From: reader@example.test\r\n", read_line(socket));
    EXPECT_EQ("Subject: hello\r\n", read_line(socket));
    EXPECT_EQ(".\r\n", read_line(socket));
    write_line(socket, "BODY 623");
    EXPECT_EQ("222 623 <message@example.test> body follows\r\n", read_line(socket));
    EXPECT_EQ("body line\r\n", read_line(socket));
    EXPECT_EQ(".\r\n", read_line(socket));
    write_line(socket, "QUIT");
    EXPECT_EQ("205 closing connection\r\n", read_line(socket));
    server.verify();
}

TEST(MockNntpServerTest, verifyReportsUnexpectedCommand)
{
    MockNntpServer server;
    server.expect_mode_reader();
    server.start();

    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, server.port());

    EXPECT_EQ("200 trn-test NNTP server ready\r\n", read_line(socket));
    write_line(socket, "LIST");
    EXPECT_EQ("500 unexpected command\r\n", read_line(socket));

    EXPECT_NONFATAL_FAILURE(server.verify(), "expected NNTP command `MODE READER`, got `LIST`");
}

TEST(MockNntpServerTest, verifyReportsMissingCommand)
{
    MockNntpServer server;
    server.expect_mode_reader();
    server.start();

    EXPECT_NONFATAL_FAILURE(server.verify(), "expected NNTP command was not received: MODE READER");
}

TEST(MockNntpServerTest, verifyReportsWorkerThreadFailure)
{
    MockNntpServer server;
    server.set_welcome_banner("200 bad\nbanner");
    server.start();

    EXPECT_NONFATAL_FAILURE(server.verify(), "welcome banner must not contain line endings");
}
