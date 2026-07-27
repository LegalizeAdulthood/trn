/* ngdata.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngdata.h>

#include <config/common.h>
#include <config/env.h>
#include <config/fdio.h>
#include <config/string_case_compare.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/change_dir.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/nntp.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/size_cast.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

struct ActiveLineFields
{
    long high{};
    long low{1};
    char status{'y'};
};

std::vector<NewsgroupData>   g_newsgroup_data;           // all newsgroup data
std::vector<NewsgroupData *> g_newsgroup_order;          // current newsgroup order
NewsgroupNum                 g_newsgroup_count{};        // all newsgroups in our current newsrc(s)
NewsgroupNum                 g_newsgroup_to_read{};      //
ArticleUnread                g_newsgroup_min_to_read{1}; // == TR_ONE or TR_NONE
NewsgroupData *g_first_newsgroup{};        //
NewsgroupData *g_last_newsgroup{};         //
NewsgroupData *g_newsgroup_ptr{};          // current newsgroup data ptr
NewsgroupData *g_current_newsgroup{};      // stable current newsgroup so we can ditz with g_ngptr
NewsgroupData *g_recent_newsgroup{};       // the prior newsgroup we visited
NewsgroupData *g_start_here{};             // set to the first newsgroup with unread news on startup
NewsgroupData *g_sel_page_np{};            //
NewsgroupData *g_sel_next_np{};            //
ArticleNum     g_abs_first{};              // 1st real article in current newsgroup
ArticleNum     g_first_art{};              // minimum unread article number in newsgroup
ArticleNum     g_last_art{};               // maximum article number in newsgroup
ArticleUnread  g_missing_count{};          // for reports on missing articles
std::string    g_moderated;                //
bool           g_redirected{};             //
std::string    g_redirected_to;            //
bool           g_threaded_group{};         //
NewsgroupData *g_ng_go_newsgroup_ptr{};    //
ArticleNum     g_ng_go_art_num{};          //
bool           g_novice_delays{true};      // +f
bool           g_in_ng{};                  // true if in a newsgroup

static int  newsgroup_order_number(const NewsgroupData *np1, const NewsgroupData *np2);
static int  newsgroup_order_group_name(const NewsgroupData *np1, const NewsgroupData *np2);
static int  newsgroup_order_count(const NewsgroupData *np1, const NewsgroupData *np2);
static ActiveLineFields parse_active_line_fields(std::string_view fields, bool has_low_field);
static void renumber_newsgroup_order();

void newsgroup_data_init()
{
    g_newsgroup_order.clear();
}

std::string_view NewsgroupData::rc_line() const
{
    const std::string_view rc_line = m_rc_line;
    return rc_line.substr(0, rc_line.find('\0'));
}

char *NewsgroupData::rc_line_data()
{
    return m_rc_line.data();
}

std::string_view NewsgroupData::rc_numbers() const
{
    const std::string_view rc_line = m_rc_line;
    const std::size_t      offset = static_cast<std::size_t>(m_num_offset);
    return rc_line.substr(std::min(offset, rc_line.size()));
}

char *NewsgroupData::rc_numbers_data()
{
    return m_rc_line.data() + m_num_offset;
}

std::string_view NewsgroupData::rc_name() const
{
    return {m_rc_line.data(), static_cast<std::size_t>(m_num_offset ? m_num_offset - 1 : m_rc_line.size())};
}

void NewsgroupData::hide_subscribe_char()
{
    if (m_num_offset)
    {
        m_rc_line[m_num_offset - 1] = '\0';
    }
}

void NewsgroupData::show_subscribe_char()
{
    if (m_num_offset)
    {
        m_rc_line[m_num_offset - 1] = m_subscribe_char;
    }
}

NewsgroupData *newsgroup_first()
{
    return g_newsgroup_order.empty() ? nullptr : g_newsgroup_order.front();
}

NewsgroupData *newsgroup_last()
{
    return g_newsgroup_order.empty() ? nullptr : g_newsgroup_order.back();
}

NewsgroupData *newsgroup_next(NewsgroupData *np)
{
    return np == nullptr ? nullptr : np->m_next;
}

NewsgroupData *newsgroup_prev(NewsgroupData *np)
{
    return np == nullptr ? nullptr : np->m_prev;
}

void append_newsgroup_order(NewsgroupData *np)
{
    np->m_prev = g_last_newsgroup;
    np->m_next = nullptr;
    np->m_num = NewsgroupNum{size_cast<long>(g_newsgroup_order)};
    if (g_last_newsgroup)
    {
        g_last_newsgroup->m_next = np;
    }
    else
    {
        g_first_newsgroup = np;
    }
    g_newsgroup_order.push_back(np);
    g_last_newsgroup = np;
}

void pop_newsgroup_order()
{
    if (g_newsgroup_order.empty())
    {
        return;
    }
    NewsgroupData *np = g_newsgroup_order.back();
    g_newsgroup_order.pop_back();
    np->m_prev = nullptr;
    np->m_next = nullptr;
    sync_newsgroup_order_links();
}

bool move_newsgroup_order(NewsgroupData *np, NewsgroupNum newnum)
{
    auto it = std::find(g_newsgroup_order.begin(), g_newsgroup_order.end(), np);
    if (it == g_newsgroup_order.end())
    {
        return false;
    }
    g_newsgroup_order.erase(it);

    long target = newnum.value_of();
    if (target < 0)
    {
        target = 0;
    }
    const auto target_pos = static_cast<std::size_t>(target);
    const auto pos = std::min(target_pos, g_newsgroup_order.size());
    g_newsgroup_order.insert(g_newsgroup_order.begin() + pos, np);
    sync_newsgroup_order_links();
    renumber_newsgroup_order();
    return true;
}

static void renumber_newsgroup_order()
{
    long num = 0;
    for (NewsgroupData *np : g_newsgroup_order)
    {
        np->m_num = NewsgroupNum{num++};
    }
}

void sync_newsgroup_order_links()
{
    g_first_newsgroup = newsgroup_first();
    g_last_newsgroup = newsgroup_last();

    NewsgroupData *prev = nullptr;
    for (NewsgroupData *np : g_newsgroup_order)
    {
        np->m_prev = prev;
        if (prev != nullptr)
        {
            prev->m_next = np;
        }
        prev = np;
    }
    if (prev != nullptr)
    {
        prev->m_next = nullptr;
    }
}

// set current newsgroup

void set_newsgroup(NewsgroupData *np)
{
    g_newsgroup_ptr = np;
    if (g_newsgroup_ptr)
    {
        set_newsgroup_name(g_newsgroup_ptr->rc_line());
    }
}

int access_newsgroup()
{
    ArticleNum old_first = g_newsgroup_ptr->m_abs_first;

    if (g_data_source->m_flags & DF_REMOTE)
    {
        int ret = nntp_group(g_newsgroup_name.c_str(),g_newsgroup_ptr);
        if (ret == -2)
        {
            return -2;
        }
        if (ret <= 0)
        {
            g_newsgroup_ptr->m_to_read = TR_BOGUS;
            return 0;
        }
        g_last_art = g_newsgroup_ptr->get_newsgroup_size();
        if (g_last_art < 0) // Impossible...
        {
            return 0;
        }
        g_abs_first = g_newsgroup_ptr->m_abs_first;
        if (g_abs_first > old_first)
        {
            g_newsgroup_ptr->check_expired(g_abs_first);
        }
    }
    else
    {
        if (eaccess(g_newsgroup_dir, 5))                 // directory read protected?
        {
            if (eaccess(g_newsgroup_dir, 0))
            {
                if (g_verbose)
                {
                    std::printf("\nNewsgroup %s does not have a spool directory!\n", g_newsgroup_name.c_str());
                }
                else
                {
                    std::printf("\nNo spool for %s!\n", g_newsgroup_name.c_str());
                }
                term_down(2);
            }
            else
            {
                if (g_verbose)
                {
                    std::printf("\nNewsgroup %s is not currently accessible.\n", g_newsgroup_name.c_str());
                }
                else
                {
                    std::printf("\n%s not readable.\n", g_newsgroup_name.c_str());
                }
                term_down(2);
            }
            // make this newsgroup temporarily invisible
            g_newsgroup_ptr->m_to_read = TR_NONE;
            return 0;
        }

        // chdir to newsgroup subdirectory
        if (change_dir(g_newsgroup_dir))
        {
            fmt::print("Can't chdir to directory {}\n", g_newsgroup_dir);
            return 0;
        }
        g_last_art = g_newsgroup_ptr->get_newsgroup_size();
        if (g_last_art < 0) // Impossible...
        {
            return 0;
        }
        g_abs_first = g_newsgroup_ptr->m_abs_first;
    }

    g_dm_count = 0;
    g_missing_count = 0;
    g_in_ng = true;                     // tell the world we are here

    build_cache();
    return 1;
}

void chdir_news_dir()
{
    if (change_dir(g_data_source->m_spool_dir.c_str()) ||
        (!(g_data_source->m_flags & DF_REMOTE) && change_dir(g_newsgroup_dir)))
    {
        fmt::print("Can't chdir to directory {}\n", g_newsgroup_dir);
        sig_catcher(0);
    }
}

void grow_newsgroup(ArticleNum new_last)
{
    g_force_grow = false;
    if (new_last > g_last_art)
    {
        ArticleNum tmpart = g_art;
        g_newsgroup_ptr->m_to_read += (ArticleUnread)(new_last-g_last_art).value_of();
        ArticleNum tmpfirst = article_after(g_last_art);
        // Increase the size of article scan arrays.
        sa_grow(g_last_art,new_last);
        do
        {
            ++g_last_art;
            article_ptr(g_last_art)->m_flags |= AF_EXISTS|AF_UNREAD;
        } while (g_last_art < new_last);
        thread_grow();
        // Score all new articles now just in case they weren't done above.
        sc_fill_score_list(tmpfirst,new_last);
        const std::string message =
            g_verbose ? fmt::format("{} more article{} arrived -- processing memorized commands...\n\n",
                                    g_last_art.value_of() - tmpfirst.value_of() + 1,
                                    g_last_art > tmpfirst ? "s have" : " has")
                      : "More news -- auto-processing...\n\n";
        term_down(2);
        if (g_kf_state & KFS_NORMAL_LINES)
        {
            bool forcelast_save = g_force_last;
            Article* artp_save = g_artp;
            kill_unwanted(tmpfirst, message, true);
            g_artp = artp_save;
            g_force_last = forcelast_save;
        }
        g_art = tmpart;
    }
}

static int newsgroup_order_number(const NewsgroupData *np1, const NewsgroupData *np2)
{
    return (int) (np1->m_num.value_of() - np2->m_num.value_of()) * g_sel_direction;
}

static int newsgroup_order_group_name(const NewsgroupData *np1, const NewsgroupData *np2)
{
    return string_case_compare(np1->m_rc_line, np2->m_rc_line) * g_sel_direction;
}

static int newsgroup_order_count(const NewsgroupData *np1, const NewsgroupData *np2)
{
    int eq = (int) (np1->m_to_read - np2->m_to_read);
    if (eq != 0)
    {
        return eq * g_sel_direction;
    }
    return (int) (np1->m_num.value_of() - np2->m_num.value_of());
}

// Sort the newsgroups into the chosen order.
void sort_newsgroups()
{
    int (*sort_procedure)(const NewsgroupData *np1, const NewsgroupData *np2);

    // If we don't have at least two newsgroups, we're done!
    if (g_newsgroup_order.size() < 2)
    {
        return;
    }

    switch (g_sel_sort)
    {
    case SS_NATURAL:
    default:
        sort_procedure = newsgroup_order_number;
        break;

    case SS_STRING:
        sort_procedure = newsgroup_order_group_name;
        break;

    case SS_COUNT:
        sort_procedure = newsgroup_order_count;
        break;
    }

    std::sort(g_newsgroup_order.begin(), g_newsgroup_order.end(),
              [sort_procedure](const NewsgroupData *np1, const NewsgroupData *np2)
              { return sort_procedure(np1, np2) < 0; });
    sync_newsgroup_order_links();
}

void newsgroup_skip()
{
    if (g_data_source->m_flags & DF_REMOTE)
    {
        clear();
        if (g_verbose)
        {
            std::fputs("Skipping unavailable article\n", stdout);
        }
        else
        {
            std::fputs("Skipping\n", stdout);
        }
        term_down(1);
        if (g_novice_delays)
        {
            pad(g_just_a_sec/3);
            sleep(1);
        }
        g_art = article_next(g_art);
        g_artp = article_ptr(g_art);
        do
        {
            // tries to grab PREFETCH_SIZE XHDRS, flagging missing articles
            prefetch_subj(g_art);
            ArticleNum artnum = g_art + ArticleNum{PREFETCH_SIZE - 1};
            artnum = std::min(artnum, g_last_art);
            while (g_art <= artnum)
            {
                if (g_artp->m_flags & AF_EXISTS)
                {
                    return;
                }
                g_art = article_next(g_art);
                g_artp = article_ptr(g_art);
            }
        } while (g_art <= g_last_art);
    }
    else
    {
        if (errno != ENOENT)    // has it not been deleted?
        {
            clear();
            if (g_verbose)
            {
                std::printf("\n(Article %ld exists but is unreadable.)\n", (long) g_art.value_of());
            }
            else
            {
                std::printf("\n(%ld unreadable.)\n", (long) g_art.value_of());
            }
            term_down(2);
            if (g_novice_delays)
            {
                pad(g_just_a_sec);
                sleep(2);
            }
        }
        inc_article(g_selected_only,false); // try next article
    }
}

// find the maximum article number of a newsgroup
//
ArticleNum NewsgroupData::get_newsgroup_size()
{
    long last;
    long first;
    char ch;

    const std::string_view group_name{rc_name()};
    int                    len = static_cast<int>(group_name.size());

    const std::string active_line = m_rc->data_source->find_active_group(group_name, m_ng_max);
    if (active_line.empty())
    {
        if (m_subscribe_char == ':')
        {
            m_subscribe_char = UNSUBSCRIBED_CHAR;
            m_rc->flags |= RF_RC_CHANGED;
            --g_newsgroup_to_read;
        }
        return ArticleNum{TR_BOGUS};
    }

    const std::string_view fields = std::string_view{active_line}.substr(static_cast<std::size_t>(len) + 1);
#ifdef ANCIENT_NEWS
    const ActiveLineFields active_fields = parse_active_line_fields(fields, false);
    first = 1;
#else
    const ActiveLineFields active_fields = parse_active_line_fields(fields, true);
    first = active_fields.low;
#endif
    last = active_fields.high;
    ch = active_fields.status;
    if (!m_abs_first)
    {
        m_abs_first = ArticleNum{first};
    }
    if (!g_in_ng)
    {
        if (g_redirected)
        {
            g_redirected = false;
            g_redirected_to.clear();
        }
        switch (ch)
        {
        case 'n':
            g_moderated = get_env_var("NOPOSTRING", " (no posting)");
            break;

        case 'm':
            g_moderated = get_env_var("MODSTRING", " (moderated)");
            break;

        case 'x':
            g_redirected = true;
            g_redirected_to.clear();
            g_moderated = " (DISABLED)";
            break;

        case '=':
        {
            std::string_view redirect = active_line;
            if (!redirect.empty() && redirect.back() == '\n')
            {
                redirect.remove_suffix(1);
            }
            g_redirected = true;
            g_redirected_to.assign(redirect.substr(redirect.rfind('=') + 1));
            g_moderated = " (REDIRECTED)";
            break;
        }

        default:
            g_moderated.clear();
            break;
        }
    }
    if (last <= m_ng_max.value_of())
    {
        return m_ng_max;
    }
    return m_ng_max = ArticleNum{last};
}

static void skip_active_field_space(std::string_view &text)
{
    const std::size_t non_space = text.find_first_not_of(" \f\n\r\t\v");
    text.remove_prefix(non_space == std::string_view::npos ? text.size() : non_space);
}

static bool read_active_field_number(std::string_view &text, long &value)
{
    skip_active_field_space(text);
    const char                  *first = text.data();
    const char                  *last = first + text.size();
    long                         parsed{};
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc{})
    {
        return false;
    }
    value = parsed;
    text.remove_prefix(static_cast<std::size_t>(result.ptr - first));
    return true;
}

static ActiveLineFields parse_active_line_fields(std::string_view fields, bool has_low_field)
{
    ActiveLineFields result;
    if (!read_active_field_number(fields, result.high))
    {
        return result;
    }
    if (has_low_field && !read_active_field_number(fields, result.low))
    {
        return result;
    }
    skip_active_field_space(fields);
    if (!fields.empty())
    {
        result.status = fields.front();
    }
    return result;
}
