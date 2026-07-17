/* inews.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/common.h>
#include <config/env.h>
#include <config/pipe_io.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <trn/string-algos.h>
#include <util/env.h>
#include <util/util2.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

enum
{
    MAX_SIGNATURE = 4
};

int new_connection{};
std::string g_server_name;
std::string g_nntp_auth_file;
char g_buf[LINE_BUF_LEN + 1]{}; // general purpose line buffer

int  valid_header(std::string_view h);
void append_signature();

static std::FILE *inews_wr_fp{};

static void inews_fputs(const char *buff)
{
    if (inews_wr_fp)
    {
        std::fputs(buff, inews_wr_fp);
    }
    else
    {
        boost::system::error_code ec;
        g_nntp_link.connection->write(buff, 0, ec);
    }
}

static void inews_fputc(char c)
{
    if (inews_wr_fp)
    {
        std::fputc(c, inews_wr_fp);
    }
    else
    {
        boost::system::error_code ec;
        g_nntp_link.connection->write(&c, 1, ec);
    }
}

static void inews_fflush()
{
    if (inews_wr_fp)
    {
        (void) std::fflush(inews_wr_fp);
    }
}

int main(int argc, char *argv[])
{
    bool has_fromline;
    bool in_header;
    bool has_pathline;
    bool found_nl;
    bool  had_nl;
    int   artpos;
    int   len;
    char *cp;
    int  i;

    int   headbuf_size = LINE_BUF_LEN * 8;
    char *headbuf = safe_malloc(headbuf_size);

#ifdef LAX_INEWS
    env_init(true);
#else
    if (!env_init(false))
    {
        fprintf(stderr,"Can't get %s information. Please contact your system adminstrator.\n",
                (!g_login_name.empty() || g_real_name.empty())? "user" : "host");
        exit(1);
    }
#endif

    argv++;
    while (argc > 1)
    {
        if (*argv[0] != '-')
        {
            break;
        }
        argv++;
        argc--;
    }
    if (argc > 1)
    {
        if (std::freopen(*argv, "r", stdin) == nullptr)
        {
            std::perror(*argv);
            std::exit(1);
        }
    }

    std::string server_name = get_env_var("NNTPSERVER");
    if (server_name.empty())
    {
        server_name = file_exp(SERVER_NAME);
        if (!server_name.empty() && FILE_REF(server_name.c_str()))
        {
            server_name = nntp_server_name(server_name);
        }
    }
    const char *line_end;
    if (!server_name.empty() && server_name != "local")
    {
        g_server_name = server_name;
        if (const auto separator = g_server_name.find(';'); separator != std::string::npos)
        {
            g_nntp_link.port_number = std::atoi(g_server_name.c_str() + separator + 1);
            g_server_name.resize(separator);
        }
        line_end = "\r\n";
        g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);
        const char *force_auth = std::getenv("NNTP_FORCE_AUTH");
        if (force_auth != nullptr && (*force_auth == 'y' || *force_auth == 'Y'))
        {
            g_nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
    }
    else
    {
        g_server_name.clear();
        line_end = "\n";
    }

    in_header = false;
    has_fromline = false;
    has_pathline = false;
    artpos = 0;
    cp = headbuf;
    had_nl = true;

    while (true)
    {
        if (headbuf_size < artpos + LINE_BUF_LEN + 1)
        {
            len = cp - headbuf;
            headbuf_size += LINE_BUF_LEN * 4;
            headbuf = safe_realloc(headbuf,headbuf_size);
            cp = headbuf + len;
        }
        i = std::getc(stdin);
        if (!g_server_name.empty() && had_nl && i == '.')
        {
            *cp++ = '.';
        }

        if (i == '\n')
        {
            if (!in_header)
            {
                continue;
            }
            break;
        }
        if (i == EOF || !std::fgets(cp + 1, LINE_BUF_LEN - 1, stdin))
        {
            // Still in header after EOF?  Hmm...
            std::fprintf(stderr,"Article was all header -- no body.\n");
            std::exit(1);
        }
        *cp = (char)i;
        len = std::strlen(cp);
        found_nl = (len && cp[len-1] == '\n');
        if (had_nl)
        {
            i = valid_header(cp);
            if (i == 0)
            {
                std::fprintf(stderr,"Invalid header:\n%s",cp);
                std::exit(1);
            }
            if (i == 2)
            {
                if (!in_header)
                {
                    continue;
                }
                break;
            }
            in_header = true;
            if (string_case_equal(cp, "From:", 5))
            {
                has_fromline = true;
            }
            else if (string_case_equal(cp, "Path:", 5))
            {
                has_pathline = true;
            }
        }
        artpos += len;
        cp += len;
        had_nl = found_nl;
        if (had_nl != 0 && !g_server_name.empty())
        {
            cp[-1] = '\r';
            *cp++ = '\n';
        }
    }
    *cp = '\0';

    // Well, the header looks ok, so let's get on with it.

    if (!g_server_name.empty())
    {
        if (!g_nntp_link.connection)
        {
            if (init_nntp() < 0 || !nntp_connect(g_server_name.c_str(),false))
            {
                std::exit(1);
            }
            new_connection = true;
        }
        if (nntp_command("POST") <= 0 || nntp_check() <= 0)
        {
            if (new_connection)
            {
                nntp_close(true);
            }
            std::fprintf(stderr,"Sorry, you can't post from this machine.\n");
            std::exit(1);
        }
    }
    else
    {
        std::sprintf(g_buf, "%s -h", EXTRA_INEWS);
        inews_wr_fp = popen(g_buf,"w");
        if (!inews_wr_fp)
        {
            std::fprintf(stderr,"Unable to execute inews for local posting.\n");
            std::exit(1);
        }
    }

    inews_fputs(headbuf);
    if (!has_pathline)
    {
        std::sprintf(g_buf,"Path: not-for-mail%s",line_end);
        inews_fputs(g_buf);
    }
    if (!has_fromline)
    {
        const char *real_name = get_val_const("NAME", g_real_name.c_str());
        std::sprintf(g_buf, "From: %s@%s (%s)%s", g_login_name.c_str(), g_p_host_name.c_str(), real_name, line_end);
        inews_fputs(g_buf);
    }
    if (!std::getenv("NO_ORIGINATOR"))
    {
        const char *real_name = get_val_const("NAME", g_real_name.c_str());
        std::sprintf(g_buf, "Originator: %s@%s (%s)%s", g_login_name.c_str(), g_p_host_name.c_str(), real_name,
                     line_end);
        inews_fputs(g_buf);
    }
    std::sprintf(g_buf, "%s", line_end);
    inews_fputs(g_buf);

    had_nl = true;
    while (std::fgets(headbuf, headbuf_size, stdin))
    {
        // Single . is eof, so put in extra one
        if (!g_server_name.empty() && had_nl && *headbuf == '.')
        {
            inews_fputc('.');
        }
        // check on newline
        cp = headbuf + std::strlen(headbuf);
        if (cp > headbuf && *--cp == '\n')
        {
            *cp = '\0';
            std::sprintf(g_buf, "%s%s", headbuf, line_end);
            inews_fputs(g_buf);
            had_nl = true;
        }
        else
        {
            inews_fputs(headbuf);
            had_nl = false;
        }
    }

    if (!inews_wr_fp)
    {
        return pclose(inews_wr_fp);
    }

    if (!had_nl)
    {
        inews_fputs(line_end);
    }

    append_signature();

    inews_fputs(".\r\n");
    inews_fflush();

    if (nntp_gets(g_ser_line, sizeof g_ser_line) < 0 //
        || *g_ser_line != NNTP_CLASS_OK)
    {
        if (std::atoi(g_ser_line) == NNTP_POSTFAIL_VAL)
        {
            std::fprintf(stderr,"Article not accepted by server; not posted:\n");
            for (cp = g_ser_line + 4; *cp && *cp != '\r'; cp++)
            {
                if (*cp == '\\')
                {
                    std::fputc('\n',stderr);
                }
                else
                {
                    std::fputc(*cp,stderr);
                }
            }
            std::fputc('\n', stderr);
        }
        else
        {
            std::fprintf(stderr, "Remote error: %s\n", g_ser_line);
        }
        if (new_connection)
        {
            nntp_close(true);
        }
        std::exit(1);
    }

    if (new_connection)
    {
        nntp_close(true);
    }
    cleanup_nntp();

    return 0;
}

// valid_header -- determine if a line is a valid header line
int valid_header(std::string_view h)
{
    // Blank or tab in first position implies this is a continuation header
    if (!h.empty() && is_hor_space(h.front()))
    {
        const std::size_t text = h.find_first_not_of(" \t");
        return text != std::string_view::npos && h[text] != '\n' ? 1 : 2;
    }

    // Just check for initial letter, colon, and space to make
    // sure we discard only invalid headers.
    const std::size_t colon = h.find(':');
    const std::size_t space = h.find(' ');
    if (!h.empty()                                             //
        && std::isalpha(static_cast<unsigned char>(h.front())) //
        && colon != std::string_view::npos && space == colon + 1)
    {
        return 1;
    }

    // Anything else is a bad header
    return 0;
}

// append_signature -- append the person's .signature file if
// they have one.  Limit .signature to MAX_SIGNATURE lines.
// The rn-style DOTDIR environmental variable is used if present.
//
void append_signature()
{
    char* cp;
    std::FILE* fp;
    int count = 0;

#ifdef NO_INEWS_DOTDIR
    g_dot_dir = g_home_dir;
#endif
    if (g_dot_dir.empty())
    {
        return;
    }

    fp = std::fopen(file_exp(SIGNATURE_FILE).c_str(), "r");
    if (fp == nullptr)
    {
        return;
    }

    std::sprintf(g_buf, "-- \r\n");
    inews_fputs(g_buf);
    while (std::fgets(g_ser_line, sizeof g_ser_line, fp))
    {
        count++;
        if (count > MAX_SIGNATURE)
        {
            std::fprintf(stderr,"Warning: .signature files should be no longer than %d lines.\n",
                    MAX_SIGNATURE);
            std::fprintf(stderr,"(Only %d lines of your .signature were posted.)\n",
                    MAX_SIGNATURE);
            break;
        }
        // Strip trailing newline
        cp = g_ser_line + std::strlen(g_ser_line) - 1;
        if (cp >= g_ser_line && *cp == '\n')
        {
            *cp = '\0';
        }
        std::sprintf(g_buf, "%s\r\n", g_ser_line);
        inews_fputs(g_buf);
    }
    (void) std::fclose(fp);
}

int nntp_handle_timeout()
{
    if (!new_connection)
    {
        static bool handling_timeout = false;
        const std::string last_command_save{g_last_command};

        if (string_case_equal(g_last_command.c_str(), "quit"))
        {
            return 0;
        }
        if (handling_timeout)
        {
            return -1;
        }
        handling_timeout = true;
        nntp_close(false);
        if (init_nntp() < 0 || nntp_connect(g_server_name.c_str(),false) <= 0)
        {
            std::exit(1);
        }
        if (nntp_command(last_command_save) <= 0)
        {
            return -1;
        }
        handling_timeout = false;
        new_connection = true;
        return 1;
    }
    std::fputs("\n503 Server timed out.\n",stderr);
    return -2;
}
