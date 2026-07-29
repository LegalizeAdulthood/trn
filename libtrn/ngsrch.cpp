/* ngsrch.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngsrch.h>

#include <config/common.h>
#include <trn/addng.h>
#include <trn/final.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngstuff.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/search.h>
#include <trn/terminal.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

static bool          s_newsgroup_do_empty{}; // search empty newsgroups?
static CompiledRegex s_newsgroup_compex;

void newsgroup_search_init()
{
    s_newsgroup_do_empty = false;
    s_newsgroup_compex.init_compex();
}

NewsgroupSearchResult newsgroup_search(std::string_view command, bool get_cmd)
{
    g_int_count = 0;
    if (command.empty())
    {
        return NGS_ABORT;
    }

    std::string completed_command;
    if (get_cmd)
    {
        completed_command = finish_command(command.substr(0, 1), false); // get rest of command
        if (completed_command.empty())
        {
            return NGS_ABORT;
        }
        command = completed_command;
    }

    perform_status_init(g_newsgroup_to_read.value_of());
    const char             cmdchr = command.front(); // what kind of search?
    const std::string_view search_text = command.substr(1);
    std::string            pattern_text;
    pattern_text.reserve(search_text.size());
    std::size_t tail_start{};
    while (tail_start < search_text.size())
    {
        if (search_text[tail_start] == '\\' && tail_start + 1 < search_text.size() &&
            search_text[tail_start + 1] == cmdchr)
        {
            ++tail_start;
        }
        else if (search_text[tail_start] == cmdchr)
        {
            break;
        }
        pattern_text += search_text[tail_start];
        ++tail_start;
    }
    std::string_view pattern{pattern_text}; // unparsed pattern
    while (!pattern.empty() && pattern.front() == ' ')
    {
        pattern.remove_prefix(1);
    }
    if (!pattern.empty())
    {
        s_newsgroup_do_empty = false;
    }

    std::string_view modifier_tail = search_text.substr(tail_start);
    if (!modifier_tail.empty()) // modifiers or commands?
    {
        modifier_tail.remove_prefix(1);
        std::size_t modifier_pos{};
        bool        done_modifiers{};
        while (modifier_pos < modifier_tail.size() && !done_modifiers)
        {
            switch (modifier_tail[modifier_pos])
            {
            case 'r':
                s_newsgroup_do_empty = true;
                ++modifier_pos;
                break;

            default:
                done_modifiers = true;
                break;
            }
        }
        modifier_tail.remove_prefix(modifier_pos);
    }
    const std::string_view::const_iterator command_begin =
        std::find_if_not(modifier_tail.begin(), modifier_tail.end(),
                         [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) || ch == ':'; });
    modifier_tail.remove_prefix(static_cast<std::size_t>(command_begin - modifier_tail.begin()));
    std::string cmdlst; // list of commands to do
    if (!modifier_tail.empty())
    {
        cmdlst = modifier_tail;
    }
    else if (g_general_mode == GM_SELECTOR)
    {
        cmdlst = "+";
    }
    NewsgroupSearchResult ret = NGS_NOT_FOUND; // assume no commands
    if (!cmdlst.empty())
    {
        ret = NGS_DONE;
    }
    const char *err = newsgroup_comp(&s_newsgroup_compex, pattern, true, true);
    if (err != nullptr)
    {
                                        // compile regular expression
        error_msg(err);
        return NGS_ERROR;
    }
    if (cmdlst.empty())
    {
        fmt::print("\nSearching..."); // give them something to read
        std::fflush(stdout);
    }

    const bool output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    if (g_first_add_group)
    {
        AddGroup *gp = g_first_add_group;
        do
        {
            if (s_newsgroup_compex.execute(gp->m_name.c_str()) != nullptr)
            {
                if (cmdlst.empty())
                {
                    return NGS_FOUND;
                }
                if (gp->add_group_perform(cmdlst, output_level && g_page_line == 1) < 0)
                {
                    return NGS_INTR;
                }
            }
            if (!output_level && g_page_line == 1)
            {
                perform_status(g_newsgroup_to_read.value_of(), 50);
            }
        } while ((gp = gp->m_next) != nullptr);
        return ret;
    }

    const bool           backward = cmdchr == '?'; // direction of search
    const NewsgroupData *ng_start = g_newsgroup_ptr;
    if (backward)
    {
        if (!g_newsgroup_ptr)
        {
            g_newsgroup_ptr = newsgroup_last();
            ng_start = newsgroup_last();
        }
        else if (cmdlst.empty())
        {
            if (g_newsgroup_ptr == newsgroup_first()) // skip current newsgroup
            {
                g_newsgroup_ptr = newsgroup_last();
            }
            else
            {
                g_newsgroup_ptr = newsgroup_prev(g_newsgroup_ptr);
            }
        }
    }
    else
    {
        if (!g_newsgroup_ptr)
        {
            g_newsgroup_ptr = newsgroup_first();
            ng_start = newsgroup_first();
        }
        else if (cmdlst.empty())
        {
            if (g_newsgroup_ptr == newsgroup_last()) // skip current newsgroup
            {
                g_newsgroup_ptr = newsgroup_first();
            }
            else
            {
                g_newsgroup_ptr = newsgroup_next(g_newsgroup_ptr);
            }
        }
    }

    if (!g_newsgroup_ptr)
    {
        return NGS_NOT_FOUND;
    }

    do
    {
        if (g_int_count)
        {
            g_int_count = 0;
            ret = NGS_INTR;
            break;
        }

        if (g_newsgroup_ptr->m_to_read >= TR_NONE && g_newsgroup_ptr->newsgroup_wanted())
        {
            if (g_newsgroup_ptr->m_to_read == TR_NONE)
            {
                g_newsgroup_ptr->set_to_read(ST_LAX);
            }
            if (s_newsgroup_do_empty || ((g_newsgroup_ptr->m_to_read > TR_NONE) ^ g_sel_rereading))
            {
                if (cmdlst.empty())
                {
                    return NGS_FOUND;
                }
                set_newsgroup(g_newsgroup_ptr);
                if (newsgroup_perform(cmdlst, output_level && g_page_line == 1) < 0)
                {
                    return NGS_INTR;
                }
            }
            if (output_level && cmdlst.empty())
            {
                fmt::print("\n[0 unread in {} -- skipping]", g_newsgroup_ptr->rc_line());
                std::fflush(stdout);
            }
        }
        if (!output_level && g_page_line == 1)
        {
            perform_status(g_newsgroup_to_read.value_of(), 50);
        }
    } while ((g_newsgroup_ptr =
                  (backward ? (newsgroup_prev(g_newsgroup_ptr) ? newsgroup_prev(g_newsgroup_ptr) : newsgroup_last())
                            : (newsgroup_next(g_newsgroup_ptr) ? newsgroup_next(g_newsgroup_ptr)
                                                               : newsgroup_first()))) != ng_start);

    return ret;
}

bool NewsgroupData::newsgroup_wanted()
{
    return s_newsgroup_compex.execute(m_rc_line.c_str()) != nullptr;
}

const char *newsgroup_comp(CompiledRegex *compex, std::string_view pattern, bool re, bool fold)
{
    if (pattern.empty())
    {
        if (compex->compile("", re, fold))
        {
            return "No previous search pattern";
        }
        return nullptr; // reuse old pattern
    }

    std::string ng_pattern;
    for (const char ch : pattern)
    {
        if (ch == '.')
        {
            ng_pattern.push_back('\\');
            ng_pattern.push_back(ch);
        }
        else if (ch == '?')
        {
            ng_pattern.push_back('.');
        }
        else if (ch == '*')
        {
            ng_pattern.push_back('.');
            ng_pattern.push_back(ch);
        }
        else
        {
            ng_pattern.push_back(ch);
        }
    }
    return compex->compile(ng_pattern, re, fold);
}
