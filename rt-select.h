/* rt-select.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef RT_SELECT_H
#define RT_SELECT_H

#include "help.h"

enum sel_mode
{
    SM_MAGIC_NUMBER = 0, // Not sure what this mode value means?
    SM_THREAD,
    SM_SUBJECT,
    SM_ARTICLE,
    SM_NEWSGROUP,
    SM_ADDGROUP,
    SM_MULTIRC,
    SM_OPTIONS,
    SM_UNIVERSAL
};

enum sel_sort_mode
{
    SS_MAGIC_NUMBER = 0, // Not sure what this mode value means?
    SS_DATE,
    SS_STRING,
    SS_AUTHOR,
    SS_COUNT,
    SS_NATURAL,
    SS_GROUPS,
    SS_LINES,
    /* NOTE: The score order is still valid even without scoring enabled. */
    /*       (The real order is then something like natural or date.) */
    SS_SCORE
};

enum
{
    DS_ASK = 1,
    DS_UPDATE = 2,
    DS_DISPLAY = 3,
    DS_RESTART = 4,
    DS_STATUS = 5,
    DS_QUIT = 6,
    DS_DOCOMMAND = 7,
    DS_ERROR = 8
};

enum
{
    UR_NORM = 1,
    UR_BREAK = 2, /* request return to selector */
    UR_ERROR = 3  /* non-normal return */
};

extern bool g_sel_rereading;
extern char g_sel_disp_char[];
extern sel_mode g_sel_mode;
extern sel_mode g_sel_defaultmode;
extern sel_mode g_sel_threadmode;
extern char *g_sel_mode_string;
extern sel_sort_mode g_sel_sort;
extern sel_sort_mode g_sel_artsort;
extern sel_sort_mode g_sel_threadsort;
extern sel_sort_mode g_sel_newsgroupsort;
extern sel_sort_mode g_sel_addgroupsort;
extern sel_sort_mode g_sel_univsort;
extern char *g_sel_sort_string;
extern int g_sel_direction;
extern bool g_sel_exclusive;
extern int g_sel_mask;
extern bool g_selected_only;
extern ART_UNREAD g_selected_count;
extern int g_selected_subj_cnt;
extern int g_added_articles;
extern char *g_sel_chars;
extern int g_sel_item_index;
extern int g_sel_last_line;
extern bool g_sel_at_end;
extern bool g_art_sel_ilock;

char article_selector(char_int cmd);
char multirc_selector();
char newsgroup_selector();
char addgroup_selector(int flags);
char option_selector();
char universal_selector();
void selector_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk);
int univ_visit_group(const char *gname);
void univ_visit_help(help_location where);

#endif
