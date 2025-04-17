/* trn/rt-page.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RT_PAGE_H
#define TRN_RT_PAGE_H

#include "trn/opt.h"
#include "trn/rt-select.h"

enum : bool
{
    PRESERVE_PAGE = false,
    FILL_LAST_PAGE = true
};

struct AddGroup;
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
    AddGroup      *gp;
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

extern SelectionItem g_sel_items[MAX_SEL];
extern int           g_sel_total_obj_cnt;
extern int           g_sel_prior_obj_cnt;
extern int           g_sel_page_obj_cnt;
extern int           g_sel_page_item_cnt;
extern Article     **g_sel_page_app;
extern Article     **g_sel_next_app;
extern Article      *g_sel_last_ap;
extern Subject      *g_sel_page_sp;
extern Subject      *g_sel_next_sp;
extern Subject      *g_sel_last_sp;
extern char         *g_sel_grp_display_mode;
extern char         *g_sel_art_display_mode;

bool set_sel_mode(char_int ch);
char *get_sel_order(SelectionMode smode);
bool set_sel_order(SelectionMode smode, const char *str);
bool set_sel_sort(SelectionMode smode, char_int ch);
void set_selector(SelectionMode smode, SelectionSortMode ssort);
void init_pages(bool fill_last_page);
bool first_page();
bool last_page();
bool next_page();
bool prev_page();
bool calc_page(Selection u);
void display_page_title(bool home_only);
void display_page();
void update_page();
void output_sel(int ix, int sel, bool update);
void display_option(int op, int item_index);

#endif
