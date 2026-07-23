/* nntplist.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/common.h>
#include <config/env.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <util/env.h>
#include <util/util2.h>
#include <wildmat/wildmat.h>

#include <fmt/format.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

static void usage();

static std::string s_server_name;
std::string g_nntp_auth_file;

int main(int argc, char *argv[])
{
    std::string_view action;
    std::string_view wildarg;
    std::FILE *out_fp{};

    while (--argc)
    {
        std::string_view arg{*++argv};
        if (!arg.empty() && arg.front() == '-')
        {
            switch (arg.size() > 1 ? arg[1] : '\0')
            {
            case 'o':
            {
                if (out_fp || !--argc)
                {
                    usage();
                }
                const char *output_file = *++argv;
                out_fp = std::fopen(output_file, "w");
                if (out_fp == nullptr)
                {
                    std::perror(output_file);
                    std::exit(1);
                }
                break;
            }

            case 'x':
                if (!wildarg.empty() || !--argc)
                {
                    usage();
                }
                wildarg = *++argv;
                break;

            default:
                usage();
                // NO RETURN
            }
        }
        else if (action.empty())
        {
            action = arg;
        }
        else
        {
            usage();
        }
    }
    if (string_case_equal(action, "active"))
    {
        action = {};
    }
    if (!out_fp)
    {
        out_fp = stdout;
    }

    env_init(true);

    std::string server_name = get_env_var("NNTPSERVER");
    if (server_name.empty())
    {
        server_name = file_exp(SERVER_NAME);
        if (!server_name.empty() && file_ref(server_name))
        {
            server_name = nntp_server_name(server_name);
        }
    }
    if (!server_name.empty() && server_name != "local")
    {
        s_server_name = server_name;
        const std::string::size_type separator = s_server_name.find_first_of(";:");
        if (separator != std::string::npos)
        {
            g_nntp_link.port_number = std::atoi(s_server_name.c_str() + separator + 1);
            s_server_name.resize(separator);
        }
        g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);
        const std::string force_auth = get_env_var("NNTP_FORCE_AUTH");
        if (!force_auth.empty() && (force_auth.front() == 'y' || force_auth.front() == 'Y'))
        {
            g_nntp_link.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
    }

    if (!s_server_name.empty())
    {
        if (init_nntp() < 0 //
            || nntp_connect(s_server_name.c_str(), false) <= 0)
        {
            std::exit(1);
        }
        std::string command{"LIST"};
        if (!action.empty())
        {
            command += ' ';
            command += action;
        }
        if (!wildarg.empty())
        {
            command += ' ';
            command += wildarg;
        }
        if (nntp_command(command) <= 0)
        {
            std::exit(1);
        }
#ifdef HAS_SIGHOLD
        sighold(SIGINT);
#endif
        if (nntp_check() <= 0)
        {
            fmt::print(stderr, "nntplist: Can't get {} file from server.\n",
                       action.empty() ? std::string_view{"active"} : action);
            fmt::print(stderr, "Server said: {}\n", g_ser_line);
            finalize(1);
        }
        std::string line;
        line.reserve(NNTP_STRLEN);
        while (nntp_gets(line, NNTP_STRLEN) == NGSR_FULL_LINE)
        {
            if (nntp_at_list_end(line))
            {
                break;
            }
            fmt::print(out_fp, "{}\n", line);
        }

#ifdef HAS_SIGHOLD
        sigrelse(SIGINT);
#endif
        nntp_close(true);
        cleanup_nntp();
    }
    else
    {
        std::string_view local_file;
        if (action.empty())
        {
            local_file = ACTIVE;
        }
        else if (string_case_equal(action, "active.times"))
        {
            local_file = ACTIVE_TIMES;
        }
        else if (string_case_equal(action, "newsgroups"))
        {
            local_file = GROUP_DESC;
        }
        else if (string_case_equal(action, "subscriptions"))
        {
            local_file = SUBSCRIPTIONS;
        }
        else if (string_case_equal(action, "overview.fmt"))
        {
            local_file = OVERVIEW_FMT;
        }
        if (local_file.empty())
        {
            fmt::print(stderr, "Don't know how to list `{}' from your local system.\n", action);
            exit(1);
        }
        std::ifstream input{file_exp(local_file)};
        if (!input)
        {
            fmt::print(stderr, "Unable to open `{}'.\n", local_file);
            std::exit(1);
        }
        std::string line;
        while (std::getline(input, line))
        {
            const bool had_newline = !input.eof();
            if (!wildarg.empty())
            {
                const std::string_view line_view{line};
                if (!wildcard_match(line_view.substr(0, line_view.find_first_of(" \f\n\r\t\v")), wildarg))
                {
                    continue;
                }
            }
            fmt::print(out_fp, "{}", line);
            if (had_newline)
            {
                fmt::print(out_fp, "\n");
            }
        }
    }
    return 0;
}

static void usage()
{
    fmt::print(stderr, "Usage: nntplist [-x WildSpec] [-o OutputFile] [type]\n"
                       "\n"
                       "Where type is any of the LIST command arguments your server accepts.\n");
    std::exit(1);
}

int nntp_handle_timeout()
{
    fmt::print(stderr, "\n503 Server timed out.\n");
    return -2;
}
