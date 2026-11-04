/* univ.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

/* Universal selector
 *
 */

#include <trn/univ.h>

#include <config/common.h>
#include <config/env.h>
#include <config/string_case_compare.h>
#include <trn/cache.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/help.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/score.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/url.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

// TODO:
//
// Be friendlier when a file has no contents.
// Implement virtual groups (largely done)
// Help scan mode replacement
// Lots more to do...
//

bool g_univ_ng_virt_flag{};   // if true, we are in the "virtual group" second pass
bool g_univ_read_virt_flag{}; // if true, we are reading an article from a "virtual group"
bool g_univ_default_cmd{};    // "follow"-related stuff (virtual groups)
bool g_univ_follow{true};
bool g_univ_follow_temp{};

UniversalMenuContext g_univ;
UniversalMenuLoader  g_univ_loader;
VirtualGroupBuilder  g_vg_builder;

static bool           s_univ_done_startup{};     //
static bool           s_univ_user_top{};         // if true, the user has loaded their own top univ. config file

static void           univ_open();
static void           univ_add_group(std::string_view desc, std::string_view grpname);
static void           univ_add_mask(std::string_view desc, std::string_view mask);
static void           univ_add_file(std::string_view desc, std::string_view fname, std::string_view label);
static UniversalItem *univ_add_virt_num(std::string_view desc, std::string_view grp, ArticleNum art);
static void           univ_add_text_file(std::string_view desc, std::string_view name);
static bool           univ_do_match(std::string_view text, std::string_view pattern);
static std::string univ_edit_new_user_file();

void univ_init()
{
    g_univ.level = 0;
}

UniversalItemIterator::UniversalItemIterator(UniversalItem *item) :
    m_index{g_univ.position(item)}
{
}

UniversalItem &UniversalItemIterator::operator*() const
{
    assert(m_index < g_univ.items.size());
    return g_univ.items[m_index];
}

UniversalItem *UniversalItemIterator::operator->() const
{
    assert(m_index < g_univ.items.size());
    return &g_univ.items[m_index];
}

UniversalItemIterator &UniversalItemIterator::operator++()
{
    assert(m_index < g_univ.items.size());
    ++m_index;
    return *this;
}

bool UniversalItemIterator::operator==(const UniversalItemIterator &other) const
{
    const bool at_end = m_index >= g_univ.items.size();
    const bool other_at_end = other.m_index >= g_univ.items.size();
    if (at_end || other_at_end)
    {
        return at_end == other_at_end;
    }
    return m_index == other.m_index;
}

bool UniversalItemIterator::operator!=(const UniversalItemIterator &other) const
{
    return !(*this == other);
}

UniversalItems::UniversalItems(UniversalItem *first) :
    m_first{g_univ.position(first)}
{
}

UniversalItemIterator UniversalItems::begin() const
{
    return UniversalItemIterator{m_first < g_univ.items.size() ? &g_univ.items[m_first] : nullptr};
}

UniversalItemIterator UniversalItems::end() const
{
    return UniversalItemIterator{nullptr};
}

UniversalItems univ_items()
{
    return UniversalItems{g_univ.first_item()};
}

UniversalItems univ_items(UniversalItemIndex first)
{
    const std::size_t position = g_univ.position(first);
    return UniversalItems{position < g_univ.items.size() ? &g_univ.items[position] : nullptr};
}

UniversalItems univ_items(UniversalItem *first)
{
    return UniversalItems{first};
}

UniversalItem *UniversalMenuContext::first_item()
{
    return items.empty() ? nullptr : &items.front();
}

UniversalItem *UniversalMenuContext::last_item()
{
    return items.empty() ? nullptr : &items.back();
}

UniversalItem *UniversalMenuContext::next_item(const UniversalItem *item)
{
    const std::size_t pos = position(item);
    return pos + 1 < items.size() ? &items[pos + 1] : nullptr;
}

UniversalItem *UniversalMenuContext::prev_item(const UniversalItem *item)
{
    const std::size_t pos = position(item);
    return pos > 0 && pos < items.size() ? &items[pos - 1] : nullptr;
}

UniversalItem *UniversalMenuContext::item(UniversalItemIndex item_index)
{
    const std::size_t pos = position(item_index);
    return pos < items.size() ? &items[pos] : nullptr;
}

UniversalItemIndex univ_index(const UniversalItem *item)
{
    return item ? item->m_num : UniversalItemIndex{};
}

std::size_t UniversalMenuContext::position(const UniversalItem *item)
{
    if (item == nullptr)
    {
        return items.size();
    }
    for (std::size_t position = 0; position < items.size(); ++position)
    {
        if (&items[position] == item)
        {
            return position;
        }
    }
    return items.size();
}

std::size_t UniversalMenuContext::position(UniversalItemIndex item_index)
{
    if (!item_index)
    {
        return items.size();
    }
    for (std::size_t position = 0; position < items.size(); ++position)
    {
        if (items[position].m_num == item_index)
        {
            return position;
        }
    }
    return items.size();
}

