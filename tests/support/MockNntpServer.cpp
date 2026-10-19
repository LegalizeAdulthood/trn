// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include "support/MockNntpServer.h"
#include "support/NntpTestCertificate.h"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/system/error_code.hpp>

#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{

namespace asio = boost::asio;
using boost::system::error_code;
using tcp = asio::ip::tcp;

constexpr const char *DEFAULT_WELCOME_BANNER = "200 trn-test NNTP server ready";

std::vector<std::string> multiline_response(std::string status_line, std::vector<std::string> lines)
{
    std::vector<std::string> response_lines;
    response_lines.reserve(lines.size() + 2);
    response_lines.push_back(std::move(status_line));
    for (std::string &line : lines)
    {
        response_lines.push_back(std::move(line));
    }
    response_lines.emplace_back(".");
    return response_lines;
}

bool has_line_endings(std::string_view line)
{
    return line.find_first_of("\r\n") != std::string_view::npos;
}

} // namespace

struct MockNntpServer::ExpectedCommand
{
    std::string              command;
    std::vector<std::string> response_lines;
    bool                     starttls{};
    bool                     handshake_failure{};
};

struct MockNntpServer::Impl
{
    asio::io_context             context;
    asio::ssl::context           tls_context{asio::ssl::context::tls_server};
    tcp::acceptor                acceptor{context};
    std::unique_ptr<tcp::socket> socket;
    std::thread                  worker;
    mutable std::mutex           mutex;
    std::deque<ExpectedCommand>  expected_commands;
    std::string                  welcome_banner{DEFAULT_WELCOME_BANNER};
    std::vector<std::string> failures;
    int                      port{};
    bool                     stopping{};
    bool                         implicit_tls{};
    bool                         handshake_failure{};
};

MockNntpServer::MockNntpServer() :
    m_impl{std::make_unique<Impl>()}
{
}

MockNntpServer::~MockNntpServer()
{
    stop();
}

void MockNntpServer::expect_body(long article_number, std::vector<std::string> lines)
{
    expect_command("BODY " + std::to_string(article_number),
                   multiline_response("222 " + std::to_string(article_number) + " <message@example.test> body follows",
                                      std::move(lines)));
}

void MockNntpServer::expect_group(std::string group_name, long count, long first, long last)
{
    const std::string command = "GROUP " + group_name;
    expect_command(std::move(command), {"211 " + std::to_string(count) + " " + std::to_string(first) + " " +
                                        std::to_string(last) + " " + std::move(group_name)});
}

void MockNntpServer::expect_head(long article_number, std::vector<std::string> lines)
{
    expect_command(
        "HEAD " + std::to_string(article_number),
        multiline_response("221 " + std::to_string(article_number) + " <message@example.test> header follows",
                           std::move(lines)));
}

void MockNntpServer::expect_list(std::vector<std::string> lines)
{
    expect_command("LIST", multiline_response("215 list follows", std::move(lines)));
}

void MockNntpServer::expect_listgroup(std::vector<long> article_numbers)
{
    std::vector<std::string> lines;
    lines.reserve(article_numbers.size());
    for (const long article_number : article_numbers)
    {
        lines.push_back(std::to_string(article_number));
    }
    expect_command("LISTGROUP", multiline_response("211 list follows", std::move(lines)));
}

void MockNntpServer::expect_capabilities(std::vector<std::string> lines)
{
    expect_command("CAPABILITIES", multiline_response("101 capability list follows", std::move(lines)));
}

void MockNntpServer::expect_mode_reader()
{
    expect_command("MODE READER", {"200 trn-test reader mode"});
}

void MockNntpServer::expect_quit()
{
    expect_command("QUIT", {"205 closing connection"});
}

void MockNntpServer::expect_starttls(bool handshake_failure)
{
    configure_tls();
    std::lock_guard guard{m_impl->mutex};
    m_impl->expected_commands.push_back({"STARTTLS", {"382 continue with TLS negotiation"}, true, handshake_failure});
}

