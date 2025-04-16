/* trn/artsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ARTSRCH_H
#define TRN_ARTSRCH_H

#include "trn/head.h"
#include "trn/search.h"

#include <string>

enum ArtSearchResult
{
    SRCH_ABORT = 0,
    SRCH_INTR = 1,
    SRCH_FOUND = 2,
    SRCH_NOTFOUND = 3,
    SRCH_DONE = 4,
    SRCH_SUBJDONE = 5,
    SRCH_ERROR = 6
};

enum ArtScope
{
    ARTSCOPE_SUBJECT = 0,
    ARTSCOPE_FROM = 1,
    ARTSCOPE_ONEHDR = 2,
    ARTSCOPE_HEAD = 3,
    ARTSCOPE_BODY_NOSIG = 4,
    ARTSCOPE_BODY = 5,
    ARTSCOPE_ARTICLE = 6
};

extern std::string      g_lastpat;          /* last search pattern */
extern COMPEX          *g_bra_compex;       /* current compex with brackets */
extern const char      *g_scopestr;         //
extern ArtScope        g_art_howmuch;      /* search scope */
extern HeaderLineType g_art_srchhdr;      /* specific header number to search */
extern bool             g_art_doread;       /* search read articles? */
extern bool             g_kill_thru_kludge; /* -k */

void              artsrch_init();
ArtSearchResult art_search(char *patbuf, int patbufsiz, bool get_cmd);

#endif
