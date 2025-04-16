/* trn/art.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ART_H
#define TRN_ART_H

#include <config/common.h>
#include <config/typedef.h>

/* do_article() return values */
enum DoArticlResult
{
    DA_NORM = 0,
    DA_RAISE = 1,
    DA_CLEAN = 2,
    DA_TOEND = 3
};

extern ArticleLine g_highlight; /* next line to be highlighted */
extern ArticleLine g_first_view;
extern ArticlePosition g_raw_artsize;    /* size in bytes of raw article */
extern ArticlePosition g_artsize;        /* size in bytes of article */
extern char g_art_line[LBUFLEN]; /* place for article lines */

extern int g_gline;
extern ArticlePosition g_innersearch;  /* g_artpos of end of line we want to visit */
extern ArticleLine g_innerlight;  /* highlight position for g_innersearch or 0 */
extern char g_hide_everything; /* if set, do not write page now, */
                             /* ...but execute char when done with page */

extern bool g_dont_filter_control; /* -j */

void art_init();
DoArticlResult do_article();
bool maybe_set_color(const char *cp, bool backsearch);
bool innermore();
void pager_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);

#endif
