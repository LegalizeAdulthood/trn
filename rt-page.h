/* rt-page.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#define PRESERVE_PAGE     false
#define FILL_LAST_PAGE    true

EXT int sel_total_obj_cnt;
EXT int sel_prior_obj_cnt;
EXT int sel_page_obj_cnt;
EXT int sel_page_item_cnt;
EXT int sel_max_per_page;
EXT int sel_max_line_cnt;

EXT ARTICLE** sel_page_app;
EXT ARTICLE** sel_next_app;
EXT ARTICLE* sel_last_ap;
EXT SUBJECT* sel_page_sp;
EXT SUBJECT* sel_next_sp;
EXT SUBJECT* sel_last_sp;

EXT char* sel_grp_dmode INIT("*slm");
EXT char* sel_art_dmode INIT("*lmds");

EXT bool group_init_done INIT(true);

union SEL_UNION
{
    ARTICLE* ap;
    SUBJECT* sp;
    ADDGROUP* gp;
    MULTIRC* mp;
    NGDATA* np;
    UNIV_ITEM* un;
    int op;
};

struct SEL_ITEM
{
    SEL_UNION u;
    int line;
    int sel;
};

#define MAX_SEL 99
EXT SEL_ITEM sel_items[MAX_SEL];

bool set_sel_mode(char_int);
char *get_sel_order(int);
bool set_sel_order(int, char *);
bool set_sel_sort(int, char_int);
void set_selector(int, int);
void init_pages(bool fill_last_page);
bool first_page();
bool last_page();
bool next_page();
bool prev_page();
bool calc_page(SEL_UNION u);
void display_page_title(bool home_only);
void display_page();
void update_page();
void output_sel(int ix, int sel, bool update);
void display_option(int, int);
