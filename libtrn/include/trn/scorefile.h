/* trn/scorefile.h
 *
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCOREFILE_H
#define TRN_SCOREFILE_H

#include <trn/head.h>

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class CompiledRegex;

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

struct ScoreRules
{
    void free_patterns();
    void clear_lists();

    // TODO: private
    void grow();
    int  check_extra_headers(std::string_view head);
    void add_extra_header(std::string_view head);
    int  match(const std::string &text, int ind);

    std::vector<ScoreFileEntry> entries;       // array of entries
    int                         num_entries{}; // # of entries
    std::vector<std::string>    extra_headers;
    bool                        has_extra_headers{};
    bool                        new_author_active{}; // if true, new_author is active
    int                         new_author{};        // bonus score given to a new (unscored) author
    bool                        reply_active{};      // if true, reply_score is active
    int                         reply_score{};       // score amount added to an article reply
};

extern ScoreRules g_score_rules;

// for cached score rules
struct ScoreFile
{
    std::string              fname;
    std::vector<std::string> lines;
    bool                     exists{};
};

using ScoreFileUrlGetter = bool(std::string_view url, const std::filesystem::path &outfile);

struct ScoreFileCache
{
    void file_clear();

    // TODO: private
    int open_file(std::string_view name);

    std::vector<ScoreFile> files;
    ScoreFileUrlGetter    *url_getter{};
};

extern ScoreFileCache g_score_cache;

struct ScoreRuleCompiler
{
    // TODO: private
    std::string_view free_form(std::string_view keyword, std::string_view remaining);
    bool             do_command(std::string_view cmd, bool check);

    std::array<std::string, 256> abbr;             // abbreviations
    int                          file_level{};     // how deep are we?
    bool                         pattern_status{}; // should we match by pattern?
    CompiledRegex               *compex{};
};

extern ScoreRuleCompiler g_score_compiler;

extern int  g_sf_score_verbose; // when true, the scoring routine prints lots of info...
extern bool g_sf_verbose;       // if true print more stuff while loading

void sf_init();
void sf_clean();

int   sf_score(ArticleNum a);
void  sf_append(std::string_view line);
void  sf_edit_file(std::string_view filespec);

#endif
