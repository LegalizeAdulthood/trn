/* rt-page.cpp
 * vi: set sw=4 ts=8 ai sm noet :
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/addng.h>
#include <trn/cache.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/ngdata.h>
#include <trn/only.h>
#include <trn/opt.h>
#include <trn/OptionCatalog.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/utf.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

SelectorPage  g_sel_page;
std::string   g_sel_grp_display_mode{"slm"};
std::string   g_sel_art_display_mode{"lmds"};
std::size_t   g_sel_grp_display_mode_index{};
std::size_t   g_sel_art_display_mode_index{};

static bool              s_group_init_done{true};

char current_group_display_mode()
{
    return g_sel_grp_display_mode.empty() ? '\0' : g_sel_grp_display_mode[g_sel_grp_display_mode_index];
}

char current_article_display_mode()
{
    return g_sel_art_display_mode.empty() ? '\0' : g_sel_art_display_mode[g_sel_art_display_mode_index];
}

static int  count_subject_lines(const Subject *subj, int *selptr);
static int  count_thread_lines(const Subject *subj, int *selptr);
static void display_page_title(bool home_only);
static void display_article(const Article *ap, int ix, int sel);
static void display_subject(const Subject *subj, int ix, int sel);
static void display_universal(const UniversalItem *ui);
static void display_group(DataSource *dp, std::string_view group, int max_len);

/// @brief Sets the selector mode based on the provided character.
///
/// This function changes the selector mode to article, subject, or thread mode
/// depending on the input character. For thread mode, it may initialize threading
/// if needed. The default selector mode is updated accordingly. Returns true if
/// a valid mode was set, false otherwise.
///
/// @param ch The character indicating the desired selector mode ('a', 's', 't', or 'T').
/// @return true if the mode was set successfully, false otherwise.
///
/// @globals
/// - g_sel.default_mode: Set to the selected mode.
/// - g_threaded_group: May be set to true or false when switching to thread mode.
/// - g_thread_always: Temporarily set during thread initialization.
/// - g_first_art: Set to g_abs_first if rereading in thread mode.
///
bool set_sel_mode(char_int ch)
{
    switch (ch)
    {
    case 'a':
        g_sel.set_selector(g_sel.default_mode = SM_ARTICLE, SS_MAGIC_NUMBER);
        break;

    case 's':
        g_sel.set_selector(g_sel.default_mode = SM_SUBJECT, SS_MAGIC_NUMBER);
        break;

    case 't':
        if (g_in_ng && !g_threaded_group)
        {
            bool always_save = g_thread_always;
            g_threaded_group = true;
            g_thread_always = true;
            if (g_sel.rereading)
            {
                g_first_art = g_abs_first;
            }
            fmt::print("\nThreading the group. ");
            std::fflush(stdout);
            term_down(1);
            thread_open();
            g_thread_always = always_save;
            if (g_last_cached < g_last_art)
            {
                g_threaded_group = false;
            }
        }
        // FALL THROUGH

    case 'T':
        g_sel.set_selector(g_sel.default_mode = SM_THREAD, SS_MAGIC_NUMBER);
        break;

    default:
        g_sel.set_selector(g_sel.default_mode, SS_MAGIC_NUMBER);
        return false;
    }
    return true;
}

/// @brief Returns a string describing the current sort order.
///
/// This function temporarily switches to the specified selector mode,
/// formats a string describing the current sort order (including
/// direction), restores the previous mode, and returns the result.
///
/// @param smode The selector mode to describe.
/// @return String containing the sort order description.
///
/// @globals
/// - g_threaded_group: Read by set_selector to normalize thread-mode availability.
///
std::string SelectorPolicy::get_order(SelectionMode smode)
{
    SelectionMode save_sel_mode = mode;
    set_selector(smode, SS_MAGIC_NUMBER);
    std::string order = direction < 0 ? "reverse " : "";
    order += sort_string;
    mode = save_sel_mode;
    set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER);
    return order;
}

/// @brief Sets the selection sort order for a given selection mode based on a string input.
/// 
/// If the string begins with 'r' or 'R', the sort order is set to reverse. The function
/// then determines the sort key character and adjusts its case to indicate direction.
/// The sort order is applied by calling SelectorPolicy::set_sort with the appropriate parameters.
///
/// @param smode The selection mode to set the sort order for.
/// @param str   The string specifying the sort order and direction.
/// @return      True if the sort order was set successfully, false otherwise.
///
/// @globals
/// - g_threaded_group: Read by set_selector through set_sort for thread-mode availability.
///
bool SelectorPolicy::set_order(SelectionMode smode, std::string_view str)
{
    bool reverse = false;

    if (!str.empty() && (str.front() == 'r' || str.front() == 'R'))
    {
        const std::size_t first_space = str.find(' ');
        str = first_space == std::string_view::npos ? std::string_view{} : str.substr(first_space);
        while (!str.empty() && str.front() == ' ')
        {
            str.remove_prefix(1);
        }
        reverse = true;
    }

    char ch = str.empty() ? '\0' : str.front();
    if (reverse)
    {
        ch = std::islower(ch) ? std::toupper(ch) : ch;
    }
    else
    {
        ch = std::isupper(ch) ? std::tolower(ch) : ch;
    }

    return set_sort(smode, ch);
}

/// @brief Sets the selection sort mode for a given selection mode and sort key character.
/// 
/// Determines the sort mode based on the provided character, sets the sort direction
/// according to the character's case, and applies the sort order by calling set_selector.
/// If the selection mode is changed during the process, it is restored to its previous value.
/// 
/// @param smode The selection mode to set the sort order for.
/// @param ch    The character specifying the sort key and direction.
/// @return      True if the sort order was set successfully, false otherwise.
/// 
/// @globals
/// - g_threaded_group: Read by set_selector to normalize thread-mode availability.
///
bool SelectorPolicy::set_sort(SelectionMode smode, char_int ch)
{
    SelectionMode     save_sel_mode = mode;
    SelectionSortMode ssort;

    switch (ch)
    {
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

    mode = smode;
    if (std::isupper(ch))
    {
        set_selector(SM_MAGIC_NUMBER, static_cast<SelectionSortMode>(-ssort));
    }
    else
    {
        set_selector(SM_MAGIC_NUMBER, ssort);
    }

    if (mode != save_sel_mode)
    {
        mode = save_sel_mode;
        set_selector(SM_MAGIC_NUMBER, SS_MAGIC_NUMBER);
    }
    return true;
}

/// @brief Sets the current selection mode and sort order for the selector.
/// 
/// Updates this policy's selection mode and sort order from the provided parameters.
/// If the sort mode is not specified, it uses the default for the current selection mode.
/// Also updates the sort direction, labels, and remembered mode-specific orders.
/// 
/// @param smode The selection mode to set.
/// @param ssort The sort mode to set. If zero, the default for the mode is used.
/// 
/// @globals
/// - g_threaded_group: Read to reject unavailable thread mode.
///
void SelectorPolicy::set_selector(SelectionMode smode, SelectionSortMode ssort)
{
    if (smode == SM_MAGIC_NUMBER)
    {
        if (mode == SM_SUBJECT)
        {
            mode = thread_mode;
        }
        smode = mode;
    }
    else
    {
        mode = smode;
    }
    if (!ssort)
    {
        switch (mode)
        {
        case SM_MULTIRC:
            ssort = SS_NATURAL;
            break;

        case SM_ADD_GROUP:
            ssort = add_group_sort;
            break;

        case SM_NEWSGROUP:
            ssort = newsgroup_sort;
            break;

        case SM_OPTIONS:
            ssort = SS_NATURAL;
            break;

        case SM_THREAD:
        case SM_SUBJECT:
            ssort = thread_sort;
            break;

        case SM_ARTICLE:
            ssort = art_sort;
            break;

        case SM_UNIVERSAL:
            ssort = universal_sort;
            break;
        }
    }
    if (ssort > SS_MAGIC_NUMBER)
    {
        direction = 1;
        sort = ssort;
    }
    else
    {
        direction = -1;
        sort = static_cast<SelectionSortMode>(-ssort);
    }

    if (mode == SM_THREAD && !g_threaded_group)
    {
        mode = SM_SUBJECT;
    }

    switch (mode)
    {
    case SM_MULTIRC:
        mode_string = "a newsrc group";
        break;

    case SM_ADD_GROUP:
        mode_string = group_listing ? "unsubscribed newsgroups" : "a newsgroup to add";
        add_group_sort = ssort;
        break;

    case SM_NEWSGROUP:
        mode_string = "a newsgroup";
        newsgroup_sort = ssort;
        break;

    case SM_OPTIONS:
        mode_string = "an option to change";
        break;

    case SM_UNIVERSAL:
        mode_string = "an item";
        universal_sort = ssort;
        break;

    case SM_THREAD:
        mode_string = "threads";
        thread_mode = smode;
        thread_sort = ssort;
        goto thread_subj_sort;

    case SM_SUBJECT:
        mode_string = "subjects";
        thread_mode = smode;
        thread_sort = ssort;
thread_subj_sort:
        if (sort == SS_AUTHOR || sort == SS_GROUPS || sort == SS_NATURAL)
          {
            sort = SS_DATE;
          }
        break;

    case SM_ARTICLE:
        mode_string = "articles";
        art_sort = ssort;
        if (sort == SS_COUNT)
        {
            sort = SS_DATE;
        }
        break;
    }

    switch (sort)
    {
    case SS_DATE:
        sort_string = "date";
        break;

    case SS_STRING:
        sort_string = mode == SM_ADD_GROUP || mode == SM_NEWSGROUP ? "group name" : "subject";
        break;

    case SS_AUTHOR:
        sort_string = "author";
        break;

    case SS_COUNT:
        sort_string = "count";
        break;

    case SS_LINES:
        sort_string = "lines";
        break;

    case SS_NATURAL:
        sort_string = "natural";
        break;

    case SS_GROUPS:
        if (mode == SM_NEWSGROUP)
        {
            sort_string = "group name";
        }
        else
        {
            sort_string = "SubjDate";
        }
        break;

    case SS_SCORE:
        sort_string = "points";
        break;
    }
}

/// @brief Initializes selection page parameters.
/// 
/// This function calculates and sets the maximum number of lines (`max_line_cnt`) and the maximum
/// number of selectable items per page (`max_per_page`) based on terminal dimensions and selection
/// mode. It also initializes the selection character set and resets the current page object and item counts.
/// 
/// @globals
/// - g_tc_LINES, g_tc_COLS, g_mouse_bar_width: Read for terminal dimensions.
/// - g_sel.group_listing: Read to exclude the group-listing command character.
/// - g_use_sel_num: Read to choose numeric or character-based capacity.
/// @note Reads SELECTCHARS for the selection character set.
///
void SelectorPage::init()
{
    max_line_cnt = g_tc_LINES - (g_tc_COLS - g_mouse_bar_width < 50 ? 6 : 5);
    chars = get_env_var("SELECTCHARS", SELECTION_CHARS);
    if (g_sel.group_listing)
    {
        chars.erase(std::remove(chars.begin(), chars.end(), 'l'), chars.end());
        if (chars.empty())
        {
            chars = "1234567890";
        }
    }
    // The numeric option of up to 99 lines will require many adaptations
    // to be able to switch from a large numeric page (more than
    // chars.size() lines) to an alphanumeric page. XXX
    //
    if (g_use_sel_num)
    {
        max_per_page = 99;
    }
    else
    {
        max_per_page = static_cast<int>(chars.size());
    }
    max_per_page = std::min(max_per_page, static_cast<int>(MAX_SEL));
    max_per_page = std::min(max_per_page, max_line_cnt);
    page_obj_cnt = 0;
    page_item_cnt = 0;
}

/// @brief Initializes and populates the selection pages for the current
/// selection mode.
///
/// This function resets and recalculates the set of selectable objects and
/// their inclusion flags for the current selection mode (such as
/// newsgroups, articles, options, etc.). It determines which objects are
/// included on the current page, updates counters, and manages page
/// navigation as needed. The function may loop to ensure at least one
/// selectable object is present, and handles special cases for exclusive
/// selection and rereading modes.
///
/// @globals
/// - g_sel_page.prior_obj_cnt: Reset to 0 and updated to count objects before the current page.
/// - g_sel_page.total_obj_cnt: Reset to 0 and incremented for each included object.
/// - g_sel.exclusive: May be set to false if no objects are included in exclusive mode.
/// - g_sel_page.page_np: Set to the current newsgroup page pointer or reset to nullptr.
/// - g_sel_page.page_mp: Set to the current multirc page pointer or reset to nullptr.
/// - g_sel_page.page_gp: Set to the current add group page index or reset.
/// - g_sel_page.page_univ_index: Set to the current universal item page index or reset to 0.
/// - g_sel_page.page_op: Set to the current option page index.
/// - g_sel_page.page_app: Set to the current article page pointer or reset to nullptr.
/// - g_sel_page.page_sp: Set to the current subject page pointer or reset to nullptr.
/// - s_group_init_done: Set to true or false to indicate group initialization state.
/// - g_selected_count: Reset to 0 and incremented for each selected object (newsgroups).
/// - g_obj_count: Reset to 0 and incremented for each object considered.
///
void init_pages(bool fill_last_page)
{
    Selection no_search;
    no_search.op = OI_NONE;
    g_sel_page.init();
try_again:
    g_sel_page.prior_obj_cnt = 0;
    g_sel_page.total_obj_cnt = 0;

    switch (g_sel.mode)
    {
    case SM_MULTIRC:
    {
        for (Multirc *mp = multirc_low(); mp; mp = multirc_next(mp))
        {
            if (mp->m_first)
            {
                mp->m_flags |= MF_INCLUDED;
                g_sel_page.total_obj_cnt++;
            }
            else
            {
                mp->m_flags &= ~MF_INCLUDED;
            }
        }
        if (g_sel_page.page_mrc == nullptr)
        {
            (void) g_sel_page.first_page();
        }
        break;
    }

    case SM_NEWSGROUP:
    {
        bool save_the_rest = false;
        s_group_init_done = true;
        g_ng_order.sort();
        g_selected_count = 0;
        g_obj_count = ArticleNum{};
        for (NewsgroupData *np = g_ng_order.first(); np; np = newsgroup_next(np))
        {
            if (g_sel_page.page_ngd == np)
            {
                g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
            }
            np->m_flags &= ~NF_INCLUDED;
            if (np->m_to_read < TR_NONE)
            {
                continue;
            }
            if (!g_only.in_list(np->rc_line()))
            {
                continue;
            }
            if (np->m_abs_first)
            {
            }
            else if (save_the_rest)
            {
                s_group_init_done = false;
                np->m_to_read = !g_sel.rereading;
            }
            else
            {
                // g_ngptr = np; ??
                // set_ngname(np->rcline);
                np->set_to_read(ST_LAX);
                if (!np->m_rc->data_source->m_act_sf.m_fp)
                {
                    save_the_rest = (g_sel.rereading ^ (np->m_to_read > TR_NONE));
                }
            }
            if (g_paranoid)
            {
                g_current_newsgroup = np;
                g_newsgroup_ptr = np;
                // this may move newsgroups around
                cleanup_newsrc(np->m_rc);
                goto try_again;
            }
            if (!(np->m_flags & g_sel.mask) &&
                (g_sel.rereading ? np->m_to_read != TR_NONE : np->m_to_read < g_newsgroup_min_to_read))
            {
                continue;
            }
            ++g_obj_count;

            if (!g_sel.exclusive || (np->m_flags & g_sel.mask))
            {
                if (np->m_flags & g_sel.mask)
                {
                    g_selected_count++;
                }
                else if (g_sel.rereading)
                {
                    np->m_flags |= NF_DEL;
                }
                np->m_flags |= NF_INCLUDED;
                g_sel_page.total_obj_cnt++;
            }
        }
        if (!g_sel_page.total_obj_cnt)
        {
            if (g_sel.exclusive)
            {
                g_sel.exclusive = false;
                g_sel_page.page_ngd = nullptr;
                goto try_again;
            }
            if (g_only.max_to_do)
            {
                end_only();
                fmt::print("{}", g_msg);
                newline();
                if (fill_last_page)
                {
                    get_anything();
                }
                g_sel_page.page_ngd = nullptr;
                goto try_again;
            }
        }
        if (!g_sel_page.page_ngd)
        {
            (void) g_sel_page.first_page();
        }
        else if (g_sel_page.page_ngd == g_ng_order.last())
        {
            (void) g_sel_page.last_page();
        }
        else if (g_sel_page.prior_obj_cnt && fill_last_page)
        {
            calc_page(no_search);
            if (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt)
            {
                (void) g_sel_page.last_page();
            }
        }
        break;
    }

    case SM_ADD_GROUP:
    {
        sort_add_groups();
        g_obj_count = ArticleNum{};
        for (AddGroupIndex index : g_add_groups.indices())
        {
            AddGroup *gp = &g_add_groups[index];
            if (g_sel_page.page_grp == index)
            {
                g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
            }
            gp->m_flags &= ~AGF_INCLUDED;
            if (!g_sel.rereading ^ !(gp->m_flags & AGF_EXCLUDED))
            {
                continue;
            }
            if (!g_sel.exclusive || (gp->m_flags & g_sel.mask))
            {
                if (g_sel.rereading && !(gp->m_flags & g_sel.mask))
                {
                    gp->m_flags |= AGF_DEL;
                }
                gp->m_flags |= AGF_INCLUDED;
                g_sel_page.total_obj_cnt++;
            }
            ++g_obj_count;
        }
        if (!g_sel_page.total_obj_cnt && g_sel.exclusive)
        {
            g_sel.exclusive = false;
            g_sel_page.page_grp = AddGroupIndex::None;
            goto try_again;
        }
        if (g_sel_page.page_grp == AddGroupIndex::None)
        {
            (void) g_sel_page.first_page();
        }
        else if (g_sel_page.page_grp == g_add_groups.last())
        {
            (void) g_sel_page.last_page();
        }
        else if (g_sel_page.prior_obj_cnt && fill_last_page)
        {
            calc_page(no_search);
            if (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt)
            {
                (void) g_sel_page.last_page();
            }
        }
        break;
    }

    case SM_UNIVERSAL:
    {
        g_obj_count = ArticleNum{};
        bool found_page_univ = !g_sel_page.page_univ_index;

        g_univ.sort();
        for (UniversalItem &item : univ_items())
        {
            if (g_sel_page.page_univ_index == univ_index(&item))
            {
                found_page_univ = true;
                g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
            }
            item.m_flags &= ~UF_INCLUDED;
            bool ui_elig = true;
            if (item.m_state != UIS_NORMAL)
            {
                continue;
            }
            if (std::holds_alternative<UniversalVirtualGroup>(item.m_data))
            {
                // always ineligible items
                ui_elig = false;
            }
            else if (const UniversalNewsgroup *newsgroup = std::get_if<UniversalNewsgroup>(&item.m_data))
            {
                const std::string &group_name = newsgroup->ng;
                if (group_name.empty())
                {
                    ui_elig = false;
                }
                else
                {
                    NewsgroupData *np = find_newsgroup(group_name.c_str());
                    if (!np)
                    {
                        ui_elig = false;
                    }
                    else
                    {
                        if (!np->m_abs_first)
                        {
                            g_to_read_quiet = true;
                            np->set_to_read(ST_LAX);
                            g_to_read_quiet = false;
                        }
                        if (!(g_sel.rereading ^ (np->m_to_read > TR_NONE)))
                        {
                            ui_elig = false;
                        }
                    }
                }
            }
            else if (const UniversalVirtualArticle *article = std::get_if<UniversalVirtualArticle>(&item.m_data))
            {
                // later: use the datasrc of the newsgroup
                ui_elig = !was_read_group(article->num, article->ng.c_str());
                if (g_sel.rereading)
                {
                    ui_elig = !ui_elig;
                }
            }
            else
            {
                ui_elig = !g_sel.rereading;
            }
            if (!ui_elig)
            {
                continue;
            }
            if (!g_sel.exclusive || (item.m_flags & static_cast<UniversalItemFlags>(g_sel.mask)))
            {
                if (g_sel.rereading && !(item.m_flags & static_cast<UniversalItemFlags>(g_sel.mask)))
                {
                    item.m_flags |= UF_DEL;
                }
                item.m_flags |= UF_INCLUDED;
                g_sel_page.total_obj_cnt++;
            }
            ++g_obj_count;
        }
        if (!g_sel_page.total_obj_cnt && g_sel.exclusive)
        {
            g_sel.exclusive = false;
            g_sel_page.page_univ_index = {};
            goto try_again;
        }
        if (!found_page_univ)
        {
            g_sel_page.page_univ_index = {};
        }
        if (!g_sel_page.page_univ_index)
        {
            (void) g_sel_page.first_page();
        }
        else if (g_sel_page.page_univ_index == univ_index(g_univ.last_item()))
        {
            (void) g_sel_page.last_page();
        }
        else if (g_sel_page.prior_obj_cnt && fill_last_page)
        {
            calc_page(no_search);
            if (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt)
            {
                (void) g_sel_page.last_page();
            }
        }
        break;
    }

    case SM_OPTIONS:
    {
        int included = 0;
        const OptionCatalog catalog;
        g_obj_count = ArticleNum{};
        for (int op = catalog.first_row(); op <= catalog.row_count(); op++)
        {
            if (g_sel_page.page_op == op)
            {
                g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
            }
            if (catalog.is_group(op))
            {
                included = (g_opt_edit.flags[op] & OF_SEL);
                g_sel_page.total_obj_cnt++;
                g_opt_edit.flags[op] |= OF_INCLUDED;
            }
            else if (included)
            {
                g_sel_page.total_obj_cnt++;
                g_opt_edit.flags[op] |= OF_INCLUDED;
            }
            else
            {
                g_opt_edit.flags[op] &= ~OF_INCLUDED;
            }
            ++g_obj_count;
        }
        if (g_sel_page.page_op < 1)
        {
            (void) g_sel_page.first_page();
        }
        else if (g_sel_page.page_op >= g_obj_count.value_of())
        {
            (void) g_sel_page.last_page();
        }
        else if (g_sel_page.prior_obj_cnt && fill_last_page)
        {
            calc_page(no_search);
            if (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt)
            {
                (void) g_sel_page.last_page();
            }
        }
        break;
    }

    case SM_ARTICLE:
    {
        Article* ap;
        Article** app;
        Article** limit;

        if (g_sel_page.page_art)
        {
            int desired_flags = (g_sel.rereading ? AF_EXISTS : (AF_EXISTS | AF_UNREAD));
            limit = article_ptr_list_end();
            ap = nullptr;
            for (app = g_sel_page.page_art; app < limit; app++)
            {
                Article *candidate = *app;
                if ((candidate->m_flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
                {
                    ap = candidate;
                    break;
                }
            }
            sort_articles();
            if (ap == nullptr)
            {
                g_sel_page.page_art = article_ptr_list_end();
            }
            else
            {
                limit = article_ptr_list_end();
                for (app = article_ptr_list_begin(); app < limit; app++)
                {
                    if (*app == ap)
                    {
                        g_sel_page.page_art = app;
                        break;
                    }
                }
            }
        }
        else
        {
            sort_articles();
        }

        while (g_sel_page.page_subj && g_sel_page.page_subj->m_misc == 0)
        {
            g_sel_page.page_subj = g_sel_page.page_subj->m_next;
        }
        // The g_artptr_list contains only unread or read articles, never both
        limit = article_ptr_list_end();
        for (app = article_ptr_list_begin(); app < limit; app++)
        {
            ap = *app;
            if (g_sel.rereading && !(ap->m_flags & g_sel.mask))
            {
                ap->m_flags |= AF_DEL;
            }
            if (g_sel_page.page_art == app //
                || (!g_sel_page.page_art && ap->m_subj == g_sel_page.page_subj))
            {
                g_sel_page.page_art = app;
                g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
            }
            if (!g_sel.exclusive || (ap->m_flags & g_sel.mask))
            {
                g_sel_page.total_obj_cnt++;
                ap->m_flags |= AF_INCLUDED;
            }
            else
            {
                ap->m_flags &= ~AF_INCLUDED;
            }
        }
        if (g_sel.exclusive && !g_sel_page.total_obj_cnt)
        {
            g_sel.exclusive = false;
            g_sel_page.page_art = nullptr;
            goto try_again;
        }
        if (!g_sel_page.page_art)
        {
            (void) g_sel_page.first_page();
        }
        else if (g_sel_page.page_art >= limit)
        {
            (void) g_sel_page.last_page();
        }
        else if (g_sel_page.prior_obj_cnt && fill_last_page)
        {
            calc_page(no_search);
            if (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt)
            {
                (void) g_sel_page.last_page();
            }
        }
        break;
    }

    default:
    {
        if (g_sel_page.page_subj)
        {
            while (g_sel_page.page_subj && g_sel_page.page_subj->m_misc == 0)
            {
                g_sel_page.page_subj = g_sel_page.page_subj->m_next;
            }
            sort_subjects();
            if (!g_sel_page.page_subj)
            {
                g_sel_page.page_subj = g_last_subject;
            }
        }
        else
        {
            sort_subjects();
        }
        for (Subject *sp = g_first_subject; sp; sp = sp->m_next)
        {
            if (g_sel.rereading && !(sp->m_flags & g_sel.mask))
            {
                sp->m_flags |= SF_DEL;
            }

            Subject *group_sp = sp;
            int      group_arts = sp->m_misc;

            if (!g_sel.exclusive || (sp->m_flags & g_sel.mask))
            {
                sp->m_flags |= SF_INCLUDED;
            }
            else
            {
                sp->m_flags &= ~SF_INCLUDED;
            }

            if (g_sel_page.page_subj == group_sp)
            {
                g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
            }
            if (g_sel.mode == SM_THREAD)
            {
                while (sp->m_next && sp->m_next->m_thread == sp->m_thread)
                {
                    sp = sp->m_next;
                    if (sp == g_sel_page.page_subj)
                    {
                        g_sel_page.prior_obj_cnt = g_sel_page.total_obj_cnt;
                        g_sel_page.page_subj = group_sp;
                    }
                    sp->m_flags &= ~SF_INCLUDED;
                    if (sp->m_flags & g_sel.mask)
                    {
                        group_sp->m_flags |= SF_INCLUDED;
                    }
                    else if (g_sel.rereading)
                    {
                        sp->m_flags |= SF_DEL;
                    }
                    group_arts += sp->m_misc;
                }
            }
            if (group_sp->m_flags & SF_INCLUDED)
            {
                g_sel_page.total_obj_cnt += group_arts;
            }
        }
        if (g_sel.exclusive && !g_sel_page.total_obj_cnt)
        {
            g_sel.exclusive = false;
            g_sel_page.page_subj = nullptr;
            goto try_again;
        }
        if (!g_sel_page.page_subj)
        {
            (void) g_sel_page.first_page();
        }
        else if (g_sel_page.page_subj == g_last_subject)
        {
            (void) g_sel_page.last_page();
        }
        else if (g_sel_page.prior_obj_cnt && fill_last_page)
        {
            calc_page(no_search);
            if (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt)
            {
                (void) g_sel_page.last_page();
            }
        }
    }
    }
}

/// @brief Sets the selection to the first included object for the current mode.
///
/// This function attempts to set the current selection page pointer to the
/// first object that is marked as included (e.g., group, article, subject,
/// etc.) for the current selection mode. If the selection is already at the
/// first included object, no change is made. Returns true if the selection
/// was changed, otherwise false.
///
/// @globals
/// - g_sel.mode: Read to select the active population.
/// - g_multircs, g_ng_order.order, g_add_groups, g_univ.items: Read through catalog helpers.
/// - g_art_ptr_list, g_first_subject: Read for article and subject traversal.
///
bool SelectorPage::first_page()
{
    prior_obj_cnt = 0;

    switch (g_sel.mode)
    {
    case SM_MULTIRC:
    {
        for (Multirc *mp = multirc_low(); mp; mp = multirc_next(mp))
        {
            if (mp->m_flags & MF_INCLUDED)
            {
                if (page_mrc != mp)
                {
                    page_mrc = mp;
                    return true;
                }
                break;
            }
        }
        break;
    }

    case SM_NEWSGROUP:
    {
        for (NewsgroupData *np = g_ng_order.first(); np; np = newsgroup_next(np))
        {
            if (np->m_flags & NF_INCLUDED)
            {
                if (page_ngd != np)
                {
                    page_ngd = np;
                    return true;
                }
                break;
            }
        }
        break;
    }

    case SM_ADD_GROUP:
    {
        for (AddGroupIndex index : g_add_groups.indices())
        {
            if (g_add_groups[index].m_flags & AGF_INCLUDED)
            {
                if (page_grp != index)
                {
                    page_grp = index;
                    return true;
                }
                break;
            }
        }
        break;
    }

    case SM_UNIVERSAL:
    {
        for (UniversalItem &item : univ_items())
        {
            if (item.m_flags & UF_INCLUDED)
            {
                if (page_univ_index != univ_index(&item))
                {
                    page_univ_index = univ_index(&item);
                    return true;
                }
                break;
            }
        }
        break;
    }

    case SM_OPTIONS:
    {
        if (page_op != 1)
        {
            page_op = 1;
            return true;
        }
        break;
    }

    case SM_ARTICLE:
    {
        Article **limit = article_ptr_list_end();
        for (Article **app = article_ptr_list_begin(); app < limit; app++)
        {
            if ((*app)->m_flags & AF_INCLUDED)
            {
                if (page_art != app)
                {
                    page_art = app;
                    return true;
                }
                break;
            }
        }
        break;
    }

    default:
    {
        for (Subject *sp = g_first_subject; sp; sp = sp->m_next)
        {
            if (sp->m_flags & SF_INCLUDED)
            {
                if (page_subj != sp)
                {
                    page_subj = sp;
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

/// @brief Sets the selection to the last included object for the current mode.
///
/// This function attempts to set the current selection page pointer to the
/// last object that is marked as included (e.g., group, article, subject,
/// etc.) for the current selection mode. It temporarily clears the current
/// page pointer, calls prev_page() to move to the last page, and restores
/// the pointer if no change occurred. Returns true if the selection was
/// changed, otherwise false.
///
/// @globals
/// - g_sel, g_obj_count: Read for selector mode, policy, and option count.
/// - g_art_ptr_list: Read to locate the end of article order.
/// @note prev_page retains its catalog, inclusion, and display-policy dependencies.
///
bool SelectorPage::last_page()
{
    prior_obj_cnt = total_obj_cnt;

    switch (g_sel.mode)
    {
    case SM_MULTIRC:
    {
        Multirc *mp = page_mrc;
        page_mrc = nullptr;
        if (!prev_page())
        {
            page_mrc = mp;
        }
        else if (mp != page_mrc)
        {
            return true;
        }
        break;
    }

    case SM_NEWSGROUP:
    {
        NewsgroupData *np = page_ngd;
        page_ngd = nullptr;
        if (!prev_page())
        {
            page_ngd = np;
        }
        else if (np != page_ngd)
        {
            return true;
        }
        break;
    }

    case SM_ADD_GROUP:
    {
        const AddGroupIndex gp = page_grp;
        page_grp = AddGroupIndex::None;
        if (!prev_page())
        {
            page_grp = gp;
        }
        else if (gp != page_grp)
        {
            return true;
        }
        break;
    }

    case SM_UNIVERSAL:
    {
        const UniversalItemIndex ui = page_univ_index;
        page_univ_index = {};
        if (!prev_page())
        {
            page_univ_index = ui;
        }
        else if (ui != page_univ_index)
        {
            return true;
        }
        break;
    }

    case SM_OPTIONS:
    {
        int op = page_op;
        page_op = g_obj_count.value_of() + 1;
        if (!prev_page())
        {
            page_op = op;
        }
        else if (op != page_op)
        {
            return true;
        }
        break;
    }

    case SM_ARTICLE:
    {
        Article **app = page_art;
        page_art = article_ptr_list_end();
        if (!prev_page())
        {
            page_art = app;
        }
        else if (app != page_art)
        {
            return true;
        }
        break;
    }

    default:
    {
        Subject *sp = page_subj;
        page_subj = nullptr;
        if (!prev_page())
        {
            page_subj = sp;
        }
        else if (sp != page_subj)
        {
            return true;
        }
        break;
    }
    }
    return false;
}

/// @brief Advances the selection to the next page of included objects.
///
/// This function updates the current selection page pointer to the next set
/// of included objects for the current selection mode (e.g., group, article,
/// subject, etc.), if available. It also increments the count of prior
/// objects by the number of objects on the current page. Returns true if the
/// selection was advanced, otherwise false.
///
/// @globals
/// - g_sel.mode: Selects the population whose next anchor is used.
/// - g_obj_count: Bounds the next option page.
/// - g_art_ptr_list: Bounds the next article page via article_ptr_list_end().
///
bool SelectorPage::next_page()
{
    if (!total_obj_cnt || prior_obj_cnt + page_obj_cnt >= total_obj_cnt)
    {
        return false;
    }
    switch (g_sel.mode)
    {
    case SM_MULTIRC:
    {
        if (next_mrc)
        {
            page_mrc = next_mrc;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }

    case SM_NEWSGROUP:
    {
        if (next_ngd)
        {
            page_ngd = next_ngd;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }

    case SM_ADD_GROUP:
    {
        if (next_grp != AddGroupIndex::None)
        {
            page_grp = next_grp;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }

    case SM_UNIVERSAL:
    {
        if (next_univ_index)
        {
            page_univ_index = next_univ_index;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }

    case SM_OPTIONS:
    {
        if (next_op <= g_obj_count.value_of())
        {
            page_op = next_op;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }

    case SM_ARTICLE:
    {
        if (next_art < article_ptr_list_end())
        {
            page_art = next_art;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }

    default:
    {
        if (next_subj)
        {
            page_subj = next_subj;
            prior_obj_cnt += page_obj_cnt;
            return true;
        }
        break;
    }
    }
    return false;
}

/// @brief Moves the selection to the previous page of included objects.
///
/// This function scans backward through the included objects for the current
/// selection mode (e.g., group, article, subject, etc.), updating the current
/// selection page pointer to the start of the previous page if possible. It
/// decrements the count of prior objects as it moves back. Returns true if the
/// selection was moved to a previous page, otherwise false.
///
/// @globals
/// - g_sel: Read for the active mode and selection policy.
/// - g_multircs, g_ng_order.order, g_add_groups, g_univ.items: Read through catalog helpers.
/// - g_opt_edit.flags, g_art_ptr_list, g_first_subject, g_last_subject: Read for included entries and traversal.
/// - g_tc_COLS, g_sel_art_display_mode, g_sel_art_display_mode_index: Read by line-count helpers.
///
bool SelectorPage::prev_page()
{
    int item_cnt = 0;

    // Scan the items in reverse to go back a page
    switch (g_sel.mode)
    {
    case SM_MULTIRC:
    {
        Multirc *mp = page_mrc;
        Multirc *page_mp = page_mrc;

        if (!mp)
        {
            mp = multirc_high();
        }
        else
        {
            mp = multirc_prev(multirc_ptr(mp->m_num));
        }
        while (mp)
        {
            if (mp->m_flags & MF_INCLUDED)
            {
                page_mp = mp;
                prior_obj_cnt--;
                if (++item_cnt >= max_per_page)
                {
                    break;
                }
            }
            mp = multirc_prev(mp);
        }
        if (page_mrc != page_mp)
        {
            page_mrc = page_mp;
            return true;
        }
        break;
    }

    case SM_NEWSGROUP:
    {
        NewsgroupData *np = page_ngd;
        NewsgroupData *page_np = page_ngd;

        if (!np)
        {
            np = g_ng_order.last();
        }
        else
        {
            np = newsgroup_prev(np);
        }
        while (np)
        {
            if (np->m_flags & NF_INCLUDED)
            {
                page_np = np;
                prior_obj_cnt--;
                if (++item_cnt >= max_per_page)
                {
                    break;
                }
            }
            np = newsgroup_prev(np);
        }
        if (page_ngd != page_np)
        {
            page_ngd = page_np;
            return true;
        }
        break;
    }

    case SM_ADD_GROUP:
    {
        AddGroupIndex gp = page_grp;
        AddGroupIndex page_gp = page_grp;

        if (gp == AddGroupIndex::None)
        {
            gp = g_add_groups.last();
        }
        else
        {
            gp = g_add_groups.prev(gp);
        }
        while (gp != AddGroupIndex::None)
        {
            if (g_add_groups[gp].m_flags & AGF_INCLUDED)
            {
                page_gp = gp;
                prior_obj_cnt--;
                if (++item_cnt >= max_per_page)
                {
                    break;
                }
            }
            gp = g_add_groups.prev(gp);
        }
        if (page_grp != page_gp)
        {
            page_grp = page_gp;
            return true;
        }
        break;
    }

    case SM_UNIVERSAL:
    {
        UniversalItem     *ui = g_univ.item(page_univ_index);
        UniversalItemIndex page_ui = page_univ_index;

        if (!ui)
        {
            ui = g_univ.last_item();
        }
        else
        {
            ui = g_univ.prev_item(ui);
        }
        while (ui)
        {
            if (ui->m_flags & UF_INCLUDED)
            {
                page_ui = univ_index(ui);
                prior_obj_cnt--;
                if (++item_cnt >= max_per_page)
                {
                    break;
                }
            }
            ui = g_univ.prev_item(ui);
        }
        if (page_univ_index != page_ui)
        {
            page_univ_index = page_ui;
            return true;
        }
        break;
    }

    case SM_OPTIONS:
    {
        int op = page_op;
        int pg_op = page_op;

        while (--op > 0)
        {
            if (g_opt_edit.flags[op] & OF_INCLUDED)
            {
                pg_op = op;
                prior_obj_cnt--;
                if (++item_cnt >= max_per_page)
                {
                    break;
                }
            }
        }
        if (page_op != pg_op)
        {
            page_op = pg_op;
            return true;
        }
        break;
    }

    case SM_ARTICLE:
    {
        Article **page_app = page_art;

        for (Article **app = page_art; app > article_ptr_list_begin();)
        {
            --app;
            if ((*app)->m_flags & AF_INCLUDED)
            {
                page_app = app;
                prior_obj_cnt--;
                if (++item_cnt >= max_per_page)
                {
                    break;
                }
            }
        }
        if (page_art != page_app)
        {
            page_art = page_app;
            return true;
        }
    }

    default:
    {
        Subject  *page_sp = page_subj;
        int       line_cnt;

        int line = 2;
        for (Subject *sp = (!page_sp ? g_last_subject : page_sp->m_prev); sp; sp = sp->m_prev)
        {
            int item_arts = sp->m_misc;
            if (g_sel.mode == SM_THREAD)
            {
                while (sp->m_prev && sp->m_prev->m_thread == sp->m_thread)
                {
                    sp = sp->m_prev;
                    item_arts += sp->m_misc;
                }
                line_cnt = count_thread_lines(sp, (int*)nullptr);
            }
            else
            {
                line_cnt = count_subject_lines(sp, (int *) nullptr);
            }
            if (!(sp->m_flags & SF_INCLUDED) || !line_cnt)
            {
                continue;
            }
            line_cnt = std::min(line_cnt, max_line_cnt);
            line += line_cnt;
            if (line > max_line_cnt + 2)
            {
                sp = page_sp;
                break;
            }
            prior_obj_cnt -= item_arts;
            page_sp = sp;
            if (++item_cnt >= max_per_page)
            {
                break;
            }
        }
        if (page_subj != page_sp)
        {
            page_subj = (page_sp ? page_sp : g_first_subject);
            return true;
        }
        break;
    }
    }
    return false;
}

/// @brief Calculates and sets up the current selection page and item index.
///
/// Determines the set of selectable objects and their positions for the
/// current page, based on the selection mode and the provided target object.
/// Updates page and item counters, next-page pointers, and selection index.
/// If the target object is not on the current page, may advance or rewind
/// pages to locate it. Returns true if a page change was required to find
/// the object, otherwise false.
///
/// @param u The Selection object specifying the target to locate on the page.
///
/// @return True if a page change was required to find the object, false
/// otherwise.
///
/// @globals
/// - g_sel_page.item_index: Set to the index of the target item or -1 if not found.
/// - g_sel_page.page_obj_cnt: Set to the number of objects on the current page.
/// - g_sel_page.page_item_cnt: Set to the number of selectable items on the page.
/// - g_term_line: Reset to 2 at the start of the function.
/// - g_sel_page.next_mp: Set to the next Multirc for paging in SM_MULTIRC mode.
/// - g_sel_page.next_np: Set to the next NewsgroupData for paging in SM_NEWSGROUP mode.
/// - g_sel_page.next_gp: Set to the next AddGroup for paging in SM_ADD_GROUP mode.
/// - g_sel_page.next_univ_index: Set to the next UniversalItem for paging in SM_UNIVERSAL mode.
/// - g_sel_page.next_op: Set to the next option index for paging in SM_OPTIONS mode.
/// - g_sel_page.next_app: Set to the next Article** for paging in SM_ARTICLE mode.
/// - g_sel_page.next_sp: Set to the next Subject for paging in subject/thread mode.
///
bool calc_page(Selection u)
{
    int ret = false;
    if (u.op != -1)
    {
        g_sel_page.item_index = -1;
    }
try_again:
    g_sel_page.page_obj_cnt = 0;
    g_sel_page.page_item_cnt = 0;
    g_term_line = 2;

    switch (g_sel.mode)
    {
    case SM_MULTIRC:
    {
        Multirc *mp = g_sel_page.page_mrc;
        for (; mp && g_sel_page.page_item_cnt < g_sel_page.max_per_page; mp = multirc_next(mp))
        {
            if (mp == u.mp)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (mp->m_flags & MF_INCLUDED)
            {
                g_sel_page.page_item_cnt++;
            }
        }
        g_sel_page.page_obj_cnt = g_sel_page.page_item_cnt;
        g_sel_page.next_mrc = mp;
        break;
    }

    case SM_NEWSGROUP:
    {
        NewsgroupData *np = g_sel_page.page_ngd;
        for (; np && g_sel_page.page_item_cnt < g_sel_page.max_per_page; np = newsgroup_next(np))
        {
            if (np == u.np)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (np->m_flags & NF_INCLUDED)
            {
                g_sel_page.page_item_cnt++;
            }
        }
        g_sel_page.page_obj_cnt = g_sel_page.page_item_cnt;
        g_sel_page.next_ngd = np;
        break;
    }

    case SM_ADD_GROUP:
    {
        AddGroupIndex gp = g_sel_page.page_grp;
        for (; gp != AddGroupIndex::None && g_sel_page.page_item_cnt < g_sel_page.max_per_page;
             gp = g_add_groups.next(gp))
        {
            if (gp == u.gp)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (g_add_groups[gp].m_flags & AGF_INCLUDED)
            {
                g_sel_page.page_item_cnt++;
            }
        }
        g_sel_page.page_obj_cnt = g_sel_page.page_item_cnt;
        g_sel_page.next_grp = gp;
        break;
    }

    case SM_UNIVERSAL:
    {
        UniversalItemIndex next_univ = {};
        for (UniversalItem &item : univ_items(g_sel_page.page_univ_index))
        {
            if (g_sel_page.page_item_cnt >= g_sel_page.max_per_page)
            {
                next_univ = univ_index(&item);
                break;
            }
            if (&item == u.un)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (item.m_flags & UF_INCLUDED)
            {
                g_sel_page.page_item_cnt++;
            }
            next_univ = univ_index(g_univ.next_item(&item));
        }
        g_sel_page.page_obj_cnt = g_sel_page.page_item_cnt;
        g_sel_page.next_univ_index = next_univ;
        break;
    }

    case SM_OPTIONS:
    {
        int op = g_sel_page.page_op;
        for (; op <= g_obj_count.value_of() && g_sel_page.page_item_cnt < g_sel_page.max_per_page; op++)
        {
            if (op == u.op)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (g_opt_edit.flags[op] & OF_INCLUDED)
            {
                g_sel_page.page_item_cnt++;
            }
        }
        g_sel_page.page_obj_cnt = g_sel_page.page_item_cnt;
        g_sel_page.next_op = op;
        break;
    }

    case SM_ARTICLE:
    {
        Article **app = g_sel_page.page_art;
        Article** limit = article_ptr_list_end();
        for (; app < limit && g_sel_page.page_item_cnt < g_sel_page.max_per_page; app++)
        {
            if (*app == u.ap)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if ((*app)->m_flags & AF_INCLUDED)
            {
                g_sel_page.page_item_cnt++;
            }
        }
        g_sel_page.page_obj_cnt = g_sel_page.page_item_cnt;
        g_sel_page.next_art = app;
        break;
    }

    default:
    {
        Subject *sp = g_sel_page.page_subj;
          int      line_cnt;
          int      sel;
        for (; sp && g_sel_page.page_item_cnt < g_sel_page.max_per_page; sp = sp->m_next)
          {
            if (sp == u.sp)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (sp->m_flags & SF_INCLUDED)
            {
                if (g_sel.mode == SM_THREAD)
                {
                    line_cnt = count_thread_lines(sp, &sel);
                }
                else
                {
                    line_cnt = count_subject_lines(sp, &sel);
                }
                if (line_cnt)
                {
                    line_cnt = std::min(line_cnt, g_sel_page.max_line_cnt);
                    if (g_term_line + line_cnt > g_sel_page.max_line_cnt + 2)
                    {
                        break;
                    }
                    g_sel_page.page_obj_cnt += sp->m_misc;
                    g_sel_page.page_item_cnt++;
                }
            }
            else
            {
                line_cnt = 0;
            }
            if (g_sel.mode == SM_THREAD)
            {
                while (sp->m_next && sp->m_next->m_thread == sp->m_thread)
                {
                    sp = sp->m_next;
                    if (!line_cnt || !sp->m_misc)
                    {
                        continue;
                    }
                    g_sel_page.page_obj_cnt += sp->m_misc;
                }
            }
        }
        g_sel_page.next_subj = sp;
        break;
    }
    }
    if (u.op != -1 && g_sel_page.item_index < 0)
    {
        if (!ret)
        {
            if (g_sel_page.first_page())
            {
                ret = true;
                goto try_again;
            }
        }
        if (g_sel_page.next_page())
        {
            ret = true;
            goto try_again;
        }
        g_sel_page.first_page();
        g_sel_page.item_index = 0;
    }
    return ret;
}

/// @brief Displays the title and summary information for the current selection page.
///
/// Outputs the appropriate page title, group/article counts, and context-specific
/// information for the current selection mode. Handles screen clearing, cursor
/// positioning, and displays additional status or restriction messages as needed.
///
/// @param home_only If true, only moves the cursor home and erases the line;
/// otherwise, clears the screen and redraws the title.
///
static void display_page_title(bool home_only)
{
    if (home_only || (g_erase_screen && g_erase_each_line))
    {
        home_cursor();
    }
    else
    {
        clear();
    }
    if (g_sel.mode == SM_MULTIRC)
    {
        color_string(COLOR_HEADING, "Newsrcs");
    }
    else if (g_sel.mode == SM_OPTIONS)
    {
        color_string(COLOR_HEADING, "Options");
    }
    else if (g_sel.mode == SM_UNIVERSAL)
    {
        g_color.object(COLOR_HEADING, true);
        fmt::print("[{}] {}", g_univ.level, g_univ.title.empty() ? "Universal Selector" : g_univ.title.c_str());
        g_color.pop();
    }
    else if (g_sel.group_listing)
    {
        color_string(COLOR_HEADING, "Unsubscribed newsgroups");
    }
    else if (g_in_ng)
    {
        color_string(COLOR_NG_NAME, g_newsgroup_name.c_str());
    }
    else if (g_sel.mode == SM_ADD_GROUP)
    {
        color_string(COLOR_HEADING, "ADD newsgroups");
    }
    else
    {
        Newsrc     *rp;
        std::string news_sources;
        news_sources.reserve(34);
        std::size_t len = 0;
        g_color.object(COLOR_HEADING, true);
        fmt::print("Newsgroups");
        for (rp = g_multirc->m_first; rp && len < 34; rp = rp->next)
        {
            if (rp->flags & RF_ACTIVE)
            {
                fmt::format_to(std::back_inserter(news_sources), "{}{}", news_sources.empty() ? "" : ", ",
                               rp->data_source->m_name);
                len = news_sources.size() + 2;
            }
        }
        if (rp)
        {
            news_sources += ", ...";
        }
        if (news_sources != "default")
        {
            fmt::print(" (group #{}: {})", g_multirc->m_num, news_sources);
        }
        g_color.pop(); // of COLOR_HEADING
    }
    if (home_only || (g_erase_screen && g_erase_each_line))
    {
        erase_eol();
    }
    if (g_sel.mode == SM_MULTIRC)
    {
    }
    else if (g_sel.mode == SM_UNIVERSAL)
    {
    }
    else if (g_sel.mode == SM_OPTIONS)
    {
        fmt::print("      (Press 'S' to save changes, 'q' to abandon, or TAB to use.)");
    }
    else if (g_in_ng && !g_sel.group_listing)
    {
        fmt::print("          {} {}article{}", g_sel_page.total_obj_cnt, g_sel.rereading ? "read " : "",
                   g_sel_page.total_obj_cnt == 1 ? "" : "s");
        if (g_sel.exclusive)
        {
            fmt::print(" out of {}", g_obj_count.value_of());
        }
        fmt::print("{}", g_moderated);
    }
    else
    {
        fmt::print("       {}{} group{}", s_group_init_done ? "" : "~", g_sel_page.total_obj_cnt,
                   plural(g_sel_page.total_obj_cnt));
        if (g_sel.exclusive)
        {
            fmt::print(" out of {}", g_obj_count.value_of());
        }
        if (g_only.max_to_do)
        {
            fmt::print(" (Restriction)");
        }
    }
    home_cursor();
    newline();
    maybe_eol();
    if (g_sel.group_listing)
    {
        fmt::print("Glob: {:.{}}  O:order R:reverse l:glob q:quit", active_list_pattern(), std::max(g_tc_COLS - 48, 0));
    }
    if (g_in_ng && g_redirected && !g_redirected_to.empty())
    {
        fmt::print("\t** Please start using {} **", g_redirected_to);
    }
    newline();
}

/// @brief Displays the current selection page with selectable items and details.
///
/// Renders the current selection page, including selectable objects, their
/// status, and summary information, according to the current selection mode.
/// Handles paging, item highlighting, and invokes the appropriate display
/// routines for each item type. If no items are present, attempts to move to
/// the previous page. Updates navigation and selection state for the page.
///
/// @globals
/// - g_sel_page.item_index: Set to the index of the last selected item on the page.
/// - g_sel_page.page_obj_cnt: Set to the number of objects displayed on the page.
/// - g_sel_page.page_item_cnt: Set to the number of selectable items on the page.
/// - g_sel_page.next_mp: Set to the next Multirc for paging in SM_MULTIRC mode.
/// - g_sel_page.next_np: Set to the next NewsgroupData for paging in SM_NEWSGROUP mode.
/// - g_sel_page.next_gp: Set to the next AddGroup for paging in SM_ADD_GROUP mode.
/// - g_sel_page.next_univ_index: Set to the next UniversalItem for paging in SM_UNIVERSAL mode.
/// - g_sel_page.next_op: Set to the next option index for paging in SM_OPTIONS mode.
/// - g_sel_page.next_app: Set to the next Article** for paging in SM_ARTICLE mode.
/// - g_sel_page.next_sp: Set to the next Subject for paging in subject/thread mode.
/// - g_sel_page.last_ap: Reset to nullptr at the end of the function.
/// - g_sel_page.last_sp: Reset to nullptr at the end of the function.
/// - g_sel_page.at_end: Set to true if at the end of the selectable items.
/// - g_sel_page.last_line: Set to the last terminal line used for display.
///
void display_page()
{
    int sel;

    display_page_title(false);

try_again:
    g_sel_page.init();

    if (!g_sel_page.total_obj_cnt)
    {
    }
    else if (g_sel.mode == SM_MULTIRC)
    {
        Multirc *mp = g_sel_page.page_mrc;
        Newsrc  *rp;
        for (; mp && g_sel_page.page_item_cnt < g_sel_page.max_per_page; mp = multirc_next(mp))
        {
            if (!(mp->m_flags & MF_INCLUDED))
            {
                continue;
            }

            // multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
            TRN_ASSERT((g_sel.mask & (AGF_DEL | AGF_DEL_SEL)) == 0);
            sel = !!(mp->m_flags & static_cast<MultircFlags>(g_sel.mask));
            g_sel_page.items[g_sel_page.page_item_cnt].u.mp = mp;
            g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
            g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
            g_sel_page.page_obj_cnt++;

            maybe_eol();
            std::string news_sources;
            news_sources.reserve(34);
            std::size_t len = 0;
            for (rp = mp->m_first; rp && len < 34; rp = rp->next)
            {
                fmt::format_to(std::back_inserter(news_sources), "{}{}", news_sources.empty() ? "" : ", ",
                               rp->data_source->m_name);
                len = news_sources.size() + 2;
            }
            if (rp)
            {
                news_sources += ", ...";
            }
            output_sel(g_sel_page.page_item_cnt, sel, false);
            fmt::print("{:5} {}\n", mp->m_num, news_sources);
            term_down(1);
            g_sel_page.page_item_cnt++;
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_mrc = mp;
    }
    else if (g_sel.mode == SM_NEWSGROUP)
    {
        NewsgroupData* np;
        int            max_len = 0;
        int            outputting = (current_group_display_mode() != 'l');
start_of_loop:
        for (np = g_sel_page.page_ngd; np; np = newsgroup_next(np))
        {
            if (np == g_newsgroup_ptr)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }

            if (!(np->m_flags & NF_INCLUDED))
            {
                continue;
            }

            if (!np->m_abs_first)
            {
                np->set_to_read(ST_LAX);
                if (g_paranoid)
                {
                    newline();
                    g_current_newsgroup = np;
                    g_newsgroup_ptr = np;
                    // this may move newsgroups around
                    cleanup_newsrc(np->m_rc);
                    init_pages(PRESERVE_PAGE);
                    display_page_title(false);
                    goto try_again;
                }
                if (g_sel.rereading ? (np->m_to_read != TR_NONE) //
                                    : (np->m_to_read < g_newsgroup_min_to_read))
                {
                    np->m_flags &= ~NF_INCLUDED;
                    g_sel_page.total_obj_cnt--;
                    --g_obj_count;
                    continue;
                }
            }

            if (g_sel_page.page_item_cnt >= g_sel_page.max_per_page)
            {
                break;
            }

            if (outputting)
            {
                sel = !!(np->m_flags & g_sel.mask) + (np->m_flags & NF_DEL);
                g_sel_page.items[g_sel_page.page_item_cnt].u.np = np;
                g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
                g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
                g_sel_page.page_obj_cnt++;

                maybe_eol();
                output_sel(g_sel_page.page_item_cnt, sel, false);
                fmt::print("{:5} ", static_cast<long>(np->m_to_read));
                display_group(np->m_rc->data_source, np->rc_name(), max_len);
            }
            else if (np->m_num_offset >= max_len)
            {
                max_len = np->m_num_offset + 1;
            }
            g_sel_page.page_item_cnt++;
        }
        if (!outputting)
        {
            outputting = 1;
            g_sel_page.init();
            goto start_of_loop;
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_ngd = np;
        if (!s_group_init_done)
        {
            for (; np; np = newsgroup_next(np))
            {
                if (!np->m_abs_first)
                {
                    break;
                }
            }
            if (!np)
            {
                int line = g_term_line;
                s_group_init_done = true;
                display_page_title(true);
                goto_xy(0,line);
            }
        }
    }
    else if (g_sel.mode == SM_ADD_GROUP)
    {
        AddGroupIndex gp = g_sel_page.page_grp;
        int max_len = 0;
        if (current_group_display_mode() == 'l')
        {
            int i = 0;
            for (; gp != AddGroupIndex::None && i < g_sel_page.max_per_page; gp = g_add_groups.next(gp))
            {
                if (!(g_add_groups[gp].m_flags & AGF_INCLUDED))
                {
                    continue;
                }
                int len = static_cast<int>(g_add_groups[gp].m_name.size()) + 2;
                max_len = std::max(len, max_len);
                i++;
            }
            gp = g_sel_page.page_grp;
        }
        for (; gp != AddGroupIndex::None && g_sel_page.page_item_cnt < g_sel_page.max_per_page;
             gp = g_add_groups.next(gp))
        {
            const AddGroup &group = g_add_groups[gp];
            if (!(group.m_flags & AGF_INCLUDED))
            {
                continue;
            }

            sel = !!(group.m_flags & g_sel.mask) + (group.m_flags & AGF_DEL);
            g_sel_page.items[g_sel_page.page_item_cnt].u.gp = gp;
            g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
            g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
            g_sel_page.page_obj_cnt++;

            maybe_eol();
            output_sel(g_sel_page.page_item_cnt, sel, false);
            fmt::print("{:5} ", group.m_to_read.value_of());
            display_group(group.m_data_src, group.m_name, max_len);
            g_sel_page.page_item_cnt++;
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_grp = gp;
    }
    else if (g_sel.mode == SM_UNIVERSAL)
    {
        UniversalItemIndex next_univ = {};
        for (UniversalItem &item : univ_items(g_sel_page.page_univ_index))
        {
            if (g_sel_page.page_item_cnt >= g_sel_page.max_per_page)
            {
                next_univ = univ_index(&item);
                break;
            }
            if (!(item.m_flags & UF_INCLUDED))
            {
                next_univ = univ_index(g_univ.next_item(&item));
                continue;
            }

            sel = !!(item.m_flags & static_cast<UniversalItemFlags>(g_sel.mask)) + (item.m_flags & UF_DEL);
            g_sel_page.items[g_sel_page.page_item_cnt].u.un = &item;
            g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
            g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
            g_sel_page.page_obj_cnt++;

            maybe_eol();
            output_sel(g_sel_page.page_item_cnt, sel, false);
            std::putchar(' ');
            display_universal(&item);
            g_sel_page.page_item_cnt++;
            next_univ = univ_index(g_univ.next_item(&item));
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_univ_index = next_univ;
    }
    else if (g_sel.mode == SM_OPTIONS)
    {
        int                 op = g_sel_page.page_op;
        const OptionCatalog catalog;
        for (; op <= g_obj_count.value_of() && g_sel_page.page_item_cnt < g_sel_page.max_per_page; op++)
        {
            if (!(g_opt_edit.flags[op] & OF_INCLUDED))
            {
                continue;
            }

            if (catalog.is_group(op))
            {
                sel = !!(g_opt_edit.flags[op] & OF_SEL);
            }
            else
            {
                const OptionIndex option = catalog.option(op);
                sel = g_opt_edit.contains(option)
                          ? 1
                          : (g_option_saved_vals[option] ? 3 : (g_option_def_vals[option] ? 0 : 2));
            }
            g_sel_page.items[g_sel_page.page_item_cnt].u.op = static_cast<OptionIndex>(op);
            g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
            g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
            g_sel_page.page_obj_cnt++;

            maybe_eol();
            display_option(op, g_sel_page.page_item_cnt);
            g_sel_page.page_item_cnt++;
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_op = op;
    }
    else if (g_sel.mode == SM_ARTICLE)
    {
        Article **limit = article_ptr_list_end();
        Article **app = g_sel_page.page_art;
        for (; app < limit && g_sel_page.page_item_cnt < g_sel_page.max_per_page; app++)
        {
            Article *ap = *app;
            if (ap == g_sel_page.last_art)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }
            if (!(ap->m_flags & AF_INCLUDED))
            {
                continue;
            }
            sel = !!(ap->m_flags & g_sel.mask) + (ap->m_flags & AF_DEL);
            g_sel_page.items[g_sel_page.page_item_cnt].u.ap = ap;
            g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
            g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
            g_sel_page.page_obj_cnt++;
            // Output the article, with optional author
            display_article(ap, g_sel_page.page_item_cnt, sel);
            g_sel_page.page_item_cnt++;
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_art = app;
    }
    else
    {
        int line_cnt;
        int etc = 0;
        int ix = -1;    // item # or -1

        Subject *sp = g_sel_page.page_subj;
        for (; sp && !etc && g_sel_page.page_item_cnt < g_sel_page.max_per_page; sp = sp->m_next)
        {
            if (sp == g_sel_page.last_subj)
            {
                g_sel_page.item_index = g_sel_page.page_item_cnt;
            }

            if (sp->m_flags & SF_INCLUDED)
            {
                // Compute how many lines we need to display this group
                if (g_sel.mode == SM_THREAD)
                {
                    line_cnt = count_thread_lines(sp, &sel);
                }
                else
                {
                    line_cnt = count_subject_lines(sp, &sel);
                }
                if (line_cnt)
                {
                    // If this item is too long to fit on the screen all by
                    // itself, trim it to fit and set the "etc" flag.
                    if (line_cnt > g_sel_page.max_line_cnt)
                    {
                        etc = line_cnt;
                        line_cnt = g_sel_page.max_line_cnt;
                    }
                    // If it doesn't fit, save it for the next page
                    if (g_term_line + line_cnt > g_sel_page.max_line_cnt + 2)
                    {
                        break;
                    }
                    g_sel_page.items[g_sel_page.page_item_cnt].u.sp = sp;
                    g_sel_page.items[g_sel_page.page_item_cnt].line = g_term_line;
                    g_sel_page.items[g_sel_page.page_item_cnt].sel = sel;
                    g_sel_page.page_obj_cnt += sp->m_misc;

                    ix = g_sel_page.page_item_cnt;
                    sel = g_sel_page.items[g_sel_page.page_item_cnt].sel;
                    g_sel_page.page_item_cnt++;
                    if (sp->m_misc)
                    {
                        display_subject(sp, ix, sel);
                        ix = -1;
                    }
                }
            }
            else
            {
                line_cnt = 0;
            }
            if (g_sel.mode == SM_THREAD)
            {
                while (sp->m_next && sp->m_next->m_thread == sp->m_thread)
                {
                    sp = sp->m_next;
                    if (!line_cnt || !sp->m_misc)
                    {
                        continue;
                    }
                    if (g_term_line < g_sel_page.max_line_cnt + 2)
                    {
                        display_subject(sp, ix, sel);
                    }
                    ix = -1;
                    g_sel_page.page_obj_cnt += sp->m_misc;
                }
            }
            if (etc)
            {
                fmt::print("     ... etc. [{} lines total]", etc);
            }
        }
        if (!g_sel_page.page_obj_cnt)
        {
            if (g_sel_page.last_page())
            {
                goto try_again;
            }
        }
        g_sel_page.next_subj = sp;
    }
    g_sel_page.last_art = nullptr;
    g_sel_page.last_subj = nullptr;
    g_sel_page.at_end = (g_sel_page.prior_obj_cnt + g_sel_page.page_obj_cnt == g_sel_page.total_obj_cnt);
    maybe_eol();
    newline();
    g_sel_page.last_line = g_term_line;
}

/// @brief Updates the display of selection items on the current page.
///
/// Iterates through all selectable items on the current page and updates their
/// display if their selection state has changed. Also advances the selection
/// index to the next item, wrapping to zero if at the end.
///
/// @globals
/// - g_sel_page.item_index: Set to the index of the last updated item, or reset to 0
///   if the end of the page is reached.
///
void update_page()
{
    int sel;
    for (int j = 0; j < g_sel_page.page_item_cnt; j++)
    {
        Selection u = g_sel_page.items[j].u;
        switch (g_sel.mode)
        {
        case SM_MULTIRC:
            // multirc_flags have no equivalent to AGF_DEL, AGF_DELSEL
            TRN_ASSERT((g_sel.mask & (AGF_DEL | AGF_DEL_SEL)) == 0);
            sel = !!(u.mp->m_flags & static_cast<MultircFlags>(g_sel.mask));
            break;

        case SM_NEWSGROUP:
            sel = !!(u.np->m_flags & g_sel.mask) + (u.np->m_flags & NF_DEL);
            break;

        case SM_ADD_GROUP:
            sel = !!(g_add_groups[u.gp].m_flags & g_sel.mask) + (g_add_groups[u.gp].m_flags & AGF_DEL);
            break;

        case SM_UNIVERSAL:
            sel = !!(u.un->m_flags & g_sel.mask) + (u.un->m_flags & UF_DEL);
            break;

        case SM_OPTIONS:
        {
            const OptionCatalog catalog;
            const int row = static_cast<int>(u.op);
            if (catalog.is_group(row))
            {
                sel = !!(g_opt_edit.flags[u.op] & OF_SEL);
            }
            else
            {
                const OptionIndex option = catalog.option(row);
                sel = g_opt_edit.contains(option)
                          ? 1
                          : (g_option_saved_vals[option] ? 3 : (g_option_def_vals[option] ? 0 : 2));
            }
            break;
        }

        case SM_ARTICLE:
            sel = !!(u.ap->m_flags & g_sel.mask) + (u.ap->m_flags & AF_DEL);
            break;

        case SM_THREAD:
            (void) count_thread_lines(u.sp, &sel);
            break;

        default:
            (void) count_subject_lines(u.sp, &sel);
            break;
        }
        if (sel == g_sel_page.items[j].sel)
        {
            continue;
        }
        goto_xy(0, g_sel_page.items[j].line);
        g_sel_page.item_index = j;
        output_sel(g_sel_page.item_index, sel, true);
    }
    if (++g_sel_page.item_index == g_sel_page.page_item_cnt)
    {
        g_sel_page.item_index = 0;
    }
}

void output_sel(int ix, int sel, bool update)
{
    if (ix < 0)
    {
        if (g_use_sel_num)
        {
            std::putchar(' ');
        }
        std::putchar(' ');
        std::putchar(' ');
        return;
    }

    if (g_use_sel_num)
    {
        // later consider no-leading-zero option
        fmt::print("{:02}", ix + 1);
    }
    else
    {
        std::putchar(g_sel_page.chars[ix]);
    }

    switch (sel)
    {
    case 1:                   // '+'
        g_color.object(COLOR_PLUS, true);
        break;

    case 2:                   // '-'
        g_color.object(COLOR_MINUS, true);
        break;

    case 3:                   // '*'
        g_color.object(COLOR_STAR, true);
        break;

    default:
        g_color.object(COLOR_DEFAULT, true);
        sel = 0;
        break;
    }
    std::putchar(" +-*"[sel]);
    g_color.pop(); // of COLOR_PLUS/MINUS/STAR/DEFAULT

    if (update)
    {
        g_sel_page.items[ix].sel = sel;
    }
}

// Counts the number of lines needed to output a subject, including
// optional authors.
//
static int count_subject_lines(const Subject *subj, int *selptr)
{
    int sel;

    if (subj->m_flags & SF_DEL)
    {
        sel = 2;
    }
    else if (subj->m_flags & g_sel.mask)
    {
        sel = 1;
        for (Article *ap = subj->m_articles; ap; ap = ap->m_subj_next)
        {
            if ((!!(ap->m_flags & AF_UNREAD) ^ g_sel.rereading) //
                && !(ap->m_flags & g_sel.mask))
            {
                sel = 3;
                break;
            }
        }
    }
    else
    {
        sel = 0;
    }
    if (selptr)
    {
        *selptr = sel;
    }
    if (g_tc_COLS / 5 < 8)
    {
        return subj->m_misc != 0;
    }
    if (current_article_display_mode() == 'l')
    {
        return subj->m_misc;
    }
    if (current_article_display_mode() == 'm')
    {
        return (subj->m_misc <= 4 ? subj->m_misc : (subj->m_misc - 4) / 3 + 4);
    }
    return (subj->m_misc != 0);
}

// Counts the number of lines needed to output a thread, including
// optional authors.
//
static int count_thread_lines(const Subject *subj, int *selptr)
{
    int total = 0;
    const Article *thread = subj->m_thread;
    int            sel = -1;
    int            subj_sel;

    do
    {
        if (subj->m_misc)
        {
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
    } while ((subj = subj->m_next) != nullptr && subj->m_thread == thread);
    if (selptr)
    {
        *selptr = (sel < 0 ? 0 : sel);
    }
    return total;
}

// Display an article, perhaps with its author.
static void display_article(const Article *ap, int ix, int sel)
{
    int subj_width = g_tc_COLS - 5 - g_use_sel_num;
    int from_width = g_tc_COLS / 5;
    int date_width = g_tc_COLS / 5;

    maybe_eol();
    subj_width = std::max(subj_width, 32);

    output_sel(ix, sel, false);
    if (current_article_display_mode() == 's' || from_width < 8)
    {
        fmt::print("  {}\n", compress_subj(ap->m_subj->m_articles, subj_width));
    }
    else if (current_article_display_mode() == 'd')
    {
        fmt::print("{}  {}\n", ap->compress_date(date_width), compress_subj(ap, subj_width - date_width));
    }
    else
    {
        fmt::print("{}  {}\n", compress_from(ap->from_view(), from_width), compress_subj(ap, subj_width - from_width));
    }
    term_down(1);
}

// Display the given subject group, with optional authors.
static void display_subject(const Subject *subj, int ix, int sel)
{
    Article*ap;
    int     subj_width = g_tc_COLS - 8 - g_use_sel_num;
    int     from_width = g_tc_COLS / 5;
    int     date_width = g_tc_COLS / 5;
#ifdef USE_UTF_HACK
    utf_init(CHARSET_NAME_UTF8, CHARSET_NAME_UTF8); // FIXME
#endif

    maybe_eol();
    subj_width = std::max(subj_width, 32);

    int j = subj->m_misc;

    output_sel(ix, sel, false);
    if (current_article_display_mode() == 's' || from_width < 8)
    {
        fmt::print("{:3}  {}\n", j, compress_subj(subj->m_articles, subj_width));
        term_down(1);
    }
    else
    {
        Article* first_ap;
        // Find the first unread article so we get the author right
        if ((first_ap = subj->m_thread) != nullptr            //
            && (first_ap->m_subj != subj || !first_ap->m_from  //
                || (!(first_ap->m_flags & AF_UNREAD) ^ g_sel.rereading)))
        {
            first_ap = nullptr;
        }
        for (ap = subj->m_articles; ap; ap = ap->m_subj_next)
        {
            if (!!(ap->m_flags & AF_UNREAD) ^ g_sel.rereading)
            {
                break;
            }
        }
        if (!first_ap)
        {
            first_ap = ap;
        }
        if (current_article_display_mode() == 'd')
        {
            fmt::print("{}{:3}  {}\n", first_ap->compress_date(date_width), j,
                       compress_subj(first_ap, subj_width - date_width));
        }
        else
        {
            fmt::print("{}{:3}  {}\n", compress_from(first_ap ? first_ap->from_view() : std::string_view{}, from_width),
                       j, compress_subj(first_ap, subj_width - from_width));
        }
        term_down(1);
        int i = -1;
        if (current_article_display_mode() != 'd' && --j && ap)
        {
            for (; ap && j; ap = ap->m_subj_next)
            {
                if ((!(ap->m_flags & AF_UNREAD) ^ g_sel.rereading) //
                    || ap == first_ap)
                {
                    continue;
                }
                j--;
                if (i < 0)
                {
                    i = 0;
                }
                else if (current_article_display_mode() == 'm')
                {
                    if (!j)
                    {
                        if (i)
                        {
                            newline();
                        }
                    }
                    else
                    {
                        if (i == 3 || !i)
                        {
                            if (i)
                            {
                                newline();
                            }
                            if (g_term_line >= g_sel_page.max_line_cnt + 2)
                            {
                                return;
                            }
                            maybe_eol();
                            i = 1;
                        }
                        else
                        {
                            i++;
                        }
                        if (g_use_sel_num)
                        {
                            std::putchar(' ');
                        }
                        fmt::print("  {}      ", compress_from(ap ? ap->from_view() : std::string_view{}, from_width));
                        continue;
                    }
                }
                if (g_term_line >= g_sel_page.max_line_cnt + 2)
                {
                    return;
                }
                maybe_eol();
                if (g_use_sel_num)
                {
                    std::putchar(' ');
                }
                fmt::print("  {}\n", compress_from(ap ? ap->from_view() : std::string_view{}, from_width));
                term_down(1);
            }
        }
    }
}

void display_option(int op, int item_index)
{
    const OptionCatalog catalog;
    std::size_t         len;
    std::string_view    pre;
    std::string_view    item;
    std::string_view    post;
    std::string_view    val;
    std::string         quoted_val;
    if (catalog.is_group(op))
    {
        item = catalog.name(op);
        len = item.size();
        pre = "==";
        post = "==================================";
        val = "";
    }
    else
    {
        pre = "  ";
        item = catalog.name(op);
        len = item.size();
        post = "..................................";
        const OptionIndex option = catalog.option(op);
        std::optional<std::string_view> option_val = g_opt_edit.value(option);
        if (!option_val.has_value())
        {
            quoted_val = quote_string(option_value(option));
            option_val = quoted_val;
        }
        val = *option_val;
    }
    post = post.substr(len);
    output_sel(item_index, g_sel_page.items[item_index].sel, false);
    fmt::print(" {}{}{} {}\n", pre, item, post, val.substr(0, 39));
    term_down(1);
}

static void display_universal(const UniversalItem *ui)
{
    if (!ui)
    {
        fmt::print("****EMPTY****");
    }
    else
    {
        if (const UniversalNewsgroup *newsgroup = std::get_if<UniversalNewsgroup>(&ui->m_data))
        {
            // later error check the UI?
            const std::string &group_name = newsgroup->ng;
            NewsgroupData     *np = find_newsgroup(group_name.c_str());
            if (!np)
            {
                fmt::print("!!!!! could not find {}", group_name);
            }
            else
            {
                // XXX set_toread() can print sometimes...
                if (!np->m_abs_first)
                {
                    np->set_to_read(ST_LAX);
                }
                int numarts = np->m_to_read;
                if (numarts >= 0)
                {
                    fmt::print("{:5} ", numarts);
                }
                else if (numarts == TR_UNSUB)
                {
                    fmt::print("UNSUB ");
                }
                else
                {
                    fmt::print("***** ");
                }
                fmt::print("{}", group_name);
            }
            newline();
        }
        else if (std::holds_alternative<UniversalVirtualArticle>(ui->m_data))
        {
            fmt::print("      {}", ui->m_desc.empty() ? ui->univ_article_desc() : ui->m_desc);
            newline();
        }
        else if (std::holds_alternative<HelpLocation>(ui->m_data))
        {
            fmt::print("      Help on the {}", ui->univ_key_help_mode_str());
            newline();
        }
        else
        {
            fmt::print("      {}", ui->m_desc.empty() ? "[No Description]" : ui->m_desc);
            newline();
        }
    }
}

static void display_group(DataSource *dp, std::string_view group, int max_len)
{
    if (current_group_display_mode() == 's')
    {
        fmt::print("{}", group);
    }
    else
    {
        std::string                  description{dp->find_group_desc(group)};
        const std::string::size_type newline = description.find('\n');
        if (!description.empty() && description.front() != '?' && newline != std::string::npos //
            && newline != 0)
        {
            const int              display_width = g_tc_COLS - max_len - 8 - 1 - g_use_sel_num;
            std::string::size_type end = newline;
            if (display_width <= 0)
            {
                end = 0;
            }
            else
            {
                end = std::min(end, static_cast<std::string::size_type>(display_width));
            }
            description.resize(end);
            if (current_group_display_mode() == 'm')
            {
                fmt::print("{}", description);
            }
            else
            {
                fmt::print("{:<{}}{}", group, std::max(max_len, static_cast<int>(group.size()) + 1), description);
            }
        }
        else
        {
            fmt::print("{}", group);
        }
    }
    newline();
}
