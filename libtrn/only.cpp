/* only.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/only.h>

#include <config/common.h>
#include <trn/final.h>
#include <trn/ngdata.h>
#include <trn/ngsrch.h>
#include <trn/search.h>
#include <trn/trn.h>
#include <trn/util.h>

#include <fmt/format.h>

#include <string>
#include <string_view>

NewsgroupRestrictions g_only;
char g_empty_only_char{'o'};

void only_init()
{
}

void NewsgroupRestrictions::set_to_do(std::string_view pat)
{
    int i = max_to_do + save_max_to_do;

    if (pat.empty())
    {
        return;
    }
    if (i < MAX_NG_TO_DO)
    {
        to_do[i].assign(pat);
        compex_to_do[i] = new CompiledRegex;
        compex_to_do[i]->init_compex();
        compex_to_do[i]->compile(to_do[i], true, true);
        const std::string_view err = newsgroup_comp(*compex_to_do[i], pat, true, true);
        if (!err.empty())
        {
            fmt::print("\n{}\n", err);
            finalize(1);
        }
        max_to_do++;
    }
}

std::string_view NewsgroupRestrictions::restriction(int index)
{
    return to_do[save_max_to_do + index];
}

// if command line list is non-null, is this newsgroup wanted?

bool NewsgroupRestrictions::in_list(std::string_view newsgroup_name)
{
    if (max_to_do == 0)
    {
        return true;
    }
    const std::string group_name{newsgroup_name};
    for (int i = save_max_to_do; i < max_to_do + save_max_to_do; i++)
    {
        if (compex_to_do[i]->execute(group_name.c_str()))
        {
            return true;
        }
    }
    return false;
}

void end_only()
{
    if (g_only.max_to_do) // did they specify newsgroup(s)
    {
        if (g_verbose)
        {
            g_msg = fmt::format("Restriction {}{} removed.", g_only.to_do[0], g_only.max_to_do > 1 ? ", etc." : "");
        }
        else
        {
            g_msg = "Exiting \"only\".";
        }
        for (int i = g_only.save_max_to_do; i < g_only.max_to_do + g_only.save_max_to_do; i++)
        {
            g_only.to_do[i].clear();
            g_only.compex_to_do[i]->free_compex();
            delete g_only.compex_to_do[i];
            g_only.compex_to_do[i] = nullptr;
        }
        g_only.max_to_do = 0;
        g_newsgroup_min_to_read = 1;
    }
}

void push_only()
{
    g_only.save_max_to_do = g_only.max_to_do;
    g_only.max_to_do = 0;
}

void pop_only()
{
    ArticleUnread save_ng_min_toread = g_newsgroup_min_to_read;

    end_only();

    g_only.max_to_do = g_only.save_max_to_do;
    g_only.save_max_to_do = 0;

    g_newsgroup_min_to_read = save_ng_min_toread;
}
