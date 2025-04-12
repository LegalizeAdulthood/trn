/* rt-page.c
 * vi: set sw=4 ts=8 ai sm noet :
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "rt-page.h"

#include "addng.h"
#include "cache.h"
#include "color.h"
#include "datasrc.h"
#include "env.h"
#include "list.h"
#include "ngdata.h"
#include "only.h"
#include "opt.h"
#include "rcln.h"
#include "rcstuff.h"
#include "rt-select.h"
#include "rt-util.h"
#include "rthread.h"
#include "string-algos.h"
#include "terminal.h"
#include "trn.h"
#include "univ.h"
#include "utf.h"
#include "util.h"

#include <algorithm>

SEL_ITEM  g_sel_items[MAX_SEL];
int       g_sel_total_obj_cnt{};
int       g_sel_prior_obj_cnt{};
int       g_sel_page_obj_cnt{};
int       g_sel_page_item_cnt{};
ARTICLE **g_sel_page_app{};
ARTICLE **g_sel_next_app{};
ARTICLE  *g_sel_last_ap{};
SUBJECT  *g_sel_page_sp{};
SUBJECT  *g_sel_next_sp{};
SUBJECT  *g_sel_last_sp{};
char     *g_sel_grp_dmode{};
char     *g_sel_art_dmode{};

static bool          s_group_init_done{true};
static int           s_sel_max_line_cnt{};
static int           s_sel_max_per_page{};
static sel_sort_mode s_sel_addgroupsort{SS_NATURAL};
static sel_sort_mode s_sel_univsort{SS_NATURAL};
static int           s_sel_next_op{};

static void sel_page_init();
static int count_subject_lines(const SUBJECT *subj, int *selptr);
static int count_thread_lines(const SUBJECT *subj, int *selptr);
static void display_article(const ARTICLE *ap, int ix, int sel);
static void display_subject(const SUBJECT *subj, int ix, int sel);
static void display_univ(const UNIV_ITEM *ui);
static void display_group(DATASRC *dp, char *group, int len, int max_len);

bool set_sel_mode(char_int ch)
{
    switch (ch) {
      case 'a':
        set_selector(g_sel_defaultmode = SM_ARTICLE, SS_MAGIC_NUMBER);
        break;
      case 's':
        set_selector(g_sel_defaultmode = SM_SUBJECT, SS_MAGIC_NUMBER);
        break;
      case 't':
        if (g_in_ng && !g_threaded_group) {
            bool always_save = g_thread_always;
            g_threaded_group = true;
            g_thread_always = true;
            if (g_sel_rereading)
            {
                g_firstart = g_absfirst;
            }
            printf("\nThreading the group. ");
            fflush(stdout);
            termdown(1);
            thread_open();
            g_thread_always = always_save;
            if (g_last_cached < g_lastart)
            {
                g_threaded_group = false;
            }
        }
        /* FALL THROUGH */
      case 'T':
        set_selector(g_sel_defaultmode = SM_THREAD, SS_MAGIC_NUMBER);
        break;
      default:
        set_selector(g_sel_defaultmode, SS_MAGIC_NUMBER);
        return false;
    }
    return true;
}

char *get_sel_order(sel_mode smode)
{
    sel_mode save_sel_mode = g_sel_mode;
    set_selector(smode, SS_MAGIC_NUMBER);
    sprintf(g_buf,"%s%s", g_sel_direction < 0? "reverse " : "",
            g_sel_sort_string);
    g_sel_mode = save_sel_mode;
    set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER);
    return g_buf;
}

bool set_sel_order(sel_mode smode, const char *str)
{
    bool reverse = false;

    if (*str == 'r' || *str == 'R') {
        str = skip_ne(str, ' ');
        str = skip_eq(str, ' ');
        reverse = true;
    }

    char ch = *str;
    if (reverse)
    {
        ch = islower(ch) ? toupper(ch) : ch;
    }
    else
    {
        ch = isupper(ch) ? tolower(ch) : ch;
    }

    return set_sel_sort(smode,ch);
}

bool set_sel_sort(sel_mode smode, char_int ch)
{
    sel_mode save_sel_mode = g_sel_mode;
    sel_sort_mode ssort;

    switch (ch) {
      case 'd':  case 'D':
        ssort = SS_DATE;
        break;
      case 's':  case 'S':
        ssort = SS_STRING;
        break;
      case 'a':  case 'A':
        ssort = SS_AUTHOR;
        break;
      case 'c':  case 'C':
        ssort = SS_COUNT;
        break;
      case 'n':  case 'N':
        ssort = SS_NATURAL;
        break;
      case 'g':  case 'G':
        ssort = SS_GROUPS;
        break;
      case 'l':  case 'L':
        ssort = SS_LINES;
        break;
      case 'p':  case 'P':
        ssort = SS_SCORE;
        break;
      default:
        return false;
    }

    g_sel_mode = smode;
    if (isupper(ch))
    {
        set_selector(SM_MAGIC_NUMBER, static_cast<sel_sort_mode>(-ssort));
    }
    else
    {
        set_selector(SM_MAGIC_NUMBER, ssort);
    }

    if (g_sel_mode != save_sel_mode) {
        g_sel_mode = save_sel_mode;
        set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER);
    }
    return true;
}

void set_selector(sel_mode smode, sel_sort_mode ssort)
{
    if (smode == SM_MAGIC_NUMBER) {
        if (g_sel_mode == SM_SUBJECT)
        {
            g_sel_mode = g_sel_threadmode;
        }
        smode = g_sel_mode;
    }
    else
    {
        g_sel_mode = smode;
    }
    if (!ssort) {
        switch (g_sel_mode) {
          case SM_MULTIRC:
            ssort = SS_NATURAL;
            break;
          case SM_ADDGROUP:
            ssort = s_sel_addgroupsort;
            break;
          case SM_NEWSGROUP:
            ssort = g_sel_newsgroupsort;
            break;
          case SM_OPTIONS:
            ssort = SS_NATURAL;
            break;
          case SM_THREAD:
          case SM_SUBJECT:
            ssort = g_sel_threadsort;
            break;
          case SM_ARTICLE:
            ssort = g_sel_artsort;
            break;
          case SM_UNIVERSAL:
            ssort = s_sel_univsort;
            break;
        }
    }
    if (ssort > SS_MAGIC_NUMBER) {
        g_sel_direction = 1;
        g_sel_sort = ssort;
    }
    else {
        g_sel_direction = -1;
        g_sel_sort = static_cast<sel_sort_mode>(-ssort);
    }

    if (g_sel_mode == SM_THREAD && !g_threaded_group)
    {
        g_sel_mode = SM_SUBJECT;
    }

    switch (g_sel_mode) {
      case SM_MULTIRC:
        g_sel_mode_string = "a newsrc group";
        break;
      case SM_ADDGROUP:
        g_sel_mode_string = "a newsgroup to add";
        s_sel_addgroupsort = ssort;
        break;
      case SM_NEWSGROUP:
        g_sel_mode_string = "a newsgroup";
        g_sel_newsgroupsort = ssort;
        break;
      case SM_OPTIONS:
        g_sel_mode_string = "an option to change";
        break;
      case SM_UNIVERSAL:
        g_sel_mode_string = "an item";
        s_sel_univsort = ssort;
        break;
      case SM_THREAD:
        g_sel_mode_string = "threads";
        g_sel_threadmode = smode;
        g_sel_threadsort = ssort;
        goto thread_subj_sort;
      case SM_SUBJECT:
        g_sel_mode_string = "subjects";
        g_sel_threadmode = smode;
        g_sel_threadsort = ssort;
     thread_subj_sort:
          if (g_sel_sort == SS_AUTHOR || g_sel_sort == SS_GROUPS || g_sel_sort == SS_NATURAL)
          {
              g_sel_sort = SS_DATE;
          }
        break;
      case SM_ARTICLE:
        g_sel_mode_string = "articles";
        g_sel_artsort = ssort;
        if (g_sel_sort == SS_COUNT)
        {
            g_sel_sort = SS_DATE;
        }
        break;
    }

    switch (g_sel_sort) {
      case SS_DATE:
        g_sel_sort_string = "date";
        break;
      case SS_STRING:
        g_sel_sort_string = "subject";
        break;
      case SS_AUTHOR:
        g_sel_sort_string = "author";
        break;
      case SS_COUNT:
        g_sel_sort_string = "count";
        break;
      case SS_LINES:
        g_sel_sort_string = "lines";
        break;
      case SS_NATURAL:
        g_sel_sort_string = "natural";
        break;
      case SS_GROUPS:
        if (g_sel_mode == SM_NEWSGROUP)
        {
            g_sel_sort_string = "group name";
        }
        else
        {
            g_sel_sort_string = "SubjDate";
        }
        break;
      case SS_SCORE:
        g_sel_sort_string = "points";
        break;
    }
}