void univ_startup()
{
    // later: make user top file an option or environment variable?
    if (!univ_file_load("%+/univ/top", "Top Level", {}))
    {
        univ_open();
        g_univ.title = "Top Level";
        g_univ.fname = "%+/univ/usertop";

        // read in trn default top file
        (void) g_univ_loader.include_file("%X/sitetop"); // pure local
        bool sys_top_load = g_univ_loader.include_file("%X/trn4top");
        bool user_top_load = g_univ_loader.use_file("%+/univ/usertop", {});

        if (!(sys_top_load || user_top_load))
        {
            // last resort--all newsgroups
            univ_close();
            univ_mask_load("*", "All Newsgroups");
        }
        if (user_top_load)
        {
            s_univ_user_top = true;
        }
    }
    else
    {
        s_univ_user_top = true;
    }
    s_univ_done_startup = true;
}

static void univ_open()
{
    g_univ.items.clear();
    g_univ.ng_names.clear();
    g_univ.vg_names.clear();
    g_sel_page.page_univ_index = {};
    g_sel_page.next_univ_index = {};
    g_univ.fname.clear();
    g_univ.title.clear();
    g_univ.label.clear();
    g_univ.tmp_file.clear();
    g_vg_builder.virt_pass_needed = false;
    g_vg_builder.current_vg_ui_index = {};
    g_univ.level++;
}

void univ_close()
{
    g_univ.items.clear();
    if (!g_univ.tmp_file.empty())
    {
        std::error_code error;
        fs::remove(g_univ.tmp_file, error);
        g_univ.tmp_file.clear();
    }
    g_univ.fname.clear();
    g_univ.title.clear();
    g_univ.label.clear();
    g_univ.ng_names.clear();
    g_univ.vg_names.clear();
    g_sel_page.page_univ_index = {};
    g_sel_page.next_univ_index = {};
    g_vg_builder.current_vg_ui_index = {};
    g_univ.level--;
}

UniversalItem *UniversalMenuContext::add(UniversalData data, std::string_view desc)
{
    UniversalItem &node = items.emplace_back();

    node.m_flags = UF_NONE;
    node.m_desc.assign(desc);
    node.m_state = UIS_NORMAL;
    node.m_num = item_counter++;
    node.m_score = 0; // consider other default scores?
    node.m_data = std::move(data);

    return &node;
}

static void univ_add_group(std::string_view desc, std::string_view grpname)
{
    UniversalItem* ui;

    if (grpname.empty())
    {
        return;
    }
    // later check grpname for bad things?

    const std::string group_name{grpname};

    if (g_univ.ng_names.find(group_name) != g_univ.ng_names.end())
    {
        // group was already added
        // perhaps it is marked as deleted?
        for (UniversalItem &item : univ_items())
        {
            const UniversalNewsgroup *newsgroup = std::get_if<UniversalNewsgroup>(&item.m_data);
            if (newsgroup == nullptr)
            {
                continue;
            }
            if (item.m_state == UIS_DESELECTED && newsgroup->ng == group_name)
            {
                // undelete the newsgroup
                item.m_state = UIS_NORMAL;
            }
        }
        return;
    }
    g_univ.ng_names.insert(group_name);
    ui = g_univ.add(UniversalNewsgroup{}, desc);
    ui->group().ng = group_name;
}

static void univ_add_mask(std::string_view desc, std::string_view mask)
{
    UniversalItem          *ui = g_univ.add(UniversalGroupMaskData{}, desc);
    UniversalGroupMaskData &group_mask = ui->group_mask();
    group_mask.mask_list.assign(mask);
    group_mask.title.assign(desc);
}

static void univ_add_file(std::string_view desc, std::string_view fname, std::string_view label)
{
    UniversalItem           *ui = g_univ.add(UniversalConfigFileData{}, desc);
    UniversalConfigFileData &config_file = ui->config_file();
    config_file.title.assign(desc);
    config_file.fname = fname;
    if (!label.empty())
    {
        config_file.label.assign(label);
    }
}

static UniversalItem *univ_add_virt_num(std::string_view desc, std::string_view grp, ArticleNum art)
{
    UniversalItem           *ui = g_univ.add(UniversalVirtualArticle{}, desc);
    UniversalVirtualArticle &article = ui->article();
    article.ng.assign(grp);
    article.num = art;
    return ui;
}

static void univ_add_text_file(std::string_view desc, std::string_view name)
{
    UniversalItem   *ui;
    fs::path         file_name{name};
    std::string_view s{name};

    switch (s.empty() ? '\0' : s.front())
    {
    // later add URL handling
    case ':':
        s.remove_prefix(1);
        // FALL THROUGH

    default:
    {
        // XXX later have error checking on length
        const fs::path current_file{g_univ.fname};
        file_name = current_file.has_parent_path() ? current_file.parent_path() : fs::path{"/"};
        file_name /= s;
    }
        // FALL THROUGH

    case '~': // ...or full file names
    case '%':
    case '/':
        ui = g_univ.add(UniversalTextFile{}, desc);
        ui->text_file().fname = file_exp(file_name.generic_string());
        break;
    }
}

