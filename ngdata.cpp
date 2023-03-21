/* ngdata.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "trn.h"
#include "hash.h"
#include "cache.h"
#include "bits.h"
#include "head.h"
#include "rthread.h"
#include "rt-select.h"
#include "ng.h"
#include "intrp.h"
#include "kfile.h"
#include "final.h"
#include "term.h"
#include "env.h"
#include "util.h"
#include "util2.h"
#include "ndir.h"
#include "score.h"
#include "scan.h"
#include "scanart.h"
#include "nntpclient.h"
#include "datasrc.h"
#include "nntp.h"
#include "rcstuff.h"
#include "rcln.h"
#include "ngdata.h"

#ifdef MSDOS
#include <direct.h>
#include <io.h>
#endif

LIST *g_ngdata_list{}; /* a list of NGDATA */
int g_ngdata_cnt{};
NG_NUM g_newsgroup_cnt{}; /* all newsgroups in our current newsrc(s) */
NG_NUM g_newsgroup_toread{};
ART_UNREAD g_ng_min_toread{1}; /* == TR_ONE or TR_NONE */
NGDATA *g_first_ng{};
NGDATA *g_last_ng{};
NGDATA *g_ngptr{};      /* current newsgroup data ptr */
NGDATA *g_current_ng{}; /* stable current newsgroup so we can ditz with g_ngptr */
NGDATA *g_recent_ng{};  /* the prior newsgroup we visited */
NGDATA *g_starthere{};  /* set to the first newsgroup with unread news on startup */
NGDATA *g_sel_page_np{};
NGDATA *g_sel_next_np{};
ART_NUM g_absfirst{};         /* 1st real article in current newsgroup */
ART_NUM g_firstart{};         /* minimum unread article number in newsgroup */
ART_NUM g_lastart{};          /* maximum article number in newsgroup */
ART_UNREAD g_missing_count{}; /* for reports on missing articles */
char *g_moderated{};
char *g_redirected{};
bool g_threaded_group{};
NGDATA *g_ng_go_ngptr{};
ART_NUM g_ng_go_artnum{};
char *g_ng_go_msgid{};

static int ngorder_number(const NGDATA **npp1, const NGDATA **npp2);
static int ngorder_groupname(const NGDATA **npp1, const NGDATA **npp2);
static int ngorder_count(const NGDATA **npp1, const NGDATA **npp2);

void ngdata_init()
{
}

/* set current newsgroup */

void set_ng(NGDATA *np)
{
    g_ngptr = np;
    if (g_ngptr)
	set_ngname(g_ngptr->rcline);
}

int access_ng()
{
    ART_NUM old_first = g_ngptr->abs1st;

    if (g_datasrc->flags & DF_REMOTE) {
	int ret = nntp_group(ngname,g_ngptr);
	if (ret == -2)
	    return -2;
	if (ret <= 0) {
	    g_ngptr->toread = TR_BOGUS;
	    return 0;
	}
	if ((g_lastart = getngsize(g_ngptr)) < 0) /* Impossible... */
	    return 0;
	g_absfirst = g_ngptr->abs1st;
	if (g_absfirst > old_first)
	    checkexpired(g_ngptr,g_absfirst);
    }
    else
    {
	if (eaccess(ngdir,5)) {		/* directory read protected? */
	    if (eaccess(ngdir,0)) {
		if (verbose)
		    printf("\nNewsgroup %s does not have a spool directory!\n",
			   ngname) FLUSH;
		else
		    printf("\nNo spool for %s!\n",ngname) FLUSH;
		termdown(2);
	    } else {
		if (verbose)
		    printf("\nNewsgroup %s is not currently accessible.\n",
			   ngname) FLUSH;
		else
		    printf("\n%s not readable.\n",ngname) FLUSH;
		termdown(2);
	    }
	    /* make this newsgroup temporarily invisible */
	    g_ngptr->toread = TR_NONE;
	    return 0;
	}

	/* chdir to newsgroup subdirectory */

	if (chdir(ngdir)) {
	    printf(nocd,ngdir) FLUSH;
	    return 0;
	}
	if ((g_lastart = getngsize(g_ngptr)) < 0) /* Impossible... */
	    return 0;
	g_absfirst = g_ngptr->abs1st;
    }

    g_dmcount = 0;
    g_missing_count = 0;
    in_ng = true;			/* tell the world we are here */

    build_cache();
    return 1;
}

