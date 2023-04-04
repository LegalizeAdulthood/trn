/* nntp.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "nntp.h"

#include "artio.h"
#include "cache.h"
#include "datasrc.h"
#include "final.h"
#include "head.h"
#include "init.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "rcstuff.h"
#include "term.h"
#include "trn.h"
#include "util2.h"

static int nntp_copybody(char *s, int limit, ART_POS pos);

static ART_POS s_body_pos{-1};
static ART_POS s_body_end{};
#ifdef SUPPORT_XTHREAD
static long s_rawbytes{-1}; /* bytes remaining to be transfered */
#endif

int nntp_list(const char *type, const char *arg, int len)
{
    int ret;
#ifdef DEBUG /*$$*/
    if (len && (debug & 1) && !strcasecmp(type,"active"))
	return -1;
#endif
    if (len)
	sprintf(g_ser_line, "LIST %s %.*s", type, len, arg);
    else if (!strcasecmp(type,"active"))
	strcpy(g_ser_line, "LIST");
    else
	sprintf(g_ser_line, "LIST %s", type);
    if (nntp_command(g_ser_line) <= 0)
	return -2;
    ret = nntp_check();
    if (ret <= 0)
	return ret? ret : -1;
    if (!len)
	return 1;
    ret = nntp_gets(g_ser_line, sizeof g_ser_line);
    if (ret < 0)
	return ret;
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
	printf("<%s\n", g_ser_line) FLUSH;
#endif
    if (nntp_at_list_end(g_ser_line))
	return 0;
    return 1;
}

void nntp_finish_list()
{
    int ret;
    do {
	while ((ret = nntp_gets(g_ser_line, sizeof g_ser_line)) == 0) {
	    /* A line w/o a newline is too long to be the end of the
	    ** list, so grab the rest of this line and try again. */
	    while ((ret = nntp_gets(g_ser_line, sizeof g_ser_line)) == 0)
		;
	    if (ret < 0)
		return;
	}
    } while (ret > 0 && !nntp_at_list_end(g_ser_line));
}

/* try to access the specified group */

int nntp_group(const char *group, NGDATA *gp)
{
    sprintf(g_ser_line, "GROUP %s", group);
    if (nntp_command(g_ser_line) <= 0)
	return -2;
    switch (nntp_check()) {
      case -2:
	return -2;
      case -1:
      case 0: {
	int ser_int = atoi(g_ser_line);
	if (ser_int != NNTP_NOSUCHGROUP_VAL
	 && ser_int != NNTP_SYNTAX_VAL) {
	    if (ser_int != NNTP_AUTH_NEEDED_VAL && ser_int != NNTP_ACCESS_VAL
	     && ser_int != NNTP_AUTH_REJECT_VAL) {
		fprintf(stderr, "\nServer's response to GROUP %s:\n%s\n",
			group, g_ser_line);
		return -1;
	    }
	}
	return 0;
      }
    }
    if (gp) {
	long count, first, last;

	(void) sscanf(g_ser_line,"%*d%ld%ld%ld",&count,&first,&last);
	/* NNTP mangles the high/low values when no articles are present. */
	if (!count)
	    gp->abs1st = gp->ngmax+1;
	else {
	    gp->abs1st = (ART_NUM)first;
	    gp->ngmax = (ART_NUM)last;
	}
    }
    return 1;
}

/* check on an article's existence */

int nntp_stat(ART_NUM artnum)
{
    sprintf(g_ser_line, "STAT %ld", (long)artnum);
    if (nntp_command(g_ser_line) <= 0)
	return -2;
    return nntp_check();
}

/* check on an article's existence by its message id */

ART_NUM nntp_stat_id(char *msgid)
{
    long artnum;

    sprintf(g_ser_line, "STAT %s", msgid);
    if (nntp_command(g_ser_line) <= 0)
	return -2;
    artnum = nntp_check();
    if (artnum > 0 && sscanf(g_ser_line, "%*d%ld", &artnum) != 1)
	artnum = 0;
    return (ART_NUM)artnum;
}

ART_NUM nntp_next_art()
{
    long artnum;

    if (nntp_command("NEXT") <= 0)
	return -2;
    artnum = nntp_check();
    if (artnum > 0 && sscanf(g_ser_line, "%*d %ld", &artnum) != 1)
	artnum = 0;
    return (ART_NUM)artnum;
}

/* prepare to get the header */

