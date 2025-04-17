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
    SRCH_NOT_FOUND = 3,
    SRCH_DONE = 4,
    SRCH_SUBJ_DONE = 5,
    SRCH_ERROR = 6
};

enum ArtScope
{
    ARTSCOPE_SUBJECT = 0,
    ARTSCOPE_FROM = 1,
    ARTSCOPE_ONE_HDR = 2,
    ARTSCOPE_HEAD = 3,
    ARTSCOPE_BODY_NO_SIG = 4,
    ARTSCOPE_BODY = 5,
    ARTSCOPE_ARTICLE = 6
};

extern std::string    g_last_pat;         /* last search pattern */
extern CompiledRegex *g_bra_compex;       /* current compex with brackets */
extern const char    *g_scope_str;        //
extern ArtScope       g_art_how_much;     /* search scope */
extern HeaderLineType g_art_srch_hdr;     /* specific header number to search */
extern bool           g_art_do_read;      /* search read articles? */
extern bool           g_kill_thru_kludge; /* -k */

void            art_search_init();
ArtSearchResult art_search(char *pat_buf, int pat_buf_siz, bool get_cmd);

#endif
