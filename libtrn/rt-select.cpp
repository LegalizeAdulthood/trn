/* rt-select.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/rt-select.h"

#include "trn/ngdata.h"
#include "trn/addng.h"
#include "trn/artsrch.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "trn/enum-flags.h"
#include "trn/final.h"
#include "trn/intrp.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/ngsrch.h"
#include "trn/ngstuff.h"
#include "trn/nntp.h"
#include "trn/only.h"
#include "trn/opt.h"
#include "trn/rcln.h"
#include "trn/rcstuff.h"
#include "trn/rt-page.h"
#include "trn/rt-util.h"
#include "trn/rthread.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/univ.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

enum DisplayState
{
    DS_ASK = 1,
    DS_UPDATE,
    DS_DISPLAY,
    DS_RESTART,
    DS_STATUS,
    DS_QUIT,
    DS_DO_COMMAND,
    DS_ERROR
};

enum UniversalReadResult
{
    UR_NORM = 1,
    UR_BREAK = 2, /* request return to selector */
    UR_ERROR = 3  /* non-normal return */
};

bool              g_sel_rereading{};
SelectionMode     g_sel_mode{};
SelectionMode     g_sel_default_mode{SM_THREAD};
SelectionMode     g_sel_thread_mode{SM_THREAD};
const char       *g_sel_mode_string{};
SelectionSortMode g_sel_sort{};
SelectionSortMode g_sel_art_sort{SS_GROUPS};
SelectionSortMode g_sel_thread_sort{SS_DATE};
SelectionSortMode g_sel_newsgroup_sort{SS_NATURAL};
const char       *g_sel_sort_string{};
int               g_sel_direction{1};
bool              g_sel_exclusive{};
AddGroupFlags     g_sel_mask{AGF_SEL};
bool              g_selected_only{};
ArticleUnread     g_selected_count{};
int               g_selected_subj_cnt{};
int               g_added_articles{};
char             *g_sel_chars{};
int               g_sel_item_index{};
int               g_sel_last_line{};
bool              g_sel_at_end{};
int               g_keep_the_group_static{}; /* -K */
char              g_newsrc_sel_cmds[3]{"Z>"};
char              g_add_sel_cmds[3]{"Z>"};
char              g_newsgroup_sel_cmds[3]{"Z>"};
char              g_news_sel_cmds[3]{"Z>"};
char              g_option_sel_cmds[3]{"Z>"};
bool              g_use_sel_num{};
bool              g_sel_num_goto{};

enum RemovedPrompt
{
    RP_NONE = 0,
    RP_MOUSE_BAR = 1,
    RP_NEWLINE = 2,
    RP_ALL = 3,
};
DECLARE_FLAGS_ENUM(RemovedPrompt, int);

static char          s_sel_ret{};
static char          s_page_char{};
static char          s_end_char{};
static int           s_disp_status_line{};
static bool          s_clean_screen{};
static RemovedPrompt s_removed_prompt{};
static int           s_force_sel_pos{};
static DisplayState (*s_extra_commands)(char_int){};

namespace
{

class PushSelectorModes
{
public:
    PushSelectorModes(MinorMode new_mode) :
        m_save_mode(g_mode),
        m_save_gmode(g_general_mode)
    {
        g_bos_on_stop = true;
        set_mode(GM_SELECTOR, new_mode);
    }
    ~PushSelectorModes()
    {
        g_bos_on_stop = false;
        set_mode(m_save_gmode, m_save_mode);
    }

private:
    MinorMode   m_save_mode;
    GeneralMode m_save_gmode;
};

}

#define PUSH_SELECTOR()                                   \
    SelectionMode save_sel_mode = g_sel_mode;             \
    const bool    save_sel_rereading = g_sel_rereading;   \
    const bool    save_sel_exclusive = g_sel_exclusive;   \
    ArticleUnread save_selected_count = g_selected_count; \
    DisplayState (*const save_extra_commands)(char_int) = s_extra_commands

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

#define PUSH_UNIV_SELECTOR()                                   \
    UniversalItem *const save_first_univ = g_first_univ;       \
    UniversalItem *const save_last_univ = g_last_univ;         \
    UniversalItem *const save_page_univ = sel_page_univ;       \
    UniversalItem *const save_next_univ = g_sel_next_univ;     \
    char *const          save_univ_fname = g_univ_fname;       \
    std::string const    save_univ_label = g_univ_label;       \
    std::string const    save_univ_title = g_univ_title;       \
    std::string const    save_univ_tmp_file = g_univ_tmp_file; \
    const char           save_sel_ret = s_sel_ret;             \
    HashTable *const     save_univ_ng_hash = g_univ_ng_hash;   \
    HashTable *const     save_univ_vg_hash = g_univ_vg_hash

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

static void                sel_do_groups();
static UniversalReadResult univ_read(UniversalItem *ui);
static void                sel_display();
static void                sel_status_msg(const char *cp);
static char                sel_input();
static void                sel_prompt();
static bool                select_item(Selection u);
static bool                delay_return_item(Selection u);
static bool                deselect_item(Selection u);
static bool                select_option(OptionIndex i);
static void                sel_cleanup();
static bool                mark_del_as_read(char *ptr, int arg);
static DisplayState        sel_command(char_int ch);
static bool                sel_perform_change(long cnt, const char *obj_type);
static char                another_command(char_int ch);
static DisplayState        article_commands(char_int ch);
static DisplayState        newsgroup_commands(char_int ch);
static DisplayState        add_group_commands(char_int ch);
static DisplayState        multirc_commands(char_int ch);
static DisplayState        option_commands(char_int ch);
static DisplayState        universal_commands(char_int ch);
static void                switch_dmode(char **dmode_cpp);
static int                 find_line(int y);