void MockNntpServer::use_implicit_tls(bool handshake_failure)
{
    configure_tls();
    m_impl->implicit_tls = true;
    m_impl->handshake_failure = handshake_failure;
}

void MockNntpServer::configure_tls()
{
    m_impl->tls_context.use_certificate(asio::buffer(NNTP_TEST_CERTIFICATE, sizeof(NNTP_TEST_CERTIFICATE) - 1),
                                        asio::ssl::context::pem);
    m_impl->tls_context.use_private_key(asio::buffer(NNTP_TEST_PRIVATE_KEY, sizeof(NNTP_TEST_PRIVATE_KEY) - 1),
                                        asio::ssl::context::pem);
}

void MockNntpServer::set_welcome_banner(std::string line)
{
    std::lock_guard guard{m_impl->mutex};
    m_impl->welcome_banner = std::move(line);
}

void MockNntpServer::start()
{
    {
        std::lock_guard guard{m_impl->mutex};
        if (m_impl->worker.joinable())
        {
            m_impl->failures.emplace_back("MockNntpServer already started");
            return;
        }
        m_impl->failures.clear();
        m_impl->port = 0;
        m_impl->socket.reset();
        m_impl->stopping = false;
    }

    error_code          ec;
    const tcp::endpoint endpoint{asio::ip::address_v4::loopback(), 0};
    m_impl->acceptor.open(endpoint.protocol(), ec);
    if (ec)
    {
        record_failure("open failed: " + ec.message());
        return;
    }

    m_impl->acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
    if (ec)
    {
        record_failure("set reuse_address failed: " + ec.message());
        return;
    }

    m_impl->acceptor.bind(endpoint, ec);
    if (ec)
    {
        record_failure("bind failed: " + ec.message());
        return;
    }

    m_impl->acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec)
    {
        record_failure("listen failed: " + ec.message());
        return;
    }

    {
        std::lock_guard guard{m_impl->mutex};
        m_impl->port = m_impl->acceptor.local_endpoint().port();
    }
    m_impl->worker = std::thread{&MockNntpServer::run, this};
}

int MockNntpServer::port() const
{
    std::lock_guard guard{m_impl->mutex};
    return m_impl->port;
}

void MockNntpServer::stop()
{
    tcp::socket *socket{};
    {
        std::lock_guard guard{m_impl->mutex};
        m_impl->stopping = true;
        socket = m_impl->socket.get();
    }

    error_code ec;
    if (socket != nullptr)
    {
        socket->close(ec);
    }
    m_impl->acceptor.close(ec);
    if (m_impl->worker.joinable())
    {
        m_impl->worker.join();
    }
    {
        std::lock_guard guard{m_impl->mutex};
        m_impl->socket.reset();
    }
}

void MockNntpServer::verify()
{
    stop();

    std::vector<std::string> failures;
    {
        std::lock_guard guard{m_impl->mutex};
        failures = m_impl->failures;
    }
    std::deque<ExpectedCommand> expected_commands;
    {
        std::lock_guard guard{m_impl->mutex};
        expected_commands = m_impl->expected_commands;
        m_impl->expected_commands.clear();
    }
    for (const std::string &failure : failures)
    {
        ADD_FAILURE() << failure;
    }
    for (const ExpectedCommand &expected : expected_commands)
    {
        ADD_FAILURE() << "expected NNTP command was not received: " << expected.command;
    }
}

bool MockNntpServer::build_welcome_banner(std::string &line)
{
    {
        std::lock_guard guard{m_impl->mutex};
        line = m_impl->welcome_banner;
    }
    if (line.find_first_of("\r\n") != std::string::npos)
    {
        record_failure("welcome banner must not contain line endings");
        return false;
    }
    line += "\r\n";
    return true;
}

void MockNntpServer::consume_expected_command()
{
    std::lock_guard guard{m_impl->mutex};
    m_impl->expected_commands.pop_front();
}

void MockNntpServer::expect_command(std::string command, std::vector<std::string> response_lines)
{
    std::lock_guard guard{m_impl->mutex};
    m_impl->expected_commands.push_back({std::move(command), std::move(response_lines)});
}

