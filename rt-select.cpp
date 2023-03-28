/* rt-select.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "rt-select.h"

#include "addng.h"
#include "artsrch.h"
#include "bits.h"
#include "cache.h"
#include "color.h"
#include "datasrc.h"
#include "final.h"
#include "intrp.h"
#include "kfile.h"
#include "list.h"
#include "ng.h"
#include "ngdata.h"
#include "ngsrch.h"
#include "ngstuff.h"
#include "nntp.h"
#include "only.h"
#include "opt.h"
#include "rcln.h"
#include "rcstuff.h"
#include "rt-page.h"
#include "rt-util.h"
#include "rthread.h"
#include "term.h"
#include "trn.h"
#include "univ.h"
#include "util.h"
#include "util2.h"

enum display_state
{
    DS_ASK = 1,
    DS_UPDATE,
    DS_DISPLAY,
    DS_RESTART,
    DS_STATUS,
    DS_QUIT,
    DS_DOCOMMAND,
    DS_ERROR
};

enum univ_read_result
{
    UR_NORM = 1,
    UR_BREAK = 2, /* request return to selector */
    UR_ERROR = 3  /* non-normal return */
};

bool g_sel_rereading{};
char g_sel_disp_char[]{" +-*"};
sel_mode g_sel_mode{};
sel_mode g_sel_defaultmode{SM_THREAD};
sel_mode g_sel_threadmode{SM_THREAD};
char *g_sel_mode_string{};
sel_sort_mode g_sel_sort{};
sel_sort_mode g_sel_artsort{SS_GROUPS};
sel_sort_mode g_sel_threadsort{SS_DATE};
sel_sort_mode g_sel_newsgroupsort{SS_NATURAL};
sel_sort_mode g_sel_addgroupsort{SS_NATURAL};
sel_sort_mode g_sel_univsort{SS_NATURAL};
char *g_sel_sort_string{};
int g_sel_direction{1};
bool g_sel_exclusive{};
addgroup_flags g_sel_mask{AGF_SEL};
bool g_selected_only{};
ART_UNREAD g_selected_count{};
int g_selected_subj_cnt{};
int g_added_articles{};
char *g_sel_chars{};
int g_sel_item_index{};
int g_sel_last_line{};
bool g_sel_at_end{};
bool g_art_sel_ilock{};

static char s_sel_ret{};
static char s_page_char{};
static char s_end_char{};
static int s_disp_status_line{};
static bool s_clean_screen{};
static int s_removed_prompt{};
static int s_force_sel_pos{};
static display_state (*s_extra_commands)(char_int){};

namespace {

class save_selector_modes
{
public:
    save_selector_modes(char new_mode) :
        m_save_mode(g_mode),
        m_save_gmode(g_general_mode)
    {
        g_bos_on_stop = true;
        set_mode('s', new_mode);
    }
    ~save_selector_modes()
    {
        g_bos_on_stop = false;
        set_mode(m_save_gmode, m_save_mode);
    }

private:
    char m_save_mode;
    char m_save_gmode;
};

}

#define PUSH_SELECTOR()                                \
    sel_mode save_sel_mode = g_sel_mode;               \
    const bool save_sel_rereading = g_sel_rereading;   \
    const bool save_sel_exclusive = g_sel_exclusive;   \
    ART_UNREAD save_selected_count = g_selected_count; \
    display_state (*const save_extra_commands)(char_int) = s_extra_commands

#define POP_SELECTOR()                                      \
    do                                                      \
    {                                                       \
        g_sel_exclusive = save_sel_exclusive;               \
        g_sel_rereading = save_sel_rereading;               \
        g_selected_count = save_selected_count;             \
        s_extra_commands = save_extra_commands;             \
        g_bos_on_stop = true;                               \
        if (g_sel_mode != save_sel_mode)                    \
        {                                                   \
            g_sel_mode = save_sel_mode;                     \
            set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER); \
            save_sel_mode = SM_MAGIC_NUMBER;                \
        }                                                   \
    } while (false)

#define PUSH_UNIV_SELECTOR()                             \
    UNIV_ITEM *const save_first_univ = g_first_univ;     \
    UNIV_ITEM *const save_last_univ = g_last_univ;       \
    UNIV_ITEM *const save_page_univ = sel_page_univ;     \
    UNIV_ITEM *const save_next_univ = g_sel_next_univ;   \
    char *const save_univ_fname = g_univ_fname;          \
    char *const save_univ_label = g_univ_label;          \
    char *const save_univ_title = g_univ_title;          \
    char *const save_univ_tmp_file = g_univ_tmp_file;    \
    const char save_sel_ret = s_sel_ret;                 \
    HASHTABLE *const save_univ_ng_hash = g_univ_ng_hash; \
    HASHTABLE *const save_univ_vg_hash = g_univ_vg_hash

#define POP_UNIV_SELECTOR()                   \
    do                                        \
    {                                         \
        g_first_univ = save_first_univ;       \
        g_last_univ = save_last_univ;         \
        sel_page_univ = save_page_univ;       \
        g_sel_next_univ = save_next_univ;     \
        g_univ_fname = save_univ_fname;       \
        g_univ_label = save_univ_label;       \
        g_univ_title = save_univ_title;       \
        g_univ_tmp_file = save_univ_tmp_file; \
        s_sel_ret = save_sel_ret;             \
        g_univ_ng_hash = save_univ_ng_hash;   \
        g_univ_vg_hash = save_univ_vg_hash;   \
    } while (false)

static void sel_dogroups();
static univ_read_result univ_read(UNIV_ITEM *ui);
static void sel_display();
static void sel_status_msg(const char *cp);
static char sel_input();
static void sel_prompt();
static bool select_item(SEL_UNION u);
static bool delay_return_item(SEL_UNION u);
static bool deselect_item(SEL_UNION u);
static bool select_option(int i);
static void sel_cleanup();
static bool mark_DEL_as_READ(char *ptr, int arg);
static display_state sel_command(char_int ch);
static bool sel_perform_change(long cnt, const char *obj_type);
static char another_command(char_int ch);
static display_state article_commands(char_int ch);
static display_state newsgroup_commands(char_int ch);
static display_state addgroup_commands(char_int ch);
static display_state multirc_commands(char_int ch);
static display_state option_commands(char_int ch);
static display_state universal_commands(char_int ch);
static void switch_dmode(char **dmode_cpp);
static int find_line(int y);

/* Display a menu of threads/subjects/articles for the user to choose from.
** If "cmd" is '+' we display all the unread items and allow the user to mark
** them as selected and perform various commands upon them.  If "cmd" is 'U'
** the list consists of previously-read items for the user to mark as unread.
*/
char article_selector(char_int cmd)
{
    bool save_selected_only;
    save_selector_modes saver('t');

    g_sel_rereading = (cmd == 'U');

    g_art = g_lastart+1;
    s_extra_commands = article_commands;
    g_keep_the_group_static = (g_keep_the_group_static == 1);

    g_sel_mode = SM_ARTICLE;
    set_sel_mode(cmd);

    if (!cache_range(g_sel_rereading? g_absfirst : g_firstart, g_lastart)) {
	s_sel_ret = '+';
	goto sel_exit;
    }

  sel_restart:
    /* Setup for selecting articles to read or set unread */
    if (g_sel_rereading) {
	s_end_char = 'Z';
	s_page_char = '>';
	g_sel_page_app = nullptr;
	g_sel_page_sp = nullptr;
	g_sel_mask = AGF_DELSEL;
    } else {
	s_end_char = g_news_sel_cmds[0];
	s_page_char = g_news_sel_cmds[1];
	if (g_curr_artp) {
	    g_sel_last_ap = g_curr_artp;
	    g_sel_last_sp = g_curr_artp->subj;
	}
	g_sel_mask = AGF_SEL;
    }
    save_selected_only = g_selected_only;
    g_selected_only = true;
    count_subjects(cmd? CS_UNSEL_STORE : CS_NORM);

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;
    *g_msg = '\0';
    if (g_added_articles) {
	long i = g_added_articles, j;
	for (j = g_lastart-i+1; j <= g_lastart; j++) {
	    if (!article_unread(j))
		i--;
	}
	if (i == g_added_articles)
	    sprintf(g_msg, "** %ld new article%s arrived **  ",
		(long)g_added_articles, plural(g_added_articles));
	else
	    sprintf(g_msg, "** %ld of %ld new articles unread **  ",
		i, (long)g_added_articles);
	s_disp_status_line = 1;
    }
    g_added_articles = 0;
    if (cmd && g_selected_count) {
	sprintf(g_msg+strlen(g_msg), "%ld article%s selected.",
		(long)g_selected_count, g_selected_count == 1? " is" : "s are");
	s_disp_status_line = 1;
    }
    cmd = 0;

    sel_display();
    if (sel_input() == 'R')
	goto sel_restart;

    sel_cleanup();
    newline();
    if (g_mousebar_cnt)
	clear_rest();

sel_exit:
    if (s_sel_ret == '\033')
	s_sel_ret = '+';
    else if (s_sel_ret == '`')
	s_sel_ret = 'Q';
    if (g_sel_rereading) {
	g_sel_rereading = false;
	g_sel_mask = AGF_SEL;
    }
    if (g_sel_mode != SM_ARTICLE || g_sel_sort == SS_GROUPS
     || g_sel_sort == SS_STRING) {
	if (g_artptr_list) {
	    free((char*)g_artptr_list);
	    g_artptr_list = g_sel_page_app = nullptr;
	    sort_subjects();
	}
	g_artptr = nullptr;
	if (!g_threaded_group)
	    g_srchahead = -1;
    }
    else
	g_srchahead = 0;
    g_selected_only = (g_selected_count != 0);
    if (s_sel_ret == '+') {
	g_selected_only = save_selected_only;
	count_subjects(CS_RESELECT);
    }
    else
	count_subjects(CS_UNSELECT);
    if (s_sel_ret == '+') {
	g_art = g_curr_art;
	g_artp = g_curr_artp;
    } else
	top_article();
    return s_sel_ret;
}

static void sel_dogroups()
{
    NGDATA* np;
    int ret;
    int save_selected_count = g_selected_count;

    for (np = g_first_ng; np; np = np->next) {
	if (!(np->flags & NF_VISIT))
	    continue;
      do_group:
	if (np->flags & NF_SEL) {
	    np->flags &= ~NF_SEL;
	    save_selected_count--;
	}
	set_ng(np);
	if (np != g_current_ng) {
	    g_recent_ng = g_current_ng;
	    g_current_ng = np;
	}
	g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
	printf("Entering %s:", g_ngname);
	if (s_sel_ret == ';') {
	    ret = do_newsgroup(savestr(";"));
	} else
	    ret = do_newsgroup("");
	switch (ret) {
	  case NG_NORM:
	  case NG_SELNEXT:
	    set_ng(np->next);
	    break;
	  case NG_NEXT:
	    set_ng(np->next);
	    goto loop_break;
	  case NG_ERROR:
	  case NG_ASK:
	    goto loop_break;
	  case NG_SELPRIOR:
	    while ((np = np->prev) != nullptr) {
		if (np->flags & NF_VISIT)
		    goto do_group;
	    }
	    (void) first_page();
	    goto loop_break;
	  case NG_MINUS:
	    np = g_recent_ng;
#if 0
/* CAA: I'm not sure why I wrote this originally--it seems unnecessary */
	    np->flags |= NF_VISIT;
#endif
	    goto do_group;
	  case NG_NOSERVER:
	    nntp_server_died(np->rc->datasrc);
	    (void) first_page();
	    break;
	  /* CAA extensions */
	  case NG_GO_ARTICLE:
	    np = g_ng_go_ngptr;
	    goto do_group;
	  /* later: possible go-to-newsgroup (applicable?) */
	}
    }
  loop_break:
    g_selected_count = save_selected_count;
}

