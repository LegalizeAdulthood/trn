/* trn/ngsrch.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NGSRCH_H
#define TRN_NGSRCH_H

struct COMPEX;
struct NewsgroupData;

enum NewsgroupSearchResult
{
    NGS_ABORT = 0,
    NGS_FOUND = 1,
    NGS_INTR = 2,
    NGS_NOTFOUND = 3,
    NGS_ERROR = 4,
    NGS_DONE = 5
};

void ngsrch_init();
NewsgroupSearchResult ng_search(char *patbuf, bool get_cmd);
bool ng_wanted(NewsgroupData *np);
const char *ng_comp(COMPEX *compex, const char *pattern, bool RE, bool fold);

#endif
