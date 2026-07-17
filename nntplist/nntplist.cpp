/* nntplist.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <trn/string-algos.h>
#include <util/env.h>
#include <util/util2.h>
#include <wildmat/wildmat.h>

#include <fmt/format.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void usage();

static std::string s_server_name;
std::string g_nntp_auth_file;

int main(int argc, char *argv[])
{
    const char *action = nullptr;
    const char *wildarg = nullptr;
    std::FILE *out_fp{};

    while (--argc)
    {
        if (**++argv == '-')
        {
            switch (*++*argv)
            {
            case 'o':
                if (out_fp || !--argc)
                {
                    usage();
                }
                out_fp = std::fopen(*++argv, "w");
                if (out_fp == nullptr)
                {
                    std::perror(*argv);
                    std::exit(1);
                }
                break;

            case 'x':
                if (wildarg || !--argc)
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
        else if (!action)
        {
            action = *argv;
        }
        else
        {
            usage();
        }
    }
    if (action && string_case_equal(action, "active"))
    {
        action = nullptr;
    }
    if (!out_fp)
    {
        out_fp = stdout;
    }

    env_init(true);

    const char *server = std::getenv("NNTPSERVER");
    char       *cp;
    std::string server_name;
    if (server == nullptr)
    {
        server_name = file_exp(SERVER_NAME);
        if (!server_name.empty())
        {
            if (FILE_REF(server_name.c_str()))
            {
                server_name = nntp_server_name(server_name);
            }
            server = server_name.c_str();
        }
    }
    if (server != nullptr && std::strcmp(server, "local") != 0)
    {
        s_server_name = server;
        cp = std::strchr(s_server_name.data(), ';');
        if (!cp)
        {
            cp = std::strchr(s_server_name.data(), ':');
        }
        if (cp)
        {
            *cp = '\0';
            g_nntp_link.port_number = std::atoi(cp+1);
        }
        g_nntp_auth_file = file_exp(NNTP_AUTH_FILE);
        const char *force_auth = getenv("NNTP_FORCE_AUTH");
        if (force_auth != nullptr && (*force_auth == 'y' || *force_auth == 'Y'))
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
        if (action)
        {
            command += ' ';
            command += action;
        }
        if (wildarg)
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
            fmt::print(stderr, "nntplist: Can't get {} file from server.\n", action ? action : "active");
            fmt::print(stderr, "Server said: {}\n", g_ser_line);
            finalize(1);
        }
        while (nntp_gets(g_ser_line, sizeof g_ser_line) == 1)
        {
            if (nntp_at_list_end(g_ser_line))
            {
                break;
            }
            std::fputs(g_ser_line, out_fp);
            std::putc('\n', out_fp);
        }

#ifdef HAS_SIGHOLD
        sigrelse(SIGINT);
#endif
        nntp_close(true);
        cleanup_nntp();
    }
    else
    {
        const char *local_file = nullptr;
        if (!action)
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
        if (!local_file || !*local_file)
        {
            fmt::print(stderr, "Don't know how to list `{}' from your local system.\n", action);
            exit(1);
        }
        std::FILE *in_fp{std::fopen(file_exp(local_file).c_str(), "r")};
        if (in_fp == nullptr)
        {
            fmt::print(stderr, "Unable to open `{}'.\n", local_file);
            std::exit(1);
        }
        while (std::fgets(g_ser_line, sizeof g_ser_line, in_fp))
        {
            if (wildarg)
            {
                cp = skip_non_space(g_ser_line);
                if (!cp)
                {
                    continue;
                }
                *cp = '\0';
                if (!wildcard_match(g_ser_line, wildarg))
                {
                    continue;
                }
                *cp = ' ';
            }
            std::fputs(g_ser_line, out_fp);
        }
    }
    return 0;
}

static void usage()
{
    std::fprintf(stderr, "Usage: nntplist [-x WildSpec] [-o OutputFile] [type]\n"
                    "\n"
                    "Where type is any of the LIST command arguments your server accepts.\n");
    std::exit(1);
}

int nntp_handle_timeout()
{
    std::fputs("\n503 Server timed out.\n",stderr);
    return -2;
}
