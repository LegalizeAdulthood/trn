/* spage.cpp
 *
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/spage.h>

#include <config/common.h>
#include <trn/ngdata.h>
#include <trn/rt-util.h> // spinner
#include <trn/samain.h>
#include <trn/scan.h>
#include <trn/scanart.h>
#include <trn/sdisp.h> // for display dimension sizes
#include <trn/size_cast.h>
#include <trn/smisc.h>
#include <trn/sorder.h>

#include <fmt/format.h>

#include <algorithm>
#include <cstdio>

static void s_clean_page();

// returns true if successful
//long end;             // entry number to be last on page
bool s_fill_page_backward(long end)
{
    int  i;
    long a;             // misc entry number
    int  page_lines;    // lines available on page
    int  line_on;       // line # currently on: 0=line after top status bar

// Debug
#if 0
    fmt::print("entry: s_fillpage_backward({})\n", end);
#endif

    page_lines = g_scr_height - g_art_scan.top_lines - g_art_scan.bot_lines;
    g_art_scan.page_ents.clear();
    if (page_lines > 0)
    {
        g_art_scan.page_ents.reserve(page_lines);
    }
    g_art_scan.bot_ent = -1; // none yet
    line_on = 0;

    // whatever happens, the entry display will need a full refresh...
    g_art_scan.ref_status = 0;
    g_art_scan.ref_desc = 0; // refresh from top entry

    if (end < 1)                // start from end
    {
        end = g_art_scan.last();
    }
    if (end == 0)               // no entries
    {
        return false;
    }
    if (s_eligible(end))
    {
        a = end;
    }
    else
    {
        a = g_art_scan.prev_elig(end);
    }
    if (!a)     // no eligible entries
    {
        return false;
    }
    // at this point we *know* that there is at least one eligible

// what if the "first" one for the page has a description too long?
// later make it shorten the descript.

// CONSIDER: make setspin conditional on context?
    g_progress.set_spin(SPIN_BACKGROUND); // turn on spin on cache misses
    while ((line_on + s_ent_lines(a)) <= page_lines)
    {
        i = s_ent_lines(a);
        g_art_scan.page_ents.push_back({a, static_cast<short>(i), 0, (char) 0});
        line_on = line_on+i;
        a = g_art_scan.prev_elig(a);
        if (!a)         // no more eligible
        {
            break;      // get out of loop and finish up...
        }
    }
// what if none on page? (desc. too long) Fix later
    g_progress.set_spin(SPIN_POP); // turn off spin on cache misses
    if (g_art_scan.page_ents.empty())
    {
        return false;
    }
    std::reverse(g_art_scan.page_ents.begin(), g_art_scan.page_ents.end());
    line_on = 0;
    for (PageEntry &entry : g_art_scan.page_ents)
    {
        entry.page_flags = (char)0;
        entry.start_line = static_cast<short>(line_on);
        line_on = line_on + entry.lines;
    }
    g_art_scan.bot_ent = size_cast<long>(g_art_scan.page_ents) - 1;
    // set new g_art_scan.top_ent
    g_art_scan.top_ent = g_art_scan.page_ents[0].ent_num;
    // Now, suppose that the pointer position is off the page.  That would
    // be bad, so lets make sure it doesn't happen.
    //
    g_art_scan.ptr_page_line = std::min<long>(g_art_scan.ptr_page_line, g_art_scan.bot_ent);
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    if (g_art_scan.page_has_unavailable_entries())
    {
        return s_fill_page_backward(end);
    }
// next time the unavail won't be chosen
    return true;        // we have a page...
}

// fills the page array
// returns true on success
//long start;                   // entry to start filling with
bool s_fill_page_forward(long start)
{
    int i;
    long a;
    int  page_lines; // lines available on page
    int  line_on;    // line # currently on (0: line after top status bar

// Debug
#if 0
    fmt::print("entry: s_fillpage_forward({})\n", start);
#endif

    page_lines = g_scr_height - g_art_scan.top_lines - g_art_scan.bot_lines;
    g_art_scan.page_ents.clear();
    if (page_lines > 0)
    {
        g_art_scan.page_ents.reserve(page_lines);
    }
    g_art_scan.bot_ent = -1;
    line_on = 0;

    // whatever happens, the entry zone will need a full refresh...
    g_art_scan.ref_status = 0;
    g_art_scan.ref_desc = 0;

    if (start < 0)      // fill from top
    {
        start = g_art_scan.first();
    }
    if (start == 0)     // no entries
    {
        return false;
    }

    if (s_eligible(start))
    {
        a = start;
    }
    else
    {
        a = g_art_scan.next_elig(start);
    }
    if (!a)     // no eligible entries
    {
        return false;
    }
    // at this point we *know* that there is at least one eligible

// what if the first entry for the page has a description too long?
// later make it shorten the descript.

    g_progress.set_spin(SPIN_BACKGROUND); // turn on spin on cache misses
                                          // ?  later do paranoia check for g_art_scan.bot_ent
    while ((line_on + s_ent_lines(a)) <= page_lines)
    {
        i = s_ent_lines(a);
        g_art_scan.page_ents.push_back({a, static_cast<short>(i), static_cast<short>(line_on), (char) 0});
        line_on = line_on+i;
        a = g_art_scan.next_elig(a);
        if (!a)         // no more eligible
        {
            break;      // get out of loop and finish up...
        }
    }
    g_progress.set_spin(SPIN_POP); // turn off spin on cache misses
    if (g_art_scan.page_ents.empty())
    {
        return false;
    }
    g_art_scan.bot_ent = size_cast<long>(g_art_scan.page_ents) - 1;
    // Now, suppose that the pointer position is off the page.  That would
    // be bad, so lets make sure it doesn't happen.
    //
    // set new g_art_scan.top_ent
    g_art_scan.top_ent = g_art_scan.page_ents[0].ent_num;
    g_art_scan.ptr_page_line = std::min<long>(g_art_scan.ptr_page_line, g_art_scan.bot_ent);
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    if (g_art_scan.page_has_unavailable_entries())
    {
        return s_fill_page_forward(start);
    }
    // next time the unavail won't be chosen
    return true;        // we have a page...
}

// Given possible changes to which entries should be on the page,
// fills the page with more/less entries.  Tries to minimize refreshing
// (the last entry on the page will be refreshed whether it needs it
//  or not.)
//
// returns true on success
bool s_refill_page()
{
    int  i;
    int  j;
    long a;
    int  page_lines; // lines available on page
    int  line_on;    // line # currently on: 0=line after top status bar

// Debug
#if 0
    fmt::print("entry: s_refillpage\n");
#endif

    page_lines = g_scr_height - g_art_scan.top_lines - g_art_scan.bot_lines;
    // if the top entry is not the g_art_scan.top_ent,
    //    or the top entry is not eligible,
    //    or the top entry is not on the first line,
    //    or the top entry has a different # of lines now,
    // just refill the whole page.
    //
    if (g_art_scan.bot_ent < 1 || g_art_scan.top_ent < 1 || g_art_scan.top_ent != g_art_scan.page_ents[0].ent_num ||
        !s_eligible(g_art_scan.page_ents[0].ent_num) || g_art_scan.page_ents[0].start_line != 0 ||
        g_art_scan.page_ents[0].lines != s_ent_lines(g_art_scan.page_ents[0].ent_num))
    {
        return s_fill_page_forward(g_art_scan.top_ent);
    }

    i = 1;
    // misc note: I used to have
    // a = g_art_scan.page_ents[1];
    // ...at this point.  This caused a truly difficult to track bug...
    // (briefly, occasionally the entry in g_art_scan.page_ents[1] would be
    //  *before* (by display order) the entry in g_art_scan.page_ents[0].  In
    //  this case the start_line entry of g_art_scan.page_ents[0] could be overwritten
    //  causing big problems...)
    //
    a = g_art_scan.next_elig(g_art_scan.page_ents[0].ent_num);

    // similar to the tests in the last loop...
    while (i <= g_art_scan.bot_ent && s_eligible(g_art_scan.page_ents[i].ent_num) //
           && g_art_scan.page_ents[i].ent_num == a                                //
           && g_art_scan.page_ents[i].lines == s_ent_lines(g_art_scan.page_ents[i].ent_num))
    {
        i++;
        a = g_art_scan.next_elig(a);
    }
    j = i-1;    // j is the last "good" entry

    g_art_scan.bot_ent = j;
    g_art_scan.page_ents.resize(j + 1);
    line_on = g_art_scan.page_ents[j].start_line + s_ent_lines(g_art_scan.page_ents[j].ent_num);
    a = g_art_scan.next_elig(g_art_scan.page_ents[j].ent_num);
    if (page_lines > 0)
    {
        g_art_scan.page_ents.reserve(page_lines);
    }

    g_progress.set_spin(SPIN_BACKGROUND);
    while (a && line_on + s_ent_lines(a) <= page_lines)
    {
        i = s_ent_lines(a);
        g_art_scan.page_ents.push_back({a, static_cast<short>(i), static_cast<short>(line_on), (char) 0});
        line_on = line_on+i;
        a = g_art_scan.next_elig(a);
    }
    g_progress.set_spin(SPIN_POP);
    g_art_scan.bot_ent = size_cast<long>(g_art_scan.page_ents) - 1;

    // there are fairly good reasons to refresh the last good entry, such
    // as clearing the rest of the screen...
    //
    g_art_scan.ref_status = j;
    g_art_scan.ref_desc = j;

    // Now, suppose that the pointer position is off the page.  That would
    // be bad, so lets make sure it doesn't happen.
    //
    g_art_scan.ptr_page_line = std::min<long>(g_art_scan.ptr_page_line, g_art_scan.bot_ent);
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    if (g_art_scan.page_has_unavailable_entries())
    {
        return s_refill_page();  // next time the unavail won't be chosen
    }
    return true;        // we have a page...
}

/* fills a page from current position.
 *   if that fails, backs up for a page.
 *     if that fails, downgrade eligibility requirements and try again
 * returns positive # on normal fill,
 *         negative # on special condition
 *         0 on failure
 */
