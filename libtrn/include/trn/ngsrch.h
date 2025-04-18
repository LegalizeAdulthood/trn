/* trn/ngsrch.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_NGSRCH_H
#define TRN_NGSRCH_H

struct CompiledRegex;
struct NewsgroupData;

enum NewsgroupSearchResult
{
    NGS_ABORT = 0,
    NGS_FOUND = 1,
    NGS_INTR = 2,
    NGS_NOT_FOUND = 3,
    NGS_ERROR = 4,
    NGS_DONE = 5
};

void newsgroup_search_init();
NewsgroupSearchResult newsgroup_search(char *patbuf, bool get_cmd);
bool newsgroup_wanted(NewsgroupData *np);
const char *newsgroup_comp(CompiledRegex *compex, const char *pattern, bool re, bool fold);

#endif