static void sel_page_init()
{
    s_sel_max_line_cnt = g_tc_LINES - (g_tc_COLS - g_mousebar_width < 50? 6 : 5);
    g_sel_chars = get_val("SELECTCHARS", SELECTCHARS);
    /* The numeric option of up to 99 lines will require many adaptations
     * to be able to switch from a large numeric page (more than
     * strlen(g_sel_chars) lines) to an alphanumeric page. XXX
     */
    if (g_use_sel_num)
    {
        s_sel_max_per_page = 99;
    }
    else
    {
        s_sel_max_per_page = strlen(g_sel_chars);
    }
    s_sel_max_per_page = std::min(s_sel_max_per_page, static_cast<int>(MAX_SEL));
    s_sel_max_per_page = std::min(s_sel_max_per_page, s_sel_max_line_cnt);
    g_sel_page_obj_cnt = 0;
    g_sel_page_item_cnt = 0;
}

void init_pages(bool fill_last_page)
{
    SEL_UNION no_search;
    no_search.op = OI_NONE;
    sel_page_init();
try_again:
    g_sel_prior_obj_cnt = 0;
    g_sel_total_obj_cnt = 0;

    switch (g_sel_mode) {
      case SM_MULTIRC: {
          for (MULTIRC *mp = multirc_low(); mp; mp = multirc_next(mp)) {
            if (mp->first) {
                mp->flags |= MF_INCLUDED;
                g_sel_total_obj_cnt++;
            }
            else
            {
                mp->flags &= ~MF_INCLUDED;
            }
        }
        if (g_sel_page_mp == nullptr)
        {
            (void) first_page();
        }
        break;
      }
      case SM_NEWSGROUP: {
          bool save_the_rest = false;
        s_group_init_done = true;
        sort_newsgroups();
        g_selected_count = 0;
        g_obj_count = 0;
        for (NGDATA *np = g_first_ng; np; np = np->next) {
            if (g_sel_page_np == np)
            {
                g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
            }
            np->flags &= ~NF_INCLUDED;
            if (np->toread < TR_NONE)
            {
                continue;
            }
            if (!inlist(np->rcline))
            {
                continue;
            }
            if (np->abs1st)
            {
            }
            else if (save_the_rest) {
                s_group_init_done = false;
                np->toread = !g_sel_rereading;
            }
            else {
                /*g_ngptr = np; ??*/
                /*set_ngname(np->rcline);*/
                set_toread(np, ST_LAX);
                if (!np->rc->datasrc->act_sf.fp)
                {
                    save_the_rest = (g_sel_rereading ^ (np->toread > TR_NONE));
                }
            }
            if (g_paranoid) {
                g_current_ng = np;
                g_ngptr = np;
                /* this may move newsgroups around */
                cleanup_newsrc(np->rc);
                goto try_again;
            }
            if (!(np->flags & g_sel_mask)
             && (g_sel_rereading? np->toread != TR_NONE
                              : np->toread < g_ng_min_toread))
            {
                continue;
            }
            g_obj_count++;

            if (!g_sel_exclusive || (np->flags & g_sel_mask)) {
                if (np->flags & g_sel_mask)
                {
                    g_selected_count++;
                }
                else if (g_sel_rereading)
                {
                    np->flags |= NF_DEL;
                }
                np->flags |= NF_INCLUDED;
                g_sel_total_obj_cnt++;
            }
        }
        if (!g_sel_total_obj_cnt) {
            if (g_sel_exclusive) {
                g_sel_exclusive = false;
                g_sel_page_np = nullptr;
                goto try_again;
            }
            if (g_maxngtodo) {
                end_only();
                fputs(g_msg, stdout);
                newline();
                if (fill_last_page)
                {
                    get_anything();
                }
                g_sel_page_np = nullptr;
                goto try_again;
            }
        }
        if (!g_sel_page_np)
        {
            (void) first_page();
        }
        else if (g_sel_page_np == g_last_ng)
        {
            (void) last_page();
        }
        else if (g_sel_prior_obj_cnt && fill_last_page) {
            calc_page(no_search);
            if (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt)
            {
                (void) last_page();
            }
        }
        break;
      }
      case SM_ADDGROUP: {
          sort_addgroups();
        g_obj_count = 0;
        for (ADDGROUP *gp = g_first_addgroup; gp; gp = gp->next) {
            if (g_sel_page_gp == gp)
            {
                g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
            }
            gp->flags &= ~AGF_INCLUDED;
            if (!g_sel_rereading ^ !(gp->flags & AGF_EXCLUDED))
            {
                continue;
            }
            if (!g_sel_exclusive || (gp->flags & g_sel_mask)) {
                if (g_sel_rereading && !(gp->flags & g_sel_mask))
                {
                    gp->flags |= AGF_DEL;
                }
                gp->flags |= AGF_INCLUDED;
                g_sel_total_obj_cnt++;
            }
            g_obj_count++;
        }
        if (!g_sel_total_obj_cnt && g_sel_exclusive) {
            g_sel_exclusive = false;
            g_sel_page_gp = nullptr;
            goto try_again;
        }
        if (g_sel_page_gp == nullptr)
        {
            (void) first_page();
        }
        else if (g_sel_page_gp == g_last_addgroup)
        {
            (void) last_page();
        }
        else if (g_sel_prior_obj_cnt && fill_last_page) {
            calc_page(no_search);
            if (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt)
            {
                (void) last_page();
            }
        }
        break;
      }
      case SM_UNIVERSAL: {
          g_obj_count = 0;

        sort_univ();
        for (UNIV_ITEM *ui = g_first_univ; ui; ui = ui->next) {
            if (sel_page_univ == ui)
            {
                g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
            }
            ui->flags &= ~UF_INCLUDED;
            bool ui_elig = true;
            switch (ui->type) {
              case UN_GROUP_DESEL:
              case UN_VGROUP_DESEL:
              case UN_DELETED:
              case UN_VGROUP:               /* first-pass item */
                /* always ineligible items */
                ui_elig = false;
                break;
              case UN_NEWSGROUP: {
                  if (!ui->data.group.ng) {
                    ui_elig = false;
                    break;
                }
                NGDATA *np = find_ng(ui->data.group.ng);
                if (!np) {
                    ui_elig = false;
                    break;
                }
                if (!np->abs1st) {
                    g_toread_quiet = true;
                    set_toread(np, ST_LAX);
                    g_toread_quiet = false;
                }
                if (!(g_sel_rereading ^ (np->toread>TR_NONE))) {
                    ui_elig = false;
                }
                break;
              }
              case UN_ARTICLE:
                /* later: use the datasrc of the newsgroup */
                ui_elig = !was_read_group(g_datasrc, ui->data.virt.num,
                                          ui->data.virt.ng);
                if (g_sel_rereading)
                {
                    ui_elig = !ui_elig;
                }
                break;
              default:
                ui_elig = !g_sel_rereading;
                break;
            }
            if (!ui_elig)
            {
                continue;
            }
            if (!g_sel_exclusive || (ui->flags & static_cast<univitem_flags>(g_sel_mask))) {
                if (g_sel_rereading && !(ui->flags & static_cast<univitem_flags>(g_sel_mask)))
                {
                    ui->flags |= UF_DEL;
                }
                ui->flags |= UF_INCLUDED;
                g_sel_total_obj_cnt++;
            }
            g_obj_count++;
        }
        if (!g_sel_total_obj_cnt && g_sel_exclusive) {
            g_sel_exclusive = false;
            sel_page_univ = nullptr;
            goto try_again;
        }
        if (sel_page_univ == nullptr)
        {
            (void) first_page();
        }
        else if (sel_page_univ == g_last_univ)
        {
            (void) last_page();
        }
        else if (g_sel_prior_obj_cnt && fill_last_page) {
            calc_page(no_search);
            if (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt)
            {
                (void) last_page();
            }
        }
        break;
      }
      case SM_OPTIONS: {
          int included = 0;
        g_obj_count = 0;
        for (int op = 1; g_options_ini[op].checksum; op++) {
            if (g_sel_page_op == op)
            {
                g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
            }
            if (*g_options_ini[op].item == '*') {
                included = (g_option_flags[op] & OF_SEL);
                g_sel_total_obj_cnt++;
                g_option_flags[op] |= OF_INCLUDED;
            }
            else if (included) {
                g_sel_total_obj_cnt++;
                g_option_flags[op] |= OF_INCLUDED;
            }
            else
            {
                g_option_flags[op] &= ~OF_INCLUDED;
            }
            g_obj_count++;
        }
#if 0
        if (!g_sel_total_obj_cnt && g_sel_exclusive) {
            g_sel_exclusive = false;
            g_sel_page_op = nullptr;
            goto try_again;
        }
#endif
        if (g_sel_page_op < 1)
        {
            (void) first_page();
        }
        else if (g_sel_page_op >= g_obj_count)
        {
            (void) last_page();
        }
        else if (g_sel_prior_obj_cnt && fill_last_page) {
            calc_page(no_search);
            if (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt)
            {
                (void) last_page();
            }
        }
        break;
      }
      case SM_ARTICLE: {
        ARTICLE* ap;
        ARTICLE** app;
        ARTICLE** limit;

        if (g_sel_page_app) {
            int desired_flags = (g_sel_rereading? AF_EXISTS:(AF_EXISTS|AF_UNREAD));
            limit = g_artptr_list + g_artptr_list_size;
            ap = nullptr;
            for (app = g_sel_page_app; app < limit; app++) {
                ap = *app;
                if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == desired_flags)
                {
                    break;
                }
            }
            sort_articles();
            if (ap == nullptr)
            {
                g_sel_page_app = g_artptr_list + g_artptr_list_size;
            }
            else {
                for (app = g_artptr_list; app < limit; app++) {
                    if (*app == ap) {
                        g_sel_page_app = app;
                        break;
                    }
                }
            }
        } else
        {
            sort_articles();
        }

        while (g_sel_page_sp && g_sel_page_sp->misc == 0)
        {
            g_sel_page_sp = g_sel_page_sp->next;
        }
        /* The g_artptr_list contains only unread or read articles, never both */
        limit = g_artptr_list + g_artptr_list_size;
        for (app = g_artptr_list; app < limit; app++) {
            ap = *app;
            if (g_sel_rereading && !(ap->flags & g_sel_mask))
            {
                ap->flags |= AF_DEL;
            }
            if (g_sel_page_app == app
             || (!g_sel_page_app && ap->subj == g_sel_page_sp)) {
                g_sel_page_app = app;
                g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
            }
            if (!g_sel_exclusive || (ap->flags & g_sel_mask)) {
                g_sel_total_obj_cnt++;
                ap->flags |= AF_INCLUDED;
            } else
            {
                ap->flags &= ~AF_INCLUDED;
            }
        }
        if (g_sel_exclusive && !g_sel_total_obj_cnt) {
            g_sel_exclusive = false;
            g_sel_page_app = nullptr;
            goto try_again;
        }
        if (!g_sel_page_app)
        {
            (void) first_page();
        }
        else if (g_sel_page_app >= limit)
        {
            (void) last_page();
        }
        else if (g_sel_prior_obj_cnt && fill_last_page) {
            calc_page(no_search);
            if (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt)
            {
                (void) last_page();
            }
        }
        break;
      }
      default: {
          if (g_sel_page_sp) {
            while (g_sel_page_sp && g_sel_page_sp->misc == 0)
            {
                g_sel_page_sp = g_sel_page_sp->next;
            }
            sort_subjects();
            if (!g_sel_page_sp)
            {
                g_sel_page_sp = g_last_subject;
            }
        } else
        {
            sort_subjects();
        }
        for (SUBJECT *sp = g_first_subject; sp; sp = sp->next) {
            if (g_sel_rereading && !(sp->flags & g_sel_mask))
            {
                sp->flags |= SF_DEL;
            }

            SUBJECT *group_sp = sp;
            int      group_arts = sp->misc;

            if (!g_sel_exclusive || (sp->flags & g_sel_mask))
            {
                sp->flags |= SF_INCLUDED;
            }
            else
            {
                sp->flags &= ~SF_INCLUDED;
            }

            if (g_sel_page_sp == group_sp)
            {
                g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
            }
            if (g_sel_mode == SM_THREAD) {
                while (sp->next && sp->next->thread == sp->thread) {
                    sp = sp->next;
                    if (sp == g_sel_page_sp) {
                        g_sel_prior_obj_cnt = g_sel_total_obj_cnt;
                        g_sel_page_sp = group_sp;
                    }
                    sp->flags &= ~SF_INCLUDED;
                    if (sp->flags & g_sel_mask)
                    {
                        group_sp->flags |= SF_INCLUDED;
                    }
                    else if (g_sel_rereading)
                    {
                        sp->flags |= SF_DEL;
                    }
                    group_arts += sp->misc;
                }
            }
            if (group_sp->flags & SF_INCLUDED)
            {
                g_sel_total_obj_cnt += group_arts;
            }
        }
        if (g_sel_exclusive && !g_sel_total_obj_cnt) {
            g_sel_exclusive = false;
            g_sel_page_sp = nullptr;
            goto try_again;
        }
        if (!g_sel_page_sp)
        {
            (void) first_page();
        }
        else if (g_sel_page_sp == g_last_subject)
        {
            (void) last_page();
        }
        else if (g_sel_prior_obj_cnt && fill_last_page) {
            calc_page(no_search);
            if (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt)
            {
                (void) last_page();
            }
        }
      }
    }
}

