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

int ArticleScanSession::find_on_page(long ent, ScanPageSearch scope) const
{
    const int step = scope == ScanPageSearch::BEFORE_POINTER ? -1 : 1;
    const int first = scope == ScanPageSearch::WHOLE_PAGE ? 0 : ptr_page_line + step;
    for (int i = first; i >= 0 && i <= bot_ent; i += step)
    {
        if (page_ents[i].ent_num == ent)
        {
            return i;
        }
    }
    return -1;
}

// returns true if successful
//long end;             // entry number to be last on page
bool ArticleScanSession::fill_page_backward(long end)
{
    int  i;
    long a;             // misc entry number
    int  page_lines;    // lines available on page
    int  line_on;       // line # currently on: 0=line after top status bar

// Debug
#if 0
    fmt::print("entry: s_fillpage_backward({})\n", end);
#endif

    page_lines = g_scr_height - top_lines - bot_lines;
    page_ents.clear();
    if (page_lines > 0)
    {
        page_ents.reserve(page_lines);
    }
    bot_ent = -1; // none yet
    line_on = 0;

    // whatever happens, the entry display will need a full refresh...
    ref_status = 0;
    ref_desc = 0; // refresh from top entry

    if (end < 1)                // start from end
    {
        end = last();
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
        a = prev_elig(end);
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
        page_ents.push_back({a, static_cast<short>(i), 0, (char) 0});
        line_on = line_on+i;
        a = prev_elig(a);
        if (!a)         // no more eligible
        {
            break;      // get out of loop and finish up...
        }
    }
// what if none on page? (desc. too long) Fix later
    g_progress.set_spin(SPIN_POP); // turn off spin on cache misses
    if (page_ents.empty())
    {
        return false;
    }
    std::reverse(page_ents.begin(), page_ents.end());
    line_on = 0;
    for (PageEntry &entry : page_ents)
    {
        entry.page_flags = (char)0;
        entry.start_line = static_cast<short>(line_on);
        line_on = line_on + entry.lines;
    }
    bot_ent = size_cast<long>(page_ents) - 1;
    // set new top_ent
    top_ent = page_ents[0].ent_num;
    // Now, suppose that the pointer position is off the page.  That would
    // be bad, so lets make sure it doesn't happen.
    //
    ptr_page_line = std::min<long>(ptr_page_line, bot_ent);
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    if (page_has_unavailable_entries())
    {
        return fill_page_backward(end);
    }
// next time the unavail won't be chosen
    return true;        // we have a page...
}

