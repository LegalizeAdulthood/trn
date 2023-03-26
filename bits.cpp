/* bits.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "common.h"
#include "bits.h"

#include "cache.h"
#include "datasrc.h"
#include "final.h"
#include "head.h"
#include "kfile.h"
#include "list.h"
#include "ndir.h"
#include "ng.h"
#include "ngdata.h"
#include "nntp.h"
#include "nntpclient.h"
#include "rcln.h"
#include "rcstuff.h"
#include "rt-select.h"
#include "rt-util.h"
#include "rthread.h"
#include "term.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

int g_dmcount{0};

static long s_chase_count{0};

static bool yank_article(char *ptr, int arg);
static bool check_chase(char *ptr, int until_key);
static int chase_xref(ART_NUM artnum, int markread);
#ifdef VALIDATE_XREF_SITE
static bool valid_xref_site(ART_NUM artnum, char *site);
#endif

void bits_init()
{
}

void rc_to_bits()
{
    char* mybuf = g_buf;			/* place to decode rc line */
    char* s;
    char* c;
    char* h;
    ART_NUM unread;
    ARTICLE* ap;

    /* modify the article flags to reflect what has already been read */

    for (s = g_ngptr->rcline + g_ngptr->numoffset; *s == ' '; s++) ;
					/* find numbers in rc line */
    long i = strlen(s);
#ifndef lint
    if (i >= LBUFLEN-2)			/* bigger than g_buf? */
	mybuf = safemalloc((MEM_SIZE)(i+2));
#endif
    strcpy(mybuf,s);			/* make scratch copy of line */
    if (mybuf[0])
	mybuf[i++] = ',';		/* put extra comma on the end */
    mybuf[i] = '\0';
    s = mybuf;				/* initialize the for loop below */
    if (set_firstart(s)) {
	s = strchr(s,',') + 1;
	for (i = article_first(g_absfirst); i < g_firstart; i = article_next(i))
	    article_ptr(i)->flags &= ~AF_UNREAD;
	g_firstart = i;
    }
    else
	g_firstart = article_first(g_firstart);
    unread = 0;
#ifdef DEBUG
    if (debug & DEB_CTLAREA_BITMAP) {
	printf("\n%s\n",mybuf) FLUSH;
	termdown(2);
	for (i = article_first(g_absfirst); i < g_firstart; i = article_next(i)) {
	    if (article_unread(i))
		printf("%ld ",(long)i) FLUSH;
	}
    }
#endif
    i = g_firstart;
    for ( ; (c = strchr(s,',')) != nullptr; s = ++c) {	/* for each range */
	ART_NUM max;
	*c = '\0';			/* do not let index see past comma */
	h = strchr(s,'-');
	ART_NUM min = atol(s);
	if (min < g_firstart)		/* make sure range is in range */
	    min = g_firstart;
	if (min > g_lastart)
	    min = g_lastart+1;
	for (; i < min; i = article_next(i)) {
	    ap = article_ptr(i);
	    if (ap->flags & AF_EXISTS) {
		if (ap->autofl & AUTO_KILLS)
		    ap->flags &= ~AF_UNREAD;
		else {
		    ap->flags |= AF_UNREAD;
		    unread++;
		    if (ap->autofl & AUTO_SELS)
			select_article(ap, ap->autofl);
		}
	    }
	}
	if (!h)
	    max = min;
	else if ((max = atol(h+1)) < min)
	    max = min-1;
	if (max > g_lastart)
	    max = g_lastart;
	/* mark all arts in range as read */
	for ( ; i <= max; i = article_next(i))
	    article_ptr(i)->flags &= ~AF_UNREAD;
#ifdef DEBUG
	if (debug & DEB_CTLAREA_BITMAP) {
	    printf("\n%s\n",s) FLUSH;
	    termdown(2);
	    for (i = g_absfirst; i <= g_lastart; i++) {
		if (!was_read(i))
		    printf("%ld ",(long)i) FLUSH;
	    }
	}
#endif
	i = article_next(max);
    }
    for (; i <= g_lastart; i = article_next(i)) {
	ap = article_ptr(i);
	if (ap->flags & AF_EXISTS) {
	    if (ap->autofl & AUTO_KILLS)
		ap->flags &= ~AF_UNREAD;
	    else {
		ap->flags |= AF_UNREAD;
		unread++;
		if (ap->autofl & AUTO_SELS)
		    select_article(ap, ap->autofl);
	    }
	}
    }