char multirc_selector()
{
    save_selector_modes saver('c');

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;

    set_selector(SM_MULTIRC, SS_MAGIC_NUMBER);

  sel_restart:
    s_end_char = g_newsrc_sel_cmds[0];
    s_page_char = g_newsrc_sel_cmds[1];
    g_sel_mask = AGF_SEL;
    s_extra_commands = multirc_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
	goto sel_restart;

    newline();
    if (g_mousebar_cnt)
	clear_rest();

    if (s_sel_ret=='\r' || s_sel_ret=='\n' || s_sel_ret=='Z' || s_sel_ret=='\t') {
	MULTIRC* mp;
	NEWSRC* rp;
	PUSH_SELECTOR();
	for (mp = multirc_low(); mp; mp = multirc_next(mp)) {
	    if (mp->flags & MF_SEL) {
		mp->flags &= ~MF_SEL;
		save_selected_count--;
		for (rp = mp->first; rp; rp = rp->next)
		    rp->datasrc->flags &= ~DF_UNAVAILABLE;
		if (use_multirc(mp)) {
		    find_new_groups();
		    do_multirc();
		    unuse_multirc(mp);
		}
		else
		    mp->flags &= ~MF_INCLUDED;
	    }
	}
	POP_SELECTOR();
	goto sel_restart;
    }
    return s_sel_ret;
}

char newsgroup_selector()
{
    save_selector_modes saver('w');

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;

    set_selector(SM_NEWSGROUP, SS_MAGIC_NUMBER);

  sel_restart:
    if (*g_sel_grp_dmode != 's') {
	NEWSRC* rp;
	for (rp = g_multirc->first; rp; rp = rp->next) {
	    if ((rp->flags & RF_ACTIVE) && !rp->datasrc->desc_sf.hp) {
		find_grpdesc(rp->datasrc, "control");
		if (rp->datasrc->desc_sf.fp)
		    rp->datasrc->flags |= DF_NOXGTITLE; /*$$ ok?*/
		else
		    rp->datasrc->desc_sf.refetch_secs = 0;
	    }
	}
    }

    s_end_char = g_newsgroup_sel_cmds[0];
    s_page_char = g_newsgroup_sel_cmds[1];
    if (g_sel_rereading) {
	g_sel_mask = AGF_DELSEL;
	g_sel_page_np = nullptr;
    } else
	g_sel_mask = AGF_SEL;
    s_extra_commands = newsgroup_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
	goto sel_restart;

    newline();
    if (g_mousebar_cnt)
	clear_rest();

    if (s_sel_ret == '\r' || s_sel_ret == '\n' || s_sel_ret == 'Z' || s_sel_ret == '\t' || s_sel_ret == ';')
    {
	NGDATA* np;
	PUSH_SELECTOR();
	for (np = g_first_ng; np; np = np->next) {
	    if ((np->flags & NF_INCLUDED)
	     && (!g_selected_count || (np->flags & g_sel_mask)))
		np->flags |= NF_VISIT;
	    else
		np->flags &= ~NF_VISIT;
	}
	sel_dogroups();
	save_selected_count = g_selected_count;
	POP_SELECTOR();
	if (g_multirc)
	    goto sel_restart;
	s_sel_ret = 'q';
    }
    sel_cleanup();
    end_only();
    return s_sel_ret;
}

char addgroup_selector(getnewsgroup_flags flags)
{
    save_selector_modes saver('j');

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;

    set_selector(SM_ADDGROUP, SS_MAGIC_NUMBER);

  sel_restart:
    if (*g_sel_grp_dmode != 's') {
	NEWSRC* rp;
	for (rp = g_multirc->first; rp; rp = rp->next) {
	    if ((rp->flags & RF_ACTIVE) && !rp->datasrc->desc_sf.hp) {
		find_grpdesc(rp->datasrc, "control");
		if (!rp->datasrc->desc_sf.fp)
		    rp->datasrc->desc_sf.refetch_secs = 0;
	    }
	}
    }

    s_end_char = g_add_sel_cmds[0];
    s_page_char = g_add_sel_cmds[1];
    /* Setup for selecting articles to read or set unread */
    if (g_sel_rereading)
	g_sel_mask = AGF_DELSEL;
    else
	g_sel_mask = AGF_SEL;
    g_sel_page_gp = nullptr;
    s_extra_commands = addgroup_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
	goto sel_restart;

    g_selected_count = 0;
    newline();
    if (g_mousebar_cnt)
	clear_rest();

    if (s_sel_ret=='\r' || s_sel_ret=='\n' || s_sel_ret=='Z' || s_sel_ret=='\t') {
	ADDGROUP *gp;
	int i;
	g_addnewbydefault = ADDNEW_SUB;
	for (gp = g_first_addgroup, i = 0; gp; gp = gp->next, i++) {
	    if (gp->flags & AGF_SEL) {
		gp->flags &= ~AGF_SEL;
		get_ng(gp->name,flags);
	    }
	}
	g_addnewbydefault = 0;
    }
    sel_cleanup();
    return s_sel_ret;
}

char option_selector()
{
    int i;
    char** vals = ini_values(g_options_ini);
    save_selector_modes saver('l');

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;
    parse_ini_section("", g_options_ini);

    set_selector(SM_OPTIONS, SS_MAGIC_NUMBER);

  sel_restart:
    s_end_char = g_option_sel_cmds[0];
    s_page_char = g_option_sel_cmds[1];
    if (g_sel_rereading)
	g_sel_mask = AGF_DELSEL;
    else
	g_sel_mask = AGF_SEL;
    g_sel_page_op = -1;
    s_extra_commands = option_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R' || s_sel_ret=='\r' || s_sel_ret=='\n')
	goto sel_restart;

    g_selected_count = 0;
    newline();
    if (g_mousebar_cnt)
	clear_rest();

    if (s_sel_ret=='Z' || s_sel_ret=='\t' || s_sel_ret == 'S') {
	set_options(vals);
	if (s_sel_ret == 'S')
	    save_options(g_ini_file);
    }
    for (i = 1; g_options_ini[i].checksum; i++) {
	if (vals[i]) {
	    if (g_option_saved_vals[i] && !strcmp(vals[i],g_option_saved_vals[i])) {
		if (g_option_saved_vals[i] != g_option_def_vals[i])
		    free(g_option_saved_vals[i]);
		g_option_saved_vals[i] = nullptr;
	    }
	    free(vals[i]);
	    vals[i] = nullptr;
	}
    }
    return s_sel_ret;
}

/* returns a command to do */
static univ_read_result univ_read(UNIV_ITEM *ui)
{
    univ_read_result exit_code = UR_NORM;
    char ch;

    g_univ_follow_temp = false;
    if (!ui) {
	printf("nullptr UI passed to reader!\n") FLUSH;
	sleep(5);
	return exit_code;
    }
    printf("\n") FLUSH;			/* prepare for output msgs... */
    switch (ui->type) {
      case UN_DEBUG1: {
	char* s;
	s = ui->data.str;
	if (s && *s) {
	    printf("Not implemented yet (%s)\n",s) FLUSH;
	    sleep(5);
	    return exit_code;
	}
	break;
      }
      case UN_TEXTFILE: {
	char* s;
	s = ui->data.textfile.fname;
	if (s && *s) {
	    /* later have some way of getting a return code back */
	    univ_page_file(s);
	}
	break;
      }
      case UN_ARTICLE: {
	int ret;
	NGDATA* np;

	if (g_in_ng) {
	    /* XXX whine: can't recurse at this time */
	    break;
	}
	if (!ui->data.virt.ng)
	    break;			/* XXX whine */
	np = find_ng(ui->data.virt.ng);

	if (!np) {
	    printf("Universal: newsgroup %s not found!",
		   ui->data.virt.ng) FLUSH;
	    sleep(5);
	    return exit_code;
	}
	set_ng(np);
	if (np != g_current_ng) {
	    g_recent_ng = g_current_ng;
	    g_current_ng = np;
	}
	g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
	printf("Virtual: Entering %s:\n", g_ngname) FLUSH;
	g_ng_go_artnum = ui->data.virt.num;
	g_univ_read_virtflag = true;
	ret = do_newsgroup("");
	g_univ_read_virtflag = false;
	switch (ret) {
	  case NG_NORM:		/* handle more cases later */
	  case NG_SELNEXT:
	  case NG_NEXT:
	    /* just continue reading */
	    break;
	  case NG_SELPRIOR:
	    /* not implemented yet */
	    /* FALL THROUGH */
	  case NG_ERROR:
	  case NG_ASK:
	    exit_code = UR_BREAK;
	    return exit_code;
	  case NG_MINUS:
	    /* not implemented */
	    break;
	  default:
	    break;
	}
	break;
      }
      case UN_GROUPMASK: {
	univ_mask_load(ui->data.gmask.masklist,ui->data.gmask.title);
	ch = universal_selector();
	switch (ch) {
	  case 'q':
	    exit_code = UR_BREAK;
	    break;
	  default:
	    exit_code = UR_NORM;
	    break;
	}
	return exit_code;
      }
      case UN_CONFIGFILE: {
	univ_file_load(ui->data.cfile.fname,ui->data.cfile.title,
		       ui->data.cfile.label);
	ch = universal_selector();
	switch (ch) {
	  case 'q':
	    exit_code = UR_BREAK;
	    break;
	  default:
	    exit_code = UR_NORM;
	    break;
	}
	return exit_code;
      }
      case UN_NEWSGROUP: {
	int ret;
	NGDATA* np;

	if (g_in_ng) {
	    /* XXX whine: can't recurse at this time */
	    break;
	}
	if (!ui->data.group.ng)
	    break;			/* XXX whine */
	np = find_ng(ui->data.group.ng);

	if (!np) {
	    printf("Universal: newsgroup %s not found!",
		   ui->data.group.ng) FLUSH;
	    sleep(5);
	    return exit_code;
	}
      do_group:
	set_ng(np);
	if (np != g_current_ng) {
	    g_recent_ng = g_current_ng;
	    g_current_ng = np;
	}
	g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
	printf("Entering %s:", g_ngname) FLUSH;
	if (s_sel_ret == ';')
	    ret = do_newsgroup(savestr(";"));
	else
	    ret = do_newsgroup("");
	switch (ret) {
	  case NG_NORM:		/* handle more cases later */
	  case NG_SELNEXT:
	  case NG_NEXT:
	    /* just continue reading */
	    break;
	  case NG_SELPRIOR:
	    /* not implemented yet */
	    /* FALL THROUGH */
	  case NG_ERROR:
	  case NG_ASK:
	    exit_code = UR_BREAK;
	    return exit_code;
	  case NG_MINUS:
	    np = g_recent_ng;
	    goto do_group;
	  case NG_NOSERVER:
	    /* Eeep! */
	    break;
	}
	break;
      }
      case UN_HELPKEY:
	if (another_command(univ_key_help(ui->data.i)))
	    pushchar(s_sel_ret | 0200);
	break;
      default:
	break;
    }
    return exit_code;
}