int nntp_header(ART_NUM artnum)
{
    sprintf(g_ser_line, "HEAD %ld", (long)artnum);
    if (nntp_command(g_ser_line) <= 0)
	return -2;
    return nntp_check();
}

/* copy the body of an article to a temporary file */

void nntp_body(ART_NUM artnum)
{
    char *artname = nntp_artname(artnum, false); /* Is it already in a tmp file? */
    if (artname) {
	if (s_body_pos >= 0)
	    nntp_finishbody(FB_DISCARD);
	g_artfp = fopen(artname,"r");
	if (g_artfp && fstat(fileno(g_artfp),&g_filestat) == 0)
	    s_body_end = g_filestat.st_size;
	return;
    }

    artname = nntp_artname(artnum, true);   /* Allocate a tmp file */
    if (!(g_artfp = fopen(artname, "w+"))) {
	fprintf(stderr, "\nUnable to write temporary file: '%s'.\n",
		artname);
	finalize(1); /*$$*/
    }
#ifndef MSDOS
    chmod(artname, 0600);
#endif
    /*artio_setbuf(g_artfp);$$*/
    if (g_parsed_art == artnum)
	sprintf(g_ser_line, "BODY %ld", (long)artnum);
    else
	sprintf(g_ser_line, "ARTICLE %ld", (long)artnum);
    if (nntp_command(g_ser_line) <= 0)
	finalize(1); /*$$*/
    switch (nntp_check()) {
      case -2:
      case -1:
	finalize(1); /*$$*/
      case 0:
	fclose(g_artfp);
	g_artfp = nullptr;
	errno = ENOENT;			/* Simulate file-not-found */
	return;
    }
    s_body_pos = 0;
    if (g_parsed_art == artnum) {
	fwrite(g_headbuf, 1, strlen(g_headbuf), g_artfp);
	s_body_end = (ART_POS)ftell(g_artfp);
	g_htype[PAST_HEADER].minpos = s_body_end;
    }
    else {
	char b[NNTP_STRLEN];
	s_body_end = 0;
	ART_POS prev_pos = 0;
	while (nntp_copybody(b, sizeof b, s_body_end+1) > 0) {
	    if (*b == '\n' && s_body_end - prev_pos < sizeof b)
		break;
	    prev_pos = s_body_end;
	}
    }
    fseek(g_artfp, 0L, 0);
    g_nntplink.flags &= ~NNTP_NEW_CMD_OK;
}

long nntp_artsize()
{
    return s_body_pos < 0 ? s_body_end : -1;
}

static int nntp_copybody(char *s, int limit, ART_POS pos)
{
    bool had_nl = true;

    while (pos > s_body_end || !had_nl) {
	int found_nl = nntp_gets(s, limit);
	if (found_nl < 0)
	    strcpy(s,"."); /*$$*/
	if (had_nl) {
	    if (nntp_at_list_end(s)) {
		fseek(g_artfp, (long)s_body_pos, 0);
		s_body_pos = -1;
		return 0;
	    }
	    if (s[0] == '.')
		safecpy(s,s+1,limit);
	}
	int len = strlen(s);
	if (found_nl)
	    strcpy(s+len, "\n");
	fputs(s, g_artfp);
	s_body_end = ftell(g_artfp);
	had_nl = found_nl;
    }
    return 1;
}

int nntp_finishbody(finishbody_mode bmode)
{
    char b[NNTP_STRLEN];
    if (s_body_pos < 0)
	return 0;
    if (bmode == FB_DISCARD) {
	/*printf("Discarding the rest of the article...\n") FLUSH; $$*/
#if 0
	/* Implement this if flushing the data becomes possible */
	nntp_artname(g_openart, -1); /* Or something... */
	g_openart = 0;	/* Since we didn't finish the art, forget its number */
#endif
    }
    else
    if (bmode == FB_OUTPUT) {
	if (g_verbose)
            printf("Receiving the rest of the article...");
        else
            printf("Receiving...");
        fflush(stdout);
    }
    if (s_body_end != s_body_pos)
	fseek(g_artfp, (long)s_body_end, 0);
    if (bmode != FB_BACKGROUND)
	nntp_copybody(b, sizeof b, (ART_POS)0x7fffffffL);
    else {
	while (nntp_copybody(b, sizeof b, s_body_end+1)) {
	    if (input_pending())
		break;
	}
	if (s_body_pos >= 0)
	    fseek(g_artfp, (long)s_body_pos, 0);
    }
    if (bmode == FB_OUTPUT)
	erase_line(false);	/* erase the prompt */
    return 1;
}

