/* trn/rt-page.h
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_PAGE_H
#define TRN_RT_PAGE_H

#include <trn/opt.h>
#include <trn/rt-select.h>
#include <trn/univ.h>

#include <cstddef>
#include <string>
#include <string_view>

enum : bool
{
    PRESERVE_PAGE = false,
    FILL_LAST_PAGE = true
};

struct Article;
struct Multirc;
struct NewsgroupData;
struct Subject;
struct UniversalItem;

enum
{
    MAX_SEL = 99
};

union Selection
{
    Article       *ap;
    Subject       *sp;
    AddGroupIndex  gp;
    Multirc       *mp;
    NewsgroupData *np;
    UniversalItem *un;
    OptionIndex    op;
};

struct SelectionItem
{
    Selection u;
    int       line;
    int       sel;
};

struct SelectorPage
{
    SelectionItem      items[MAX_SEL]{};
    int                total_obj_cnt{};
    int                prior_obj_cnt{};
    int                page_obj_cnt{};
    int                page_item_cnt{};
    Article          **page_art{};
    Article          **next_art{};
    Article           *last_art{};
    Subject           *page_subj{};
    Subject           *next_subj{};
    Subject           *last_subj{};
    int                max_line_cnt{};
    int                max_per_page{};
    int                next_op{};
    int                item_index{};
    int                last_line{};
    bool               at_end{};
    std::string        chars;
    NewsgroupData     *page_ngd{};
    NewsgroupData     *next_ngd{};
    AddGroupIndex      page_grp{};
    AddGroupIndex      next_grp{};
    Multirc           *page_mrc{};
    Multirc           *next_mrc{};
    int                page_op{};
    UniversalItemIndex page_univ_index{};
    UniversalItemIndex next_univ_index{};

    void init();
    bool first_page();
    bool prev_page();
    bool last_page();
    bool next_page();
};

extern SelectorPage g_sel_page;

extern std::string   g_sel_grp_display_mode;
extern std::string   g_sel_art_display_mode;
extern std::size_t   g_sel_grp_display_mode_index;
extern std::size_t   g_sel_art_display_mode_index;

bool set_sel_mode(char_int ch);
char  current_group_display_mode();
char  current_article_display_mode();
void init_pages(bool fill_last_page);
bool calc_page(Selection u);
void display_page();
void update_page();
void output_sel(int ix, int sel, bool update);
void display_option(int op, int item_index);

#endif
