/* This file Copyright 1992 by Clifford A. Adams */
/* spage.c
 *
 */

#include "config/common.h"
#include "trn/spage.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/cache.h"
#include "trn/rt-util.h" /* spinner */
#include "trn/samain.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/sdisp.h" /* for display dimension sizes */
#include "trn/smisc.h"
#include "trn/sorder.h"

#include <algorithm>
#include <cstdio>

/* returns true if successful */
//long end;             /* entry number to be last on page */
bool s_fillpage_backward(long end)
{
    int min_page_ents;  /* current minimum */
    int  i;
    int  j;
    long a;             /* misc entry number */
    int page_lines;     /* lines available on page */
    int line_on;        /* line # currently on: 0=line after top status bar */

/* Debug */
#if 0
    std::printf("entry: s_fillpage_backward(%d)\n",end);
#endif

    page_lines = g_scr_height - g_s_top_lines - g_s_bot_lines;
    min_page_ents = MAX_PAGE_SIZE-1;
    g_s_bot_ent = -1;   /* none yet */
    line_on = 0;

    /* whatever happens, the entry display will need a full refresh... */
    g_s_ref_status = 0;
    g_s_ref_desc = 0;   /* refresh from top entry */

    if (end < 1)                /* start from end */
    {
        end = s_last();
    }
    if (end == 0)               /* no entries */
    {
        return false;
    }
    if (s_eligible(end))
    {
        a = end;
    }
    else
    {
        a = s_prev_elig(end);
    }
    if (!a)     /* no eligible entries */
    {
        return false;
    }
    /* at this point we *know* that there is at least one eligible */

/* what if the "first" one for the page has a description too long? */
/* later make it shorten the descript. */

/* CONSIDER: make setspin conditional on context? */
    setspin(SPIN_BACKGROUND);   /* turn on spin on cache misses */
    /* uncertain what next comment means now */
    /* later do sheer paranoia check for min_page_ents */
    while ((line_on + s_ent_lines(a)) <= page_lines)
    {
        g_page_ents[min_page_ents].entnum = a;
        i = s_ent_lines(a);
        g_page_ents[min_page_ents].lines = i;
        min_page_ents--;
        g_s_bot_ent += 1;
        line_on = line_on+i;
        a = s_prev_elig(a);
        if (!a)         /* no more eligible */
        {
            break;      /* get out of loop and finish up... */
        }
    }
/* what if none on page? (desc. too long) Fix later */
    setspin(SPIN_POP);  /* turn off spin on cache misses */
    /* replace the entries at the front of the g_page_ents array */
    /* also set start_line entries */
    j = 0;
    line_on = 0;
    for (int k = min_page_ents + 1; k < MAX_PAGE_SIZE; k++)
    {
        g_page_ents[j].entnum = g_page_ents[k].entnum;
        g_page_ents[j].pageflags = (char)0;
        g_page_ents[j].lines = g_page_ents[k].lines;
        g_page_ents[j].start_line = line_on;
        line_on = line_on + g_page_ents[j].lines;
        j++;
    }
    /* set new g_s_top_ent */
    g_s_top_ent = g_page_ents[0].entnum;
    /* Now, suppose that the pointer position is off the page.  That would
     * be bad, so lets make sure it doesn't happen.
     */
    g_s_ptr_page_line = std::min<long>(g_s_ptr_page_line, g_s_bot_ent);
    if (g_s_cur_type != S_ART)
    {
        return true;
    }
    /* temporary fix.  Under some conditions ineligible entries will
     * not be found until they are in the page.  In this case just
     * refill the page.
     */
    for (i = 0; i <= g_s_bot_ent; i++)
    {
        if (is_unavailable(g_sa_ents[g_page_ents[i].entnum].artnum))
        {
            break;
        }
    }
    if (i <= g_s_bot_ent)
    {
        return s_fillpage_backward(end);
    }
/* next time the unavail won't be chosen */
    return true;        /* we have a page... */
}