bool first_page()
{
    g_sel_prior_obj_cnt = 0;

    switch (g_sel_mode) {
      case SM_MULTIRC: {
          for (MULTIRC *mp = multirc_low(); mp; mp = multirc_next(mp)) {
            if (mp->flags & MF_INCLUDED) {
                if (g_sel_page_mp != mp) {
                    g_sel_page_mp = mp;
                    return true;
                }
                break;
            }
        }
        break;
      }
      case SM_NEWSGROUP: {
          for (NGDATA *np = g_first_ng; np; np = np->next) {
            if (np->flags & NF_INCLUDED) {
                if (g_sel_page_np != np) {
                    g_sel_page_np = np;
                    return true;
                }
                break;
            }
        }
        break;
      }
      case SM_ADDGROUP: {
          for (ADDGROUP *gp = g_first_addgroup; gp; gp = gp->next) {
            if (gp->flags & AGF_INCLUDED) {
                if (g_sel_page_gp != gp) {
                    g_sel_page_gp = gp;
                    return true;
                }
                break;
            }
        }
        break;
      }
      case SM_UNIVERSAL: {
          for (UNIV_ITEM *ui = g_first_univ; ui; ui = ui->next) {
            if (ui->flags & UF_INCLUDED) {
                if (sel_page_univ != ui) {
                    sel_page_univ = ui;
                    return true;
                }
                break;
            }
        }
        break;
      }
      case SM_OPTIONS: {
        if (g_sel_page_op != 1) {
            g_sel_page_op = 1;
            return true;
        }
        break;
      }
      case SM_ARTICLE: {
          ARTICLE **limit = g_artptr_list + g_artptr_list_size;
        for (ARTICLE **app = g_artptr_list; app < limit; app++) {
            if ((*app)->flags & AF_INCLUDED) {
                if (g_sel_page_app != app) {
                    g_sel_page_app = app;
                    return true;
                }
                break;
            }
        }
        break;
      }
      default: {
          for (SUBJECT *sp = g_first_subject; sp; sp = sp->next) {
            if (sp->flags & SF_INCLUDED) {
                if (g_sel_page_sp != sp) {
                    g_sel_page_sp = sp;
                    return true;
                }
                break;
            }
        }
        break;
      }
    }
    return false;
}

