/* head.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "artio.h"
#include "hash.h"
#include "cache.h"
#include "ng.h"
#include "ngdata.h"
#include "nntpclient.h"
#include "nntpinit.h"
#include "datasrc.h"
#include "nntp.h"
#include "util.h"
#include "util2.h"
#include "rthread.h"
#include "rt-process.h"
#include "rt-util.h"
#include "final.h"
#include "parsedate.h"
#include "mempool.h"
#include "INTERN.h"
#include "head.h"

bool first_one;		/* is this the 1st occurance of this header line? */
bool reading_nntp_header;

static short htypeix[26] =
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void head_init()
{
    int i;

    for (i = HEAD_FIRST+1; i < HEAD_LAST; i++)
	htypeix[*htype[i].name - 'a'] = i;

    user_htype_max = 10;
    user_htype = (USER_HEADTYPE*)safemalloc(user_htype_max
					    * sizeof (USER_HEADTYPE));
    user_htype[user_htype_cnt++].name = "*";

    headbuf_size = LBUFLEN * 8;
    headbuf = safemalloc(headbuf_size);
}

#ifdef DEBUG
void dumpheader(char *where)
{
    register int i;

    printf("header: %ld %s", (long)parsed_art, where);

    for (i = HEAD_FIRST-1; i < HEAD_LAST; i++) {
	printf("%15s %4ld %4ld %03o\n",htype[i].name,
	       (long)htype[i].minpos, (long)htype[i].maxpos,
	       htype[i].flags) FLUSH;
    }
}
#endif

int set_line_type(char *bufptr, char *colon)
{
    char* t;
    char* f;
    int i, len;

    if (colon-bufptr > sizeof msg)
	return SOME_LINE;

    for (t = msg, f = bufptr; f < colon; f++, t++) {
	/* guard against space before : */
	if (isspace(*f))
	    return SOME_LINE;
	*t = isupper(*f) ? tolower(*f) : *f;
    }
    *t = '\0';
    f = msg;				/* get msg into a register */
    len = t - f;

    /* now scan the HEADTYPE table, backwards so we don't have to supply an
     * extra terminating value, using first letter as index, and length as
     * optimization to avoid calling subroutine strEQ unnecessarily.  Hauls.
     */
    if (*f >= 'a' && *f <= 'z') {
	for (i = htypeix[*f - 'a']; *htype[i].name == *f; i--) {
	    if (len == htype[i].length && !strcmp(f,htype[i].name))
		return i;
	}
	if (len == htype[CUSTOM_LINE].length
	 && !strcmp(f,htype[(((0+1)+1)+1)].name))
	    return CUSTOM_LINE;
	for (i = user_htypeix[*f - 'a']; *user_htype[i].name == *f; i--) {
	    if (len >= user_htype[i].length
	     && !strncmp(f, user_htype[i].name, user_htype[i].length)) {
		if (user_htype[i].flags & HT_HIDE)
		    return HIDDEN_LINE;
		return SHOWN_LINE;
	    }
	}
    }
    return SOME_LINE;
}

int get_header_num(char *s)
{
    char* end = s + strlen(s);
    int i;

    i = set_line_type(s, end);	    /* Sets msg to lower-cased header name */

    if (i <= SOME_LINE && i != CUSTOM_LINE) {
	char* bp;
	char ch;
	if (htype[CUSTOM_LINE].name != "")
	    free(htype[CUSTOM_LINE].name);
	htype[CUSTOM_LINE].name = savestr(msg);
	htype[CUSTOM_LINE].length = end - s;
	htype[CUSTOM_LINE].flags = htype[i].flags;
	htype[CUSTOM_LINE].minpos = -1;
	htype[CUSTOM_LINE].maxpos = 0;
	for (bp = headbuf; *bp; bp = end) {
	    if (!(end = strchr(bp,'\n')) || end == bp)
		break;
	    ch = *++end;
	    *end = '\0';
	    s = strchr(bp,':');
	    *end = ch;
	    if (!s || (i = set_line_type(bp,s)) != CUSTOM_LINE)
		continue;
	    htype[CUSTOM_LINE].minpos = bp - headbuf;
	    while (*end == ' ' || *end == '\t') {
		if (!(end = strchr(end, '\n'))) {
		    end = bp + strlen(bp);
		    break;
		}
		end++;
	    }
	    htype[CUSTOM_LINE].maxpos = end - headbuf;
	    break;
	}
	i = CUSTOM_LINE;
    }
    return i;
}

