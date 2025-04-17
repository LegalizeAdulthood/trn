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

char *g_ngtodo[MAXNGTODO]; /* restrictions in effect */
int   g_maxngtodo{};       /*  0 => no restrictions */
                           /* >0 => # of entries in g_ngtodo */
char g_empty_only_char{'o'};

static int     s_save_maxngtodo{};
static CompiledRegex *s_compextodo[MAXNGTODO]; /* restrictions in compiled form */

void only_init()
{
}

void setngtodo(const char *pat)
{
    int i = g_maxngtodo + s_save_maxngtodo;

    if (!*pat)
    {
        return;
    }
    if (i < MAXNGTODO)
    {
        g_ngtodo[i] = savestr(pat);
#ifndef lint
        s_compextodo[i] = (CompiledRegex*)safemalloc(sizeof(CompiledRegex));
#endif
        init_compex(s_compextodo[i]);
        compile(s_compextodo[i],pat,true,true);
        const char *err = newsgroup_comp(s_compextodo[i], pat, true, true);
        if (err != nullptr)
        {
            std::printf("\n%s\n",err);
            finalize(1);
        }
        g_maxngtodo++;
    }
}

/* if command line list is non-null, is this newsgroup wanted? */

bool inlist(const char *ngnam)
{
    if (g_maxngtodo == 0)
    {
        return true;
    }
    for (int i = s_save_maxngtodo; i < g_maxngtodo + s_save_maxngtodo; i++)
    {
        if (execute(s_compextodo[i],ngnam))
        {
            return true;
        }
    }
    return false;
}

void end_only()
{
    if (g_maxngtodo)                    /* did they specify newsgroup(s) */
    {
        if (g_verbose)
        {
            std::sprintf(g_msg, "Restriction %s%s removed.",g_ngtodo[0],
                   g_maxngtodo > 1 ? ", etc." : "");
        }
        else
        {
            std::sprintf(g_msg, "Exiting \"only\".");
        }
        for (int i = s_save_maxngtodo; i < g_maxngtodo + s_save_maxngtodo; i++)
        {
            std::free(g_ngtodo[i]);
            free_compex(s_compextodo[i]);
#ifndef lint
            std::free((char*)s_compextodo[i]);
#endif
        }
        g_maxngtodo = 0;
        g_newsgroup_min_to_read = 1;
    }
}

void push_only()
{
    s_save_maxngtodo = g_maxngtodo;
    g_maxngtodo = 0;
}

void pop_only()
{
    ArticleUnread save_ng_min_toread = g_newsgroup_min_to_read;

    end_only();

    g_maxngtodo = s_save_maxngtodo;
    s_save_maxngtodo = 0;

    g_newsgroup_min_to_read = save_ng_min_toread;
}