int nntp_seekart(ART_POS pos)
{
    if (s_body_pos >= 0) {
	if (s_body_end < pos) {
	    char b[NNTP_STRLEN];
	    fseek(g_artfp, (long)s_body_end, 0);
	    nntp_copybody(b, sizeof b, pos);
	    if (s_body_pos >= 0)
		s_body_pos = pos;
	}
	else
	    s_body_pos = pos;
    }
    return fseek(g_artfp, (long)pos, 0);
}

ART_POS nntp_tellart()
{
    return s_body_pos < 0 ? (ART_POS)ftell(g_artfp) : s_body_pos;
}

char *nntp_readart(char *s, int limit)
{
    if (s_body_pos >= 0) {
	if (s_body_pos == s_body_end) {
	    if (nntp_copybody(s, limit, s_body_pos+1) <= 0)
		return nullptr;
	    if (s_body_end - s_body_pos < limit) {
		s_body_pos = s_body_end;
		return s;
	    }
	    fseek(g_artfp, (long)s_body_pos, 0);
	}
	s = fgets(s, limit, g_artfp);
	s_body_pos = ftell(g_artfp);
	if (s_body_pos == s_body_end)
	    fseek(g_artfp, (long)s_body_pos, 0);  /* Prepare for coming write */
	return s;
    }
    return fgets(s, limit, g_artfp);
}