// mostly the same as the newsgroup stuff
void VirtualGroupBuilder::add_virtual_group(std::string_view grpname)
{
    UniversalItem *ui;

    if (grpname.data() == nullptr)
    {
        return;
    }

    // later check grpname for bad things?

    // perhaps leave if group has no unread, or other factor
    virt_pass_needed = true;
    const std::string group_name{grpname};
    if (g_univ.vg_names.find(group_name) != g_univ.vg_names.end())
    {
        // group was already added
        // perhaps it is marked as deleted?
        for (UniversalItem &item : univ_items())
        {
            const UniversalVirtualGroup *vgroup = std::get_if<UniversalVirtualGroup>(&item.m_data);
            if (vgroup == nullptr)
            {
                continue;
            }
            if (item.m_state == UIS_DESELECTED && vgroup->ng == group_name)
            {
                // undelete the newsgroup
                item.m_state = UIS_NORMAL;
            }
        }
        return;
    }
    g_univ.vg_names.insert(group_name);
    ui = g_univ.add(UniversalVirtualGroup{}, {});
    UniversalVirtualGroup &vgroup = ui->vgroup();
    vgroup.flags = UF_VG_NONE;
    vgroup.min_score = 0;
    vgroup.max_score = 0;
    if (use_min_score)
    {
        vgroup.flags |= UF_VG_MIN_SCORE;
        vgroup.min_score = min_score;
    }
    vgroup.ng = group_name;
}

// univ_DoMatch uses a modified Wildmat function which is
// based on Rich $alz's wildmat, reduced to the simple case of *
// and text.  The complete version can be found in wildmat.c.
//

//
//  Match text and pattern, return true, false.
//
static bool univ_do_match(std::string_view text, std::string_view pattern)
{
    while (!pattern.empty())
    {
        if (pattern.front() == '*')
        {
            const std::size_t pattern_start = pattern.find_first_not_of('*');
            if (pattern_start == std::string_view::npos)
            {
                // Trailing star matches everything.
                return true;
            }
            pattern.remove_prefix(pattern_start);
            while (!text.empty())
            {
                if (univ_do_match(text, pattern))
                {
                    return true;
                }
                text.remove_prefix(1);
            }
            return false;
        }
        if (text.empty() || text.front() != pattern.front())
        {
            return false;
        }
        text.remove_prefix(1);
        pattern.remove_prefix(1);
    }
    return text.empty();
}

// type: 0=newsgroup, 1=virtual (more in future?)
void UniversalMenuLoader::use_pattern(std::string_view pattern, int type)
{
    NewsgroupData *np;

    if (pattern.empty())
    {
        fmt::print("\ngroup pattern: empty regular expression\n");
        return;
    }
    // XXX later: match all newsgroups in current datasrc to the pattern.
    // XXX later do a quick check to see if the group is a simple one.

    if (pattern.front() == '!')
    {
        pattern.remove_prefix(1);
        switch (type)
        {
        case 0:
            for (UniversalItem &item : univ_items())
            {
                const UniversalNewsgroup *newsgroup = std::get_if<UniversalNewsgroup>(&item.m_data);
                if (newsgroup == nullptr)
                {
                    continue;
                }
                if (item.m_state == UIS_NORMAL && univ_do_match(newsgroup->ng, pattern))
                {
                    item.m_state = UIS_DESELECTED;
                }
            }
            break;

        case 1:
            for (UniversalItem &item : univ_items())
            {
                const UniversalVirtualGroup *vgroup = std::get_if<UniversalVirtualGroup>(&item.m_data);
                if (vgroup == nullptr)
                {
                    continue;
                }
                if (item.m_state == UIS_NORMAL && univ_do_match(vgroup->ng, pattern))
                {
                    item.m_state = UIS_DESELECTED;
                }
            }
            break;
        }
    }
    else
    {
        switch (type)
        {
        case 0:
            for (np = g_ng_order.first(); np; np = newsgroup_next(np))
            {
                if (univ_do_match(np->rc_line(), pattern))
                {
                    univ_add_group(np->rc_line(), np->rc_line());
                }
            }
            break;

        case 1:
            for (np = g_ng_order.first(); np; np = newsgroup_next(np))
            {
                if (univ_do_match(np->rc_line(), pattern))
                {
                    g_vg_builder.add_virtual_group(np->rc_line());
                }
            }
            break;
        }
    }
}

// interprets a line of newsgroups, adding or subtracting each pattern
// Newsgroup patterns are separated by spaces and/or commas
void UniversalMenuLoader::use_group_line(std::string_view line, int type)
{
    if (line.empty())
    {
        return;
    }

    // newsgroup patterns will be separated by space(s) and/or comma(s)
    while (!line.empty())
    {
        const std::size_t pattern_start = line.find_first_not_of(" ,");
        if (pattern_start == std::string_view::npos)
        {
            break;
        }
        line.remove_prefix(pattern_start);
        const std::size_t pattern_end = line.find_first_of(" ,");
        if (pattern_end == std::string_view::npos)
        {
            use_pattern(line, type);
            break;
        }
        use_pattern(line.substr(0, pattern_end), type);
        line.remove_prefix(pattern_end);
    }
}

