/* scorefile.cpp
 *
 * A simple "proof of concept" scoring file for headers.
 * (yeah, right. :)
 */
// This file Copyright 1992, 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile-internal.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <trn/cache.h>
#include <trn/head.h>
#include <trn/mempool.h>
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

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

static std::string_view sf_get_extra_header(ArticleNum art, int hnum);
static bool             sf_default_url_get(std::string_view url, const char *outfile);

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
static char          **s_sf_abbr{};           // abbreviations
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

static int sf_open_file(const char *name);
static void sf_file_clear();
static char *sf_file_get_line(int fnum);
static void  sf_grow();
static int   sf_check_extra_headers(const char *head);
static void  sf_add_extra_header(const char *head);
static std::string sf_get_filename(int level);
static std::string sf_cmd_fname(std::string_view s);
static bool  sf_do_command(char *cmd, bool check);
static char *sf_freeform(char *start1, char *end1);
static bool  sf_do_line(char *line, bool check);
static void  sf_do_file(const char *fname);
static int   score_match(const char *str, int ind);
static std::string sf_missing_score(const char *line);
static std::string sf_get_line(ArticleNum a, HeaderLineType h);
static void  sf_print_match(int indx);
static void  sf_exclude_file(const char *fname);

static bool sf_default_url_get(std::string_view url, const char *outfile)
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
    s_sf_abbr = (char**)safe_malloc(256 * sizeof (char*));
    std::memset((char*)s_sf_abbr,0,256 * sizeof (char*));

    if (g_sf_verbose)
    {
        std::printf("\nReading score files...\n");
    }
    s_sf_file_level = 0;
    // find # of levels
    const std::string group_name = file_exp("%C");
    int level = 0;
    for (char ch : group_name)
    {
        if (ch == '.')
        {
            level++;            // count dots in group name
        }
    }
    level++;

    // the main read-in loop
    for (int i = 0; i <= level; i++)
    {
        std::string s = sf_get_filename(i);
        if (!s.empty())
        {
            sf_do_file(s.c_str());
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
                for (j = i+1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_KILL_THRESHOLD)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) // no later thresholds
                {
                    std::printf("killthreshold %d\n",g_kill_thresh);
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
                for (j = i+1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_NEW_AUTHOR)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) // no later newauthors
                {
                    std::printf("New Author score: %d\n",s_new_author);
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
                for (j = i+1; j < g_sf_num_entries; j++)
                {
                    if (s_sf_entries[j].head_type == SF_REPLY)
                    {
                        break;
                    }
                }
                if (j == g_sf_num_entries) // no later reply rules
                {
                    std::printf("Reply score: %d\n",s_reply_score);
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
    mp_free(MP_SCORE1);         // free memory pool
    if (s_sf_abbr)
    {
        for (int i = 0; i < 256; i++)
        {
            if (s_sf_abbr[i])
            {
                std::free(s_sf_abbr[i]);
                s_sf_abbr[i] = nullptr;
            }
        }
        std::free(s_sf_abbr);
    }
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
//char* head;           // header name, (without ':' character)
static int sf_check_extra_headers(const char *head)
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
//char* head;           // new header name, (without ':' character)
static void sf_add_extra_header(const char *head)
{
    std::string header_name{head};
    header_name += ':';

    // check to see if it's already known
    // first see if it is a known system header
    if (set_line_type(header_name.data(), header_name.data() + header_name.size() - 1) != SOME_LINE)
    {
        return; // known types should be interpreted in normal way
    }
    // then check to see if it's a known extra header
    if (sf_check_extra_headers(head) >= 0)
    {
        return;
    }

    header_name.pop_back();
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
    parse_header(art);   // fast if already parsed

    const std::string &head = s_sf_extra_headers[hnum];
    int                len = static_cast<int>(head.size());

    for (const char *s = g_head_buf; s && *s && *s != '\n'; s++)
    {
        if (string_case_equal(head.c_str(), s, len))
        {
            s = std::strchr(s,':');
            if (!s)
            {
                return {};
            }
            s++;        // skip the colon
            s = skip_hor_space(s);
            if (!*s)
            {
                return {};
            }
            const char *text = s;
            s = std::strchr(s,'\n');
            if (!s)
            {
                return {};
            }
            return {text, static_cast<std::size_t>(s - text)};
        }
        s = std::strchr(s,'\n');     // '\n' will be skipped on loop increment
    }
    return {};
}

// filenames of type a/b/c/foo.bar.misc for group foo.bar.misc
static std::string sf_get_filename(int level)
{
    std::string filename = file_exp(get_val_const("SCOREDIR", DEFAULT_SCOREDIR));
    filename += "/";
    if (!level)
    {
        // allow environment variable later...
        filename += "global";
    }
    else
    {
        filename += file_exp("%C");
        std::string::size_type pos = filename.rfind('/');
        // maybe redo this logic later...
        while (level--)
        {
            if (pos == filename.size()) // no more name to match
            {
                return {};
            }
            pos = filename.find('.', pos);
            if (pos == std::string::npos)
            {
                pos = filename.size();
            }
            if (pos < filename.size() && level)
            {
                pos++;
            }
        }
        filename.resize(pos); // cut end of score file
    }
    return filename;
}

// given a string, if no slashes prepends SCOREDIR env. variable
static std::string sf_cmd_fname(std::string_view s)
{
    if (s.find('/') != std::string_view::npos)
    {
        return std::string{s};
    }
    // no slashes in this filename
    return (fs::path{get_val_const("SCOREDIR", DEFAULT_SCOREDIR)} / std::string{s}).generic_string();
}

// returns true if good command, false otherwise
//char* cmd;            // text of command
//bool check;           // if true, just check, don't execute
static bool sf_do_command(char *cmd, bool check)
{
    char* s;
    int i;

    if (!std::strncmp(cmd, "killthreshold", 13))
    {
        // skip whitespace and = sign
        for (s = cmd+13; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }

        // make **sure** that there is a number here
        i = std::atoi(s);
        if (i == 0)             // it might not be a number
        {
            if (!is_text_zero(s))
            {
                std::printf("\nBad killthreshold: %s",cmd);
                return false;   // continue looping
            }
        }
        if (check)
        {
            return true;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_KILL_THRESHOLD);
        s_sf_entries[g_sf_num_entries-1].score = i;
        return true;
    }
    if (!std::strncmp(cmd, "savescores", 10))
    {
        // skip whitespace and = sign
        for (s = cmd+10; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }
        if (!std::strncmp(s, "off", 3))
        {
            if (!check)
            {
                g_sc_saves_cores = false;
            }
            return true;
        }
        if (*s)         // there is some argument
        {
            if (check)
            {
                return true;
            }
            g_sc_saves_cores = true;
            return true;
        }
        std::printf("Bad savescores command: |%s|\n",cmd);
        return false;
    }
    if (!std::strncmp(cmd, "newauthor", 9))
    {
        // skip whitespace and = sign
        for (s = cmd+9; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }

        // make **sure** that there is a number here
        i = std::atoi(s);
        if (i == 0)             // it might not be a number
        {
            if (!is_text_zero(s))
            {
                std::printf("\nBad newauthor: %s",cmd);
                return false;   // continue looping
            }
        }
        if (check)
        {
            return true;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_NEW_AUTHOR);
        s_sf_entries[g_sf_num_entries-1].score = i;
        return true;
    }
    if (!std::strncmp(cmd, "include", 7))
    {
        if (check)
        {
            return true;
        }
        s = skip_hor_space(cmd + 7); // skip whitespace
        if (!*s)
        {
            std::printf("Bad include command (missing filename)\n");
            return false;
        }
        sf_do_file(file_exp(sf_cmd_fname(s)).c_str());
        return true;
    }
    if (!std::strncmp(cmd, "exclude", 7))
    {
        if (check)
        {
            return true;
        }
        s = skip_hor_space(cmd + 7); // skip whitespace
        if (!*s)
        {
            std::printf("Bad exclude command (missing filename)\n");
            return false;
        }
        sf_exclude_file(file_exp(sf_cmd_fname(s)).c_str());
        return true;
    }
    if (!std::strncmp(cmd, "header", 6))
    {
        s = skip_hor_space(cmd + 7); // skip whitespace
        char *s2 = skip_ne(s, ':');
        if (!s2)
        {
            std::printf("\nBad header command (missing :)\n%s\n",cmd);
            return false;
        }
        if (check)
        {
            return true;
        }
        *s2 = '\0';
        sf_add_extra_header(s);
        *s2 = ':';
        return true;
    }
    if (!std::strncmp(cmd, "begin", 5))
    {
        s = skip_hor_space(cmd + 6); // skip whitespace
        if (!std::strncmp(s, "score", 5))
        {
            // do something useful later
            return true;
        }
        return true;
    }
    if (!std::strncmp(cmd, "reply", 5))
    {
        // skip whitespace and = sign
        for (s = cmd+5; *s && (is_hor_space(*s) || *s == '='); s++)
        {
        }

        // make **sure** that there is a number here
        i = std::atoi(s);
        if (i == 0)             // it might not be a number
        {
            if (!is_text_zero(s))
            {
                std::printf("\nBad reply command: %s\n",cmd);
                return false;   // continue looping
            }
        }
        if (check)
        {
            return true;
        }
        sf_grow();
        s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_REPLY);
        s_sf_entries[g_sf_num_entries-1].score = i;
        return true;
    }
    if (!std::strncmp(cmd, "file", 4))
    {
        if (check)
        {
            return true;
        }
        s = skip_hor_space(cmd + 4); // skip whitespace
        if (!*s)
        {
            std::printf("Bad file command (missing parameters)\n");
            return false;
        }
        char ch = *s++;
        s = skip_hor_space(s); // skip whitespace
        if (!*s)
        {
            std::printf("Bad file command (missing parameters)\n");
            return false;
        }
        if (s_sf_abbr[(int)ch])
        {
            std::free(s_sf_abbr[(int)ch]);
        }
        s_sf_abbr[(int)ch] = save_str(sf_cmd_fname(s));
        return true;
    }
    if (!std::strncmp(cmd, "end", 3))
    {
        s = skip_hor_space(cmd + 4); // skip whitespace
        if (!std::strncmp(s, "score", 5))
        {
            // do something useful later
            return true;
        }
        return true;
    }
    if (!std::strncmp(cmd, "newsclip", 8))
    {
        std::printf("Newsclip is no longer supported.\n");
        return false;
    }
    // no command matched
    std::printf("Unknown command: |%s|\n",cmd);
    return false;
}

//char* start1;         // points to first character of keyword
//char* end1;           // points to last  character of keyword
static char *sf_freeform(char *start1, char *end1)
{
    char*s;

    bool error = false; // be optimistic :-)
    // cases are # of letters in keyword
    switch (end1 - start1 + 1)
    {
    case 7:
        if (!std::strncmp(start1,"pattern",7))
        {
            s_sf_pattern_status = true;
            break;
        }
        error = true;
        break;

    case 4:
#ifdef UNDEF
        // here is an example of a hypothetical freeform key with an argument
        if (!std::strncmp(start1,"date",4))
        {
            char* s1;
            int datenum;
            // skip whitespace and = sign
            s = skip_hor_space(end1 + 1);
            if (!*s)    // ran out of line
            {
                std::printf("freeform: date keyword: ran out of input\n");
                return s;
            }
            datenum = atoi(s);
            std::printf("Date: %d\n",datenum);
            s = skip_digits(s); // skip datenum
            end1 = s;           // end of key data
            break;
        }
#endif
        error = true;
        break;

    default:
        error = true;
        break;
    }
    if (error)
    {
        s = end1+1;
        char ch = *s;
        *s = '\0';
        std::printf("Scorefile freeform: unknown key: |%s|\n",start1);
        *s = ch;
        return nullptr; // error indicated
    }
    // no error, so skip whitespace at end of key
    return skip_hor_space(end1 + 1);
}

//bool check;           // if true, just check the line, don't act.
static bool sf_do_line(char *line, bool check)
{
    if (!line || !*line)
    {
        return true;            // very empty line
    }
    char *s = line + std::strlen(line) - 1;
    if (*s == '\n')
    {
        *s = '\0';              // kill the newline
    }

    char ch = line[0];
    if (ch == '#')              // comment
    {
        return true;
    }

    // reset any per-line bitflags
    s_sf_pattern_status = false;

    if (std::isalpha(ch))            // command line
    {
        return sf_do_command(line,check);
    }

    // skip whitespace
    s = skip_hor_space(line);
    if (!*s || *s == '#')
    {
        return true;    // line was whitespace or comment after whitespace
    }
    // convert line to lowercase (make optional later?)
    for (char *s2 = s; *s2 != '\0'; s2++)
    {
        if (std::isupper(*s2))
        {
            *s2 = std::tolower(*s2);         // convert to lower case
        }
    }
    int i = std::atoi(s);
    if (i == 0)         // it might not be a number
    {
        if (!is_text_zero(s))
        {
            std::printf("\nBad scorefile line:\n|%s|\n",s);
            return false;
        }
    }
    // add the line as a scoring entry
    while (std::isdigit(*s) || *s == '+' || *s == '-' || is_hor_space(*s))
    {
        s++;    // skip score
    }
    char *s2;
    while (true)
    {
        for (s2 = s; *s2 && !is_hor_space(*s2); s2++)
        {
        }
        s2--;
        if (*s2 == ':') // did header
        {
            break;      // go to set header routine
        }
        s = sf_freeform(s,s2);
        if (!s || !*s)          // used up all the line's text, or error
        {
            std::printf("Scorefile entry error error (freeform parse).  ");
            std::printf("Line was:\n|%s|\n",line);
            return false;       // error
        }
    } // while
    // s is start of header name, s2 points to the ':' character
    int j = set_line_type(s, s2);
    if (j == SOME_LINE)
    {
        *s2 = '\0';
        j = sf_check_extra_headers(s);
        *s2 = ':';
        if (j >= 0)
        {
            j += HEAD_LAST;
        }
        else
        {
            std::printf("Unknown score header type.  Line follows:\n|%s|\n",line);
            return false;
        }
    }
    // skip whitespace
    s = skip_hor_space(++s2);
    if (!*s)    // no pattern
    {
        std::printf("Empty score pattern.  Line follows:\n|%s|\n",line);
        return false;
    }
    if (check)
    {
        return true;            // limits of check
    }
    sf_grow();          // acutally make an entry
    s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(j);
    s_sf_entries[g_sf_num_entries-1].score = i;
    if (s_sf_pattern_status)    // in pattern matching mode
    {
        s_sf_entries[g_sf_num_entries-1].flags |= 1;
        s_sf_entries[g_sf_num_entries-1].str1 = mp_save_str(s,MP_SCORE1);
        s_sf_compex = new CompiledRegex;
        s_sf_compex->init_compex();
        // compile arguments:
        // 1st is COMPEX to store compiled regex in
        // 2nd is search string
        // 3rd should be true if the search string is a regex
        // 4th is true for case-insensitivity
        const char *compile_error = s_sf_compex->compile(s, true, true);
        if (compile_error != nullptr)
        {
            std::printf("Bad pattern : |%s|\n",s);
            std::printf("Compex returns: |%s|\n",compile_error);
            s_sf_compex->free_compex();
            delete s_sf_compex;
            s_sf_compex = nullptr;
            s_sf_entries[g_sf_num_entries-1].compex = nullptr;
            return false;
        }
        s_sf_entries[g_sf_num_entries-1].compex = s_sf_compex;
    }
    else
    {
        s_sf_entries[g_sf_num_entries-1].flags &= 0xfe;
        s_sf_entries[g_sf_num_entries-1].str2 = nullptr;
        // Note: consider allowing * wildcard on other header filenames
        if (j == FROM_LINE)     // may have * wildcard
        {
            s2 = std::strchr(s, '*');
            if (s2 != nullptr)
            {
                s_sf_entries[g_sf_num_entries - 1].str2 = mp_save_str(s2 + 1, MP_SCORE1);
                *s2 = '\0';
            }
        }
        s_sf_entries[g_sf_num_entries-1].str1 = mp_save_str(s,MP_SCORE1);
    }
    return true;
}

static void sf_do_file(const char *fname)
{
    int sf_fp = sf_open_file(fname);
    if (sf_fp < 0)
    {
        return;
    }
    s_sf_file_level++;
    if (g_sf_verbose)
    {
        for (int i = 1; i < s_sf_file_level; i++)
        {
            std::printf(".");                // maybe later putchar...
        }
        std::printf("Score file: %s\n",fname);
    }
    std::string safefilename{fname};
    // add end marker to scoring array
    sf_grow();
    s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_FILE_MARK_START);
    // file_level is 1 to n
    s_sf_entries[g_sf_num_entries-1].score = s_sf_file_level;
    s_sf_entries[g_sf_num_entries-1].str2 = nullptr;
    s_sf_entries[g_sf_num_entries-1].str1 = save_str(safefilename.c_str());

    while (char *s = sf_file_get_line(sf_fp))
    {
        std::string line{s};
        (void)sf_do_line(line.data(),false);
    }
    // add end marker to scoring array
    sf_grow();
    s_sf_entries[g_sf_num_entries-1].head_type = static_cast<HeaderLineType>(SF_FILE_MARK_END);
    // file_level is 1 to n
    s_sf_entries[g_sf_num_entries-1].score = s_sf_file_level;
    s_sf_entries[g_sf_num_entries-1].str2 = nullptr;
    s_sf_entries[g_sf_num_entries-1].str1 = save_str(safefilename.c_str());
    s_sf_file_level--;
}

//const char* str;      // string to match on
//int ind;              // index into s_sf_entries
static int score_match(const char *str, int ind)
{
    const char *s1 = s_sf_entries[ind].str1;
    const char *s2 = s_sf_entries[ind].str2;

    if (s_sf_entries[ind].flags & 1)    // pattern style match
    {
        if (s_sf_entries[ind].compex != nullptr)
        {
            // we have a good pattern
            s2 = s_sf_entries[ind].compex->execute(str);
            if (s2 != nullptr)
            {
                return true;
            }
        }
        return false;
    }
    // default case
    const char *s3 = std::strstr(str, s1);
    return s3 != nullptr && (!s2 || std::strstr(s3 + std::strlen(s1), s2));
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
        HeaderLineType h = s_sf_entries[i].head_type;
        if (h <= 0)     // don't use command headers for scoring
        {
            continue;   // the outer for loop
        }
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
                if (score_match(s.c_str(), j))
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
        sum = sum+s_new_author;  // add new author bonus
        if (g_sf_score_verbose)
        {
            std::printf("New Author: %d\n",s_new_author);
            // consider: print which file the bonus came from
        }
    }
    if (s_reply_active)
    {
        // should be in cache if a rule above used the subject
        const char *reply_subject = fetch_cache(a, SUBJ_LINE, true);
        // later: consider other possible reply forms (threading?)
        if (reply_subject != nullptr)
        {
            char reply_subject_buf[LINE_BUF_LEN];
            safe_copy(reply_subject_buf, reply_subject, sizeof reply_subject_buf);
            if (subject_has_re(reply_subject_buf, nullptr))
            {
                sum = sum+s_reply_score;
                if (g_sf_score_verbose)
                {
                    std::printf("Reply: %d\n",s_reply_score);
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
static std::string sf_missing_score(const char *line)
{
    // save line since it is probably pointing at (the TRN-global) g_buf
    std::string saved_line{line};
    std::printf("Possibly missing score.\n"
           "Type a score now or delete the colon to abort this entry:\n");
    g_buf[0] = ':';
    g_buf[1] = FINISH_CMD;
    if (!finish_command(true)) // print the CR
    {
        return {}; // there was no score
    }
    std::string result{g_buf + 1};
    result += ' ';
    result += saved_line;
    return result;
}

// Interprets the '\"' command for creating new score entries online
// consider using some external buffer rather than the 2 internal ones
void sf_append(char *line)
{
    if (!line)
    {
        return; // do nothing with empty string
    }

    char filechar = *line; // ch is file abbreviation

    if (filechar == '?') // list known file abbreviations
    {
        std::printf("List of abbreviation/file pairs\n");
        for (int i = 0; i < 256; i++)
        {
            if (s_sf_abbr[i])
            {
                std::printf("%c %s\n", (char) i, s_sf_abbr[i]);
            }
        }
        std::printf("\" [The current newsgroup's score file]\n");
        std::printf("* [The global score file]\n");
        return;
    }

    // skip whitespace after filechar
    char *scoreline = skip_hor_space(line + 1);
    std::string missing_scoreline;

    char ch = *scoreline; // first non-whitespace after filechar
    // If the scorefile line does not begin with a number,
    // and is not a valid command, request a score
    if (!std::isdigit(ch) && ch != '+' && ch != '-' && ch != ':' && ch != '!' && ch != '#')
    {
        if (!sf_do_line(scoreline, true)) // just checking
        {
            missing_scoreline = sf_missing_score(scoreline);
            if (missing_scoreline.empty()) // no score typed
            {
                std::printf("Score entry aborted.\n");
                return;
            }
            scoreline = missing_scoreline.data();
        }
    }

    // scoretext = first non-whitespace after score#
    std::string_view  scoretext{scoreline};
    const std::size_t scoretext_start = scoretext.find_first_not_of("0123456789+- \t");
    scoretext.remove_prefix(scoretext_start == std::string_view::npos ? scoretext.size() : scoretext_start);
    std::string shortcut_scoreline;

    // special one-character shortcuts
    if (scoretext.size() == 1)
    {
        const std::size_t prefix_size = scoretext.data() - scoreline;
        switch (scoretext.front())
        {
        case 'F': // domain-shortened FROM line
            shortcut_scoreline.assign(scoreline, prefix_size);
            shortcut_scoreline += file_exp("from: %y");
            scoreline = shortcut_scoreline.data();
            break;

        case 'S': // current subject
        {
            const char *s = fetch_cache(g_art, SUBJ_LINE, true);
            if (!s || !*s)
            {
                std::printf("No subject: score entry aborted.\n");
                return;
            }
            if (s[0] == 'R' && s[1] == 'e' && s[2] == ':' && s[3] == ' ')
            {
                s += 4;
            }
            shortcut_scoreline.assign(scoreline, prefix_size);
            shortcut_scoreline += "subject: ";
            // Preserve the historical LINE_BUF_LEN-derived subject limit.
            shortcut_scoreline.append(std::string_view{s}.substr(0, 900));
            scoreline = shortcut_scoreline.data();
            break;
        }

        default:
            std::printf("\nBad scorefile line: |%s| (not added)\n", line);
            return;
        }
        std::printf("%s\n", scoreline);
    }

    // test the scoring line unless filechar is '!' (meaning do it now)
    if (!sf_do_line(scoreline, filechar != '!'))
    {
        std::printf("Bad score line (ignored)\n");
        return;
    }
    if (filechar == '!')
    {
        return; // don't actually append to file
    }
    std::string filename;
    if (filechar == '"') // do local group
    {
        // Note: should probably be changed to use sf_ file functions
        filename = get_val_const("SCOREDIR", DEFAULT_SCOREDIR);
        filename += "/%C";
    }
    else if (filechar == '*') // do global scorefile
    {
        // Note: should probably be changed to use sf_ file functions
        filename = get_val_const("SCOREDIR", DEFAULT_SCOREDIR);
        filename += "/global";
    }
    else if (!s_sf_abbr[(int) filechar])
    {
        std::printf("\nBad file abbreviation: %c\n", filechar);
        return;
    }
    else
    {
        filename = s_sf_abbr[(int) filechar];
    }
    const fs::path score_file{file_exp(sf_cmd_fname(filename))}; // allow shortcuts
    // make sure directory exists...
    std::error_code error;
    fs::create_directories(score_file.parent_path(), error);
    sf_file_clear();
    std::FILE *fp = std::fopen(score_file.string().c_str(), "a");
    if (fp != nullptr)
    {
        std::fprintf(fp, "%s\n", scoreline); // open (or create) for append
        std::fclose(fp);
    }
    else // unsuccessful in opening file
    {
        std::printf("\nCould not open (for append) file %s\n", score_file.string().c_str());
    }
}

// returns a lowercased copy of the header line type h
static std::string sf_get_line(ArticleNum a, HeaderLineType h)
{
    std::string_view line;

    if (h <= SOME_LINE)
    {
        std::printf("sf_get_line(%d,%d): bad header type\n",(int)a.value_of(),h);
        std::printf("(Internal error: header number too low)\n");
        return {};
    }
    if (h >= HEAD_LAST)
    {
        if (h - HEAD_LAST < size_cast<int>(s_sf_extra_headers))
        {
            line = sf_get_extra_header(a,h-HEAD_LAST);
        }
        else
        {
            std::printf("sf_get_line(%d,%d): bad header type\n",(int)a.value_of(),h);
            std::printf("(Internal error: header number too high)\n");
            return {};
        }
    }
    else if (h == SUBJ_LINE)
    {
        if (const char *s = fetch_cache(a,h,true))       // get compressed copy
        {
            line = s;
        }
    }
    else
    {
        if (char *s = prefetch_lines(a,h,false))   // don't make a copy
        {
            line = s;
        }
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
    const char*head_name;
    const char*pattern;

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
                if (s_sf_entries[k].head_type == static_cast<HeaderLineType>(SF_FILE_MARK_START) //
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
        if (s_sf_entries[i].head_type == static_cast<HeaderLineType>(SF_FILE_MARK_START) //
            && s_sf_entries[i].score <= level)
        {
            level--;    // go out...
            for (int k = 0; k < level; k++)
            {
                std::printf(".");            // make putchar later?
            }
            std::printf("From file: %s\n",s_sf_entries[i].str1);
            if (level == 0)             // top level
            {
                break;          // out of the big for loop
            }
        }
    }
    if (s_sf_entries[indx].flags & 1)   // regex type
    {
        pattern = "pattern ";
    }
    else
    {
        pattern = "";
    }

    if (s_sf_entries[indx].head_type >= HEAD_LAST)
    {
        head_name = s_sf_extra_headers[s_sf_entries[indx].head_type - HEAD_LAST].c_str();
    }
    else
    {
        head_name = g_header_type[s_sf_entries[indx].head_type].name.c_str();
    }
    std::printf("%d %s%s: %s", s_sf_entries[indx].score,pattern,head_name,
           s_sf_entries[indx].str1);
    if (s_sf_entries[indx].str2)
    {
        std::printf("*%s",s_sf_entries[indx].str2);
    }
    std::printf("\n");
}

static void sf_exclude_file(const char *fname)
{
    int       start;
    int       end;

    for (start = 0; start < g_sf_num_entries; start++)
    {
        if (s_sf_entries[start].head_type == static_cast<HeaderLineType>(SF_FILE_MARK_START)
         && !std::strcmp(s_sf_entries[start].str1,fname))
        {
            break;
        }
    }
    if (start == g_sf_num_entries)
    {
        std::printf("Exclude: file |%s| was not included\n",fname);
        return;
    }
    for (end = start+1; end < g_sf_num_entries; end++)
    {
        if (s_sf_entries[end].head_type==SF_FILE_MARK_END
         && !std::strcmp(s_sf_entries[end].str1,fname))
        {
            break;
        }
    }
    if (end == g_sf_num_entries)
    {
        std::printf("Exclude: file |%s| is incomplete at exclusion command\n",
                fname);
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
        std::printf("Excluded file: %s\n",fname);
    }
}

//char* filespec;               // file abbrev. or name
void sf_edit_file(const char *filespec)
{
    if (!filespec || !*filespec)
    {
        return;         // empty, do nothing (error later?)
    }
    char filechar = *filespec;
    std::string file_name;
    // if more than one character use as filename
    if (filespec[1])
    {
        file_name = filespec;
    }
    else if (filechar == '"')   // edit local group
    {
        // Note: should probably be changed to use sf_ file functions
        file_name = get_val_const("SCOREDIR", DEFAULT_SCOREDIR);
        file_name += "/%C";
    }
    else if (filechar == '*')   // edit global scorefile
    {
        // Note: should probably be changed to use sf_ file functions
        file_name = get_val_const("SCOREDIR", DEFAULT_SCOREDIR);
        file_name += "/global";
    }
    else        // abbreviation
    {
        if (!s_sf_abbr[(int) filechar])
        {
            std::printf("\nBad file abbreviation: %c\n",filechar);
            return;
        }
        file_name = s_sf_abbr[(int) filechar];
    }
    const std::string fname_noexpand{sf_cmd_fname(file_name)};
    const std::string expanded_file{file_exp(fname_noexpand)};
    // make sure directory exists...
    if (!make_dir(expanded_file.c_str(), MD_FILE))
    {
        (void)edit_file(fname_noexpand.c_str());
        sf_file_clear();
    }
    else
    {
        std::printf("Can't make %s\n", expanded_file.c_str());
    }
}

// returns file number
// if file number is negative, the file does not exist or cannot be opened
static int sf_open_file(const char *name)
{
    std::size_t i;

    if (!name || !*name)
    {
        return 0;       // unable to open
    }
    for (i = 0; i < s_sf_files.size(); i++)
    {
        if (s_sf_files[i].fname == name)
        {
            if (!s_sf_files[i].exists)          // nonexistent
            {
                return -1;      // no such file
            }
            s_sf_files[i].line_on = 0;
            return static_cast<int>(i);
        }
    }
    s_sf_files.push_back(ScoreFile{});
    i = s_sf_files.size() - 1;
    ScoreFile &file = s_sf_files[i];
    file.fname = name;

    char *temp_name = nullptr;
    if (string_case_equal(name, "URL:", 4))
    {
        temp_name = temp_filename();
        if (!s_url_getter(std::string_view{name}.substr(4), temp_name))
        {
            name = nullptr;
        }
        else
        {
            name = temp_name;
        }
    }
    if (!name)
    {
        return -1;
    }
    std::FILE *fp = std::fopen(name, "r");
    if (!fp)
    {
        return -1;
    }
    file.exists = true;
    std::string line(LINE_BUF_LEN, '\0');
    while (std::fgets(line.data(), LINE_BUF_LEN - 4, fp) != nullptr)
    {
        // I kind of like the next line in a twisted sort of way.
        file.lines.push_back(mp_save_str(line.c_str(), MP_SCORE2));
    }
    std::fclose(fp);
    if (temp_name)
    {
        remove(temp_name);
    }
    return static_cast<int>(i);
}

static void sf_file_clear()
{
    mp_free(MP_SCORE2);
    s_sf_files.clear();
}

static char *sf_file_get_line(int fnum)
{
    if (fnum < 0)
    {
        return nullptr;
    }
    const std::size_t i = static_cast<std::size_t>(fnum);
    if (i >= s_sf_files.size())
    {
        return nullptr;
    }
    ScoreFile &file = s_sf_files[i];
    if (file.line_on >= file.lines.size())
    {
        return nullptr;         // past end of file, or empty file
    }
    // below: one of the more twisted lines of my career  (:-)
    return file.lines[file.line_on++];
}
