/* nntpinit.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include <fdio.h>

#include "common.h"
#include "nntpinit.h"

#include "nntpclient.h"

#include <boost/asio.hpp>

#include <iostream>
#include <map>
#include <string>
#include <utility>

namespace asio = boost::asio;

static asio::io_context        s_context;
static asio::ip::tcp::resolver s_resolver(s_context);
static ConnectionFactory       s_nntp_connection_factory;

using resolver_results = asio::ip::tcp::resolver::results_type;
using error_code = boost::system::error_code;

class NNTPConnection : public INNTPConnection
{
public:
    NNTPConnection(const char *server, const resolver_results &results)
        : m_server(server)
    {
        error_code ec;
        asio::connect(m_socket, results, ec);
        if (ec)
        {
            throw std::runtime_error("Couldn't connect socket" + ec.what());
        }
    }
    ~NNTPConnection() override = default;

    std::string read_line(error_code &ec) override;
    void        write_line(const std::string &line, error_code &ec) override;
    void        write(const char *buffer, size_t len, error_code &ec) override;
    size_t      read(char *buf, size_t size, error_code &ec) override;

private:
    std::string           m_server;
    asio::ip::tcp::socket m_socket{s_context};
    asio::streambuf       m_buffer;
};

std::string NNTPConnection::read_line(error_code &ec)
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

void NNTPConnection::write_line(const std::string &line, error_code &ec)
{
    const std::string buffer{line + "\r\n"};
    write(buffer.c_str(), buffer.size(), ec);
}

void NNTPConnection::write(const char *buffer, size_t len, error_code &ec)
{
    asio::write(m_socket, asio::buffer(buffer, len), ec);
}

size_t NNTPConnection::read(char *buf, size_t size, error_code &ec)
{
    return asio::read(m_socket, asio::buffer(buf, size), ec);
}

static ConnectionPtr create_nntp_connection(const char *machine, int port, const char *service)
{
    std::string service_name{service};
    if (g_nntplink.port_number)
    {
        service_name = std::to_string(g_nntplink.port_number);
    }

    error_code ec;
    asio::ip::tcp::resolver::results_type results = s_resolver.resolve(machine, service_name, ec);
    if (ec)
    {
        return nullptr;
    }

    ConnectionPtr connection{};
    try
    {
        connection = std::make_shared<NNTPConnection>(machine, results);
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

int server_init(const char *machine)
{
    g_nntplink.connection = s_nntp_connection_factory(machine, g_nntplink.port_number, "nntp");
    if (g_nntplink.connection == nullptr)
    {
        return -1;
    }

    /* Now get the server's signon message */
    nntp_check();

    if (*g_ser_line == NNTP_CLASS_OK) {
        char save_line[NNTP_STRLEN];
        strcpy(save_line, g_ser_line);
        /* Try MODE READER just in case we're talking to innd.
        ** If it is not an invalid command, use the new reply. */
        if (nntp_command("MODE READER") <= 0)
        {
            sprintf(g_ser_line, "%d failed to send MODE READER\n", NNTP_ACCESS_VAL);
        }
        else if (nntp_check() <= 0 && atoi(g_ser_line) == NNTP_BAD_COMMAND_VAL)
        {
            strcpy(g_ser_line, save_line);
        }
    }
    return atoi(g_ser_line);
}

void cleanup_nntp()
{
}

int get_tcp_socket(const char *machine, int port, const char *service)
{
    int s;
#if INET6
    struct addrinfo hints;
    struct addrinfo* res;
    struct addrinfo* res0;
    char portstr[8];
    char* cause = nullptr;
    int error;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    if (port)
        sprintf(service = portstr, "%d", port);
    error = getaddrinfo(machine, service, &hints, &res0);
    if (error) {
        fprintf(stderr, "%s", gai_strerror(error));
        return -1;
    }
    for (res = res0; res; res = res->ai_next) {
        char buf[64] = "";
        s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s < 0) {
            cause = "socket";
            continue;
        }

        inet_ntop(res->ai_family, res->ai_addr, buf, sizeof buf);
        if (res != res0)
            fprintf(stderr, "trying %s...", buf);

        if (connect(s, res->ai_addr, res->ai_addrlen) >= 0)
            break;  /* okay we got one */

        fprintf(stderr, "connection to %s: ", buf);
        perror("");
        cause = "connect";
        close(s);
        s = -1;
    }
    if (s < 0) {
        fprintf(stderr, "giving up... ");
        perror(cause);
    }
    freeaddrinfo(res0);