void start_header(ART_NUM artnum)
{
    int i;

#ifdef DEBUG
    if (debug & DEB_HEADER)
	dumpheader("start_header\n");
#endif
    for (i = 0; i < HEAD_LAST; i++) {
	htype[i].minpos = -1;
	htype[i].maxpos = 0;
    }
    in_header = SOME_LINE;
    first_one = false;
    parsed_art = artnum;
    parsed_artp = article_ptr(artnum);
}

void end_header_line()
{
    if (first_one) {		/* did we just pass 1st occurance? */
	first_one = false;
	/* remember where line left off */
	htype[in_header].maxpos = artpos;
	if (htype[in_header].flags & HT_CACHED) {
	    if (!get_cached_line(parsed_artp, in_header, true)) {
		int start = htype[in_header].minpos
			  + htype[in_header].length + 1;
		MEM_SIZE size;
		while (headbuf[start] == ' ' || headbuf[start] == '\t')
		    start++;
		size = artpos - start + 1 - 1;	/* pre-strip newline */
		if (in_header == SUBJ_LINE)
		    set_subj_line(parsed_artp,headbuf+start,size-1);
		else {
		    char* s = safemalloc(size);
		    safecpy(s,headbuf+start,size);
		    set_cached_line(parsed_artp,in_header,s);
		}
	    }
	}
    }
}

bool parseline(char *art_buf, int newhide, int oldhide)
{
    char* s;

    if (*art_buf == ' ' || *art_buf == '\t') /* continuation line? */
	return oldhide;

    end_header_line();
    s = strchr(art_buf,':');
    if (s == nullptr) {	/* is it the end of the header? */
	    /* Did NNTP ship us a mal-formed header line? */
	    if (reading_nntp_header && *art_buf && *art_buf != '\n') {
		in_header = SOME_LINE;
		return newhide;
	    }
	    in_header = PAST_HEADER;
    }
    else {		/* it is a new header line */
	    in_header = set_line_type(art_buf,s);
	    first_one = (htype[in_header].minpos < 0);
	    if (first_one) {
		htype[in_header].minpos = artpos;
		if (in_header == DATE_LINE) {
		    if (!parsed_artp->date)
			parsed_artp->date = parsedate(art_buf+6);
		}
	    }
#ifdef DEBUG
	    if (debug & DEB_HEADER)
		dumpheader(art_buf);
#endif
	    if (htype[in_header].flags & HT_HIDE)
		return newhide;
    }
    return false;			/* don't hide this line */
}

void end_header()
{
    ARTICLE* ap = parsed_artp;

    end_header_line();
    in_header = PAST_HEADER;	/* just to be sure */

    if (!ap->subj)
	set_subj_line(ap,"<NONE>",6);

    if (reading_nntp_header) {
	reading_nntp_header = false;
	htype[PAST_HEADER].minpos = artpos + 1;	/* nntp_body will fix this */
    }
    else
	htype[PAST_HEADER].minpos = tellart();

    /* If there's no References: line, then the In-Reply-To: line may give us
    ** more information.
    */
    if (ThreadedGroup
     && (!(ap->flags & AF_THREADED) || htype[INREPLY_LINE].minpos >= 0)) {
	if (valid_article(ap)) {
	    ARTICLE* artp_hold = artp;
	    char* references = fetchlines(parsed_art, REFS_LINE);
	    char* inreply = fetchlines(parsed_art, INREPLY_LINE);
	    int reflen = strlen(references) + 1;
	    growstr(&references, &reflen, reflen + strlen(inreply) + 1);
	    safecat(references, inreply, reflen);
	    thread_article(ap, references);
	    free(inreply);
	    free(references);
	    artp = artp_hold;
	    check_poster(ap);
	}
    } else if (!(ap->flags & AF_CACHED)) {
	cache_article(ap);
	check_poster(ap);
    }
}

/* read the header into memory and parse it if we haven't already */

