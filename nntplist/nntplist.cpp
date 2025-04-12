/* nntplist.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <string>

#include <config/string_case_compare.h>

#include "config/common.h"

#include "util/env.h"
#include "nntp/nntpclient.h"
#include "nntp/nntpinit.h"
#include "string-algos.h"
#include "util/util2.h"
#include "util3.h"
#include "util/wildmat.h"

void Usage();

char *g_server_name{};
std::string g_nntp_auth_file;
int debug{};             /* make nntpclient.c happy */
char g_buf[LBUFLEN + 1]{}; /* general purpose line buffer */

int main(int argc, char *argv[])
{
    char command[32];
    char*action = nullptr;
    char*wildarg = nullptr;
    FILE*out_fp = nullptr;

    while (--argc) {
        if (**++argv == '-') {
            switch (*++*argv) {
            case 'o':
                if (out_fp || !--argc)
                {
                    Usage();
                }
                out_fp = fopen(*++argv, "w");
                if (out_fp == nullptr) {
                    perror(*argv);
                    exit(1);
                }
                break;
            case 'x':
                if (wildarg || !--argc)
                {
                    Usage();
                }
                wildarg = *++argv;
                break;
            default:
                Usage();
                /* NO RETURN */
            }
        }
        else if (!action)
        {
            action = *argv;
        }
        else
        {
            Usage();
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

    char *cp = getenv("NNTPSERVER");
    if (!cp) {
        cp = filexp(SERVER_NAME);
        if (*cp == '/')
        {
            cp = nntp_servername(cp);
        }
    }
    if (strcmp(cp,"local") != 0)
    {
        g_server_name = savestr(cp);
        cp = strchr(g_server_name, ';');
        if (!cp)
        {
            cp = strchr(g_server_name, ':');
        }
        if (cp) {
            *cp = '\0';
            g_nntplink.port_number = atoi(cp+1);
        }
        g_nntp_auth_file = filexp(NNTP_AUTH_FILE);
        if ((cp = getenv("NNTP_FORCE_AUTH")) != nullptr
         && (*cp == 'y' || *cp == 'Y'))
        {
            g_nntplink.flags |= NNTP_FORCE_AUTH_NEEDED;
        }
    }

    if (g_server_name) {
        if (init_nntp() < 0
         || nntp_connect(g_server_name,false) <= 0)
        {
            exit(1);
        }
        if (action)
        {
            sprintf(command,"LIST %s",action);
        }
        else
        {
            strcpy(command,"LIST");
        }
        if (wildarg)
        {
            sprintf(command+strlen(command)," %s",wildarg);
        }
        if (nntp_command(command) <= 0)
        {
            exit(1);
        }
#ifdef HAS_SIGHOLD
        sighold(SIGINT);
#endif
        if (nntp_check() <= 0) {
            fprintf(stderr,"nntplist: Can't get %s file from server.\n",action? action : "active");
            fprintf(stderr, "Server said: %s\n", g_ser_line);
            finalize(1);
        }
        while (nntp_gets(g_ser_line, sizeof g_ser_line) == 1) {
            if (nntp_at_list_end(g_ser_line))
            {
                break;
            }
            fputs(g_ser_line, out_fp);
            putc('\n', out_fp);
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
            fprintf(stderr, "Don't know how to list `%s' from your local system.\n",
                    action);
            exit(1);
        }
        FILE *in_fp = fopen(filexp(cp), "r");
        if (in_fp == nullptr)
        {
            fprintf(stderr,"Unable to open `%s'.\n", cp);
            exit(1);
        }
        while (fgets(g_ser_line, sizeof g_ser_line, in_fp)) {
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
            fputs(g_ser_line, out_fp);
        }
    }
    return 0;
}

void Usage()
{
    fprintf(stderr, "Usage: nntplist [-x WildSpec] [-o OutputFile] [type]\n"
                    "\n"
                    "Where type is any of the LIST command arguments your server accepts.\n");
    exit(1);
}

int nntp_handle_timeout()
{
    fputs("\n503 Server timed out.\n",stderr);
    return -2;
}
