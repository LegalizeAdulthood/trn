/* trn/art.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ART_H
#define TRN_ART_H

#include <config/common.h>
#include <config/typedef.h>
#include <trn/search.h>

#include <string>
#include <string_view>

// do_article() return values
enum DoArticleResult
{
    DA_NORM = 0,
    DA_RAISE = 1,
    DA_CLEAN = 2,
    DA_TO_END = 3
};

// page_switch() return values
enum PageSwitchResult
{
    PS_NORM = 0,
    PS_ASK = 1,
    PS_RAISE = 2,
    PS_TO_END = 3
};

struct ArticlePager
{
    void init();
    PageSwitchResult page_switch(std::string_view command);
    DoArticleResult  do_article(std::string &article_command);

    // TODO: private
    bool inner_more();

    ArticleLine     highlight{-1};     // next line to be highlighted
    int             goto_line{};       // TODO: ArticleLine?
    ArticlePosition inner_search{};    // g_art_input.pos of end of line we want to visit
    ArticleLine     inner_light{};     // highlight position for inner_search or 0
    char            hide_everything{}; // if set, do not write page now,
                                       // ...but execute char when done with page
    bool            do_fseek{};        // should we back up in article file?
    ArticleLine     top_line{-1};      // top line of current screen
    std::string     prompt;            // current prompt
    bool            special{};         // is next page special length?
    int             special_lines{};   // how long to make page when special
    ArticlePosition restart{};         // if nonzero, the place where last line left off on line split
    ArticlePosition a_line_begin{};    // where in file current line began
    int             more_prompt_col{}; // non-zero when the more prompt is indented
    ArticleLine     i_search_line{};   // last line to display
    CompiledRegex   gcompex{};         // in article search pattern
    bool            first_page{};      // is this the 1st page of article?
    bool            continuation{};    // this line/header is being continued
};

extern ArticlePager g_pager;

extern ArticleLine     g_first_view;        //
extern ArticlePosition g_raw_art_size;      // size in bytes of raw article
extern ArticlePosition g_art_size;          // size in bytes of article
extern std::string     g_art_line;          // place for article lines

extern bool g_dont_filter_control; // -j

DoArticleResult do_article();
void pager_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);

#endif
