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

#include <cstdio>
#include <string>
#include <string_view>

std::array<std::string, MAX_NG_TO_DO> g_newsgroup_to_do;       // restrictions in effect
int                                   g_max_newsgroup_to_do{}; // 0 => no restrictions
                                                               // >0 => # of entries in g_ngtodo
char g_empty_only_char{'o'};

static int            s_save_max_newsgroup_to_do{};
static CompiledRegex *s_compex_to_do[MAX_NG_TO_DO]; // restrictions in compiled form

void only_init()
{
}

void set_newsgroup_to_do(std::string_view pat)
{
    int i = g_max_newsgroup_to_do + s_save_max_newsgroup_to_do;

    if (pat.empty())
    {
        return;
    }
    if (i < MAX_NG_TO_DO)
    {
        g_newsgroup_to_do[i].assign(pat);
        s_compex_to_do[i] = new CompiledRegex;
        s_compex_to_do[i]->init_compex();
        s_compex_to_do[i]->compile(g_newsgroup_to_do[i], true, true);
        const char *err = newsgroup_comp(s_compex_to_do[i], pat, true, true);
        if (err != nullptr)
        {
            std::printf("\n%s\n",err);
            finalize(1);
        }
        g_max_newsgroup_to_do++;
    }
}

// if command line list is non-null, is this newsgroup wanted?

bool in_list(std::string_view newsgroup_name)
{
    if (g_max_newsgroup_to_do == 0)
    {
        return true;
    }
    const std::string group_name{newsgroup_name};
    for (int i = s_save_max_newsgroup_to_do; i < g_max_newsgroup_to_do + s_save_max_newsgroup_to_do; i++)
    {
        if (s_compex_to_do[i]->execute(group_name.c_str()))
        {
            return true;
        }
    }
    return false;
}

void end_only()
{
    if (g_max_newsgroup_to_do)                    // did they specify newsgroup(s)
    {
        if (g_verbose)
        {
            g_msg = fmt::format("Restriction {}{} removed.", g_newsgroup_to_do[0],
                                g_max_newsgroup_to_do > 1 ? ", etc." : "");
        }
        else
        {
            g_msg = "Exiting \"only\".";
        }
        for (int i = s_save_max_newsgroup_to_do; i < g_max_newsgroup_to_do + s_save_max_newsgroup_to_do; i++)
        {
            g_newsgroup_to_do[i].clear();
            s_compex_to_do[i]->free_compex();
            delete s_compex_to_do[i];
            s_compex_to_do[i] = nullptr;
        }
        g_max_newsgroup_to_do = 0;
        g_newsgroup_min_to_read = 1;
    }
}

void push_only()
{
    s_save_max_newsgroup_to_do = g_max_newsgroup_to_do;
    g_max_newsgroup_to_do = 0;
}

void pop_only()
{
    ArticleUnread save_ng_min_toread = g_newsgroup_min_to_read;

    end_only();

    g_max_newsgroup_to_do = s_save_max_newsgroup_to_do;
    s_save_max_newsgroup_to_do = 0;

    g_newsgroup_min_to_read = save_ng_min_toread;
}