bool last_page()
{
    g_sel_prior_obj_cnt = g_sel_total_obj_cnt;

    switch (g_sel_mode) {
      case SM_MULTIRC: {
        MULTIRC* mp = g_sel_page_mp;
        g_sel_page_mp = nullptr;
        if (!prev_page())
        {
            g_sel_page_mp = mp;
        }
        else if (mp != g_sel_page_mp)
        {
            return true;
        }
        break;
      }
      case SM_NEWSGROUP: {
        NGDATA* np = g_sel_page_np;
        g_sel_page_np = nullptr;
        if (!prev_page())
        {
            g_sel_page_np = np;
        }
        else if (np != g_sel_page_np)
        {
            return true;
        }
        break;
      }
      case SM_ADDGROUP: {
        ADDGROUP* gp = g_sel_page_gp;
        g_sel_page_gp = nullptr;
        if (!prev_page())
        {
            g_sel_page_gp = gp;
        }
        else if (gp != g_sel_page_gp)
        {
            return true;
        }
        break;
      }
      case SM_UNIVERSAL: {
        UNIV_ITEM* ui = sel_page_univ;
        sel_page_univ = nullptr;
        if (!prev_page())
        {
            sel_page_univ = ui;
        }
        else if (ui != sel_page_univ)
        {
            return true;
        }
        break;
      }
      case SM_OPTIONS: {
        int op = g_sel_page_op;
        g_sel_page_op = g_obj_count+1;
        if (!prev_page())
        {
            g_sel_page_op = op;
        }
        else if (op != g_sel_page_op)
        {
            return true;
        }
        break;
      }
      case SM_ARTICLE: {
        ARTICLE** app = g_sel_page_app;
        g_sel_page_app = g_artptr_list + g_artptr_list_size;
        if (!prev_page())
        {
            g_sel_page_app = app;
        }
        else if (app != g_sel_page_app)
        {
            return true;
        }
        break;
      }
      default: {
        SUBJECT* sp = g_sel_page_sp;
        g_sel_page_sp = nullptr;
        if (!prev_page())
        {
            g_sel_page_sp = sp;
        }
        else if (sp != g_sel_page_sp)
        {
            return true;
        }
        break;
      }
    }
    return false;
}

