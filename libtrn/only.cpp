/* only.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/only.h"

#include "trn/ngdata.h"
#include "trn/final.h"
#include "trn/ngsrch.h"
#include "trn/search.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cstdio>
#include <cstdlib>

char *g_newsgroup_to_do[MAX_NG_TO_DO]; /* restrictions in effect */
int   g_max_newsgroup_to_do{};       /*  0 => no restrictions */
                           /* >0 => # of entries in g_ngtodo */
char g_empty_only_char{'o'};

static int     s_save_maxngtodo{};
static CompiledRegex *s_compextodo[MAX_NG_TO_DO]; /* restrictions in compiled form */

void only_init()
{
}

void set_newsgroup_to_do(const char *pat)
{
    int i = g_max_newsgroup_to_do + s_save_maxngtodo;

    if (!*pat)
    {
        return;
    }
    if (i < MAX_NG_TO_DO)
    {
        g_newsgroup_to_do[i] = save_str(pat);
#ifndef lint
        s_compextodo[i] = (CompiledRegex*)safe_malloc(sizeof(CompiledRegex));
#endif
        init_compex(s_compextodo[i]);
        compile(s_compextodo[i],pat,true,true);
        const char *err = newsgroup_comp(s_compextodo[i], pat, true, true);
        if (err != nullptr)
        {
            std::printf("\n%s\n",err);
            finalize(1);
        }
        g_max_newsgroup_to_do++;
    }
}

/* if command line list is non-null, is this newsgroup wanted? */

bool in_list(const char *newsgroup_name)
{
    if (g_max_newsgroup_to_do == 0)
    {
        return true;
    }
    for (int i = s_save_maxngtodo; i < g_max_newsgroup_to_do + s_save_maxngtodo; i++)
    {
        if (execute(s_compextodo[i],newsgroup_name))
        {
            return true;
        }
    }
    return false;
}

void end_only()
{
    if (g_max_newsgroup_to_do)                    /* did they specify newsgroup(s) */
    {
        if (g_verbose)
        {
            std::sprintf(g_msg, "Restriction %s%s removed.",g_newsgroup_to_do[0],
                   g_max_newsgroup_to_do > 1 ? ", etc." : "");
        }
        else
        {
            std::sprintf(g_msg, "Exiting \"only\".");
        }
        for (int i = s_save_maxngtodo; i < g_max_newsgroup_to_do + s_save_maxngtodo; i++)
        {
            std::free(g_newsgroup_to_do[i]);
            free_compex(s_compextodo[i]);
#ifndef lint
            std::free((char*)s_compextodo[i]);
#endif
        }
        g_max_newsgroup_to_do = 0;
        g_newsgroup_min_to_read = 1;
    }
}

void push_only()
{
    s_save_maxngtodo = g_max_newsgroup_to_do;
    g_max_newsgroup_to_do = 0;
}

void pop_only()
{
    ArticleUnread save_ng_min_toread = g_newsgroup_min_to_read;

    end_only();

    g_max_newsgroup_to_do = s_save_maxngtodo;
    s_save_maxngtodo = 0;

    g_newsgroup_min_to_read = save_ng_min_toread;
}