char universal_selector()
{
    save_selector_modes saver('v');		/* kind of like 'v'irtual... */

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;

    set_selector(SM_UNIVERSAL, SS_MAGIC_NUMBER);

    g_selected_count = 0;

sel_restart:
    /* make options */
    s_end_char = 'Z';
    s_page_char = '>';

    /* Setup for selecting articles to read or set unread */
    if (g_sel_rereading)
	g_sel_mask = AGF_DELSEL;
    else
	g_sel_mask = AGF_SEL;
    sel_page_univ = nullptr;
    s_extra_commands = universal_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
	goto sel_restart;

    newline();
    if (g_mousebar_cnt)
	clear_rest();

    if (s_sel_ret == '\r' || s_sel_ret == '\n' || s_sel_ret == '\t' || s_sel_ret == ';' || s_sel_ret == 'Z')
    {
	UNIV_ITEM *ui;
	int i;
	for (ui = g_first_univ, i = 0; ui; ui = ui->next, i++) {
            if (ui->flags & UF_SEL) {
		PUSH_SELECTOR();
		PUSH_UNIV_SELECTOR();

		ui->flags &= ~UF_SEL;
		save_selected_count--;
		univ_read_result ret = univ_read(ui);

		POP_UNIV_SELECTOR();
		POP_SELECTOR();
		if (ret == UR_ERROR || ret == UR_BREAK) {
		    s_sel_ret = ' ';	/* don't leave completely. */
		    break;		/* jump out of for loop */
		}
	    }
	}
    }
/*univ_loop_break:*/
    /* restart the selector unless the user explicitly quits.
     * possibly later have an option for 'Z' to quit levels>1.
     */
    if (s_sel_ret != 'q' && (s_sel_ret != 'Q'))
	goto sel_restart;
    sel_cleanup();
    univ_close();
    return s_sel_ret;
}

static void sel_display()
{
    /* Present a page of items to the user */
    display_page();
    if (g_erase_screen && g_erase_each_line)
	erase_line(true);

    if (g_sel_item_index >= g_sel_page_item_cnt)
	g_sel_item_index = 0;

    if (s_disp_status_line == 1) {
	newline();
	fputs(g_msg, stdout);
	g_term_col = strlen(g_msg);
	s_disp_status_line = 2;
    }
}

static void sel_status_msg(const char *cp)
{
    if (g_can_home)
	goto_xy(0,g_sel_last_line+1);
    else
	newline();
    fputs(cp, stdout);
    g_term_col = strlen(cp);
    goto_xy(0,g_sel_items[g_sel_item_index].line);
    fflush(stdout);	/* CAA: otherwise may not be visible */
    s_disp_status_line = 2;
}

static char sel_input()
{
    int j;
    int ch, action;
    char* in_select;
    int got_dash, got_goto;
    int sel_number;
    int ch_num1;

    /* CAA: TRN proudly continues the state machine traditions of RN.
     *      April 2, 1996: 28 gotos in this function.  Conversion to
     *      structured programming is left as an exercise for the reader.
     */
    /* If one immediately types a goto command followed by a dash ('-'),
     * the following will be the default action.
     */
    action = '+';
  reask_selector:
    /* Prompt the user */
    sel_prompt();
  position_selector:
    got_dash = got_goto = 0;
    s_force_sel_pos = -1;
    if (s_removed_prompt & 1) {
	draw_mousebar(g_tc_COLS,false);
	s_removed_prompt &= ~1;
    }
    if (g_can_home)
	goto_xy(0,g_sel_items[g_sel_item_index].line);

reinp_selector:
    if (s_removed_prompt & 1)
	goto position_selector;	/* (CAA: TRN considered harmful? :-) */
    /* Grab some commands from the user */
    fflush(stdout);
    eat_typeahead();
    if (g_use_sel_num)
	g_spin_char = '0' + (g_sel_item_index+1)/10;	/* first digit */
    else
	g_spin_char = g_sel_chars[g_sel_item_index];
    cache_until_key();
    getcmd(g_buf);
    if (*g_buf == ' ')
	setdef(g_buf, g_sel_at_end? &s_end_char : &s_page_char);
    ch = *g_buf;
    if (errno)
	ch = Ctl('l');
    if (s_disp_status_line == 2) {
	if (g_can_home) {
	    goto_xy(0,g_sel_last_line+1);
	    erase_line(false);
	    if (g_term_line == g_tc_LINES-1)
		s_removed_prompt |= 1;
	}
	s_disp_status_line = 0;
    }
    if (ch == '-' && g_sel_page_item_cnt) {
	got_dash = 1;
	got_goto = 0;	/* right action is not clear if both are true */
	if (g_can_home) {
	    if (!input_pending()) {
		j = (g_sel_item_index > 0? g_sel_item_index : g_sel_page_item_cnt);
		if (g_use_sel_num)
		    sprintf(g_msg,"Range: %d-", j);
		else
		    sprintf(g_msg,"Range: %c-", g_sel_chars[j-1]);
		sel_status_msg(g_msg);
	    }
	}
	else {
	    putchar('-');
	    fflush(stdout);
	}
	goto reinp_selector;
    }
    /* allow the user to back out of a range or a goto with erase char */
    if (ch == g_erase_char || ch == g_kill_char) {
	/* later consider dingaling() if neither got_{dash,goto} is true */
	got_dash = 0;
	got_goto = 0;
	/* following if statement should be function */
	if (s_disp_status_line == 2) {	/* status was printed */
	    if (g_can_home) {
		goto_xy(0,g_sel_last_line+1);
		erase_line(false);
		if (g_term_line == g_tc_LINES-1)
		    s_removed_prompt |= 1;
	    }
	    s_disp_status_line = 0;
	}
	goto position_selector;
    }
    if (ch == Ctl('g')) {
	got_goto = 1;
	got_dash = 0;	/* right action is not clear if both are true */
	if (!input_pending()) {
	    if (g_use_sel_num)
		sel_status_msg("Go to number?");
	    else
		sel_status_msg("Go to letter?");
	}
	goto reinp_selector;
    }
    if (g_sel_mode == SM_OPTIONS && (ch == '\r' || ch == '\n'))
	ch = '.';
    in_select = strchr(g_sel_chars, ch);
    if (g_use_sel_num && ch >= '0' && ch <= '9') {
	ch_num1 = ch;
	/* would be *very* nice to use wait_key_pause() here */
	if (!input_pending()) {
	    if (got_dash) {
		if (g_sel_item_index > 0) {
		    j = g_sel_item_index;  /* -1, +1 to print */
		} else	/* wrap around from the bottom */
		    j = g_sel_page_item_cnt;
		sprintf(g_msg,"Range: %d-%c", j, ch);
	    } else {
		if (got_goto) {
		    sprintf(g_msg,"Go to number: %c", ch);
		} else {
		    sprintf(g_msg,"%c", ch);
		}
	    }
	    sel_status_msg(g_msg);
	}	
	/* Consider cache_until_key() here.  The middle of typing a
	 * number is a lousy time to delay, however.
	 */
	getcmd(g_buf);
	ch = *g_buf;
	if (s_disp_status_line == 2) {	/* status was printed */
	    if (g_can_home) {
		goto_xy(0,g_sel_last_line+1);
		erase_line(false);
		if (g_term_line == g_tc_LINES-1)
		    s_removed_prompt |= 1;
	    }
	    s_disp_status_line = 0;
	}
	if (ch == g_kill_char) {	/* kill whole command in progress */
	    got_goto = 0;
	    got_dash = 0;
	    goto position_selector;
	}
	if (ch == g_erase_char) {
	    /* Erase any first digit printed, but allow complex
	     * commands to continue.  Spaces at end of message are
	     * there to wipe out old first digit.
	     */
	    if (got_dash) {
		if (g_sel_item_index > 0) {
		    j = g_sel_item_index;  /* -1, +1 to print */
		} else	/* wrap around from the bottom */
		    j = g_sel_page_item_cnt;
		sprintf(g_msg,"Range: %d- ", j);
		sel_status_msg(g_msg);
		goto reinp_selector;
	    }
	    if (got_goto) {
		sel_status_msg("Go to number?  ");
		goto reinp_selector;
	    }
	    goto position_selector;
	}
	if (ch >= '0' && ch <= '9')
	    sel_number = ((ch_num1 - '0') * 10) + (ch - '0');
	else {
	    pushchar(ch);	/* for later use */
	    sel_number = (ch_num1 - '0');
	}
	j = sel_number-1;
	if ((j < 0) || (j >= g_sel_page_item_cnt)) {
	    dingaling();
	    sprintf(g_msg, "No item %c%c on this page.", ch_num1, ch);
	    sel_status_msg(g_msg);
	    goto position_selector;
	}
	else if (got_goto || (g_sel_num_goto && !got_dash)) {
	    /* (but only do always-goto if there was not a dash) */
	    g_sel_item_index = j;
	    goto position_selector;
	} else if (got_dash)
	    ;
	else if (g_sel_items[j].sel == 1)
	    action = (g_sel_rereading? 'k' : '-');
	else
	    action = '+';
    } else if (in_select && !g_use_sel_num) {
	j = in_select - g_sel_chars;
	if (j >= g_sel_page_item_cnt) {
	    dingaling();
	    sprintf(g_msg, "No item '%c' on this page.", ch);
	    sel_status_msg(g_msg);
	    goto position_selector;
	}
	else if (got_goto) {
	    g_sel_item_index = j;
	    goto position_selector;
	} else if (got_dash)
	    ;
	else if (g_sel_items[j].sel == 1)
	    action = (g_sel_rereading? 'k' : '-');
	else
	    action = '+';
    } else if (ch == '*' && g_sel_mode == SM_ARTICLE) {
	ARTICLE* ap;
	if (!g_sel_page_item_cnt)
	    dingaling();
	else {
	    ap = g_sel_items[g_sel_item_index].u.ap;
	    if (g_sel_items[g_sel_item_index].sel)
		deselect_subject(ap->subj);
	    else
		select_subject(ap->subj, AUTO_KILL_NONE);
	    update_page();
	}
	goto position_selector;
    } else if (ch == 'y' || ch == '.' || ch == '*' || ch == Ctl('t')) {
	j = g_sel_item_index;
	if (g_sel_page_item_cnt && g_sel_items[j].sel == 1)
	    action = (g_sel_rereading? 'k' : '-');
	else
	    action = '+';
	if (ch == Ctl('t'))
	    s_force_sel_pos = j;
    } else if (ch == 'k' || ch == 'j' || ch == ',') {
	j = g_sel_item_index;
	action = 'k';
    } else if (ch == 'm' || ch == '|') {
	j = g_sel_item_index;
	action = '-';
    } else if (ch == 'M' && g_in_ng) {
	j = g_sel_item_index;
	action = 'M';
    } else if (ch == '@') {
	g_sel_item_index = 0;
	j = g_sel_page_item_cnt-1;
	got_dash = 1;
	action = '@';
    } else if (ch == '[' || ch == 'p') {
	if (--g_sel_item_index < 0)
	    g_sel_item_index = g_sel_page_item_cnt? g_sel_page_item_cnt-1 : 0;
	goto position_selector;
    } else if (ch == ']' || ch == 'n') {
	if (++g_sel_item_index >= g_sel_page_item_cnt)
	    g_sel_item_index = 0;
	goto position_selector;
    } else {
	s_sel_ret = ch;
	switch (sel_command(ch)) {
	  case DS_ASK:
	    if (!s_clean_screen) {
		sel_display();
		goto reask_selector;
	    }
	    if (s_removed_prompt & 2) {
		carriage_return();
		goto reask_selector;
	    }
	    goto position_selector;
	  case DS_DISPLAY:
	    sel_display();
	    goto reask_selector;
	  case DS_UPDATE:
	    if (!s_clean_screen) {
		sel_display();
		goto reask_selector;
	    }
	    if (s_disp_status_line == 1) {
		newline();
		fputs(g_msg,stdout);
		g_term_col = strlen(g_msg);
		if (s_removed_prompt & 1) {
		    draw_mousebar(g_tc_COLS,false);
		    s_removed_prompt &= ~1;
		}
		s_disp_status_line = 2;
	    }
	    update_page();
	    if (g_can_home)
		goto_xy(0,g_sel_last_line);
	    goto reask_selector;
	  case DS_RESTART:
	    return 'R'; /*Restart*/
	  case DS_QUIT:
	    return 'Q'; /*Quit*/
	  case DS_STATUS:
	    s_disp_status_line = 1;
	    if (!s_clean_screen) {
		sel_display();
		goto reask_selector;
	    }
	    sel_status_msg(g_msg);

	    if (!g_can_home)
		newline();
	    if (s_removed_prompt & 2)
		goto reask_selector;
	    goto position_selector;
	}
    }

    if (!g_sel_page_item_cnt) {
	dingaling();
	goto position_selector;
    }

    /* The user manipulated one of the letters -- handle it. */
    if (!got_dash)
	g_sel_item_index = j;
    else {
	if (j < g_sel_item_index) {
	    ch = g_sel_item_index-1;
	    g_sel_item_index = j;
	    j = ch;
	}
    }

    if (++j == g_sel_page_item_cnt)
	j = 0;
    do {
	int sel = g_sel_items[g_sel_item_index].sel;
	if (g_can_home)
	    goto_xy(0,g_sel_items[g_sel_item_index].line);
	if (action == '@') {
	    if (sel == 2)
		ch = (g_sel_rereading? '+' : ' ');
	    else if (g_sel_rereading)
		ch = 'k';
	    else if (sel == 1)
		ch = '-';
	    else
		ch = '+';
	} else
	    ch = action;
	switch (ch) {
	  case '+':
	    if (select_item(g_sel_items[g_sel_item_index].u))
		output_sel(g_sel_item_index, 1, true);
	    if (g_term_line >= g_sel_last_line) {
		sel_display();
		goto reask_selector;
	    }
	    break;
	  case '-':  case 'k':  case 'M': {
	    bool sel_reread_save = g_sel_rereading;
	    if (ch == 'M')
		delay_return_item(g_sel_items[g_sel_item_index].u);
	    if (ch == '-')
		g_sel_rereading = false;
	    else
		g_sel_rereading = true;
	    if (deselect_item(g_sel_items[g_sel_item_index].u))
		output_sel(g_sel_item_index, ch == '-' ? 0 : 2, true);
	    g_sel_rereading = sel_reread_save;
	    if (g_term_line >= g_sel_last_line) {
		sel_display();
		goto reask_selector;
	    }
	    break;
	  }
	}
	if (g_can_home)
	    carriage_return();
	fflush(stdout);
	if (++g_sel_item_index == g_sel_page_item_cnt)
	    g_sel_item_index = 0;
    } while (g_sel_item_index != j);
    if (s_force_sel_pos >= 0)
	g_sel_item_index = s_force_sel_pos;
    goto position_selector;
}

