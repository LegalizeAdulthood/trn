/* trn/rt-select.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RT_SELECT_H
#define TRN_RT_SELECT_H

#include <config/config2.h>

#include "trn/addng.h"
#include "trn/help.h"
#include "trn/rcstuff.h"

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

extern bool              g_sel_rereading;
extern SelectionMode     g_sel_mode;
extern SelectionMode     g_sel_default_mode;
extern SelectionMode     g_sel_thread_mode;
extern const char       *g_sel_mode_string;
extern SelectionSortMode g_sel_sort;
extern SelectionSortMode g_sel_art_sort;
extern SelectionSortMode g_sel_thread_sort;
extern SelectionSortMode g_sel_newsgroup_sort;
extern const char       *g_sel_sort_string;
extern int               g_sel_direction;
extern bool              g_sel_exclusive;
extern AddGroupFlags     g_sel_mask;
extern bool              g_selected_only;
extern ArticleUnread     g_selected_count;
extern int               g_selected_subj_cnt;
extern int               g_added_articles;
extern char             *g_sel_chars;
extern int               g_sel_item_index;
extern int               g_sel_last_line;
extern bool              g_sel_at_end;
extern int               g_keep_the_group_static; // -K
extern char              g_newsrc_sel_cmds[3];
extern char              g_add_sel_cmds[3];
extern char              g_newsgroup_sel_cmds[3];
extern char              g_news_sel_cmds[3];
extern char              g_option_sel_cmds[3];
extern bool              g_use_sel_num;
extern bool              g_sel_num_goto;

char article_selector(char_int cmd);
char multirc_selector();
char newsgroup_selector();
char add_group_selector(GetNewsgroupFlags flags);
char option_selector();
char universal_selector();
void selector_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);
int  univ_visit_group(const char *group_name);
void univ_visit_help(HelpLocation where);

#endif
