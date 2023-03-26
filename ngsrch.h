/* ngsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NGSRCH_H
#define TRN_NGSRCH_H

struct COMPEX;
struct NGDATA;

enum ng_search_result
{
    NGS_ABORT = 0,
    NGS_FOUND = 1,
    NGS_INTR = 2,
    NGS_NOTFOUND = 3,
    NGS_ERROR = 4,
    NGS_DONE = 5
};

extern bool g_ng_doempty; /* search empty newsgroups? */

void ngsrch_init();
ng_search_result ng_search(char *patbuf, bool get_cmd);
bool ng_wanted(NGDATA *np);
const char *ng_comp(COMPEX *compex, const char *pattern, bool RE, bool fold);

#endif