/* fills the page array */
/* returns true on success */
//long start;                   /* entry to start filling with */
bool s_fillpage_forward(long start)
{
    int i;
    long a;
    int page_lines;     /* lines available on page */
    int line_on;        /* line # currently on (0: line after top status bar */

/* Debug */
#if 0
    std::printf("entry: s_fillpage_forward(%d)\n",start);
#endif

    page_lines = g_scr_height - g_s_top_lines - g_s_bot_lines;
    g_s_bot_ent = -1;
    line_on = 0;

    /* whatever happens, the entry zone will need a full refresh... */
    g_s_ref_status = 0;
    g_s_ref_desc = 0;

    if (start < 0)      /* fill from top */
    {
        start = s_first();
    }
    if (start == 0)     /* no entries */
    {
        return false;
    }

    if (s_eligible(start))
    {
        a = start;
    }
    else
    {
        a = s_next_elig(start);
    }
    if (!a)     /* no eligible entries */
    {
        return false;
    }
    /* at this point we *know* that there is at least one eligible */

/* what if the first entry for the page has a description too long? */
/* later make it shorten the descript. */

    setspin(SPIN_BACKGROUND);   /* turn on spin on cache misses */
/* ?  later do paranoia check for g_s_bot_ent */
    while ((line_on + s_ent_lines(a)) <= page_lines)
    {
        g_s_bot_ent += 1;
        g_page_ents[g_s_bot_ent].entnum = a;
        g_page_ents[g_s_bot_ent].start_line = line_on;
        i = s_ent_lines(a);
        g_page_ents[g_s_bot_ent].lines = i;
        g_page_ents[g_s_bot_ent].pageflags = (char)0;
        line_on = line_on+i;
        a = s_next_elig(a);
        if (!a)         /* no more eligible */
        {
            break;      /* get out of loop and finish up... */
        }
    }
    setspin(SPIN_POP);  /* turn off spin on cache misses */
    /* Now, suppose that the pointer position is off the page.  That would
     * be bad, so lets make sure it doesn't happen.
     */
    /* set new g_s_top_ent */
    g_s_top_ent = g_page_ents[0].entnum;
    g_s_ptr_page_line = std::min<long>(g_s_ptr_page_line, g_s_bot_ent);
    if (g_s_cur_type != S_ART)
    {
        return true;
    }
    /* temporary fix.  Under some conditions ineligible entries will
     * not be found until they are in the page.  In this case just
     * refill the page.
     */
    for (i = 0; i <= g_s_bot_ent; i++)
    {
        if (is_unavailable(g_sa_ents[g_page_ents[i].entnum].artnum))
        {
            break;
        }
    }
    if (i <= g_s_bot_ent)
    {
        return s_fillpage_forward(start);
    }
    /* next time the unavail won't be chosen */
    return true;        /* we have a page... */
}

/* Given possible changes to which entries should be on the page,
 * fills the page with more/less entries.  Tries to minimize refreshing
 * (the last entry on the page will be refreshed whether it needs it
 *  or not.)
 */
/* returns true on success */
bool s_refillpage()
{
    int  i;
    int  j;
    long a;
    int page_lines;     /* lines available on page */
    int line_on;        /* line # currently on: 0=line after top status bar */

/* Debug */
#if 0
    std::printf("entry: s_refillpage\n");
#endif

    page_lines = g_scr_height - g_s_top_lines - g_s_bot_lines;
    /* if the top entry is not the g_s_top_ent,
     *    or the top entry is not eligible,
     *    or the top entry is not on the first line,
     *    or the top entry has a different # of lines now,
     * just refill the whole page.
     */
    if (g_s_bot_ent < 1 || g_s_top_ent < 1
     || g_s_top_ent != g_page_ents[0].entnum || !s_eligible(g_page_ents[0].entnum)
     || g_page_ents[0].start_line != 0
     || g_page_ents[0].lines != s_ent_lines(g_page_ents[0].entnum))
    {
        return s_fillpage_forward(g_s_top_ent);
    }

    i = 1;
    /* misc note: I used to have
     * a = g_page_ents[1];
     * ...at this point.  This caused a truly difficult to track bug...
     * (briefly, occasionally the entry in g_page_ents[1] would be
     *  *before* (by display order) the entry in g_page_ents[0].  In
     *  this case the start_line entry of g_page_ents[0] could be overwritten
     *  causing big problems...)
     */
    a = s_next_elig(g_page_ents[0].entnum);

    /* similar to the tests in the last loop... */
    while (i <= g_s_bot_ent && s_eligible(g_page_ents[i].entnum) //
           && g_page_ents[i].entnum == a                         //
           && g_page_ents[i].lines == s_ent_lines(g_page_ents[i].entnum))
    {
        i++;
        a = s_next_elig(a);
    }
    j = i-1;    /* j is the last "good" entry */

    g_s_bot_ent = j;
    line_on = g_page_ents[j].start_line + s_ent_lines(g_page_ents[j].entnum);
    a = s_next_elig(g_page_ents[j].entnum);

    setspin(SPIN_BACKGROUND);
    while (a && line_on + s_ent_lines(a) <= page_lines)
    {
        i = s_ent_lines(a);
        g_s_bot_ent += 1;
        g_page_ents[g_s_bot_ent].entnum = a;
        g_page_ents[g_s_bot_ent].lines = i;
        g_page_ents[g_s_bot_ent].start_line = line_on;
        g_page_ents[g_s_bot_ent].pageflags = (char)0;
        line_on = line_on+i;
        a = s_next_elig(a);
    }
    setspin(SPIN_POP);

    /* there are fairly good reasons to refresh the last good entry, such
     * as clearing the rest of the screen...
     */
    g_s_ref_status = j;
    g_s_ref_desc = j;

    /* Now, suppose that the pointer position is off the page.  That would
     * be bad, so lets make sure it doesn't happen.
     */
    g_s_ptr_page_line = std::min<long>(g_s_ptr_page_line, g_s_bot_ent);
    if (g_s_cur_type != S_ART)
    {
        return true;
    }
    /* temporary fix.  Under some conditions ineligible entries will
     * not be found until they are in the page.  In this case just
     * refill the page.
     */
    for (i = 0; i <= g_s_bot_ent; i++)
    {
        if (is_unavailable(g_sa_ents[g_page_ents[i].entnum].artnum))
        {
            break;
        }
    }
    if (i <= g_s_bot_ent)
    {
        return s_refillpage();  /* next time the unavail won't be chosen */
    }
    return true;        /* we have a page... */
}