static void sel_prompt()
{
    draw_mousebar(g_tc_COLS,false);
    if (g_can_home)
	goto_xy(0,g_sel_last_line);
#ifdef MAILCALL
    setmail(false);
#endif
    if (g_sel_at_end)
	sprintf(g_cmd_buf, "%s [%c%c] --",
		(!g_sel_prior_obj_cnt? "All" : "Bot"), s_end_char, s_page_char);
    else
	sprintf(g_cmd_buf, "%s%ld%% [%c%c] --",
		(!g_sel_prior_obj_cnt? "Top " : ""),
		(long)((g_sel_prior_obj_cnt+g_sel_page_obj_cnt)*100 / g_sel_total_obj_cnt),
		s_page_char, s_end_char);
    interp(g_buf, sizeof g_buf, g_mailcall);
    sprintf(g_msg, "%s-- %s %s (%s%s order) -- %s", g_buf,
	    g_sel_exclusive && g_in_ng? "SELECTED" : "Select", g_sel_mode_string,
	    g_sel_direction<0? "reverse " : "", g_sel_sort_string, g_cmd_buf);
    color_string(COLOR_CMD,g_msg);
    g_term_col = strlen(g_msg);
    s_removed_prompt = 0;
}

static bool select_item(SEL_UNION u)
{
    switch (g_sel_mode) {
      case SM_MULTIRC:
	// multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
        TRN_ASSERT((g_sel_mask & (AGF_DEL | AGF_DELSEL)) == 0);
	if (!(u.mp->flags & static_cast<multirc_flags>(g_sel_mask)))
	    g_selected_count++;
	u.mp->flags |= static_cast<multirc_flags>(g_sel_mask);
	break;
      case SM_ADDGROUP:
	if (!(u.gp->flags & g_sel_mask))
	    g_selected_count++;
	u.gp->flags = (u.gp->flags & ~AGF_DEL) | g_sel_mask;
	break;
      case SM_NEWSGROUP:
	if (!(u.np->flags & g_sel_mask))
	    g_selected_count++;
	u.np->flags = (u.np->flags & ~NF_DEL) | static_cast<newsgroup_flags>(g_sel_mask);
	break;
      case SM_OPTIONS:
	if (!select_option(u.op) || !ini_value(g_options_ini, u.op))
	    return false;
	break;
      case SM_THREAD:
	select_thread(u.sp->thread, AUTO_KILL_NONE);
	break;
      case SM_SUBJECT:
	select_subject(u.sp, AUTO_KILL_NONE);
	break;
      case SM_ARTICLE:
	select_article(u.ap, AUTO_KILL_NONE);
	break;
      case SM_UNIVERSAL:
	if (!(u.un->flags & g_sel_mask))
	    g_selected_count++;
	u.un->flags = (u.un->flags & ~UF_DEL) | static_cast<univitem_flags>(g_sel_mask);
	break;
    }
    return true;
}

static bool delay_return_item(SEL_UNION u)
{
    switch (g_sel_mode) {
      case SM_MULTIRC:
      case SM_ADDGROUP:
      case SM_NEWSGROUP:
      case SM_OPTIONS:
      case SM_UNIVERSAL:
	return false;
      case SM_ARTICLE:
	delay_unmark(u.ap);
	break;
      default: {
	  ARTICLE* ap;
	  if (g_sel_mode == SM_THREAD) {
	      for (ap = first_art(u.sp); ap; ap = next_art(ap))
		  if (!!(ap->flags & AF_UNREAD) ^ g_sel_rereading)
		      delay_unmark(ap);
	  } else {
	      for (ap = u.sp->articles; ap; ap = ap->subj_next)
		  if (!!(ap->flags & AF_UNREAD) ^ g_sel_rereading)
		      delay_unmark(ap);
	  }
	  break;
      }
    }
    return true;
}

static bool deselect_item(SEL_UNION u)
{
    switch (g_sel_mode) {
      case SM_MULTIRC:
	// multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
        TRN_ASSERT((g_sel_mask & (AGF_DEL | AGF_DELSEL)) == 0);
	if (u.mp->flags & static_cast<multirc_flags>(g_sel_mask)) {
	    u.mp->flags &= ~static_cast<multirc_flags>(g_sel_mask);
	    g_selected_count--;
	}
#if 0
	if (g_sel_rereading)
	    u.mp->flags |= MF_DEL;
#endif
	break;
      case SM_ADDGROUP:
	if (u.gp->flags & g_sel_mask) {
	    u.gp->flags &= ~g_sel_mask;
	    g_selected_count--;
	}
	if (g_sel_rereading)
	    u.gp->flags |= AGF_DEL;
	break;
      case SM_NEWSGROUP:
	if (u.np->flags & static_cast<newsgroup_flags>(g_sel_mask)) {
	    u.np->flags &= ~static_cast<newsgroup_flags>(g_sel_mask);
	    g_selected_count--;
	}
	if (g_sel_rereading)
	    u.np->flags |= NF_DEL;
	break;
      case SM_OPTIONS:
	if (!select_option(u.op) || ini_value(g_options_ini, u.op))
	    return false;
	break;
      case SM_THREAD:
	deselect_thread(u.sp->thread);
	break;
      case SM_SUBJECT:
	deselect_subject(u.sp);
	break;
      case SM_UNIVERSAL:
	if (u.un->flags & static_cast<univitem_flags>(g_sel_mask)) {
	    u.un->flags &= ~static_cast<univitem_flags>(g_sel_mask);
	    g_selected_count--;
	}
	if (g_sel_rereading)
	    u.un->flags |= UF_DEL;
	break;
      default:
	deselect_article(u.ap, AUTO_KILL_NONE);
	break;
    }
    return true;
}

static bool select_option(int i)
{
    bool changed = false;
    char** vals = ini_values(g_options_ini);
    char* val;
    char* oldval;

    if (*g_options_ini[i].item == '*') {
	g_option_flags[i] ^= OF_SEL;
	init_pages(FILL_LAST_PAGE);
	g_term_line = g_sel_last_line;
	return false;
    }

    goto_xy(0,g_sel_last_line);
    erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
    color_object(COLOR_CMD, true);
    printf("Change `%s' (%s)",g_options_ini[i].item, g_options_ini[i].help_str);
    color_pop();	/* of COLOR_CMD */
    newline();
    *g_buf = '\0';
    oldval = savestr(quote_string(option_value(i)));
    val = vals[i]? vals[i] : oldval;
    s_clean_screen = in_choice("> ", val, g_options_ini[i].help_str, 'z');
    if (strcmp(g_buf,val)) {
	char* to = g_buf;
	char* from = g_buf;
	parse_string(&to, &from);
	changed = true;
	if (vals[i]) {
	    free(vals[i]);
	    g_selected_count--;
	}
	if (val != oldval && !strcmp(g_buf,oldval))
	    vals[i] = nullptr;
	else {
	    vals[i] = savestr(g_buf);
	    g_selected_count++;
	}
    }
    free(oldval);
    if (s_clean_screen) {
	up_line();
	erase_line(true);
	sel_prompt();
	goto_xy(0,g_sel_items[g_sel_item_index].line);
	if (changed) {
	    erase_line(false);
	    display_option(i,g_sel_item_index);
	    up_line();
	}
    }
    else
	return false;
    return true;
}

