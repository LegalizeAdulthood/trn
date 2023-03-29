/* nntpclient.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "nntpclient.h"

#include "nntp.h"
#include "nntpauth.h"
#include "nntpinit.h"

NNTPLINK g_nntplink{};		/* the current server's file handles */
bool g_nntp_allow_timeout{};
char g_ser_line[NNTP_STRLEN]{};
char g_last_command[NNTP_STRLEN]{};

static time_t s_last_command_diff{};

int nntp_connect(const char *machine, bool verbose)
{
    int response;

    if (g_nntplink.rd_fp)
	return 1;

    if (g_nntplink.flags & NNTP_FORCE_AUTH_NEEDED)
	g_nntplink.flags |= NNTP_FORCE_AUTH_NOW;
    g_nntplink.flags |= NNTP_NEW_CMD_OK;
#if 0
  try_to_connect:
#endif
    if (verbose) {
	printf("Connecting to %s...",machine);
	fflush(stdout);
    }
    switch (response = server_init(machine)) {
    case NNTP_GOODBYE_VAL:
	if (atoi(g_ser_line) == response) {
	    char tmpbuf[LBUFLEN];
	    if (verbose)
		printf("failed: %s\n",&g_ser_line[4]);
	    else {
		sprintf(tmpbuf,"News server \"%s\" is unavailable: %s\n",
			machine,&g_ser_line[4]);
		nntp_init_error(tmpbuf);
	    }
	    response = 0;
	    break;
	}
    case -1:
	if (verbose)
	    printf("failed.\n");
	else {
	    sprintf(g_ser_line,"News server \"%s\" is unavailable.\n",machine);
	    nntp_init_error(g_ser_line);
	}
	response = 0;
	break;
    case NNTP_ACCESS_VAL:
	if (verbose)
	    printf("access denied.\n");
	else {
	    sprintf(g_ser_line,
		    "This machine does not have permission to use the %s news server.\n\n",
		    machine);
	    nntp_init_error(g_ser_line);
	}
	response = -1;
	break;
    case NNTP_NOPOSTOK_VAL:
	if (verbose)
	    printf("Done (but no posting).\n\n");
	response = 1;
	break;
    case NNTP_POSTOK_VAL:
	if (verbose)
	    printf("Done.\n") FLUSH;
	response = 1;
	break;
    default:
	if (verbose)
	    printf("unknown response: %d.\n",response);
	else {
	    sprintf(g_ser_line,"\nUnknown response code %d from %s.\n",
		    response,machine);
	    nntp_init_error(g_ser_line);
	}
	response = 0;
	break;
    }
#if 0
    if (!response) {
	if (handle_no_connect() > 0)
	    goto try_to_connect;
    }
#endif
    return response;
}

char *nntp_servername(char *name)
{
    FILE* fp;

    if (FILE_REF(name) && (fp = fopen(name, "r")) != nullptr) {
	while (fgets(g_ser_line, sizeof g_ser_line, fp) != nullptr) {
	    if (*g_ser_line == '\n' || *g_ser_line == '#')
		continue;
            name = strchr(g_ser_line, '\n');
            if (name != nullptr)
		*name = '\0';
	    name = g_ser_line;
	    break;
	}
	fclose(fp);
    }
    return name;
}

int nntp_command(const char *bp)
{
    time_t now;
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
	printf(">%s\n", bp) FLUSH;
#endif
    strcpy(g_last_command, bp);
    if (!g_nntplink.rd_fp)
	return nntp_handle_timeout();
    if (g_nntplink.flags & NNTP_FORCE_AUTH_NOW) {
	g_nntplink.flags &= ~NNTP_FORCE_AUTH_NOW;
	return nntp_handle_auth_err();
    }
    if (!(g_nntplink.flags & NNTP_NEW_CMD_OK)) {
	int ret = nntp_handle_nested_lists();
	if (ret <= 0)
	    return ret;
    }
    if (fprintf(g_nntplink.wr_fp, "%s\r\n", bp) < 0
     || fflush(g_nntplink.wr_fp) < 0)
	return nntp_handle_timeout();
    now = time((time_t*)nullptr);
    s_last_command_diff = now - g_nntplink.last_command;
    g_nntplink.last_command = now;
    return 1;
}