#ifdef DEBUG
    if (debug & DEB_CTLAREA_BITMAP) {
	fputs("\n(hit CR)",stdout) FLUSH;
	termdown(1);
	fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    if (mybuf != g_buf)
	free(mybuf);
    g_ngptr->toread = unread;
}

bool set_firstart(const char *s)
{
    while (*s == ' ') s++;

    if (!strncmp(s,"1-",2)) {		/* can we save some time here? */
	g_firstart = atol(s+2)+1;		/* process first range thusly */
	if (g_firstart < g_absfirst)
	    g_firstart = g_absfirst;
	return true;
    }

    g_firstart = g_absfirst;
    return false;
}

/* reconstruct the .newsrc line in a human readable form */

void bits_to_rc()
{
    char* mybuf = g_buf;
    ART_NUM i;
    ART_NUM count=0;
    int safelen = LBUFLEN - 32;

    strcpy(g_buf,g_ngptr->rcline);            /* start with the newsgroup name */
    char *s = g_buf + g_ngptr->numoffset - 1; /* use s for buffer pointer */
    *s++ = g_ngptr->subscribechar;            /* put the requisite : or !*/
    for (i = article_first(g_absfirst); i <= g_lastart; i = article_next(i)) {
	if (article_unread(i))
	    break;
    }
    sprintf(s," 1-%ld,",(long)i-1);
    s += strlen(s);
    for (; i<=g_lastart; i++) {	/* for each article in newsgroup */
	if (s-mybuf > safelen) {	/* running out of room? */
	    safelen *= 2;
	    if (mybuf == g_buf) {		/* currently static? */
		*s = '\0';
		mybuf = safemalloc((MEM_SIZE)safelen + 32);
		strcpy(mybuf,g_buf);	/* so we must copy it */
		s = mybuf + (s-g_buf);
					/* fix the pointer, too */
	    }
	    else {			/* just grow in place, if possible */
		int oldlen = s - mybuf;
		mybuf = saferealloc(mybuf,(MEM_SIZE)safelen + 32);
		s = mybuf + oldlen;
	    }
	}
	if (!was_read(i))		/* still unread? */
	    count++;			/* then count it */
	else {				/* article was read */

            sprintf(s,"%ld",(long)i); /* put out the min of the range */
	    s += strlen(s);           /* keeping house */
	    ART_NUM oldi = i;         /* remember this spot */
	    do i++; while (i <= g_lastart && was_read(i));
					/* find 1st unread article or end */
	    i--;			/* backup to last read article */
	    if (i > oldi) {		/* range of more than 1? */
		sprintf(s,"-%ld,",(long)i);
					/* then it out as a range */
		s += strlen(s);		/* and housekeep */
	    }
	    else
		*s++ = ',';		/* otherwise, just a comma will do */
	}
    }
    if (*(s-1) == ',')			/* is there a final ','? */
	s--;				/* take it back */
    *s++ = '\0';			/* and terminate string */
#ifdef DEBUG
    if ((debug & DEB_NEWSRC_LINE) && !g_panic) {
	printf("%s: %s\n",g_ngptr->rcline,g_ngptr->rcline+g_ngptr->numoffset) FLUSH;
	printf("%s\n",mybuf) FLUSH;
	termdown(2);
    }
#endif
    free(g_ngptr->rcline);		/* return old rc line */
    if (mybuf == g_buf) {
	g_ngptr->rcline = safemalloc((MEM_SIZE)(s-g_buf)+1);
					/* grab a new rc line */
	strcpy(g_ngptr->rcline, g_buf);	/* and load it */
    }
    else {
	mybuf = saferealloc(mybuf,(MEM_SIZE)(s-mybuf)+1);
					/* be nice to the heap */
	g_ngptr->rcline = mybuf;
    }
    *(g_ngptr->rcline + g_ngptr->numoffset - 1) = '\0';
    if (g_ngptr->subscribechar == NEGCHAR)/* did they unsubscribe? */
	g_ngptr->toread = TR_UNSUB;	/* make line invisible */
    else
	g_ngptr->toread = (ART_UNREAD)count; /* otherwise, remember the count */
    g_ngptr->rc->flags |= RF_RCCHANGED;
}