bool parseheader(ART_NUM artnum)
{
    char* bp;
    int len;
    bool had_nl = true;
    int found_nl;

    if (parsed_art == artnum)
	return true;
    if (artnum > lastart)
	return false;
    spin(20);
    if (datasrc->flags & DF_REMOTE) {
	char *s = nntp_artname(artnum, false);
	if (s) {
	    if (!artopen(artnum,(ART_POS)0))
		return false;
	}
	else if (nntp_header(artnum) <= 0) {
	    uncache_article(article_ptr(artnum),false);
	    return false;
	}
	else
	    reading_nntp_header = true;
    }
    else if (!artopen(artnum,(ART_POS)0))
	return false;

    start_header(artnum);
    artpos = 0;
    bp = headbuf;
    while (in_header) {
	if (headbuf_size < artpos + LBUFLEN) {
	    len = bp - headbuf;
	    headbuf_size += LBUFLEN * 4;
	    headbuf = saferealloc(headbuf,headbuf_size);
	    bp = headbuf + len;
	}
	if (reading_nntp_header) {
	    found_nl = nntp_gets(bp,LBUFLEN);
	    if (found_nl < 0)
		strcpy(bp,"."); /*$$*/
	    if (had_nl && *bp == '.') {
		if (!bp[1]) {
		    *bp++ = '\n';	/* tag the end with an empty line */
		    break;
		}
		strcpy(bp,bp+1);
	    }
	    len = strlen(bp);
	    if (found_nl)
		bp[len++] = '\n';
	    bp[len] = '\0';
	}
	else
	{
	    if (readart(bp,LBUFLEN) == nullptr)
		break;
	    len = strlen(bp);
	    found_nl = (bp[len-1] == '\n');
	}
	if (had_nl)
	    parseline(bp,false,false);
	had_nl = found_nl;
	artpos += len;
	bp += len;
    }
    *bp = '\0';
    end_header();
    return true;
}

/* get a header line from an article */

/* article to get line from */
/* type of line desired */
char *fetchlines(ART_NUM artnum, int which_line)
{
    char* s;
    char* t;
    ART_POS firstpos;
    ART_POS lastpos;
    int size;

    /* Only return a cached line if it isn't the current article */
    if (parsed_art != artnum) {
	/* If the line is not in the cache, this will parse the header */
	s = fetchcache(artnum,which_line, FILL_CACHE);
	if (s)
	    return savestr(s);
    }
    if ((firstpos = htype[which_line].minpos) < 0)
	return savestr("");

    firstpos += htype[which_line].length + 1;
    lastpos = htype[which_line].maxpos;
    size = lastpos - firstpos;
    t = headbuf + firstpos;
    while (*t == ' ' || *t == '\t') t++, size--;
#ifdef DEBUG
    if (debug && (size < 1 || size > 1000)) {
	printf("Firstpos = %ld, lastpos = %ld\n",(long)firstpos,(long)lastpos);
	fgets(cmd_buf, sizeof cmd_buf, stdin);
    }
#endif
    s = safemalloc((MEM_SIZE)size);
    safecpy(s,t,size);
    return s;
}

/* (strn) like fetchlines, but for memory pools */
/* ART_NUM artnum   article to get line from */
/* int which_line   type of line desired */
/* int pool         which memory pool to use */
char *mp_fetchlines(ART_NUM artnum, int which_line, int pool)
{
    char* s;
    char* t;
    ART_POS firstpos;
    ART_POS lastpos;
    int size;

    /* Only return a cached line if it isn't the current article */
    if (parsed_art != artnum) {
	/* If the line is not in the cache, this will parse the header */
	s = fetchcache(artnum,which_line, FILL_CACHE);
	if (s)
	    return mp_savestr(s,pool);
    }
    if ((firstpos = htype[which_line].minpos) < 0)
	return mp_savestr("",pool);

    firstpos += htype[which_line].length + 1;
    lastpos = htype[which_line].maxpos;
    size = lastpos - firstpos;
    t = headbuf + firstpos;
    while (*t == ' ' || *t == '\t') t++, size--;
#ifdef DEBUG
    if (debug && (size < 1 || size > 1000)) {
	printf("Firstpos = %ld, lastpos = %ld\n",(long)firstpos,(long)lastpos);
	fgets(cmd_buf, sizeof cmd_buf, stdin);
    }
#endif
    s = mp_malloc(size,pool);
    safecpy(s,t,size);
    return s;
}

/* prefetch a header line from one or more articles */

