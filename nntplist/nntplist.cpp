/* nntplist.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <tool/util3.h>
#include <trn/string-algos.h>
#include <util/env.h>
#include <util/util2.h>
#include <util/wildmat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void usage();

static char *s_server_name{};
std::string g_nntp_auth_file;
int debug{};             /* make nntpclient happy */

int main(int argc, char *argv[])
{
    char command[32];
    char*action = nullptr;
    char*wildarg = nullptr;
    std::FILE *out_fp{};

    while (--argc) {
        if (**++argv == '-') {
            switch (*++*argv) {
            case 'o':
                if (out_fp || !--argc)
                {
                    usage();
                }
                out_fp = std::fopen(*++argv, "w");
                if (out_fp == nullptr) {
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
                /* NO RETURN */
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

    char tcbuf[1024];
    tcbuf[0] = 0;
    env_init(tcbuf, true);

    char *cp = std::getenv("NNTPSERVER");
    if (!cp) {
        cp = filexp(SERVER_NAME);
        if (*cp == '/')
        {
            cp = nntp_servername(cp);
        }
    }
    if (std::strcmp(cp,"local") != 0)
    {
        s_server_name = savestr(cp);
        cp = std::strchr(s_server_name, ';');
        if (!cp)
        {
            cp = std::strchr(s_server_name, ':');
        }
        if (cp) {
            *cp = '\0';
            g_nntplink.port_number = std::atoi(cp+1);
        }
        g_nntp_auth_file = filexp(NNTP_AUTH_FILE);
        if ((cp = getenv("NNTP_FORCE_AUTH")) != nullptr
         && (*cp == 'y' || *cp == 'Y'))
        {
            g_nntplink.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
    }

    if (s_server_name) {
        if (init_nntp() < 0
         || nntp_connect(s_server_name,false) <= 0)
        {
            std::exit(1);
        }
        if (action)
        {
            std::sprintf(command,"LIST %s",action);
        }
        else
        {
            std::strcpy(command,"LIST");
        }
        if (wildarg)
        {
            std::sprintf(command+strlen(command)," %s",wildarg);
        }
        if (nntp_command(command) <= 0)
        {
            std::exit(1);
        }
#ifdef HAS_SIGHOLD
        sighold(SIGINT);
#endif
        if (nntp_check() <= 0) {
            std::fprintf(stderr,"nntplist: Can't get %s file from server.\n",action? action : "active");
            std::fprintf(stderr, "Server said: %s\n", g_ser_line);
            finalize(1);
        }
        while (nntp_gets(g_ser_line, sizeof g_ser_line) == 1) {
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
    else {
        cp = nullptr;
        if (!action)
        {
            cp = ACTIVE;
        }
        else if (string_case_equal(action, "active.times"))
        {
            cp = ACTIVE_TIMES;
        }
        else if (string_case_equal(action, "newsgroups"))
        {
            cp = GROUPDESC;
        }
        else if (string_case_equal(action, "subscriptions"))
        {
            cp = SUBSCRIPTIONS;
        }
        else if (string_case_equal(action, "overview.fmt"))
        {
            cp = OVERVIEW_FMT;
        }
        if (!cp || !*cp) {
            std::fprintf(stderr, "Don't know how to list `%s' from your local system.\n",
                    action);
            exit(1);
        }
        std::FILE *in_fp{std::fopen(filexp(cp), "r")};
        if (in_fp == nullptr)
        {
            std::fprintf(stderr,"Unable to open `%s'.\n", cp);
            std::exit(1);
        }
        while (std::fgets(g_ser_line, sizeof g_ser_line, in_fp)) {
            if (wildarg) {
                cp = skip_non_space(g_ser_line);
                if (!cp)
                {
                    continue;
                }
                *cp = '\0';
                if (!wildmat(g_ser_line, wildarg))
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