void find_existing_articles()
{
    ART_NUM an;
    ARTICLE* ap;

    if (g_datasrc->flags & DF_REMOTE) {
	/* Parse the LISTGROUP output and remember everything we find */
	if (/*nntp_rover() ||*/ nntp_artnums()) {
	    /*char* s;*/

	    for (ap = article_ptr(article_first(g_absfirst));
		 ap && article_num(ap) <= g_lastart;
		 ap = article_nextp(ap))
		ap->flags &= ~AF_EXISTS;
	    for (;;) {
		if (nntp_gets(g_ser_line, sizeof g_ser_line) < 0)
		    break; /*$$*/
		if (nntp_at_list_end(g_ser_line))
		    break;
		an = (ART_NUM)atol(g_ser_line);
		if (an < g_absfirst)
		    continue;	/* Ignore some wacked-out NNTP servers */
		ap = article_ptr(an);
		if (!(ap->flags2 & AF2_BOGUS))
		    ap->flags |= AF_EXISTS;
#if 0
		s = strchr(g_ser_line, ' ');
		if (s)
		    rover_thread(article_ptr(an), s);
#endif
	    }
	}
	else if (g_first_subject && g_cached_all_in_range) {
	    if (!g_datasrc->ov_opened || g_datasrc->over_dir != nullptr) {
		for (ap = article_ptr(article_first(g_first_cached));
		     ap && article_num(ap) <= g_last_cached;
		     ap = article_nextp(ap))
		{
		    if (ap->flags & AF_CACHED)
			ap->flags |= AF_EXISTS;
		}
	    }
	    for (an = g_absfirst; an < g_first_cached; an++) {
		ap = article_ptr(an);
		if (!(ap->flags2 & AF2_BOGUS))
		    ap->flags |= AF_EXISTS;
	    }
	    for (an = g_last_cached+1; an <= g_lastart; an++) {
		ap = article_ptr(an);
		if (!(ap->flags2 & AF2_BOGUS))
		    ap->flags |= AF_EXISTS;
	    }
	}
	else {
	    for (an = g_absfirst; an <= g_lastart; an++) {
		ap = article_ptr(an);
		if (!(ap->flags2 & AF2_BOGUS))
		    ap->flags |= AF_EXISTS;
	    }
	}
    }
    else
    {
	ART_NUM first = g_lastart+1;
	ART_NUM last = 0;
	DIR* dirp;
	Direntry_t* dp;
	char ch;
	long lnum;

	/* Scan the directory to find which articles are present. */

	if (!(dirp = opendir(".")))
	    return;

	for (ap = article_ptr(article_first(g_absfirst));
	     ap && article_num(ap) <= g_lastart;
	     ap = article_nextp(ap))
	    ap->flags &= ~AF_EXISTS;

	while ((dp = readdir(dirp)) != nullptr) {
	    if (sscanf(dp->d_name, "%ld%c", &lnum, &ch) == 1) {
		an = (ART_NUM)lnum;
		if (an <= g_lastart && an >= g_absfirst) {
		    if (an < first)
			first = an;
		    if (an > last)
			last = an;
		    ap = article_ptr(an);
		    if (!(ap->flags2 & AF2_BOGUS))
			ap->flags |= AF_EXISTS;
		}
	    }
	}
	closedir(dirp);

	g_ngptr->abs1st = first;
	g_ngptr->ngmax = last;

	if (first > g_absfirst) {
	    checkexpired(g_ngptr,first);
	    for (g_absfirst = article_first(g_absfirst);
		 g_absfirst < first;
		 g_absfirst = article_next(g_absfirst))
	    {
		onemissing(article_ptr(g_absfirst));
	    }
	    g_absfirst = first;
	}
	g_lastart = last;
    }

    if (g_firstart < g_absfirst)
	g_firstart = g_absfirst;
    if (g_firstart > g_lastart)
	g_firstart = g_lastart + 1;
    if (g_first_cached < g_absfirst)
	g_first_cached = g_absfirst;
    if (g_last_cached < g_absfirst)
	g_last_cached = g_absfirst - 1;
}

/* mark an article unread, keeping track of toread[] */

void onemore(ARTICLE *ap)
{
    if (!(ap->flags & AF_UNREAD)) {
	ART_NUM artnum = article_num(ap);
	check_first(artnum);
	ap->flags |= AF_UNREAD;
	ap->flags &= ~AF_DEL;
	g_ngptr->toread++;
	if (ap->subj) {
	    if (g_selected_only) {
		if (ap->subj->flags & g_sel_mask) {
		    ap->flags |= g_sel_mask;
		    g_selected_count++;
		}
	    } else
		ap->subj->flags |= SF_VISIT;
	}
    }
}

