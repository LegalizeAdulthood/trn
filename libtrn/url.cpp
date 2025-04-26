// This file Copyright 1993 by Clifford A. Adams
/* url.c
 *
 * Routines for handling WWW URL references.
 */

#include "config/common.h"
#include "trn/url.h"

#include "nntp/nntpinit.h"
#include "trn/util.h"
#include "util/util2.h"

// Lower-level net routines grabbed from nntpinit.c.

#include <boost/asio.hpp>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace asio = boost::asio;
using resolver_results = asio::ip::tcp::resolver::results_type;
using error_code_t = boost::system::error_code;

static char s_url_buf[1030];
// XXX just a little bit larger than necessary...
static char s_url_type[256];
static char s_url_host[256];
static int  s_url_port;
static char s_url_path[1024];

// returns true if successful
bool fetch_http(const char *host, int port, const char *path, const char *outname)
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

    // XXX length check
    // XXX later consider using HTTP/1.0 format (and user-agent)
    std::sprintf(s_url_buf, "GET %s\r\n", path);
    asio::write(socket, asio::buffer(s_url_buf, std::strlen(s_url_buf)), ec);
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
    while (true)
    {
        size_t len = read(socket, asio::buffer(s_url_buf, sizeof(s_url_buf)), ec);
        if (ec != asio::error::eof)
        {
            std::printf("\nError: reading URL reply\n");
            return false;
        }
        if (len == 0)
        {
            break;      // no data, end connection
        }
        std::fwrite(s_url_buf,1,len,fp_out);
    }
    std::fclose(fp_out);
    return true;
}

// add port support later?
bool fetch_ftp(const char *host, const char *origpath, const char *outname)
{
#ifdef USE_FTP
    static char cmdline[1024];
    static char path[512];      // use to make writable copy
    // buffers used because because file_exp overwrites previous call results
    static char username[128];
    static char userhost[128];
    int         status;
    char*       cdpath;

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
    safe_copy(username,file_exp("%L"),120);
    safe_copy(userhost,file_exp("%H"),120);
    if (p != path)      // not of form /foo
    {
        *p = '\0';
        cdpath = path;
    }
    else
    {
        cdpath = "/";
    }

    std::sprintf(cmdline,"%s/ftpgrab %s ftp %s@%s %s %s %s",
            file_exp("%X"),host,username,userhost,cdpath,p+1,outname);

    // modified escape_shell_cmd code from NCSA HTTPD util.c
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

#if 0
    std::printf("ftpgrab command:\n|%s|\n",cmdline);
#endif

    *p = '/';
    status = do_shell(nullptr,cmdline);
#if 0
    std::printf("\nFTP command status is %d\n",status);
    while (!input_pending())
    {
    }
    eat_typeahead();
#endif
    return true;
#else
    std::printf("\nThis copy of trn does not have URL:ftp support.\n");
    return false;
#endif
}

// right now only full, absolute URLs are allowed.
// use relative URLs later?
// later: pay more attention to long URLs
bool parse_url(const char *url)
{
    const char* s;

    // consider using 0 as default to look up the service?
    s_url_port = 80;    // the default
    if (!url || !*url)
    {
        std::printf("Empty URL -- ignoring.\n");
        return false;
    }
    char *p = s_url_type;
    for (s = url; *s && *s != ':'; *p++ = *s++)
    {
    }
    *p = '\0';
    if (!*s)
    {
        std::printf("Incomplete URL: %s\n",url);
        return false;
    }
    s++;
    if (!std::strncmp(s, "//", 2))
    {
        // normal URL type, will have host (optional portnum)
        s += 2;
        p = s_url_host;
        // check for address literal: news://[ip:v6:address]:port/
        if (*s == '[')
        {
            while (*s && *s != ']')
            {
                *p++ = *s++;
            }
            if (!*s)
            {
                std::printf("Bad address literal: %s\n",url);
                return false;
            }
            s++;        // skip ]
        }
        else
        {
            while (*s && *s != '/' && *s != ':')
            {
                *p++ = *s++;
            }
        }
        *p = '\0';
        if (!*s)
        {
            std::printf("Incomplete URL: %s\n",url);
            return false;
        }
        if (*s == ':')
        {
            s++;
            p = s_url_buf;      // temp space
            if (!std::isdigit(*s))
            {
                std::printf("Bad URL (non-numeric portnum): %s\n",url);
                return false;
            }
            while (std::isdigit(*s))
            {
                *p++ = *s++;
            }
            *p = '\0';
            s_url_port = std::atoi(s_url_buf);
        }
    }
    else
    {
        if (!!std::strcmp(s_url_type, "news"))
        {
            std::printf("URL needs a hostname: %s\n",url);
            return false;
        }
    }
    // finally, just do the path
    if (*s != '/')
    {
        std::printf("Bad URL (path does not start with /): %s\n",url);
        return false;
    }
    std::strcpy(s_url_path,s);
    return true;
}

bool url_get(const char *url, const char *outfile)
{
    bool flag;

    if (!parse_url(url))
    {
        return false;
    }

    if (!std::strcmp(s_url_type,"http"))
    {
        flag = fetch_http(s_url_host,s_url_port,s_url_path,outfile);
    }
    else if (!std::strcmp(s_url_type,"ftp"))
    {
        flag = fetch_ftp(s_url_host,s_url_path,outfile);
    }
    else
    {
        if (s_url_type)
        {
            std::printf("\nURL type %s not supported (yet?)\n",s_url_type);
        }
        flag = false;
    }
    return flag;
}
