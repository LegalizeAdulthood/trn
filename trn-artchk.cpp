/* trn-artchk.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */


/* A program to check an article's validity and print warnings if problems
** are found.
**
** Usage: trn-artchk <article> <maxLineLen> <newsgroupsFile> <activeFile>
*/

#include "EXTERN.h"
#include "common.h"
#include "env.h"
#include "util2.h"
#include "util3.h"
#include "nntpclient.h"
#include "nntpinit.h"
#include "INTERN.h"
#include "common.h"

#define MAXNGS 100

int server_connection ();
int nntp_handle_timeout ();

char* server_name;
char* g_nntp_auth_file;

int debug = 0;

char* g_home_dir;
char* g_dot_dir;

int main(int argc, char *argv[])
{
    FILE* fp;
    FILE* fp_active = nullptr;
    FILE* fp_ng = nullptr;
    bool check_active = false;
    bool check_ng = false;
    char buff[LBUFLEN];
    char* cp;
    char* cp2;
    char* ngptrs[MAXNGS];
    int nglens[MAXNGS];
    int foundactive[MAXNGS];
    int i, col, max_col_len, line_num = 0, ngcnt = 0, ngleft;
    int found_newsgroups = 0;

    g_home_dir = getenv("HOME");
    if (g_home_dir == nullptr)
	g_home_dir = getenv("LOGDIR");
    g_dot_dir = getenv("DOTDIR");
    if (!g_dot_dir)
	g_dot_dir = g_home_dir;

    if (argc != 5 || !(max_col_len = atoi(argv[2]))) {
	fprintf(stderr, "\
Usage: trn-artchk <article> <maxLineLen> <newsgroupsFile> <activeFile>\n");
	exit(1);
    }

    if ((fp = fopen(argv[1], "r")) == nullptr) {
	fprintf(stderr, "trn-artchk: unable to open article `%s'.\n", argv[1]);
	exit(1);
    }

    /* Check the header for proper format and report on the newsgroups */
    while (fgets(buff, LBUFLEN, fp)) {
	line_num++;
	buff[strlen(buff)-1] = '\0';
	if (!*buff)
	    break;
	if (*buff == ' ' || *buff == '\t')
	    continue;
	if (!(cp = strchr(buff, ':'))) {
	    printf("\nERROR: line %d is an invalid header line:\n%s\n",
		   line_num, buff);
	    break;
	}
	if (cp[1] != ' ' && cp[1] != '\0') {
	    printf("\n\
ERROR: header on line %d does not have a space after the colon:\n%s\n",
		   line_num, buff);
	}
	if (cp - buff == 10 && !strncmp(buff, "Newsgroups", 10)) {
	    found_newsgroups = 1;
	    for (cp = buff + 11; *cp == ' '; cp++)
		;
	    if (strchr(cp, ' ')) {
		printf("\n\
ERROR: the \"Newsgroups:\" line has spaces in it that MUST be removed. The\n\
only allowable space is the one separating the colon (:) from the contents.\n\
Use a comma (,) to separate multiple newsgroup names.\n");
		continue;
	    }
	    while (*cp) {
		if (!(cp2 = strchr(cp, ',')))
		    cp2 = cp + strlen(cp);
		else
		    *cp2++ = '\0';
		if (ngcnt < MAXNGS) {
		    nglens[ngcnt] = strlen(cp);
		    foundactive[ngcnt] = 0;
		    ngptrs[ngcnt] = safemalloc(nglens[ngcnt]+1);
		    strcpy(ngptrs[ngcnt], cp);
		    ngcnt++;
		}
		cp = cp2;
	    }
	    if (!ngcnt) {
		printf("\n\
ERROR: the \"Newsgroups:\" line lists no newsgroups.\n");
		continue;
	    }
	}
    }
    if (!found_newsgroups) {
	printf("\nERROR: the \"Newsgroups:\" line is missing from the header.\n");
    }

    /* Check the body of the article for long lines */
    while (fgets(buff, LBUFLEN, fp)) {
	line_num++;
	col = strlen(buff)-1;
	if (buff[col] != '\n')
	    printf("\n\
Warning: line %d has no trailing newline character and may get lost.\n",
		   line_num);
	else
	    buff[col] = '\0';
	col = 0;
	for (cp = buff; *cp; cp++) {
	    if (*cp == '\t')
		col += 8 - (col%8);
	    else
		col++;
	}
	if (col > max_col_len) {
	    printf("\n\
Warning: posting exceeds %d columns.  Line %d is the first long one:\n%s\n",
		   max_col_len, line_num, buff);
	    break;
	}
    }
    cp = getenv("NNTPSERVER");
    if (!cp) {
	cp = filexp(SERVER_NAME);
	if (FILE_REF(cp))
	    cp = nntp_servername(cp);
    }
    if (strcmp(cp,"local")) {
	server_name = savestr(cp);
	cp = strchr(server_name, ';');
	if (!cp)
	    cp = strchr(server_name, ':');
	if (cp) {
	    *cp = '\0';
	    nntplink.port_number = atoi(cp+1);
	}
	g_nntp_auth_file = filexp(NNTP_AUTH_FILE);
	if ((cp = getenv("NNTP_FORCE_AUTH")) != nullptr
	 && (*cp == 'y' || *cp == 'Y'))
	    nntplink.flags |= NNTP_FORCE_AUTH_NEEDED;
	if (init_nntp() < 0)
	    server_name = nullptr;
    }
    if (ngcnt) {
	struct stat st;
	if (stat(argv[3], &st) != -1)
	    check_ng = st.st_size > 0 && (fp_ng = fopen(argv[3], "r")) != nullptr;
	else if (server_name && server_connection())
	    check_ng = true;
	if (stat(argv[4], &st) != -1)
	    check_active = st.st_size > 0 && (fp_active = fopen(argv[4], "r")) != nullptr;
	else if (server_name && server_connection())
	    check_active = true;
    }
    if (ngcnt && (check_ng || check_active)) {
	/* Print a note about each newsgroup */
	printf("\nYour article's newsgroup%s:\n", PLURAL(ngcnt));
	if (!check_active) {
	    for (i = 0; i < ngcnt; i++) {
		foundactive[i] = 1;
	    }
	}
	else if (fp_active != nullptr) {
	    ngleft = ngcnt;
	    while (fgets(buff, LBUFLEN, fp_active)) {
		if (!ngleft)
		    break;
		for (i = 0; i < ngcnt; i++) {
		    if (!foundactive[i]) {
			if ((buff[nglens[i]] == '\t' || buff[nglens[i]] == ' ')
			  && !strncmp(ngptrs[i], buff, nglens[i])) {
			    foundactive[i] = 1;
			    ngleft--;
			}
		    }
		}
	    }
	    fclose(fp_active);
	}
	else if (server_name) {
	    int listactive_works = 1;
	    for (i = 0; i < ngcnt; i++) {
		if (listactive_works) {
		    sprintf(g_ser_line, "list active %s", ngptrs[i]);
		    if (nntp_command(g_ser_line) <= 0)
			break;
		    if (nntp_check() > 0) {
			while (nntp_gets(g_ser_line, sizeof g_ser_line) >= 0) {
			    if (nntp_at_list_end(g_ser_line))
				break;
			    foundactive[i] = 1;
			}
		    }
		    else if (*g_ser_line == NNTP_CLASS_FATAL) {
			listactive_works = false;
			i--;
		    }
		}
		else {
		    sprintf(g_ser_line, "GROUP %s", ngptrs[i]);
		    if (nntp_command(g_ser_line) <= 0)
			break;
		    if (nntp_check() > 0)
			foundactive[i] = 1;
		}
	    }
	}
	if (check_ng && fp_ng == nullptr) {
	    fp_ng = fopen(argv[3], "w+");
	    unlink(argv[3]);
	    if (fp_ng != nullptr) {
		for (i = 0; i < ngcnt; i++) {
		    /* issue a description list command */
		    sprintf(g_ser_line, "XGTITLE %s", ngptrs[i]);
		    if (nntp_command(g_ser_line) <= 0)
			break;
		    /*$$ use list newsgroups if this fails...? */
		    if (nntp_check() > 0) {
			/* write results to fp_ng */
			while (nntp_gets(g_ser_line, sizeof g_ser_line) >= 0) {
			    if (nntp_at_list_end(g_ser_line))
				break;
			    fprintf(fp_ng, "%s\n", g_ser_line);
			}
		    }
		}
		fseek(fp_ng, 0L, 0);
	    }
	}
	if (fp_ng != nullptr) {
	    ngleft = ngcnt;
	    while (fgets(buff, LBUFLEN, fp_ng)) {
		if (!ngleft)
		    break;
		for (i = 0; i < ngcnt; i++) {
		    if (foundactive[i] && ngptrs[i]) {
			if ((buff[nglens[i]] == '\t' || buff[nglens[i]] == ' ')
			  && !strncmp(ngptrs[i], buff, nglens[i])) {
			    cp = &buff[nglens[i]];
			    *cp++ = '\0';
			    while (*cp == ' ' || *cp == '\t')
				cp++;
			    if (cp[0] == '?' && cp[1] == '?')
				cp = "[no description available]\n";
			    printf("%-23s %s", buff, cp);
			    free(ngptrs[i]);
			    ngptrs[i] = 0;
			    ngleft--;
			}
		    }
		}
	    }
	    fclose(fp_ng);
	}
	for (i = 0; i < ngcnt; i++) {
	    if (!foundactive[i]) {
		printf("%-23s ** invalid news group -- check spelling **\n",
		   ngptrs[i]);
		free(ngptrs[i]);
	    }
	    else if (ngptrs[i]) {
		printf("%-23s [no description available]\n", ngptrs[i]);
		free(ngptrs[i]);
	    }
	}
    }

    nntp_close(true);
    if (server_name)
	cleanup_nntp();

    return 0;
}

int server_connection()
{
    static int server_stat = 0;
    if (!server_stat) {
	if (nntp_connect(server_name,false) > 0)
	    server_stat = 1;
	else
	    server_stat = -1;
    }
    return server_stat == 1;
}

int nntp_handle_timeout()
{
    fputs("\n503 Server timed out.\n",stderr);
    return -2;
}
