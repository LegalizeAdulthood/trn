/* nntpclient.c
*/
// This software is copyrighted as detailed in the LICENSE file.

#include "nntp/nntpclient.h"

#include "nntp/nntpauth.h"
#include "nntp/nntpinit.h"

#include <config/common.h>
#include <trn/nntp.h>

#include <cstdio>
#include <cstring>
#include <ctime>

NNTPLink g_nntp_link{}; // the current server's file handles
bool     g_nntp_allow_timeout{};
char     g_ser_line[NNTP_STRLEN]{};
char     g_last_command[NNTP_STRLEN]{};

static std::time_t s_last_command_diff{};

int nntp_connect(const char *machine, bool verbose)
{
    int response;

    if (g_nntp_link.connection)
    {
        return 1;
    }

    if (g_nntp_link.flags & NNTP_FORCE_AUTH_NEEDED)
    {
        g_nntp_link.flags |= NNTP_FORCE_AUTH_NOW;
    }
    g_nntp_link.flags |= NNTP_NEW_CMD_OK;
#if 0
try_to_connect:
#endif
    if (verbose)
    {
        std::printf("Connecting to %s...",machine);
        std::fflush(stdout);
    }
    switch (response = server_init(machine))
    {
    case NNTP_GOODBYE_VAL:
        if (atoi(g_ser_line) == response)
        {
            char tmpbuf[LINE_BUF_LEN];
            if (verbose)
            {
                std::printf("failed: %s\n",&g_ser_line[4]);
            }
            else
            {
                std::sprintf(tmpbuf,"News server \"%s\" is unavailable: %s\n",
                        machine,&g_ser_line[4]);
                nntp_init_error(tmpbuf);
            }
            response = 0;
            break;
        }

    case -1:
        if (verbose)
        {
            std::printf("failed.\n");
        }
        else
        {
            std::sprintf(g_ser_line,"News server \"%s\" is unavailable.\n",machine);
            nntp_init_error(g_ser_line);
        }
        response = 0;
        break;

    case NNTP_ACCESS_VAL:
        if (verbose)
        {
            std::printf("access denied.\n");
        }
        else
        {
            std::sprintf(g_ser_line,
                    "This machine does not have permission to use the %s news server.\n\n",
                    machine);
            nntp_init_error(g_ser_line);
        }
        response = -1;
        break;

    case NNTP_NOPOSTOK_VAL:
        if (verbose)
        {
            std::printf("Done (but no posting).\n\n");
        }
        response = 1;
        break;

    case NNTP_POSTOK_VAL:
        if (verbose)
        {
            std::printf("Done.\n");
        }
        response = 1;
        break;

    default:
        if (verbose)
        {
            std::printf("unknown response: %d.\n",response);
        }
        else
        {
            std::sprintf(g_ser_line,"\nUnknown response code %d from %s.\n",
                    response,machine);
            nntp_init_error(g_ser_line);
        }
        response = 0;
        break;
    }
#if 0
    if (!response)
    {
        if (handle_no_connect() > 0)
        {
            goto try_to_connect;
        }
    }
#endif
    return response;
}

char *nntp_server_name(char *name)
{
    std::FILE* fp;

    if (FILE_REF(name) && (fp = std::fopen(name, "r")) != nullptr)
    {
        while (std::fgets(g_ser_line, sizeof g_ser_line, fp) != nullptr)
        {
            if (*g_ser_line == '\n' || *g_ser_line == '#')
            {
                continue;
            }
            name = std::strchr(g_ser_line, '\n');
            if (name != nullptr)
            {
                *name = '\0';
            }
            name = g_ser_line;
            break;
        }
        std::fclose(fp);
    }
    return name;
}

int nntp_command(const char *bp)
{
    std::time_t now;
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
    {
        std::printf(">%s\n", bp);
    }
#endif
    std::strcpy(g_last_command, bp);
    if (!g_nntp_link.connection)
    {
        return nntp_handle_timeout();
    }
    if (g_nntp_link.flags & NNTP_FORCE_AUTH_NOW)
    {
        g_nntp_link.flags &= ~NNTP_FORCE_AUTH_NOW;
        return nntp_handle_auth_err();
    }
    if (!(g_nntp_link.flags & NNTP_NEW_CMD_OK))
    {
        int ret = nntp_handle_nested_lists();
        if (ret <= 0)
        {
            return ret;
        }
    }
    error_code_t ec;
    g_nntp_link.connection->write_line(bp, ec);
    if (ec)
    {
        return nntp_handle_timeout();
    }
    now = std::time((std::time_t*)nullptr);
    s_last_command_diff = now - g_nntp_link.last_command;
    g_nntp_link.last_command = now;
    return 1;
}

