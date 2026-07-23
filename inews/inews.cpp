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

#include <fmt/format.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

constexpr int MAX_SIGNATURE{4};

int new_connection{};
std::string g_server_name;
std::string g_nntp_auth_file;

int  valid_header(std::string_view h);
void append_signature();

static std::FILE *inews_wr_fp{};

static void inews_fputs(std::string_view buff)
{
    if (inews_wr_fp)
    {
        std::fwrite(buff.data(), 1, buff.size(), inews_wr_fp);
    }
    else
    {
        boost::system::error_code ec;
        g_nntp_link.connection->write(buff, ec);
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
        g_nntp_link.connection->write(std::string_view{&c, 1}, ec);
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
    bool        has_pathline;
    bool        found_nl;
    bool        had_nl;
    char       *cp;
    int         i;
    std::string article_header;
    std::string input_line;
    std::string body_line;
    std::string output_line;

    article_header.reserve(LINE_BUF_LEN * 8);
    input_line.reserve(LINE_BUF_LEN);
    body_line.reserve(LINE_BUF_LEN * 8);
    output_line.reserve(LINE_BUF_LEN);

    env_init(true);

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
        if (!server_name.empty() && file_ref(server_name))
        {
            server_name = nntp_server_name(server_name);
        }
    }
    std::string_view line_end;
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
        const std::string force_auth = get_env_var("NNTP_FORCE_AUTH");
        if (!force_auth.empty() && (force_auth.front() == 'y' || force_auth.front() == 'Y'))
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
    had_nl = true;

    while (true)
    {
        i = std::getc(stdin);
        if (!g_server_name.empty() && had_nl && i == '.')
        {
            article_header += '.';
        }

        if (i == '\n')
        {
            if (!in_header)
            {
                continue;
            }
            break;
        }
        if (i == EOF)
        {
            // Still in header after EOF?  Hmm...
            fmt::print(stderr, "Article was all header -- no body.\n");
            std::exit(1);
        }
        input_line.clear();
        input_line += static_cast<char>(i);
        while ((i = std::getc(stdin)) != EOF)
        {
            input_line += static_cast<char>(i);
            if (i == '\n')
            {
                break;
            }
        }
        found_nl = !input_line.empty() && input_line.back() == '\n';
        if (had_nl)
        {
            i = valid_header(input_line);
            if (i == 0)
            {
                fmt::print(stderr, "Invalid header:\n{}", input_line);
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
            if (string_case_equal(std::string_view{input_line}.substr(0, 5), "From:"))
            {
                has_fromline = true;
            }
            else if (string_case_equal(std::string_view{input_line}.substr(0, 5), "Path:"))
            {
                has_pathline = true;
            }
        }
        had_nl = found_nl;
        if (had_nl != 0 && !g_server_name.empty())
        {
            input_line.back() = '\r';
            input_line += '\n';
        }
        article_header += input_line;
    }

    // Well, the header looks ok, so let's get on with it.

#ifndef LAX_INEWS
    if (!env_init(false))
    {
        fmt::print(stderr, "Can't get {} information. Please contact your system adminstrator.\n",
                   (!g_login_name.empty() || g_real_name.empty()) ? "user" : "host");
        exit(1);
    }
#endif

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
            fmt::print(stderr, "Sorry, you can't post from this machine.\n");
            std::exit(1);
        }
    }
    else
    {
        output_line = fmt::format("{} -h", EXTRA_INEWS);
        inews_wr_fp = popen(output_line.c_str(),"w");
        if (!inews_wr_fp)
        {
            fmt::print(stderr, "Unable to execute inews for local posting.\n");
            std::exit(1);
        }
    }

    inews_fputs(article_header);
    if (!has_pathline)
    {
        output_line = fmt::format("Path: not-for-mail{}", line_end);
        inews_fputs(output_line);
    }
    if (!has_fromline)
    {
        const std::string real_name = get_env_var("NAME", g_real_name);
        output_line = fmt::format("From: {}@{} ({}){}", g_login_name, g_p_host_name, real_name, line_end);
        inews_fputs(output_line);
    }
    if (get_env_var("NO_ORIGINATOR").empty())
    {
        const std::string real_name = get_env_var("NAME", g_real_name);
        output_line = fmt::format("Originator: {}@{} ({}){}", g_login_name, g_p_host_name, real_name, line_end);
        inews_fputs(output_line);
    }
    inews_fputs(line_end);

    had_nl = true;
    while (true)
    {
        body_line.clear();
        while ((i = std::getc(stdin)) != EOF)
        {
            body_line += static_cast<char>(i);
            if (i == '\n')
            {
                break;
            }
        }
        if (body_line.empty())
        {
            break;
        }
        // Single . is eof, so put in extra one
        if (!g_server_name.empty() && had_nl && body_line.front() == '.')
        {
            inews_fputc('.');
        }
        // check on newline
        found_nl = body_line.back() == '\n';
        if (found_nl)
        {
            body_line.pop_back();
            body_line += line_end;
            inews_fputs(body_line);
            had_nl = true;
        }
        else
        {
            inews_fputs(body_line);
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
            fmt::print(stderr, "Article not accepted by server; not posted:\n");
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
            fmt::print(stderr, "Remote error: {}\n", g_ser_line);
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
    int         count = 0;
    std::string line;

#ifdef NO_INEWS_DOTDIR
    g_dot_dir = g_home_dir;
#endif
    if (g_dot_dir.empty())
    {
        return;
    }

    std::ifstream signature{file_exp(SIGNATURE_FILE)};
    if (!signature)
    {
        return;
    }

    line.reserve(NNTP_STRLEN);
    inews_fputs("-- \r\n");
    while (std::getline(signature, line))
    {
        count++;
        if (count > MAX_SIGNATURE)
        {
            fmt::print(stderr, "Warning: .signature files should be no longer than {} lines.\n", MAX_SIGNATURE);
            fmt::print(stderr, "(Only {} lines of your .signature were posted.)\n", MAX_SIGNATURE);
            break;
        }
        line += "\r\n";
        inews_fputs(line);
    }
}

int nntp_handle_timeout()
{
    if (!new_connection)
    {
        static bool handling_timeout = false;
        const std::string last_command_save{g_last_command};

        if (string_case_equal(g_last_command, "quit"))
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
    fmt::print(stderr, "\n503 Server timed out.\n");
    return -2;
}