void chdir_newsdir()
{
    if (chdir(g_datasrc->spool_dir) || (!(g_datasrc->flags & DF_REMOTE) && chdir(ngdir)))
    {
	printf(nocd,ngdir) FLUSH;
	sig_catcher(0);
    }
}

void grow_ng(ART_NUM newlast)
{
    ART_NUM tmpfirst;

    g_forcegrow = false;
    if (newlast > g_lastart) {
	ART_NUM tmpart = g_art;
	g_ngptr->toread += (ART_UNREAD)(newlast-g_lastart);
	tmpfirst = g_lastart+1;
	/* Increase the size of article scan arrays. */
	sa_grow(g_lastart,newlast);
	do {
	    g_lastart++;
	    article_ptr(g_lastart)->flags |= AF_EXISTS|AF_UNREAD;
	} while (g_lastart < newlast);
	g_article_list->high = g_lastart;
	thread_grow();
	/* Score all new articles now just in case they weren't done above. */
	sc_fill_scorelist(tmpfirst,newlast);
	if (verbose)
	    sprintf(buf,
		"%ld more article%s arrived -- processing memorized commands...\n\n",
		(long)(g_lastart - tmpfirst + 1),
		(g_lastart > tmpfirst ? "s have" : " has" ) );
	else			/* my, my, how clever we are */
	    strcpy(buf, "More news -- auto-processing...\n\n");
	termdown(2);
	if (g_kf_state & KFS_NORMAL_LINES) {
	    bool forcelast_save = g_forcelast;
	    ARTICLE* artp_save = g_artp;
	    kill_unwanted(tmpfirst,buf,true);
	    g_artp = artp_save;
	    g_forcelast = forcelast_save;
	}
	g_art = tmpart;
    }
}

static int ngorder_number(const NGDATA **npp1, const NGDATA **npp2)
{
    return (int)((*npp1)->num - (*npp2)->num) * sel_direction;
}

static int ngorder_groupname(const NGDATA **npp1, const NGDATA **npp2)
{
    return strcasecmp((*npp1)->rcline, (*npp2)->rcline) * sel_direction;
}

static int ngorder_count(const NGDATA **npp1, const NGDATA **npp2)
{
    int eq;
    if ((eq = (int)((*npp1)->toread - (*npp2)->toread)) != 0)
	return eq * sel_direction;
    return (int)((*npp1)->num - (*npp2)->num);
}

/* Sort the newsgroups into the chosen order.
*/
void sort_newsgroups()
{
    NGDATA* np;
    int i;
    NGDATA** lp;
    NGDATA** ng_list;
    int (*sort_procedure)(const NGDATA **npp1,const NGDATA **npp2);

    /* If we don't have at least two newsgroups, we're done! */
    if (!g_first_ng || !g_first_ng->next)
	return;

    switch (sel_sort) {
      case SS_NATURAL:
      default:
	sort_procedure = ngorder_number;
	break;
      case SS_STRING:
	sort_procedure = ngorder_groupname;
	break;
      case SS_COUNT:
	sort_procedure = ngorder_count;
	break;
    }

    ng_list = (NGDATA**)safemalloc(g_newsgroup_cnt * sizeof (NGDATA*));
    for (lp = ng_list, np = g_first_ng; np; np = np->next)
	*lp++ = np;
    assert(lp - ng_list == newsgroup_cnt);

    qsort(ng_list, g_newsgroup_cnt, sizeof (NGDATA*), (int(*)(void const *, void const *))sort_procedure);

    g_first_ng = np = ng_list[0];
    np->prev = nullptr;
    for (i = g_newsgroup_cnt, lp = ng_list; --i; lp++) {
	lp[0]->next = lp[1];
	lp[1]->prev = lp[0];
    }
    g_last_ng = lp[0];
    g_last_ng->next = nullptr;
    free((char*)ng_list);
}