static void sel_cleanup()
{
    switch (g_sel_mode) {
      case SM_MULTIRC:
	break;
      case SM_ADDGROUP:
	if (g_sel_rereading) {
	    ADDGROUP* gp;
	    for (gp = g_first_addgroup; gp; gp = gp->next) {
		if (gp->flags & AGF_DELSEL) {
		    if (!(gp->flags & AGF_SEL))
			g_selected_count++;
		    gp->flags = (gp->flags&~(AGF_DELSEL|AGF_EXCLUDED))|AGF_SEL;
		}
		gp->flags &= ~AGF_DEL;
	    }
	}
	else {
	    ADDGROUP* gp;
	    for (gp = g_first_addgroup; gp; gp = gp->next) {
		if (gp->flags & AGF_DEL)
		    gp->flags = (gp->flags & ~AGF_DEL) | AGF_EXCLUDED;
	    }
	}
	break;
      case SM_NEWSGROUP:
	if (g_sel_rereading) {
	    NGDATA* np;
	    for (np = g_first_ng; np; np = np->next) {
		if (np->flags & NF_DELSEL) {
		    if (!(np->flags & NF_SEL))
			g_selected_count++;
		    np->flags = (np->flags & ~NF_DELSEL) | NF_SEL;
		}
		np->flags &= ~NF_DEL;
	    }
	}
	else {
	    NGDATA* np;
	    for (np = g_first_ng; np; np = np->next) {
		if (np->flags & NF_DEL) {
		    np->flags &= ~NF_DEL;
		    catch_up(np, 0, 0);
		}
	    }
	}
	break;
      case SM_OPTIONS:
	break;
      /* should probably be expanded... */
      case SM_UNIVERSAL:
	break;
      default:
	if (g_sel_rereading) {
	    /* Turn selections into unread selected articles.  Let
	    ** count_subjects() fix the counts after we're through.
	    */
	    SUBJECT* sp;
	    g_sel_last_ap = nullptr;
	    g_sel_last_sp = nullptr;
	    for (sp = g_first_subject; sp; sp = sp->next)
		unkill_subject(sp);
	}
	else {
	    if (g_sel_mode == SM_ARTICLE)
		article_walk(mark_DEL_as_READ, 0);
	    else {
		SUBJECT* sp;
		for (sp = g_first_subject; sp; sp = sp->next) {
		    if (sp->flags & SF_DEL) {
			sp->flags &= ~SF_DEL;
			if (g_sel_mode == SM_THREAD)
			    kill_thread(sp->thread, AFFECT_UNSEL);
			else
			    kill_subject(sp, AFFECT_UNSEL);
		    }
		}
	    }
	}
	break;
    }
}

static bool mark_DEL_as_READ(char *ptr, int arg)
{
    ARTICLE* ap = (ARTICLE*)ptr;
    if (ap->flags & AF_DEL) {
	ap->flags &= ~AF_DEL;
	set_read(ap);
    }
    return false;
}

static display_state sel_command(char_int ch)
{
    display_state ret;
    if (g_can_home)
	goto_xy(0,g_sel_last_line);
    s_clean_screen = true;
    g_term_scrolled = 0;
    g_page_line = 1;
    if (g_sel_mode == SM_NEWSGROUP) {
	if (g_sel_item_index < g_sel_page_item_cnt)
	    set_ng(g_sel_items[g_sel_item_index].u.np);
	else
	    g_ngptr = nullptr;
    }
  do_command:
    *g_buf = ch;
    g_buf[1] = FINISHCMD;
    g_output_chase_phrase = true;
    switch (ch) {
      case '>':
	g_sel_item_index = 0;
	if (next_page())
	    return DS_DISPLAY;
	break;
      case '<':
	g_sel_item_index = 0;
	if (prev_page())
	    return DS_DISPLAY;
	break;
      case '^':  case Ctl('r'):
	g_sel_item_index = 0;
	if (first_page())
	    return DS_DISPLAY;
	break;
      case '$':
	g_sel_item_index = 0;
	if (last_page())
	    return DS_DISPLAY;
	break;
      case Ctl('l'):
	return DS_DISPLAY;
      case Ctl('^'):
	erase_line(false);		/* erase the prompt */
	s_removed_prompt |= 2;
#ifdef MAILCALL
	setmail(true);		/* force a mail check */
#endif
	break;
      case '\r':  case '\n':
	if (!g_selected_count && g_sel_page_item_cnt) {
	    if (g_sel_rereading || g_sel_items[g_sel_item_index].sel != 2)
		select_item(g_sel_items[g_sel_item_index].u);
	}
	return DS_QUIT;
      case 'Z':  case '\t':
	return DS_QUIT;
      case 'q':  case 'Q':  case '+':  case '`':
	return DS_QUIT;
      case Ctl('Q'):  case '\033':
	s_sel_ret = '\033';
	return DS_QUIT;
      case '\b':  case '\177':
	return DS_QUIT;
      case Ctl('k'):
	edit_kfile();
	return DS_DISPLAY;
      case '&':  case '!':
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (!finish_command(true)) {	/* get rest of command */
	    if (s_clean_screen)
		break;
	}
	else {
	    PUSH_SELECTOR();
	    g_one_command = true;
	    perform(g_buf, false);
	    g_one_command = false;
	    if (g_term_line != g_sel_last_line+1 || g_term_scrolled)
		s_clean_screen = false;
	    POP_SELECTOR();
	    if (!save_sel_mode)
		return DS_RESTART;
	    if (s_clean_screen) {
		erase_line(false);
		return DS_ASK;
	    }
	}
        ch = another_command(1);
        if (ch != '\0')
	    goto do_command;
	return DS_DISPLAY;
      case 'v':
	newline();
	trn_version();
        ch = another_command(1);
        if (ch != '\0')
	    goto do_command;
	return DS_DISPLAY;
      case '\\':
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (g_sel_mode == SM_NEWSGROUP)
	    printf("[%s] Cmd: ", g_ngptr? g_ngptr->rcline : "*End*");
	else
	    fputs("Cmd: ", stdout);
	fflush(stdout);
	getcmd(g_buf);
	if (*g_buf == '\\')
	    goto the_default;
	if (*g_buf != ' ' && *g_buf != '\n' && *g_buf != '\r') {
	    ch = *g_buf;
	    goto do_command;
	}
	if (s_clean_screen) {
	    erase_line(false);
	    break;
	}
        ch = another_command(1);
        if (ch != '\0')
	    goto do_command;
	return DS_DISPLAY;
      default:
      the_default:
	ret = s_extra_commands(ch);
	switch (ret) {
	  case DS_ERROR:
	    break;
	  case DS_DOCOMMAND:
	    ch = s_sel_ret;
	    goto do_command;
	  default:
	    return ret;
	}
	strcpy(g_msg,"Type ? for help.");
	settle_down();
	if (s_clean_screen)
	    return DS_STATUS;
	printf("\n%s\n",g_msg);
        ch = another_command(1);
        if (ch != '\0')
	    goto do_command;
	return DS_DISPLAY;
    }
    return DS_ASK;
}

static bool sel_perform_change(long cnt, const char *obj_type)
{
    int ret;

    carriage_return();
    if (g_page_line == 1) {
	s_disp_status_line = 1;
	if (g_term_line != g_sel_last_line+1 || g_term_scrolled)
	    s_clean_screen = false;
    }
    else
	s_clean_screen = false;

    if (g_error_occurred) {
	print_lines(g_msg, NOMARKING);
	s_clean_screen = g_error_occurred = false;
    }

    ret = perform_status_end(cnt, obj_type);
    if (ret)
	s_disp_status_line = 1; 
    if (s_clean_screen) {
	if (ret != 2) {
	    up_line();
	    return true;
	}
    }
    else if (s_disp_status_line == 1) {
	print_lines(g_msg, NOMARKING);
	s_disp_status_line = 0;
    }

    init_pages(PRESERVE_PAGE);

    return false;
}

#define SPECIAL_CMD_LETTERS "<+>^$!?&:/\\hDEJLNOPqQRSUXYZ\n\r\t\033;"

static char another_command(char_int ch)
{
    bool skip_q = !ch;
    if (ch < 0)
	return 0;
    if (ch > 1) {
	read_tty(g_buf,1);
	ch = *g_buf;
    }
    else
	ch = pause_getcmd();
    if (ch != 0 && ch != '\n' && ch != '\r' && (!skip_q || ch != 'q')) {
	if (ch > 0) {
	    /* try to optimize the screen update for some commands. */
	    if (!strchr(g_sel_chars, ch)
	     && (strchr(SPECIAL_CMD_LETTERS, ch) || ch == Ctl('k'))) {
		s_sel_ret = ch;
		return ch;
	    }
	    pushchar(ch | 0200);
	}
    }
    return '\0';
}

