/* trn/artsrch.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ARTSRCH_H
#define TRN_ARTSRCH_H

#include <trn/head.h>
#include <trn/search.h>

#include <functional>
#include <string>
#include <string_view>

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

struct ArticleSearchContext
{
    std::string    last_pat;                // last search pattern
    CompiledRegex  sub_compex{};            // last compiled subject search
    CompiledRegex  art_compex{};            // last compiled normal search
    CompiledRegex *bra_compex{&art_compex}; // current compex with brackets
    ArtScope       how_much{};              // search scope
    HeaderLineType srch_hdr{};              // specific header number to search
    bool           do_read{};               // search read articles?
};

extern ArticleSearchContext   g_art_search;
extern const std::string_view g_scope_str; //
extern bool           g_kill_thru_kludge; // -k

void            art_search_init();
ArtSearchResult art_search(std::string_view command, bool get_cmd,
                           const std::function<void(ArticleNum)> &progress = {});

#endif
