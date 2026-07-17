/* url.cpp
 *
 * Routines for handling WWW URL references.
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/url.h>

#include <config/common.h>
#include <nntp/nntpinit.h>
#include <trn/util.h>
#include <util/util2.h>

// Lower-level net routines grabbed from nntpinit.c.

#include <boost/asio.hpp>
#include <fmt/format.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace asio = boost::asio;
using resolver_results = asio::ip::tcp::resolver::results_type;
using error_code_t = boost::system::error_code;

struct UrlParts
{
    std::string type;
    std::string host;
    std::string path;
    int         port{80};
};

static bool fetch_http(const char *host, int port, const char *path, const char *outname);
static bool fetch_ftp(const char *host, const char *origpath, const char *outname);
static UrlParts parse_url(std::string_view url);

// returns true if successful
static bool fetch_http(const char *host, int port, const char *path, const char *outname)
{
    asio::io_context context;
    asio::ip::tcp::resolver s_resolver(context);
    error_code_t ec;
    std::string service{"http"};
    if (port)
    {
        service =std::to_string(port);
    }
    asio::ip::tcp::resolver::results_type results = s_resolver.resolve(host, service, ec);
    if (ec)
    {
        return false;
    }

    asio::ip::tcp::socket socket{context};
    asio::connect(socket, results, ec);
    if (ec)
    {
        return false;
    }

    // XXX later consider using HTTP/1.0 format (and user-agent)
    const std::string request = fmt::format("GET {}\r\n", path);
    asio::write(socket, asio::buffer(request), ec);
    if (ec)
    {
        return false;
    }

    std::FILE *fp_out = std::fopen(outname, "w");
    if (!fp_out)
    {
        std::printf("\nURL output file could not be opened.\n");
        return false;
    }
    std::array<char, 1030> read_buffer;
    while (true)
    {
        size_t len = read(socket, asio::buffer(read_buffer), ec);
        if (ec != asio::error::eof)
        {
            std::printf("\nError: reading URL reply\n");
            return false;
        }
        if (len == 0)
        {
            break;      // no data, end connection
        }
        std::fwrite(read_buffer.data(), 1, len, fp_out);
    }
    std::fclose(fp_out);
    return true;
}

// add port support later?
static bool fetch_ftp(const char *host, const char *origpath, const char *outname)
{
#ifdef USE_FTP
    static char cmdline[1024];
    static char path[512];      // use to make writable copy
    static char username[128];
    static char userhost[128];
    int         status;
    const char *cdpath;

    safe_copy(path,origpath,510);
    char *p = std::strrchr(path, '/'); // p points to last slash or nullptr
    if (p == nullptr)
    {
        std::printf("Error: URL:ftp path has no '/' character.\n");
        return false;
    }
    if (p[1] == '\0')
    {
        std::printf("Error: URL:ftp path has no final filename.\n");
        return false;
    }
    safe_copy(username, file_exp("%L").c_str(), 120);
    safe_copy(userhost, file_exp("%H").c_str(), 120);
    if (p != path) // not of form /foo
    {
        *p = '\0';
        cdpath = path;
    }
    else
    {
        cdpath = "/";
    }

    std::sprintf(cmdline, "%s/ftpgrab %s ftp %s@%s %s %s %s", file_exp("%X").c_str(), host, username, userhost, cdpath,
                 p + 1, outname);

    // modified escape_shell_cmd code from NCSA HTTPD util.cpp
    // serious security holes could result without this code
    int l = std::strlen(cmdline);
    for (int x = 0; cmdline[x]; x++)
    {
        if (std::strchr("&;`'\"|*?~<>^()[]{}$\\", cmdline[x]))
        {
            for (int y = l + 1; y > x; y--)
            {
                cmdline[y] = cmdline[y-1];
            }
            l++; // length has been increased
            cmdline[x] = '\\';
            x++; // skip the character
        }
    }

// Debug
#if 0
    std::printf("ftpgrab command:\n|%s|\n",cmdline);
#endif

    *p = '/';
    status = do_shell(nullptr,cmdline);
    return true;
#else
    std::printf("\nThis copy of trn does not have URL:ftp support.\n");
    return false;
#endif
}

// right now only full, absolute URLs are allowed.
// use relative URLs later?
// later: pay more attention to long URLs
static UrlParts parse_url(std::string_view url)
{
    if (url.empty())
    {
        fmt::print("Empty URL -- ignoring.\n");
        return {};
    }

    const std::string_view full_url = url;
    const std::size_t      scheme_end = full_url.find(':');
    if (scheme_end == std::string_view::npos)
    {
        fmt::print("Incomplete URL: {}\n", url);
        return {};
    }
    const std::string_view url_type = full_url.substr(0, scheme_end);
    std::string_view       rest = full_url.substr(scheme_end + 1);
    std::string_view       url_host;
    UrlParts               parts;

    if (rest.substr(0, 2) == "//")
    {
        // normal URL type, will have host (optional portnum)
        rest.remove_prefix(2);

        // check for address literal: news://[ip:v6:address]:port/
        if (!rest.empty() && rest.front() == '[')
        {
            const std::size_t host_end = rest.find(']');
            if (host_end == std::string_view::npos)
            {
                fmt::print("Bad address literal: {}\n", url);
                return {};
            }
            url_host = rest.substr(0, host_end);
            rest.remove_prefix(host_end + 1);
        }
        else
        {
            const std::size_t host_end = rest.find_first_of("/:");
            url_host = rest.substr(0, host_end);
            rest.remove_prefix(host_end == std::string_view::npos ? rest.size() : host_end);
        }
        if (rest.empty())
        {
            fmt::print("Incomplete URL: {}\n", url);
            return {};
        }
        if (rest.front() == ':')
        {
            rest.remove_prefix(1);
            if (rest.empty() || !std::isdigit(static_cast<unsigned char>(rest.front())))
            {
                fmt::print("Bad URL (non-numeric portnum): {}\n", url);
                return {};
            }
            std::size_t port_len = 0;
            while (port_len < rest.size() && std::isdigit(static_cast<unsigned char>(rest[port_len])))
            {
                port_len++;
            }
            const std::string port{rest.substr(0, port_len)};
            parts.port = std::atoi(port.c_str());
            rest.remove_prefix(port_len);
        }
    }
    else
    {
        if (url_type != "news")
        {
            fmt::print("URL needs a hostname: {}\n", url);
            return {};
        }
    }
    // finally, just do the path
    if (rest.empty() || rest.front() != '/')
    {
        fmt::print("Bad URL (path does not start with /): {}\n", url);
        return {};
    }
    parts.type = url_type;
    parts.host = url_host;
    parts.path = rest;
    return parts;
}

bool url_get(std::string_view url, const char *outfile)
{
    const UrlParts parts = parse_url(url);
    if (parts.path.empty())
    {
        return false;
    }

    if (parts.type == "http")
    {
        return fetch_http(parts.host.c_str(), parts.port, parts.path.c_str(), outfile);
    }
    if (parts.type == "ftp")
    {
        return fetch_ftp(parts.host.c_str(), parts.path.c_str(), outfile);
    }
    fmt::print("\nURL type {} not supported (yet?)\n", parts.type);
    return false;
}