// returns true on success, false otherwise
bool UniversalMenuLoader::use_file(std::string_view fname, std::string_view label)
{
    bool begin_top = true; // default assumption (might be changed later)

    std::string_view file_name = fname;
    std::string      open_name{file_name};
    bool             have_open_name = true;
    // open URLs and translate them into local temporary filenames
    if (string_case_equal(std::string_view{open_name}.substr(0, 4), "URL:"))
    {
        open_name = temp_filename();
        g_univ.tmp_file = open_name;

        if (!url_get(file_name.substr(4), open_name))
        {
            have_open_name = false;
        }
        begin_top = false; // we will need a "begin group"
    }
    else if (!file_name.empty() && file_name.front() == ':') // relative to last file's directory
    {
        fmt::print("Colon filespec not supported for |{}|\n", open_name);
        have_open_name = false;
    }
    if (!have_open_name)
    {
        return false;
    }
    begin_found = begin_top;
    begin_label.clear();
    if (!label.empty())
    {
        begin_label = label;
    }
    std::ifstream input{file_exp(open_name)};
    if (!input)
    {
        return false; // unsuccessful (XXX: complain)
    }
    // Later considerations:
    // 1. Long lines
    // 2. Backslash continuations
    //
    std::string line;
    while (std::getline(input, line))
    {
        if (!do_line(line))
        {
            break; // end of useful file
        }
    }
    if (!begin_found)
    {
        fmt::print("\"begin group\" not found.\n");
    }
    if (!begin_label.empty())
    {
        fmt::print("label not found: {}\n", begin_label);
    }
    if (g_vg_builder.virt_pass_needed)
    {
        g_vg_builder.virt_pass();
    }
    g_univ.sort();
    return true;
}

bool UniversalMenuLoader::include_file(std::string_view fname)
{
    const std::string old_univ_fname = g_univ.fname;
    g_univ.fname = fname;
    bool retval = use_file(g_univ.fname, {});
    g_univ.fname = old_univ_fname;
    return retval;
}

// do the '$' extensions of the line.
void UniversalMenuLoader::do_line_ext1(std::string_view desc, std::string_view line)
{
    if (line.empty())
    {
        return;
    }
    line.remove_prefix(1);
    if (line.empty())
    {
        return;
    }
    switch (line.front())
    {
    case 'v':
    {
        line.remove_prefix(1);
        if (line.empty())
        {
            break;
        }
        switch (line.front())
        {
        case '0': // test vector: "desc" $v0
            line.remove_prefix(1);
            (void) univ_add_virt_num(!desc.empty() ? desc : line, "news.software.readers", ArticleNum{15000});
            break;

        case '1': // "desc" $v1 1500 news.admin
        {
            line.remove_prefix(1);
            const std::size_t number_start = line.find_first_not_of(" \f\n\r\t\v");
            if (number_start == std::string_view::npos)
            {
                break;
            }
            line.remove_prefix(number_start);
            std::size_t number_end = line.find_first_not_of("0123456789");
            if (number_end == std::string_view::npos)
            {
                number_end = line.size();
            }
            const std::string_view art_num_text = line.substr(0, number_end);
            long                   art_num{};
            if (!art_num_text.empty())
            {
                (void) std::from_chars(art_num_text.data(), art_num_text.data() + art_num_text.size(), art_num);
            }
            if (number_end < line.size())
            {
                (void) univ_add_virt_num(!desc.empty() ? desc : line, line.substr(number_end + 1), ArticleNum{art_num});
            }
            break;
        }

        case 'g': // $vg [scorenum] news.* !news.foo.*
        {
            line.remove_prefix(1);
            const std::size_t pattern_start = line.find_first_not_of(" \f\n\r\t\v");
            if (pattern_start == std::string_view::npos)
            {
                break;
            }
            line.remove_prefix(pattern_start);
            std::string_view score_text = line;
            if (!score_text.empty() && (score_text.front() == '+' || score_text.front() == '-'))
            {
                score_text.remove_prefix(1);
            }
            const std::size_t score_offset = static_cast<std::size_t>(score_text.data() - line.data());
            const std::size_t score_digits_end = score_text.find_first_not_of("0123456789");
            const std::size_t score_end =
                score_digits_end == std::string_view::npos ? line.size() : score_digits_end + score_offset;
            if (score_end < line.size() && std::isspace(static_cast<unsigned char>(line[score_end])))
            {
                score_text = line.substr(0, score_end);
                bool negative = false;
                if (!score_text.empty() && (score_text.front() == '+' || score_text.front() == '-'))
                {
                    negative = score_text.front() == '-';
                    score_text.remove_prefix(1);
                }
                int score{};
                if (!score_text.empty())
                {
                    (void) std::from_chars(score_text.data(), score_text.data() + score_text.size(), score);
                }
                g_vg_builder.min_score = negative ? -score : score;
                g_vg_builder.use_min_score = true;
                line.remove_prefix(score_end + 1);
            }
            use_group_line(line, 1);
            g_vg_builder.use_min_score = false;
            break;
        }
        }
        break;
    }

    case 't': // text file
        line.remove_prefix(1);
        if (line.empty())
        {
            break;
        }
        switch (line.front())
        {
        case '0':             // test vector: "desc" $t0
            line.remove_prefix(1);
            univ_add_text_file(!desc.empty() ? desc : line, "/home/c/caadams/ztext");
            break;
        }
        break;

    default:
          break;
    }
}

