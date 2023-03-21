/* only.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include "EXTERN.h"
#include "common.h"
#include "search.h"
#include "util.h"
#include "util2.h"
#include "final.h"
#include "ngdata.h"
#include "ngsrch.h"
#include "only.h"

char *g_ngtodo[MAXNGTODO];       /* restrictions in effect */
COMPEX *g_compextodo[MAXNGTODO]; /* restrictions in compiled form */
int g_maxngtodo{};               /*  0 => no restrictions */
                                 /* >0 => # of entries in g_ngtodo */
char g_empty_only_char{'o'};

static int s_save_maxngtodo{};

void only_init()
{
}

void setngtodo(const char *pat)
{
    char* s;
    int i = g_maxngtodo + s_save_maxngtodo;

    if (!*pat)
	return;
    if (i < MAXNGTODO) {
	g_ngtodo[i] = savestr(pat);
#ifndef lint
	g_compextodo[i] = (COMPEX*)safemalloc(sizeof(COMPEX));
#endif
	init_compex(g_compextodo[i]);
	compile(g_compextodo[i],pat,true,true);
	if ((s = ng_comp(g_compextodo[i],pat,true,true)) != nullptr) {
	    printf("\n%s\n",s) FLUSH;
	    finalize(1);
	}
	g_maxngtodo++;
    }
}

/* if command line list is non-null, is this newsgroup wanted? */

bool inlist(char *ngnam)
{
    int i;

    if (g_maxngtodo == 0)
	return true;
    for (i = s_save_maxngtodo; i < g_maxngtodo + s_save_maxngtodo; i++) {
	if (execute(g_compextodo[i],ngnam))
	    return true;
    }
    return false;
}

void end_only()
{
    if (g_maxngtodo) {			/* did they specify newsgroup(s) */
	int i;

	if (verbose)
	    sprintf(msg, "Restriction %s%s removed.",g_ngtodo[0],
		    g_maxngtodo > 1 ? ", etc." : "");
	else
	    sprintf(msg, "Exiting \"only\".");
	for (i = s_save_maxngtodo; i < g_maxngtodo + s_save_maxngtodo; i++) {
	    free(g_ngtodo[i]);
	    free_compex(g_compextodo[i]);
#ifndef lint
	    free((char*)g_compextodo[i]);
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
