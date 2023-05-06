/* This file Copyright 1992 by Clifford A. Adams */
/* samain.c
 *
 * main working routines  (may change later)
 */

#include "common.h"
#include "samain.h"

#include "cache.h"
#include "list.h"
#include "ngdata.h"
#include "sadisp.h"
#include "samisc.h"
#include "sathread.h"
#include "scan.h"
#include "scanart.h"
#include "scmd.h"
#include "score.h"
#include "sdisp.h"
#include "sorder.h"
#include "spage.h"
#include "util.h"

bool g_sa_mode_zoom{};      /* true if in "zoom" (display only selected) mode */
int  g_sa_scan_context{-1}; /* contains the scan-context number for the current article scan */

static int s_sa_ents_alloc{};
static bool s_sa_context_init{}; /* has context been initialized? */

void sa_init()
{
    sa_init_context();
    if (g_lastart == 0 || g_absfirst > g_lastart)
	return;		/* no articles */
    if (s_initscreen())		/* If not able to init screen...*/
	return;				/* ...most likely dumb terminal */
    sa_initmode();			/*      mode differences */
    sa_init_threads();
    g_sa_mode_read_elig = false;
    if (g_firstart > g_lastart)		/* none unread */
	g_sa_mode_read_elig = true;	/* unread+read in some situations */
    if (!sa_initarts())			/* init article array(s) */
	return;				/* ... no articles */
#ifdef PENDING
    if (g_sa_mode_read_elig) {
	g_sc_fill_read = true;
	g_sc_fill_max = g_absfirst - 1;
    }
#endif
    s_save_context();
    g_sa_initialized = true;		/* all went well... */
}

void sa_init_ents()
{
    g_sa_num_ents = 0;
    s_sa_ents_alloc = 0;
    g_sa_ents = (SA_ENTRYDATA*)nullptr;
}

void sa_clean_ents()
{
    free(g_sa_ents);
}

/* returns entry number that was added */
//ART_NUM artnum;		/* article number to be added */
long sa_add_ent(ART_NUM artnum)
{
    g_sa_num_ents++;
    if (g_sa_num_ents > s_sa_ents_alloc) {
	s_sa_ents_alloc += 100;
	if (s_sa_ents_alloc == 100) {	/* newly allocated */
	    /* don't use number 0, just allocate it and skip it */
	    g_sa_num_ents = 2;
	    g_sa_ents = (SA_ENTRYDATA*)safemalloc(s_sa_ents_alloc
					* sizeof (SA_ENTRYDATA));
        } else {
	    g_sa_ents = (SA_ENTRYDATA*)saferealloc((char*)g_sa_ents,
			s_sa_ents_alloc * sizeof (SA_ENTRYDATA));
	}
    }
    long cur = g_sa_num_ents - 1;
    g_sa_ents[cur].artnum = artnum;
    g_sa_ents[cur].subj_thread_num = 0;
    g_sa_ents[cur].sa_flags = SAF_NONE;
    s_order_add(cur);
    return cur;
}

void sa_cleanmain()
{
    sa_clean_ents();

    g_sa_mode_zoom = false;	/* doesn't survive across groups */
    /* remove the now-unused scan-context */
    s_delete_context(g_sa_scan_context);
    s_sa_context_init = false;
    g_sa_scan_context = -1;
    /* no longer "in" article scan mode */
    g_sa_mode_read_elig = false;	/* the default */
    g_sa_in = false;
}

void sa_growarts(long oldlast, long last)
{
    for (int i = oldlast + 1; i <= last; i++)
	(void)sa_add_ent(i);
}

/* Initialize the scan-context to enter article scan mode. */
void sa_init_context()
{
    if (s_sa_context_init)
	return;		/* already initialized */

    if (g_sa_scan_context == -1)
	g_sa_scan_context = s_new_context(S_ART);
    s_change_context(g_sa_scan_context);
    s_sa_context_init = true;
}

bool sa_initarts()
{
    sa_init_ents();
    /* add all available articles to entry list */
    for (int a = article_first(g_absfirst); a <= g_lastart; a = article_next(a)) {
	if (article_exists(a))
	    (void)sa_add_ent(a);
    }
    return true;
}

/* note: initscreen must be called before (for g_scr_width) */
void sa_initmode()
{
    /* set up screen sizes */
    sa_set_screen();

    g_sa_mode_zoom = false;			/* reset zoom */
}

sa_main_result sa_mainloop()
{
    /* Eventually, strn will need a variable in score.[ch] set when the
     * scoring initialization *failed*, so that strn could try to
     * initialize the scoring again or decide to disallow score operations.)
     */
    /* If strn is in score mode but scoring is not initialized,
     * then try to initialize.
     * If that fails then strn will just use arrival ordering.
     */
    if (!g_sc_initialized && g_sa_mode_order == SA_ORDER_DESCENDING) {
	g_sc_delay = false;	/* yes, actually score... */
	sc_init(true);		/* wait for articles to score */
	if (!g_sc_initialized)
            g_sa_mode_order = SA_ORDER_ARRIVAL; /* arrival order */
    }
    /* redraw it *all* */
    g_s_ref_all = true;
    if (g_s_top_ent < 1)
	g_s_top_ent = s_first();
    int i = s_fillpage();
    if (i == -1 || i == 0) {
	/* for now just quit if no page could be filled. */
	return SA_QUIT;
    }
    i = s_cmdloop();
    if (i == SA_READ) {
	i = SA_NORM;
    }
    if (i > 0) {
	g_sa_art = g_sa_ents[i].artnum;
	return SA_NORM;
    }
    /* something else (quit, return, etc...) */
    return static_cast<sa_main_result>(i);
}

/* do something useful until a key is pressed. */
void sa_lookahead()
{
#ifdef PENDING
    sc_lookahead(true,false);		/* do resorting now... */
#else /* !PENDING */
    ;				/* so the function isn't empty */
#endif
}

/* Returns first marked entry number, or 0 if no articles are marked. */
long sa_readmarked_elig()
{
    long e = s_first();
    if (!e)
	return 0L;
    for ( ; e; e = s_next(e)) {
	if (!sa_basic_elig(e))
	    continue;
	if (sa_marked(e))
	    return e;
    }
    /* This is possible since the marked articles might not be eligible. */
    return 0;
}
