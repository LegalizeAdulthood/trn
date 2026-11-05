/* trn/ngsrch.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_NGSRCH_H
#define TRN_NGSRCH_H

#include <trn/search.h>

#include <string_view>

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

struct NewsgroupSearchContext
{
    bool          do_empty{}; // search empty newsgroups?
    CompiledRegex compex;
};

extern NewsgroupSearchContext g_ng_search;

void newsgroup_search_init();
NewsgroupSearchResult newsgroup_search(std::string_view command, bool get_cmd);
std::string_view newsgroup_comp(CompiledRegex &compex, std::string_view pattern, bool re, bool fold);

#endif
