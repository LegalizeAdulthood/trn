// This file Copyright 1992 by Clifford A. Adams
/* samain.c
 *
 * main working routines  (may change later)
 */

#include "config/common.h"
#include "trn/samain.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/cache.h"
#include "trn/sadisp.h"
#include "trn/samisc.h"
#include "trn/sathread.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/scmd.h"
#include "trn/score.h"
#include "trn/sdisp.h"
#include "trn/sorder.h"
#include "trn/spage.h"
#include "trn/util.h"

#include <cstdlib>

bool g_sa_mode_zoom{};      // true if in "zoom" (display only selected) mode
int  g_sa_scan_context{-1}; // contains the scan-context number for the current article scan

static int  s_sa_ents_alloc{};
static bool s_sa_context_init{}; // has context been initialized?

void sa_init()
{
    sa_init_context();
    if (g_last_art == 0 || g_abs_first > g_last_art)
    {
        return;         // no articles
    }
    if (s_init_screen())         // If not able to init screen...
    {
        return;                         // ...most likely dumb terminal
    }
    sa_init_mode();                      // mode differences
    sa_init_threads();
    g_sa_mode_read_elig = false;
    if (g_first_art > g_last_art)         // none unread
    {
        g_sa_mode_read_elig = true;     // unread+read in some situations
    }
    if (!sa_init_arts())                 // init article array(s)
    {
        return;                         // ... no articles
    }
#ifdef PENDING
    if (g_sa_mode_read_elig)
    {
        g_sc_fill_read = true;
        g_sc_fill_max = article_before(g_abs_first);
    }
#endif
    s_save_context();
    g_sa_initialized = true;            // all went well...
}

void sa_init_ents()
{
    g_sa_num_ents = 0;
    s_sa_ents_alloc = 0;
    g_sa_ents = (ScanArticleEntryData*)nullptr;
}

void sa_clean_ents()
{
    std::free(g_sa_ents);
}

// returns entry number that was added
//ART_NUM artnum;               // article number to be added
long sa_add_ent(ArticleNum artnum)
{
    g_sa_num_ents++;
    if (g_sa_num_ents > s_sa_ents_alloc)
    {
        s_sa_ents_alloc += 100;
        if (s_sa_ents_alloc == 100)     // newly allocated
        {
            // don't use number 0, just allocate it and skip it
            g_sa_num_ents = 2;
            g_sa_ents = (ScanArticleEntryData*)safe_malloc(s_sa_ents_alloc
                                        * sizeof (ScanArticleEntryData));
        }
        else
        {
            g_sa_ents = (ScanArticleEntryData*)safe_realloc((char*)g_sa_ents,
                        s_sa_ents_alloc * sizeof (ScanArticleEntryData));
        }
    }
    long cur = g_sa_num_ents - 1;
    g_sa_ents[cur].artnum = artnum;
    g_sa_ents[cur].subj_thread_num = 0;
    g_sa_ents[cur].sa_flags = SAF_NONE;
    s_order_add(cur);
    return cur;
}

void sa_clean_main()
{
    sa_clean_ents();

    g_sa_mode_zoom = false;     // doesn't survive across groups
    // remove the now-unused scan-context
    s_delete_context(g_sa_scan_context);
    s_sa_context_init = false;
    g_sa_scan_context = -1;
    // no longer "in" article scan mode
    g_sa_mode_read_elig = false;        // the default
    g_sa_in = false;
}

void sa_grow_arts(ArticleNum oldlast, ArticleNum last)
{
    for (ArticleNum i{article_after(oldlast)}; i <= last; ++i)
    {
        (void)sa_add_ent(i);
    }
}

// Initialize the scan-context to enter article scan mode.
void sa_init_context()
{
    if (s_sa_context_init)
    {
        return;         // already initialized
    }

    if (g_sa_scan_context == -1)
    {
        g_sa_scan_context = s_new_context(S_ART);
    }
    s_change_context(g_sa_scan_context);
    s_sa_context_init = true;
}

bool sa_init_arts()
{
    sa_init_ents();
    // add all available articles to entry list
    for (ArticleNum a = article_first(g_abs_first); a <= g_last_art; a = article_next(a))
    {
        if (article_exists(a))
        {
            (void)sa_add_ent(a);
        }
    }
    return true;
}

// note: initscreen must be called before (for g_scr_width)
void sa_init_mode()
{
    // set up screen sizes
    sa_set_screen();

    g_sa_mode_zoom = false;                     // reset zoom
}

SaMainResult sa_main_loop()
{
    // Eventually, strn will need a variable in score.[ch] set when the
    // scoring initialization *failed*, so that strn could try to
    // initialize the scoring again or decide to disallow score operations.)
    //
    // If strn is in score mode but scoring is not initialized,
    // then try to initialize.
    // If that fails then strn will just use arrival ordering.
    //
    if (!g_sc_initialized && g_sa_mode_order == SA_ORDER_DESCENDING)
    {
        g_sc_delay = false;     // yes, actually score...
        sc_init(true);          // wait for articles to score
        if (!g_sc_initialized)
        {
            g_sa_mode_order = SA_ORDER_ARRIVAL; // arrival order
        }
    }
    // redraw it *all*
    g_s_ref_all = true;
    if (g_s_top_ent < 1)
    {
        g_s_top_ent = s_first();
    }
    int i = s_fill_page();
    if (i == -1 || i == 0)
    {
        // for now just quit if no page could be filled.
        return SA_QUIT;
    }
    i = s_cmd_loop();
    if (i == SA_READ)
    {
        i = SA_NORM;
    }
    if (i > 0)
    {
        g_sa_art = g_sa_ents[i].artnum;
        return SA_NORM;
    }
    // something else (quit, return, etc...)
    return static_cast<SaMainResult>(i);
}

// do something useful until a key is pressed.
void sa_lookahead()
{
#ifdef PENDING
    sc_look_ahead(true,false);           // do resorting now...
#else // !PENDING
    ;                           // so the function isn't empty
#endif
}

// Returns first marked entry number, or 0 if no articles are marked.
long sa_read_marked_elig()
{
    long e = s_first();
    if (!e)
    {
        return 0L;
    }
    for (; e; e = s_next(e))
    {
        if (!sa_basic_elig(e))
        {
            continue;
        }
        if (sa_marked(e))
        {
            return e;
        }
    }
    // This is possible since the marked articles might not be eligible.
    return 0;
}
