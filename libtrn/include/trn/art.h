/* trn/art.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_ART_H
#define TRN_ART_H

#include <config/common.h>
#include <config/typedef.h>

// do_article() return values
enum DoArticleResult
{
    DA_NORM = 0,
    DA_RAISE = 1,
    DA_CLEAN = 2,
    DA_TO_END = 3
};

extern ArticleLine     g_highlight;         // next line to be highlighted
extern ArticleLine     g_first_view;        //
extern ArticlePosition g_raw_art_size;      // size in bytes of raw article
extern ArticlePosition g_art_size;          // size in bytes of article
extern char            g_art_line[LINE_BUF_LEN]; // place for article lines

extern int             g_g_line;
extern ArticlePosition g_inner_search;    // g_artpos of end of line we want to visit
extern ArticleLine     g_inner_light;     // highlight position for g_innersearch or 0
extern char            g_hide_everything; // if set, do not write page now,
                                          // ...but execute char when done with page

extern bool g_dont_filter_control; // -j

void art_init();
DoArticleResult do_article();
bool maybe_set_color(const char *cp, bool back_search);
bool inner_more();
void pager_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);

#endif
