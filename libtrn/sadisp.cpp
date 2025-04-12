/* This file Copyright 1992 by Clifford A. Adams */
/* sadisp.c
 *
 * display stuff
 */

#include "config/common.h"
#include "trn/sadisp.h"

#include "trn/color.h"
#include "trn/samain.h"
#include "trn/samisc.h"
#include "trn/scan.h"
#include "trn/scanart.h"
#include "trn/score.h"
#include "trn/sdisp.h"
#include "trn/terminal.h"
#include "trn/trn.h"

void sa_refresh_top()
{
    color_object(COLOR_SCORE, true);
    printf("%s |",g_ngname.c_str());
/* # of articles might be optional later */
    printf(" %d",sa_number_arts());

    if (g_sa_mode_read_elig)
    {
        printf(" unread+read");
    }
    else
    {
        printf(" unread");
    }
    if (g_sa_mode_zoom)
    {
        printf(" zoom");
    }
    if (g_sa_mode_fold)
    {
        printf(" Fold");
    }
    if (g_sa_follow)
    {
        printf(" follow");
    }
    color_pop();        /* of COLOR_SCORE */
    erase_eol();
    printf("\n");
}

void sa_refresh_bot()
{
    char* s;

    color_object(COLOR_SCORE, true);
    s_mail_and_place();
    printf("(");
    switch (g_sa_mode_order) {
      case SA_ORDER_ARRIVAL:
        s = "arrival";
        break;
      case SA_ORDER_DESCENDING:
        if (g_score_newfirst)
        {
            s = "score (new>old)";
        }
        else
        {
            s = "score (old>new)";
        }
        break;
      default:
        s = "unknown";
        break;
    }
    printf("%s order",s);
    printf(", %d%% scored",sc_percent_scored());
    printf(")");
    color_pop();        /* of COLOR_SCORE */
    fflush(stdout);
}

/* set up various screen dimensions */
void sa_set_screen()
{
    /* One size fits all for now. */
    /* these things here because they may vary by screen size later */
    g_s_top_lines = 1;
    g_s_bot_lines = 1;
    g_s_status_cols = 3;
    g_s_cursor_cols = 2;

    if (g_s_itemnum)
    {
        g_s_itemnum_cols = 3;
    }
    else
    {
        g_s_itemnum_cols = 0;
    }

    /* (g_scr_width-1) keeps last character blank. */
    g_s_desc_cols = (g_scr_width-1) -g_s_status_cols -g_s_cursor_cols -g_s_itemnum_cols;
}