/* fills a page from current position.
 *   if that fails, backs up for a page.
 *     if that fails, downgrade eligibility requirements and try again
 * returns positive # on normal fill,
 *         negative # on special condition
 *         0 on failure
 */
int s_fillpage()
{
    int i;

    g_s_refill = false; /* we don't need one now */
    if (g_s_top_ent < 1)        /* set top to first entry */
    {
        g_s_top_ent = s_first();
    }
    if (g_s_top_ent == 0)       /* no entries */
    {
        return 0;       /* failure */
    }
    if (!s_refillpage())        /* try for efficient refill */
    {
        if (!s_fillpage_backward(g_s_top_ent))
        {
            /* downgrade eligibility standards */
            switch (g_s_cur_type)
            {
            case S_ART:               /* article context */
                if (g_sa_mode_zoom)             /* we were zoomed in */
                {
                    g_s_ref_top = true; /* for "FOLD" display */
                    g_sa_mode_zoom = false;     /* zoom out */
                    if (g_sa_unzoomrefold)
                    {
                        g_sa_mode_fold = true;
                    }
                    (void)s_go_top_ents();      /* go to top (ents and page) */
                    return s_fillpage();
                }
                return -1;              /* there just aren't entries! */

            default:
                return -1;              /* there just aren't entries! */
            } /* switch */
        } /* if */
    }
    /* temporary fix.  Under some conditions ineligible entries will
     * not be found until they are in the page.  In this case just
     * refill the page.
     */
    for (int j = 0; j <= g_s_bot_ent; j++)
    {
        if (!s_eligible(g_page_ents[j].entnum))
        {
            return s_fillpage();  /* ineligible won't be chosen again */
        }
    }
    if (g_s_cur_type != S_ART)
    {
        return 1;
    }
    /* be extra cautious about the article scan pages */
    for (i = 0; i <= g_s_bot_ent; i++)
    {
        if (is_unavailable(g_sa_ents[g_page_ents[i].entnum].artnum))
        {
            break;
        }
    }
    if (i <= g_s_bot_ent)
    {
        return s_fillpage();    /* next time the unavail won't be chosen */
    }
    return 1;   /* I guess everything worked :-) */
}

void s_cleanpage()
{
}

void s_go_top_page()
{
    g_s_ptr_page_line = 0;
}

void s_go_bot_page()
{
    g_s_ptr_page_line = g_s_bot_ent;
}

bool s_go_top_ents()
{
    g_s_top_ent = s_first();
    if (!g_s_top_ent)
    {
        std::printf("s_go_top_ents(): no first entry\n");
    }
    TRN_ASSERT(g_s_top_ent);    /* be nicer later */
    if (!s_eligible(g_s_top_ent))       /* this may save a redraw...*/
    {
        g_s_top_ent = s_next_elig(g_s_top_ent);
    }
    if (!g_s_top_ent)   /* none eligible */
    {
        /* just go to the top of all the entries */
        if (g_s_cur_type == S_ART)
        {
            g_s_top_ent = g_absfirst;
        }
        else
        {
            g_s_top_ent = 1;    /* not very nice coding */
        }
    }
    g_s_refill = true;
    s_go_top_page();
    return true;        /* successful */
}

bool s_go_bot_ents()
{
    bool flag = s_fillpage_backward(s_last()); /* fill backwards */
    if (!flag)
    {
        return false;
    }
    s_go_bot_page();
    return true;
}

void s_go_next_page()
{
    long a = s_next_elig(g_page_ents[g_s_bot_ent].entnum);
    if (!a)
    {
        return;         /* no next page (we shouldn't have been called) */
    }
    /* the fill-page will set the refresh for the screen */
    bool flag = s_fillpage_forward(a);
    TRN_ASSERT(flag);           /* I *must* be able to fill a page */
    g_s_ptr_page_line = 0;      /* top of page */
}

void s_go_prev_page()
{
    long a = s_prev_elig(g_page_ents[0].entnum);
    if (!a)
    {
        return;         /* no prev. page (we shouldn't have been called) */
    }
    /* the fill-page will set the refresh for the screen */
    bool flag = s_fillpage_backward(a); /* fill backward */
    TRN_ASSERT(flag);                   /* be nicer later... */
    /* take care of partially filled previous pages */
    flag = s_refillpage();
    TRN_ASSERT(flag);           /* be nicer later... */
    g_s_ref_status = 0;
    g_s_ref_desc = 0;      /* refresh from top */
    g_s_ptr_page_line = 0; /* top of page */
}
