// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/MockNntpServer.h"

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include <boost/asio.hpp>

#include <string>
#include <string_view>
#include <vector>

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

std::string read_line(tcp::socket &socket, std::string &buffer)
{
    const std::size_t size = asio::read_until(socket, asio::dynamic_buffer(buffer), "\r\n");
    std::string       line = buffer.substr(0, size);
    buffer.erase(0, size);
    return line;
}

std::string read_one_line_from_loopback(int port)
{
    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, port);
    std::string      buffer;

    return read_line(socket, buffer);
}

void write_line(tcp::socket &socket, std::string_view line)
{
    std::string buffer{line};
    buffer += "\r\n";
    write(socket, asio::buffer(buffer));
}

class ConnectedMockNntpServerTest : public testing::Test
{
protected:
    void connect();
    void TearDown() override
    {
        m_server.verify();
    }
    std::vector<std::string> read_lines(std::size_t count);

    MockNntpServer   m_server;
    asio::io_context m_context;
    tcp::socket      m_socket{m_context};
    std::string      m_buffer;
};

void ConnectedMockNntpServerTest::connect()
{
    m_server.start();
    m_socket = connect_to_loopback(m_context, m_server.port());
    ASSERT_EQ("200 trn-test NNTP server ready\r\n", read_line(m_socket, m_buffer));
}

std::vector<std::string> ConnectedMockNntpServerTest::read_lines(std::size_t count)
{
    std::vector<std::string> lines;
    while (count-- > 0)
    {
        lines.push_back(read_line(m_socket, m_buffer));
    }
    return lines;
}

class GroupedMockNntpServerTest : public ConnectedMockNntpServerTest
{
protected:
    void SetUp() override
    {
        m_server.expect_group("comp.lang.apl", 1, 623, 623);
    }
    void connect_group();
};

void GroupedMockNntpServerTest::connect_group()
{
    ASSERT_NO_FATAL_FAILURE(connect());
    write_line(m_socket, "GROUP comp.lang.apl");
    ASSERT_EQ("211 1 623 623 comp.lang.apl\r\n", read_line(m_socket, m_buffer));
}

} // namespace

TEST(MockNntpServerTest, startsOnLoopbackPort)
{
    MockNntpServer server;
    server.start();

    EXPECT_GT(server.port(), 0);
    server.verify();
}

TEST(MockNntpServerTest, repeatedlyStopsBeforeClientConnects)
{
    MockNntpServer server;
    for (int pass = 0; pass < 20; ++pass)
    {
        SCOPED_TRACE(pass);
        server.start();
        EXPECT_GT(server.port(), 0);
        server.stop();
    }
    server.verify();
}

TEST(MockNntpServerTest, stopsWhileWaitingForCommand)
{
    MockNntpServer server;
    server.expect_mode_reader();
    server.start();

    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, server.port());
    std::string      buffer;
    ASSERT_EQ("200 trn-test NNTP server ready\r\n", read_line(socket, buffer));

    server.stop();

    EXPECT_NONFATAL_FAILURE(server.verify(), "expected NNTP command was not received: MODE READER");
}

TEST(MockNntpServerTest, stopsDuringStartTlsHandshake)
{
    MockNntpServer server;
    server.expect_starttls(true);
    server.start();

    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, server.port());
    std::string      buffer;
    ASSERT_EQ("200 trn-test NNTP server ready\r\n", read_line(socket, buffer));
    write_line(socket, "STARTTLS");
    ASSERT_EQ("382 continue with TLS negotiation\r\n", read_line(socket, buffer));

    server.stop();
    server.verify();
}