#else   /* !INET6 */
    struct sockaddr_in sin;
#ifdef __hpux
    int socksize = 0;
    int socksizelen = sizeof socksize;
#endif
    struct hostent* hp;
#ifdef h_addr
    int         x = 0;
    static char*alist[1];
#endif /* h_addr */
    static struct hostent def;
    static struct in_addr defaddr;
    static char namebuf[256];

    memset((char*)&sin,0,sizeof sin);

    if (port)
    {
        sin.sin_port = htons(port);
    }
    else {
        struct servent *sp = getservbyname(service, "tcp");
        if (sp == nullptr)
        {
            fprintf(stderr, "%s/tcp: Unknown service.\n", service);
            return -1;
        }
        sin.sin_port = sp->s_port;
    }
    /* If not a raw ip address, try nameserver */
    if (!isdigit(*machine)
#ifdef INADDR_NONE
     || (defaddr.s_addr = inet_addr(machine)) == INADDR_NONE)
#else
     || (long)(defaddr.s_addr = inet_addr(machine)) == -1)
#endif
    {
        hp = gethostbyname(machine);
    }
    else {
        /* Raw ip address, fake  */
        (void) strcpy(namebuf, machine);
        def.h_name = namebuf;
#ifdef h_addr
        def.h_addr_list = alist;
#endif
        def.h_addr = (char*)&defaddr;
        def.h_length = sizeof (struct in_addr);
        def.h_addrtype = AF_INET;
        def.h_aliases = nullptr;
        hp = &def;
    }
    if (hp == nullptr) {
        fprintf(stderr, "%s: Unknown host.\n", machine);
        return -1;
    }

    sin.sin_family = hp->h_addrtype;

    /* get a socket and initiate connection -- use multiple addresses */
    for (char **cp = hp->h_addr_list; cp && *cp; cp++) {
        s = socket(hp->h_addrtype, SOCK_STREAM, 0);
        if (s < 0) {
            perror("socket");
            return -1;
        }
        memcpy((char*)&sin.sin_addr,*cp,hp->h_length);

        if (x < 0)
        {
            fprintf(stderr, "trying %s\n", inet_ntoa(sin.sin_addr));
        }
        x = connect(s, (struct sockaddr*)&sin, sizeof (sin));
        if (x == 0)
        {
            break;
        }
        fprintf(stderr, "connection to %s: ", inet_ntoa(sin.sin_addr));
        perror("");
        (void) close(s);
    }
    if (x < 0) {
        fprintf(stderr, "giving up...\n");
        return -1;
    }
#endif /* !INET6 */
#ifdef __hpux   /* recommended by raj@cup.hp.com */
#define HPSOCKSIZE 0x8000
    getsockopt(s, SOL_SOCKET, SO_SNDBUF, (caddr_t)&socksize, (caddr_t)&socksizelen);
    if (socksize < HPSOCKSIZE) {
        socksize = HPSOCKSIZE;
        setsockopt(s, SOL_SOCKET, SO_SNDBUF, (caddr_t)&socksize, sizeof (socksize));
    }
    socksize = 0;
    socksizelen = sizeof (socksize);
    getsockopt(s, SOL_SOCKET, SO_RCVBUF, (caddr_t)&socksize, (caddr_t)&socksizelen);
    if (socksize < HPSOCKSIZE) {
        socksize = HPSOCKSIZE;
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, (caddr_t)&socksize, sizeof (socksize));
    }
#endif
    return s;
}
