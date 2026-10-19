/* addng.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/addng-internal.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <trn/autosub.h>
#include <trn/datasrc.h>
#include <trn/hash.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/only.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/size_cast.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <ctime>
#include <vector>

namespace fs = std::filesystem;

struct ActiveLineFields
{
    long high{};
    long low{1};
    char status{'y'};
};

AddGroupTable g_add_groups;
AddGroupIndex g_sel_page_gp{};
AddGroupIndex g_sel_next_gp{};
bool      g_quick_start{};           // -q
bool      g_use_add_selector{true}; //

static void process_list(GetNewsgroupFlags flag);
static ActiveLineFields parse_active_line_fields(std::string_view fields);
static int  list_groups(int key_len, HashDatum *data, int add_matching);
static void scan_active_line(std::string_view active_line, bool add_matching);
static int  add_group_order_number(const AddGroup *left, const AddGroup *right);
static int  add_group_order_group_name(const AddGroup *left, const AddGroup *right);
static int  add_group_order_count(const AddGroup *left, const AddGroup *right);

AddGroupIndex AddGroupTable::add(std::string_view name, int to_read, int subscription, DataSource *source)
{
    const auto where =
        std::lower_bound(m_names.begin(), m_names.end(), name,
                         [this](AddGroupIndex index, std::string_view key) { return (*this)[index].m_name < key; });
    if (where != m_names.end() && (*this)[*where].m_name == name)
    {
        return *where;
    }
    const AddGroupIndex index = static_cast<AddGroupIndex>(m_groups.size() + 1);
    const AddGroupFlags flags = subscription == ':' ? AGF_SEL : subscription == '!' ? AGF_DEL : AGF_NONE;
    m_groups.push_back(
        {source, ArticleNum{std::max(to_read, 0)}, NewsgroupNum{size_cast<int>(m_groups)}, flags, std::string{name}});
    m_names.insert(where, index);
    m_positions.push_back(m_order.size());
    m_order.push_back(index);
    return index;
}

void AddGroupTable::clear()
{
    m_groups.clear();
    m_order.clear();
    m_positions.clear();
    m_names.clear();
}

void AddGroupTable::sort(int (*compare)(const AddGroup *, const AddGroup *))
{
    std::sort(m_order.begin(), m_order.end(), [this, compare](AddGroupIndex left, AddGroupIndex right)
              { return compare(&(*this)[left], &(*this)[right]) < 0; });
    for (std::size_t pos = 0; pos != m_order.size(); ++pos)
    {
        m_positions[static_cast<std::size_t>(m_order[pos]) - 1] = pos;
    }
}

AddGroupIndex AddGroupTable::first() const
{
    return m_order.empty() ? AddGroupIndex::None : m_order.front();
}

AddGroupIndex AddGroupTable::last() const
{
    return m_order.empty() ? AddGroupIndex::None : m_order.back();
}

AddGroupIndex AddGroupTable::next(AddGroupIndex index) const
{
    if (index == AddGroupIndex::None)
    {
        return AddGroupIndex::None;
    }
    const std::size_t pos = m_positions[static_cast<std::size_t>(index) - 1] + 1;
    return pos == m_order.size() ? AddGroupIndex::None : m_order[pos];
}

AddGroupIndex AddGroupTable::prev(AddGroupIndex index) const
{
    if (index == AddGroupIndex::None)
    {
        return AddGroupIndex::None;
    }
    const std::size_t pos = m_positions[static_cast<std::size_t>(index) - 1];
    return pos == 0 ? AddGroupIndex::None : m_order[pos - 1];
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

static ActiveLineFields parse_active_line_fields(std::string_view fields)
{
    ActiveLineFields result;
    if (!read_active_field_number(fields, result.high))
    {
        return result;
    }
    if (!read_active_field_number(fields, result.low))
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

void add_ng_init()
{
}

bool find_new_groups()
{
    const NewsgroupNum old_cnt = g_newsgroup_count;      // remember # newsgroups

    // Skip this check if the -q flag was given.
    if (g_quick_start)
    {
        return false;
    }

    for (const Newsrc *rp = g_multirc->m_first; rp; rp = rp->next)
    {
        if (all_bits(rp->flags, RF_ADD_NEW_GROUPS | RF_ACTIVE))
        {
            if (rp->data_source->m_flags & DF_REMOTE)
            {
                new_nntp_groups(rp->data_source);
            }
            else
            {
                new_local_groups(rp->data_source);
            }
        }
    }
    g_add_new_by_default = ADDNEW_ASK;

    process_list(GNG_RELOC);

    return old_cnt != g_newsgroup_count;
}

static void process_list(GetNewsgroupFlags flag)
{
    if (flag == GNG_NONE)
    {
        std::string line;
        line.reserve(CMD_BUF_LEN);
        fmt::format_to(std::back_inserter(line),
                       "\n"
                       "Unsubscribed but mentioned in your current newsrc{}:\n",
                       g_multirc->m_first->next ? "s" : "");
        print_lines(line, STANDOUT);
    }
    if (!g_add_groups.empty() && flag != GNG_NONE && g_use_add_selector)
    {
        add_group_selector(flag);
    }
    for (AddGroupIndex index : g_add_groups.indices())
    {
        const AddGroup &group = g_add_groups[index];
        if (flag == GNG_NONE)
        {
            std::string line;
            line.reserve(CMD_BUF_LEN);
            fmt::format_to(std::back_inserter(line), "{}\n", group.m_name);
            print_lines(line, NO_MARKING);
        }
        else if (!g_use_add_selector)
        {
            get_newsgroup(group.m_name, flag); // add newsgroup -- maybe
        }
    }
    g_add_groups.clear();
    g_sel_page_gp = AddGroupIndex::None;
    g_sel_next_gp = AddGroupIndex::None;
}

void new_nntp_groups(DataSource *dp)
{
    bool found_something = false;

    set_data_source(dp);

    const std::time_t server_time = nntp_time();
    if (server_time == -2)
    {
        return;
    }
    if (nntp_new_groups(dp->m_last_new_group) < 1)
    {
        fmt::print("Can't get new groups from server:\n{}\n", g_ser_line);
        return;
    }
    std::string new_group_line;
    new_group_line.reserve(NNTP_STRLEN);

    while (true)
    {
        ActiveLineFields active_fields;
        if (nntp_gets(new_group_line, NNTP_STRLEN) == NGSR_ERROR)
        {
            break;
        }
#ifdef DEBUG
        if (g_debug & DEB_NNTP)
        {
            fmt::print("<{}\n", new_group_line);
        }
#endif
        if (nntp_at_list_end(new_group_line))
        {
            break;
        }
        found_something = true;
        const std::string_view line{new_group_line};
        const std::size_t      name_end = line.find(' ');
        const std::string_view group_name = line.substr(0, name_end);
        const bool             has_fields = name_end != std::string_view::npos;
        const std::string_view fields = has_fields ? line.substr(name_end + 1) : std::string_view{};
        if (dp->m_act_sf.m_fp)
        {
            const std::string active_line = dp->find_active_group(group_name, ArticleNum{});
            if (!active_line.empty())
            {
                if (!has_fields)
                {
                    const std::size_t status =
                        active_line.find_first_not_of("0123456789 \f\n\r\t\v", group_name.size() + 1);
                    if (status != std::string::npos && (active_line[status] == 'x' || active_line[status] == '='))
                    {
                        continue;
                    }
                }
            }
            else
            {
                if (has_fields)
                {
                    active_fields = parse_active_line_fields(fields);
                }
                const std::string new_active_line = fmt::format("{} {:010} {:05} {}\n", group_name, active_fields.high,
                                                                active_fields.low, active_fields.status);
                (void) dp->m_act_sf.append(new_active_line, static_cast<int>(group_name.size()));
            }
        }
        if (has_fields)
        {
            const std::size_t status = fields.find_first_not_of("0123456789 \f\n\r\t\v");
            if (status != std::string_view::npos && (fields[status] == 'x' || fields[status] == '='))
            {
                continue;
            }
        }
        NewsgroupData *np = find_newsgroup(group_name);
        if (np != nullptr && np->m_to_read > TR_UNSUB)
        {
            continue;
        }
        add_to_list(group_name, active_fields.high - active_fields.low, auto_subscribe(group_name));
    }
    if (found_something)
    {
        dp->m_act_sf.end_append(dp->m_extra_name);
        dp->m_last_new_group = server_time;
    }
}

void new_local_groups(DataSource *dp)
{
    g_data_source = dp;

    const long file_size{static_cast<long>(fs::file_size(dp->m_extra_name))};
    // did active.times file grow?
    if (file_size == dp->m_act_sf.m_recent_cnt)
    {
        return;
    }

    std::FILE *fp = std::fopen(dp->m_extra_name.c_str(), "r");
    if (fp == nullptr)
    {
        fmt::print("Can't open {}\n", dp->m_extra_name);
        term_down(1);
        return;
    }
    std::time_t last_one = std::time(nullptr) - 24L * 60 * 60 - 1;

    for (std::string active_times_line = get_a_line(fp); !active_times_line.empty(); active_times_line = get_a_line(fp))
    {
        const std::string_view active_times_view{active_times_line};
        const std::size_t      name_end = active_times_view.find(' ');
        if (name_end == std::string_view::npos)
        {
            continue;
        }
        std::string_view active_time = active_times_view.substr(name_end + 1);
        skip_active_field_space(active_time);
        std::time_t parsed_time{};
        (void) std::from_chars(active_time.data(), active_time.data() + active_time.size(), parsed_time);
        last_one = parsed_time;
        if (last_one < dp->m_last_new_group)
        {
            continue;
        }
        const std::string_view group_name = active_times_view.substr(0, name_end);
        const std::string      active_line = g_data_source->find_active_group(group_name, ArticleNum{});
        if (active_line.empty())
        {
            continue;
        }
        const std::string_view fields = std::string_view{active_line}.substr(group_name.size() + 1);
        const ActiveLineFields active_fields = parse_active_line_fields(fields);
        if (active_fields.status == 'x' || active_fields.status == '=')
        {
            continue;
        }
        NewsgroupData *np = find_newsgroup(group_name);
        if (np != nullptr)
        {
            continue;
        }
        add_to_list(group_name, active_fields.high - active_fields.low, auto_subscribe(group_name));
    }
    std::fclose(fp);

    dp->m_last_new_group = last_one+1;
    dp->m_act_sf.m_recent_cnt = file_size;
}

AddGroupIndex add_to_list(std::string_view name, int to_read, char_int ch)
{
    return g_add_groups.add(name, to_read, ch, g_data_source);
}

std::string active_list_pattern()
{
    if (g_max_newsgroup_to_do != 1)
    {
        return "*";
    }

    const std::string_view restriction{g_newsgroup_to_do[0]};
    std::string            pattern;
    pattern.reserve(LINE_BUF_LEN);
    if (!restriction.empty() && restriction.front() == '^')
    {
        pattern = restriction.substr(1);
        pattern += '*';
    }
    else
    {
        pattern = '*';
        pattern += restriction;
        pattern += '*';
    }
    if (pattern.size() >= 2 && pattern[pattern.size() - 2] == '$')
    {
        pattern.resize(pattern.size() - 2);
    }
    return pattern;
}

bool scan_active(bool add_matching)
{
    const NewsgroupNum old_count = g_newsgroup_count;      // remember # of newsgroups

    if (!add_matching)
    {
        print_lines("Completely unsubscribed newsgroups:\n", STANDOUT);
    }

    for (DataSource *dp = data_source_first(); dp; dp = data_source_next(dp))
    {
        if (!(dp->m_flags & DF_OPEN))
        {
            continue;
        }
        set_data_source(dp);
        if (dp->m_act_sf.m_fp)
        {
            hash_walk(dp->m_act_sf.m_hp, list_groups, add_matching);
        }
        else
        {
            std::string active_line;
            active_line.reserve(NNTP_STRLEN);
            if (nntp_list("active", active_list_pattern(), active_line) == 1)
            {
                while (!nntp_at_list_end(active_line))
                {
                    scan_active_line(active_line, add_matching);
                    if (nntp_gets(active_line, NNTP_STRLEN) == NGSR_ERROR)
                    {
                        break;
                    }
                }
            }
        }
    }

    process_list(add_matching ? GNG_RELOC : GNG_NONE);

    if (g_in_ng)
    {
        set_data_source(g_newsgroup_ptr->m_rc->data_source);
    }

    return old_count != g_newsgroup_count;
}

static int list_groups(int key_len, HashDatum *data, int add_matching)
{
    (void) key_len;
    SourceFile        *source_file = reinterpret_cast<SourceFile *>(data->dat_ptr);
    const std::string &line = source_file->m_lines[data->dat_len];
    scan_active_line(line, add_matching != 0);
    return 0;
}

static void scan_active_line(std::string_view active_line, bool add_matching)
{
    const std::size_t name_end = active_line.find(' ');
    if (name_end == std::string_view::npos)
    {
        return;
    }

    const std::string_view group_name = active_line.substr(0, name_end);
    std::string_view       fields = active_line.substr(name_end + 1);
    const ActiveLineFields active_fields = parse_active_line_fields(fields);
    if (active_fields.status == 'x' || (group_name.size() >= 3 && group_name.substr(0, 3) == "to."))
    {
        return;
    }
    if (!in_list(group_name))
    {
        return;
    }
    NewsgroupData *np = find_newsgroup(group_name);
    if (np != nullptr && np->m_to_read > TR_UNSUB)
    {
        return;
    }
    if (add_matching || np)
    {
        // it's not in a newsrc
        add_to_list(group_name, active_fields.high - active_fields.low, 0);
    }
    else
    {
        std::string line{group_name};
        line += '\n';
        print_lines(line, NO_MARKING);
    }
}

static int add_group_order_number(const AddGroup *left, const AddGroup *right)
{
    const NewsgroupNum eq = left->m_num - right->m_num;
    if (eq == 0)
    {
        return 0;
    }
    return eq.value_of() > 0 ? g_sel_direction : -g_sel_direction;
}

static int add_group_order_group_name(const AddGroup *left, const AddGroup *right)
{
    return string_case_compare(left->m_name, right->m_name) * g_sel_direction;
}

static int add_group_order_count(const AddGroup *left, const AddGroup *right)
{
    const ArticleNum eq = left->m_to_read - right->m_to_read;
    if (eq != 0)
    {
        return eq > 0 ? g_sel_direction : -g_sel_direction;
    }
    return add_group_order_group_name(left, right);
}

// Sort the newsgroups into the chosen order.
void sort_add_groups()
{
    int (*sort_procedure)(const AddGroup *left, const AddGroup *right);

    switch (g_sel_sort)
    {
    case SS_NATURAL:
    default:
        sort_procedure = add_group_order_number;
        break;
    case SS_STRING:
        sort_procedure = add_group_order_group_name;
        break;
    case SS_COUNT:
        sort_procedure = add_group_order_count;
        break;
    }

    g_add_groups.sort(sort_procedure);
}
