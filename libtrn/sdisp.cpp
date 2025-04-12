/* This file Copyright 1992 by Clifford A. Adams */
/* sdisp.c
 *
 * display stuff
 */

#include "config/common.h"
#include "trn/sdisp.h"

#include "trn/ng.h" /* g_mailcall */
#include "trn/sadisp.h"
#include "trn/scan.h"
#include "trn/smisc.h"
#include "trn/sorder.h"
#include "trn/terminal.h"

int  g_scr_height{}; /* height of screen in characters */
int  g_scr_width{};  /* width of screen in characters */
bool g_s_resized{};  /* has the window been resized? */

void s_goxy(int x, int y)
{
    tputs(tgoto(g_tc_CM, x, y), 1, putchr);
}

/* Print a string with the placing of the page and mail status.
 * sample: "(mail)-MIDDLE-"
 * Good for most bottom status bars.
 */
void s_mail_and_place()
{
    bool previous;
    bool next;

#ifdef MAILCALL
    setmail(false);             /* another chance to check mail */
    printf("%s",g_mailcall);
#endif /* MAILCALL */
    /* print page status wrt all entries */
    previous = (0 != s_prev_elig(g_page_ents[0].entnum));
    next = (0 != s_next_elig(g_page_ents[g_s_bot_ent].entnum));
    if (previous && next)
    {
        printf("-MIDDLE-");             /* middle of entries */
    }
    else if (next && !previous)
    {
        printf("-TOP-");
    }
    else if (previous && !next)
    {
        printf("-BOTTOM-");
    }
    else        /* !previous && !next */
    {
        printf("-ALL-");
    }
}

void s_refresh_top()
{
    home_cursor();
    switch (g_s_cur_type) {
      case S_ART:
        sa_refresh_top();
        break;
    }
    g_s_ref_top = false;
}

void s_refresh_bot()
{
    /* if bottom bar exists, then it is at least one character high... */
    s_goxy(0,g_tc_LINES-g_s_bot_lines);
    switch (g_s_cur_type) {
      case S_ART:
        sa_refresh_bot();
        break;
    }
    g_s_ref_bot = false;
}

/* refresh both status and description */
void s_refresh_entzone()
{
    int start;          /* starting page_arts index to refresh... */

    if (g_s_ref_status < g_s_ref_desc) {
        /* refresh status characters up to (not including) desc_line */
        for (int i = g_s_ref_status; i <= g_s_bot_ent && i < g_s_ref_desc; i++)
        {
            s_refresh_description(i);
        }
        start = g_s_ref_desc;
    } else {
        for (int i = g_s_ref_desc; i <= g_s_bot_ent && i < g_s_ref_status; i++)
        {
            s_refresh_status(i);
        }
        start = g_s_ref_status;
    }
    for (int i = start; i <= g_s_bot_ent; i++)
    {
        s_ref_entry(i,i==start);
    }
    /* clear to end of screen */
    clear_rest();
    /* now we need to redraw the bottom status line */
    g_s_ref_bot = true;
    g_s_ref_status = -1;
    g_s_ref_desc = -1;
}

void s_place_ptr()
{
    s_goxy(g_s_status_cols,
            g_s_top_lines+g_page_ents[g_s_ptr_page_line].start_line);
    putchar('>');
    fflush(stdout);
}

/* refresh the status line for an article on screen page */
/* note: descriptions will not (for now) be individually refreshable */
void s_refresh_status(int line)
{
    long ent = g_page_ents[line].entnum;
    TRN_ASSERT(line <= g_s_bot_ent);    /* better be refreshing on-page */
    s_goxy(0,g_s_top_lines+g_page_ents[line].start_line);
    int j = g_page_ents[line].lines;
    for (int i = 1; i <= j; i++)
    {
        printf("%s\n",s_get_statchars(ent,i));
    }
    fflush(stdout);
}

void s_refresh_description(int line)
{
    long ent = g_page_ents[line].entnum;
    TRN_ASSERT(line <= g_s_bot_ent);    /* better be refreshing on-page */
    int startline = g_s_top_lines + g_page_ents[line].start_line;
    int j = g_page_ents[line].lines;
    for (int i = 1; i <= j; i++) {
        s_goxy(g_s_status_cols+g_s_cursor_cols,(i-1)+startline);
        /* allow flexible format later? */
        if (g_s_itemnum_cols) {
            if (i == 1) {       /* first description line */
                if (line < 99)
                {
                    printf("%2d ",line+1);
                }
                else
                {
                    printf("** ");      /* too big */
                }
            } else
            {
                printf("   ");
            }
        }
        printf("%s",s_get_desc(ent,i,true));
        erase_eol();
        putchar('\n');
    }
    fflush(stdout);
}

//int jump;     /* true means that the cursor should be positioned */
void s_ref_entry(int line, int jump)
{
    long ent = g_page_ents[line].entnum;
    TRN_ASSERT(line <= g_s_bot_ent);    /* better be refreshing on-page */
    if (jump)
    {
        s_goxy(0,g_s_top_lines+g_page_ents[line].start_line);
    }
    int j = g_page_ents[line].lines;
    for (int i = 1; i <= j; i++) {
/* later replace middle with variable #spaces routine */
        printf("%s%s",s_get_statchars(ent,i),"  ");
        if (g_s_itemnum_cols) {
            if (i == 1) {       /* first description line */
                if (line < 99)
                {
                    printf("%2d ",line+1);
                }
                else
                {
                    printf("** ");      /* too big */
                }
            } else
            {
                printf("   ");
            }
        }
        printf("%s",s_get_desc(ent,i,true));
        erase_eol();
        putchar('\n');
    }
}

void s_rub_ptr()
{
    rubout();
}

void s_refresh()
{
    if (g_s_ref_all) {
        clear();        /* make a clean slate */
        g_s_ref_desc = 0;
        g_s_ref_status = 0;
    }
    if ((g_s_ref_all || g_s_ref_top) && g_s_top_lines>0)
    {
        s_refresh_top();
    }
    if (g_s_ref_all || ((g_s_ref_status>=0) && (g_s_ref_desc>=0)))
    {
        s_refresh_entzone();
    }
    else {
        if (g_s_ref_status>=0) {
            for (int i = g_s_ref_status; i <= g_s_bot_ent; i++)
            {
                s_refresh_status(i);
            }
        }
        if (g_s_ref_desc >= 0) {
            for (int i = g_s_ref_desc; i <= g_s_bot_ent; i++)
            {
                s_refresh_description(i);
            }
        }
    }
    g_s_ref_status = -1;
    g_s_ref_desc = -1;
    if ((g_s_ref_all || g_s_ref_bot) && g_s_bot_lines > 0)
    {
        s_refresh_bot();
    }
    g_s_ref_all = false;
}

int s_initscreen()
{
    /* check to see if term is too dumb: if so, return non-zero */

    /* set scr_{height,width} */
    /* return 0 if all went well */

    g_scr_height = g_tc_LINES;
    g_scr_width = g_tc_COLS;
    if (g_scr_height > 2 && g_scr_width > 1)    /* current dependencies */
    {
        return 0;       /* everything is OK. */
    }
    return 1;   /* we can't play with this... */
}

/* screen-refresh the status if on-page */
void s_ref_status_onpage(long ent)
{
    for (int i = 0; i <= g_s_bot_ent; i++)
    {
        if (g_page_ents[i].entnum == ent)
        {
            s_refresh_status(i);
        }
    }
}


void s_resize_win()
{
#if 0
    int i;

    i = s_initscreen();
    /* later possibly use the return value for an error abort? */
    g_s_resized = true;
#endif
}
