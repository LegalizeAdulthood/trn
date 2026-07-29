/* scorefile.cpp
 *
 * A simple "proof of concept" scoring file for headers.
 * (yeah, right. :)
 */
// This file Copyright 1992, 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile-internal.h>

#include <config/common.h>
#include <config/env.h>
#include <config/string_case_compare.h>
#include <trn/cache.h>
#include <trn/head.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-util.h>
#include <trn/score.h>  // shared stuff...
#include <trn/search.h> // regex matches
#include <trn/size_cast.h>
#include <trn/string-algos.h>
#include <trn/terminal.h> // finish_command()
#include <trn/url.h>
#include <trn/util.h>
#include <util/env.h> // get_val
#include <util/util2.h>

#include <fmt/format.h>

#include <array>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static std::string_view sf_get_extra_header(ArticleNum art, int hnum);
static bool             sf_default_url_get(std::string_view url, const fs::path &outfile);

int  g_sf_num_entries{};   // # of entries
int  g_sf_score_verbose{}; // when true, the scoring routine prints lots of info...
bool g_sf_verbose{true};   // if true print more stuff while loading

// list of score array markers (in g_header_type field of score entry)
// entry is a file marker.  Score is the file level
enum
{
    SF_FILE_MARK_START = -1,
    SF_FILE_MARK_END = -2,
    // other misc. rules
    SF_KILL_THRESHOLD = -3,
    SF_NEW_AUTHOR = -4,
    SF_REPLY = -5
};

static std::vector<ScoreFileEntry> s_sf_entries; // array of entries
static std::vector<ScoreFile> s_sf_files;
static std::array<std::string, 256> s_sf_abbr; // abbreviations
static bool            s_new_author_active{}; // if true, s_newauthor is active
static int             s_new_author{};        // bonus score given to a new (unscored) author
static bool            s_sf_pattern_status{}; // should we match by pattern?
static bool            s_reply_active{};      // if true, s_reply_score is active
static int             s_reply_score{};       // score amount added to an article reply
static int             s_sf_file_level{};     // how deep are we?
static std::vector<std::string> s_sf_extra_headers;
static bool            s_sf_has_extra_headers{};
static ScoreFileUrlGetter s_url_getter{sf_default_url_get};
static CompiledRegex  *s_sf_compex{};

static int sf_open_file(std::string_view name);
static void sf_file_clear();
static void  sf_grow();
static int sf_check_extra_headers(std::string_view head);
static void sf_add_extra_header(std::string_view head);
static fs::path sf_get_filename(int level);
static std::string sf_cmd_fname(std::string_view s);
static bool        sf_do_command(std::string_view cmd, bool check);
static std::string_view sf_freeform(std::string_view keyword, std::string_view remaining);
static bool  sf_do_line(std::string_view line, bool check);
static void  sf_do_file(std::string_view fname);
static int   score_match(const std::string &text, int ind);
static std::string sf_missing_score(std::string_view line);
static std::string sf_get_line(ArticleNum a, HeaderLineType h);
static void  sf_print_match(int indx);
static void  sf_exclude_file(std::string_view fname);

static bool sf_is_url(std::string_view name)
{
    return string_case_equal(name.substr(0, 4), "URL:");
}

static bool sf_default_url_get(std::string_view url, const fs::path &outfile)
{
    return url_get(url, outfile);
}

void sf_set_url_getter_for_test(ScoreFileUrlGetter getter)
{
    s_url_getter = getter != nullptr ? getter : sf_default_url_get;
}

void sf_clear_file_cache_for_test()
{
    sf_file_clear();
}

// Must be called before any other sf_ routine (once for each group)
void sf_init()
{
    g_sf_num_entries = 0;
    s_sf_entries.clear();
    s_sf_extra_headers.clear();

    // initialize abbreviation list
    s_sf_abbr = {};

    if (g_sf_verbose)
    {
        fmt::print("\nReading score files...\n");
    }
    s_sf_file_level = 0;
    // find # of levels
    const std::string group_name = file_exp("%C");
    int               level = 0;
    for (char ch : group_name)
    {
        if (ch == '.')
        {
            level++; // count dots in group name
        }
    }
    level++;

    // the main read-in loop
    for (int i = 0; i <= level; i++)
    {
        const fs::path score_file = sf_get_filename(i);
        if (!score_file.empty())
        {
            sf_do_file(score_file.generic_string());
        }
    }

    // do post-processing (set thresholds and detect extra header usage)
    s_sf_has_extra_headers = false;
    // set thresholds from the s_sf_entries
    s_reply_active = false;
    s_new_author_active = false;
    g_kill_thresh_active = false;
    for (int i = 0; i < g_sf_num_entries; i++)
    {
        if (s_sf_entries[i].head_type >= HEAD_LAST)
        {
            s_sf_has_extra_headers = true;
        }
        switch (s_sf_entries[i].head_type)
        {
        case SF_KILL_THRESHOLD:
            g_kill_thresh_active = true;
            g_kill_thresh = s_sf_entries[i].score;
            if (g_sf_verbose)
            {
                int j;
                // rethink?
                for (j = i + 1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_KILL_THRESHOLD)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) // no later thresholds
                {
                    fmt::print("killthreshold {}\n", g_kill_thresh);
                }
            }
            break;

        case SF_NEW_AUTHOR:
            s_new_author_active = true;
            s_new_author = s_sf_entries[i].score;
            if (g_sf_verbose)
            {
                int j;
                // rethink?
                for (j = i + 1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_NEW_AUTHOR)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) // no later newauthors
                {
                    fmt::print("New Author score: {}\n", s_new_author);
                }
            }
            break;

        case SF_REPLY:
            s_reply_active = true;
            s_reply_score = s_sf_entries[i].score;
            if (g_sf_verbose)
            {
                int j;
                // rethink?
                for (j = i + 1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_REPLY)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) // no later reply rules
                {
                    fmt::print("Reply score: {}\n", s_reply_score);
                }
            }
            break;
        }
    }
}