// returns false when no more lines should be interpreted
bool UniversalMenuLoader::do_line(std::string_view line)
{
    std::string relative_file;

    if (!line.empty() && line.back() == '\n')
    {
        line.remove_suffix(1);
    }

    const std::size_t line_start = line.find_first_not_of(" \f\n\r\t\v");
    if (line_start == std::string_view::npos)
    {
        return true; // empty line
    }
    line.remove_prefix(line_start);

    if (!begin_found)
    {
        if (!string_case_equal(line.substr(0, 11), "begin group"))
        {
            return true; // wait until "begin group" is found
        }
        begin_found = true;
    }
    if (!begin_label.empty())
    {
        if (line.size() >= 2 && line[0] == '>' && line[1] == ':' && line.substr(2) == begin_label)
        {
            begin_label.clear(); // interpret starting at next line
        }
        return true;
    }
    line_desc.clear();
    if (line.front() == '"') // description name
    {
        std::string_view description_text = line.substr(1);
        std::string      description;
        description.reserve(description_text.size());
        std::size_t description_size{};
        while (description_size < description_text.size())
        {
            if (description_text[description_size] == '\\' && description_size + 1 < description_text.size() &&
                description_text[description_size + 1] == '"')
            {
                ++description_size;
            }
            else if (description_text[description_size] == '"')
            {
                break;
            }
            description += description_text[description_size];
            ++description_size;
        }
        if (description_size == description_text.size())
        {
            fmt::print("univ: unmatched quote in string:\n\"{}\"\n", description);
            return true;
        }
        line_desc = description;
        line.remove_prefix(description_size + 2);
    }
    const std::size_t text_start = line.find_first_not_of(" \f\n\r\t\v");
    line = text_start == std::string_view::npos ? std::string_view{} : line.substr(text_start);
    const std::string_view desc{line_desc};
    if (string_case_equal(line.substr(0, 9), "end group"))
    {
        return false;
    }
    if (string_case_equal(line.substr(0, 4), "URL:"))
    {
        std::string_view  label;
        const std::size_t label_pos = line.find('>');
        if (label_pos != std::string_view::npos && label_pos + 1 < line.size())
        {
            label = line.substr(label_pos + 1);
            // XXX later do more error checking
        }
        // description defaults to name
        univ_add_file(!desc.empty() ? desc : line, line, label);
    }
    else if (!line.empty())
    {
        switch (line.front())
        {
        case '#': // comment
            break;

        case ':': // relative to g_univ.fname
            if (!g_univ.fname.empty())
            {
                const fs::path current_file{g_univ.fname};
                fs::path relative_path = current_file.has_parent_path() ? current_file.parent_path() : fs::path{"/"};
                relative_path /= line.substr(1);
                relative_file = relative_path.generic_string();
                line = relative_file;
            } // XXX later have else which will complain
            // FALL THROUGH

        case '~': // ...or full file names
        case '%':
        case '/':
        {
            std::string       file_name{line};
            std::string       label;
            const std::size_t label_pos = file_name.find('>');
            if (label_pos != std::string::npos)
            {
                label = file_name.substr(label_pos + 1);
                file_name.erase(label_pos);
            }
            // description defaults to name
            univ_add_file(!desc.empty() ? desc : std::string_view{file_name}, file_exp(file_name), label);
            break;
        }

        case '-': // label within same file
            if (line.size() < 2 || line[1] != '>')
            {
                // XXX give an error message later
                break;
            }
            line.remove_prefix(2);
            univ_add_file(!desc.empty() ? desc : line, g_univ.fname, line);
            break;

        case '>':
            if (line.size() >= 2 && line[1] == ':')
            {
                return false; // label found, end of previous block
            }
            break; // just ignore the line (print warning later?)

        case '@':  // virtual newsgroup file
            break; // not used now

        case '&': // text file shortcut (for help files)
            line.remove_prefix(1);
            univ_add_text_file(!desc.empty() ? desc : line, line);
            break;

        case '$': // extension 1
            do_line_ext1(desc, line);
            break;

        default:
            // if there is a description, this must be a restriction list
            if (!desc.empty())
            {
                univ_add_mask(desc, line);
                break;
            }
            // one or more newsgroups instead
            use_group_line(line, 0);
            break;
        }
    }
    else if (!desc.empty())
    {
        univ_add_mask(desc, line);
    }
    return true; // continue reading
}

// features to return later (?):
//   text files
//

// level generator
bool univ_file_load(std::string_view fname, std::string_view title, std::string_view label)
{
    univ_open();

    if (!fname.empty())
    {
        g_univ.fname = fname;
    }
    g_univ.title = title;
    if (!label.empty())
    {
        g_univ.label = label;
    }
    bool flag = !fname.empty() && g_univ_loader.use_file(fname, label);
    if (!flag)
    {
        univ_close();
    }
    if (g_int_count)
    {
        g_int_count = 0;
    }
    if (finput_pending(true))
    {
        // later, *maybe* eat input
    }
    return flag;
}

