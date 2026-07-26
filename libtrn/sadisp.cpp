/* sadisp.cpp
 *
 * display stuff
 */
// This file Copyright 1992 by Clifford A. Adams

#include <trn/sadisp.h>

#include <trn/sadisp-internal.h>

#include <config/common.h>
#include <trn/color.h>
#include <trn/samain.h>
#include <trn/samisc.h>
#include <trn/scan.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/sdisp.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <fmt/format.h>

#include <string_view>

#include <cstdio>

static std::string_view sa_order_text()
{
    switch (g_sa_mode_order)
    {
    case SA_ORDER_ARRIVAL:
        return "arrival";

    case SA_ORDER_DESCENDING:
        if (g_score_new_first)
        {
            return "score (new>old)";
        }
        return "score (old>new)";

    default:
        return "unknown";
    }
}

std::string_view sa_order_text_for_test()
{
    return sa_order_text();
}

void sa_refresh_top()
{
    color_object(COLOR_SCORE, true);
    std::printf("%s |",g_newsgroup_name.c_str());
// # of articles might be optional later
    std::printf(" %d",sa_number_arts());

    if (g_sa_mode_read_elig)
    {
        std::printf(" unread+read");
    }
    else
    {
        std::printf(" unread");
    }
    if (g_sa_mode_zoom)
    {
        std::printf(" zoom");
    }
    if (g_sa_mode_fold)
    {
        std::printf(" Fold");
    }
    if (g_sa_follow)
    {
        std::printf(" follow");
    }
    color_pop();        // of COLOR_SCORE
    erase_eol();
    std::printf("\n");
}

void sa_refresh_bot()
{
    color_object(COLOR_SCORE, true);
    s_mail_and_place();
    const std::string_view order = sa_order_text();
    fmt::print("({} order, {}% scored)", order, sc_percent_scored());
    color_pop();        // of COLOR_SCORE
    std::fflush(stdout);
}

// set up various screen dimensions
void sa_set_screen()
{
    // One size fits all for now.
    // these things here because they may vary by screen size later
    g_s_top_lines = 1;
    g_s_bot_lines = 1;
    g_s_status_cols = 3;
    g_s_cursor_cols = 2;

    if (g_s_item_num)
    {
        g_s_item_num_cols = 3;
    }
    else
    {
        g_s_item_num_cols = 0;
    }

    // (g_scr_width-1) keeps last character blank.
    g_s_desc_cols = (g_scr_width-1) -g_s_status_cols -g_s_cursor_cols -g_s_item_num_cols;
}
