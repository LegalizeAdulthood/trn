/* nntpinit.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <nntp/nntpinit.h>

#include <config/common.h>
#include <config/fdio.h>
#include <nntp/nntpclient.h>

#include <fmt/format.h>

#include <boost/asio.hpp>

#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace asio = boost::asio;

static asio::io_context        s_context;
static asio::ip::tcp::resolver s_resolver(s_context);
static ConnectionFactory       s_nntp_connection_factory;

using resolver_results = asio::ip::tcp::resolver::results_type;
using error_code_t = boost::system::error_code;

class NNTPConnection : public INNTPConnection
{
public:
    NNTPConnection(std::string_view server, const resolver_results &results)
        : m_server(server)
    {
        error_code_t ec;
        asio::connect(m_socket, results, ec);
        if (ec)
        {
            throw std::runtime_error("Couldn't connect socket" + ec.what());
        }
    }
    ~NNTPConnection() override = default;

    std::string read_line(error_code_t &ec) override;
    void        write_line(const std::string &line, error_code_t &ec) override;
    void        write(std::string_view buffer, error_code_t &ec) override;
    size_t      read(char *buf, size_t size, error_code_t &ec) override;

private:
    std::string           m_server;
    asio::ip::tcp::socket m_socket{s_context};
    asio::streambuf       m_buffer;
};

std::string NNTPConnection::read_line(error_code_t &ec)
{
    read_until(m_socket, m_buffer, "\r\n", ec);
    if (ec)
    {
        return {};
    }

    std::string line;
    std::istream istr(&m_buffer);
    std::getline(istr, line);
    line += '\n';
    return line;
}

void NNTPConnection::write_line(const std::string &line, error_code_t &ec)
{
    const std::string buffer{line + "\r\n"};
    write(buffer, ec);
}

void NNTPConnection::write(std::string_view buffer, error_code_t &ec)
{
    asio::write(m_socket, asio::buffer(buffer.data(), buffer.size()), ec);
}

size_t NNTPConnection::read(char *buf, size_t size, error_code_t &ec)
{
    return asio::read(m_socket, asio::buffer(buf, size), ec);
}

static ConnectionPtr create_nntp_connection(std::string_view machine, int port, std::string_view service)
{
    std::string server_name{machine};
    std::string service_name{service};
    if (port)
    {
        service_name = std::to_string(port);
    }

    error_code_t                          ec;
    asio::ip::tcp::resolver::results_type results = s_resolver.resolve(server_name, service_name, ec);
    if (ec)
    {
        return nullptr;
    }

    ConnectionPtr connection{};
    try
    {
        connection = std::make_shared<NNTPConnection>(server_name, results);
    }
    catch (...)
    {
        return nullptr;
    }
    return connection;
}

void set_nntp_connection_factory(ConnectionFactory factory)
{
    s_nntp_connection_factory = std::move(factory);
}

int init_nntp()
{
    set_nntp_connection_factory(create_nntp_connection);
    return 1;
}

int server_init(std::string_view machine)
{
    g_nntp_link.connection = s_nntp_connection_factory(machine, g_nntp_link.port_number, "nntp");
    if (g_nntp_link.connection == nullptr)
    {
        return -1;
    }

    // Now get the server's signon message
    nntp_check();

    if (!g_ser_line.empty() && g_ser_line.front() == NNTP_CLASS_OK)
    {
        const std::string save_line{g_ser_line};
        // Try MODE READER just in case we're talking to innd.
        // If it is not an invalid command, use the new reply.
        if (nntp_command("MODE READER") <= 0)
        {
            g_ser_line = fmt::format("{} failed to send MODE READER\n", static_cast<int>(NNTP_ACCESS_VAL));
        }
        else if (nntp_check() <= 0 && nntp_response_code(g_ser_line) == NNTP_BAD_COMMAND_VAL)
        {
            g_ser_line = save_line;
        }
    }
    return nntp_response_code(g_ser_line);
}

void cleanup_nntp()
{
}