// level generator
void univ_mask_load(std::string_view mask, std::string_view title)
{
    univ_open();

    g_univ_loader.use_group_line(mask, 0);
    g_univ.title = title;
    if (g_int_count)
    {
        g_int_count = 0;
    }
}

void univ_redo_file()
{
    const std::string tmp_fname = g_univ.fname;
    const std::string tmp_title = g_univ.title;
    const std::string tmp_label = g_univ.label;

    univ_close();
    if (g_univ.level)
    {
        (void)univ_file_load(tmp_fname, tmp_title, tmp_label);
    }
    else
    {
        univ_startup();
    }
}

static std::string univ_edit_new_user_file()
{
    const fs::path user_top{file_exp("%+/univ/usertop")};

    // later, create a new user top file, and return its filename.
    // later perhaps ask whether to create or edit current file.
    // note: the user may need to restart in order to use the new file.
    //       (trn could do a univ_redofile, but it may be confusing.)
    //

    // if the file exists, do not create a new one
    std::FILE *fp = std::fopen(user_top.string().c_str(), "r");
    if (fp)
    {
        std::fclose(fp);
        return g_univ.fname; // as if this function was not called
    }

    std::error_code error;
    fs::create_directories(user_top.parent_path(), error);

    fp = std::fopen(user_top.string().c_str(), "w");
    if (!fp)
    {
        fmt::print("Could not create new user file.\n");
        fmt::print("Editing current system file\n");
        (void) get_anything();
        return g_univ.fname;
    }
    fmt::print(fp, "# User Toplevel (Universal Selector)\n");
    std::fclose(fp);
    fmt::print("New User Toplevel file created.\n");
    fmt::print("After editing this file, exit and restart trn to use it.\n");
    (void) get_anything();
    s_univ_user_top = true;               // do not overwrite this file
    return user_top.string();
}

// code adapted from edit_kfile in kfile.cpp
void univ_edit()
{
    std::string filename;

    if (s_univ_user_top || !(s_univ_done_startup))
    {
        if (!g_univ.tmp_file.empty())
        {
            filename = g_univ.tmp_file;
        }
        else
        {
            filename = g_univ.fname;
        }
    }
    else
    {
        filename = univ_edit_new_user_file();
    }

    // later consider directory push/pop pair around editing
    (void)edit_file(filename);
}

// later use some internal pager
void univ_page_file(const fs::path &fname)
{
    if (fname.empty())
    {
        return;
    }

    const std::string command = fmt::format("{} {}", file_exp(get_env_var("HELPPAGER", get_env_var("PAGER", "more"))),
                                            file_exp(fname.generic_string()));
    term_down(3);
    reset_tty();                  // make sure tty is friendly
    do_shell(SH, command);        // invoke the shell
    no_echo();                   // and make terminal
    cr_mode();                   // unfriendly again
    // later: consider something else that will return the key, and
    //        returning different codes based on the key.
    //
    if (command.rfind("more ", 0) == 0)
    {
        get_anything();
    }
}

// virtual newsgroup second pass function
// called from within newsgroup
void univ_newsgroup_virtual()
{
    UniversalItem *current_vg_ui = g_univ.item(g_vg_builder.current_vg_ui_index);
    if (current_vg_ui == nullptr)
    {
        return;
    }

    if (std::holds_alternative<UniversalVirtualGroup>(current_vg_ui->m_data))
    {
        g_vg_builder.vg_add_group();
    }
    else if (std::holds_alternative<UniversalVirtualArticle>(current_vg_ui->m_data))
    {
        // get article number from message-id
    }

    // later, get subjects and article numbers when needed
    // also do things like check scores, add newsgroups, etc.
}

void VirtualGroupBuilder::vg_add_article(ArticleNum a)
{
    int score = sc_score_art(a, false);
    if (use_min_score && (score < min_score))
    {
        return;
    }
    std::string subject = fetch_subj_copy(a);
    if (subject.empty())
    {
        return;
    }
    std::string from = prefetch_lines_copy(a, FROM_LINE);
    if (from.empty())
    {
        from = "<No Author>";
    }

    // later scan/replace bad characters

    // later consider author in description, scoring, etc.
    UniversalItem *ui = univ_add_virt_num({}, g_newsgroup_name, a);
    ui->m_score = score;
    UniversalVirtualArticle &article = ui->article();
    article.subj = subject;
    article.from = from;
}


void VirtualGroupBuilder::vg_add_group()
{
    // later: allow was-read articles, etc...
    for (ArticleNum a = g_cache.first(g_first_art); a <= g_last_art; a = g_cache.next(a))
    {
        if (!g_cache.unread(a))
        {
            continue;
        }
        // minimum score check
        vg_add_article(a);
    }
}