/* This is a 1-relative list */
static int s_maxdays[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

time_t nntp_time()
{
    if (nntp_command("DATE") <= 0)
	return -2;
    if (nntp_check() <= 0)
	return time((time_t*)nullptr);

    char * s = strrchr(g_ser_line, ' ') + 1;
    int    month = (s[4] - '0') * 10 + (s[5] - '0');
    int    day = (s[6] - '0') * 10 + (s[7] - '0');
    int    hh = (s[8] - '0') * 10 + (s[9] - '0');
    int    mm = (s[10] - '0') * 10 + (s[11] - '0');
    time_t ss = (s[12] - '0') * 10 + (s[13] - '0');
    s[4] = '\0';
    int year = atoi(s);

    /* This simple algorithm will be valid until the year 2100 */
    if (year % 4)
	s_maxdays[2] = 28;
    else
	s_maxdays[2] = 29;
    if (month < 1 || month > 12 || day < 1 || day > s_maxdays[month]
     || hh < 0 || hh > 23 || mm < 0 || mm > 59
     || ss < 0 || ss > 59)
	return time((time_t*)nullptr);

    for (month--; month; month--)
	day += s_maxdays[month];

    ss = ((((year-1970) * 365 + (year-1969)/4 + day - 1) * 24L + hh) * 60
	  + mm) * 60 + ss;

    return ss;
}

int nntp_newgroups(time_t t)
{
    struct tm *ts = gmtime(&t);
    sprintf(g_ser_line, "NEWGROUPS %02d%02d%02d %02d%02d%02d GMT",
	ts->tm_year % 100, ts->tm_mon+1, ts->tm_mday,
	ts->tm_hour, ts->tm_min, ts->tm_sec);
    if (nntp_command(g_ser_line) <= 0)
	return -2;
    return nntp_check();
}

int nntp_artnums()
{
    if (g_datasrc->flags & DF_NOLISTGROUP)
	return 0;
    if (nntp_command("LISTGROUP") <= 0)
	return -2;
    if (nntp_check() <= 0) {
	g_datasrc->flags |= DF_NOLISTGROUP;
	return 0;
    }
    return 1;
}

#if 0
int nntp_rover()
{
    if (g_datasrc->flags & DF_NOXROVER)
	return 0;
    if (nntp_command("XROVER 1-") <= 0)
	return -2;
    if (nntp_check() <= 0) {
	g_datasrc->flags |= DF_NOXROVER;
	return 0;
    }
    return 1;
}
#endif

ART_NUM nntp_find_real_art(ART_NUM after)
{
    ART_NUM an;

    if (g_last_cached > after || g_last_cached < g_absfirst
     || nntp_stat(g_last_cached) <= 0) {
	if (nntp_stat_id("") > after)
	    return 0;
    }

    while ((an = nntp_next_art()) > 0) {
	if (an > after)
	    return an;
	if (after - an > 10)
	    break;
    }

    return 0;
}

char *nntp_artname(ART_NUM artnum, bool allocate)
{
    static ART_NUM artnums[MAX_NNTP_ARTICLES];
    static time_t  artages[MAX_NNTP_ARTICLES];
    time_t         lowage;
    int            i, j;

    if (!artnum) {
	for (i = 0; i < MAX_NNTP_ARTICLES; i++) {
	    artnums[i] = 0;
	    artages[i] = 0;
	}
	return nullptr;
    }

    time_t now = time((time_t*)nullptr);

    for (i = j = 0, lowage = now; i < MAX_NNTP_ARTICLES; i++) {
	if (artnums[i] == artnum) {
	    artages[i] = now;
	    return nntp_tmpname(i);
	}
	if (artages[i] <= lowage)
	{
	    j = i;
	    lowage = artages[j];
	}
    }

    if (allocate) {
	artnums[j] = artnum;
	artages[j] = now;
	return nntp_tmpname(j);
    }

    return nullptr;
}

char *nntp_tmpname(int ndx)
{
    static char artname[20];
    sprintf(artname,"rrn.%ld.%d",g_our_pid,ndx);
    return artname;
}

int nntp_handle_nested_lists()
{
    if (!strcasecmp(g_last_command,"quit"))
	return 0; /*$$ flush data needed? */
    if (nntp_finishbody(FB_DISCARD))
	return 1;
    fprintf(stderr,"Programming error! Nested NNTP calls detected.\n");
    return -1;
}

int nntp_handle_timeout()
{
    static bool handling_timeout = false;
    char last_command_save[NNTP_STRLEN];

    if (!strcasecmp(g_last_command,"quit"))
	return 0;
    if (handling_timeout)
	return -1;
    handling_timeout = true;
    strcpy(last_command_save, g_last_command);
    nntp_close(false);
    g_datasrc->nntplink = g_nntplink;
    if (nntp_connect(g_datasrc->newsid, false) <= 0)
	return -2;
    g_datasrc->nntplink = g_nntplink;
    if (g_in_ng && nntp_group(g_ngname, (NGDATA*)nullptr) <= 0)
	return -2;
    if (nntp_command(last_command_save) <= 0)
	return -1;
    strcpy(g_last_command, last_command_save); /*$$ Is this really needed? */
    handling_timeout = false;
    return 1;
}

void nntp_server_died(DATASRC *dp)
{
    MULTIRC* mp = g_multirc;
    close_datasrc(dp);
    dp->flags |= DF_UNAVAILABLE;
    unuse_multirc(mp);
    if (!use_multirc(mp))
	g_multirc = nullptr;
    fprintf(stderr,"\n%s\n", g_ser_line);
    get_anything();
}

/* nntp_readcheck -- get a line of text from the server, interpreting
** it as a status message for a binary command.  Call this once
** before calling nntp_read() for the actual data transfer.
*/
#ifdef SUPPORT_XTHREAD
long nntp_readcheck()
{
    /* try to get the status line and the status code */
    switch (nntp_check()) {
      case -2:
	return -2;
      case -1:
      case 0:
	return s_rawbytes = -1;
    }

    /* try to get the number of bytes being transfered */
    if (sscanf(g_ser_line, "%*d%ld", &s_rawbytes) != 1)
	return s_rawbytes = -1;
    return s_rawbytes;
}
#endif

/* nntp_read -- read data from the server in binary format.  This call must
** be preceeded by an appropriate binary command and an nntp_readcheck call.
*/
#ifdef SUPPORT_XTHREAD
long nntp_read(char *buf, long n)
{
    /* if no bytes to read, then just return EOF */
    if (s_rawbytes < 0)
	return 0;

#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif

    /* try to read some data from the server */
    if (s_rawbytes) {
	n = fread(buf, 1, n > s_rawbytes ? s_rawbytes : n, g_nntplink.rd_fp);
	s_rawbytes -= n;
    } else
	n = 0;

    /* if no more left, then fetch the end-of-command signature */
    if (!s_rawbytes) {
	char buf[5];	/* "\r\n.\r\n" */

	fread(buf, 1, 5, g_nntplink.rd_fp);
	s_rawbytes = -1;
    }
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    return n;
}
#endif /* SUPPORT_XTHREAD */
