/* only.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include "common.h"
#include "only.h"

#include "final.h"
#include "ngdata.h"
#include "ngsrch.h"
#include "search.h"
#include "trn.h"
#include "util.h"
#include "util2.h"

char *g_ngtodo[MAXNGTODO]; /* restrictions in effect */
int   g_maxngtodo{};       /*  0 => no restrictions */
                           /* >0 => # of entries in g_ngtodo */
char g_empty_only_char{'o'};

static int     s_save_maxngtodo{};
static COMPEX *s_compextodo[MAXNGTODO]; /* restrictions in compiled form */

void only_init()
{
}

void setngtodo(const char *pat)
{
    int i = g_maxngtodo + s_save_maxngtodo;

    if (!*pat)
        return;
    if (i < MAXNGTODO) {
        g_ngtodo[i] = savestr(pat);
#ifndef lint
        s_compextodo[i] = (COMPEX*)safemalloc(sizeof(COMPEX));
#endif
        init_compex(s_compextodo[i]);
        compile(s_compextodo[i],pat,true,true);
        const char *err = ng_comp(s_compextodo[i], pat, true, true);
        if (err != nullptr)
        {
            printf("\n%s\n",err) FLUSH;
            finalize(1);
        }
        g_maxngtodo++;
    }
}

/* if command line list is non-null, is this newsgroup wanted? */

bool inlist(const char *ngnam)
{
    if (g_maxngtodo == 0)
        return true;
    for (int i = s_save_maxngtodo; i < g_maxngtodo + s_save_maxngtodo; i++) {
        if (execute(s_compextodo[i],ngnam))
            return true;
    }
    return false;
}

void end_only()
{
    if (g_maxngtodo) {                  /* did they specify newsgroup(s) */

        if (g_verbose)
            sprintf(g_msg, "Restriction %s%s removed.",g_ngtodo[0],
                    g_maxngtodo > 1 ? ", etc." : "");
        else
            sprintf(g_msg, "Exiting \"only\".");
        for (int i = s_save_maxngtodo; i < g_maxngtodo + s_save_maxngtodo; i++) {
            free(g_ngtodo[i]);
            free_compex(s_compextodo[i]);
#ifndef lint
            free((char*)s_compextodo[i]);
#endif
        }
        g_maxngtodo = 0;
        g_ng_min_toread = 1;
    }
}

void push_only()
{
    s_save_maxngtodo = g_maxngtodo;
    g_maxngtodo = 0;
}

void pop_only()
{
    ART_UNREAD save_ng_min_toread = g_ng_min_toread;

    end_only();

    g_maxngtodo = s_save_maxngtodo;
    s_save_maxngtodo = 0;

    g_ng_min_toread = save_ng_min_toread;
}