/* mark an article read, keeping track of toread[] */

void oneless(ARTICLE *ap)
{
    if (ap->flags & AF_UNREAD) {
	ap->flags &= ~AF_UNREAD;
	/* Keep g_selected_count accurate */
	if (ap->flags & g_sel_mask) {
	    g_selected_count--;
	    ap->flags &= ~g_sel_mask;
	}
	if (g_ngptr->toread > TR_NONE)
	    g_ngptr->toread--;
    }
}

void oneless_artnum(ART_NUM artnum)
{
    ARTICLE* ap = article_find(artnum);
    if (ap)
	oneless(ap);
}

void onemissing(ARTICLE *ap)
{
    g_missing_count += (ap->flags & AF_UNREAD) != 0;
    oneless(ap);
    ap->flags = (ap->flags & ~(AF_HAS_RE|AF_YANKBACK|AF_FROMTRUNCED|AF_EXISTS))
	      | AF_CACHED|AF_THREADED;
}

/* mark an article as unread, with possible xref chasing */

void unmark_as_read(ARTICLE *ap)
{
    onemore(ap);
#ifdef MCHASE
    if (ap->xrefs != "" && !(ap->flags & AF_MCHASE)) {
	ap->flags |= AF_MCHASE;
	s_chase_count++;
    }
#endif
}

/* Mark an article as read in this newsgroup and possibly chase xrefs.
** Don't call this on missing articles.
*/
void set_read(ARTICLE *ap)
{
    oneless(ap);
    if (!g_olden_days && ap->xrefs != "" && !(ap->flags & AF_KCHASE)) {
	ap->flags |= AF_KCHASE;
	s_chase_count++;
    }
}

/* temporarily mark article as read.  When newsgroup is exited, articles */
/* will be marked as unread.  Called via M command */

void delay_unmark(ARTICLE *ap)
{
    if (!(ap->flags & AF_YANKBACK)) {
	ap->flags |= AF_YANKBACK;
	g_dmcount++;
    }
}

/* mark article as read.  If article is cross referenced to other */
/* newsgroups, mark them read there also. */

void mark_as_read(ARTICLE *ap)
{
    oneless(ap);
    if (ap->xrefs != "" && !(ap->flags & AF_KCHASE)) {
	ap->flags |= AF_KCHASE;
	s_chase_count++;
    }
    g_checkcount++;			/* get more worried about crashes */
}

void mark_missing_articles()
{
    for (ARTICLE *ap = article_ptr(article_first(g_absfirst));
         ap && article_num(ap) <= g_lastart;
         ap = article_nextp(ap))
    {
	if (!(ap->flags & AF_EXISTS))
	    onemissing(ap);
    }
}

/* keep g_firstart pointing at the first unread article */

void check_first(ART_NUM min)
{
    if (min < g_absfirst)
	min = g_absfirst;
    if (min < g_firstart)
	g_firstart = min;
}

/* bring back articles marked with M */
void yankback()
{
    if (g_dmcount) {			/* delayed unmarks pending? */
	if (g_panic)
	    ;
	else if (g_general_mode == 's')
	    sprintf(g_msg, "Returned %ld Marked article%s.",(long)g_dmcount,
		plural(g_dmcount));
	else {
	    printf("\nReturning %ld Marked article%s...\n",(long)g_dmcount,
		plural(g_dmcount)) FLUSH;
	    termdown(2);
	}
	article_walk(yank_article, 0);
	g_dmcount = 0;
    }
}

static bool yank_article(char *ptr, int arg)
{
    ARTICLE* ap = (ARTICLE*)ptr;
    if (ap->flags & AF_YANKBACK) {
	unmark_as_read(ap);
	if (g_selected_only)
	    select_article(ap, 0);
	ap->flags &= ~AF_YANKBACK;
    }
    return false;
}

bool chase_xrefs(bool until_key)
{
    if (!s_chase_count)
	return true;
    if (until_key)
	setspin(SPIN_BACKGROUND);

    article_walk(check_chase, until_key);
    s_chase_count = 0;
    return true;
}