// ART_NUM artnum   article to get line from */
// int which_line   type of line desired */
// bool copy    do you want it savestr()ed? */
char *prefetchlines(ART_NUM artnum, int which_line, bool copy)
{
    char* s;
    char* t;
    ART_POS firstpos;
    ART_POS lastpos;
    int size;

    if ((datasrc->flags & DF_REMOTE) && parsed_art != artnum) {
	ARTICLE* ap;
	int size;
	ART_NUM num, priornum, lastnum;
	bool cached;
 	bool hasxhdr = true;

	s = fetchcache(artnum,which_line, DONT_FILL_CACHE);
	if (s) {
	    if (copy)
		s = savestr(s);
	    return s;
	}

	spin(20);
	if (copy)
	    s = safemalloc((MEM_SIZE)(size = LBUFLEN));
	else {
	    s = cmd_buf;
	    size = sizeof cmd_buf;
	}
	*s = '\0';
	priornum = artnum-1;
	if ((cached = (htype[which_line].flags & HT_CACHED)) != 0) {
	    lastnum = artnum + PREFETCH_SIZE - 1;
	    if (lastnum > lastart)
		lastnum = lastart;
	    sprintf(g_ser_line,"XHDR %s %ld-%ld",htype[which_line].name,
		artnum,lastnum);
	} else {
	    lastnum = artnum;
	    sprintf(g_ser_line,"XHDR %s %ld",htype[which_line].name,artnum);
	}
	if (nntp_command(g_ser_line) <= 0)
	    finalize(1); /*$$*/
	if (nntp_check() > 0) {
	    char* line;
	    char* last_buf = g_ser_line;
	    MEM_SIZE last_buflen = sizeof g_ser_line;
	    for (;;) {
		line = nntp_get_a_line(last_buf,last_buflen,last_buf!=g_ser_line);
# ifdef DEBUG
		if (debug & DEB_NNTP)
		    printf("<%s", line? line : "<EOF>") FLUSH;
# endif
		if (nntp_at_list_end(line))
		    break;
		last_buf = line;
		last_buflen = buflen_last_line_got;
		if ((t = strchr(line, '\r')) != nullptr)
		    *t = '\0';
		if (!(t = strchr(line, ' ')))
		    continue;
		t++;
		num = atol(line);
		if (num < artnum || num > lastnum)
		    continue;
		if (!(datasrc->flags & DF_XHDR_BROKEN)) {
		    while ((priornum = article_next(priornum)) < num)
			uncache_article(article_ptr(priornum),false);
		}
		ap = article_find(num);
		if (which_line == SUBJ_LINE)
		    set_subj_line(ap, t, strlen(t));
		else if (cached)
		    set_cached_line(ap, which_line, savestr(t));
		if (num == artnum)
		    safecat(s,t,size);
	    }
	    if (last_buf != g_ser_line)
		free(last_buf);
	} else {
	    hasxhdr = false;
	    lastnum = artnum;
	    if (!parseheader(artnum)) {
	        fprintf(stderr,"\nBad NNTP response.\n");
		finalize(1);
	    }
	    s = fetchlines(artnum,which_line);
	}
	if (hasxhdr && !(datasrc->flags & DF_XHDR_BROKEN)) {
	    for (priornum = article_first(priornum); priornum < lastnum; priornum = article_next(priornum))
		uncache_article(article_ptr(priornum),false);
	}
	if (copy)
	    s = saferealloc(s, (MEM_SIZE)strlen(s)+1);
	return s;
    }

    /* Only return a cached line if it isn't the current article */
    s = nullptr;
    if (parsed_art != artnum)
	s = fetchcache(artnum,which_line, FILL_CACHE);
    if (parsed_art == artnum && (firstpos = htype[which_line].minpos) < 0)
	s = "";
    if (s) {
	if (copy)
	    s = savestr(s);
	return s;
    }

    firstpos += htype[which_line].length + 1;
    lastpos = htype[which_line].maxpos;
    size = lastpos - firstpos;
    t = headbuf + firstpos;
    while (*t == ' ' || *t == '\t') t++, size--;
    if (copy)
	s = safemalloc((MEM_SIZE)size);
    else {				/* hope this is okay--we're */
	s = cmd_buf;			/* really scraping for space here */
	if (size > sizeof cmd_buf)
	    size = sizeof cmd_buf;
    }
    safecpy(s,t,size);
    return s;
}
