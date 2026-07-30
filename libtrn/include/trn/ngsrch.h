/* trn/ngsrch.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NGSRCH_H
#define TRN_NGSRCH_H

#include <string_view>

class CompiledRegex;
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
NewsgroupSearchResult newsgroup_search(std::string_view command, bool get_cmd);
const char *newsgroup_comp(CompiledRegex &compex, std::string_view pattern, bool re, bool fold);

#endif