TEST(MockNntpServerTest, lineReadsPreserveBufferedReplies)
{
    asio::io_context context;
    tcp::socket      socket{context};
    std::string      buffer{"200 first\r\n201 second\r\npartial"};

    EXPECT_EQ("200 first\r\n", read_line(socket, buffer));
    EXPECT_EQ("201 second\r\npartial", buffer);
    EXPECT_EQ("201 second\r\n", read_line(socket, buffer));
    EXPECT_EQ("partial", buffer);
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

TEST_F(ConnectedMockNntpServerTest, sendsExpectedReaderModeResponse)
{
    m_server.expect_mode_reader();
    ASSERT_NO_FATAL_FAILURE(connect());

    write_line(m_socket, "MODE READER");
    const std::string response = read_line(m_socket, m_buffer);

    EXPECT_EQ("200 trn-test reader mode\r\n", response);
}

TEST_F(ConnectedMockNntpServerTest, sendsExpectedListResponse)
{
    m_server.expect_list({"comp.lang.apl 2 1 y", "comp.lang.c++ 3 1 y"});
    ASSERT_NO_FATAL_FAILURE(connect());
    const std::vector<std::string> expected{"215 list follows\r\n", "comp.lang.apl 2 1 y\r\n",
                                            "comp.lang.c++ 3 1 y\r\n", ".\r\n"};

    write_line(m_socket, "LIST");
    const std::vector<std::string> response = read_lines(expected.size());

    EXPECT_EQ(expected, response);
}

TEST_F(ConnectedMockNntpServerTest, sendsExpectedQuitResponse)
{
    m_server.expect_quit();
    ASSERT_NO_FATAL_FAILURE(connect());

    write_line(m_socket, "QUIT");
    const std::string response = read_line(m_socket, m_buffer);

    EXPECT_EQ("205 closing connection\r\n", response);
}

TEST_F(ConnectedMockNntpServerTest, sendsExpectedGroupResponse)
{
    m_server.expect_group("comp.lang.apl", 1, 623, 623);
    ASSERT_NO_FATAL_FAILURE(connect());

    write_line(m_socket, "GROUP comp.lang.apl");
    const std::string response = read_line(m_socket, m_buffer);

    EXPECT_EQ("211 1 623 623 comp.lang.apl\r\n", response);
}

TEST_F(GroupedMockNntpServerTest, sendsExpectedListGroupResponse)
{
    m_server.expect_listgroup({623});
    ASSERT_NO_FATAL_FAILURE(connect_group());
    const std::vector<std::string> expected{"211 list follows\r\n", "623\r\n", ".\r\n"};

    write_line(m_socket, "LISTGROUP");
    const std::vector<std::string> response = read_lines(expected.size());

    EXPECT_EQ(expected, response);
}

TEST_F(GroupedMockNntpServerTest, sendsExpectedHeadResponse)
{
    m_server.expect_head(623, {"From: reader@example.test", "Subject: hello"});
    ASSERT_NO_FATAL_FAILURE(connect_group());
    const std::vector<std::string> expected{"221 623 <message@example.test> header follows\r\n",
                                            "From: reader@example.test\r\n", "Subject: hello\r\n", ".\r\n"};

    write_line(m_socket, "HEAD 623");
    const std::vector<std::string> response = read_lines(expected.size());

    EXPECT_EQ(expected, response);
}

TEST_F(GroupedMockNntpServerTest, sendsExpectedBodyResponse)
{
    m_server.expect_body(623, {"body line"});
    ASSERT_NO_FATAL_FAILURE(connect_group());
    const std::vector<std::string> expected{"222 623 <message@example.test> body follows\r\n", "body line\r\n",
                                            ".\r\n"};

    write_line(m_socket, "BODY 623");
    const std::vector<std::string> response = read_lines(expected.size());

    EXPECT_EQ(expected, response);
}

TEST(MockNntpServerTest, verifyReportsUnexpectedCommand)
{
    MockNntpServer server;
    server.expect_mode_reader();
    server.start();

    asio::io_context context;
    tcp::socket      socket = connect_to_loopback(context, server.port());
    std::string      buffer;

    EXPECT_EQ("200 trn-test NNTP server ready\r\n", read_line(socket, buffer));
    write_line(socket, "LIST");
    EXPECT_EQ("500 unexpected command\r\n", read_line(socket, buffer));

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