int nntp_check()
{
    int len = 0;

 read_it:
#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif
    errno = 0;
    int ret = (fgets(g_ser_line, sizeof g_ser_line, g_nntplink.rd_fp) == nullptr) ? -2 : 0;
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    if (ret < 0) {
	if (errno == EINTR)
	    goto read_it;
	strcpy(g_ser_line, "503 Server closed connection.");
    }
    if (len == 0 && atoi(g_ser_line) == NNTP_TMPERR_VAL
     && g_nntp_allow_timeout && s_last_command_diff > 60) {
	ret = nntp_handle_timeout();
	switch (ret) {
	case 1:
	    len = 1;
	    goto read_it;
	case 0:		/* We're quitting, so pretend it's OK */
	    strcpy(g_ser_line, "205 Ok");
	    break;
	default:
	    break;
	}
    }
    else
    if (*g_ser_line <= NNTP_CLASS_CONT && *g_ser_line >= NNTP_CLASS_INF)
	ret = 1;			/* (this includes NNTP_CLASS_OK) */
    else if (*g_ser_line == NNTP_CLASS_FATAL)
	ret = -1;
    /* Even though the following check doesn't catch all possible lists, the
     * bit will get set right when the caller checks nntp_at_list_end(). */
    if (atoi(g_ser_line) == NNTP_LIST_FOLLOWS_VAL)
	g_nntplink.flags &= ~NNTP_NEW_CMD_OK;
    else
	g_nntplink.flags |= NNTP_NEW_CMD_OK;
    len = strlen(g_ser_line);
    if (len >= 2 && g_ser_line[len-1] == '\n' && g_ser_line[len-2] == '\r')
	g_ser_line[len-2] = '\0';
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
	printf("<%s\n", g_ser_line) FLUSH;
#endif
    if (atoi(g_ser_line) == NNTP_AUTH_NEEDED_VAL) {
	ret = nntp_handle_auth_err();
	if (ret > 0)
	    goto read_it;
    }
    return ret;
}

bool nntp_at_list_end(const char *s)
{
    if (!s || (*s == '.' && (s[1] == '\0' || s[1] == '\r'))) {
	g_nntplink.flags |= NNTP_NEW_CMD_OK;
	return true;
    }
    g_nntplink.flags &= ~NNTP_NEW_CMD_OK;
    return false;
}

/* This returns 1 when it reads a full line, 0 if it reads a partial
 * line, and -2 on EOF/error.  The maximum length includes a spot for
 * the null-terminator, and we need room for our "\r\n"-stripping code
 * to work right, so "len" MUST be at least 3.
 */
int nntp_gets(char *bp, int len)
{
    int ch, n = 0;
    char* cp = bp;

#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif
    if (g_nntplink.trailing_CR) {
	*cp++ = '\r';
	len--;
	g_nntplink.trailing_CR = false;
    }
    while (true) {
	if (len == 1) {
	    if (cp[-1] == '\r') {
		/* Hold a trailing CR until next time because we may need
		 * to strip it if it is followed by a newline. */
		cp--;
		g_nntplink.trailing_CR = true;
	    }
	    break;
	}
	do {
	    errno = 0;
	    ch = fgetc(g_nntplink.rd_fp);
	} while (errno == EINTR);
	if (ch == EOF) {
	    g_nntplink.flags |= NNTP_NEW_CMD_OK;
	    n = -2;
	    break;
	}
	if (ch == '\n') {
	    if (cp != bp && cp[-1] == '\r')
		cp--;
	    n = 1;
	    break;
	}
	*cp++ = ch;
	len--;
    }
    *cp = '\0';
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    return n;
}

void nntp_close(bool send_quit)
{
    if (send_quit && g_nntplink.wr_fp != nullptr && g_nntplink.rd_fp != nullptr) {
	if (nntp_command("QUIT") > 0)
	    nntp_check();
    }
    /* the nntp_check() above might have closed these already. */
    if (g_nntplink.wr_fp != nullptr) {
	fclose(g_nntplink.wr_fp);
	g_nntplink.wr_fp = nullptr;
    }
    if (g_nntplink.rd_fp != nullptr) {
	fclose(g_nntplink.rd_fp);
	g_nntplink.rd_fp = nullptr;
    }
    g_nntplink.flags |= NNTP_NEW_CMD_OK;
}