bool MockNntpServer::next_expected_command(ExpectedCommand &command) const
{
    std::lock_guard guard{m_impl->mutex};
    if (m_impl->expected_commands.empty())
    {
        return false;
    }
    command = m_impl->expected_commands.front();
    return true;
}

void MockNntpServer::record_failure(std::string message)
{
    std::lock_guard guard{m_impl->mutex};
    m_impl->failures.push_back(std::move(message));
}

void MockNntpServer::run()
{
    std::string banner;
    if (!build_welcome_banner(banner))
    {
        return;
    }

    tcp::socket *socket{};
    {
        std::lock_guard guard{m_impl->mutex};
        m_impl->socket = std::make_unique<tcp::socket>(m_impl->context);
        socket = m_impl->socket.get();
    }

    error_code ec;
    m_impl->acceptor.accept(*socket, ec);
    if (ec)
    {
        if (!stopping())
        {
            record_failure("accept failed: " + ec.message());
        }
        return;
    }

    std::unique_ptr<asio::ssl::stream<tcp::socket &>> tls;
    auto                                              negotiate_tls = [&socket, &tls, this](bool handshake_failure)
    {
        tls = std::make_unique<asio::ssl::stream<tcp::socket &>>(*socket, m_impl->tls_context);
        error_code handshake_ec;
        tls->handshake(asio::ssl::stream_base::server, handshake_ec);
        if (handshake_ec)
        {
            if (!handshake_failure)
            {
                record_failure("TLS handshake failed: " + handshake_ec.message());
            }
            return false;
        }
        if (handshake_failure)
        {
            record_failure("TLS handshake unexpectedly succeeded");
        }
        return true;
    };
    if (m_impl->implicit_tls && !negotiate_tls(m_impl->handshake_failure))
    {
        return;
    }
    if (tls)
    {
        asio::write(*tls, asio::buffer(banner), ec);
    }
    else
    {
        asio::write(*socket, asio::buffer(banner), ec);
    }
    if (ec && !stopping())
    {
        record_failure("write failed: " + ec.message());
        return;
    }

    std::string                                       buffer;
    auto read_command = [&socket, &tls, &buffer, this](std::string &command)
    {
        error_code  read_ec;
        const std::size_t size = tls ? asio::read_until(*tls, asio::dynamic_buffer(buffer), "\r\n", read_ec)
                                     : asio::read_until(*socket, asio::dynamic_buffer(buffer), "\r\n", read_ec);
        if (read_ec)
        {
            if (!stopping())
            {
                record_failure("read failed: " + read_ec.message());
            }
            return false;
        }
        command = buffer.substr(0, size - 2);
        buffer.erase(0, size);
        return true;
    };

    auto write_line = [&socket, &tls, this](std::string_view line)
    {
        if (has_line_endings(line))
        {
            record_failure("NNTP response line must not contain line endings");
            return false;
        }

        error_code  write_ec;
        std::string response{line};
        response += "\r\n";
        if (tls)
        {
            asio::write(*tls, asio::buffer(response), write_ec);
        }
        else
        {
            asio::write(*socket, asio::buffer(response), write_ec);
        }
        if (write_ec && !stopping())
        {
            record_failure("write failed: " + write_ec.message());
            return false;
        }
        return true;
    };

    ExpectedCommand expected;
    while (next_expected_command(expected))
    {
        std::string command;
        if (!read_command(command))
        {
            return;
        }
        if (command != expected.command)
        {
            (void) write_line("500 unexpected command");
            record_failure("expected NNTP command `" + expected.command + "`, got `" + command + "`");
            consume_expected_command();
            return;
        }
        for (const std::string &response_line : expected.response_lines)
        {
            if (!write_line(response_line))
            {
                return;
            }
        }
        if (expected.starttls && !negotiate_tls(expected.handshake_failure))
        {
            consume_expected_command();
            return;
        }
        consume_expected_command();
    }
}

bool MockNntpServer::stopping() const
{
    std::lock_guard guard{m_impl->mutex};
    return m_impl->stopping;
}