void sf_clean()
{
    for (int i = 0; i < g_sf_num_entries; i++)
    {
        if (s_sf_entries[i].compex != nullptr)
        {
            s_sf_entries[i].compex->free_compex();
            delete s_sf_entries[i].compex;
        }
    }
    s_sf_abbr = {};
    s_sf_entries.clear();
    g_sf_num_entries = 0;
    s_sf_extra_headers.clear();
}

static void sf_grow()
{
    s_sf_entries.push_back(ScoreFileEntry{});
    g_sf_num_entries = size_cast<int>(s_sf_entries);
}

// Returns -1 if no matching extra header found, otherwise returns offset
// into the s_sf_extra_headers array.
//
// header name, without ':' character
static int sf_check_extra_headers(std::string_view head)
{
    std::string lower_head{head};

    // convert to lower case
    for (char &ch : lower_head)
    {
        if (std::isalpha(ch) && std::isupper(ch))
        {
            ch = static_cast<char>(std::tolower(ch)); // convert to lower case
        }
    }
    for (std::size_t i = 0; i < s_sf_extra_headers.size(); i++)
    {
        if (lower_head == s_sf_extra_headers[i])
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// adds the header to the list of known extra headers if it is not already
// known.
//
// new header name, without ':' character
static void sf_add_extra_header(std::string_view head)
{
    // check to see if it's already known
    // first see if it is a known system header
    if (set_line_type(head) != SOME_LINE)
    {
        return; // known types should be interpreted in normal way
    }
    // then check to see if it's a known extra header
    if (sf_check_extra_headers(head) >= 0)
    {
        return;
    }

    std::string header_name{head};
    for (char &ch : header_name)
    {
        if (std::isalpha(ch) && std::isupper(ch))
        {
            ch = static_cast<char>(std::tolower(ch)); // convert to lower case
        }
    }
    s_sf_extra_headers.push_back(header_name);
}

//ART_NUM art;          // article number to check
//int hnum;             // header number: offset into s_sf_extra_headers
static std::string_view sf_get_extra_header(ArticleNum art, int hnum)
{
    parse_header(art); // fast if already parsed

    const std::string     &head = s_sf_extra_headers[hnum];
    const std::string_view header_text{g_head_buf};

    for (std::size_t line_start{}; line_start < header_text.size();)
    {
        const std::size_t line_end = header_text.find('\n', line_start);
        if (line_end == std::string_view::npos || line_end == line_start)
        {
            break;
        }
        const std::string_view line = header_text.substr(line_start, line_end - line_start);
        const std::size_t      colon = line.find(':');
        if (colon != std::string_view::npos && string_case_equal(line.substr(0, colon), head))
        {
            std::string_view text = line.substr(colon + 1);
            text = skip_hor_space(text);
            return text;
        }
        line_start = line_end + 1;
    }
    return {};
}

// filenames of type a/b/c/foo.bar.misc for group foo.bar.misc
static fs::path sf_get_filename(int level)
{
    const fs::path score_dir{file_exp(get_env_var("SCOREDIR", DEFAULT_SCOREDIR))};
    if (!level)
    {
        // allow environment variable later...
        return score_dir / "global";
    }

    std::string            group_name = file_exp("%C");
    std::string::size_type pos = 0;
    // maybe redo this logic later...
    while (level--)
    {
        if (pos == group_name.size()) // no more name to match
        {
            return {};
        }
        pos = group_name.find('.', pos);
        if (pos == std::string::npos)
        {
            pos = group_name.size();
        }
        if (pos < group_name.size() && level)
        {
            pos++;
        }
    }
    group_name.resize(pos); // cut end of score file
    return score_dir / group_name;
}

// given a string, if no slashes prepends SCOREDIR env. variable
static std::string sf_cmd_fname(std::string_view s)
{
    if (s.find('/') != std::string_view::npos)
    {
        return std::string{s};
    }
    // no slashes in this filename
    return (fs::path{get_env_var("SCOREDIR", DEFAULT_SCOREDIR)} / std::string{s}).generic_string();
}

// returns true if good command, false otherwise
static bool sf_do_command(std::string_view cmd, bool check)
{
    const std::size_t      command_end = cmd.find_first_of(" \t=");
    const std::string_view command = cmd.substr(0, command_end);
    const std::string_view arguments =
        command_end == std::string_view::npos ? std::string_view{} : cmd.substr(command_end);

    if (command == "killthreshold" || command == "newauthor" || command == "reply")
    {
        std::string_view argument{arguments};
        argument.remove_prefix(std::min(argument.find_first_not_of(" \t="), argument.size()));
        std::string_view number{argument};
        if (!number.empty() && number.front() == '+')
        {
            number.remove_prefix(1);
        }
        int                          score{};
        const std::from_chars_result result = std::from_chars(number.data(), number.data() + number.size(), score);
        if (result.ec != std::errc{})
        {
            if (command == "reply")
            {
                fmt::print("\nBad reply command: {}\n", cmd);
            }
            else
            {
                fmt::print("\nBad {}: {}", command, cmd);
            }
            return false;
        }
        if (check)
        {
            return true;
        }
        int head_type;
        if (command == "killthreshold")
        {
            head_type = SF_KILL_THRESHOLD;
        }
        else if (command == "newauthor")
        {
            head_type = SF_NEW_AUTHOR;
        }
        else
        {
            head_type = SF_REPLY;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries - 1].head_type = head_type;
        s_sf_entries[g_sf_num_entries - 1].score = score;
        return true;
    }
    if (command == "savescores")
    {
        std::string_view argument{arguments};
        argument.remove_prefix(std::min(argument.find_first_not_of(" \t="), argument.size()));
        const std::string_view value = argument.substr(0, argument.find_first_of(" \t"));
        if (value == "off")
        {
            if (!check)
            {
                g_sc_saves_cores = false;
            }
            return true;
        }
        if (!argument.empty())
        {
            if (!check)
            {
                g_sc_saves_cores = true;
            }
            return true;
        }
        fmt::print("Bad savescores command: |{}|\n", cmd);
        return false;
    }
    if (command == "include" || command == "exclude")
    {
        if (check)
        {
            return true;
        }
        std::string_view argument{arguments};
        argument = skip_hor_space(argument);
        if (argument.empty())
        {
            fmt::print("Bad {} command (missing filename)\n", command);
            return false;
        }
        if (command == "include")
        {
            sf_do_file(file_exp(sf_cmd_fname(argument)));
        }
        else
        {
            sf_exclude_file(file_exp(sf_cmd_fname(argument)));
        }
        return true;
    }
    if (command == "header")
    {
        std::string_view argument{arguments};
        argument.remove_prefix(std::min(argument.find_first_not_of(" \t="), argument.size()));
        const std::size_t colon = argument.find(':');
        if (colon == std::string_view::npos)
        {
            fmt::print("\nBad header command (missing :)\n{}\n", cmd);
            return false;
        }
        if (check)
        {
            return true;
        }
        sf_add_extra_header(argument.substr(0, colon));
        return true;
    }
    if (command == "begin" || command == "end")
    {
        return true;
    }
    if (command == "file")
    {
        if (check)
        {
            return true;
        }
        std::string_view argument{arguments};
        argument = skip_hor_space(argument);
        if (argument.empty())
        {
            fmt::print("Bad file command (missing parameters)\n");
            return false;
        }
        const char abbreviation = argument.front();
        argument.remove_prefix(1);
        argument = skip_hor_space(argument);
        if (argument.empty())
        {
            fmt::print("Bad file command (missing parameters)\n");
            return false;
        }
        s_sf_abbr[static_cast<unsigned char>(abbreviation)] = sf_cmd_fname(argument);
        return true;
    }
    if (command == "newsclip")
    {
        fmt::print("Newsclip is no longer supported.\n");
        return false;
    }
    // no command matched
    fmt::print("Unknown command: |{}|\n", cmd);
    return false;
}

static std::string_view sf_freeform(std::string_view keyword, std::string_view remaining)
{
    if (keyword == "pattern")
    {
        s_sf_pattern_status = true;
        remaining = skip_hor_space(remaining);
        return remaining;
    }
    fmt::print("Scorefile freeform: unknown key: |{}|\n", keyword);
    return {};
}

//bool check;           // if true, just check the line, don't act.
static bool sf_do_line(std::string_view line, bool check)
{
    const std::size_t nul = line.find('\0');
    if (nul != std::string_view::npos)
    {
        line = line.substr(0, nul);
    }
    if (line.empty())
    {
        return true; // very empty line
    }
    if (line.back() == '\n')
    {
        line.remove_suffix(1);
    }
    if (line.empty())
    {
        return true;
    }

    const char ch = line.front();
    if (ch == '#') // comment
    {
        return true;
    }

    // reset any per-line bitflags
    s_sf_pattern_status = false;

    if (std::isalpha(static_cast<unsigned char>(ch))) // command line
    {
        return sf_do_command(line, check);
    }

    // skip whitespace
    line = skip_hor_space(line);
    if (line.empty() || line.front() == '#')
    {
        return true; // line was whitespace or comment after whitespace
    }
    // convert line to lowercase (make optional later?)
    std::string normalized_line{line};
    for (char &value : normalized_line)
    {
        if (std::isupper(static_cast<unsigned char>(value)))
        {
            value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
        }
    }
    const std::string_view normalized_text{normalized_line};
    const std::size_t      header_start = normalized_text.find_first_not_of("0123456789+- \t");
    const std::string_view score_text = normalized_text.substr(0, std::min(header_start, normalized_text.size()));
    std::string_view       score_digits{score_text};
    bool                   negative_score = false;
    if (!score_digits.empty() && (score_digits.front() == '+' || score_digits.front() == '-'))
    {
        negative_score = score_digits.front() == '-';
        score_digits.remove_prefix(1);
    }
    int score{};
    if (!score_digits.empty() && score_digits.front() != '+' && score_digits.front() != '-')
    {
        const std::from_chars_result score_result =
            std::from_chars(score_digits.data(), score_digits.data() + score_digits.size(), score);
        if (score_result.ec != std::errc{} || score_result.ptr == score_digits.data())
        {
            score = 0;
        }
        else if (negative_score)
        {
            score = -score;
        }
    }
    if (score == 0) // it might not be a number
    {
        const bool text_zero = !score_text.empty() && (score_text.front() == '0' ||
                                                       ((score_text.front() == '+' || score_text.front() == '-') &&
                                                        score_text.size() > 1 && score_text[1] == '0'));
        if (!text_zero)
        {
            fmt::print("\nBad scorefile line:\n|{}|\n", normalized_text);
            return false;
        }
    }
    if (header_start == std::string_view::npos)
    {
        fmt::print("Scorefile entry error error (freeform parse).  Line was:\n|{}|\n", normalized_text);
        return false;
    }
    std::string_view line_rest = normalized_text.substr(header_start);
    std::string_view header;
    std::string_view pattern;
    while (true)
    {
        const std::size_t      keyword_end = line_rest.find_first_of(" \t");
        const std::string_view keyword = line_rest.substr(0, keyword_end);
        if (!keyword.empty() && keyword.back() == ':') // did header
        {
            header = keyword.substr(0, keyword.size() - 1);
            pattern = keyword_end == std::string_view::npos ? std::string_view{} : line_rest.substr(keyword_end);
            pattern = skip_hor_space(pattern);
            break; // go to set header routine
        }
        const std::string_view remaining =
            keyword_end == std::string_view::npos ? std::string_view{} : line_rest.substr(keyword_end);
        line_rest = sf_freeform(keyword, remaining);
        if (line_rest.empty()) // used up all the line's text, or error
        {
            fmt::print("Scorefile entry error error (freeform parse).  Line was:\n|{}|\n", normalized_text);
            return false; // error
        }
    } // while
    int j = set_line_type(header);
    if (j == SOME_LINE)
    {
        j = sf_check_extra_headers(header);
        if (j >= 0)
        {
            j += HEAD_LAST;
        }
        else
        {
            fmt::print("Unknown score header type.  Line follows:\n|{}|\n", normalized_text);
            return false;
        }
    }
    if (pattern.empty()) // no pattern
    {
        fmt::print("Empty score pattern.  Line follows:\n|{}|\n", normalized_text);
        return false;
    }
    if (check)
    {
        return true; // limits of check
    }
    sf_grow(); // acutally make an entry
    ScoreFileEntry &entry = s_sf_entries[g_sf_num_entries - 1];
    entry.head_type = j;
    entry.score = score;
    if (s_sf_pattern_status) // in pattern matching mode
    {
        entry.flags |= 1;
        entry.str1.assign(pattern);
        s_sf_compex = new CompiledRegex;
        s_sf_compex->init_compex();
        // compile arguments:
        // 1st is COMPEX to store compiled regex in
        // 2nd is search string
        // 3rd should be true if the search string is a regex
        // 4th is true for case-insensitivity
        const char *compile_error = s_sf_compex->compile(entry.str1, true, true);
        if (compile_error != nullptr)
        {
            fmt::print("Bad pattern : |{}|\n", pattern);
            fmt::print("Compex returns: |{}|\n", compile_error);
            s_sf_compex->free_compex();
            delete s_sf_compex;
            s_sf_compex = nullptr;
            entry.compex = nullptr;
            return false;
        }
        entry.compex = s_sf_compex;
    }
    else
    {
        entry.flags &= 0xfe;
        entry.str2.clear();
        // Note: consider allowing * wildcard on other header filenames
        if (j == FROM_LINE) // may have * wildcard
        {
            const std::size_t separator = pattern.find('*');
            if (separator != std::string_view::npos)
            {
                entry.str2.assign(pattern.substr(separator + 1));
                pattern = pattern.substr(0, separator);
            }
        }
        entry.str1.assign(pattern);
    }
    return true;
}

static void sf_do_file(std::string_view fname)
{
    const std::string filename{fname};
    int               sf_fp = sf_open_file(filename);
    if (sf_fp < 0)
    {
        return;
    }
    s_sf_file_level++;
    if (g_sf_verbose)
    {
        for (int i = 1; i < s_sf_file_level; i++)
        {
            fmt::print("."); // maybe later putchar...
        }
        fmt::print("Score file: {}\n", filename);
    }
    // add end marker to scoring array
    sf_grow();
    s_sf_entries[g_sf_num_entries-1].head_type = SF_FILE_MARK_START;
    // file_level is 1 to n
    s_sf_entries[g_sf_num_entries-1].score = s_sf_file_level;
    s_sf_entries[g_sf_num_entries-1].str2.clear();
    s_sf_entries[g_sf_num_entries-1].str1 = filename;

    const ScoreFile &file = s_sf_files[static_cast<std::size_t>(sf_fp)];
    for (const std::string &s : file.lines)
    {
        (void)sf_do_line(s, false);
    }
    // add end marker to scoring array
    sf_grow();
    s_sf_entries[g_sf_num_entries-1].head_type = SF_FILE_MARK_END;
    // file_level is 1 to n
    s_sf_entries[g_sf_num_entries-1].score = s_sf_file_level;
    s_sf_entries[g_sf_num_entries-1].str2.clear();
    s_sf_entries[g_sf_num_entries-1].str1 = filename;
    s_sf_file_level--;
}

//const std::string& text; // string to match on
//int ind;                 // index into s_sf_entries
static int score_match(const std::string &text, int ind)
{
    if (s_sf_entries[ind].flags & 1)    // pattern style match
    {
        if (s_sf_entries[ind].compex != nullptr)
        {
            // we have a good pattern
            const char *s2 = s_sf_entries[ind].compex->execute(text.c_str());
            if (s2 != nullptr)
            {
                return true;
            }
        }
        return false;
    }
    // default case
    const std::string     &s1 = s_sf_entries[ind].str1;
    const std::string     &s2 = s_sf_entries[ind].str2;
    const std::size_t      first = text.find(s1);
    return first != std::string::npos &&
           (s2.empty() || text.find(s2, first + s1.size()) != std::string::npos);
}

int sf_score(ArticleNum a)
{
    if (is_unavailable(a))
    {
        return LOW_SCORE;        // unavailable arts get low negative score.
    }

    // if there are no score entries, then the answer is real easy and quick
    if (g_sf_num_entries == 0)
    {
        return 0;
    }
    bool old_untrim = g_untrim_cache;
    g_untrim_cache = true;
    g_sc_scoring = true;                // loop prevention
    int sum = 0;

    // parse the header now if there are extra headers
    // (This could save disk accesses.)
    if (s_sf_has_extra_headers)
    {
        parse_header(a);
    }

    for (int i = 0; i < g_sf_num_entries; i++)
    {
        const int head_type = s_sf_entries[i].head_type;
        if (head_type <= 0) // don't use command headers for scoring
        {
            continue;   // the outer for loop
        }
        const HeaderLineType h = static_cast<HeaderLineType>(head_type);
        // if this head_type has been done before, this entry
        // has already been done
        if (s_sf_entries[i].flags & 2)          // rule has been applied
        {
            s_sf_entries[i].flags &= 0xfd; // turn off flag
            continue;                   // ...with the next rule
        }

        const std::string s = sf_get_line(a,h);
        if (s.empty())  // no such line for the article
        {
            continue;   // with the s_sf_entries.
        }

        // do the matches for this header
        for (int j = i; j < g_sf_num_entries; j++)
        {
            // see if there is a match
            if (h == s_sf_entries[j].head_type)
            {
                if (j != i)             // set flag only for future rules
                {
                    s_sf_entries[j].flags |= 2; // rule has been applied.
                }
                if (score_match(s, j))
                {
                    sum = sum + s_sf_entries[j].score;
                    if (h == FROM_LINE)
                    {
                        article_ptr(a)->m_score_flags |= SFLAG_AUTHOR;
                    }
                    if (g_sf_score_verbose)
                    {
                        sf_print_match(j);
                    }
                }
            }
        }
    }
    if (s_new_author_active && !(article_ptr(a)->m_score_flags & SFLAG_AUTHOR))
    {
        sum = sum + s_new_author; // add new author bonus
        if (g_sf_score_verbose)
        {
            fmt::print("New Author: {}\n", s_new_author);
            // consider: print which file the bonus came from
        }
    }
    if (s_reply_active)
    {
        // should be in cache if a rule above used the subject
        const std::string reply_subject = fetch_cache(a, SUBJ_LINE, true);
        // later: consider other possible reply forms (threading?)
        if (!reply_subject.empty())
        {
            if (subject_has_re(reply_subject))
            {
                sum = sum + s_reply_score;
                if (g_sf_score_verbose)
                {
                    fmt::print("Reply: {}\n", s_reply_score);
                    // consider: print which file the bonus came from
                }
            }
        }
    }
    g_untrim_cache = old_untrim;
    g_sc_scoring = false;
    return sum;
}

// returns changed score line or empty if no changes
static std::string sf_missing_score(std::string_view line)
{
    // Keep an owned copy while finish_command reads new input.
    std::string saved_line{line};
    fmt::print("Possibly missing score.\n"
               "Type a score now or delete the colon to abort this entry:\n");
    const std::string command = finish_command(":", true); // print the CR
    if (command.empty())
    {
        return {}; // there was no score
    }
    std::string result = command.substr(1);
    result += ' ';
    result += saved_line;
    return result;
}

// Interprets the '\"' command for creating new score entries online
// consider using some external buffer rather than the 2 internal ones
void sf_append(std::string_view line)
{
    const std::size_t nul = line.find('\0');
    if (nul != std::string_view::npos)
    {
        line = line.substr(0, nul);
    }
    if (line.empty())
    {
        return; // do nothing with empty string
    }

    char filechar = line.front(); // ch is file abbreviation

    if (filechar == '?') // list known file abbreviations
    {
        fmt::print("List of abbreviation/file pairs\n");
        for (int i = 0; i < 256; i++)
        {
            if (!s_sf_abbr[i].empty())
            {
                fmt::print("{} {}\n", static_cast<char>(i), s_sf_abbr[i]);
            }
        }
        fmt::print("\" [The current newsgroup's score file]\n");
        fmt::print("* [The global score file]\n");
        return;
    }

    // skip whitespace after filechar
    std::string_view scoreline = line.substr(1);
    scoreline = skip_hor_space(scoreline);
    std::string missing_scoreline;

    const char ch = scoreline.empty() ? '\0' : scoreline.front(); // first non-whitespace after filechar
    // If the scorefile line does not begin with a number,
    // and is not a valid command, request a score
    if (!std::isdigit(static_cast<unsigned char>(ch)) && ch != '+' && ch != '-' && ch != ':' && ch != '!' && ch != '#')
    {
        if (!sf_do_line(scoreline, true)) // just checking
        {
            missing_scoreline = sf_missing_score(scoreline);
            if (missing_scoreline.empty()) // no score typed
            {
                fmt::print("Score entry aborted.\n");
                return;
            }
            scoreline = missing_scoreline;
        }
    }

    // scoretext = first non-whitespace after score#
    std::string_view  scoretext = scoreline;
    const std::size_t scoretext_start = scoretext.find_first_not_of("0123456789+- \t");
    scoretext.remove_prefix(scoretext_start == std::string_view::npos ? scoretext.size() : scoretext_start);
    std::string shortcut_scoreline;

    // special one-character shortcuts
    if (scoretext.size() == 1)
    {
        const std::size_t prefix_size = static_cast<std::size_t>(scoretext.data() - scoreline.data());
        switch (scoretext.front())
        {
        case 'F': // domain-shortened FROM line
            shortcut_scoreline.assign(scoreline.data(), prefix_size);
            shortcut_scoreline += file_exp("from: %y");
            scoreline = shortcut_scoreline;
            break;

        case 'S': // current subject
        {
            const std::string subject = fetch_cache(g_art, SUBJ_LINE, true);
            if (subject.empty())
            {
                fmt::print("No subject: score entry aborted.\n");
                return;
            }
            std::string_view subject_text{subject};
            if (subject_text.size() >= 4 && subject_text[0] == 'R' && subject_text[1] == 'e' //
                && subject_text[2] == ':' && subject_text[3] == ' ')
            {
                subject_text.remove_prefix(4);
            }
            shortcut_scoreline.assign(scoreline.data(), prefix_size);
            shortcut_scoreline += "subject: ";
            // Preserve the historical LINE_BUF_LEN-derived subject limit.
            shortcut_scoreline.append(subject_text.substr(0, 900));
            scoreline = shortcut_scoreline;
            break;
        }

        default:
            fmt::print("\nBad scorefile line: |{}| (not added)\n", line);
            return;
        }
        fmt::print("{}\n", scoreline);
    }

    // test the scoring line unless filechar is '!' (meaning do it now)
    if (!sf_do_line(scoreline, filechar != '!'))
    {
        fmt::print("Bad score line (ignored)\n");
        return;
    }
    if (filechar == '!')
    {
        return; // don't actually append to file
    }
    std::string output_scoreline{scoreline};
    if (!output_scoreline.empty() && output_scoreline.back() == '\n')
    {
        output_scoreline.pop_back();
    }
    std::string_view output_text{output_scoreline};
    if (!output_text.empty() && output_text.front() != '#' &&
        !std::isalpha(static_cast<unsigned char>(output_text.front())))
    {
        const std::size_t lower_start = output_text.find_first_not_of(" \t");
        if (lower_start != std::string_view::npos && output_text[lower_start] != '#')
        {
            for (std::size_t offset = lower_start; offset < output_scoreline.size(); ++offset)
            {
                char &value = output_scoreline[offset];
                if (std::isupper(static_cast<unsigned char>(value)))
                {
                    value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
                }
            }
        }
    }
    std::string filename;
    if (filechar == '"') // do local group
    {
        // Note: should probably be changed to use sf_ file functions
        filename = get_env_var("SCOREDIR", DEFAULT_SCOREDIR);
        filename += "/%C";
    }
    else if (filechar == '*') // do global scorefile
    {
        // Note: should probably be changed to use sf_ file functions
        filename = get_env_var("SCOREDIR", DEFAULT_SCOREDIR);
        filename += "/global";
    }
    else
    {
        const std::string &abbreviation = s_sf_abbr[static_cast<unsigned char>(filechar)];
        if (abbreviation.empty())
        {
            fmt::print("\nBad file abbreviation: {}\n", filechar);
            return;
        }
        filename = abbreviation;
    }
    const fs::path score_file{file_exp(sf_cmd_fname(filename))}; // allow shortcuts
    // make sure directory exists...
    std::error_code error;
    fs::create_directories(score_file.parent_path(), error);
    sf_file_clear();
    std::ofstream output{score_file, std::ios::app};
    if (output)
    {
        output << output_scoreline << '\n'; // open (or create) for append
    }
    else // unsuccessful in opening file
    {
        fmt::print("\nCould not open (for append) file {}\n", score_file.generic_string());
    }
}

// returns a lowercased copy of the header line type h
static std::string sf_get_line(ArticleNum a, HeaderLineType h)
{
    std::string      cached_line;
    std::string_view line;

    if (h <= SOME_LINE)
    {
        fmt::print("sf_get_line({},{}): bad header type\n", a.value_of(), static_cast<int>(h));
        fmt::print("(Internal error: header number too low)\n");
        return {};
    }
    if (h >= HEAD_LAST)
    {
        if (h - HEAD_LAST < size_cast<int>(s_sf_extra_headers))
        {
            line = sf_get_extra_header(a, h - HEAD_LAST);
        }
        else
        {
            fmt::print("sf_get_line({},{}): bad header type\n", a.value_of(), static_cast<int>(h));
            fmt::print("(Internal error: header number too high)\n");
            return {};
        }
    }
    else if (h == SUBJ_LINE)
    {
        const std::string subject = fetch_cache(a, h, true); // get compressed copy
        if (!subject.empty())
        {
            cached_line = subject;
            line = cached_line;
        }
    }
    else
    {
        cached_line = prefetch_lines_copy(a,h);
        line = cached_line;
    }
    std::string result{line.substr(0, LINE_BUF_LEN - 1)};
    for (char &ch : result)
    {
        if (std::isupper(ch))
        {
            ch = static_cast<char>(std::tolower(ch));
        }
    }
    return result;
}

// given an index into s_sf_entries, print information about that index
static void sf_print_match(int indx)
{
    int  i;
    int  level; // level is initialized iff used

    for (i = indx; i >= 0; i--)
    {
        int j = s_sf_entries[i].head_type;
        if (j == SF_FILE_MARK_START)  // found immediate inclusion.
        {
            break;
        }
        if (j == SF_FILE_MARK_END)      // found included file, skip
        {
            int tmplevel = s_sf_entries[i].score;
            int k;
            for (k = i; k >= 0; k--)
            {
                if (s_sf_entries[k].head_type == SF_FILE_MARK_START //
                    && s_sf_entries[k].score == tmplevel)
                {
                    break;      // inner for loop
                }
            }
            i = k;      // will be decremented again
        }
    }
    if (i >= 0)
    {
        level = s_sf_entries[i].score;
    }
    // print the file markers.
    for (; i >= 0; i--)
    {
        if (s_sf_entries[i].head_type == SF_FILE_MARK_START //
            && s_sf_entries[i].score <= level)
        {
            level--;    // go out...
            for (int k = 0; k < level; k++)
            {
                fmt::print(".");            // make putchar later?
            }
            fmt::print("From file: {}\n", s_sf_entries[i].str1);
            if (level == 0)             // top level
            {
                break;          // out of the big for loop
            }
        }
    }
    const std::string_view pattern = (s_sf_entries[indx].flags & 1) ? "pattern " : "";

    std::string_view head_name;
    if (s_sf_entries[indx].head_type >= HEAD_LAST)
    {
        head_name = s_sf_extra_headers[s_sf_entries[indx].head_type - HEAD_LAST];
    }
    else
    {
        head_name = g_header_type[s_sf_entries[indx].head_type].name;
    }
    fmt::print("{} {}{}: {}", s_sf_entries[indx].score, pattern, head_name, s_sf_entries[indx].str1);
    if (!s_sf_entries[indx].str2.empty())
    {
        fmt::print("*{}", s_sf_entries[indx].str2);
    }
    fmt::print("\n");
}

static void sf_exclude_file(std::string_view fname)
{
    int       start;
    int       end;

    for (start = 0; start < g_sf_num_entries; start++)
    {
        if (s_sf_entries[start].head_type == SF_FILE_MARK_START
         && std::string_view{s_sf_entries[start].str1} == fname)
        {
            break;
        }
    }
    if (start == g_sf_num_entries)
    {
        fmt::print("Exclude: file |{}| was not included\n", fname);
        return;
    }
    for (end = start+1; end < g_sf_num_entries; end++)
    {
        if (s_sf_entries[end].head_type==SF_FILE_MARK_END
         && std::string_view{s_sf_entries[end].str1} == fname)
        {
            break;
        }
    }
    if (end == g_sf_num_entries)
    {
        fmt::print("Exclude: file |{}| is incomplete at exclusion command\n", fname);
        // insert more explanation later?
        return;
    }

#ifdef UNDEF
    int newnum = g_sf_num_entries - (end - start) - 1;
    // Deal with exclusion of all scorefile entries.
    // This cannot happen since the exclusion command has to be within a
    // file.  Code kept in case online exclusions allowed later.
    if (newnum==0)
    {
        g_sf_num_entries = 0;
        s_sf_entries.clear();
        return;
    }
#endif
    s_sf_entries.erase(s_sf_entries.begin() + start, s_sf_entries.begin() + end + 1);
    g_sf_num_entries = size_cast<int>(s_sf_entries);
    if (g_sf_verbose)
    {
        fmt::print("Excluded file: {}\n", fname);
    }
}

void sf_edit_file(std::string_view filespec)
{
    if (filespec.empty())
    {
        return;         // empty, do nothing (error later?)
    }
    char filechar = filespec.front();
    std::string file_name;
    // if more than one character use as filename
    if (filespec.size() > 1)
    {
        file_name.assign(filespec);
    }
    else if (filechar == '"')   // edit local group
    {
        // Note: should probably be changed to use sf_ file functions
        file_name = (fs::path{get_env_var("SCOREDIR", DEFAULT_SCOREDIR)} / "%C").generic_string();
    }
    else if (filechar == '*')   // edit global scorefile
    {
        // Note: should probably be changed to use sf_ file functions
        file_name = (fs::path{get_env_var("SCOREDIR", DEFAULT_SCOREDIR)} / "global").generic_string();
    }
    else        // abbreviation
    {
        const std::string &abbreviation = s_sf_abbr[static_cast<unsigned char>(filechar)];
        if (abbreviation.empty())
        {
            fmt::print("\nBad file abbreviation: {}\n", filechar);
            return;
        }
        file_name = abbreviation;
    }
    const std::string fname_noexpand{sf_cmd_fname(file_name)};
    const fs::path    expanded_file{file_exp(fname_noexpand)};
    // make sure directory exists...
    std::error_code error;
    fs::create_directories(expanded_file.parent_path(), error);
    if (!error)
    {
        (void)edit_file(fname_noexpand);
        sf_file_clear();
    }
    else
    {
        fmt::print("Can't make {}\n", expanded_file.generic_string());
    }
}

// returns file number
// if file number is negative, the file does not exist or cannot be opened
static int sf_open_file(std::string_view name)
{
    std::size_t i;

    if (name.empty())
    {
        return 0;       // unable to open
    }
    const std::string name_text{name};
    for (i = 0; i < s_sf_files.size(); i++)
    {
        if (s_sf_files[i].fname == name_text)
        {
            if (!s_sf_files[i].exists)          // nonexistent
            {
                return -1;      // no such file
            }
            return static_cast<int>(i);
        }
    }
    s_sf_files.push_back(ScoreFile{});
    i = s_sf_files.size() - 1;
    ScoreFile &file = s_sf_files[i];
    file.fname = name_text;

    std::string temp_name;
    std::ifstream input;
    if (sf_is_url(name_text))
    {
        temp_name = temp_filename();
        if (!s_url_getter(std::string_view{name_text}.substr(4), temp_name))
        {
            return -1;
        }
        input.open(temp_name);
    }
    else
    {
        input.open(fs::path{name_text});
    }
    if (!input)
    {
        return -1;
    }
    file.exists = true;
    std::string line;
    line.reserve(LINE_BUF_LEN);
    while (std::getline(input, line))
    {
        file.lines.push_back(line);
    }
    input.close();
    if (!temp_name.empty())
    {
        std::error_code error;
        fs::remove(temp_name, error);
    }
    return static_cast<int>(i);
}

static void sf_file_clear()
{
    s_sf_files.clear();
}