static display_state article_commands(char_int ch)
{
    switch (ch) {
      case 'U':
	sel_cleanup();
	g_sel_rereading = !g_sel_rereading;
	g_sel_page_sp = nullptr;
	g_sel_page_app = nullptr;
	if (!cache_range(g_sel_rereading? g_absfirst : g_firstart, g_lastart))
	    g_sel_rereading = !g_sel_rereading;
	return DS_RESTART;
      case '#':
	if (g_sel_page_item_cnt) {
	    SUBJECT* sp;
	    for (sp = g_first_subject; sp; sp = sp->next)
		sp->flags &= ~SF_SEL;
	    g_selected_count = 0;
	    deselect_item(g_sel_items[g_sel_item_index].u);
	    select_item(g_sel_items[g_sel_item_index].u);
	    if (!g_keep_the_group_static)
		g_keep_the_group_static = 2;
	}
	return DS_QUIT;
      case 'N':  case 'P':
	return DS_QUIT;
      case 'L':
	switch_dmode(&g_sel_art_dmode);	    /* sets g_msg */
	return DS_DISPLAY;
      case 'Y':
	if (!g_dmcount) {
	    strcpy(g_msg,"No marked articles to yank back.");
	    return DS_STATUS;
	}
	yankback();
	if (!g_sel_rereading)
	    sel_cleanup();
	s_disp_status_line = 1;
	count_subjects(CS_NORM);
	g_sel_page_sp = nullptr;
	g_sel_page_app = nullptr;
	init_pages(PRESERVE_PAGE);
	return DS_DISPLAY;
      case '=':
	if (!g_sel_rereading)
	    sel_cleanup();
	if (g_sel_mode == SM_ARTICLE) {
	    set_selector(g_sel_threadmode, SS_MAGIC_NUMBER);
	    g_sel_page_sp = g_sel_page_app? g_sel_page_app[0]->subj : nullptr;
	} else {
	    set_selector(SM_ARTICLE, SS_MAGIC_NUMBER);
	    g_sel_page_app = 0;
	}
	count_subjects(CS_NORM);
	g_sel_item_index = 0;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'S':
	if (!g_sel_rereading)
	    sel_cleanup();
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
      reask_output:
	in_char("Selector mode:  Threads, Subjects, Articles?", 'o', "tsa");
	printcmd();
	if (*g_buf == 'h' || *g_buf == 'H') {
	    if (g_verbose)
		fputs("\n\
Type t or SP to display/select thread groups (threads the group, if needed).\n\
Type s to display/select subject groups.\n\
Type a to display/select individual articles.\n\
Type q to leave things as they are.\n\n\
",stdout) FLUSH;
	    else
		fputs("\n\
t or SP selects thread groups (threads the group too).\n\
s selects subject groups.\n\
a selects individual articles.\n\
q does nothing.\n\n\
",stdout) FLUSH;
	    s_clean_screen = false;
	    goto reask_output;
	} else if (*g_buf == 'q') {
	    if (g_can_home)
		erase_line(false);
	    return DS_ASK;
	}
	if (isupper(*g_buf))
	    *g_buf = tolower(*g_buf);
	set_sel_mode(*g_buf);
	count_subjects(CS_NORM);
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'O':
	if (!g_sel_rereading)
	    sel_cleanup();
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
      reask_sort:
	if (g_sel_mode == SM_ARTICLE)
	    in_char(
                "Order by Date,Subject,Author,Number,subj-date Groups,Points?",
                'q', "dsangpDSANGP");
	else
	    in_char("Order by Date, Subject, Count, Lines, or Points?",
                    'q', "dsclpDSCLP");
	printcmd();
	if (*g_buf == 'h' || *g_buf == 'H') {
	    if (g_verbose) {
		fputs("\n\
Type d or SP to order the displayed items by date.\n\
Type s to order the items by subject.\n\
Type p to order the items by score points.\n\
",stdout) FLUSH;
		if (g_sel_mode == SM_ARTICLE)
		    fputs("\
Type a to order the items by author.\n\
Type n to order the items by number.\n\
Type g to order the items in subject-groups by date.\n\
",stdout) FLUSH;
		else
		    fputs("\
Type c to order the items by count.\n\
",stdout) FLUSH;
		fputs("\
Typing your selection in upper case it will reverse the selected order.\n\
Type q to leave things as they are.\n\n\
",stdout) FLUSH;
	    }
	    else
	    {
		fputs("\n\
d or SP sorts by date.\n\
s sorts by subject.\n\
p sorts by points.\n\
",stdout) FLUSH;
		if (g_sel_mode == SM_ARTICLE)
		    fputs("\
a sorts by author.\n\
g sorts in subject-groups by date.\n\
",stdout) FLUSH;
		else
		    fputs("\
c sorts by count.\n\
",stdout) FLUSH;
		fputs("\
Upper case reverses the sort.\n\
q does nothing.\n\n\
",stdout) FLUSH;
	    }
	    s_clean_screen = false;
	    goto reask_sort;
	} else if (*g_buf == 'q') {
	    if (g_can_home)
		erase_line(false);
	    return DS_ASK;
	}
	set_sel_sort(g_sel_mode,*g_buf);
	count_subjects(CS_NORM);
	g_sel_page_sp = nullptr;
	g_sel_page_app = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'R':
	if (!g_sel_rereading)
	    sel_cleanup();
	set_selector(g_sel_mode, static_cast<sel_sort_mode>(g_sel_sort * -g_sel_direction));
	count_subjects(CS_NORM);
	g_sel_page_sp = nullptr;
	g_sel_page_app = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'E':
	if (!g_sel_rereading)
	    sel_cleanup();
	g_sel_exclusive = !g_sel_exclusive;
	count_subjects(CS_NORM);
	g_sel_page_sp = nullptr;
	g_sel_page_app = nullptr;
	init_pages(FILL_LAST_PAGE);
	g_sel_item_index = 0;
	return DS_DISPLAY;
      case 'X':  case 'D':  case 'J':
	if (!g_sel_rereading) {
	    if (g_sel_mode == SM_ARTICLE) {
		ARTICLE* ap;
		ARTICLE** app;
		ARTICLE** limit;
		limit = g_artptr_list + g_artptr_list_size;
		if (ch == 'D')
		    app = g_sel_page_app;
		else
		    app = g_artptr_list;
		while (app < limit) {
		    ap = *app;
		    if ((!(ap->flags & AF_SEL) ^ (ch == 'J'))
		     || (ap->flags & AF_DEL))
			if (ch == 'J' || !g_sel_exclusive
			 || (ap->flags & AF_INCLUDED)) {
			    set_read(ap);
			}
		    app++;
		    if (ch == 'D' && app == g_sel_next_app)
			break;
		}
	    } else {
		SUBJECT* sp;
		if (ch == 'D')
		    sp = g_sel_page_sp;
		else
		    sp = g_first_subject;
		while (sp) {
		    if (((!(sp->flags & SF_SEL) ^ (ch == 'J')) && sp->misc)
		     || (sp->flags & SF_DEL)) {
			if (ch == 'J' || !g_sel_exclusive
			 || (sp->flags & SF_INCLUDED)) {
			    kill_subject(sp, ch=='J'? AFFECT_ALL:AFFECT_UNSEL);
			}
		    }
		    sp = sp->next;
		    if (ch == 'D' && sp == g_sel_next_sp)
			break;
		}
	    }
	    count_subjects(CS_UNSELECT);
	    if (g_obj_count && (ch == 'J' || (ch == 'D' && !g_selected_count))) {
		init_pages(FILL_LAST_PAGE);
		g_sel_item_index = 0;
		return DS_DISPLAY;
	    }
	    if (g_artptr_list && g_obj_count)
		sort_articles();
	} else if (ch == 'J') {
	    SUBJECT* sp;
	    for (sp = g_first_subject; sp; sp = sp->next)
		deselect_subject(sp);
	    g_selected_subj_cnt = g_selected_count = 0;
	    return DS_DISPLAY;
	}
	return DS_QUIT;
      case 'T':
	if (!g_threaded_group) {
	    strcpy(g_msg,"Group is not threaded.");
	    return DS_STATUS;
	}
	/* FALL THROUGH */
      case 'A':
	if (!g_sel_page_item_cnt) {
	    dingaling();
	    break;
	}
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (g_sel_mode == SM_ARTICLE)
	    g_artp = g_sel_items[g_sel_item_index].u.ap;
	else {
	    SUBJECT* sp = g_sel_items[g_sel_item_index].u.sp;
	    if (g_sel_mode == SM_THREAD) {
		while (!sp->misc)
		    sp = sp->thread_link;
	    }
	    g_artp = sp->articles;
	}
	g_art = article_num(g_artp);
	/* This call executes the action too */
	switch (ask_memorize(ch)) {
	  case 'J': case 'j': case 'K':  case ',':
	    count_subjects(g_sel_rereading? CS_NORM : CS_UNSELECT);
	    init_pages(PRESERVE_PAGE);
	    strcpy(g_msg,"Kill memorized.");
	    s_disp_status_line = 1;
	    return DS_DISPLAY;
	  case '+': case '.': case 'S': case 'm':
	    strcpy(g_msg,"Selection memorized.");
	    s_disp_status_line = 1;
	    return DS_UPDATE;
	  case 'c':  case 'C':
	    strcpy(g_msg,"Auto-commands cleared.");
	    s_disp_status_line = 1;
	    return DS_UPDATE;
	  case 'q':
	    return DS_UPDATE;
	  case 'Q':
	    break;
	}
	if (g_can_home)
	    erase_line(false);
	break;
      case ';':
	s_sel_ret = ';';
	return DS_QUIT;
      case ':':
	if (g_sel_page_item_cnt) {
	    if (g_sel_mode == SM_ARTICLE)
		g_artp = g_sel_items[g_sel_item_index].u.ap;
	    else {
		SUBJECT* sp = g_sel_items[g_sel_item_index].u.sp;
		if (g_sel_mode == SM_THREAD) {
		    while (!sp->misc)
			sp = sp->thread_link;
		}
		g_artp = sp->articles;
	    }
	    g_art = article_num(g_artp);
	}
	else
	    g_art = 0;
	/* FALL THROUGH */
      case '/':
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (!finish_command(true)) {	/* get rest of command */
	    if (s_clean_screen)
		break;
	}
	else {
	    if (ch == ':') {
		thread_perform();
		if (!g_sel_rereading) {
		    SUBJECT* sp;
		    for (sp = g_first_subject; sp; sp = sp->next) {
			if (sp->flags & SF_DEL) {
			    sp->flags = SF_NONE;
			    if (g_sel_mode == SM_THREAD)
				kill_thread(sp->thread, AFFECT_UNSEL);
			    else
				kill_subject(sp, AFFECT_UNSEL);
			}
		    }
		}
	    } else {
		/* Force the search to begin at g_absfirst or g_firstart,
		** depending upon whether they specified the 'r' option.
		*/
		g_art = g_lastart+1;
		switch (art_search(g_buf, sizeof g_buf, false)) {
		  case SRCH_ERROR:
		  case SRCH_ABORT:
		    break;
		  case SRCH_INTR:
		    errormsg("Interrupted");
		    break;
		  case SRCH_DONE:
		  case SRCH_SUBJDONE:
		  case SRCH_FOUND:
		    break;
		  case SRCH_NOTFOUND:
		    errormsg("Not found.");
		    break;
		}
	    }
	    g_sel_item_index = 0;
	    /* Recount, in case something has changed. */
	    count_subjects(g_sel_rereading? CS_NORM : CS_UNSELECT);

	    if (sel_perform_change(g_ngptr->toread, "article"))
		return DS_UPDATE;
	    if (s_clean_screen)
		return DS_DISPLAY;
	}
	if (another_command(1))
	    return DS_DOCOMMAND;
	return DS_DISPLAY;
      case 'c':
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
        ch = ask_catchup();
        if (ch == 'y' || ch == 'u')
        {
	    count_subjects(CS_UNSELECT);
	    if (ch != 'u' && g_obj_count) {
		g_sel_page_sp = nullptr;
		g_sel_page_app = nullptr;
		init_pages(FILL_LAST_PAGE);
		return DS_DISPLAY;
	    }
	    s_sel_ret = 'Z';
	    return DS_QUIT;
	}
	if (ch != 'N')
	    return DS_DISPLAY;
	if (g_can_home)
	    erase_line(false);
	break;
      case 'h':
      case '?':
	univ_help(UHELP_ARTSEL);
	return DS_RESTART;
      case 'H':
	newline();
	if (another_command(help_artsel()))
	    return DS_DOCOMMAND;
        return DS_DISPLAY;
      default:
	return DS_ERROR;
    }
    return DS_ASK;
}

