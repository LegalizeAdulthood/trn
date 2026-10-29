/* trn/rt-select.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RT_SELECT_H
#define TRN_RT_SELECT_H

#include <config/config2.h>
#include <trn/addng.h>
#include <trn/help.h>
#include <trn/rcstuff.h>

#include <string>
#include <string_view>

enum SelectionMode
{
    SM_MAGIC_NUMBER = 0, // Not sure what this mode value means?
    SM_THREAD,
    SM_SUBJECT,
    SM_ARTICLE,
    SM_NEWSGROUP,
    SM_ADD_GROUP,
    SM_MULTIRC,
    SM_OPTIONS,
    SM_UNIVERSAL
};

enum SelectionSortMode
{
    SS_MAGIC_NUMBER = 0, // Not sure what this mode value means?
    SS_DATE,
    SS_STRING,
    SS_AUTHOR,
    SS_COUNT,
    SS_NATURAL,
    SS_GROUPS,
    SS_LINES,
    // NOTE: The score order is still valid even without scoring enabled.
    // (The real order is then something like natural or date.)
    SS_SCORE
};

struct SelectorPolicy
{
    bool              rereading{};
    SelectionMode     mode{};
    SelectionMode     default_mode{SM_THREAD};
    SelectionMode     thread_mode{SM_THREAD};
    std::string_view  mode_string{};
    SelectionSortMode sort{};
    SelectionSortMode art_sort{SS_GROUPS};
    SelectionSortMode thread_sort{SS_DATE};
    SelectionSortMode newsgroup_sort{SS_NATURAL};
    std::string_view  sort_string{};
    int               direction{1};
    bool              exclusive{};
    AddGroupFlags     mask{AGF_SEL};
    bool              group_listing{};
    bool              selected_only{};
    SelectionSortMode add_group_sort{SS_NATURAL};
    SelectionSortMode universal_sort{SS_NATURAL};

    void set_selector(SelectionMode smode, SelectionSortMode ssort);
    bool set_sort(SelectionMode smode, char_int ch);
    bool set_order(SelectionMode smode, std::string_view str);
};

extern SelectorPolicy    g_sel;
extern ArticleUnread     g_selected_count;
extern int               g_selected_subj_cnt;
extern int               g_added_articles;
extern std::string       g_sel_chars;
extern int               g_sel_item_index;
extern int               g_sel_last_line;
extern bool              g_sel_at_end;
extern int               g_keep_the_group_static; // -K
extern std::string       g_newsrc_sel_cmds;
extern std::string       g_add_sel_cmds;
extern std::string       g_newsgroup_sel_cmds;
extern std::string       g_news_sel_cmds;
extern std::string       g_option_sel_cmds;
extern bool              g_use_sel_num;
extern bool              g_sel_num_goto;

void set_selector_commands(std::string &commands, std::string_view value);
char selector_end_command(std::string_view commands);
char selector_page_command(std::string_view commands);
char article_selector(char_int cmd);
char multirc_selector();
char newsgroup_selector();
char add_group_selector(GetNewsgroupFlags flags);
char option_selector();
char universal_selector();
void selector_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);
int  univ_visit_group(std::string_view group_name);
void univ_visit_help(HelpLocation where);

#endif