/* Display a menu of threads/subjects/articles for the user to choose from.
** If "cmd" is '+' we display all the unread items and allow the user to mark
** them as selected and perform various commands upon them.  If "cmd" is 'U'
** the list consists of previously-read items for the user to mark as unread.
*/
char article_selector(char_int cmd)
{
    bool save_selected_only;
    PushSelectorModes saver(MM_THREAD_SELECTOR);

    g_sel_rereading = (cmd == 'U');

    g_art = g_last_art+1;
    s_extra_commands = article_commands;
    g_keep_the_group_static = (g_keep_the_group_static == 1);

    g_sel_mode = SM_ARTICLE;
    set_sel_mode(cmd);

    if (!cache_range(g_sel_rereading ? g_abs_first : g_first_art, g_last_art))
    {
        s_sel_ret = '+';
        goto sel_exit;
    }

sel_restart:
    /* Setup for selecting articles to read or set unread */
    if (g_sel_rereading)
    {
        s_end_char = 'Z';
        s_page_char = '>';
        g_sel_page_app = nullptr;
        g_sel_page_sp = nullptr;
        g_sel_mask = AGF_DEL_SEL;
    }
    else
    {
        s_end_char = g_news_sel_cmds[0];
        s_page_char = g_news_sel_cmds[1];
        if (g_curr_artp)
        {
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
    if (g_added_articles)
    {
        long i = g_added_articles;
        for (long j = g_last_art - i + 1; j <= g_last_art; j++)
        {
            if (!article_unread(j))
            {
                i--;
            }
        }
        if (i == g_added_articles)
        {
            std::sprintf(g_msg, "** %ld new article%s arrived **  ",
               (long)g_added_articles, plural(g_added_articles));
        }
        else
        {
            std::sprintf(g_msg, "** %ld of %ld new articles unread **  ",
               i, (long)g_added_articles);
        }
        s_disp_status_line = 1;
    }
    g_added_articles = 0;
    if (cmd && g_selected_count)
    {
        std::sprintf(g_msg+std::strlen(g_msg), "%ld article%s selected.",
                (long)g_selected_count, g_selected_count == 1? " is" : "s are");
        s_disp_status_line = 1;
    }
    cmd = 0;

    sel_display();
    if (sel_input() == 'R')
    {
        goto sel_restart;
    }

    sel_cleanup();
    newline();
    if (g_mouse_bar_cnt)
    {
        clear_rest();
    }

sel_exit:
    if (s_sel_ret == '\033')
    {
        s_sel_ret = '+';
    }
    else if (s_sel_ret == '`')
    {
        s_sel_ret = 'Q';
    }
    if (g_sel_rereading)
    {
        g_sel_rereading = false;
        g_sel_mask = AGF_SEL;
    }
    if (g_sel_mode != SM_ARTICLE || g_sel_sort == SS_GROUPS || g_sel_sort == SS_STRING)
    {
        if (g_art_ptr_list)
        {
            std::free((char*)g_art_ptr_list);
            g_art_ptr_list = nullptr;
            g_sel_page_app = nullptr;
            sort_subjects();
        }
        g_art_ptr = nullptr;
        if (!g_threaded_group)
        {
            g_search_ahead = -1;
        }
    }
    else
    {
        g_search_ahead = 0;
    }
    g_selected_only = (g_selected_count != 0);
    if (s_sel_ret == '+')
    {
        g_selected_only = save_selected_only;
        count_subjects(CS_RESELECT);
    }
    else
    {
        count_subjects(CS_UNSELECT);
    }
    if (s_sel_ret == '+')
    {
        g_art = g_curr_art;
        g_artp = g_curr_artp;
    }
    else
    {
        top_article();
    }
    return s_sel_ret;
}

static void sel_do_groups()
{
    int ret;
    int save_selected_count = g_selected_count;

    for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
    {
        if (!(np->flags & NF_VISIT))
        {
            continue;
        }
do_group:
        if (np->flags & NF_SEL)
        {
            np->flags &= ~NF_SEL;
            save_selected_count--;
        }
        set_newsgroup(np);
        if (np != g_current_newsgroup)
        {
            g_recent_newsgroup = g_current_newsgroup;
            g_current_newsgroup = np;
        }
        g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
        std::printf("Entering %s:", g_newsgroup_name.c_str());
        if (s_sel_ret == ';')
        {
            ret = do_newsgroup(save_str(";"));
        }
        else
        {
            ret = do_newsgroup("");
        }
        switch (ret)
        {
        case NG_NORM:
        case NG_SEL_NEXT:
            set_newsgroup(np->next);
            break;

        case NG_NEXT:
            set_newsgroup(np->next);
            goto loop_break;

        case NG_ERROR:
        case NG_ASK:
            goto loop_break;

        case NG_SEL_PRIOR:
            while ((np = np->prev) != nullptr)
            {
                if (np->flags & NF_VISIT)
                {
                    goto do_group;
                }
            }
            (void) first_page();
            goto loop_break;

        case NG_MINUS:
            np = g_recent_newsgroup;
            goto do_group;

        case NG_NO_SERVER:
            nntp_server_died(np->rc->data_source);
            (void) first_page();
            break;

        /* extensions */
        case NG_GO_ARTICLE:
            np = g_ng_go_newsgroup_ptr;
            goto do_group;
            /* later: possible go-to-newsgroup (applicable?) */
        }
    }
loop_break:
    g_selected_count = save_selected_count;
}

char multirc_selector()
{
    PushSelectorModes saver(MM_NEWSRC_SELECTOR);

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
    {
        goto sel_restart;
    }

    newline();
    if (g_mouse_bar_cnt)
    {
        clear_rest();
    }

    if (s_sel_ret == '\r' || s_sel_ret == '\n' || s_sel_ret == 'Z' || s_sel_ret == '\t')
    {
        PUSH_SELECTOR();
        for (Multirc *mp = multirc_low(); mp; mp = multirc_next(mp))
        {
            if (mp->flags & MF_SEL)
            {
                mp->flags &= ~MF_SEL;
                save_selected_count--;
                for (Newsrc *rp = mp->first; rp; rp = rp->next)
                {
                    rp->data_source->flags &= ~DF_UNAVAILABLE;
                }
                if (use_multirc(mp))
                {
                    find_new_groups();
                    do_multirc();
                    unuse_multirc(mp);
                }
                else
                {
                    mp->flags &= ~MF_INCLUDED;
                }
            }
        }
        POP_SELECTOR();
        goto sel_restart;
    }
    return s_sel_ret;
}

char newsgroup_selector()
{
    PushSelectorModes saver(MM_NEWSGROUP_SELECTOR);

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;

    set_selector(SM_NEWSGROUP, SS_MAGIC_NUMBER);

sel_restart:
    if (*g_sel_grp_display_mode != 's')
    {
        for (Newsrc *rp = g_multirc->first; rp; rp = rp->next)
        {
            if ((rp->flags & RF_ACTIVE) && !rp->data_source->desc_sf.hp)
            {
                find_group_desc(rp->data_source, "control");
                if (rp->data_source->desc_sf.fp)
                {
                    rp->data_source->flags |= DF_NO_XGTITLE; /* TODO: ok?*/
                }
                else
                {
                    rp->data_source->desc_sf.refetch_secs = 0;
                }
            }
        }
    }

    s_end_char = g_newsgroup_sel_cmds[0];
    s_page_char = g_newsgroup_sel_cmds[1];
    if (g_sel_rereading)
    {
        g_sel_mask = AGF_DEL_SEL;
        g_sel_page_np = nullptr;
    }
    else
    {
        g_sel_mask = AGF_SEL;
    }
    s_extra_commands = newsgroup_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
    {
        goto sel_restart;
    }

    newline();
    if (g_mouse_bar_cnt)
    {
        clear_rest();
    }

    if (s_sel_ret == '\r' || s_sel_ret == '\n' || s_sel_ret == 'Z' || s_sel_ret == '\t' || s_sel_ret == ';')
    {
        PUSH_SELECTOR();
        for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
        {
            if ((np->flags & NF_INCLUDED)
             && (!g_selected_count || (np->flags & g_sel_mask)))
            {
                np->flags |= NF_VISIT;
            }
            else
            {
                np->flags &= ~NF_VISIT;
            }
        }
        sel_do_groups();
        save_selected_count = g_selected_count;
        POP_SELECTOR();
        if (g_multirc)
        {
            goto sel_restart;
        }
        s_sel_ret = 'q';
    }
    sel_cleanup();
    end_only();
    return s_sel_ret;
}

char add_group_selector(GetNewsgroupFlags flags)
{
    PushSelectorModes saver(MM_ADD_GROUP_SELECTOR);

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;

    set_selector(SM_ADD_GROUP, SS_MAGIC_NUMBER);

sel_restart:
    if (*g_sel_grp_display_mode != 's')
    {
        for (Newsrc *rp = g_multirc->first; rp; rp = rp->next)
        {
            if ((rp->flags & RF_ACTIVE) && !rp->data_source->desc_sf.hp)
            {
                find_group_desc(rp->data_source, "control");
                if (!rp->data_source->desc_sf.fp)
                {
                    rp->data_source->desc_sf.refetch_secs = 0;
                }
            }
        }
    }

    s_end_char = g_add_sel_cmds[0];
    s_page_char = g_add_sel_cmds[1];
    /* Setup for selecting articles to read or set unread */
    if (g_sel_rereading)
    {
        g_sel_mask = AGF_DEL_SEL;
    }
    else
    {
        g_sel_mask = AGF_SEL;
    }
    g_sel_page_gp = nullptr;
    s_extra_commands = add_group_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
    {
        goto sel_restart;
    }

    g_selected_count = 0;
    newline();
    if (g_mouse_bar_cnt)
    {
        clear_rest();
    }

    if (s_sel_ret == '\r' || s_sel_ret == '\n' || s_sel_ret == 'Z' || s_sel_ret == '\t')
    {
        AddGroup *gp;
        int i;
        g_add_new_by_default = ADDNEW_SUB;
        for (gp = g_first_add_group, i = 0; gp; gp = gp->next, i++)
        {
            if (gp->flags & AGF_SEL)
            {
                gp->flags &= ~AGF_SEL;
                get_newsgroup(gp->name,flags);
            }
        }
        g_add_new_by_default = ADDNEW_ASK;
    }
    sel_cleanup();
    return s_sel_ret;
}

char option_selector()
{
    char** vals = ini_values(g_options_ini);
    PushSelectorModes saver(MM_OPTION_SELECTOR);

    g_sel_rereading = false;
    g_sel_exclusive = false;
    g_selected_count = 0;
    parse_ini_section("", g_options_ini);

    set_selector(SM_OPTIONS, SS_MAGIC_NUMBER);

sel_restart:
    s_end_char = g_option_sel_cmds[0];
    s_page_char = g_option_sel_cmds[1];
    if (g_sel_rereading)
    {
        g_sel_mask = AGF_DEL_SEL;
    }
    else
    {
        g_sel_mask = AGF_SEL;
    }
    g_sel_page_op = -1;
    s_extra_commands = option_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R' || s_sel_ret=='\r' || s_sel_ret=='\n')
    {
        goto sel_restart;
    }

    g_selected_count = 0;
    newline();
    if (g_mouse_bar_cnt)
    {
        clear_rest();
    }

    if (s_sel_ret == 'Z' || s_sel_ret == '\t' || s_sel_ret == 'S')
    {
        set_options(vals);
        if (s_sel_ret == 'S')
        {
            save_options(g_ini_file.c_str());
        }
    }
    for (int i = 1; g_options_ini[i].checksum; i++)
    {
        if (vals[i])
        {
            if (g_option_saved_vals[i] && !strcmp(vals[i], g_option_saved_vals[i]))
            {
                if (g_option_saved_vals[i] != g_option_def_vals[i])
                {
                    std::free(g_option_saved_vals[i]);
                }
                g_option_saved_vals[i] = nullptr;
            }
            std::free(vals[i]);
            vals[i] = nullptr;
        }
    }
    return s_sel_ret;
}

/* returns a command to do */
static UniversalReadResult univ_read(UniversalItem *ui)
{
    UniversalReadResult exit_code = UR_NORM;
    char ch;

    g_univ_follow_temp = false;
    if (!ui)
    {
        std::printf("nullptr UI passed to reader!\n");
        sleep(5);
        return exit_code;
    }
    std::printf("\n");                 /* prepare for output msgs... */
    switch (ui->type)
    {
    case UN_DEBUG1:
    {
        char *s = ui->data.str;
        if (s && *s)
        {
            std::printf("Not implemented yet (%s)\n",s);
            sleep(5);
            return exit_code;
        }
        break;
    }

    case UN_TEXT_FILE:
    {
        char *s = ui->data.text_file.fname;
        if (s && *s)
        {
            /* later have some way of getting a return code back */
            univ_page_file(s);
        }
        break;
    }

    case UN_ARTICLE:
    {
        if (g_in_ng)
        {
            /* XXX whine: can't recurse at this time */
            break;
        }
        if (!ui->data.virt.ng)
        {
            break;                      /* XXX whine */
        }
        NewsgroupData *np = find_newsgroup(ui->data.virt.ng);

        if (!np)
        {
            std::printf("Universal: newsgroup %s not found!",
                   ui->data.virt.ng);
            sleep(5);
            return exit_code;
        }
        set_newsgroup(np);
        if (np != g_current_newsgroup)
        {
            g_recent_newsgroup = g_current_newsgroup;
            g_current_newsgroup = np;
        }
        g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
        std::printf("Virtual: Entering %s:\n", g_newsgroup_name.c_str());
        g_ng_go_art_num = ui->data.virt.num;
        g_univ_read_virt_flag = true;
        int ret = do_newsgroup("");
        g_univ_read_virt_flag = false;
        switch (ret)
        {
        case NG_NORM:         /* handle more cases later */
        case NG_SEL_NEXT:
        case NG_NEXT:
            /* just continue reading */
            break;

        case NG_SEL_PRIOR:
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

    case UN_GROUP_MASK:
    {
        univ_mask_load(ui->data.gmask.mask_list,ui->data.gmask.title);
        ch = universal_selector();
        switch (ch)
        {
        case 'q':
            exit_code = UR_BREAK;
            break;

        default:
            exit_code = UR_NORM;
            break;
        }
        return exit_code;
    }

    case UN_CONFIG_FILE:
    {
        univ_file_load(ui->data.cfile.fname,ui->data.cfile.title,
                       ui->data.cfile.label);
        ch = universal_selector();
        switch (ch)
        {
        case 'q':
            exit_code = UR_BREAK;
            break;

        default:
            exit_code = UR_NORM;
            break;
        }
        return exit_code;
    }

    case UN_NEWSGROUP:
    {
        int ret;

        if (g_in_ng)
        {
            /* XXX whine: can't recurse at this time */
            break;
        }
        if (!ui->data.group.ng)
        {
            break;                      /* XXX whine */
        }
        NewsgroupData *np = find_newsgroup(ui->data.group.ng);

        if (!np)
        {
            std::printf("Universal: newsgroup %s not found!",
                   ui->data.group.ng);
            sleep(5);
            return exit_code;
        }
do_group:
        set_newsgroup(np);
        if (np != g_current_newsgroup)
        {
            g_recent_newsgroup = g_current_newsgroup;
            g_current_newsgroup = np;
        }
        g_threaded_group = (g_use_threads && !(np->flags & NF_UNTHREADED));
        std::printf("Entering %s:", g_newsgroup_name.c_str());
        if (s_sel_ret == ';')
        {
            ret = do_newsgroup(save_str(";"));
        }
        else
        {
            ret = do_newsgroup("");
        }
        switch (ret)
        {
        case NG_NORM:         /* handle more cases later */
        case NG_SEL_NEXT:
        case NG_NEXT:
            /* just continue reading */
            break;

        case NG_SEL_PRIOR:
            /* not implemented yet */
            /* FALL THROUGH */

        case NG_ERROR:
        case NG_ASK:
            exit_code = UR_BREAK;
            return exit_code;

        case NG_MINUS:
            np = g_recent_newsgroup;
            goto do_group;

        case NG_NO_SERVER:
            /* Eeep! */
            break;
        }
        break;
    }

    case UN_HELP_KEY:
        if (another_command(univ_key_help(ui->data.i)))
        {
            push_char(s_sel_ret | 0200);
        }
        break;

    default:
        break;
    }
    return exit_code;
}

char universal_selector()
{
    PushSelectorModes saver(MM_UNIVERSAL);            /* kind of like 'v'irtual... */

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
    {
        g_sel_mask = AGF_DEL_SEL;
    }
    else
    {
        g_sel_mask = AGF_SEL;
    }
    sel_page_univ = nullptr;
    s_extra_commands = universal_commands;

    init_pages(FILL_LAST_PAGE);
    g_sel_item_index = 0;

    sel_display();
    if (sel_input() == 'R')
    {
        goto sel_restart;
    }

    newline();
    if (g_mouse_bar_cnt)
    {
        clear_rest();
    }

    if (s_sel_ret == '\r' || s_sel_ret == '\n' || s_sel_ret == '\t' || s_sel_ret == ';' || s_sel_ret == 'Z')
    {
        UniversalItem *ui;
        int i;
        for (ui = g_first_univ, i = 0; ui; ui = ui->next, i++)
        {
            if (ui->flags & UF_SEL)
            {
                PUSH_SELECTOR();
                PUSH_UNIV_SELECTOR();

                ui->flags &= ~UF_SEL;
                save_selected_count--;
                UniversalReadResult ret = univ_read(ui);

                POP_UNIV_SELECTOR();
                POP_SELECTOR();
                if (ret == UR_ERROR || ret == UR_BREAK)
                {
                    s_sel_ret = ' ';    /* don't leave completely. */
                    break;              /* jump out of for loop */
                }
            }
        }
    }
/*univ_loop_break:*/
    /* restart the selector unless the user explicitly quits.
     * possibly later have an option for 'Z' to quit levels>1.
     */
    if (s_sel_ret != 'q' && (s_sel_ret != 'Q'))
    {
        goto sel_restart;
    }
    sel_cleanup();
    univ_close();
    return s_sel_ret;
}

static void sel_display()
{
    /* Present a page of items to the user */
    display_page();
    if (g_erase_screen && g_erase_each_line)
    {
        erase_line(true);
    }

    if (g_sel_item_index >= g_sel_page_item_cnt)
    {
        g_sel_item_index = 0;
    }

    if (s_disp_status_line == 1)
    {
        newline();
        std::fputs(g_msg, stdout);
        g_term_col = std::strlen(g_msg);
        s_disp_status_line = 2;
    }
}

static void sel_status_msg(const char *cp)
{
    if (g_can_home)
    {
        goto_xy(0, g_sel_last_line + 1);
    }
    else
    {
        newline();
    }
    std::fputs(cp, stdout);
    g_term_col = std::strlen(cp);
    goto_xy(0,g_sel_items[g_sel_item_index].line);
    std::fflush(stdout);     /* otherwise may not be visible */
    s_disp_status_line = 2;
}

static char sel_input()
{
    int j;
    int sel_number;

    /* TRN proudly continues the state machine traditions of RN.
     * April 2, 1996: 28 gotos in this function.  Conversion to
     * structured programming is left as an exercise for the reader.
     */
    /* If one immediately types a goto command followed by a dash ('-'),
     * the following will be the default action.
     */
    int action = '+';
reask_selector:
    /* Prompt the user */
    sel_prompt();
position_selector:
      int got_dash = 0;
      int got_goto = 0;
    s_force_sel_pos = -1;
    if (s_removed_prompt & RP_MOUSE_BAR)
    {
        draw_mouse_bar(g_tc_COLS,false);
        s_removed_prompt &= ~RP_MOUSE_BAR;
    }
    if (g_can_home)
    {
        goto_xy(0, g_sel_items[g_sel_item_index].line);
    }

reinp_selector:
    if (s_removed_prompt & RP_MOUSE_BAR)
    {
        goto position_selector; /* (TRN considered harmful? :-) */
    }
    /* Grab some commands from the user */
    std::fflush(stdout);
    eat_typeahead();
    if (g_use_sel_num)
    {
        g_spin_char = '0' + (g_sel_item_index + 1) / 10; /* first digit */
    }
    else
    {
        g_spin_char = g_sel_chars[g_sel_item_index];
    }
    cache_until_key();
    get_cmd(g_buf);
    if (*g_buf == ' ')
    {
        set_def(g_buf, g_sel_at_end ? &s_end_char : &s_page_char);
    }
    int ch = *g_buf;
    if (errno)
    {
        ch = Ctl('l');
    }
    if (s_disp_status_line == 2)
    {
        if (g_can_home)
        {
            goto_xy(0,g_sel_last_line+1);
            erase_line(false);
            if (g_term_line == g_tc_LINES-1)
            {
                s_removed_prompt |= RP_MOUSE_BAR;
            }
        }
        s_disp_status_line = 0;
    }
    if (ch == '-' && g_sel_page_item_cnt)
    {
        got_dash = 1;
        got_goto = 0;   /* right action is not clear if both are true */
        if (g_can_home)
        {
            if (!input_pending())
            {
                j = (g_sel_item_index > 0? g_sel_item_index : g_sel_page_item_cnt);
                if (g_use_sel_num)
                {
                    std::sprintf(g_msg, "Range: %d-", j);
                }
                else
                {
                    std::sprintf(g_msg, "Range: %c-", g_sel_chars[j - 1]);
                }
                sel_status_msg(g_msg);
            }
        }
        else
        {
            std::putchar('-');
            std::fflush(stdout);
        }
        goto reinp_selector;
    }
    /* allow the user to back out of a range or a goto with erase char */
    if (ch == g_erase_char || ch == g_kill_char)
    {
        /* later consider dingaling() if neither got_{dash,goto} is true */
        got_dash = 0;
        got_goto = 0;
        /* following if statement should be function */
        if (s_disp_status_line == 2)    /* status was printed */
        {
            if (g_can_home)
            {
                goto_xy(0,g_sel_last_line+1);
                erase_line(false);
                if (g_term_line == g_tc_LINES-1)
                {
                    s_removed_prompt |= RP_MOUSE_BAR;
                }
            }
            s_disp_status_line = 0;
        }
        goto position_selector;
    }
    if (ch == Ctl('g'))
    {
        got_goto = 1;
        got_dash = 0;   /* right action is not clear if both are true */
        if (!input_pending())
        {
            if (g_use_sel_num)
            {
                sel_status_msg("Go to number?");
            }
            else
            {
                sel_status_msg("Go to letter?");
            }
        }
        goto reinp_selector;
    }
    if (g_sel_mode == SM_OPTIONS && (ch == '\r' || ch == '\n'))
    {
        ch = '.';
    }
    char *in_select = std::strchr(g_sel_chars, ch);
    if (g_use_sel_num && ch >= '0' && ch <= '9')
    {
        int ch_num1 = ch;
        /* would be *very* nice to use wait_key_pause() here */
        if (!input_pending())
        {
            if (got_dash)
            {
                if (g_sel_item_index > 0)
                {
                    j = g_sel_item_index;  /* -1, +1 to print */
                }
                else  /* wrap around from the bottom */
                {
                    j = g_sel_page_item_cnt;
                }
                std::sprintf(g_msg,"Range: %d-%c", j, ch);
            }
            else
            {
                if (got_goto)
                {
                    std::sprintf(g_msg,"Go to number: %c", ch);
                }
                else
                {
                    std::sprintf(g_msg,"%c", ch);
                }
            }
            sel_status_msg(g_msg);
        }
        /* Consider cache_until_key() here.  The middle of typing a
         * number is a lousy time to delay, however.
         */
        get_cmd(g_buf);
        ch = *g_buf;
        if (s_disp_status_line == 2)    /* status was printed */
        {
            if (g_can_home)
            {
                goto_xy(0,g_sel_last_line+1);
                erase_line(false);
                if (g_term_line == g_tc_LINES-1)
                {
                    s_removed_prompt |= RP_MOUSE_BAR;
                }
            }
            s_disp_status_line = 0;
        }
        if (ch == g_kill_char)          /* kill whole command in progress */
        {
            got_goto = 0;
            got_dash = 0;
            goto position_selector;
        }
        if (ch == g_erase_char)
        {
            /* Erase any first digit printed, but allow complex
             * commands to continue.  Spaces at end of message are
             * there to wipe out old first digit.
             */
            if (got_dash)
            {
                if (g_sel_item_index > 0)
                {
                    j = g_sel_item_index;  /* -1, +1 to print */
                }
                else /* wrap around from the bottom */
                {
                    j = g_sel_page_item_cnt;
                }
                std::sprintf(g_msg,"Range: %d- ", j);
                sel_status_msg(g_msg);
                goto reinp_selector;
            }
            if (got_goto)
            {
                sel_status_msg("Go to number?  ");
                goto reinp_selector;
            }
            goto position_selector;
        }
        if (ch >= '0' && ch <= '9')
        {
            sel_number = ((ch_num1 - '0') * 10) + (ch - '0');
        }
        else
        {
            push_char(ch);       /* for later use */
            sel_number = (ch_num1 - '0');
        }
        j = sel_number-1;
        if ((j < 0) || (j >= g_sel_page_item_cnt))
        {
            dingaling();
            std::sprintf(g_msg, "No item %c%c on this page.", ch_num1, ch);
            sel_status_msg(g_msg);
            goto position_selector;
        }
        else if (got_goto || (g_sel_num_goto && !got_dash))
        {
            /* (but only do always-goto if there was not a dash) */
            g_sel_item_index = j;
            goto position_selector;
        }
        else if (got_dash)
        {
        }
        else if (g_sel_items[j].sel == 1)
        {
            action = (g_sel_rereading ? 'k' : '-');
        }
        else
        {
            action = '+';
        }
    }
    else if (in_select && !g_use_sel_num)
    {
        j = in_select - g_sel_chars;
        if (j >= g_sel_page_item_cnt)
        {
            dingaling();
            std::sprintf(g_msg, "No item '%c' on this page.", ch);
            sel_status_msg(g_msg);
            goto position_selector;
        }
        else if (got_goto)
        {
            g_sel_item_index = j;
            goto position_selector;
        }
        else if (got_dash)
        {
        }
        else if (g_sel_items[j].sel == 1)
        {
            action = (g_sel_rereading ? 'k' : '-');
        }
        else
        {
            action = '+';
        }
    }
    else if (ch == '*' && g_sel_mode == SM_ARTICLE)
    {
        if (!g_sel_page_item_cnt)
        {
            dingaling();
        }
        else
        {
            Article *ap = g_sel_items[g_sel_item_index].u.ap;
            if (g_sel_items[g_sel_item_index].sel)
            {
                deselect_subject(ap->subj);
            }
            else
            {
                select_subject(ap->subj, AUTO_KILL_NONE);
            }
            update_page();
        }
        goto position_selector;
    }
    else if (ch == 'y' || ch == '.' || ch == '*' || ch == Ctl('t'))
    {
        j = g_sel_item_index;
        if (g_sel_page_item_cnt && g_sel_items[j].sel == 1)
        {
            action = (g_sel_rereading ? 'k' : '-');
        }
        else
        {
            action = '+';
        }
        if (ch == Ctl('t'))
        {
            s_force_sel_pos = j;
        }
    }
    else if (ch == 'k' || ch == 'j' || ch == ',')
    {
        j = g_sel_item_index;
        action = 'k';
    }
    else if (ch == 'm' || ch == '|')
    {
        j = g_sel_item_index;
        action = '-';
    }
    else if (ch == 'M' && g_in_ng)
    {
        j = g_sel_item_index;
        action = 'M';
    }
    else if (ch == '@')
    {
        g_sel_item_index = 0;
        j = g_sel_page_item_cnt-1;
        got_dash = 1;
        action = '@';
    }
    else if (ch == '[' || ch == 'p')
    {
        if (--g_sel_item_index < 0)
        {
            g_sel_item_index = g_sel_page_item_cnt ? g_sel_page_item_cnt - 1 : 0;
        }
        goto position_selector;
    }
    else if (ch == ']' || ch == 'n')
    {
        if (++g_sel_item_index >= g_sel_page_item_cnt)
        {
            g_sel_item_index = 0;
        }
        goto position_selector;
    }
    else
    {
        s_sel_ret = ch;
        switch (sel_command(ch))
        {
        case DS_ASK:
            if (!s_clean_screen)
            {
                sel_display();
                goto reask_selector;
            }
            if (s_removed_prompt & RP_NEWLINE)
            {
                carriage_return();
                goto reask_selector;
            }
            goto position_selector;

        case DS_DISPLAY:
            sel_display();
            goto reask_selector;

        case DS_UPDATE:
            if (!s_clean_screen)
            {
                sel_display();
                goto reask_selector;
            }
            if (s_disp_status_line == 1)
            {
                newline();
                std::fputs(g_msg,stdout);
                g_term_col = std::strlen(g_msg);
                if (s_removed_prompt & RP_MOUSE_BAR)
                {
                    draw_mouse_bar(g_tc_COLS,false);
                    s_removed_prompt &= ~RP_MOUSE_BAR;
                }
                s_disp_status_line = 2;
            }
            update_page();
            if (g_can_home)
            {
                goto_xy(0, g_sel_last_line);
            }
            goto reask_selector;

        case DS_RESTART:
            return 'R'; /*Restart*/

        case DS_QUIT:
            return 'Q'; /*Quit*/

        case DS_STATUS:
            s_disp_status_line = 1;
            if (!s_clean_screen)
            {
                sel_display();
                goto reask_selector;
            }
            sel_status_msg(g_msg);

            if (!g_can_home)
            {
                newline();
            }
            if (s_removed_prompt & RP_NEWLINE)
            {
                goto reask_selector;
            }
            goto position_selector;
        }
    }

    if (!g_sel_page_item_cnt)
    {
        dingaling();
        goto position_selector;
    }

    /* The user manipulated one of the letters -- handle it. */
    if (!got_dash)
    {
        g_sel_item_index = j;
    }
    else
    {
        if (j < g_sel_item_index)
        {
            ch = g_sel_item_index-1;
            g_sel_item_index = j;
            j = ch;
        }
    }

    if (++j == g_sel_page_item_cnt)
    {
        j = 0;
    }
    do
    {
        int sel = g_sel_items[g_sel_item_index].sel;
        if (g_can_home)
        {
            goto_xy(0, g_sel_items[g_sel_item_index].line);
        }
        if (action == '@')
        {
            if (sel == 2)
            {
                ch = (g_sel_rereading ? '+' : ' ');
            }
            else if (g_sel_rereading)
            {
                ch = 'k';
            }
            else if (sel == 1)
            {
                ch = '-';
            }
            else
            {
                ch = '+';
            }
        }
        else
        {
            ch = action;
        }
        switch (ch)
        {
        case '+':
            if (select_item(g_sel_items[g_sel_item_index].u))
            {
                output_sel(g_sel_item_index, 1, true);
            }
            if (g_term_line >= g_sel_last_line)
            {
                sel_display();
                goto reask_selector;
            }
            break;

        case '-':  case 'k':  case 'M':
        {
            bool sel_reread_save = g_sel_rereading;
            if (ch == 'M')
            {
                delay_return_item(g_sel_items[g_sel_item_index].u);
            }
            g_sel_rereading = ch != '-';
            if (deselect_item(g_sel_items[g_sel_item_index].u))
            {
                output_sel(g_sel_item_index, ch == '-' ? 0 : 2, true);
            }
            g_sel_rereading = sel_reread_save;
            if (g_term_line >= g_sel_last_line)
            {
                sel_display();
                goto reask_selector;
            }
            break;
        }
        }
        if (g_can_home)
        {
            carriage_return();
        }
        std::fflush(stdout);
        if (++g_sel_item_index == g_sel_page_item_cnt)
        {
            g_sel_item_index = 0;
        }
    } while (g_sel_item_index != j);
    if (s_force_sel_pos >= 0)
    {
        g_sel_item_index = s_force_sel_pos;
    }
    goto position_selector;
}

static void sel_prompt()
{
    draw_mouse_bar(g_tc_COLS,false);
    if (g_can_home)
    {
        goto_xy(0, g_sel_last_line);
    }
#ifdef MAIL_CALL
    set_mail(false);
#endif
    if (g_sel_at_end)
    {
        std::sprintf(g_cmd_buf, "%s [%c%c] --",
                (!g_sel_prior_obj_cnt? "All" : "Bot"), s_end_char, s_page_char);
    }
    else
    {
        std::sprintf(g_cmd_buf, "%s%ld%% [%c%c] --",
                (!g_sel_prior_obj_cnt? "Top " : ""),
                (long)((g_sel_prior_obj_cnt+g_sel_page_obj_cnt)*100 / g_sel_total_obj_cnt),
                s_page_char, s_end_char);
    }
    interp(g_buf, sizeof g_buf, g_mail_call);
    std::sprintf(g_msg, "%s-- %s %s (%s%s order) -- %s", g_buf,
            g_sel_exclusive && g_in_ng? "SELECTED" : "Select", g_sel_mode_string,
            g_sel_direction<0? "reverse " : "", g_sel_sort_string, g_cmd_buf);
    color_string(COLOR_CMD,g_msg);
    g_term_col = std::strlen(g_msg);
    s_removed_prompt = RP_NONE;
}

static bool select_item(Selection u)
{
    switch (g_sel_mode)
    {
    case SM_MULTIRC:
        // multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
        TRN_ASSERT((g_sel_mask & (AGF_DEL | AGF_DEL_SEL)) == 0);
        if (!(u.mp->flags & static_cast<MultircFlags>(g_sel_mask)))
        {
            g_selected_count++;
        }
        u.mp->flags |= static_cast<MultircFlags>(g_sel_mask);
        break;

    case SM_ADD_GROUP:
        if (!(u.gp->flags & g_sel_mask))
        {
            g_selected_count++;
        }
        u.gp->flags = (u.gp->flags & ~AGF_DEL) | g_sel_mask;
        break;

    case SM_NEWSGROUP:
        if (!(u.np->flags & g_sel_mask))
        {
            g_selected_count++;
        }
        u.np->flags = (u.np->flags & ~NF_DEL) | static_cast<NewsgroupFlags>(g_sel_mask);
        break;

    case SM_OPTIONS:
        if (!select_option(u.op) || !ini_value(g_options_ini, u.op))
        {
            return false;
        }
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
        {
            g_selected_count++;
        }
        u.un->flags = (u.un->flags & ~UF_DEL) | static_cast<UniversalItemFlags>(g_sel_mask);
        break;
    }
    return true;
}

static bool delay_return_item(Selection u)
{
    switch (g_sel_mode)
    {
    case SM_MULTIRC:
    case SM_ADD_GROUP:
    case SM_NEWSGROUP:
    case SM_OPTIONS:
    case SM_UNIVERSAL:
        return false;

    case SM_ARTICLE:
        delay_unmark(u.ap);
        break;

    default:
    {
        Article* ap;
        if (g_sel_mode == SM_THREAD)
        {
            for (ap = first_art(u.sp); ap; ap = next_article(ap))
            {
                if (!!(ap->flags & AF_UNREAD) ^ g_sel_rereading)
                {
                    delay_unmark(ap);
                }
            }
        }
        else
        {
            for (ap = u.sp->articles; ap; ap = ap->subj_next)
            {
                if (!!(ap->flags & AF_UNREAD) ^ g_sel_rereading)
                {
                    delay_unmark(ap);
                }
            }
        }
        break;
    }
    }
    return true;
}

static bool deselect_item(Selection u)
{
    switch (g_sel_mode)
    {
    case SM_MULTIRC:
        // multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
        TRN_ASSERT((g_sel_mask & (AGF_DEL | AGF_DEL_SEL)) == 0);
        if (u.mp->flags & static_cast<MultircFlags>(g_sel_mask))
        {
            u.mp->flags &= ~static_cast<MultircFlags>(g_sel_mask);
            g_selected_count--;
        }
#if 0
        if (g_sel_rereading)
        {
            u.mp->flags |= MF_DEL;
        }
#endif
        break;

    case SM_ADD_GROUP:
        if (u.gp->flags & g_sel_mask)
        {
            u.gp->flags &= ~g_sel_mask;
            g_selected_count--;
        }
        if (g_sel_rereading)
        {
            u.gp->flags |= AGF_DEL;
        }
        break;

    case SM_NEWSGROUP:
        if (u.np->flags & static_cast<NewsgroupFlags>(g_sel_mask))
        {
            u.np->flags &= ~static_cast<NewsgroupFlags>(g_sel_mask);
            g_selected_count--;
        }
        if (g_sel_rereading)
        {
            u.np->flags |= NF_DEL;
        }
        break;

    case SM_OPTIONS:
        if (!select_option(u.op) || ini_value(g_options_ini, u.op))
        {
            return false;
        }
        break;

    case SM_THREAD:
        deselect_thread(u.sp->thread);
        break;

    case SM_SUBJECT:
        deselect_subject(u.sp);
        break;

    case SM_UNIVERSAL:
        if (u.un->flags & static_cast<UniversalItemFlags>(g_sel_mask))
        {
            u.un->flags &= ~static_cast<UniversalItemFlags>(g_sel_mask);
            g_selected_count--;
        }
        if (g_sel_rereading)
        {
            u.un->flags |= UF_DEL;
        }
        break;

    default:
        deselect_article(u.ap, AUTO_KILL_NONE);
        break;
    }
    return true;
}

static bool select_option(OptionIndex i)
{
    bool  changed = false;
    char**vals = ini_values(g_options_ini);

    if (*g_options_ini[i].item == '*')
    {
        g_option_flags[i] ^= OF_SEL;
        init_pages(FILL_LAST_PAGE);
        g_term_line = g_sel_last_line;
        return false;
    }

    goto_xy(0,g_sel_last_line);
    erase_line(g_mouse_bar_cnt > 0);     /* erase the prompt */
    color_object(COLOR_CMD, true);
    std::printf("Change `%s' (%s)",g_options_ini[i].item, g_options_ini[i].help_str);
    color_pop();        /* of COLOR_CMD */
    newline();
    *g_buf = '\0';
    char *oldval = save_str(quote_string(option_value(i)));
    char *val = vals[i] ? vals[i] : oldval;
    s_clean_screen = in_choice("> ", val, g_options_ini[i].help_str, MM_OPTION_EDIT_PROMPT);
    if (std::strcmp(g_buf,val) != 0)
    {
        char * to = g_buf;
        char* from = g_buf;
        parse_string(&to, &from);
        changed = true;
        if (vals[i])
        {
            std::free(vals[i]);
            g_selected_count--;
        }
        if (val != oldval && !std::strcmp(g_buf,oldval))
        {
            vals[i] = nullptr;
        }
        else
        {
            vals[i] = save_str(g_buf);
            g_selected_count++;
        }
    }
    std::free(oldval);
    if (s_clean_screen)
    {
        up_line();
        erase_line(true);
        sel_prompt();
        goto_xy(0,g_sel_items[g_sel_item_index].line);
        if (changed)
        {
            erase_line(false);
            display_option(i,g_sel_item_index);
            up_line();
        }
    }
    else
    {
        return false;
    }
    return true;
}

static void sel_cleanup()
{
    switch (g_sel_mode)
    {
    case SM_MULTIRC:
        break;

    case SM_ADD_GROUP:
        if (g_sel_rereading)
        {
            for (AddGroup *gp = g_first_add_group; gp; gp = gp->next)
            {
                if (gp->flags & AGF_DEL_SEL)
                {
                    if (!(gp->flags & AGF_SEL))
                    {
                        g_selected_count++;
                    }
                    gp->flags = (gp->flags&~(AGF_DEL_SEL|AGF_EXCLUDED))|AGF_SEL;
                }
                gp->flags &= ~AGF_DEL;
            }
        }
        else
        {
            for (AddGroup *gp = g_first_add_group; gp; gp = gp->next)
            {
                if (gp->flags & AGF_DEL)
                {
                    gp->flags = (gp->flags & ~AGF_DEL) | AGF_EXCLUDED;
                }
            }
        }
        break;

    case SM_NEWSGROUP:
        if (g_sel_rereading)
        {
            for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
            {
                if (np->flags & NF_DEL_SEL)
                {
                    if (!(np->flags & NF_SEL))
                    {
                        g_selected_count++;
                    }
                    np->flags = (np->flags & ~NF_DEL_SEL) | NF_SEL;
                }
                np->flags &= ~NF_DEL;
            }
        }
        else
        {
            for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
            {
                if (np->flags & NF_DEL)
                {
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
        if (g_sel_rereading)
        {
            g_sel_last_ap = nullptr;
            g_sel_last_sp = nullptr;
            for (Subject *sp = g_first_subject; sp; sp = sp->next)
            {
                unkill_subject(sp);
            }
        }
        else
        {
            if (g_sel_mode == SM_ARTICLE)
            {
                article_walk(mark_del_as_read, 0);
            }
            else
            {
                for (Subject *sp = g_first_subject; sp; sp = sp->next)
                {
                    if (sp->flags & SF_DEL)
                    {
                        sp->flags &= ~SF_DEL;
                        if (g_sel_mode == SM_THREAD)
                        {
                            kill_thread(sp->thread, AFFECT_UNSEL);
                        }
                        else
                        {
                            kill_subject(sp, AFFECT_UNSEL);
                        }
                    }
                }
            }
        }
        break;
    }
}

static bool mark_del_as_read(char *ptr, int arg)
{
    Article* ap = (Article*)ptr;
    if (ap->flags & AF_DEL)
    {
        ap->flags &= ~AF_DEL;
        set_read(ap);
    }
    return false;
}

static DisplayState sel_command(char_int ch)
{
    DisplayState ret;
    if (g_can_home)
    {
        goto_xy(0, g_sel_last_line);
    }
    s_clean_screen = true;
    g_term_scrolled = 0;
    g_page_line = 1;
    if (g_sel_mode == SM_NEWSGROUP)
    {
        if (g_sel_item_index < g_sel_page_item_cnt)
        {
            set_newsgroup(g_sel_items[g_sel_item_index].u.np);
        }
        else
        {
            g_newsgroup_ptr = nullptr;
        }
    }
do_command:
    *g_buf = ch;
    g_buf[1] = FINISH_CMD;
    g_output_chase_phrase = true;
    switch (ch)
    {
    case '>':
        g_sel_item_index = 0;
        if (next_page())
        {
            return DS_DISPLAY;
        }
        break;

    case '<':
        g_sel_item_index = 0;
        if (prev_page())
        {
            return DS_DISPLAY;
        }
        break;

    case '^':  case Ctl('r'):
        g_sel_item_index = 0;
        if (first_page())
        {
            return DS_DISPLAY;
        }
        break;

    case '$':
        g_sel_item_index = 0;
        if (last_page())
        {
            return DS_DISPLAY;
        }
        break;

    case Ctl('l'):
        return DS_DISPLAY;

    case Ctl('^'):
        erase_line(false);              /* erase the prompt */
        s_removed_prompt |= RP_NEWLINE;
#ifdef MAIL_CALL
        set_mail(true);          /* force a mail check */
#endif
        break;

    case '\r':  case '\n':
        if (!g_selected_count && g_sel_page_item_cnt)
        {
            if (g_sel_rereading || g_sel_items[g_sel_item_index].sel != 2)
            {
                select_item(g_sel_items[g_sel_item_index].u);
            }
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
        edit_kill_file();
        return DS_DISPLAY;

    case '&':  case '!':
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (!finish_command(true))      /* get rest of command */
        {
            if (s_clean_screen)
            {
                break;
            }
        }
        else
        {
            PUSH_SELECTOR();
            g_one_command = true;
            perform(g_buf, false);
            g_one_command = false;
            if (g_term_line != g_sel_last_line+1 || g_term_scrolled)
            {
                s_clean_screen = false;
            }
            POP_SELECTOR();
            if (!save_sel_mode)
            {
                return DS_RESTART;
            }
            if (s_clean_screen)
            {
                erase_line(false);
                return DS_ASK;
            }
        }
        ch = another_command(1);
        if (ch != '\0')
        {
            goto do_command;
        }
        return DS_DISPLAY;

    case 'v':
        newline();
        trn_version();
        ch = another_command(1);
        if (ch != '\0')
        {
            goto do_command;
        }
        return DS_DISPLAY;

    case '\\':
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (g_sel_mode == SM_NEWSGROUP)
        {
            std::printf("[%s] Cmd: ", g_newsgroup_ptr ? g_newsgroup_ptr->rc_line : "*End*");
        }
        else
        {
            std::fputs("Cmd: ", stdout);
        }
        std::fflush(stdout);
        get_cmd(g_buf);
        if (*g_buf == '\\')
        {
            goto the_default;
        }
        if (*g_buf != ' ' && *g_buf != '\n' && *g_buf != '\r')
        {
            ch = *g_buf;
            goto do_command;
        }
        if (s_clean_screen)
        {
            erase_line(false);
            break;
        }
        ch = another_command(1);
        if (ch != '\0')
        {
            goto do_command;
        }
        return DS_DISPLAY;

the_default:
    default:
        ret = s_extra_commands(ch);
        switch (ret)
        {
        case DS_ERROR:
            break;

        case DS_DO_COMMAND:
            ch = s_sel_ret;
            goto do_command;

        default:
            return ret;
        }
        std::strcpy(g_msg,"Type ? for help.");
        settle_down();
        if (s_clean_screen)
        {
            return DS_STATUS;
        }
        std::printf("\n%s\n",g_msg);
        ch = another_command(1);
        if (ch != '\0')
        {
            goto do_command;
        }
        return DS_DISPLAY;
    }
    return DS_ASK;
}

static bool sel_perform_change(long cnt, const char *obj_type)
{
    carriage_return();
    if (g_page_line == 1)
    {
        s_disp_status_line = 1;
        if (g_term_line != g_sel_last_line+1 || g_term_scrolled)
        {
            s_clean_screen = false;
        }
    }
    else
    {
        s_clean_screen = false;
    }

    if (g_error_occurred)
    {
        print_lines(g_msg, NO_MARKING);
        s_clean_screen = false;
        g_error_occurred = false;
    }

    int ret = perform_status_end(cnt, obj_type);
    if (ret)
    {
        s_disp_status_line = 1;
    }
    if (s_clean_screen)
    {
        if (ret != 2)
        {
            up_line();
            return true;
        }
    }
    else if (s_disp_status_line == 1)
    {
        print_lines(g_msg, NO_MARKING);
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
    {
        return 0;
    }
    if (ch > 1)
    {
        read_tty(g_buf,1);
        ch = *g_buf;
    }
    else
    {
        ch = pause_get_cmd();
    }
    if (ch != 0 && ch != '\n' && ch != '\r' && (!skip_q || ch != 'q'))
    {
        if (ch > 0)
        {
            /* try to optimize the screen update for some commands. */
            if (!std::strchr(g_sel_chars, ch) //
                && (std::strchr(SPECIAL_CMD_LETTERS, ch) || ch == Ctl('k')))
            {
                s_sel_ret = ch;
                return ch;
            }
            push_char(ch | 0200);
        }
    }
    return '\0';
}

static DisplayState article_commands(char_int ch)
{
    switch (ch)
    {
    case 'U':
        sel_cleanup();
        g_sel_rereading = !g_sel_rereading;
        g_sel_page_sp = nullptr;
        g_sel_page_app = nullptr;
        if (!cache_range(g_sel_rereading? g_abs_first : g_first_art, g_last_art))
        {
            g_sel_rereading = !g_sel_rereading;
        }
        return DS_RESTART;

    case '#':
        if (g_sel_page_item_cnt)
        {
            for (Subject *sp = g_first_subject; sp; sp = sp->next)
            {
                sp->flags &= ~SF_SEL;
            }
            g_selected_count = 0;
            deselect_item(g_sel_items[g_sel_item_index].u);
            select_item(g_sel_items[g_sel_item_index].u);
            if (!g_keep_the_group_static)
            {
                g_keep_the_group_static = 2;
            }
        }
        return DS_QUIT;

    case 'N':  case 'P':
        return DS_QUIT;

    case 'L':
        switch_dmode(&g_sel_art_display_mode);     /* sets g_msg */
        return DS_DISPLAY;

    case 'Y':
        if (!g_dm_count)
        {
            std::strcpy(g_msg,"No marked articles to yank back.");
            return DS_STATUS;
        }
        yank_back();
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        s_disp_status_line = 1;
        count_subjects(CS_NORM);
        g_sel_page_sp = nullptr;
        g_sel_page_app = nullptr;
        init_pages(PRESERVE_PAGE);
        return DS_DISPLAY;

    case '=':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        if (g_sel_mode == SM_ARTICLE)
        {
            set_selector(g_sel_thread_mode, SS_MAGIC_NUMBER);
            g_sel_page_sp = g_sel_page_app? g_sel_page_app[0]->subj : nullptr;
        }
        else
        {
            set_selector(SM_ARTICLE, SS_MAGIC_NUMBER);
            g_sel_page_app = nullptr;
        }
        count_subjects(CS_NORM);
        g_sel_item_index = 0;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'S':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
reask_output:
        in_char("Selector mode:  Threads, Subjects, Articles?", MM_SELECTOR_ORDER_PROMPT, "tsa");
        print_cmd();
        if (*g_buf == 'h' || *g_buf == 'H')
        {
            if (g_verbose)
            {
                std::fputs("\n"
                      "Type t or SP to display/select thread groups (threads the group, if needed).\n"
                      "Type s to display/select subject groups.\n"
                      "Type a to display/select individual articles.\n"
                      "Type q to leave things as they are.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\n"
                      "t or SP selects thread groups (threads the group too).\n"
                      "s selects subject groups.\n"
                      "a selects individual articles.\n"
                      "q does nothing.\n\n",
                      stdout);
            }
            s_clean_screen = false;
            goto reask_output;
        }
        else if (*g_buf == 'q')
        {
            if (g_can_home)
            {
                erase_line(false);
            }
            return DS_ASK;
        }
        if (std::isupper(*g_buf))
        {
            *g_buf = std::tolower(*g_buf);
        }
        set_sel_mode(*g_buf);
        count_subjects(CS_NORM);
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'O':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
reask_sort:
        if (g_sel_mode == SM_ARTICLE)
        {
            in_char(
                "Order by Date,Subject,Author,Number,subj-date Groups,Points?",
                MM_Q, "dsangpDSANGP");
        }
        else
        {
            in_char("Order by Date, Subject, Count, Lines, or Points?",
                    MM_Q, "dsclpDSCLP");
        }
        print_cmd();
        if (*g_buf == 'h' || *g_buf == 'H')
        {
            if (g_verbose)
            {
                std::fputs("\n"
                      "Type d or SP to order the displayed items by date.\n"
                      "Type s to order the items by subject.\n"
                      "Type p to order the items by score points.\n",
                      stdout);
                if (g_sel_mode == SM_ARTICLE)
                {
                    std::fputs("Type a to order the items by author.\n"
                          "Type n to order the items by number.\n"
                          "Type g to order the items in subject-groups by date.\n",
                          stdout);
                }
                else
                {
                    std::fputs("Type c to order the items by count.\n", stdout);
                }
                std::fputs("Typing your selection in upper case it will reverse the selected order.\n"
                      "Type q to leave things as they are.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\n"
                      "d or SP sorts by date.\n"
                      "s sorts by subject.\n"
                      "p sorts by points.\n",
                      stdout);
                if (g_sel_mode == SM_ARTICLE)
                {
                    std::fputs("a sorts by author.\n"
                          "g sorts in subject-groups by date.\n",
                          stdout);
                }
                else
                {
                    std::fputs("c sorts by count.\n", stdout);
                }
                std::fputs("Upper case reverses the sort.\n"
                      "q does nothing.\n"
                      "\n",
                      stdout);
            }
            s_clean_screen = false;
            goto reask_sort;
        }
        else if (*g_buf == 'q')
        {
            if (g_can_home)
            {
                erase_line(false);
            }
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
        {
            sel_cleanup();
        }
        set_selector(g_sel_mode, static_cast<SelectionSortMode>(g_sel_sort * -g_sel_direction));
        count_subjects(CS_NORM);
        g_sel_page_sp = nullptr;
        g_sel_page_app = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'E':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        g_sel_exclusive = !g_sel_exclusive;
        count_subjects(CS_NORM);
        g_sel_page_sp = nullptr;
        g_sel_page_app = nullptr;
        init_pages(FILL_LAST_PAGE);
        g_sel_item_index = 0;
        return DS_DISPLAY;

    case 'X':  case 'D':  case 'J':
        if (!g_sel_rereading)
        {
            if (g_sel_mode == SM_ARTICLE)
            {
                Article** app;
                Article **limit = g_art_ptr_list + g_art_ptr_list_size;
                if (ch == 'D')
                {
                    app = g_sel_page_app;
                }
                else
                {
                    app = g_art_ptr_list;
                }
                while (app < limit)
                {
                    Article *ap = *app;
                    if ((!(ap->flags & AF_SEL) ^ (ch == 'J')) || (ap->flags & AF_DEL))
                    {
                        if (ch == 'J' || !g_sel_exclusive || (ap->flags & AF_INCLUDED))
                        {
                            set_read(ap);
                        }
                    }
                    app++;
                    if (ch == 'D' && app == g_sel_next_app)
                    {
                        break;
                    }
                }
            }
            else
            {
                Subject* sp;
                if (ch == 'D')
                {
                    sp = g_sel_page_sp;
                }
                else
                {
                    sp = g_first_subject;
                }
                while (sp)
                {
                    if (((!(sp->flags & SF_SEL) ^ (ch == 'J')) && sp->misc) //
                        || (sp->flags & SF_DEL))
                    {
                        if (ch == 'J' || !g_sel_exclusive //
                            || (sp->flags & SF_INCLUDED))
                        {
                            kill_subject(sp, ch=='J'? AFFECT_ALL:AFFECT_UNSEL);
                        }
                    }
                    sp = sp->next;
                    if (ch == 'D' && sp == g_sel_next_sp)
                    {
                        break;
                    }
                }
            }
            count_subjects(CS_UNSELECT);
            if (g_obj_count && (ch == 'J' || (ch == 'D' && !g_selected_count)))
            {
                init_pages(FILL_LAST_PAGE);
                g_sel_item_index = 0;
                return DS_DISPLAY;
            }
            if (g_art_ptr_list && g_obj_count)
            {
                sort_articles();
            }
        }
        else if (ch == 'J')
        {
            for (Subject *sp = g_first_subject; sp; sp = sp->next)
            {
                deselect_subject(sp);
            }
            g_selected_subj_cnt = 0;
            g_selected_count = 0;
            return DS_DISPLAY;
        }
        return DS_QUIT;

    case 'T':
        if (!g_threaded_group)
        {
            std::strcpy(g_msg,"Group is not threaded.");
            return DS_STATUS;
        }
        /* FALL THROUGH */

    case 'A':
        if (!g_sel_page_item_cnt)
        {
            dingaling();
            break;
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (g_sel_mode == SM_ARTICLE)
        {
            g_artp = g_sel_items[g_sel_item_index].u.ap;
        }
        else
        {
            Subject* sp = g_sel_items[g_sel_item_index].u.sp;
            if (g_sel_mode == SM_THREAD)
            {
                while (!sp->misc)
                {
                    sp = sp->thread_link;
                }
            }
            g_artp = sp->articles;
        }
        g_art = article_num(g_artp);
        /* This call executes the action too */
        switch (ask_memorize(ch))
        {
        case 'J': case 'j': case 'K':  case ',':
            count_subjects(g_sel_rereading? CS_NORM : CS_UNSELECT);
            init_pages(PRESERVE_PAGE);
            std::strcpy(g_msg,"Kill memorized.");
            s_disp_status_line = 1;
            return DS_DISPLAY;

        case '+': case '.': case 'S': case 'm':
            std::strcpy(g_msg,"Selection memorized.");
            s_disp_status_line = 1;
            return DS_UPDATE;

        case 'c':  case 'C':
            std::strcpy(g_msg,"Auto-commands cleared.");
            s_disp_status_line = 1;
            return DS_UPDATE;

        case 'q':
            return DS_UPDATE;

        case 'Q':
            break;
        }
        if (g_can_home)
        {
            erase_line(false);
        }
        break;

    case ';':
        s_sel_ret = ';';
        return DS_QUIT;

    case ':':
        if (g_sel_page_item_cnt)
        {
            if (g_sel_mode == SM_ARTICLE)
            {
                g_artp = g_sel_items[g_sel_item_index].u.ap;
            }
            else
            {
                Subject* sp = g_sel_items[g_sel_item_index].u.sp;
                if (g_sel_mode == SM_THREAD)
                {
                    while (!sp->misc)
                    {
                        sp = sp->thread_link;
                    }
                }
                g_artp = sp->articles;
            }
            g_art = article_num(g_artp);
        }
        else
        {
            g_art = 0;
        }
        /* FALL THROUGH */

    case '/':
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (!finish_command(true))      /* get rest of command */
        {
            if (s_clean_screen)
            {
                break;
            }
        }
        else
        {
            if (ch == ':')
            {
                thread_perform();
                if (!g_sel_rereading)
                {
                    for (Subject *sp = g_first_subject; sp; sp = sp->next)
                    {
                        if (sp->flags & SF_DEL)
                        {
                            sp->flags = SF_NONE;
                            if (g_sel_mode == SM_THREAD)
                            {
                                kill_thread(sp->thread, AFFECT_UNSEL);
                            }
                            else
                            {
                                kill_subject(sp, AFFECT_UNSEL);
                            }
                        }
                    }
                }
            }
            else
            {
                /* Force the search to begin at g_absfirst or g_firstart,
                ** depending upon whether they specified the 'r' option.
                */
                g_art = g_last_art+1;
                switch (art_search(g_buf, sizeof g_buf, false))
                {
                case SRCH_ERROR:
                case SRCH_ABORT:
                    break;

                case SRCH_INTR:
                    error_msg("Interrupted");
                    break;

                case SRCH_DONE:
                case SRCH_SUBJ_DONE:
                case SRCH_FOUND:
                    break;

                case SRCH_NOT_FOUND:
                    error_msg("Not found.");
                    break;
                }
            }
            g_sel_item_index = 0;
            /* Recount, in case something has changed. */
            count_subjects(g_sel_rereading? CS_NORM : CS_UNSELECT);

            if (sel_perform_change(g_newsgroup_ptr->to_read, "article"))
            {
                return DS_UPDATE;
            }
            if (s_clean_screen)
            {
                return DS_DISPLAY;
            }
        }
        if (another_command(1))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    case 'c':
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        ch = ask_catchup();
        if (ch == 'y' || ch == 'u')
        {
            count_subjects(CS_UNSELECT);
            if (ch != 'u' && g_obj_count)
            {
                g_sel_page_sp = nullptr;
                g_sel_page_app = nullptr;
                init_pages(FILL_LAST_PAGE);
                return DS_DISPLAY;
            }
            s_sel_ret = 'Z';
            return DS_QUIT;
        }
        if (ch != 'N')
        {
            return DS_DISPLAY;
        }
        if (g_can_home)
        {
            erase_line(false);
        }
        break;

    case 'h':
    case '?':
        univ_help(UHELP_ARTSEL);
        return DS_RESTART;

    case 'H':
        newline();
        if (another_command(help_article_selector()))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    default:
        return DS_ERROR;
    }
    return DS_ASK;
}

static DisplayState newsgroup_commands(char_int ch)
{
    switch (ch)
    {
    case Ctl('n'):
    case Ctl('p'):
        return DS_QUIT;

    case 'U':
        sel_cleanup();
        g_sel_rereading = !g_sel_rereading;
        g_sel_page_np = nullptr;
        return DS_RESTART;

    case 'L':
        switch_dmode(&g_sel_grp_display_mode);     /* sets g_msg */
        if (*g_sel_grp_display_mode != 's' && !g_multirc->first->data_source->desc_sf.hp)
        {
            newline();
            return DS_RESTART;
        }
        return DS_DISPLAY;

    case 'X':  case 'D':  case 'J':
        if (!g_sel_rereading)
        {
            NewsgroupData* np;
            if (ch == 'D')
            {
                np = g_sel_page_np;
            }
            else
            {
                np = g_first_newsgroup;
            }
            while (np)
            {
                if (((!(np->flags & NF_SEL) ^ (ch == 'J')) && np->to_read != TR_UNSUB) //
                    || (np->flags & NF_DEL))
                {
                    if (ch == 'J' || (np->flags & NF_INCLUDED))
                    {
                        catch_up(np, 0, 0);
                    }
                    np->flags &= ~(NF_DEL|NF_SEL);
                }
                np = np->next;
                if (ch == 'D' && np == g_sel_next_np)
                {
                    break;
                }
            }
            if (ch == 'J' || (ch == 'D' && !g_selected_count))
            {
                init_pages(FILL_LAST_PAGE);
                if (g_sel_total_obj_cnt)
                {
                    g_sel_item_index = 0;
                    return DS_DISPLAY;
                }
            }
        }
        else if (ch == 'J')
        {
            for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
            {
                np->flags &= ~NF_DEL_SEL;
            }
            g_selected_count = 0;
            return DS_DISPLAY;
        }
        return DS_QUIT;

    case '=':
    {
        sel_cleanup();
        g_missing_count = 0;
        for (NewsgroupData *np = g_first_newsgroup; np; np = np->next)
        {
            if (np->to_read > TR_UNSUB && np->to_read < g_newsgroup_min_to_read)
            {
                g_newsgroup_to_read++;
            }
            np->abs_first = 0;
        }
        erase_line(false);
        check_active_refetch(true);
        return DS_RESTART;
    }

    case 'O':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
reask_sort:
        in_char("Order by Newsrc, Group name, or Count?", MM_Q, "ngcNGC");
        print_cmd();
        switch (*g_buf)
        {
        case 'n': case 'N':
            break;

        case 'g': case 'G':
            *g_buf += 's' - 'g';                /* Group name == SS_STRING */
            break;

        case 'c': case 'C':
            break;

        case 'h': case 'H':
            if (g_verbose)
            {
                std::fputs("\n"
                      "Type n or SP to order the newsgroups in the .newsrc order.\n"
                      "Type g to order the items by group name.\n"
                      "Type c to order the items by count.\n"
                      "Typing your selection in upper case it will reverse the selected order.\n"
                      "Type q to leave things as they are.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\n"
                      "n or SP sorts by .newsrc.\n"
                      "g sorts by group name.\n"
                      "c sorts by count.\n"
                      "Upper case reverses the sort.\n"
                      "q does nothing.\n\n",
                      stdout);
            }
            s_clean_screen = false;
            goto reask_sort;

        default:
            if (g_can_home)
            {
                erase_line(false);
            }
            return DS_ASK;
        }
        set_sel_sort(g_sel_mode,*g_buf);
        g_sel_page_np = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'R':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        set_selector(g_sel_mode, static_cast<SelectionSortMode>(g_sel_sort * -g_sel_direction));
        g_sel_page_np = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'E':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        g_sel_exclusive = !g_sel_exclusive;
        g_sel_page_np = nullptr;
        init_pages(FILL_LAST_PAGE);
        g_sel_item_index = 0;
        return DS_DISPLAY;

    case ':':
#if 0
        if (g_ngptr != g_current_ng)
        {
            g_recent_ng = g_current_ng;
            g_current_ng = g_ngptr;
        }
        /* FALL THROUGH */
#endif

    case '/':
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (!finish_command(true))      /* get rest of command */
        {
            if (s_clean_screen)
            {
                break;
            }
        }
        else
        {
            if (ch == ':')
            {
                newsgroup_sel_perform();
            }
            else
            {
                g_newsgroup_ptr = nullptr;
                switch (newsgroup_search(g_buf, false))
                {
                case NGS_ERROR:
                case NGS_ABORT:
                    break;

                case NGS_INTR:
                    error_msg("Interrupted");
                    break;

                case NGS_FOUND:
                case NGS_NOT_FOUND:
                case NGS_DONE:
                    break;
                }
                g_newsgroup_ptr = g_current_newsgroup;
            }
            g_sel_item_index = 0;

            if (sel_perform_change(g_newsgroup_to_read, "newsgroup"))
            {
                return DS_UPDATE;
            }
            if (s_clean_screen)
            {
                return DS_DISPLAY;
            }
        }
        if (another_command(1))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    case 'c':
        if (g_sel_item_index < g_sel_page_item_cnt)
        {
            set_newsgroup(g_sel_items[g_sel_item_index].u.np);
        }
        else
        {
            std::strcpy(g_msg, "No newsgroup to catchup.");
            s_disp_status_line = 1;
            return DS_UPDATE;
        }
        if (g_newsgroup_ptr != g_current_newsgroup)
        {
            g_recent_newsgroup = g_current_newsgroup;
            g_current_newsgroup = g_newsgroup_ptr;
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        ch = ask_catchup();
        if (ch == 'y' || ch == 'u')
        {
            return DS_DISPLAY;
        }
        if (ch != 'N')
        {
            return DS_DISPLAY;
        }
        if (g_can_home)
        {
            erase_line(false);
        }
        break;

    case 'h':
    case '?':
        univ_help(UHELP_NGSEL);
        return DS_RESTART;

    case 'H':
        newline();
        if (another_command(help_newsgroup_selector()))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    case ';':
        s_sel_ret = ';';
        return DS_QUIT;

    default:
    {
        Selection u;
        InputNewsgroupResult ret;
        bool was_at_top = !g_sel_prior_obj_cnt;
        PUSH_SELECTOR();
        if (!(s_removed_prompt & RP_NEWLINE))
        {
            erase_line(g_mouse_bar_cnt > 0);     /* erase the prompt */
            s_removed_prompt = RP_ALL;
            std::printf("[%s] Cmd: ", g_newsgroup_ptr? g_newsgroup_ptr->rc_line : "*End*");
            std::fflush(stdout);
        }
        g_default_cmd = "\\";
        set_mode(GM_READ,MM_NEWSGROUP_LIST);
        if (ch == '\\')
        {
            std::putchar(ch);
            std::fflush(stdout);
        }
        else
        {
            push_char(ch | 0200);
        }
        do
        {
            ret = input_newsgroup();
        } while (ret == ING_INPUT);
        set_mode(GM_SELECTOR,MM_NEWSGROUP_SELECTOR);
        POP_SELECTOR();
        switch (ret)
        {
        case ING_NO_SERVER:
            if (g_multirc)
            {
                if (!was_at_top)
                {
                    (void) first_page();
                }
                return DS_RESTART;
            }
            /* FALL THROUGH */

        case ING_QUIT:
            s_sel_ret = 'q';
            return DS_QUIT;

        case ING_ERROR:
            return DS_ERROR;

        case ING_ERASE:
            if (s_clean_screen)
            {
                erase_line(false);
                return DS_ASK;
            }
            break;

        default:
            if (!save_sel_mode)
            {
                return DS_RESTART;
            }
            if (g_term_line == g_sel_last_line)
            {
                newline();
            }
            if (g_term_line != g_sel_last_line+1 || g_term_scrolled)
            {
                s_clean_screen = false;
            }
            break;
        }
        g_sel_item_index = 0;
        init_pages(PRESERVE_PAGE);
        if (ret == ING_SPECIAL && g_newsgroup_ptr && g_newsgroup_ptr->to_read < g_newsgroup_min_to_read)
        {
            g_newsgroup_ptr->flags |= NF_INCLUDED;
            g_sel_total_obj_cnt++;
            ret = ING_DISPLAY;
        }
        u.np = g_newsgroup_ptr;
        if ((calc_page(u) || ret == ING_DISPLAY) && s_clean_screen)
        {
            return DS_DISPLAY;
        }
        if (ret == ING_MESSAGE)
        {
            s_clean_screen = false;
            return DS_STATUS;
        }
        if (was_at_top)
        {
            (void) first_page();
        }
        if (s_clean_screen)
        {
            return DS_ASK;
        }
        newline();
        if (another_command(1))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;
    }
    }
    return DS_ASK;
}

static DisplayState add_group_commands(char_int ch)
{
    switch (ch)
    {
    case 'O':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
reask_sort:
        in_char("Order by Natural-order, Group name, or Count?", MM_Q, "ngcNGC");
        print_cmd();
        switch (*g_buf)
        {
        case 'n': case 'N':
            break;

        case 'g': case 'G':
            *g_buf += 's' - 'g';                /* Group name == SS_STRING */
            break;

        case 'c': case 'C':
            break;

        case 'h': case 'H':
            if (g_verbose)
            {
                std::fputs("\n"
                      "Type n or SP to order the items in their naturally occurring order.\n"
                      "Type g to order the items by newsgroup name.\n"
                      "Type c to order the items by article count.\n"
                      "Typing your selection in upper case it will reverse the selected order.\n"
                      "Type q to leave things as they are.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\n"
                      "n or SP sorts by natural order.\n"
                      "g sorts by newsgroup name.\n"
                      "c sorts by article count.\n"
                      "Upper case reverses the sort.\n"
                      "q does nothing.\n\n",
                      stdout);
            }
            s_clean_screen = false;
            goto reask_sort;

        default:
            if (g_can_home)
            {
                erase_line(false);
            }
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
        {
            sel_cleanup();
        }
        set_selector(g_sel_mode, static_cast<SelectionSortMode>(g_sel_sort * -g_sel_direction));
        g_sel_page_gp = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'E':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        g_sel_exclusive = !g_sel_exclusive;
        g_sel_page_gp = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'L':
        switch_dmode(&g_sel_grp_display_mode);     /* sets g_msg */
        if (*g_sel_grp_display_mode != 's' && !g_data_source->desc_sf.hp)
        {
            newline();
            return DS_RESTART;
        }
        return DS_DISPLAY;

    case 'X':  case 'D':  case 'J':
        if (!g_sel_rereading)
        {
            AddGroup* gp;
            if (ch == 'D')
            {
                gp = g_sel_page_gp;
            }
            else
            {
                gp = g_first_add_group;
            }
            while (gp)
            {
                if ((!(gp->flags & AGF_SEL) ^ (ch == 'J' ? AGF_SEL : AGF_NONE)) //
                    || (gp->flags & AGF_DEL))
                {
                    if (ch == 'J' || (gp->flags & AGF_INCLUDED))
                    {
                        gp->flags |= AGF_EXCLUDED;
                    }
                    gp->flags &= ~(AGF_DEL|AGF_SEL);
                }
                gp = gp->next;
                if (ch == 'D' && gp == g_sel_next_gp)
                {
                    break;
                }
            }
            if (ch == 'J' || (ch == 'D' && !g_selected_count))
            {
                init_pages(FILL_LAST_PAGE);
                if (g_sel_total_obj_cnt)
                {
                    g_sel_item_index = 0;
                    return DS_DISPLAY;
                }
            }
        }
        else if (ch == 'J')
        {
            for (AddGroup *gp = g_first_add_group; gp; gp = gp->next)
            {
                gp->flags &= ~AGF_DEL_SEL;
            }
            g_selected_count = 0;
            return DS_DISPLAY;
        }
        return DS_QUIT;

    case ':':
    case '/':
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (!finish_command(true))      /* get rest of command */
        {
            if (s_clean_screen)
            {
                break;
            }
        }
        else
        {
            if (ch == ':')
            {
                add_group_sel_perform();
            }
            else
            {
                switch (newsgroup_search(g_buf,false))
                {
                case NGS_ERROR:
                case NGS_ABORT:
                    break;

                case NGS_INTR:
                    error_msg("Interrupted");
                    break;

                case NGS_FOUND:
                case NGS_NOT_FOUND:
                case NGS_DONE:
                    break;
                }
            }
            g_sel_item_index = 0;

            if (sel_perform_change(g_newsgroup_to_read, "newsgroup"))
            {
                return DS_UPDATE;
            }
            if (s_clean_screen)
            {
                return DS_DISPLAY;
            }
        }
        if (another_command(1))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    case 'h':
    case '?':
        univ_help(UHELP_ADDSEL);
        return DS_RESTART;

    case 'H':
        newline();
        if (another_command(help_add_group_selector()))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    default:
        return DS_ERROR;
    }
    return DS_ASK;
}

static DisplayState multirc_commands(char_int ch)
{
    switch (ch)
    {
    case 'R':
        set_selector(g_sel_mode, static_cast<SelectionSortMode>(g_sel_sort * -g_sel_direction));
        g_sel_page_mp = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'E':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        g_sel_exclusive = !g_sel_exclusive;
        g_sel_page_mp = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case '/':
        break;

    case 'h':
    case '?':
        univ_help(UHELP_MULTIRC);
        return DS_RESTART;

    case 'H':
        newline();
        if (another_command(help_multirc()))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    default:
        return DS_ERROR;
    }
    return DS_ASK;
}

static DisplayState option_commands(char_int ch)
{
    switch (ch)
    {
    case 'R':
        set_selector(g_sel_mode, static_cast<SelectionSortMode>(g_sel_sort * -g_sel_direction));
        g_sel_page_op = 1;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'E':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        g_sel_exclusive = !g_sel_exclusive;
        g_sel_page_op = 1;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'S':
        return DS_QUIT;

    case '/':
    {
        Selection u;
        char*     pattern;
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
        if (!finish_command(true))      /* get rest of command */
        {
            break;
        }
        for (pattern = g_buf; *pattern == ' '; pattern++)
        {
        }
        char *s = compile(&g_opt_compex, pattern, true, true);
        if (s != nullptr)
        {
            std::strcpy(g_msg,s);
            return DS_STATUS;
        }
        int i = g_sel_items[g_sel_item_index].u.op;
        int j = g_sel_items[g_sel_item_index].u.op;
        do
        {
            if (++i > g_obj_count)
            {
                i = 1;
            }
            if (*g_options_ini[i].item == '*')
            {
                continue;
            }
            if (execute(&g_opt_compex,g_options_ini[i].item))
            {
                break;
            }
        } while (i != j);
        u.op = static_cast<OptionIndex>(i);
        if (!(g_option_flags[i] & OF_INCLUDED))
        {
            for (j = i-1; *g_options_ini[j].item != '*'; j--)
            {
            }
            g_option_flags[j] |= OF_SEL;
            init_pages(FILL_LAST_PAGE);
            calc_page(u);
            return DS_DISPLAY;
        }
        if (calc_page(u))
        {
            return DS_DISPLAY;
        }
        return DS_ASK;
    }

    case 'h':
    case '?':
        univ_help(UHELP_OPTIONS);
        return DS_RESTART;

    case 'H':
        newline();
        if (another_command(help_options()))
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    default:
        return DS_ERROR;
    }
    return DS_ASK;
}

static DisplayState universal_commands(char_int ch)
{
    switch (ch)
    {
    case 'R':
        set_selector(g_sel_mode, static_cast<SelectionSortMode>(g_sel_sort * -g_sel_direction));
        sel_page_univ = nullptr;
        init_pages(FILL_LAST_PAGE);
        return DS_DISPLAY;

    case 'E':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
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
        univ_redo_file();
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
        {
            return DS_DO_COMMAND;
        }
        return DS_DISPLAY;

    case 'O':
        if (!g_sel_rereading)
        {
            sel_cleanup();
        }
        erase_line(g_mouse_bar_cnt > 0); /* erase the prompt */
        s_removed_prompt = RP_ALL;
reask_sort:
        in_char("Order by Natural, or score Points?", MM_Q, "npNP");
        print_cmd();
        if (*g_buf == 'h' || *g_buf == 'H')
        {
            if (g_verbose)
            {
                std::fputs("\n"
                      "Type n or SP to order the items in the natural order.\n"
                      "Type p to order the items by score points.\n"
                      "Typing your selection in upper case it will reverse the selected order.\n"
                      "Type q to leave things as they are.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\n"
                      "n or SP sorts by natural order.\n"
                      "p sorts by score.\n"
                      "Upper case reverses the sort.\n"
                      "q does nothing.\n\n",
                      stdout);
            }
            s_clean_screen = false;
            goto reask_sort;
        }
        else if (*g_buf == 'q' || (std::tolower(*g_buf) != 'n' && std::tolower(*g_buf) != 'p'))
        {
            if (g_can_home)
            {
                erase_line(false);
            }
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

    if (!*++*dmode_cpp)
    {
        do
        {
            --*dmode_cpp;
        } while ((*dmode_cpp)[-1] != '*');
    }
    switch (**dmode_cpp)
    {
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
    std::sprintf(g_msg,"(%s display style)",s);
    s_disp_status_line = 1;
}

static int find_line(int y)
{
    int i;
    for (i = 0; i < g_sel_page_item_cnt; i++)
    {
        if (g_sel_items[i].line > y)
        {
            break;
        }
    }
    if (i > 0)
    {
        i--;
    }
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
    if (check_mouse_bar(btn, x,y, btn_clk, x_clk,y_clk))
    {
        return;
    }

    if (btn != 3)
    {
        /* Handle button-down event */
        switch (btn_clk)
        {
        case 0:
        case 1:
            if (y > 0 && y < g_sel_last_line)
            {
                if (btn & 4)
                {
                    push_char(btn_clk == 0? '\n' : 'Z');
                    g_mouse_is_down = false;
                }
                else
                {
                    s_force_sel_pos = find_line(y);
                    if (g_use_sel_num)
                    {
                        push_char(('0' + (s_force_sel_pos+1) % 10) | 0200);
                        push_char(('0' + (s_force_sel_pos+1)/10) | 0200);
                    }
                    else
                    {
                        push_char(g_sel_chars[s_force_sel_pos] | 0200);
                    }
                    if (btn == 1)
                    {
                        push_char(Ctl('g') | 0200);
                    }
                }
            }
            break;

        case 2:
            break;
        }
    }
    else
    {
        /* Handle the button-up event */
        switch (btn_clk)
        {
        case 0:
            if (!y)
            {
                push_char('<' | 0200);
            }
            else if (y >= g_sel_last_line)
            {
                push_char(' ');
            }
            else
            {
                int i = find_line(y);
                if (g_sel_items[i].line != g_term_line)
                {
                    if (g_use_sel_num)
                    {
                        push_char(('0' + (i+1) % 10) | 0200);
                        push_char(('0' + (i+1) / 10) | 0200);
                    }
                    else
                    {
                        push_char(g_sel_chars[i] | 0200);
                    }
                    push_char('-' | 0200);
                    s_force_sel_pos = i;
                }
            }
            break;

        case 1:
            if (!y)
            {
                push_char('<' | 0200);
            }
            else if (y >= g_sel_last_line)
            {
                push_char('>' | 0200);
            }
            break;

        case 2:
            /* move forward or backwards a page:
             *   if cursor in top half: backwards
             *   if cursor in bottom half: forwards
             */
            if (y < g_tc_LINES/2)
            {
                push_char('<' | 0200);
            }
            else
            {
                push_char('>' | 0200);
            }
            break;
        }
    }
}

/* Icky placement, but the PUSH/POP stuff is local to this file */
int univ_visit_group(const char *group_name)
{
    PUSH_SELECTOR();

    univ_visit_group_main(group_name);

    POP_SELECTOR();
    return 0;           /* later may have some error return values */
}

/* later consider returning universal_selector() value */
void univ_visit_help(HelpLocation where)
{
    PUSH_SELECTOR();
    PUSH_UNIV_SELECTOR();

    univ_help_main(where);
    (void)universal_selector();

    POP_UNIV_SELECTOR();
    POP_SELECTOR();
}