// fills the page array
// returns true on success
//long start;                   // entry to start filling with
bool ArticleScanSession::fill_page_forward(long start)
{
    int i;
    long a;
    int  page_lines; // lines available on page
    int  line_on;    // line # currently on (0: line after top status bar

// Debug
#if 0
    fmt::print("entry: s_fillpage_forward({})\n", start);
#endif

    page_lines = g_scr_height - top_lines - bot_lines;
    page_ents.clear();
    if (page_lines > 0)
    {
        page_ents.reserve(page_lines);
    }
    bot_ent = -1;
    line_on = 0;

    // whatever happens, the entry zone will need a full refresh...
    ref_status = 0;
    ref_desc = 0;

    if (start < 0)      // fill from top
    {
        start = first();
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
        a = next_elig(start);
    }
    if (!a)     // no eligible entries
    {
        return false;
    }
    // at this point we *know* that there is at least one eligible

// what if the first entry for the page has a description too long?
// later make it shorten the descript.

    g_progress.set_spin(SPIN_BACKGROUND); // turn on spin on cache misses
                                          // ?  later do paranoia check for bot_ent
    while ((line_on + s_ent_lines(a)) <= page_lines)
    {
        i = s_ent_lines(a);
        page_ents.push_back({a, static_cast<short>(i), static_cast<short>(line_on), (char) 0});
        line_on = line_on+i;
        a = next_elig(a);
        if (!a)         // no more eligible
        {
            break;      // get out of loop and finish up...
        }
    }
    g_progress.set_spin(SPIN_POP); // turn off spin on cache misses
    if (page_ents.empty())
    {
        return false;
    }
    bot_ent = size_cast<long>(page_ents) - 1;
    // Now, suppose that the pointer position is off the page.  That would
    // be bad, so lets make sure it doesn't happen.
    //
    // set new top_ent
    top_ent = page_ents[0].ent_num;
    ptr_page_line = std::min<long>(ptr_page_line, bot_ent);
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    if (page_has_unavailable_entries())
    {
        return fill_page_forward(start);
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
bool ArticleScanSession::refill_page()
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

    page_lines = g_scr_height - top_lines - bot_lines;
    // if the top entry is not the top_ent,
    //    or the top entry is not eligible,
    //    or the top entry is not on the first line,
    //    or the top entry has a different # of lines now,
    // just refill the whole page.
    //
    if (bot_ent < 1 || top_ent < 1 || top_ent != page_ents[0].ent_num || !s_eligible(page_ents[0].ent_num) ||
        page_ents[0].start_line != 0 || page_ents[0].lines != s_ent_lines(page_ents[0].ent_num))
    {
        return fill_page_forward(top_ent);
    }

    i = 1;
    // misc note: I used to have
    // a = page_ents[1];
    // ...at this point.  This caused a truly difficult to track bug...
    // (briefly, occasionally the entry in page_ents[1] would be
    //  *before* (by display order) the entry in page_ents[0].  In
    //  this case the start_line entry of page_ents[0] could be overwritten
    //  causing big problems...)
    //
    a = next_elig(page_ents[0].ent_num);

    // similar to the tests in the last loop...
    while (i <= bot_ent && s_eligible(page_ents[i].ent_num) //
           && page_ents[i].ent_num == a                     //
           && page_ents[i].lines == s_ent_lines(page_ents[i].ent_num))
    {
        i++;
        a = next_elig(a);
    }
    j = i-1;    // j is the last "good" entry

    bot_ent = j;
    page_ents.resize(j + 1);
    line_on = page_ents[j].start_line + s_ent_lines(page_ents[j].ent_num);
    a = next_elig(page_ents[j].ent_num);
    if (page_lines > 0)
    {
        page_ents.reserve(page_lines);
    }

    g_progress.set_spin(SPIN_BACKGROUND);
    while (a && line_on + s_ent_lines(a) <= page_lines)
    {
        i = s_ent_lines(a);
        page_ents.push_back({a, static_cast<short>(i), static_cast<short>(line_on), (char) 0});
        line_on = line_on+i;
        a = next_elig(a);
    }
    g_progress.set_spin(SPIN_POP);
    bot_ent = size_cast<long>(page_ents) - 1;

    // there are fairly good reasons to refresh the last good entry, such
    // as clearing the rest of the screen...
    //
    ref_status = j;
    ref_desc = j;

    // Now, suppose that the pointer position is off the page.  That would
    // be bad, so lets make sure it doesn't happen.
    //
    ptr_page_line = std::min<long>(ptr_page_line, bot_ent);
    // temporary fix.  Under some conditions ineligible entries will
    // not be found until they are in the page.  In this case just
    // refill the page.
    //
    if (page_has_unavailable_entries())
    {
        return refill_page(); // next time the unavail won't be chosen
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
int ArticleScanSession::fill_page()
{
    refill = false;  // we don't need one now
    if (top_ent < 1) // set top to first entry
    {
        top_ent = first();
    }
    if (top_ent == 0) // no entries
    {
        return 0;       // failure
    }
    if (!refill_page()) // try for efficient refill
    {
        if (!fill_page_backward(top_ent))
        {
            // downgrade eligibility standards
            switch (cur_type)
            {
            case S_ART:               // article context
                if (g_sa_mode_zoom)             // we were zoomed in
                {
                    ref_top = true;             // for "FOLD" display
                    g_sa_mode_zoom = false;     // zoom out
                    if (g_sa_unzoom_refold)
                    {
                        g_sa_mode_fold = true;
                    }
                    (void) go_top_ents(); // go to top (ents and page)
                    return fill_page();
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
    for (int j = 0; j <= bot_ent; j++)
    {
        if (!s_eligible(page_ents[j].ent_num))
        {
            return fill_page(); // ineligible won't be chosen again
        }
    }
    // be extra cautious about the article scan pages
    if (page_has_unavailable_entries())
    {
        return fill_page(); // next time the unavail won't be chosen
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

bool ArticleScanSession::go_top_ents()
{
    top_ent = first();
    if (!top_ent)
    {
        fmt::print("s_go_top_ents(): no first entry\n");
    }
    TRN_ASSERT(top_ent);      // be nicer later
    if (!s_eligible(top_ent)) // this may save a redraw...
    {
        top_ent = next_elig(top_ent);
    }
    if (!top_ent) // none eligible
    {
        // just go to the top of all the entries
        if (cur_type == S_ART)
        {
            top_ent = g_abs_first.value_of();
        }
        else
        {
            top_ent = 1; // not very nice coding
        }
    }
    refill = true;
    go_top_page();
    return true;        // successful
}

bool ArticleScanSession::go_bot_ents()
{
    bool flag = fill_page_backward(last()); // fill backwards
    if (!flag)
    {
        return false;
    }
    go_bot_page();
    return true;
}

void ArticleScanSession::go_next_page()
{
    long a = next_elig(page_ents[bot_ent].ent_num);
    if (!a)
    {
        return;         // no next page (we shouldn't have been called)
    }
    // the fill-page will set the refresh for the screen
    bool flag = fill_page_forward(a);
    TRN_ASSERT(flag);           // I *must* be able to fill a page
    ptr_page_line = 0;          // top of page
}

void ArticleScanSession::go_prev_page()
{
    long a = prev_elig(page_ents[0].ent_num);
    if (!a)
    {
        return;         // no prev. page (we shouldn't have been called)
    }
    // the fill-page will set the refresh for the screen
    bool flag = fill_page_backward(a);  // fill backward
    TRN_ASSERT(flag);                   // be nicer later...
    // take care of partially filled previous pages
    flag = refill_page();
    TRN_ASSERT(flag);           // be nicer later...
    ref_status = 0;
    ref_desc = 0;      // refresh from top
    ptr_page_line = 0; // top of page
}