static display_state newsgroup_commands(char_int ch)
{
    switch (ch) {
      case Ctl('n'):
      case Ctl('p'):
	return DS_QUIT;
      case 'U':
	sel_cleanup();
	g_sel_rereading = !g_sel_rereading;
	g_sel_page_np = nullptr;
	return DS_RESTART;
      case 'L':
	switch_dmode(&g_sel_grp_dmode);	    /* sets g_msg */
	if (*g_sel_grp_dmode != 's' && !g_multirc->first->datasrc->desc_sf.hp) {
	    newline();
	    return DS_RESTART;
	}
	return DS_DISPLAY;
      case 'X':  case 'D':  case 'J':
	if (!g_sel_rereading) {
	    NGDATA* np;
	    if (ch == 'D')
		np = g_sel_page_np;
	    else
		np = g_first_ng;
	    while (np) {
		if (((!(np->flags&NF_SEL) ^ (ch=='J')) && np->toread!=TR_UNSUB)
		 || (np->flags & NF_DEL)) {
		    if (ch == 'J' || (np->flags & NF_INCLUDED))
			catch_up(np, 0, 0);
		    np->flags &= ~(NF_DEL|NF_SEL);
		}
		np = np->next;
		if (ch == 'D' && np == g_sel_next_np)
		    break;
	    }
	    if (ch == 'J' || (ch == 'D' && !g_selected_count)) {
		init_pages(FILL_LAST_PAGE);
		if (g_sel_total_obj_cnt) {
		    g_sel_item_index = 0;
		    return DS_DISPLAY;
		}
	    }
	} else if (ch == 'J') {
	    NGDATA* np;
	    for (np = g_first_ng; np; np = np->next)
		np->flags &= ~NF_DELSEL;
	    g_selected_count = 0;
	    return DS_DISPLAY;
	}
	return DS_QUIT;
      case '=': {
	NGDATA* np;
	sel_cleanup();
	g_missing_count = 0;
	for (np = g_first_ng; np; np = np->next) {
	    if (np->toread > TR_UNSUB && np->toread < g_ng_min_toread)
		g_newsgroup_toread++;
	    np->abs1st = 0;
	}
	erase_line(false);
	check_active_refetch(true);
	return DS_RESTART;
      }
      case 'O':
	if (!g_sel_rereading)
	    sel_cleanup();
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
      reask_sort:
	in_char("Order by Newsrc, Group name, or Count?", 'q', "ngcNGC");
	printcmd();
	switch (*g_buf) {
	  case 'n': case 'N':
	    break;
	  case 'g': case 'G':
	    *g_buf += 's' - 'g';		/* Group name == SS_STRING */
	    break;
	  case 'c': case 'C':
	    break;
	  case 'h': case 'H':
	    if (g_verbose) {
		fputs("\n\
Type n or SP to order the newsgroups in the .newsrc order.\n\
Type g to order the items by group name.\n\
Type c to order the items by count.\n\
",stdout) FLUSH;
		fputs("\
Typing your selection in upper case it will reverse the selected order.\n\
Type q to leave things as they are.\n\n\
",stdout) FLUSH;
	    }
	    else
	    {
		fputs("\n\
n or SP sorts by .newsrc.\n\
g sorts by group name.\n\
c sorts by count.\n\
",stdout) FLUSH;
		fputs("\
Upper case reverses the sort.\n\
q does nothing.\n\n\
",stdout) FLUSH;
	    }
	    s_clean_screen = false;
	    goto reask_sort;
	  default:
	    if (g_can_home)
		erase_line(false);
	    return DS_ASK;
	}
	set_sel_sort(g_sel_mode,*g_buf);
	g_sel_page_np = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'R':
	if (!g_sel_rereading)
	    sel_cleanup();
	set_selector(g_sel_mode, static_cast<sel_sort_mode>(g_sel_sort * -g_sel_direction));
	g_sel_page_np = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'E':
	if (!g_sel_rereading)
	    sel_cleanup();
	g_sel_exclusive = !g_sel_exclusive;
	g_sel_page_np = nullptr;
	init_pages(FILL_LAST_PAGE);
	g_sel_item_index = 0;
	return DS_DISPLAY;
      case ':':
#if 0
	if (g_ngptr != g_current_ng) {
	    g_recent_ng = g_current_ng;
	    g_current_ng = g_ngptr;
	}
	/* FALL THROUGH */
#endif
      case '/':
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (!finish_command(true)) {	/* get rest of command */
	    if (s_clean_screen)
		break;
	}
	else {
	    if (ch == ':') {
		ngsel_perform();
	    } else {
		g_ngptr = nullptr;
		switch (ng_search(g_buf,false)) {
		  case NGS_ERROR:
		  case NGS_ABORT:
		    break;
		  case NGS_INTR:
		    errormsg("Interrupted");
		    break;
		  case NGS_FOUND:
		  case NGS_NOTFOUND:
		  case NGS_DONE:
		    break;
		}
		g_ngptr = g_current_ng;
	    }
	    g_sel_item_index = 0;

	    if (sel_perform_change(g_newsgroup_toread, "newsgroup"))
		return DS_UPDATE;
	    if (s_clean_screen)
		return DS_DISPLAY;
	}
	if (another_command(1))
	    return DS_DOCOMMAND;
	return DS_DISPLAY;
      case 'c':
	if (g_sel_item_index < g_sel_page_item_cnt)
	    set_ng(g_sel_items[g_sel_item_index].u.np);
	else {
	    strcpy(g_msg, "No newsgroup to catchup.");
	    s_disp_status_line = 1;
	    return DS_UPDATE;
	}
	if (g_ngptr != g_current_ng) {
	    g_recent_ng = g_current_ng;
	    g_current_ng = g_ngptr;
	}
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
        ch = ask_catchup();
        if (ch == 'y' || ch == 'u')
	    return DS_DISPLAY;
	if (ch != 'N')
	    return DS_DISPLAY;
	if (g_can_home)
	    erase_line(false);
	break;
      case 'h':
      case '?':
	univ_help(UHELP_NGSEL);
	return DS_RESTART;
      case 'H':
	newline();
	if (another_command(help_ngsel()))
	    return DS_DOCOMMAND;
        return DS_DISPLAY;
      case ';':
	s_sel_ret = ';';
	return DS_QUIT;
      default: {
	SEL_UNION u;
	input_newsgroup_result ret;
	bool was_at_top = !g_sel_prior_obj_cnt;
	PUSH_SELECTOR();
	if (!(s_removed_prompt & 2)) {
	    erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	    s_removed_prompt = 3;
	    printf("[%s] Cmd: ", g_ngptr? g_ngptr->rcline : "*End*");
	    fflush(stdout);
	}
	g_dfltcmd = "\\";
	set_mode('r','n');
	if (ch == '\\') {
	    putchar(ch);
	    fflush(stdout);
	}
	else
	    pushchar(ch | 0200);
	do {
	    ret = input_newsgroup();
	} while (ret == ING_INPUT);
	set_mode('s','w');
	POP_SELECTOR();
	switch (ret) {
	  case ING_NOSERVER:
	    if (g_multirc) {
		if (!was_at_top)
		    (void) first_page();
		return DS_RESTART;
	    }
	    /* FALL THROUGH */
	  case ING_QUIT:
	    s_sel_ret = 'q';
	    return DS_QUIT;
	  case ING_ERROR:
	    return DS_ERROR;
	  case ING_ERASE:
	    if (s_clean_screen) {
		erase_line(false);
		return DS_ASK;
	    }
	    break;
	  default:
	    if (!save_sel_mode)
		return DS_RESTART;
	    if (g_term_line == g_sel_last_line)
		newline();
	    if (g_term_line != g_sel_last_line+1 || g_term_scrolled)
		s_clean_screen = false;
	    break;
	}
	g_sel_item_index = 0;
	init_pages(PRESERVE_PAGE);
	if (ret == ING_SPECIAL && g_ngptr && g_ngptr->toread < g_ng_min_toread){
	    g_ngptr->flags |= NF_INCLUDED;
	    g_sel_total_obj_cnt++;
	    ret = ING_DISPLAY;
	}
	u.np = g_ngptr;
	if ((calc_page(u) || ret == ING_DISPLAY) && s_clean_screen)
	    return DS_DISPLAY;
	if (ret == ING_MESSAGE) {
	    s_clean_screen = false;
	    return DS_STATUS;
	}
	if (was_at_top)
	    (void) first_page();
	if (s_clean_screen)
	    return DS_ASK;
	newline();
	if (another_command(1))
	    return DS_DOCOMMAND;
	return DS_DISPLAY;
      }
    }
    return DS_ASK;
}

static display_state addgroup_commands(char_int ch)
{
    switch (ch) {
      case 'O':
	if (!g_sel_rereading)
	    sel_cleanup();
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
      reask_sort:
	in_char("Order by Natural-order, Group name, or Count?", 'q', "ngcNGC");
	printcmd();
	switch (*g_buf) {
	  case 'n': case 'N':
	    break;
	  case 'g': case 'G':
	    *g_buf += 's' - 'g';		/* Group name == SS_STRING */
	    break;
	  case 'c': case 'C':
	    break;
	  case 'h': case 'H':
	    if (g_verbose) {
		fputs("\n\
Type n or SP to order the items in their naturally occurring order.\n\
Type g to order the items by newsgroup name.\n\
Type c to order the items by article count.\n\
",stdout) FLUSH;
		fputs("\
Typing your selection in upper case it will reverse the selected order.\n\
Type q to leave things as they are.\n\n\
",stdout) FLUSH;
	    }
	    else
	    {
		fputs("\n\
n or SP sorts by natural order.\n\
g sorts by newsgroup name.\n\
c sorts by article count.\n\
",stdout) FLUSH;
		fputs("\
Upper case reverses the sort.\n\
q does nothing.\n\n\
",stdout) FLUSH;
	    }
	    s_clean_screen = false;
	    goto reask_sort;
	  default:
	    if (g_can_home)
		erase_line(false);
	    return DS_ASK;
	}
	set_sel_sort(g_sel_mode,*g_buf);
	g_sel_page_np = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'U':
	sel_cleanup();
	g_sel_rereading = !g_sel_rereading;
	g_sel_page_gp = nullptr;
	return DS_RESTART;
      case 'R':
	if (!g_sel_rereading)
	    sel_cleanup();
	set_selector(g_sel_mode, static_cast<sel_sort_mode>(g_sel_sort * -g_sel_direction));
	g_sel_page_gp = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'E':
	if (!g_sel_rereading)
	    sel_cleanup();
	g_sel_exclusive = !g_sel_exclusive;
	g_sel_page_gp = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'L':
	switch_dmode(&g_sel_grp_dmode);	    /* sets g_msg */
	if (*g_sel_grp_dmode != 's' && !g_datasrc->desc_sf.hp) {
	    newline();
	    return DS_RESTART;
	}
	return DS_DISPLAY;
      case 'X':  case 'D':  case 'J':
	if (!g_sel_rereading) {
	    ADDGROUP* gp;
	    if (ch == 'D')
		gp = g_sel_page_gp;
	    else
		gp = g_first_addgroup;
	    while (gp) {
		if ((!(gp->flags&AGF_SEL) ^ (ch=='J' ? AGF_SEL : AGF_NONE))
		 || (gp->flags & AGF_DEL)) {
		    if (ch == 'J' || (gp->flags & AGF_INCLUDED))
			gp->flags |= AGF_EXCLUDED;
		    gp->flags &= ~(AGF_DEL|AGF_SEL);
		}
		gp = gp->next;
		if (ch == 'D' && gp == g_sel_next_gp)
		    break;
	    }
	    if (ch == 'J' || (ch == 'D' && !g_selected_count)) {
		init_pages(FILL_LAST_PAGE);
		if (g_sel_total_obj_cnt) {
		    g_sel_item_index = 0;
		    return DS_DISPLAY;
		}
	    }
	} else if (ch == 'J') {
	    ADDGROUP* gp;
	    for (gp = g_first_addgroup; gp; gp = gp->next)
		gp->flags &= ~AGF_DELSEL;
	    g_selected_count = 0;
	    return DS_DISPLAY;
	}
	return DS_QUIT;
      case ':':
      case '/':
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (!finish_command(true)) {	/* get rest of command */
	    if (s_clean_screen)
		break;
	}
	else {
	    if (ch == ':') {
		addgrp_sel_perform();
	    } else {
		switch (ng_search(g_buf,false)) {
		  case NGS_ERROR:
		  case NGS_ABORT:
		    break;
		  case NGS_INTR:
		    errormsg("Interrupted");
		    break;
		  case NGS_FOUND:
		  case NGS_NOTFOUND:
		  case NGS_DONE:
		    break;
		}
	    }
	    g_sel_item_index = 0;

	    if (sel_perform_change(g_newsgroup_toread, "newsgroup"))
		return DS_UPDATE;
	    if (s_clean_screen)
		return DS_DISPLAY;
	}
	if (another_command(1))
	    return DS_DOCOMMAND;
	return DS_DISPLAY;
      case 'h':
      case '?':
	univ_help(UHELP_ADDSEL);
	return DS_RESTART;
      case 'H':
	newline();
	if (another_command(help_addsel()))
	    return DS_DOCOMMAND;
        return DS_DISPLAY;
      default:
	return DS_ERROR;
    }
    return DS_ASK;
}

