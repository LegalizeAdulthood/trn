/* This file Copyright 1992 by Clifford A. Adams */
/* samisc.c
 *
 * lower-level routines
 */

#include "EXTERN.h"
#include "common.h"
#include "list.h"
#include "hash.h"
#include "cache.h"
#include "artio.h"		/* openart */
#include "bits.h"
#include "final.h"		/* assert's sig_catcher() */
#include "term.h"		/* for "backspace" */
#include "util.h"
#include "scanart.h"
#include "samain.h"
#include "sathread.h"
#include "sorder.h"
#include "trn.h"
#include "ngdata.h"		/* several */
#include "rcstuff.h"
#include "ng.h"			/* for "g_art" */
#include "head.h"		/* fetchsubj */
#include "rthread.h"
#include "rt-select.h"		/* g_sel_mode */
#include "score.h"
#include "INTERN.h"
#include "samisc.h"

#ifdef UNDEF	/* use function for breakpoint debugging */
inline int check_article(long a)
{
    if (a < g_absfirst || a > g_lastart)
    {
        printf("\nArticle %d out of range\n", a) FLUSH;
        return false;
    }
    return true;
}
#else
/* note that argument is used twice. */
inline bool check_article(long a)
{
    return a >= g_absfirst && a <= g_lastart;
}
#endif

/* ignoring "Fold" (or later recursive) mode(s), is this article eligible? */
bool sa_basic_elig(long a)
{
    ART_NUM artnum;

    artnum = sa_ents[a].artnum;
    assert(check_article(artnum));

    /* "run the gauntlet" style (:-) */
    if (!sa_mode_read_elig && was_read(artnum))
	return false;
    if (sa_mode_zoom && !sa_selected1(a))
	return false;
    if (sa_mode_order == 2)	/* score order */
	if (!SCORED(artnum))
	    return false;
    /* now just check availability */
    if (is_unavailable(artnum)) {
	if (!was_read(artnum))
	    oneless_artnum(artnum);
	return false;		/* don't try positively unavailable */
    }
    /* consider later positively checking availability */
    return true;	/* passed all tests */
}

bool sa_eligible(long a)
{

    assert(check_article(sa_ents[a].artnum));
    if (!sa_basic_elig(a))
	return false;		/* must always be basic-eligible */
    if (!sa_mode_fold)
	return true;		/* just use basic-eligible */
    else {
	if (sa_subj_thread_prev(a))
	    return false;	/* there was an earlier match */
	return true;		/* no prior matches */
    }
}

/* given an article number, return the entry number for that article */
/* (There is no easy mapping between entry numbers and article numbers.) */
long sa_artnum_to_ent(ART_NUM artnum)
{
    long i;
    for (i = 1; i < sa_num_ents; i++)
	if (sa_ents[i].artnum == artnum)
	    return i;
    /* this had better not happen (complain?) */
    return -1;
}

/* select1 the articles picked in the TRN thread selector */
void sa_selthreads()
{
    SUBJECT *sp;
    ARTICLE *ap;
    bool want_unread;
#if 0
    /* this does not work now, but maybe it will help debugging? */
    int subj_mask = (g_sel_mode == SM_THREAD? (SF_THREAD|SF_VISIT) : SF_VISIT);
#endif
    int subj_mask = SF_VISIT;

    long art;
    long i;

    want_unread = !sa_mode_read_elig;

    /* clear any old selections */
    for (i = 1; i < sa_num_ents; i++)
	sa_ents[i].sa_flags =
	    (sa_ents[i].sa_flags & 0xfd);

    /* Loop through all (selected) articles. */
    for (sp = g_first_subject; sp; sp = sp->next) {
	if ((sp->flags & subj_mask) == subj_mask) {
	    for (ap = first_art(sp); ap; ap = next_art(ap)) {
		art = article_num(ap);
		if ((ap->flags & AF_SEL)
		 && (!(ap->flags & AF_UNREAD) ^ want_unread)) {
		    /* this was a trn-thread selected article */
		    sa_select1(sa_artnum_to_ent(art));
    /* if scoring, make sure that this article is scored... */
		    if (sa_mode_order == 2)	/* score order */
			sc_score_art(art,false);
		    }
		}/* for all articles */
	    }/* if selected */
	}/* for all threads */
    s_sort();
}

int sa_number_arts()
{
    int total;
    long i;
    ART_NUM a;

    if (sa_mode_read_elig)
	i = g_absfirst;
    else
	i = g_firstart;
    total = 0;
    for (i = 1; i < sa_num_ents; i++) {
	a = sa_ents[i].artnum;
	if (is_unavailable(a))
	    continue;
	if (!article_unread(a) && !sa_mode_read_elig)
	    continue;
	total++;
    }
    return total;
}

/* needed for a couple functions which act within the
 * scope of an article.
 */
void sa_go_art(long a)
{
    g_art = a;
    (void)article_find(g_art);
    if (openart != g_art)
        artopen(g_art,(ART_POS)0);
}

// long a,b;		/* the entry numbers to compare */
int sa_compare(long a, long b)
{
    long i,j;
    
    if (sa_mode_order == 2) {	/* score order */
	/* do not score the articles here--move the articles to
	 * the end of the list if unscored.
	 */
	if (!SCORED(sa_ents[a].artnum)) {	/* a unscored */
	    if (!SCORED(sa_ents[b].artnum)) {	/* a+b unscored */
	        if (a < b)		/* keep ordering consistent */
		    return -1;
		return 1;
	    }
	    return 1;		/* move unscored (a) to end */
	}
	if (!SCORED(sa_ents[b].artnum))	/* only b unscored */
	    return -1;		/* move unscored (b) to end */

	i = sc_score_art(sa_ents[a].artnum,true);
	j = sc_score_art(sa_ents[b].artnum,true);
	if (i < j)
	    return 1;
	if (i > j)
	    return -1;
	/* i == j */
	if (score_newfirst) {
	    if (a < b)
		return 1;
	    return -1;
	} else {
	    if (a < b)
		return -1;
	    return 1;
	}
    }
    if (a < b)
	return -1;
    return 1;
}