int s_fill_page()
{
    g_art_scan.refill = false;  // we don't need one now
    if (g_art_scan.top_ent < 1) // set top to first entry
    {
        g_art_scan.top_ent = g_art_scan.first();
    }
    if (g_art_scan.top_ent == 0) // no entries
    {
        return 0;       // failure
    }
    if (!s_refill_page())        // try for efficient refill
    {
        if (!s_fill_page_backward(g_art_scan.top_ent))
        {
            // downgrade eligibility standards
            switch (g_art_scan.cur_type)
            {
            case S_ART:               // article context
                if (g_sa_mode_zoom)             // we were zoomed in
                {
                    g_art_scan.ref_top = true;  // for "FOLD" display
                    g_sa_mode_zoom = false;     // zoom out
                    if (g_sa_unzoom_refold)
                    {
                        g_sa_mode_fold = true;
                    }
                    (void)s_go_top_ents();      // go to top (ents and page)
                    return s_fill_page();
                }
                return -1;              // there just aren't entries!

            default:
                return -1;              // there just aren't entries!
            } // switch
        } // if
    }
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    for (int j = 0; j <= g_art_scan.bot_ent; j++)
    {
        if (!s_eligible(g_art_scan.page_ents[j].ent_num))
        {
            return s_fill_page();  // ineligible won't be chosen again
        }
    }
    // be extra cautious about the article scan pages
    if (g_art_scan.page_has_unavailable_entries())
    {
        return s_fill_page();    // next time the unavail won't be chosen
    }
    return 1;   // I guess everything worked :-)
}