static bool check_chase(char *ptr, int until_key)
{
    ARTICLE* ap = (ARTICLE*)ptr;

    if (ap->flags & AF_KCHASE) {
	chase_xref(article_num(ap),true);
	ap->flags &= ~AF_KCHASE;
	if (!--s_chase_count)
	    return true;
    }
#ifdef MCHASE
    if (ap->flags & AF_MCHASE) {
	chase_xref(article_num(ap),true);
	ap->flags &= ~AF_MCHASE;
	if (!--s_chase_count)
	    return 1;
    }
#endif
    if (until_key && input_pending())
	return true;
    return false;
}

/* run down xref list and mark as read or unread */

/* The Xref-line-using version */
static int chase_xref(ART_NUM artnum, int markread)
{
    char* xartnum;
    ART_NUM x;
    char *curxref;
    char tmpbuf[128];

    if (g_datasrc->flags & DF_NOXREFS)
	return 0;

    if (inbackground())
	spin(10);
    else {
	if (g_output_chase_phrase) {
	    if (g_verbose)
		fputs("\nChasing xrefs", stdout);
	    else
		fputs("\nXrefs", stdout);
	    termdown(1);
	    g_output_chase_phrase = false;
	}
	putchar('.');
	fflush(stdout);
    }

    char *xref_buf = fetchcache(artnum, XREF_LINE, FILL_CACHE);
    if (!xref_buf || !*xref_buf)
	return 0;

    xref_buf = savestr(xref_buf);
# ifdef DEBUG
    if (debug & DEB_XREF_MARKER) {
	printf("Xref: %s\n",xref_buf) FLUSH;
	termdown(1);
    }
# endif
    curxref = cpytill(tmpbuf,xref_buf,' ') + 1;
# ifdef VALIDATE_XREF_SITE
    if (valid_xref_site(artnum,tmpbuf))
# endif
    {
	while (*curxref) {		/* for each newsgroup */
	    curxref = cpytill(tmpbuf,curxref,' ');
	    xartnum = strchr(tmpbuf,':');
	    if (!xartnum)
		break;
	    *xartnum++ = '\0';
	    if (!(x = atol(xartnum)))
		continue;
	    if (!strcmp(tmpbuf,g_ngname)) {/* is this the current newsgroup? */
		if (x < g_absfirst || x > g_lastart)
		    continue;
		if (markread)
		    oneless(article_ptr(x)); /* take care of old C newses */
# ifdef MCHASE
		else
		    onemore(article_ptr(x));
# endif
	    } else {
		if (markread) {
		    if (addartnum(g_datasrc,x,tmpbuf))
			break;
		}
# ifdef MCHASE
		else
		    subartnum(g_datasrc,x,tmpbuf);
# endif
	    }
	    while (*curxref && isspace(*curxref))
		curxref++;
	}
    }
    free(xref_buf);
    return 0;
}

/* Make sure the site name on Xref matches what inews thinks the site
 * is.  Check first against last inews_site.  If it matches, fine.
 * If not, fetch inews_site from current Path or Relay-Version line and
 * check again.  This is so that if the new administrator decides
 * to change the system name as known to inews, rn will still do
 * Xrefs correctly--each article need only match itself to be valid.
 */ 
# ifdef VALIDATE_XREF_SITE
static bool valid_xref_site(ART_NUM artnum, char *site)
{
    static char* inews_site = nullptr;
    char* sitebuf;
    char* s;

    if (inews_site && !strcmp(site,inews_site))
	return true;

    if (inews_site)
	free(inews_site);
#ifndef ANCIENT_NEWS
    /* Grab the site from the first component of the Path line */
    sitebuf = fetchlines(artnum,PATH_LINE);
    s = strchr(sitebuf, '!');
    if (s != nullptr)
    {
	*s = '\0';
	inews_site = savestr(sitebuf);
    }
#else /* ANCIENT_NEWS */
    /* Grab the site from the Posting-Version line */
    sitebuf = fetchlines(artnum,RVER_LINE);
    s = instr(sitebuf, "; site ", true);
    if (s != nullptr)
    {
	char* t = strchr(s+7, '.');
	if (t)
	    *t = '\0';
	inews_site = savestr(s+7);
    }
#endif /* ANCIENT_NEWS */
    else
	inews_site = savestr("");
    free(sitebuf);

    if (!strcmp(site,inews_site))
	return true;

#ifdef DEBUG
    if (debug) {
	printf("Xref not from %s -- ignoring\n",inews_site) FLUSH;
	termdown(1);
    }
#endif
    return false;
}
# endif /* VALIDATE_XREF_SITE */
