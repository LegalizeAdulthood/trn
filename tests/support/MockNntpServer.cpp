// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include "support/MockNntpServer.h"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
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

bool has_line_endings(std::string_view line)
{
    return line.find_first_of("\r\n") != std::string_view::npos;
}

} // namespace

struct MockNntpServer::ExpectedCommand
{
    std::string              command;
    std::vector<std::string> response_lines;
};

struct MockNntpServer::Impl
{
    asio::io_context             context;
    tcp::acceptor                acceptor{context};
    std::unique_ptr<tcp::socket> socket;
    std::thread                  worker;
    mutable std::mutex           mutex;
    std::deque<ExpectedCommand>  expected_commands;
    std::string                  welcome_banner{DEFAULT_WELCOME_BANNER};
    std::vector<std::string> failures;
    int                      port{};
    bool                     stopping{};
};

MockNntpServer::MockNntpServer() :
    m_impl{std::make_unique<Impl>()}
{
}

MockNntpServer::~MockNntpServer()
{
    stop();
}

void MockNntpServer::expect_list(std::vector<std::string> lines)
{
    std::vector<std::string> response_lines;
    response_lines.reserve(lines.size() + 2);
    response_lines.emplace_back("215 list follows");
    for (std::string &line : lines)
    {
        response_lines.push_back(std::move(line));
    }
    response_lines.emplace_back(".");
    expect_command("LIST", std::move(response_lines));
}

void MockNntpServer::expect_mode_reader()
{
    expect_command("MODE READER", {"200 trn-test reader mode"});
}

void MockNntpServer::expect_quit()
{
    expect_command("QUIT", {"205 closing connection"});
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

    asio::write(*socket, asio::buffer(banner), ec);
    if (ec && !stopping())
    {
        record_failure("write failed: " + ec.message());
        return;
    }

    auto read_command = [&socket, this](std::string &command)
    {
        error_code  read_ec;
        std::string line;
        asio::read_until(*socket, asio::dynamic_buffer(line), "\r\n", read_ec);
        if (read_ec)
        {
            if (!stopping())
            {
                record_failure("read failed: " + read_ec.message());
            }
            return false;
        }
        if (line.size() >= 2 && line.compare(line.size() - 2, 2, "\r\n") == 0)
        {
            line.resize(line.size() - 2);
        }
        command = std::move(line);
        return true;
    };

    auto write_line = [&socket, this](std::string_view line)
    {
        if (has_line_endings(line))
        {
            record_failure("NNTP response line must not contain line endings");
            return false;
        }

        error_code  write_ec;
        std::string response{line};
        response += "\r\n";
        asio::write(*socket, asio::buffer(response), write_ec);
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
        consume_expected_command();
    }
}

bool MockNntpServer::stopping() const
{
    std::lock_guard guard{m_impl->mutex};
    return m_impl->stopping;
}