static void s_clean_page()
{
}

void ArticleScanSession::go_top_page()
{
    ptr_page_line = 0;
}

void ArticleScanSession::go_bot_page()
{
    ptr_page_line = bot_ent;
}

bool s_go_top_ents()
{
    g_art_scan.top_ent = g_art_scan.first();
    if (!g_art_scan.top_ent)
    {
        fmt::print("s_go_top_ents(): no first entry\n");
    }
    TRN_ASSERT(g_art_scan.top_ent);      // be nicer later
    if (!s_eligible(g_art_scan.top_ent)) // this may save a redraw...
    {
        g_art_scan.top_ent = g_art_scan.next_elig(g_art_scan.top_ent);
    }
    if (!g_art_scan.top_ent) // none eligible
    {
        // just go to the top of all the entries
        if (g_art_scan.cur_type == S_ART)
        {
            g_art_scan.top_ent = g_abs_first.value_of();
        }
        else
        {
            g_art_scan.top_ent = 1; // not very nice coding
        }
    }
    g_art_scan.refill = true;
    g_art_scan.go_top_page();
    return true;        // successful
}

bool s_go_bot_ents()
{
    bool flag = s_fill_page_backward(g_art_scan.last()); // fill backwards
    if (!flag)
    {
        return false;
    }
    g_art_scan.go_bot_page();
    return true;
}

void s_go_next_page()
{
    long a = g_art_scan.next_elig(g_art_scan.page_ents[g_art_scan.bot_ent].ent_num);
    if (!a)
    {
        return;         // no next page (we shouldn't have been called)
    }
    // the fill-page will set the refresh for the screen
    bool flag = s_fill_page_forward(a);
    TRN_ASSERT(flag);           // I *must* be able to fill a page
    g_art_scan.ptr_page_line = 0; // top of page
}

void s_go_prev_page()
{
    long a = g_art_scan.prev_elig(g_art_scan.page_ents[0].ent_num);
    if (!a)
    {
        return;         // no prev. page (we shouldn't have been called)
    }
    // the fill-page will set the refresh for the screen
    bool flag = s_fill_page_backward(a); // fill backward
    TRN_ASSERT(flag);                   // be nicer later...
    // take care of partially filled previous pages
    flag = s_refill_page();
    TRN_ASSERT(flag);           // be nicer later...
    g_art_scan.ref_status = 0;
    g_art_scan.ref_desc = 0;      // refresh from top
    g_art_scan.ptr_page_line = 0; // top of page
}
