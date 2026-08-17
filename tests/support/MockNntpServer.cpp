// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include "support/MockNntpServer.h"

#include <gtest/gtest.h>

#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>

#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace
{

namespace asio = boost::asio;
using boost::system::error_code;
using tcp = asio::ip::tcp;

constexpr const char *DEFAULT_WELCOME_BANNER = "200 trn-test NNTP server ready";

} // namespace

struct MockNntpServer::Impl
{
    asio::io_context         context;
    tcp::acceptor            acceptor{context};
    std::thread              worker;
    mutable std::mutex       mutex;
    std::string              welcome_banner{DEFAULT_WELCOME_BANNER};
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
    {
        std::lock_guard guard{m_impl->mutex};
        m_impl->stopping = true;
    }

    error_code ec;
    m_impl->acceptor.close(ec);
    if (m_impl->worker.joinable())
    {
        m_impl->worker.join();
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
    for (const std::string &failure : failures)
    {
        ADD_FAILURE() << failure;
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

    error_code  ec;
    tcp::socket socket{m_impl->context};
    m_impl->acceptor.accept(socket, ec);
    if (ec)
    {
        if (!stopping())
        {
            record_failure("accept failed: " + ec.message());
        }
        return;
    }

    asio::write(socket, asio::buffer(banner), ec);
    if (ec && !stopping())
    {
        record_failure("write failed: " + ec.message());
    }
}

bool MockNntpServer::stopping() const
{
    std::lock_guard guard{m_impl->mutex};
    return m_impl->stopping;
}
