/* trn/artstate.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_ARTSTATE_H
#define TRN_ARTSTATE_H

#include <config/typedef.h>

#include "trn/search.h"

#include <string>
#include <string_view>

struct SearchPattern
{
    void compile_from_env_var(std::string_view var);
    bool matches(std::string_view line);

    std::string   text;
    CompiledRegex compex{};
};

extern bool          g_reread;          // consider current art temporarily unread?
extern bool          g_old_subject;     // not 1st art in subject thread
extern bool          g_do_hiding;       // hide header lines with -h?
extern bool          g_rotate;          // has rotation been requested?
extern std::string   g_first_line;      // special first line?
extern SearchPattern g_hide_line;       // custom line hiding?
extern SearchPattern g_page_stop;       // custom page terminator?

#endif
