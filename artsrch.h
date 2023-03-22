/* artsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef ARTSRCH_H
#define ARTSRCH_H

#ifndef NBRA
#include "search.h"
#endif

enum
{
    SRCH_ABORT = 0,
    SRCH_INTR = 1,
    SRCH_FOUND = 2,
    SRCH_NOTFOUND = 3,
    SRCH_DONE = 4,
    SRCH_SUBJDONE = 5,
    SRCH_ERROR = 6
};

enum
{
    ARTSCOPE_SUBJECT = 0,
    ARTSCOPE_FROM = 1,
    ARTSCOPE_ONEHDR = 2,
    ARTSCOPE_HEAD = 3,
    ARTSCOPE_BODY_NOSIG = 4,
    ARTSCOPE_BODY = 5,
    ARTSCOPE_ARTICLE = 6
};

extern char *g_lastpat;      /* last search pattern */
extern COMPEX g_sub_compex;  /* last compiled subject search */
extern COMPEX g_art_compex;  /* last compiled normal search */
extern COMPEX *g_bra_compex; /* current compex with brackets */
extern char g_scopestr[];    //
extern int g_art_howmuch;    /* search scope */
extern int g_art_srchhdr;    /* specific header number to search */
extern bool g_art_doread;    /* search read articles? */

void artsrch_init();
int art_search(char *patbuf, int patbufsiz, int get_cmd);
bool wanted(COMPEX *compex, ART_NUM artnum, char_int scope);

#endif