void ng_skip()
{
    if (g_datasrc->flags & DF_REMOTE) {
	ART_NUM artnum;

	clear();
	if (verbose)
	    fputs("Skipping unavailable article\n",stdout);
	else
	    fputs("Skipping\n",stdout);
	termdown(1);
	if (novice_delays) {
	    pad(just_a_sec/3);
	    sleep(1);
	}
	g_art = article_next(g_art);
	g_artp = article_ptr(g_art);
	do {
	    /* tries to grab PREFETCH_SIZE XHDRS, flagging missing articles */
	    (void) fetchsubj(g_art, false);
	    artnum = g_art+PREFETCH_SIZE-1;
	    if (artnum > g_lastart)
		artnum = g_lastart;
	    while (g_art <= artnum) {
		if (g_artp->flags & AF_EXISTS)
		    return;
		g_art = article_next(g_art);
		g_artp = article_ptr(g_art);
	    }
	} while (g_art <= g_lastart);
    }
    else
    {
	if (errno != ENOENT) {	/* has it not been deleted? */
	    clear();
	    if (verbose)
		printf("\n(Article %ld exists but is unreadable.)\n",(long)g_art)
			FLUSH;
	    else
		printf("\n(%ld unreadable.)\n",(long)g_art) FLUSH;
	    termdown(2);
	    if (novice_delays) {
		pad(just_a_sec);
		sleep(2);
	    }
	}
	inc_art(selected_only,false);	/* try next article */
    }
}

/* find the maximum article number of a newsgroup */

ART_NUM getngsize(NGDATA *gp)
{
    int len;
    char* nam;
    char tmpbuf[LBUFLEN];
    long last, first;
    char ch;

    nam = gp->rcline;
    len = gp->numoffset - 1;

    if (!find_actgrp(gp->rc->datasrc,tmpbuf,nam,len,gp->ngmax)) {
	if (gp->subscribechar == ':') {
	    gp->subscribechar = NEGCHAR;
	    gp->rc->flags |= RF_RCCHANGED;
	    g_newsgroup_toread--;
	}
	return TR_BOGUS;
    }

#ifdef ANCIENT_NEWS
    sscanf(tmpbuf+len+1, "%ld %c", &last, &ch);
    first = 1;
#else
    sscanf(tmpbuf+len+1, "%ld %ld %c", &last, &first, &ch);
#endif
    if (!gp->abs1st)
	gp->abs1st = (ART_NUM)first;
    if (!in_ng) {
	if (g_redirected) {
	    if (g_redirected != "")
		free(g_redirected);
	    g_redirected = nullptr;
	}
	switch (ch) {
	case 'n':
	    g_moderated = get_val("NOPOSTRING"," (no posting)");
	    break;
	case 'm':
	    g_moderated = get_val("MODSTRING", " (moderated)");
	    break;
	case 'x':
	    g_redirected = "";
	    g_moderated = " (DISABLED)";
	    break;
	case '=':
	    len = strlen(tmpbuf);
	    if (tmpbuf[len-1] == '\n')
		tmpbuf[len-1] = '\0';
	    g_redirected = savestr(strrchr(tmpbuf, '=') + 1);
	    g_moderated = " (REDIRECTED)";
	    break;
	default:
	    g_moderated = "";
	    break;
	}
    }
    if (last <= gp->ngmax)
	return gp->ngmax;
    return gp->ngmax = (ART_NUM)last;
}