// returns do_newsgroup() value
int univ_visit_group_main(std::string_view gname)
{
    if (gname.empty())
    {
        return NG_ERROR;
    }
    const std::string group_name{gname};

    NewsgroupData *np = find_newsgroup(gname);
    if (!np)
    {
        fmt::print("Univ/Virt: newsgroup {} not found!", group_name);
        return NG_ERROR;
    }
    // unsubscribed, bogus, etc. groups are not visited
    if (np->m_to_read <= TR_UNSUB)
    {
      return NG_ERROR;
    }

    set_newsgroup(np);
    if (np != g_current_newsgroup)
    {
        // probably unnecessary...
        g_recent_newsgroup = g_current_newsgroup;
        g_current_newsgroup = np;
    }
    bool old_threaded = g_threaded_group;
    g_threaded_group = (g_use_threads && !(np->m_flags & NF_UNTHREADED));
    fmt::print("\nScanning newsgroup {}\n", group_name);
    int ret = do_newsgroup(std::string{});
    g_threaded_group = old_threaded;
    return ret;
}

// LATER: allow the loop to be interrupted
void VirtualGroupBuilder::virt_pass()
{
    virt_pass(univ_visit_group);
}

// LATER: allow the loop to be interrupted
void VirtualGroupBuilder::virt_pass(UniversalGroupVisitor visit_group)
{
    virt_pass(visit_group, input_pending);
}

// LATER: allow the loop to be interrupted
void VirtualGroupBuilder::virt_pass(UniversalGroupVisitor visit_group, UniversalInputPending input_is_pending)
{
    g_univ_ng_virt_flag = true;
    virt_pass_needed = false;

    for (std::size_t position = 0; position < g_univ.items.size();)
    {
        if (input_is_pending != nullptr && input_is_pending())
        {
            // later consider cleaning up the remains
            break;
        }
        UniversalItem *ui = &g_univ.items[position];
        if (ui->m_state != UIS_NORMAL)
        {
            ++position;
            continue;
        }
        UniversalVirtualGroup *vgroup = std::get_if<UniversalVirtualGroup>(&ui->m_data);
        if (vgroup != nullptr)
        {
            if (vgroup->ng.empty())
            {
                ++position;             // XXX whine
                continue;
            }
            const UniversalItemIndex current_index = univ_index(ui);
            const std::string        group_name = vgroup->ng;
            current_vg_ui_index = current_index;
            if (vgroup->flags & UF_VG_MIN_SCORE)
            {
                use_min_score = true;
                min_score = vgroup->min_score;
            }
            (void)visit_group(group_name);
            use_min_score = false;
            current_vg_ui_index = {};
            // later do something with return value
            const std::size_t expanded_position = g_univ.position(current_index);
            if (expanded_position < g_univ.items.size())
            {
                g_univ.items.erase(g_univ.items.begin() +
                                   static_cast<UniversalItemList::difference_type>(expanded_position));
                position = expanded_position;
                continue;
            }
            ++position;
            continue;
        }

        const UniversalVirtualArticle *article = std::get_if<UniversalVirtualArticle>(&ui->m_data);
        if (article != nullptr)
        {
            // if article number is not set, visit newsgroup with callback
            // later also check for descriptions
            if ((article->num) && (!ui->m_desc.empty()))
            {
                ++position;
                continue;
            }
            if (!article->subj.empty())
            {
                ++position;
                continue;
            }
            const std::string group_name = article->ng;
            current_vg_ui_index = univ_index(ui);
            (void)visit_group(group_name);
            current_vg_ui_index = {};
            // later do something with return value
        }
        ++position;
    }
    current_vg_ui_index = {};
    g_univ_ng_virt_flag = false;
}

void UniversalMenuContext::sort()
{
    if (items.size() <= 1)
    {
        return;
    }

    switch (g_sel.sort)
    {
    case SS_SCORE:
        std::sort(items.begin(), items.end(),
                  [](const UniversalItem &left, const UniversalItem &right)
                  {
                      if (left.m_score != right.m_score)
                      {
                          return g_sel.direction > 0 ? left.m_score > right.m_score : left.m_score < right.m_score;
                      }
                      return g_sel.direction > 0 ? left.m_num < right.m_num : left.m_num > right.m_num;
                  });
        break;

    case SS_NATURAL:
    default:
        std::sort(items.begin(), items.end(), [](const UniversalItem &left, const UniversalItem &right)
                  { return g_sel.direction > 0 ? left.m_num < right.m_num : left.m_num > right.m_num; });
        break;
    }
}

// return a description of the article
// do this better later, like the code in sadesc.cpp
std::string UniversalItem::univ_article_desc() const
{
    const UniversalVirtualArticle &art = article();
    std::string from = art.from.empty() ? "<No Author> " : compress_from(art.from, 16);
    if (from.size() > 16)
    {
        from.resize(16);
    }

    std::string subject;
    if (art.subj.empty())
    {
        subject = "<No Subject>";
    }
    else if (art.subj.size() >= 4 && art.subj[0] == 'R' && art.subj[1] == 'e' //
             && art.subj[2] == ':' && art.subj[3] == ' ')
    {
        subject = ">";
        subject += art.subj.substr(4, 54);
    }
    else
    {
        subject = art.subj.substr(0, 55);
    }
    if (subject.size() > 55)
    {
        subject.resize(55);
    }

    std::string description = fmt::format("[{:3}] {:>16} {}", m_score, from, subject);
    for (char &ch : description)
    {
        if (ch == Ctl('h') || ch == '\t' || ch == '\n' || ch == '\r')
        {
            ch = ' ';
        }
    }
    if (description.size() > 70)
    {
        description.resize(70);
    }
    return description;
}

