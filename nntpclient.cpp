/* nntpclient.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"

#ifdef SUPPORT_NNTP

#include "nntpinit.h"
#include "INTERN.h"
#include "nntpclient.h"

time_t last_command_diff;

char *savestr(char *);
#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE);
#endif
#ifdef NNTP_HANDLE_TIMEOUT
int nntp_handle_timeout(void);
#endif

int nntp_handle_nested_lists(void);

#ifdef NNTP_HANDLE_AUTH_ERR
int nntp_handle_auth_err(void);
#endif

int nntp_connect(const char *machine, bool_int verbose)
{
    int response;

    if (nntplink.rd_fp)
	return 1;

    if (nntplink.flags & NNTP_FORCE_AUTH_NEEDED)
	nntplink.flags |= NNTP_FORCE_AUTH_NOW;
    nntplink.flags |= NNTP_NEW_CMD_OK;
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

    if (FILE_REF(name) && (fp = fopen(name, "r")) != NULL) {
	while (fgets(g_ser_line, sizeof g_ser_line, fp) != NULL) {
	    if (*g_ser_line == '\n' || *g_ser_line == '#')
		continue;
	    if ((name = index(g_ser_line, '\n')) != NULL)
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
#if defined(NNTP_HANDLE_TIMEOUT) || defined(NNTP_HANDLE_AUTH_ERR)
    strcpy(g_last_command, bp);
# ifdef NNTP_HANDLE_TIMEOUT
    if (!nntplink.rd_fp)
	return nntp_handle_timeout();
# endif
# ifdef NNTP_HANDLE_AUTH_ERR
    if (nntplink.flags & NNTP_FORCE_AUTH_NOW) {
	nntplink.flags &= ~NNTP_FORCE_AUTH_NOW;
	return nntp_handle_auth_err();
    }
# endif
#endif
    if (!(nntplink.flags & NNTP_NEW_CMD_OK)) {
	int ret = nntp_handle_nested_lists();
	if (ret <= 0)
	    return ret;
    }
    if (fprintf(nntplink.wr_fp, "%s\r\n", bp) < 0
     || fflush(nntplink.wr_fp) < 0)
#ifdef NNTP_HANDLE_TIMEOUT
	return nntp_handle_timeout();
#else
	return -2;
#endif
    now = time((time_t*)NULL);
    last_command_diff = now - nntplink.last_command;
    nntplink.last_command = now;
    return 1;
}

int nntp_check(void)
{
    int ret;
    int len = 0;

 read_it:
#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif
    errno = 0;
    ret = (fgets(g_ser_line, sizeof g_ser_line, nntplink.rd_fp) == NULL)? -2 : 0;
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    if (ret < 0) {
	if (errno == EINTR)
	    goto read_it;
	strcpy(g_ser_line, "503 Server closed connection.");
    }
#ifdef NNTP_HANDLE_TIMEOUT
    if (len == 0 && atoi(g_ser_line) == NNTP_TMPERR_VAL
     && nntp_allow_timeout && last_command_diff > 60) {
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
#endif
    if (*g_ser_line <= NNTP_CLASS_CONT && *g_ser_line >= NNTP_CLASS_INF)
	ret = 1;			/* (this includes NNTP_CLASS_OK) */
    else if (*g_ser_line == NNTP_CLASS_FATAL)
	ret = -1;
    /* Even though the following check doesn't catch all possible lists, the
     * bit will get set right when the caller checks nntp_at_list_end(). */
    if (atoi(g_ser_line) == NNTP_LIST_FOLLOWS_VAL)
	nntplink.flags &= ~NNTP_NEW_CMD_OK;
    else
	nntplink.flags |= NNTP_NEW_CMD_OK;
    len = strlen(g_ser_line);
    if (len >= 2 && g_ser_line[len-1] == '\n' && g_ser_line[len-2] == '\r')
	g_ser_line[len-2] = '\0';
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
	printf("<%s\n", g_ser_line) FLUSH;
#endif
#ifdef NNTP_HANDLE_AUTH_ERR
    if (atoi(g_ser_line) == NNTP_AUTH_NEEDED_VAL) {
	ret = nntp_handle_auth_err();
	if (ret > 0)
	    goto read_it;
    }
#endif
    return ret;
}

bool nntp_at_list_end(const char *s)
{
    if (!s || (*s == '.' && (s[1] == '\0' || s[1] == '\r'))) {
	nntplink.flags |= NNTP_NEW_CMD_OK;
	return 1;
    }
    nntplink.flags &= ~NNTP_NEW_CMD_OK;
    return 0;
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
    if (nntplink.trailing_CR) {
	*cp++ = '\r';
	len--;
	nntplink.trailing_CR = 0;
    }
    while (1) {
	if (len == 1) {
	    if (cp[-1] == '\r') {
		/* Hold a trailing CR until next time because we may need
		 * to strip it if it is followed by a newline. */
		cp--;
		nntplink.trailing_CR = 1;
	    }
	    break;
	}
	do {
	    errno = 0;
	    ch = fgetc(nntplink.rd_fp);
	} while (errno == EINTR);
	if (ch == EOF) {
	    nntplink.flags |= NNTP_NEW_CMD_OK;
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

void nntp_close(bool_int send_quit)
{
    if (send_quit && nntplink.wr_fp != NULL && nntplink.rd_fp != NULL) {
	if (nntp_command("QUIT") > 0)
	    nntp_check();
    }
    /* the nntp_check() above might have closed these already. */
    if (nntplink.wr_fp != NULL) {
	fclose(nntplink.wr_fp);
	nntplink.wr_fp = NULL;
    }
    if (nntplink.rd_fp != NULL) {
	fclose(nntplink.rd_fp);
	nntplink.rd_fp = NULL;
    }
    nntplink.flags |= NNTP_NEW_CMD_OK;
}

#endif /* SUPPORT_NNTP */
