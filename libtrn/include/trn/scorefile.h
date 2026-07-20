/* trn/scorefile.h
 *
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCOREFILE_H
#define TRN_SCOREFILE_H

#include <trn/head.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct CompiledRegex;

#define DEFAULT_SCOREDIR "%+/scores"

struct ScoreFileEntry
{
    int            head_type; // header # (see trn/head.h)
    int            score;     // score change
    std::string    str1;      // first string part
    std::string    str2;      // second string part
    CompiledRegex *compex;    // regular expression ptr
    char           flags;     // 1: regex is valid
                              // 2: rule has been applied to the current article.
                              // 4: use faster rule checking  (later)
                              //
};
// note that negative header #s are used to indicate special entries...

// for cached score rules
struct ScoreFile
{
    std::string              fname;
    std::vector<std::string> lines;
    bool                     exists{};
};

extern int  g_sf_num_entries;   // # of entries
extern int  g_sf_score_verbose; // when true, the scoring routine prints lots of info...
extern bool g_sf_verbose;       // if true print more stuff while loading

void sf_init();
void sf_clean();

// Returns true if text pointed to by s is a text representation of
// the number 0.  Used for error checking.
// Note: does not check for trailing garbage ("+00kjsdfk" returns true).
//
inline bool is_text_zero(const char *s)
{
    return *s == '0' || ((*s == '+' || *s == '-') && s[1]=='0');
}

int   sf_score(ArticleNum a);
void  sf_append(std::string_view line);
void  sf_edit_file(std::string_view filespec);

#endif
