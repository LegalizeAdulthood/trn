/* trn/only.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ONLY_H
#define TRN_ONLY_H

#include <config/common.h>

// TODO: why does this need to be here?
#ifndef NBRA
#include <trn/search.h>
#endif

#include <array>
#include <string>
#include <string_view>

constexpr int MAX_NG_TO_DO{512}; // number of newsgroups allowed on command line

class CompiledRegex;

struct NewsgroupRestrictions
{
    void set_to_do(std::string_view pat);
    void push_only();
    void clear_active();
    void restore_saved();

    std::string_view restriction(int index);
    bool             in_list(std::string_view newsgroup_name);

    std::array<std::string, MAX_NG_TO_DO> to_do;       // restrictions in effect
    int                                   max_to_do{}; // 0 => no restrictions; >0 => # of entries in to_do
    int                                   save_max_to_do{};
    CompiledRegex                        *compex_to_do[MAX_NG_TO_DO]{}; // restrictions in compiled form
};

extern NewsgroupRestrictions g_only;
extern char g_empty_only_char;

void only_init();
void end_only();
void pop_only();

#endif