bool next_page()
{
    switch (g_sel_mode) {
      case SM_MULTIRC: {
        if (g_sel_next_mp) {
            g_sel_page_mp = g_sel_next_mp;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
      case SM_NEWSGROUP: {
        if (g_sel_next_np) {
            g_sel_page_np = g_sel_next_np;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
      case SM_ADDGROUP: {
        if (g_sel_next_gp) {
            g_sel_page_gp = g_sel_next_gp;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
      case SM_UNIVERSAL: {
        if (g_sel_next_univ) {
            sel_page_univ = g_sel_next_univ;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
      case SM_OPTIONS: {
        if (s_sel_next_op <= g_obj_count) {
            g_sel_page_op = s_sel_next_op;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
      case SM_ARTICLE: {
        if (g_sel_next_app < g_artptr_list + g_artptr_list_size) {
            g_sel_page_app = g_sel_next_app;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
      default: {
        if (g_sel_next_sp) {
            g_sel_page_sp = g_sel_next_sp;
            g_sel_prior_obj_cnt += g_sel_page_obj_cnt;
            return true;
        }
        break;
      }
    }
    return false;
}

bool prev_page()
{
    int item_cnt = 0;

    /* Scan the items in reverse to go back a page */
    switch (g_sel_mode) {
      case SM_MULTIRC: {
        MULTIRC* mp = g_sel_page_mp;
        MULTIRC* page_mp = g_sel_page_mp;

        if (!mp)
        {
            mp = multirc_high();
        }
        else
        {
            mp = multirc_prev(multirc_ptr(mp->num));
        }
        while (mp) {
            if (mp->flags & MF_INCLUDED) {
                page_mp = mp;
                g_sel_prior_obj_cnt--;
                if (++item_cnt >= s_sel_max_per_page)
                {
                    break;
                }
            }
            mp = multirc_prev(mp);
        }
        if (g_sel_page_mp != page_mp) {
            g_sel_page_mp = page_mp;
            return true;
        }
        break;
      }
      case SM_NEWSGROUP: {
        NGDATA* np = g_sel_page_np;
        NGDATA* page_np = g_sel_page_np;

        if (!np)
        {
            np = g_last_ng;
        }
        else
        {
            np = np->prev;
        }
        while (np) {
            if (np->flags & NF_INCLUDED) {
                page_np = np;
                g_sel_prior_obj_cnt--;
                if (++item_cnt >= s_sel_max_per_page)
                {
                    break;
                }
            }
            np = np->prev;
        }
        if (g_sel_page_np != page_np) {
            g_sel_page_np = page_np;
            return true;
        }
        break;
      }
      case SM_ADDGROUP: {
        ADDGROUP* gp = g_sel_page_gp;
        ADDGROUP* page_gp = g_sel_page_gp;

        if (!gp)
        {
            gp = g_last_addgroup;
        }
        else
        {
            gp = gp->prev;
        }
        while (gp) {
            if (gp->flags & AGF_INCLUDED) {
                page_gp = gp;
                g_sel_prior_obj_cnt--;
                if (++item_cnt >= s_sel_max_per_page)
                {
                    break;
                }
            }
            gp = gp->prev;
        }
        if (g_sel_page_gp != page_gp) {
            g_sel_page_gp = page_gp;
            return true;
        }
        break;
      }
      case SM_UNIVERSAL: {
        UNIV_ITEM* ui = sel_page_univ;
        UNIV_ITEM* page_ui = sel_page_univ;

        if (!ui)
        {
            ui = g_last_univ;
        }
        else
        {
            ui = ui->prev;
        }
        while (ui) {
            if (ui->flags & UF_INCLUDED) {
                page_ui = ui;
                g_sel_prior_obj_cnt--;
                if (++item_cnt >= s_sel_max_per_page)
                {
                    break;
                }
            }
            ui = ui->prev;
        }
        if (sel_page_univ != page_ui) {
            sel_page_univ = page_ui;
            return true;
        }
        break;
      }
      case SM_OPTIONS: {
        int op = g_sel_page_op;
        int page_op = g_sel_page_op;

        while (--op > 0) {
            if (g_option_flags[op] & OF_INCLUDED) {
                page_op = op;
                g_sel_prior_obj_cnt--;
                if (++item_cnt >= s_sel_max_per_page)
                {
                    break;
                }
            }
        }
        if (g_sel_page_op != page_op) {
            g_sel_page_op = page_op;
            return true;
        }
        break;
      }
      case SM_ARTICLE: {
          ARTICLE** page_app = g_sel_page_app;

        for (ARTICLE **app = g_sel_page_app; --app >= g_artptr_list; ) {
            if ((*app)->flags & AF_INCLUDED) {
                page_app = app;
                g_sel_prior_obj_cnt--;
                if (++item_cnt >= s_sel_max_per_page)
                {
                    break;
                }
            }
        }
        if (g_sel_page_app != page_app) {
            g_sel_page_app = page_app;
            return true;
        }
      }
      default: {
          SUBJECT*page_sp = g_sel_page_sp;
        int       line_cnt;

        int line = 2;
        for (SUBJECT *sp = (!page_sp ? g_last_subject : page_sp->prev); sp; sp=sp->prev) {
            int item_arts = sp->misc;
            if (g_sel_mode == SM_THREAD) {
                while (sp->prev && sp->prev->thread == sp->thread) {
                    sp = sp->prev;
                    item_arts += sp->misc;
                }
                line_cnt = count_thread_lines(sp, (int*)nullptr);
            } else
            {
                line_cnt = count_subject_lines(sp, (int *) nullptr);
            }
            if (!(sp->flags & SF_INCLUDED) || !line_cnt)
            {
                continue;
            }
            line_cnt = std::min(line_cnt, s_sel_max_line_cnt);
            line += line_cnt;
            if (line > s_sel_max_line_cnt + 2) {
                sp = page_sp;
                break;
            }
            g_sel_prior_obj_cnt -= item_arts;
            page_sp = sp;
            if (++item_cnt >= s_sel_max_per_page)
            {
                break;
            }
        }
        if (g_sel_page_sp != page_sp) {
            g_sel_page_sp = (page_sp? page_sp : g_first_subject);
            return true;
        }
        break;
      }
    }
    return false;
}

/* Return true if we had to change pages to find the object */
bool calc_page(SEL_UNION u)
{
    int ret = false;
    if (u.op != -1)
    {
        g_sel_item_index = -1;
    }
try_again:
    g_sel_page_obj_cnt = 0;
    g_sel_page_item_cnt = 0;
    g_term_line = 2;

    switch (g_sel_mode) {
      case SM_MULTIRC: {
        MULTIRC* mp = g_sel_page_mp;
        if (mp)
        {
            (void) multirc_ptr(mp->num);
        }
        for (; mp && g_sel_page_item_cnt<s_sel_max_per_page; mp=multirc_next(mp)) {
            if (mp == u.mp)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (mp->flags & MF_INCLUDED)
            {
                g_sel_page_item_cnt++;
            }
        }
        g_sel_page_obj_cnt = g_sel_page_item_cnt;
        g_sel_next_mp = mp;
        break;
      }
      case SM_NEWSGROUP: {
        NGDATA* np = g_sel_page_np;
        for (; np && g_sel_page_item_cnt < s_sel_max_per_page; np = np->next) {
            if (np == u.np)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (np->flags & NF_INCLUDED)
            {
                g_sel_page_item_cnt++;
            }
        }
        g_sel_page_obj_cnt = g_sel_page_item_cnt;
        g_sel_next_np = np;
        break;
      }
      case SM_ADDGROUP: {
        ADDGROUP* gp = g_sel_page_gp;
        for (; gp && g_sel_page_item_cnt < s_sel_max_per_page; gp = gp->next) {
            if (gp == u.gp)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (gp->flags & AGF_INCLUDED)
            {
                g_sel_page_item_cnt++;
            }
        }
        g_sel_page_obj_cnt = g_sel_page_item_cnt;
        g_sel_next_gp = gp;
        break;
      }
      case SM_UNIVERSAL: {
        UNIV_ITEM* ui = sel_page_univ;
        for (; ui && g_sel_page_item_cnt < s_sel_max_per_page; ui = ui->next) {
            if (ui == u.un)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (ui->flags & UF_INCLUDED)
            {
                g_sel_page_item_cnt++;
            }
        }
        g_sel_page_obj_cnt = g_sel_page_item_cnt;
        g_sel_next_univ = ui;
        break;
      }
      case SM_OPTIONS: {
        int op = g_sel_page_op;
        for (; op <= g_obj_count && g_sel_page_item_cnt < s_sel_max_per_page; op++) {
            if (op == u.op)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (g_option_flags[op] & OF_INCLUDED)
            {
                g_sel_page_item_cnt++;
            }
        }
        g_sel_page_obj_cnt = g_sel_page_item_cnt;
        s_sel_next_op = op;
        break;
      }
      case SM_ARTICLE: {
        ARTICLE** app = g_sel_page_app;
        ARTICLE** limit = g_artptr_list + g_artptr_list_size;
        for (; app < limit && g_sel_page_item_cnt < s_sel_max_per_page; app++) {
            if (*app == u.ap)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if ((*app)->flags & AF_INCLUDED)
            {
                g_sel_page_item_cnt++;
            }
        }
        g_sel_page_obj_cnt = g_sel_page_item_cnt;
        g_sel_next_app = app;
        break;
      }
      default: {
        SUBJECT *sp = g_sel_page_sp;
          int      line_cnt;
          int      sel;
          for (; sp && g_sel_page_item_cnt < s_sel_max_per_page; sp = sp->next) {
            if (sp == u.sp)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (sp->flags & SF_INCLUDED) {
                if (g_sel_mode == SM_THREAD)
                {
                    line_cnt = count_thread_lines(sp, &sel);
                }
                else
                {
                    line_cnt = count_subject_lines(sp, &sel);
                }
                if (line_cnt) {
                    line_cnt = std::min(line_cnt, s_sel_max_line_cnt);
                    if (g_term_line + line_cnt > s_sel_max_line_cnt+2)
                    {
                        break;
                    }
                    g_sel_page_obj_cnt += sp->misc;
                    g_sel_page_item_cnt++;
                }
            } else
            {
                line_cnt = 0;
            }
            if (g_sel_mode == SM_THREAD) {
                while (sp->next && sp->next->thread == sp->thread) {
                    sp = sp->next;
                    if (!line_cnt || !sp->misc)
                    {
                        continue;
                    }
                    g_sel_page_obj_cnt += sp->misc;
                }
            }
        }
        g_sel_next_sp = sp;
        break;
      }
    }
    if (u.op != -1 && g_sel_item_index < 0) {
        if (!ret) {
            if (first_page()) {
                ret = true;
                goto try_again;
            }
        }
        if (next_page()) {
            ret = true;
            goto try_again;
        }
        first_page();
        g_sel_item_index = 0;
    }
    return ret;
}

void display_page_title(bool home_only)
{
    if (home_only || (g_erase_screen && g_erase_each_line))
    {
        home_cursor();
    }
    else
    {
        clear();
    }
    if (g_sel_mode == SM_MULTIRC)
    {
        color_string(COLOR_HEADING, "Newsrcs");
    }
    else if (g_sel_mode == SM_OPTIONS)
    {
        color_string(COLOR_HEADING, "Options");
    }
    else if (g_sel_mode == SM_UNIVERSAL) {
        color_object(COLOR_HEADING, true);
        printf("[%d] %s",g_univ_level,
               g_univ_title.empty() ? "Universal Selector" : g_univ_title.c_str());
        color_pop();
    }
    else if (g_in_ng)
    {
        color_string(COLOR_NGNAME, g_ngname.c_str());
    }
    else if (g_sel_mode == SM_ADDGROUP)
    {
        color_string(COLOR_HEADING, "ADD newsgroups");
    }
    else {
        int len;
        NEWSRC* rp;
        color_object(COLOR_HEADING, true);
        printf("Newsgroups");
        for (rp = g_multirc->first, len = 0; rp && len < 34; rp = rp->next) {
            if (rp->flags & RF_ACTIVE) {
                sprintf(g_buf+len, ", %s", rp->datasrc->name);
                len += strlen(g_buf+len);
            }
        }
        if (rp)
        {
            strcpy(g_buf + len, ", ...");
        }
        if (strcmp(g_buf+2,"default") != 0)
        {
            printf(" (group #%d: %s)", g_multirc->num, g_buf + 2);
        }
        color_pop();    /* of COLOR_HEADING */
    }
    if (home_only || (g_erase_screen && g_erase_each_line))
    {
        erase_eol();
    }
    if (g_sel_mode == SM_MULTIRC)
    {
    }
    else if (g_sel_mode == SM_UNIVERSAL)
    {
    }
    else if (g_sel_mode == SM_OPTIONS)
    {
        printf("      (Press 'S' to save changes, 'q' to abandon, or TAB to use.)");
    }
    else if (g_in_ng) {
        printf("          %ld %sarticle%s", (long)g_sel_total_obj_cnt,
               g_sel_rereading? "read " : "",
               g_sel_total_obj_cnt == 1 ? "" : "s");
        if (g_sel_exclusive)
        {
            printf(" out of %ld", (long) g_obj_count);
        }
        fputs(g_moderated.c_str(),stdout);
    }
    else {
        printf("       %s%ld group%s",s_group_init_done? "" : "~",
            (long)g_sel_total_obj_cnt, plural(g_sel_total_obj_cnt));
        if (g_sel_exclusive)
        {
            printf(" out of %ld", (long) g_obj_count);
        }
        if (g_maxngtodo)
        {
            printf(" (Restriction)");
        }
    }
    home_cursor();
    newline();
    maybe_eol();
    if (g_in_ng && g_redirected && !g_redirected_to.empty())
    {
        printf("\t** Please start using %s **", g_redirected_to.c_str());
    }
    newline();
}

void display_page()
{
    int sel;

    display_page_title(false);

try_again:
    sel_page_init();

    if (!g_sel_total_obj_cnt)
    {
    }
    else if (g_sel_mode == SM_MULTIRC) {
        MULTIRC* mp = g_sel_page_mp;
        NEWSRC* rp;
        int len;
        if (mp)
        {
            (void) multirc_ptr(mp->num);
        }
        for (; mp && g_sel_page_item_cnt<s_sel_max_per_page; mp=multirc_next(mp)) {
#if 0
            if (mp == g_multirc)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
#endif

            if (!(mp->flags & MF_INCLUDED))
            {
                continue;
            }

            // multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
            TRN_ASSERT((g_sel_mask & (AGF_DEL | AGF_DELSEL)) == 0);
            sel = !!(mp->flags & static_cast<multirc_flags>(g_sel_mask));
            g_sel_items[g_sel_page_item_cnt].u.mp = mp;
            g_sel_items[g_sel_page_item_cnt].line = g_term_line;
            g_sel_items[g_sel_page_item_cnt].sel = sel;
            g_sel_page_obj_cnt++;

            maybe_eol();
            for (rp = mp->first, len = 0; rp && len < 34; rp = rp->next) {
                sprintf(g_buf+len, ", %s", rp->datasrc->name);
                len += strlen(g_buf+len);
            }
            if (rp)
            {
                strcpy(g_buf + len, ", ...");
            }
            output_sel(g_sel_page_item_cnt, sel, false);
            printf("%5d %s\n", mp->num, g_buf+2);
            termdown(1);
            g_sel_page_item_cnt++;
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        g_sel_next_mp = mp;
    }
    else if (g_sel_mode == SM_NEWSGROUP) {
        NGDATA* np;
        int max_len = 0;
        int outputting = (*g_sel_grp_dmode != 'l');
      start_of_loop:
        for (np = g_sel_page_np; np; np = np->next) {
            if (np == g_ngptr)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }

            if (!(np->flags & NF_INCLUDED))
            {
                continue;
            }

            if (!np->abs1st) {
                set_toread(np, ST_LAX);
                if (g_paranoid) {
                    newline();
                    g_current_ng = np;
                    g_ngptr = np;
                    /* this may move newsgroups around */
                    cleanup_newsrc(np->rc);
                    init_pages(PRESERVE_PAGE);
                    display_page_title(false);
                    goto try_again;
                }
                if (g_sel_rereading? (np->toread != TR_NONE)
                                 : (np->toread < g_ng_min_toread)) {
                    np->flags &= ~NF_INCLUDED;
                    g_sel_total_obj_cnt--;
                    g_newsgroup_toread--;
                    g_missing_count++;
                    continue;
                }
            }

            if (g_sel_page_item_cnt >= s_sel_max_per_page)
            {
                break;
            }

            if (outputting) {
                sel = !!(np->flags & g_sel_mask) + (np->flags & NF_DEL);
                g_sel_items[g_sel_page_item_cnt].u.np = np;
                g_sel_items[g_sel_page_item_cnt].line = g_term_line;
                g_sel_items[g_sel_page_item_cnt].sel = sel;
                g_sel_page_obj_cnt++;

                maybe_eol();
                output_sel(g_sel_page_item_cnt, sel, false);
                printf("%5ld ", (long)np->toread);
                display_group(np->rc->datasrc,np->rcline,np->numoffset-1,max_len);
            }
            else if (np->numoffset >= max_len)
            {
                max_len = np->numoffset + 1;
            }
            g_sel_page_item_cnt++;
        }
        if (!outputting) {
            outputting = 1;
            sel_page_init();
            goto start_of_loop;
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        g_sel_next_np = np;
        if (!s_group_init_done) {
            for (; np; np = np->next) {
                if (!np->abs1st)
                {
                    break;
                }
            }
            if (!np) {
                int line = g_term_line;
                s_group_init_done = true;
                display_page_title(true);
                goto_xy(0,line);
            }
        }
    }
    else if (g_sel_mode == SM_ADDGROUP) {
        ADDGROUP* gp = g_sel_page_gp;
        int max_len = 0;
        if (*g_sel_grp_dmode == 'l') {
            int i = 0;
            for (; gp && i < s_sel_max_per_page; gp = gp->next) {
                if (!(gp->flags & AGF_INCLUDED))
                {
                    continue;
                }
                int len = strlen(gp->name) + 2;
                max_len = std::max(len, max_len);
                i++;
            }
            gp = g_sel_page_gp;
        }
        for (; gp && g_sel_page_item_cnt < s_sel_max_per_page; gp = gp->next) {
#if 0
            if (gp == xx)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
#endif

            if (!(gp->flags & AGF_INCLUDED))
            {
                continue;
            }

            sel = !!(gp->flags & g_sel_mask) + (gp->flags & AGF_DEL);
            g_sel_items[g_sel_page_item_cnt].u.gp = gp;
            g_sel_items[g_sel_page_item_cnt].line = g_term_line;
            g_sel_items[g_sel_page_item_cnt].sel = sel;
            g_sel_page_obj_cnt++;

            maybe_eol();
            output_sel(g_sel_page_item_cnt, sel, false);
            printf("%5ld ", (long)gp->toread);
            display_group(gp->datasrc, gp->name, strlen(gp->name), max_len);
            g_sel_page_item_cnt++;
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        g_sel_next_gp = gp;
    }
    else if (g_sel_mode == SM_UNIVERSAL) {
        UNIV_ITEM* ui = sel_page_univ;
        for (; ui && g_sel_page_item_cnt < s_sel_max_per_page; ui = ui->next) {
#if 0
            if (ui == xx)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
#endif

            if (!(ui->flags & UF_INCLUDED))
            {
                continue;
            }

            sel = !!(ui->flags & static_cast<univitem_flags>(g_sel_mask)) + (ui->flags & UF_DEL);
            g_sel_items[g_sel_page_item_cnt].u.un = ui;
            g_sel_items[g_sel_page_item_cnt].line = g_term_line;
            g_sel_items[g_sel_page_item_cnt].sel = sel;
            g_sel_page_obj_cnt++;

            maybe_eol();
            output_sel(g_sel_page_item_cnt, sel, false);
            putchar(' ');
            display_univ(ui);
            g_sel_page_item_cnt++;
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        g_sel_next_univ = ui;
    }
    else if (g_sel_mode == SM_OPTIONS) {
        int op = g_sel_page_op;
        for (; op <= g_obj_count && g_sel_page_item_cnt<s_sel_max_per_page; op++) {
#if 0
            if (op == xx)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
#endif

            if (!(g_option_flags[op] & OF_INCLUDED))
            {
                continue;
            }

            if (*g_options_ini[op].item == '*')
            {
                sel = !!(g_option_flags[op] & OF_SEL);
            }
            else
            {
                sel = ini_values(g_options_ini)[op]? 1 :
                          (g_option_saved_vals[op]? 3 :
                               (g_option_def_vals[op]? 0 : 2));
            }
            g_sel_items[g_sel_page_item_cnt].u.op = static_cast<option_index>(op);
            g_sel_items[g_sel_page_item_cnt].line = g_term_line;
            g_sel_items[g_sel_page_item_cnt].sel = sel;
            g_sel_page_obj_cnt++;

            maybe_eol();
            display_option(op,g_sel_page_item_cnt);
            g_sel_page_item_cnt++;
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        s_sel_next_op = op;
    }
    else if (g_sel_mode == SM_ARTICLE) {
        ARTICLE **limit = g_artptr_list + g_artptr_list_size;
        ARTICLE **app = g_sel_page_app;
        for (; app < limit && g_sel_page_item_cnt < s_sel_max_per_page; app++) {
            ARTICLE *ap = *app;
            if (ap == g_sel_last_ap)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }
            if (!(ap->flags & AF_INCLUDED))
            {
                continue;
            }
            sel = !!(ap->flags & g_sel_mask) + (ap->flags & AF_DEL);
            g_sel_items[g_sel_page_item_cnt].u.ap = ap;
            g_sel_items[g_sel_page_item_cnt].line = g_term_line;
            g_sel_items[g_sel_page_item_cnt].sel = sel;
            g_sel_page_obj_cnt++;
            /* Output the article, with optional author */
            display_article(ap, g_sel_page_item_cnt, sel);
            g_sel_page_item_cnt++;
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        g_sel_next_app = app;
    }
    else {
        int line_cnt;
        int etc = 0;
        int ix = -1;    /* item # or -1 */

        SUBJECT *sp = g_sel_page_sp;
        for (; sp && !etc && g_sel_page_item_cnt<s_sel_max_per_page; sp=sp->next) {
            if (sp == g_sel_last_sp)
            {
                g_sel_item_index = g_sel_page_item_cnt;
            }

            if (sp->flags & SF_INCLUDED) {
                /* Compute how many lines we need to display this group */
                if (g_sel_mode == SM_THREAD)
                {
                    line_cnt = count_thread_lines(sp, &sel);
                }
                else
                {
                    line_cnt = count_subject_lines(sp, &sel);
                }
                if (line_cnt) {
                    /* If this item is too long to fit on the screen all by
                    ** itself, trim it to fit and set the "etc" flag.
                    */
                    if (line_cnt > s_sel_max_line_cnt) {
                        etc = line_cnt;
                        line_cnt = s_sel_max_line_cnt;
                    }
                    /* If it doesn't fit, save it for the next page */
                    if (g_term_line + line_cnt > s_sel_max_line_cnt + 2)
                    {
                        break;
                    }
                    g_sel_items[g_sel_page_item_cnt].u.sp = sp;
                    g_sel_items[g_sel_page_item_cnt].line = g_term_line;
                    g_sel_items[g_sel_page_item_cnt].sel = sel;
                    g_sel_page_obj_cnt += sp->misc;

                    ix = g_sel_page_item_cnt;
                    sel = g_sel_items[g_sel_page_item_cnt].sel;
                    g_sel_page_item_cnt++;
                    if (sp->misc) {
                        display_subject(sp, ix, sel);
                        ix = -1;
                    }
                }
            } else
            {
                line_cnt = 0;
            }
            if (g_sel_mode == SM_THREAD) {
                while (sp->next && sp->next->thread == sp->thread) {
                    sp = sp->next;
                    if (!line_cnt || !sp->misc)
                    {
                        continue;
                    }
                    if (g_term_line < s_sel_max_line_cnt + 2)
                    {
                        display_subject(sp, ix, sel);
                    }
                    ix = -1;
                    g_sel_page_obj_cnt += sp->misc;
                }
            }
            if (etc)
            {
                printf("     ... etc. [%d lines total]", etc);
            }
        }
        if (!g_sel_page_obj_cnt) {
            if (last_page())
            {
                goto try_again;
            }
        }
        g_sel_next_sp = sp;
    }
    g_sel_last_ap = nullptr;
    g_sel_last_sp = nullptr;
    g_sel_at_end = (g_sel_prior_obj_cnt + g_sel_page_obj_cnt == g_sel_total_obj_cnt);
    maybe_eol();
    newline();
    g_sel_last_line = g_term_line;
}

void update_page()
{
    int sel;
    for (int j = 0; j < g_sel_page_item_cnt; j++) {
        SEL_UNION u = g_sel_items[j].u;
        switch (g_sel_mode) {
          case SM_MULTIRC:
            // multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
            TRN_ASSERT((g_sel_mask & (AGF_DEL | AGF_DELSEL)) == 0);
            sel = !!(u.mp->flags & static_cast<multirc_flags>(g_sel_mask));
            break;
          case SM_NEWSGROUP:
            sel = !!(u.np->flags & g_sel_mask) + (u.np->flags & NF_DEL);
            break;
          case SM_ADDGROUP:
            sel = !!(u.gp->flags & g_sel_mask) + (u.gp->flags & AGF_DEL);
            break;
          case SM_UNIVERSAL:
            sel = !!(u.un->flags & g_sel_mask) + (u.un->flags & UF_DEL);
            break;
          case SM_OPTIONS:
            if (*g_options_ini[u.op].item == '*')
            {
                sel = !!(g_option_flags[u.op] & OF_SEL);
            }
            else
            {
                sel = ini_value(g_options_ini, u.op)? 1 :
                          (g_option_saved_vals[u.op]? 3 :
                               (g_option_def_vals[u.op]? 0 : 2));
            }
            break;
          case SM_ARTICLE:
            sel = !!(u.ap->flags & g_sel_mask) + (u.ap->flags & AF_DEL);
            break;
          case SM_THREAD:
            (void) count_thread_lines(u.sp, &sel);
            break;
          default:
            (void) count_subject_lines(u.sp, &sel);
            break;
        }
        if (sel == g_sel_items[j].sel)
        {
            continue;
        }
        goto_xy(0,g_sel_items[j].line);
        g_sel_item_index = j;
        output_sel(g_sel_item_index, sel, true);
    }
    if (++g_sel_item_index == g_sel_page_item_cnt)
    {
        g_sel_item_index = 0;
    }
}

void output_sel(int ix, int sel, bool update)
{
    if (ix < 0) {
        if (g_use_sel_num)
        {
            putchar(' ');
        }
        putchar(' ');
        putchar(' ');
        return;
    }

    if (g_use_sel_num) {
        /* later consider no-leading-zero option */
        printf("%02d",ix+1);
    } else
    {
        putchar(g_sel_chars[ix]);
    }

    switch (sel) {
      case 1:                   /* '+' */
        color_object(COLOR_PLUS, true);
        break;
      case 2:                   /* '-' */
        color_object(COLOR_MINUS, true);
        break;
      case 3:                   /* '*' */
        color_object(COLOR_STAR, true);
        break;
      default:
        color_object(COLOR_DEFAULT, true);
        sel = 0;
        break;
    }
    putchar(" +-*"[sel]);
    color_pop();        /* of COLOR_PLUS/MINUS/STAR/DEFAULT */

    if (update)
    {
        g_sel_items[ix].sel = sel;
    }
}

/* Counts the number of lines needed to output a subject, including
** optional authors.
*/
static int count_subject_lines(const SUBJECT *subj, int *selptr)
{
    int sel;

    if (subj->flags & SF_DEL)
    {
        sel = 2;
    }
    else if (subj->flags & g_sel_mask) {
        sel = 1;
        for (ARTICLE *ap = subj->articles; ap; ap = ap->subj_next) {
            if ((!!(ap->flags&AF_UNREAD) ^ g_sel_rereading)
              && !(ap->flags & g_sel_mask)) {
                sel = 3;
                break;
            }
        }
    } else
    {
        sel = 0;
    }
    if (selptr)
    {
        *selptr = sel;
    }
    if (*g_sel_art_dmode == 'l')
    {
        return subj->misc;
    }
    if (*g_sel_art_dmode == 'm')
    {
        return (subj->misc <= 4 ? subj->misc : (subj->misc - 4) / 3 + 4);
    }
    return (subj->misc != 0);
}

/* Counts the number of lines needed to output a thread, including
** optional authors.
*/
static int count_thread_lines(const SUBJECT *subj, int *selptr)
{
    int total = 0;
    const ARTICLE *thread = subj->thread;
    int            sel = -1;
    int            subj_sel;

    do {
        if (subj->misc) {
            total += count_subject_lines(subj, &subj_sel);
            if (sel < 0)
            {
                sel = subj_sel;
            }
            else if (sel != subj_sel)
            {
                sel = 3;
            }
        }
    } while ((subj = subj->next) != nullptr && subj->thread == thread);
    if (selptr)
    {
        *selptr = (sel < 0 ? 0 : sel);
    }
    return total;
}

/* Display an article, perhaps with its author.
*/
static void display_article(const ARTICLE *ap, int ix, int sel)
{
    int subj_width = g_tc_COLS - 5 - g_use_sel_num;
    int from_width = g_tc_COLS / 5;
    int date_width = g_tc_COLS / 5;

    maybe_eol();
    subj_width = std::max(subj_width, 32);

    output_sel(ix, sel, false);
    if (*g_sel_art_dmode == 's' || from_width < 8)
    {
        printf("  %s\n", compress_subj(ap->subj->articles, subj_width));
    }
    else if (*g_sel_art_dmode == 'd') {
          printf("%s  %s\n",
               compress_date(ap, date_width),
               compress_subj(ap, subj_width - date_width));
    } else {
        printf("%s  %s\n",
               compress_from(ap->from, from_width),
               compress_subj(ap, subj_width - from_width));
    }
    termdown(1);
}

/* Display the given subject group, with optional authors.
*/
static void display_subject(const SUBJECT *subj, int ix, int sel)
{
    ARTICLE*ap;
    int     subj_width = g_tc_COLS - 8 - g_use_sel_num;
    int     from_width = g_tc_COLS / 5;
    int     date_width = g_tc_COLS / 5;
#ifdef USE_UTF_HACK
    utf_init(CHARSET_NAME_UTF8, CHARSET_NAME_UTF8); /* FIXME */
#endif

    maybe_eol();
    subj_width = std::max(subj_width, 32);

    int j = subj->misc;

    output_sel(ix, sel, false);
    if (*g_sel_art_dmode == 's' || from_width < 8) {
        printf("%3d  %s\n",j,compress_subj(subj->articles,subj_width));
        termdown(1);
    }
    else {
        ARTICLE* first_ap;
        /* Find the first unread article so we get the author right */
        if ((first_ap = subj->thread) != nullptr
         && (first_ap->subj != subj || first_ap->from == nullptr
          || (!(first_ap->flags&AF_UNREAD) ^ g_sel_rereading)))
        {
            first_ap = nullptr;
        }
        for (ap = subj->articles; ap; ap = ap->subj_next) {
            if (!!(ap->flags&AF_UNREAD) ^ g_sel_rereading)
            {
                break;
            }
        }
        if (!first_ap)
        {
            first_ap = ap;
        }
        if (*g_sel_art_dmode == 'd') {
            printf("%s%3d  %s\n",
                   compress_date(first_ap, date_width), j,
                   compress_subj(first_ap, subj_width - date_width));
        } else {
            printf("%s%3d  %s\n",
                   compress_from(first_ap? first_ap->from : nullptr, from_width), j,
                   compress_subj(first_ap, subj_width - from_width));
        }
        termdown(1);
        int i = -1;
        if (*g_sel_art_dmode != 'd' && --j && ap) {
            for ( ; ap && j; ap = ap->subj_next) {
                if ((!(ap->flags&AF_UNREAD) ^ g_sel_rereading)
                 || ap == first_ap)
                {
                    continue;
                }
                j--;
                if (i < 0)
                {
                    i = 0;
                }
                else if (*g_sel_art_dmode == 'm') {
                    if (!j) {
                        if (i)
                        {
                            newline();
                        }
                    }
                    else {
                        if (i == 3 || !i) {
                            if (i)
                            {
                                newline();
                            }
                            if (g_term_line >= s_sel_max_line_cnt + 2)
                            {
                                return;
                            }
                            maybe_eol();
                            i = 1;
                        } else
                        {
                            i++;
                        }
                        if (g_use_sel_num)
                        {
                            putchar(' ');
                        }
                        printf("  %s      ",
                               compress_from(ap? ap->from : nullptr, from_width));
                        continue;
                    }
                }
                if (g_term_line >= s_sel_max_line_cnt + 2)
                {
                    return;
                }
                maybe_eol();
                if (g_use_sel_num)
                {
                    putchar(' ');
                }
                printf("  %s\n", compress_from(ap? ap->from : nullptr, from_width));
                termdown(1);
            }
        }
    }
}

void display_option(int op, int item_index)
{
    int len;
    const char *pre;
    const char *item;
    const char *post;
    const char *val;
    if (*g_options_ini[op].item == '*') {
        len = strlen(g_options_ini[op].item+1);
        pre = "==";
        item = g_options_ini[op].item+1;
        post = "==================================";
        val = "";
    }
    else {
        len = (g_options_ini[op].checksum & 0xff);
        pre = "  ";
        item = g_options_ini[op].item;
        post = "..................................";
        val = ini_values(g_options_ini)[op];
        if (!val)
        {
            val = quote_string(option_value(static_cast<option_index>(op)));
        }
    }
    output_sel(item_index, g_sel_items[item_index].sel, false);
    printf(" %s%s%s %.39s\n", pre, item, post + len, val);
    termdown(1);
}

static void display_univ(const UNIV_ITEM *ui)
{
    if (!ui) {
        fputs("****EMPTY****",stdout);
    } else {
        switch(ui->type) {
          case UN_NEWSGROUP: {
              /* later error check the UI? */
            NGDATA *np = find_ng(ui->data.group.ng);
            if (!np)
            {
                printf("!!!!! could not find %s", ui->data.group.ng);
            }
            else {
                /* XXX set_toread() can print sometimes... */
                if (!np->abs1st)
                {
                    set_toread(np, ST_LAX);
                }
                int numarts = np->toread;
                if (numarts >= 0)
                {
                    printf("%5ld ", (long) numarts);
                }
                else if (numarts == TR_UNSUB)
                {
                    printf("UNSUB ");
                }
                else
                {
                    printf("***** ");
                }
                fputs(ui->data.group.ng,stdout);
            }
            newline();
            break;
          }
          case UN_ARTICLE:
            printf("      %s",ui->desc? ui->desc : univ_article_desc(ui));
            newline();
            break;
          case UN_HELPKEY:
            printf("      Help on the %s",univ_keyhelp_modestr(ui));
            newline();
            break;
          default:
            printf("      %s",ui->desc? ui->desc : "[No Description]");
            newline();
            break;
        }
    }
}

static void display_group(DATASRC *dp, char *group, int len, int max_len)
{
    if (*g_sel_grp_dmode == 's')
    {
        fputs(group, stdout);
    }
    else {
        char* end;
        char buff[256];
        strcpy(buff, find_grpdesc(dp, group));
        char* cp = buff;
        if (*cp != '?' && (end = strchr(cp, '\n')) != nullptr
         && end != cp) {
            if (end - cp > g_tc_COLS - max_len - 8 - 1 - g_use_sel_num)
            {
                end = cp + g_tc_COLS - max_len - 8 - 1 - g_use_sel_num;
            }
            char ch = *end;
            *end = '\0';
            if (*g_sel_grp_dmode == 'm')
            {
                fputs(cp, stdout);
            }
            else {
                int i = max_len - len;
                fputs(group, stdout);
                do {
                    putchar(' ');
                } while (--i > 0);
                fputs(cp, stdout);
            }
            *end = ch;
        }
        else
        {
            fputs(group, stdout);
        }
    }
    newline();
}