int nntp_xgtitle(const char *groupname)
{
    std::sprintf(g_ser_line, "XGTITLE %s", groupname);
    const int status = nntp_command(g_ser_line);
    if (status <= 0)
    {
        return status;
    }

    return nntp_check();
}

int nntp_check()
{
    int len = 0;

read_it:
#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif
    errno = 0;
    error_code_t ec;
    std::string line = g_nntp_link.connection->read_line(ec);
    std::strncpy(g_ser_line, line.c_str(), sizeof g_ser_line);
    int ret = ec ? -2 : 0;
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    if (ret < 0)
    {
        if (errno == EINTR)
        {
            goto read_it;
        }
        std::strcpy(g_ser_line, "503 Server closed connection.");
    }
    if (len == 0 && std::atoi(g_ser_line) == NNTP_TMPERR_VAL //
        && g_nntp_allow_timeout && s_last_command_diff > 60)
    {
        ret = nntp_handle_timeout();
        switch (ret)
        {
        case 1:
            len = 1;
            goto read_it;

        case 0:         // We're quitting, so pretend it's OK
            std::strcpy(g_ser_line, "205 Ok");
            break;

        default:
            break;
        }
    }
    else if (*g_ser_line <= NNTP_CLASS_CONT && *g_ser_line >= NNTP_CLASS_INF)
    {
        ret = 1;                        // (this includes NNTP_CLASS_OK)
    }
    else if (*g_ser_line == NNTP_CLASS_FATAL)
    {
        ret = -1;
    }
    /* Even though the following check doesn't catch all possible lists, the
     * bit will get set right when the caller checks nntp_at_list_end(). */
    if (std::atoi(g_ser_line) == NNTP_LIST_FOLLOWS_VAL)
    {
        g_nntp_link.flags &= ~NNTP_NEW_CMD_OK;
    }
    else
    {
        g_nntp_link.flags |= NNTP_NEW_CMD_OK;
    }
    len = std::strlen(g_ser_line);
    if (len >= 2 && g_ser_line[len-1] == '\n' && g_ser_line[len-2] == '\r')
    {
        g_ser_line[len-2] = '\0';
    }
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
    {
        std::printf("<%s\n", g_ser_line);
    }
#endif
    if (std::atoi(g_ser_line) == NNTP_AUTH_NEEDED_VAL)
    {
        ret = nntp_handle_auth_err();
        if (ret > 0)
        {
            goto read_it;
        }
    }
    return ret;
}

bool nntp_at_list_end(const char *s)
{
    if (!s || (*s == '.' && (s[1] == '\0' || s[1] == '\r')))
    {
        g_nntp_link.flags |= NNTP_NEW_CMD_OK;
        return true;
    }
    g_nntp_link.flags &= ~NNTP_NEW_CMD_OK;
    return false;
}

class SigIntHolder
{
public:
    SigIntHolder()
    {
#ifdef HAS_SIGHOLD
        sighold(SIGINT);
#endif
    }
    ~SigIntHolder()
    {
#ifdef HAS_SIGHOLD
        sigrelse(SIGINT);
#endif
    }
};

static std::string s_nntp_gets_line;

/* This returns 1 when it reads a full line, 0 if it reads a partial
 * line, and -2 on EOF/error.  The maximum length includes a spot for
 * the null-terminator, and we need room for our "\r\n"-stripping code
 * to work right, so "len" MUST be at least 3.
 */
NNTPGetsResult nntp_gets(char *bp, int len)
{
    SigIntHolder holder;

    error_code_t ec;
    if (s_nntp_gets_line.empty())
    {
        s_nntp_gets_line = g_nntp_link.connection->read_line(ec);
        if (ec)
        {
            return NGSR_ERROR;
        }
    }

    std::strncpy(bp, s_nntp_gets_line.c_str(), len-1);
    if (len >= static_cast<int>(s_nntp_gets_line.length()))
    {
        bp[s_nntp_gets_line.length()] = '\0';
        s_nntp_gets_line.clear();
        return NGSR_FULL_LINE;
    }

    bp[len-1] = '\0';
    s_nntp_gets_line = s_nntp_gets_line.substr(len);
    return NGSR_PARTIAL_LINE;
}

void nntp_gets_clear_buffer()
{
    s_nntp_gets_line.clear();
}

void nntp_close(bool send_quit)
{
    if (send_quit && g_nntp_link.connection)
    {
        if (nntp_command("QUIT") > 0)
        {
            nntp_check();
        }
    }
    g_nntp_link.connection.reset();
    g_nntp_link.flags |= NNTP_NEW_CMD_OK;
}