UniversalNewsgroup &UniversalItem::group()
{
    assert(std::holds_alternative<UniversalNewsgroup>(m_data));
    return std::get<UniversalNewsgroup>(m_data);
}

const UniversalNewsgroup &UniversalItem::group() const
{
    assert(std::holds_alternative<UniversalNewsgroup>(m_data));
    return std::get<UniversalNewsgroup>(m_data);
}

UniversalVirtualGroup &UniversalItem::vgroup()
{
    assert(std::holds_alternative<UniversalVirtualGroup>(m_data));
    return std::get<UniversalVirtualGroup>(m_data);
}

const UniversalVirtualGroup &UniversalItem::vgroup() const
{
    assert(std::holds_alternative<UniversalVirtualGroup>(m_data));
    return std::get<UniversalVirtualGroup>(m_data);
}

UniversalVirtualArticle &UniversalItem::article()
{
    assert(std::holds_alternative<UniversalVirtualArticle>(m_data));
    return std::get<UniversalVirtualArticle>(m_data);
}

const UniversalVirtualArticle &UniversalItem::article() const
{
    assert(std::holds_alternative<UniversalVirtualArticle>(m_data));
    return std::get<UniversalVirtualArticle>(m_data);
}

UniversalConfigFileData &UniversalItem::config_file()
{
    assert(std::holds_alternative<UniversalConfigFileData>(m_data));
    return std::get<UniversalConfigFileData>(m_data);
}

const UniversalConfigFileData &UniversalItem::config_file() const
{
    assert(std::holds_alternative<UniversalConfigFileData>(m_data));
    return std::get<UniversalConfigFileData>(m_data);
}

UniversalGroupMaskData &UniversalItem::group_mask()
{
    assert(std::holds_alternative<UniversalGroupMaskData>(m_data));
    return std::get<UniversalGroupMaskData>(m_data);
}

const UniversalGroupMaskData &UniversalItem::group_mask() const
{
    assert(std::holds_alternative<UniversalGroupMaskData>(m_data));
    return std::get<UniversalGroupMaskData>(m_data);
}

UniversalTextFile &UniversalItem::text_file()
{
    assert(std::holds_alternative<UniversalTextFile>(m_data));
    return std::get<UniversalTextFile>(m_data);
}

const UniversalTextFile &UniversalItem::text_file() const
{
    assert(std::holds_alternative<UniversalTextFile>(m_data));
    return std::get<UniversalTextFile>(m_data);
}

std::string &UniversalItem::debug_string()
{
    assert(std::holds_alternative<UniversalDebugData>(m_data));
    return std::get<UniversalDebugData>(m_data).text;
}

const std::string &UniversalItem::debug_string() const
{
    assert(std::holds_alternative<UniversalDebugData>(m_data));
    return std::get<UniversalDebugData>(m_data).text;
}

HelpLocation &UniversalItem::help_location()
{
    assert(std::holds_alternative<HelpLocation>(m_data));
    return std::get<HelpLocation>(m_data);
}

HelpLocation UniversalItem::help_location() const
{
    assert(std::holds_alternative<HelpLocation>(m_data));
    return std::get<HelpLocation>(m_data);
}

// Help start

// later: add online help as a new item type, add appropriate item
//        to the new level
//
//int where;    // what context were we in--use later for key help?
void univ_help_main(HelpLocation where)
{
    univ_open();
    g_univ.title = "Extended Help";

    // first add help on current mode
    (void) g_univ.add(where, {});

    // later, do other mode sensitive stuff

    // site-specific help
    g_univ_loader.include_file("%X/sitehelp/top");

    // read in main help file
    g_univ.fname = "%X/HelpFiles/top";
    bool flag = g_univ_loader.use_file(g_univ.fname, g_univ.label);

    // later: if flag is not true, then add message?
}

void univ_help(HelpLocation where)
{
    univ_visit_help(where);     // push old selector info to stack
}

std::string_view UniversalItem::univ_key_help_mode_str() const
{
    switch (help_location())
    {
    case UHELP_PAGE:
        return "Article Pager Mode";

    case UHELP_ART:
        return "Article Display/Selection Mode";

    case UHELP_NG:
        return "Newsgroup Browse Mode";

    case UHELP_NGSEL:
        return "Newsgroup Selector";

    case UHELP_ADDSEL:
        return "Add-Newsgroup Selector";

    case UHELP_SUBS:
        return "Escape Substitutions";

    case UHELP_ARTSEL:
        return "Thread/Subject/Article Selector";

    case UHELP_MULTIRC:
        return "Newsrc Selector";

    case UHELP_OPTIONS:
        return "Option Selector";

    case UHELP_SCANART:
        return "Article Scan Mode";

    case UHELP_UNIV:
        return "Universal Selector";

    default:
        return {};
    }
}