static display_state multirc_commands(char_int ch)
{
    switch (ch) {
      case 'R':
	set_selector(g_sel_mode, static_cast<sel_sort_mode>(g_sel_sort * -g_sel_direction));
	g_sel_page_mp = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'E':
	if (!g_sel_rereading)
	    sel_cleanup();
	g_sel_exclusive = !g_sel_exclusive;
	g_sel_page_mp = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case '/':
	/*$$$$*/
	break;
      case 'h':
      case '?':
	univ_help(UHELP_MULTIRC);
	return DS_RESTART;
      case 'H':
	newline();
	if (another_command(help_multirc()))
	    return DS_DOCOMMAND;
        return DS_DISPLAY;
      default:
	return DS_ERROR;
    }
    return DS_ASK;
}

static display_state option_commands(char_int ch)
{
    switch (ch) {
      case 'R':
	set_selector(g_sel_mode, static_cast<sel_sort_mode>(g_sel_sort * -g_sel_direction));
	g_sel_page_op = 1;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'E':
	if (!g_sel_rereading)
	    sel_cleanup();
	g_sel_exclusive = !g_sel_exclusive;
	g_sel_page_op = 1;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'S':
	return DS_QUIT;
      case '/': {
	SEL_UNION u;
	char* s;
	char* pattern;
	int i, j;
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
	if (!finish_command(true))	/* get rest of command */
	    break;
	s = cpytill(g_buf,g_buf+1,'/');
        for (pattern = g_buf; *pattern == ' '; pattern++)
            ;
        s = compile(&g_optcompex, pattern, true, true);
        if (s != nullptr)
        {
	    strcpy(g_msg,s);
	    return DS_STATUS;
	}
	i = j = g_sel_items[g_sel_item_index].u.op;
	do {
	    if (++i > g_obj_count)
		i = 1;
	    if (*g_options_ini[i].item == '*')
		continue;
	    if (execute(&g_optcompex,g_options_ini[i].item))
		break;
	} while (i != j);
	u.op = i;
	if (!(g_option_flags[i] & OF_INCLUDED)) {
	    for (j = i-1; *g_options_ini[j].item != '*'; j--) ;
	    g_option_flags[j] |= OF_SEL;
	    init_pages(FILL_LAST_PAGE);
	    calc_page(u);
	    return DS_DISPLAY;
	}
	if (calc_page(u))
	    return DS_DISPLAY;
	return DS_ASK;
      }
      case 'h':
      case '?':
	univ_help(UHELP_OPTIONS);
	return DS_RESTART;
      case 'H':
	newline();
	if (another_command(help_options()))
	    return DS_DOCOMMAND;
        return DS_DISPLAY;
      default:
	return DS_ERROR;
    }
    return DS_ASK;
}

static display_state universal_commands(char_int ch)
{
    switch (ch) {
      case 'R':
	set_selector(g_sel_mode, static_cast<sel_sort_mode>(g_sel_sort * -g_sel_direction));
	sel_page_univ = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case 'E':
	if (!g_sel_rereading)
	    sel_cleanup();
	g_sel_exclusive = !g_sel_exclusive;
	sel_page_univ = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case ';':
	s_sel_ret = ';';
	return DS_QUIT;
      case 'U':
	sel_cleanup();
	g_sel_rereading = !g_sel_rereading;
	sel_page_univ = nullptr;
	return DS_RESTART;
      case Ctl('e'):
	univ_edit();
	univ_redofile();
	sel_cleanup();
	sel_page_univ = nullptr;
	return DS_RESTART;
      case 'h':
      case '?':
	univ_help(UHELP_UNIV);
	return DS_RESTART;
      case 'H':
	newline();
	if (another_command(help_univ()))
	    return DS_DOCOMMAND;
        return DS_DISPLAY;
      case 'O':
	if (!g_sel_rereading)
	    sel_cleanup();
	erase_line(g_mousebar_cnt > 0);	/* erase the prompt */
	s_removed_prompt = 3;
      reask_sort:
	in_char("Order by Natural, or score Points?", 'q', "npNP");
	printcmd();
	if (*g_buf == 'h' || *g_buf == 'H') {
	    if (g_verbose) {
		fputs("\n\
Type n or SP to order the items in the natural order.\n\
Type p to order the items by score points.\n\
",stdout) FLUSH;
		fputs("\
Typing your selection in upper case it will reverse the selected order.\n\
Type q to leave things as they are.\n\n\
",stdout) FLUSH;
	    }
	    else
	    {
		fputs("\n\
n or SP sorts by natural order.\n\
p sorts by score.\n\
",stdout) FLUSH;
		fputs("\
Upper case reverses the sort.\n\
q does nothing.\n\n\
",stdout) FLUSH;
	    }
	    s_clean_screen = false;
	    goto reask_sort;
	} else if (*g_buf == 'q' ||
		   (tolower(*g_buf) != 'n' && tolower(*g_buf) != 'p')) {
	    if (g_can_home)
		erase_line(false);
	    return DS_ASK;
	}
	set_sel_sort(g_sel_mode,*g_buf);
	sel_page_univ = nullptr;
	init_pages(FILL_LAST_PAGE);
	return DS_DISPLAY;
      case '~':
	univ_virt_pass();
	sel_cleanup();
	sel_page_univ = nullptr;
	return DS_RESTART;
      default:
	break;
    }
    return DS_ERROR;
}

static void switch_dmode(char **dmode_cpp)
{
    const char* s = "?";

    if (!*++*dmode_cpp) {
	do {
	    --*dmode_cpp;
	} while ((*dmode_cpp)[-1] != '*');
    }
    switch (**dmode_cpp) {
      case 's':
	s = "short";
	break;
      case 'm':
	s = "medium";
	break;
      case 'd':
	s = "date";
	break;
      case 'l':
	s = "long";
	break;
    }
    sprintf(g_msg,"(%s display style)",s);
    s_disp_status_line = 1;
}

static int find_line(int y)
{
    int i;
    for (i = 0; i < g_sel_page_item_cnt; i++) {
	if (g_sel_items[i].line > y)
	    break;
    }
    if (i > 0)
	i--;
    return i;
}

/* On click:
 *    btn = 0 (left), 1 (middle), or 2 (right) + 4 if double-clicked;
 *    x = 0 to g_tc_COLS-1; y = 0 to g_tc_LINES-1;
 *    btn_clk = 0, 1, or 2 (no 4); x_clk = x; y_clk = y.
 * On release:
 *    btn = 3; x = release's x; y = release's y;
 *    btn_clk = click's 0, 1, or 2; x_clk = click's x; y_clk = click's y.
 */
void selector_mouse(int btn, int x, int y, int btn_clk, int x_clk, int y_clk)
{
    if (check_mousebar(btn, x,y, btn_clk, x_clk,y_clk))
	return;

    if (btn != 3) {
	/* Handle button-down event */
	switch (btn_clk) {
	  case 0:
	  case 1:
	    if (y > 0 && y < g_sel_last_line) {
		if (btn & 4) {
		    pushchar(btn_clk == 0? '\n' : 'Z');
		    g_mouse_is_down = false;
		}
		else {
		    s_force_sel_pos = find_line(y);
		    if (g_use_sel_num) {
			pushchar(('0' + (s_force_sel_pos+1) % 10) | 0200);
			pushchar(('0' + (s_force_sel_pos+1)/10) | 0200);
		    } else {
			pushchar(g_sel_chars[s_force_sel_pos] | 0200);
		    }
		    if (btn == 1)
			pushchar(Ctl('g') | 0200);
		}
	    }
	    break;
	  case 2:
	    break;
	}
    }
    else {
	/* Handle the button-up event */
	switch (btn_clk) {
	  case 0:
	    if (!y)
		pushchar('<' | 0200);
	    else if (y >= g_sel_last_line)
		pushchar(' ');
	    else {
		int i = find_line(y);
		if (g_sel_items[i].line != g_term_line) {
		    if (g_use_sel_num) {
			pushchar(('0' + (i+1) % 10) | 0200);
			pushchar(('0' + (i+1) / 10) | 0200);
		    } else {
			pushchar(g_sel_chars[i] | 0200);
		    }
		    pushchar('-' | 0200);
		    s_force_sel_pos = i;
		}
	    }
	    break;
	  case 1:
	    if (!y)
		pushchar('<' | 0200);
	    else if (y >= g_sel_last_line)
		pushchar('>' | 0200);
	    break;
	  case 2:
	    /* move forward or backwards a page:
	     *   if cursor in top half: backwards
	     *   if cursor in bottom half: forwards
	     */
	    if (y < g_tc_LINES/2)
		pushchar('<' | 0200);
	    else
		pushchar('>' | 0200);
	    break;
	}
    }
}

/* Icky placement, but the PUSH/POP stuff is local to this file */
int univ_visit_group(const char *gname)
{
    PUSH_SELECTOR();

    univ_visit_group_main(gname);

    POP_SELECTOR();
    return 0;		/* later may have some error return values */
}

/* later consider returning universal_selector() value */
void univ_visit_help(help_location where)
{
    PUSH_SELECTOR();
    PUSH_UNIV_SELECTOR();

    univ_help_main(where);
    (void)universal_selector();

    POP_UNIV_SELECTOR();
    POP_SELECTOR();
}
