/* rcstuff.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rcstuff.h>

#include <config/common.h>
#include <config/env.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <trn/autosub.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/IniDocument.h>
#include <trn/IniSectionValues.h>
#include <trn/init.h>
#include <trn/last.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/only.h>
#include <trn/RcGroupConfig.h>
#include <trn/rcln.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/size_cast.h>
#include <trn/smisc.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

HashTable           *g_newsrc_hash{};
Multirc             *g_sel_page_mp{};
Multirc             *g_sel_next_mp{};
std::vector<Multirc> g_multircs;                       // all Multircs
Multirc             *g_multirc{};                      // the current Multirc
bool                 g_paranoid{};                     // did we detect some inconsistency in .newsrc?
AddNewType           g_add_new_by_default{ADDNEW_ASK}; //
bool                 g_check_flag{};                   // -c
bool                 g_suppress_cn{};                  // -s
int                  g_countdown{5};                   // how many lines to list before invoking -s
bool                 g_fuzzy_get{};                    // -G
bool                 g_append_unsub{};                 // -I

static bool s_found_any{};

static constexpr std::ptrdiff_t NO_NEWSGROUP_INDEX = -1;
static constexpr std::size_t    NEWSGROUP_DATA_RESERVE_SLACK = 4096;

static void           clear_newsgroup_item(NewsgroupData *np);
static void           ensure_newsgroup_data_capacity(std::size_t count);
static NewsgroupData *append_newsgroup_data();
static std::size_t    count_newsrc_lines(const fs::path &path);
static Multirc       *ensure_multirc(int num);
static Newsrc        *new_newsrc(const RcGroupConfig &config);
static bool           lock_newsrc(Newsrc *rp);
static void           unlock_newsrc(Newsrc *rp);
static bool           open_newsrc(Newsrc *rp);
static void           parse_rcline(NewsgroupData *np);
static void           reserve_newsgroup_data(Multirc *mptr);
static NewsgroupData *add_newsgroup(Newsrc *rp, std::string_view ngn, char_int c);
static void           set_hash(NewsgroupData *np);
static void           rebuild_newsgroup_hash();
static std::ptrdiff_t newsgroup_pointer_index(NewsgroupData *base, std::size_t count, NewsgroupData *np);
static NewsgroupData *newsgroup_pointer_from_index(NewsgroupData *base, std::ptrdiff_t index);
static int            rcline_cmp(std::string_view key, HashDatum data);
static char           command_char(std::string_view command);
static std::string    read_newsrc_prompt_command(std::string_view prompt, MinorMode newmode, std::string_view dflt);
static void           print_prompt_command(std::string_view command);

static void print_cant_recreate(const fs::path &name)
{
    fmt::print("Can't recreate {} -- restoring older version.\n"
               "Perhaps you are near or over quota?\n",
               name.generic_string());
}

static void ensure_newsgroup_data_capacity(std::size_t count)
{
    if (count <= g_newsgroup_data.capacity())
    {
        return;
    }

    const std::size_t old_count = g_newsgroup_data.size();
    if (old_count == 0)
    {
        g_newsgroup_data.reserve(count);
        return;
    }

    NewsgroupData              *old_base = g_newsgroup_data.data();
    const std::ptrdiff_t        ptr_index = newsgroup_pointer_index(old_base, old_count, g_newsgroup_ptr);
    const std::ptrdiff_t        current_index = newsgroup_pointer_index(old_base, old_count, g_current_newsgroup);
    const std::ptrdiff_t        recent_index = newsgroup_pointer_index(old_base, old_count, g_recent_newsgroup);
    const std::ptrdiff_t        start_index = newsgroup_pointer_index(old_base, old_count, g_start_here);
    const std::ptrdiff_t        sel_page_index = newsgroup_pointer_index(old_base, old_count, g_sel_page_np);
    const std::ptrdiff_t        sel_next_index = newsgroup_pointer_index(old_base, old_count, g_sel_next_np);
    const std::ptrdiff_t        go_index = newsgroup_pointer_index(old_base, old_count, g_ng_go_newsgroup_ptr);
    std::vector<std::ptrdiff_t> order_indexes;
    order_indexes.reserve(g_newsgroup_order.size());
    for (NewsgroupData *np : g_newsgroup_order)
    {
        order_indexes.push_back(newsgroup_pointer_index(old_base, old_count, np));
    }
    std::vector<std::ptrdiff_t> selection_indexes;
    if (g_sel_mode == SM_NEWSGROUP)
    {
        for (int i = 0; i != g_sel_page_item_cnt; ++i)
        {
            selection_indexes.push_back(newsgroup_pointer_index(old_base, old_count, g_sel_items[i].u.np));
        }
    }

    const std::size_t capacity = g_newsgroup_data.capacity();
    g_newsgroup_data.reserve(std::max(count, capacity + std::max(capacity, NEWSGROUP_DATA_RESERVE_SLACK)));

    NewsgroupData *new_base = g_newsgroup_data.data();
    for (std::size_t i = 0; i != g_newsgroup_order.size(); ++i)
    {
        g_newsgroup_order[i] = newsgroup_pointer_from_index(new_base, order_indexes[i]);
    }
    sync_newsgroup_order_links();
    g_newsgroup_ptr = newsgroup_pointer_from_index(new_base, ptr_index);
    g_current_newsgroup = newsgroup_pointer_from_index(new_base, current_index);
    g_recent_newsgroup = newsgroup_pointer_from_index(new_base, recent_index);
    g_start_here = newsgroup_pointer_from_index(new_base, start_index);
    g_sel_page_np = newsgroup_pointer_from_index(new_base, sel_page_index);
    g_sel_next_np = newsgroup_pointer_from_index(new_base, sel_next_index);
    g_ng_go_newsgroup_ptr = newsgroup_pointer_from_index(new_base, go_index);
    if (g_sel_mode == SM_NEWSGROUP)
    {
        for (int i = 0; i != g_sel_page_item_cnt; ++i)
        {
            g_sel_items[i].u.np = newsgroup_pointer_from_index(new_base, selection_indexes[i]);
        }
    }
    rebuild_newsgroup_hash();
}

static NewsgroupData *append_newsgroup_data()
{
    ensure_newsgroup_data_capacity(g_newsgroup_data.size() + 1);
    g_newsgroup_data.emplace_back();
    NewsgroupData *np = &g_newsgroup_data.back();
    np->m_num = NewsgroupNum{size_cast<long>(g_newsgroup_data) - 1};
    return np;
}

static std::size_t count_newsrc_lines(const fs::path &path)
{
    std::FILE *fp = std::fopen(path.string().c_str(), "r");
    if (fp == nullptr)
    {
        return 200;
    }

    std::size_t count = 0;
    int         ch;
    bool        saw_any = false;
    bool        last_was_newline = true;
    while ((ch = std::fgetc(fp)) != EOF)
    {
        saw_any = true;
        last_was_newline = ch == '\n';
        if (last_was_newline)
        {
            ++count;
        }
    }
    if (saw_any && !last_was_newline)
    {
        ++count;
    }
    std::fclose(fp);
    return count;
}

static void reserve_newsgroup_data(Multirc *mptr)
{
    if (!g_newsgroup_data.empty())
    {
        return;
    }

    std::size_t count = NEWSGROUP_DATA_RESERVE_SLACK;
    for (Newsrc *rp = mptr->m_first; rp; rp = rp->next)
    {
        count += count_newsrc_lines(rp->name);
    }
    ensure_newsgroup_data_capacity(count);
}

static std::ptrdiff_t newsgroup_pointer_index(NewsgroupData *base, std::size_t count, NewsgroupData *np)
{
    if (np == nullptr)
    {
        return NO_NEWSGROUP_INDEX;
    }
    if (base == nullptr || count == 0)
    {
        std::fputs("Newsgroup pointer outside data storage.\n", stdout);
        finalize(1);
    }

    std::ptrdiff_t index = np - base;
    if (index < 0 || static_cast<std::size_t>(index) >= count)
    {
        std::fputs("Newsgroup pointer outside data storage.\n", stdout);
        finalize(1);
    }
    return index;
}

static NewsgroupData *newsgroup_pointer_from_index(NewsgroupData *base, std::ptrdiff_t index)
{
    return index == NO_NEWSGROUP_INDEX ? nullptr : base + index;
}

static Multirc *ensure_multirc(int num)
{
    auto it = std::lower_bound(g_multircs.begin(), g_multircs.end(), num,
                               [](const Multirc &mp, int mp_num) { return mp.m_num < mp_num; });
    if (it == g_multircs.end() || it->m_num != num)
    {
        it = g_multircs.insert(it, Multirc{});
        it->m_num = num;
    }
    return &*it;
}

static Multirc *rcstuff_init_data()
{
    bool first_group_found = false;
    int  first_group_num = 0;

    g_multircs.clear();
    g_multircs.reserve(20);

    if (!g_trn_access_text.empty())
    {
        IniDocument      document{g_trn_access_text, TRNACCESS};
        IniSectionValues values;
        for (const IniSection section : document)
        {
            if (section.has_condition())
            {
                if (!check_ini_cond(section.condition()))
                {
                    continue;
                }
            }

            const std::string_view section_name = section.name();
            if (!string_case_equal(section_name.substr(0, 6), "group "))
            {
                continue;
            }
            std::string_view                       group_index = section_name.substr(6);
            const std::string_view::const_iterator group_index_start =
                std::find_if_not(group_index.begin(), group_index.end(),
                                 [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
            group_index.remove_prefix(static_cast<std::size_t>(group_index_start - group_index.begin()));
            int i{};
            (void) std::from_chars(group_index.data(), group_index.data() + group_index.size(), i);
            i = std::max(i, 0);
            parse_ini_section(section, RcGroupConfig::schema(), values);
            Newsrc *rp = new_newsrc(RcGroupConfig::from(values));
            if (rp)
            {
                Multirc *mp = ensure_multirc(i);
                Newsrc  *prev_rp = mp->m_first;
                if (!prev_rp)
                {
                    mp->m_first = rp;
                }
                else
                {
                    while (prev_rp->next)
                    {
                        prev_rp = prev_rp->next;
                    }
                    prev_rp->next = rp;
                }
                if (!first_group_found)
                {
                    first_group_found = true;
                    first_group_num = i;
                }
            }
        }
        g_trn_access_text.clear();
    }
    return first_group_found ? multirc_ptr(first_group_num) : nullptr;
}

bool rcstuff_init()
{
    Multirc *mptr = rcstuff_init_data();

    if (g_use_newsrc_selector && !g_check_flag)
    {
        return true;
    }

    s_found_any = false;
    if (mptr && !mptr->use_multirc())
    {
        mptr->use_next_multirc();
    }
    if (!g_multirc)
    {
        mptr = ensure_multirc(0);
        RcGroupConfig config;
        config.set_id("default");
        mptr->m_first = new_newsrc(config);
        if (!mptr->use_multirc())
        {
            std::printf("Couldn't open any newsrc groups.  Is your access file ok?\n");
            finalize(1);
        }
    }
    if (g_check_flag)                    // were we just checking?
    {
        finalize(s_found_any);           // tell them what we found
    }
    return s_found_any;
}

void rcstuff_final()
{
    if (g_multirc)
    {
        unuse_multirc(g_multirc);
        g_multirc = nullptr;
    }
    g_multircs.clear();
}

static Newsrc *new_newsrc(const RcGroupConfig &config)
{
    const std::string_view name = config.id();
    if (name.empty())
    {
        return nullptr;
    }

    std::string newsrc{config.newsrc()};
    if (newsrc.empty())
    {
        newsrc = get_env_var("NEWSRC", RCNAME);
    }

    const std::string_view add_ok = config.add_groups();
    DataSource *dp = get_data_source(name);
    if (!dp)
    {
        return nullptr;
    }

    Newsrc *rp = new Newsrc{};
    rp->data_source = dp;
    rp->name = file_exp(newsrc);
    rp->old_name = rp->name;
    rp->old_name += ".old";
    rp->new_name = rp->name;
    rp->new_name += ".new";

    switch (add_ok.empty() ? 'y' : add_ok.front())
    {
    case 'n':
    case 'N':
        break;

    default:
        if (dp->m_flags & DF_ADD_OK)
        {
            rp->flags |= RF_ADD_NEW_GROUPS;
        }
        // FALL THROUGH

    case 'm':
    case 'M':
        rp->flags |= RF_ADD_GROUPS;
        break;
    }
    return rp;
}

bool Multirc::use_multirc()
{
    bool had_trouble = false;
    bool had_success = false;

    reserve_newsgroup_data(this);
    for (Newsrc *rp = m_first; rp; rp = rp->next)
    {
        if ((rp->data_source->m_flags & DF_UNAVAILABLE) || !lock_newsrc(rp) //
            || !rp->data_source->open() || !open_newsrc(rp))
        {
            unlock_newsrc(rp);
            had_trouble = true;
        }
        else
        {
            rp->data_source->m_flags |= DF_ACTIVE;
            rp->flags |= RF_ACTIVE;
            had_success = true;
        }
    }
    if (had_trouble)
    {
        get_anything();
    }
    if (!had_success)
    {
        return false;
    }
    g_multirc = this;
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
    return true;
}

// TODO: why does this check mptr for nullptr?
//
void unuse_multirc(Multirc *mptr)
{
    if (!mptr)
    {
        return;
    }

    write_newsrcs(mptr);

    for (Newsrc *rp = mptr->m_first; rp; rp = rp->next)
    {
        unlock_newsrc(rp);
        rp->flags &= ~RF_ACTIVE;
        rp->data_source->m_flags &= ~DF_ACTIVE;
    }
    if (g_newsrc_hash)
    {
        close_cache();
        hash_destroy(g_newsrc_hash);
        g_newsrc_hash = nullptr;
        for (NewsgroupData &np : g_newsgroup_data)
        {
            clear_newsgroup_item(&np);
        }
        g_newsgroup_data.clear();
        g_newsgroup_order.clear();
        g_first_newsgroup = nullptr;
        g_last_newsgroup = nullptr;
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
        g_recent_newsgroup = nullptr;
        g_start_here = nullptr;
        g_sel_page_np = nullptr;
        g_sel_next_np = nullptr;
        g_ng_go_newsgroup_ptr = nullptr;
    }
    g_newsgroup_count = NewsgroupNum{};
    g_newsgroup_to_read = NewsgroupNum{};
    g_multirc = nullptr;
}

bool Multirc::use_next_multirc()
{
    Multirc *mp = multirc_ptr(m_num);

    unuse_multirc(this);

    while (true)
    {
        mp = multirc_next(mp);
        if (!mp)
        {
            mp = multirc_low();
        }
        if (mp == this)
        {
            use_multirc();
            return false;
        }
        if (mp->use_multirc())
        {
            break;
        }
    }
    return true;
}

bool Multirc::use_prev_multirc()
{
    Multirc *mp = multirc_ptr(m_num);

    unuse_multirc(this);

    while (true)
    {
        mp = multirc_prev(mp);
        if (!mp)
        {
            mp = multirc_high();
        }
        if (mp == this)
        {
            use_multirc();
            return false;
        }
        if (mp->use_multirc())
        {
            break;
        }
    }
    return true;
}

std::string Multirc::multirc_name() const
{
    if (m_first->next)
    {
        return "<each-newsrc>";
    }
    return m_first->name.filename().generic_string();
}

static void clear_newsgroup_item(NewsgroupData *np)
{
    np->m_rc_line.clear();
}

// make sure there is no trn out there reading this newsrc

static bool lock_newsrc(Newsrc *rp)
{
    long processnum = 0;

    if (g_check_flag)
    {
        return true;
    }

    const std::string rcname = file_exp(RCNAME);
    if (rp->name == rcname)
    {
        rp->lock_name = file_exp(LOCKNAME);
    }
    else
    {
        rp->info_name = rp->name;
        rp->info_name += ".info";
        rp->lock_name = rp->name;
        rp->lock_name += ".LOCK";
    }

    std::string runninghost;
    if (std::FILE *fp = std::fopen(rp->lock_name.string().c_str(), "r"))
    {
        const std::string pid_line = get_a_line(fp);
        if (!pid_line.empty())
        {
            std::string_view                       pid_text{pid_line};
            const std::string_view::const_iterator pid_text_start = std::find_if_not(
                pid_text.begin(), pid_text.end(), [](char ch) { return std::isspace(static_cast<unsigned char>(ch)); });
            pid_text.remove_prefix(static_cast<std::size_t>(pid_text_start - pid_text.begin()));
            (void) std::from_chars(pid_text.data(), pid_text.data() + pid_text.size(), processnum);
            runninghost = get_a_line(fp);
            if (!runninghost.empty() && runninghost.back() == '\n')
            {
                runninghost.pop_back();
            }
        }
        std::fclose(fp);
    }
    if (processnum)
    {
#ifndef MSDOS
        if (g_verbose)
        {
            fmt::print("\nThe requested newsrc is locked by process {} on host {}.\n", processnum, runninghost);
        }
        else
        {
            fmt::print("\nNewsrc locked by {} on host {}.\n", processnum, runninghost);
        }
        term_down(2);
        if (g_local_host != runninghost)
        {
            if (g_verbose)
            {
                fmt::print("\n"
                           "Since that's not the same host as this one ({}), we must\n"
                           "assume that process still exists.  To override this check, remove\n"
                           "the lock file: {}\n",
                           g_local_host, rp->lock_name.generic_string());
            }
            else
            {
                fmt::print("\nThis host ({}) doesn't match.\nCan't unlock {}.\n", g_local_host,
                           rp->lock_name.generic_string());
            }
            term_down(2);
            if (g_bizarre)
            {
                reset_tty();
            }
            finalize(0);
        }
        if (processnum == g_our_pid)
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "Hey, that *my* pid!  Your access file is trying to use the same newsrc\n"
                       "more than once.\n");
            }
            else
            {
                std::printf("\nAccess file error (our pid detected).\n");
            }
            term_down(2);
            return false;
        }
        if (kill(processnum, 0) != 0)
        {
            // Process is apparently gone
            sleep(2);
            if (g_verbose)
            {
                std::fputs("\n"
                      "That process does not seem to exist anymore.  The count of read articles\n"
                      "may be incorrect in the last newsgroup accessed by that other (defunct)\n"
                      "process.\n\n",
                      stdout);
            }
            else
            {
                std::fputs("\nProcess crashed.\n",stdout);
            }
            if (!g_last_newsgroup_name.empty())
            {
                if (g_verbose)
                {
                    std::printf("(The last newsgroup accessed was %s.)\n\n",
                           g_last_newsgroup_name.c_str());
                }
                else
                {
                    std::printf("(In %s.)\n\n",g_last_newsgroup_name.c_str());
                }
            }
            term_down(2);
            get_anything();
            newline();
        }
        else
        {
            if (g_verbose)
            {
                fmt::print("\n"
                           "It looks like that process still exists.  To override this, remove\n"
                           "the lock file: {}\n",
                           rp->lock_name.generic_string());
            }
            else
            {
                fmt::print("\nCan't unlock {}.\n", rp->lock_name.generic_string());
            }
            term_down(2);
            if (g_bizarre)
            {
                reset_tty();
            }
            finalize(0);
        }
#endif
    }
    std::FILE *fp = std::fopen(rp->lock_name.string().c_str(), "w");
    if (fp == nullptr)
    {
        fmt::print("Can't create {}\n", rp->lock_name.generic_string());
        sig_catcher(0);
    }
    fmt::print(fp, "{}\n{}\n", g_our_pid, g_local_host);
    std::fclose(fp);
    return true;
}

static void unlock_newsrc(Newsrc *rp)
{
    rp->info_name.clear();
    if (!rp->lock_name.empty())
    {
        std::error_code error;
        fs::remove(rp->lock_name, error);
        rp->lock_name.clear();
    }
}

static bool open_newsrc(Newsrc *rp)
{
    // make sure the .newsrc file exists

    std::FILE *rcfp = std::fopen(rp->name.string().c_str(), "r");
    if (rcfp == nullptr)
    {
        rcfp = std::fopen(rp->name.string().c_str(), "w+");
        if (rcfp == nullptr)
        {
            fmt::print("\nCan't create {}.\n", rp->name.generic_string());
            term_down(2);
            return false;
        }
        constexpr std::string_view subscriptions{SUBSCRIPTIONS};
        if ((rp->data_source->m_flags & DF_REMOTE) //
            && nntp_list("SUBSCRIPTIONS", "") == 1)
        {
            std::string subscription_line;
            while (nntp_gets(subscription_line, NNTP_STRLEN) != NGSR_ERROR)
            {
                if (nntp_at_list_end(subscription_line))
                {
                    break;
                }
                fmt::print(rcfp, "{}\n", subscription_line);
            }
        }
        else if (!subscriptions.empty())
        {
            if (std::FILE *fp = std::fopen(file_exp(subscriptions).c_str(), "r"))
            {
                for (std::string subscription_line = get_a_line(fp); !subscription_line.empty();
                     subscription_line = get_a_line(fp))
                {
                    fmt::print(rcfp, "{}", subscription_line);
                }
                std::fclose(fp);
            }
        }
        std::fseek(rcfp, 0L, 0);
    }
    else
    {
        // File exists; if zero length and backup isn't, complain
        stat_t newsrc_stat{};
        if (fstat(fileno(rcfp), &newsrc_stat) < 0)
        {
            std::perror(rp->name.string().c_str());
            return false;
        }
        if (newsrc_stat.st_size == 0 //
            && stat(rp->old_name.string().c_str(), &newsrc_stat) >= 0 && newsrc_stat.st_size > 0)
        {
            fmt::print("Warning: {} is zero length but {} is not.\n", rp->name.generic_string(),
                       rp->old_name.generic_string());
            fmt::print("Either recover your newsrc or else remove the backup copy.\n");
            term_down(2);
            return false;
        }
        // unlink backup file name and backup current name
        std::error_code error;
        fs::remove(rp->old_name, error);
        error.clear();
        fs::copy_file(rp->name, rp->old_name, fs::copy_options::overwrite_existing, error);
        if (error)
        {
            fmt::print("Can't copy backup ({}) from .newsrc ({})\n", rp->old_name.string(), rp->name.string());
            finalize(1);
            return false;
        }
    }

    if (g_newsrc_hash == nullptr)
    {
        g_newsrc_hash = hash_create(3001, rcline_cmp);
    }

    // read in the .newsrc file

    while (true)
    {
        std::string line = get_a_line(rcfp);
        if (line.empty())
        {
            break;
        }
        if (line.size() <= 1) // only a newline???
        {
            continue;
        }
        NewsgroupData *np = append_newsgroup_data();
        append_newsgroup_order(np);
        np->m_rc = rp;
        ++g_newsgroup_count;
        if (line.back() == '\n')
        {
            line.pop_back(); // wipe out newline
        }
        np->m_rc_line = std::move(line);
        if (const std::string_view rc_line{np->m_rc_line}; //
            is_hor_space(rc_line.front())                  //
            || rc_line.substr(0, 7) == "options")          // non-useful line?
        {
            np->m_to_read = TR_JUNK;
            np->m_subscribe_char = ' ';
            np->m_num_offset = 0;
            continue;
        }
        parse_rcline(np);
        HashDatum data = hash_fetch(g_newsrc_hash, np->rc_name());
        if (data.dat_ptr)
        {
            np->m_to_read = TR_IGNORE;
            continue;
        }
        if (np->m_subscribe_char == UNSUBSCRIBED_CHAR)
        {
            np->m_to_read = TR_UNSUB;
            set_hash(np);
            continue;
        }
        ++g_newsgroup_to_read;

        // now find out how much there is to read

        if (!in_list(np->rc_line()) || (g_suppress_cn && s_found_any && !g_paranoid))
        {
            np->m_to_read = TR_NONE; // no need to calculate now
        }
        else
        {
            np->set_to_read(ST_LAX);
        }
        if (np->m_to_read > TR_NONE) // anything unread?
        {
            if (!s_found_any)
            {
                g_start_here = np;
                s_found_any = true; // remember that fact
            }
            if (g_suppress_cn) // if no listing desired
            {
                if (g_check_flag) // if that is all they wanted
                {
                    finalize(1); // then bomb out
                }
            }
            else
            {
                if (g_verbose)
                {
                    fmt::print("Unread news in {:<40} {:5} article{}\n", np->rc_line(),
                               static_cast<long>(np->m_to_read), plural(np->m_to_read));
                }
                else
                {
                    fmt::print("{}: {} article{}\n", np->rc_line(), static_cast<long>(np->m_to_read),
                               plural(np->m_to_read));
                }
                term_down(1);
                if (g_int_count)
                {
                    g_countdown = 1;
                    g_int_count = 0;
                }
                if (g_countdown)
                {
                    if (!--g_countdown)
                    {
                        std::fputs("etc.\n",stdout);
                        if (g_check_flag)
                        {
                            finalize(1);
                        }
                        g_suppress_cn = true;
                    }
                }
            }
        }
        set_hash(np);
    }
    std::fclose(rcfp);                       // close .newsrc
    if (!rp->info_name.empty())
    {
        std::FILE *info = std::fopen(rp->info_name.string().c_str(), "r");
        if (info != nullptr)
        {
            const std::string info_line = get_a_line(info);
            if (!info_line.empty())
            {
                long             actnum;
                long             descnum;
                std::string_view last_group_line{info_line};
                if (last_group_line.back() == '\n')
                {
                    last_group_line.remove_suffix(1);
                }
                const std::size_t separator = last_group_line.find(':');
                if (separator != std::string_view::npos && separator + 2 < last_group_line.size() &&
                    last_group_line[separator + 1] == ' ')
                {
                    g_last_newsgroup_name = last_group_line.substr(separator + 2);
                }
                if (std::fscanf(info, "New-Group-State: %ld,%ld,%ld", //
                                &g_last_new_time, &actnum, &descnum) == 3)
                {
                    rp->data_source->m_act_sf.m_recent_cnt = actnum;
                    rp->data_source->m_desc_sf.m_recent_cnt = descnum;
                }
            }
            std::fclose(info);
        }
    }
    else
    {
        read_last();
        if (rp->data_source->m_flags & DF_REMOTE)
        {
            rp->data_source->m_act_sf.m_recent_cnt = g_last_active_size;
            rp->data_source->m_desc_sf.m_recent_cnt = g_last_extra_num;
        }
        else
        {
            rp->data_source->m_act_sf.m_recent_cnt = g_last_extra_num;
            rp->data_source->m_desc_sf.m_recent_cnt = 0;
        }
    }
    rp->data_source->m_last_new_group = g_last_new_time;

    if (g_paranoid && !g_check_flag)
    {
        cleanup_newsrc(rp);
    }
    return true;
}

static void parse_rcline(NewsgroupData *np)
{
    std::string                &rc_line = np->m_rc_line;
    const std::string::iterator delimiter =
        std::find_if(rc_line.begin(), rc_line.end(),
                     [](unsigned char ch) { return ch == ':' || ch == UNSUBSCRIBED_CHAR || std::isspace(ch); });
    const std::size_t delimiter_pos = static_cast<std::size_t>(std::distance(rc_line.begin(), delimiter));
    if ((delimiter == rc_line.end() || std::isspace(static_cast<unsigned char>(*delimiter))) && !g_check_flag)
    {
        rc_line.resize(delimiter_pos);
        rc_line += ": ";
    }
    if (delimiter_pos + 2 < rc_line.size() && rc_line[delimiter_pos] == ':' && rc_line[delimiter_pos + 1] &&
        rc_line[delimiter_pos + 2] == '0')
    {
        np->m_flags |= NF_UNTHREADED;
        rc_line[delimiter_pos + 2] = '1';
    }
    np->m_subscribe_char = delimiter_pos < rc_line.size() ? rc_line[delimiter_pos] : '\0'; // salt away the : or !
    np->m_num_offset = static_cast<int>(delimiter_pos + 1); // remember where the numbers are
    if (delimiter_pos < rc_line.size())
    {
        rc_line[delimiter_pos] = '\0'; // null terminate newsgroup name
    }
}

void NewsgroupData::abandon_newsgroup()
{
    std::string some_buf;

    // open newsrc backup copy and try to find the prior value for the group.
    std::FILE *rcfp = std::fopen(m_rc->old_name.string().c_str(), "r");
    if (rcfp != nullptr)
    {
        const std::string_view group_name = rc_name();
        const std::size_t      separator = group_name.size();

        while (!(some_buf = get_a_line(rcfp)).empty())
        {
            if (some_buf.back() == '\n')
            {
                some_buf.pop_back(); // wipe out newline
            }
            const std::string_view old_line{some_buf};
            if (old_line.size() > separator &&
                (old_line[separator] == ':' || old_line[separator] == UNSUBSCRIBED_CHAR) &&
                old_line.substr(0, separator) == group_name)
            {
                break;
            }
            some_buf.clear();
        }
        std::fclose(rcfp);
    }
    else if (errno != ENOENT)
    {
        fmt::print("Unable to open {}.\n", m_rc->old_name.generic_string());
        term_down(1);
        return;
    }
    if (some_buf.empty())
    {
        m_rc_line.resize(static_cast<std::size_t>(m_num_offset - 1));
        m_abs_first = ArticleNum{};         // force group to be re-calculated
    }
    else
    {
        m_rc_line = std::move(some_buf);
    }
    parse_rcline(this);
    if (m_subscribe_char == UNSUBSCRIBED_CHAR)
    {
        m_subscribe_char = ':';
    }
    m_rc->flags |= RF_RC_CHANGED;
    set_to_read(ST_LAX);
}

// try to find or add an explicitly specified newsgroup
// returns true if found or added, false if not.
// assumes that we are chdir'ed to NEWS_SPOOL

bool get_newsgroup(std::string_view what, GetNewsgroupFlags flags)
{
    const char *n_to_forget;

    if (g_verbose)
    {
        n_to_forget = "Type n to forget about this newsgroup.\n";
    }
    else
    {
        n_to_forget = "n to forget it.\n";
    }
    if (what.find('/') != std::string_view::npos)
    {
        dingaling();
        std::printf("\nBad newsgroup name.\n");
        term_down(2);
check_fuzzy_match:
        if (g_fuzzy_get && (flags & GNG_FUZZY))
        {
            flags &= ~GNG_FUZZY;
            if (find_close_match())
            {
                what = g_newsgroup_name;
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    set_newsgroup_name(what);
    g_newsgroup_ptr = find_newsgroup(g_newsgroup_name);
    if (g_newsgroup_ptr == nullptr)             // not in .newsrc?
    {
        Newsrc* rp;
        for (rp = g_multirc->m_first; rp; rp = rp->next)
        {
            if (!all_bits(rp->flags, RF_ADD_GROUPS | RF_ACTIVE))
            {
                continue;
            }
            // TODO: this may scan a datasrc multiple times...
            if (!rp->data_source->find_active_group(g_newsgroup_name, ArticleNum{}).empty())
            {
                break; // TODO: let them choose which server
            }
        }
        if (!rp)
        {
            dingaling();
            if (g_verbose)
            {
                std::printf("\nNewsgroup %s does not exist!\n", g_newsgroup_name.c_str());
            }
            else
            {
                std::printf("\nNo %s!\n", g_newsgroup_name.c_str());
            }
            term_down(2);
            if (g_novice_delays)
            {
                sleep(2);
            }
            goto check_fuzzy_match;
        }
        AddNewType autosub;
        if (g_mode != MM_INITIALIZING || !(autosub = auto_subscribe(g_newsgroup_name.c_str())))
        {
            autosub = g_add_new_by_default;
        }
        if (autosub)
        {
            if (g_append_unsub)
            {
                fmt::print("(Adding {} to end of your .newsrc {}subscribed)\n", g_newsgroup_name,
                           autosub == ADDNEW_SUB ? "" : "un");
                term_down(1);
                g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name, autosub);
            }
            else
            {
                if (autosub == ADDNEW_SUB)
                {
                    fmt::print("(Subscribing to {})\n", g_newsgroup_name);
                    term_down(1);
                    g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name, autosub);
                }
                else
                {
                    fmt::print("(Ignoring {})\n", g_newsgroup_name);
                    term_down(1);
                    return false;
                }
            }
            flags &= ~GNG_RELOC;
        }
        else
        {
            const std::string add_prompt{
                g_verbose ? fmt::format("\nNewsgroup {} not in .newsrc -- subscribe?", g_newsgroup_name)
                          : fmt::format("\nSubscribe {}?", g_newsgroup_name)};
reask_add:
            const std::string command = read_newsrc_prompt_command(add_prompt, MM_ADD_NEWSGROUP_PROMPT, "ynYN");
            print_prompt_command(command);
            newline();
            const char command_ch = command_char(command);
            if (command_ch == 'h')
            {
                if (g_verbose)
                {
                    std::printf("Type y or SP to subscribe to %s.\n"
                           "Type Y to subscribe to this and all remaining new groups.\n"
                           "Type N to leave all remaining new groups unsubscribed.\n",
                           g_newsgroup_name.c_str());
                    term_down(3);
                }
                else
                {
                    std::fputs("y or SP to subscribe, Y to subscribe all new groups, N to unsubscribe all\n",
                          stdout);
                    term_down(1);
                }
                std::fputs(n_to_forget,stdout);
                term_down(1);
                goto reask_add;
            }
            else if (command_ch == 'n' || command_ch == 'q')
            {
                if (g_append_unsub)
                {
                    g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name, UNSUBSCRIBED_CHAR);
                }
                return false;
            }
            else if (command_ch == 'y')
            {
                g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name, ':');
                flags |= GNG_RELOC;
            }
            else if (command_ch == 'Y')
            {
                g_add_new_by_default = ADDNEW_SUB;
                if (g_append_unsub)
                {
                    fmt::print("(Adding {} to end of your .newsrc subscribed)\n", g_newsgroup_name);
                }
                else
                {
                    fmt::print("(Subscribing to {})\n", g_newsgroup_name);
                }
                term_down(1);
                g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name, ':');
                flags &= ~GNG_RELOC;
            }
            else if (command_ch == 'N')
            {
                g_add_new_by_default = ADDNEW_UNSUB;
                if (g_append_unsub)
                {
                    fmt::print("(Adding {} to end of your .newsrc unsubscribed)\n", g_newsgroup_name);
                    term_down(1);
                    g_newsgroup_ptr = add_newsgroup(rp, g_newsgroup_name, UNSUBSCRIBED_CHAR);
                    flags &= ~GNG_RELOC;
                }
                else
                {
                    fmt::print("(Ignoring {})\n", g_newsgroup_name);
                    term_down(1);
                    return false;
                }
            }
            else
            {
                std::fputs("Type h for help.\n", stdout);
                term_down(1);
                settle_down();
                goto reask_add;
            }
        }
    }
    else if (g_mode == MM_INITIALIZING)         // adding new groups during init?
    {
        return false;
    }
    else if (g_newsgroup_ptr->m_subscribe_char == UNSUBSCRIBED_CHAR)  // unsubscribed?
    {
        const std::string resubscribe_prompt{
            g_verbose ? fmt::format("\nNewsgroup {} is unsubscribed -- resubscribe?", g_newsgroup_name)
                      : fmt::format("\nResubscribe {}?", g_newsgroup_name)};
reask_unsub:
        const std::string command = read_newsrc_prompt_command(resubscribe_prompt, MM_RESUBSCRIBE_PROMPT, "yn");
        print_prompt_command(command);
        newline();
        const char command_ch = command_char(command);
        if (command_ch == 'h')
        {
            if (g_verbose)
            {
                fmt::print("Type y or SP to resubscribe to {}.\n", g_newsgroup_name);
            }
            else
            {
                std::fputs("y or SP to resubscribe.\n", stdout);
            }
            std::fputs(n_to_forget,stdout);
            term_down(2);
            goto reask_unsub;
        }
        else if (command_ch == 'n' || command_ch == 'q')
        {
            return false;
        }
        else if (command_ch == 'y')
        {
            const std::string_view numbers = g_newsgroup_ptr->rc_numbers();
            g_newsgroup_ptr->m_flags =
                (numbers.size() > 1 && numbers[0] != '\0' && numbers[1] == '0' ? NF_UNTHREADED : NF_NONE);
            g_newsgroup_ptr->m_subscribe_char = ':';
            g_newsgroup_ptr->m_rc->flags |= RF_RC_CHANGED;
            flags &= ~GNG_RELOC;
        }
        else
        {
            std::fputs("Type h for help.\n", stdout);
            term_down(1);
            settle_down();
            goto reask_unsub;
        }
    }

    // now calculate how many unread articles in newsgroup

    g_newsgroup_ptr->set_to_read(ST_STRICT);
    if (flags & GNG_RELOC)
    {
        if (!g_newsgroup_ptr->relocate_newsgroup(NewsgroupNum{-1}))
        {
            return false;
        }
    }
    return g_newsgroup_ptr->m_to_read >= TR_NONE;
}

// add a newsgroup to the newsrc file (eventually)

static NewsgroupData *add_newsgroup(Newsrc *rp, std::string_view ngn, char_int c)
{
    NewsgroupData *np = append_newsgroup_data();
    append_newsgroup_order(np);
    ++g_newsgroup_count;

    np->m_rc = rp;
    np->m_num_offset = static_cast<int>(ngn.size() + 1);
    np->m_rc_line = ngn;
    np->m_rc_line.push_back('\0');
    np->m_rc_line.push_back(' ');
    np->m_subscribe_char = c; // subscribe or unsubscribe
    if (c != UNSUBSCRIBED_CHAR)
    {
        ++g_newsgroup_to_read;
    }
    np->m_to_read = TR_NONE; // just for prettiness
    set_hash(np);            // so we can find it again
    rp->flags |= RF_RC_CHANGED;
    return np;
}

static char command_char(std::string_view command)
{
    return command.empty() ? '\0' : command.front();
}

static std::string read_newsrc_prompt_command(std::string_view prompt, MinorMode newmode, std::string_view dflt)
{
    MinorMode   mode_save = g_mode;
    GeneralMode gmode_save = g_general_mode;
    const int   newlines = static_cast<int>(std::count(prompt.begin(), prompt.end(), '\n'));

    while (true)
    {
        unflush_output();
        fmt::print("{} [{}] ", prompt, dflt);
        std::fflush(stdout);
        term_down(newlines);
        eat_typeahead();
        set_mode(GM_PROMPT, newmode);
        std::string command = get_cmd();
        if (errno || command_char(command) == '\f')
        {
            newline();
            continue;
        }

        g_s_default_cmd = false;
        g_univ_default_cmd = false;
        const char command_ch = command_char(command);
        if (command_ch == ' '                           //
#ifndef STRICT_CR                                       //
            || command_ch == '\n' || command_ch == '\r' //
#endif                                                  //
        )
        {
            g_s_default_cmd = true;
            g_univ_default_cmd = true;
            if (dflt.size() > 1 && dflt.front() == '^' && std::isupper(static_cast<unsigned char>(dflt[1])))
            {
                push_char(Ctl(dflt[1]));
            }
            else
            {
                push_char(dflt.empty() ? '\0' : dflt.front());
            }
            command = get_cmd();
        }
        set_mode(gmode_save, mode_save);
        return command;
    }
}

static bool relocation_command_needs_completion(std::string_view command)
{
    return command.size() > 1 && command[1] == FINISH_CMD;
}

static std::string finish_relocation_command(std::string_view command)
{
    if (!relocation_command_needs_completion(command))
    {
        return std::string{command};
    }
    return finish_command(command.substr(0, 1), true);
}

static void print_prompt_command(std::string_view command)
{
    if (g_verify && command.size() > 1 && command[1] == FINISH_CMD)
    {
        if (!at_norm_char(command.substr(0, 1)))
        {
            std::putchar('^');
            std::putchar((command.front() & 0x7F) | 64);
            backspace();
            backspace();
        }
        else
        {
            std::putchar(command.front());
            backspace();
        }
        std::fflush(stdout);
    }
}

bool NewsgroupData::relocate_newsgroup(NewsgroupNum newnum)
{
    const std::string_view dflt = (this != g_current_newsgroup ? "$^.Lq" : "$^Lq");
    SelectionSortMode save_sort = g_sel_sort;

    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        if (newnum < 0)
        {
            // ask if they want to keep the current order
            const std::string command = read_newsrc_prompt_command(
                "Sort newsrc(s) using current sort order?", MM_DELETE_BOGUS_NEWSGROUPS_PROMPT, "yn"); // TODO: !'D'
            print_prompt_command(command);
            newline();
            if (command_char(command) == 'y')
            {
                set_selector(SM_NEWSGROUP, SS_NATURAL);
            }
            else
            {
                g_sel_sort = SS_NATURAL;
                g_sel_direction = 1;
                sort_newsgroups();
            }
        }
        else
        {
            g_sel_sort = SS_NATURAL;
            g_sel_direction = 1;
            sort_newsgroups();
        }
    }

    g_start_here = nullptr; // Disable this optimization
    if (!move_newsgroup_order(this, newsgroup_before(g_newsgroup_count)))
    {
        return false;
    }
    m_rc->flags |= RF_RC_CHANGED;

    if (newnum < 0)
    {
reask_reloc:
        unflush_output();               // disable any ^O in effect
        if (g_verbose)
        {
            fmt::print("\nPut newsgroup where? [{}] ", dflt);
        }
        else
        {
            fmt::print("\nPut where? [{}] ", dflt);
        }
        std::fflush(stdout);
        term_down(1);
reinp_reloc:
        eat_typeahead();
        std::string command = get_cmd();
        char        command_char = command.empty() ? '\0' : command.front();
        if (errno || command_char == '\f')    // if return from stop signal
        {
            goto reask_reloc;           // give them a prompt again
        }
        g_s_default_cmd = false;
        g_univ_default_cmd = false;
        if (command_char == ' ' || command_char == '\n' || command_char == '\r')
        {
            g_s_default_cmd = true;
            g_univ_default_cmd = true;
            command.clear();
            command.push_back(dflt.empty() ? '\0' : dflt.front());
            command.push_back(FINISH_CMD);
            command_char = command.empty() ? '\0' : command.front();
        }
        print_prompt_command(command);
        if (command_char == 'h')
        {
            if (g_verbose)
            {
                std::printf("\n"
                       "\n"
                       "Type ^ to put the newsgroup first (position 0).\n"
                       "Type $ to put the newsgroup last (position %d).\n",
                       g_newsgroup_count.value_of() - 1);
                std::printf("Type . to put it before the current newsgroup.\n"
                       "Type -newsgroup name to put it before that newsgroup.\n"
                       "Type +newsgroup name to put it after that newsgroup.\n"
                       "Type a number between 0 and %d to put it at that position.\n",
                       g_newsgroup_count.value_of() - 1);
                std::printf("Type L for a listing of newsgroups and their positions.\n"
                       "Type q to abort the current action.\n");
            }
            else
            {
                std::printf("\n"
                       "\n"
                       "^ to put newsgroup first (pos 0).\n"
                       "$ to put last (pos %d).\n",
                       g_newsgroup_count.value_of() - 1);
                std::printf(". to put before current newsgroup.\n"
                       "-newsgroup to put before newsgroup.\n"
                       "+newsgroup to put after.\n"
                       "number in 0-%d to put at that pos.\n"
                       "L for list of newsrc.\n"
                       "q to abort\n",
                       g_newsgroup_count.value_of() - 1);
            }
            term_down(10);
            goto reask_reloc;
        }
        else if (command_char == 'q')
        {
            return false;
        }
        else if (command_char == 'L')
        {
            newline();
            list_newsgroups();
            goto reask_reloc;
        }
        else if (std::isdigit(static_cast<unsigned char>(command_char)))
        {
            const std::string full_command = finish_relocation_command(command);
            if (full_command.empty()) // get rest of command
            {
                goto reinp_reloc;
            }
            long command_number{};
            (void) std::from_chars(full_command.data(), full_command.data() + full_command.size(), command_number);
            newnum = NewsgroupNum{command_number};
            newnum = std::max(newnum, NewsgroupNum{});
            if (newnum >= g_newsgroup_count)
            {
                newnum = newsgroup_before(g_newsgroup_count);
            }
        }
        else if (command_char == '^')
        {
            newline();
            newnum = NewsgroupNum{};
        }
        else if (command_char == '$')
        {
            newnum = newsgroup_before(g_newsgroup_count);
        }
        else if (command_char == '.')
        {
            newline();
            newnum = g_current_newsgroup->m_num;
        }
        else if (command_char == '-' || command_char == '+')
        {
            const std::string full_command = finish_relocation_command(command);
            if (full_command.empty()) // get rest of command
            {
                goto reinp_reloc;
            }
            std::string_view group_name{full_command};
            group_name.remove_prefix(1);
            NewsgroupData *np = find_newsgroup(group_name);
            if (np == nullptr)
            {
                std::fputs("Not found.",stdout);
                goto reask_reloc;
            }
            newnum = np->m_num;
            if (command_char == '+')
            {
                ++newnum;
            }
        }
        else
        {
            std::fputs("\nType h for help.\n", stdout);
            term_down(2);
            settle_down();
            goto reask_reloc;
        }
    }

    if (newnum < newsgroup_before(g_newsgroup_count))
    {
        if (!move_newsgroup_order(this, newnum))
        {
            return false;
        }
    }
    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        g_sel_sort = g_sel_newsgroup_sort;
        sort_newsgroups();
        g_sel_sort = save_sort;
    }
    return true;
}

// List out the newsrc with annotations

void list_newsgroups()
{
    NewsgroupData     *np;
    NewsgroupNum       i;
    std::string        line;
    static constexpr std::array<std::string_view, 5> status{
        "(READ)", "(UNSUB)", "(DUP)", "(BOGUS)", "(JUNK)"};

    line.reserve(2048);
    page_start();
    print_lines("  #  Status  Newsgroup\n", STANDOUT);
    for (np = newsgroup_first(), i = NewsgroupNum{}; np && !g_int_count; np = newsgroup_next(np), ++i)
    {
        if (np->m_to_read >= 0)
        {
            np->set_to_read(ST_LAX);
        }
        np->show_subscribe_char();
        line.clear();
        if (np->m_to_read > 0)
        {
            fmt::format_to(std::back_inserter(line), "{:3} {:6}   {}", i.value_of(), np->m_to_read, np->rc_line());
        }
        else
        {
            fmt::format_to(std::back_inserter(line), "{:3} {:>7}  {}", i.value_of(), status[-np->m_to_read],
                           np->rc_line());
        }
        np->hide_subscribe_char();
        if (print_lines(line, NO_MARKING) != 0)
        {
            break;
        }
    }
    g_int_count = 0;
}

// find a newsgroup in any newsrc

NewsgroupData *find_newsgroup(std::string_view ngnam)
{
    HashDatum data = hash_fetch(g_newsrc_hash, ngnam);
    return (NewsgroupData *) data.dat_ptr;
}

void cleanup_newsrc(Newsrc *rp)
{
    NewsgroupNum bogosity{};

    if (g_verbose)
    {
        fmt::print("Checking out '{}' -- hang on a second...\n", rp->name.generic_string());
    }
    else
    {
        fmt::print("Checking '{}' -- hang on...\n", rp->name.generic_string());
    }
    term_down(1);
    NewsgroupData *np;
    for (np = newsgroup_first(); np; np = newsgroup_next(np))
    {
        if (np->m_to_read >= TR_UNSUB)
        {
            np->set_to_read(ST_LAX); // this may reset the group or declare it bogus
        }
        if (np->m_to_read == TR_BOGUS)
        {
            ++bogosity;
        }
    }
    for (np = newsgroup_last(); np && np->m_to_read == TR_BOGUS; np = newsgroup_prev(np))
    {
        --bogosity; // discount already moved ones
    }
    if (g_newsgroup_count > 5 && bogosity > g_newsgroup_count / NewsgroupNum{2})
    {
        std::fputs("It looks like the active file is messed up.  Contact your news administrator,\n",
              stdout);
        std::fputs("leave the \"bogus\" groups alone, and they may come back to normal.  Maybe.\n",
              stdout);
        term_down(2);
    }
    else if (bogosity)
    {
        if (g_verbose)
        {
            fmt::print("Moving bogus newsgroups to the end of '{}'.\n", rp->name.generic_string());
        }
        else
        {
            std::fputs("Moving boguses to the end.\n", stdout);
        }
        term_down(1);
        while (np)
        {
            NewsgroupData *prev_np = newsgroup_prev(np);
            if (np->m_to_read == TR_BOGUS)
            {
                np->relocate_newsgroup(NewsgroupNum{g_newsgroup_count.value_of() - 1});
            }
            np = prev_np;
        }
        rp->flags |= RF_RC_CHANGED;
reask_bogus:
        const std::string command =
            read_newsrc_prompt_command("Delete bogus newsgroups?", MM_DELETE_BOGUS_NEWSGROUPS_PROMPT, "ny");
        print_prompt_command(command);
        newline();
        const char command_ch = command_char(command);
        if (command_ch == 'h')
        {
            if (g_verbose)
            {
                std::fputs("Type y to delete bogus newsgroups.\n"
                           "Type n or SP to leave them at the end in case they return.\n",
                           stdout);
                term_down(2);
            }
            else
            {
                std::fputs("y to delete, n to keep\n", stdout);
                term_down(1);
            }
            goto reask_bogus;
        }
        else if (command_ch == 'n' || command_ch == 'q')
        {
        }
        else if (command_ch == 'y')
        {
            while ((np = newsgroup_last()) != nullptr && np->m_to_read == TR_BOGUS)
            {
                hash_delete(g_newsrc_hash, np->rc_name());
                clear_newsgroup_item(np);
                pop_newsgroup_order();
                --g_newsgroup_count;
            }
            rp->flags |= RF_RC_CHANGED; // TODO: needed?
            if (g_current_newsgroup && g_current_newsgroup->m_rc_line.empty())
            {
                g_current_newsgroup = newsgroup_first();
            }
            if (g_recent_newsgroup && g_recent_newsgroup->m_rc_line.empty())
            {
                g_recent_newsgroup = newsgroup_first();
            }
            if (g_newsgroup_ptr && g_newsgroup_ptr->m_rc_line.empty())
            {
                g_newsgroup_ptr = newsgroup_first();
            }
            if (g_sel_page_np && g_sel_page_np->m_rc_line.empty())
            {
                g_sel_page_np = nullptr;
            }
            if (g_sel_next_np && g_sel_next_np->m_rc_line.empty())
            {
                g_sel_next_np = nullptr;
            }
        }
        else
        {
            std::fputs("Type h for help.\n", stdout);
            term_down(1);
            settle_down();
            goto reask_bogus;
        }
    }
    g_paranoid = false;
}

// make an entry in the hash table for the current newsgroup

static void set_hash(NewsgroupData *np)
{
    HashDatum data;
    data.dat_ptr = (char *) np;
    data.dat_len = np->m_num_offset - 1;
    hash_store(g_newsrc_hash, np->rc_name(), data);
}

static void rebuild_newsgroup_hash()
{
    if (g_newsrc_hash == nullptr)
    {
        return;
    }

    hash_destroy(g_newsrc_hash);
    g_newsrc_hash = hash_create(3001, rcline_cmp);
    for (NewsgroupData &np : g_newsgroup_data)
    {
        if (!np.m_rc_line.empty())
        {
            set_hash(&np);
        }
    }
}

static int rcline_cmp(std::string_view key, HashDatum data)
{
    const auto            *newsgroup = (NewsgroupData *) data.dat_ptr;
    const std::string_view rc_line = newsgroup->rc_line().substr(0, key.size());

    return key.compare(rc_line);
}

// checkpoint the newsrc(s)

void checkpoint_newsrcs()
{
#ifdef DEBUG
    if (g_debug & DEB_CHECKPOINTING)
    {
        std::fputs("(ckpt)",stdout);
        std::fflush(stdout);
    }
#endif
    if (g_doing_ng)
    {
        bits_to_rc();                   // do not restore M articles
    }
    if (!write_newsrcs(g_multirc))
    {
        get_anything();
    }
#ifdef DEBUG
    if (g_debug & DEB_CHECKPOINTING)
    {
        std::fputs("(done)",stdout);
        std::fflush(stdout);
    }
#endif
}

// write out the (presumably) revised newsrc(s)
//
// TODO: why does this check mptr for nullptr?
//
bool write_newsrcs(Multirc *mptr)
{
    SelectionSortMode save_sort = g_sel_sort;
    bool          total_success = true;

    if (!mptr)
    {
        return true;
    }

    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        g_sel_sort = SS_NATURAL;
        g_sel_direction = 1;
        sort_newsgroups();
    }

    for (Newsrc *rp = mptr->m_first; rp; rp = rp->next)
    {
        if (!(rp->flags & RF_ACTIVE))
        {
            continue;
        }

        if (!rp->info_name.empty())
        {
            std::FILE *info = std::fopen(rp->info_name.string().c_str(), "w");
            if (info != nullptr)
            {
                fmt::print(info,"Last-Group: {}\nNew-Group-State: {},{},{}\n",
                        g_newsgroup_name,rp->data_source->m_last_new_group,
                        rp->data_source->m_act_sf.m_recent_cnt,
                        rp->data_source->m_desc_sf.m_recent_cnt);
                std::fclose(info);
            }
        }
        else
        {
            read_last();
            if (rp->data_source->m_flags & DF_REMOTE)
            {
                g_last_active_size = rp->data_source->m_act_sf.m_recent_cnt;
                g_last_extra_num = rp->data_source->m_desc_sf.m_recent_cnt;
            }
            else
            {
                g_last_extra_num = rp->data_source->m_act_sf.m_recent_cnt;
            }
            g_last_new_time = rp->data_source->m_last_new_group;
            write_last();
        }

        if (!(rp->flags & RF_RC_CHANGED))
        {
            continue;
        }

        std::FILE *rcfp = std::fopen(rp->new_name.string().c_str(), "w");
        if (rcfp == nullptr)
        {
            print_cant_recreate(rp->name);
            total_success = false;
            continue;
        }
#ifndef MSDOS
        stat_t perms;
        if (stat(rp->name.string().c_str(),&perms)>=0)   // preserve permissions
        {
            chmod(rp->new_name.string().c_str(),perms.st_mode&0666);
            chown(rp->new_name.string().c_str(),perms.st_uid,perms.st_gid);
        }
#endif
        // write out each line

        for (NewsgroupData *np = newsgroup_first(); np; np = newsgroup_next(np))
        {
            if (np->m_rc != rp)
            {
                continue;
            }
            if (np->m_num_offset)
            {
                np->show_subscribe_char();
            }
            std::string line{np->rc_line()};
            if (np->m_num_offset)
            {
                const std::size_t delimiter = static_cast<std::size_t>(np->m_num_offset - 1);
                const std::size_t marker = delimiter + 2;
                if ((np->m_flags & NF_UNTHREADED) && marker < line.size() && line[marker] == '1')
                {
                    line[marker] = '0';
                }
            }
#ifdef DEBUG
            if (g_debug & DEB_NEWSRC_LINE)
            {
                fmt::print("{}\n", line);
                term_down(1);
            }
#endif
            fmt::print(rcfp, "{}\n", line);
            if (std::ferror(rcfp))
            {
                std::fclose(rcfp); // close new newsrc
                goto write_error;
            }
            if (np->m_num_offset)
            {
                np->hide_subscribe_char(); // might still need this line
            }
        }
        std::fflush(rcfp);
        // fclose is the only sure test for full disks via NFS
        if (std::ferror(rcfp))
        {
            std::fclose(rcfp);
            goto write_error;
        }
        if (std::fclose(rcfp) == EOF)
        {
write_error:
            print_cant_recreate(rp->name);
            std::error_code error;
            fs::remove(rp->new_name, error);
            total_success = false;
            continue;
        }
        rp->flags &= ~RF_RC_CHANGED;

        std::error_code error;
        fs::remove(rp->name, error);
        error.clear();
        fs::rename(rp->new_name, rp->name, error);
    }

    if (g_sel_newsgroup_sort != SS_NATURAL)
    {
        g_sel_sort = g_sel_newsgroup_sort;
        sort_newsgroups();
        g_sel_sort = save_sort;
    }
    return total_success;
}

// TODO: why does this check mptr for nullptr?
//
void get_old_newsrcs(Multirc *mptr)
{
    if (mptr)
    {
        for (Newsrc *rp = mptr->m_first; rp; rp = rp->next)
        {
            if (rp->flags & RF_ACTIVE)
            {
                std::error_code error;
                fs::remove(rp->new_name, error);
                error.clear();
                fs::rename(rp->name, rp->new_name, error);
                error.clear();
                fs::rename(rp->old_name, rp->name, error);
            }
        }
    }
}
