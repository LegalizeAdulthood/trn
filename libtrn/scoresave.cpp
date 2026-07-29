/* scoresave.cpp
 *
 * Saving/restoring scores from a file.
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/scoresave.h>

#include <config/common.h>
#include <config/env.h>
#include <trn/cache.h>
#include <trn/ngdata.h>
#include <trn/scan.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/string-algos.h>
#include <trn/util.h> // several
#include <util/env.h> // get_val
#include <util/util2.h>

#include <fmt/format.h>

#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

int g_sc_loaded_count{}; // how many articles were loaded?

static long                     s_sc_save_new{}; // new articles (unloaded)
static std::vector<std::string> s_lines;
static int                      s_loaded{};
static int                      s_used{};
static int                      s_saved{};
static ArticleNum               s_last{};

static void       sc_sv_add(std::string_view str);
static void       sc_sv_del_group(std::string_view gname);
static void       sc_sv_get_file();
static ArticleNum sc_sv_use_line(std::string_view line, ArticleNum a);
static ArticleNum sc_sv_make_line(ArticleNum a);

static void sc_sv_add(std::string_view str)
{
    s_lines.emplace_back(str);
}

static void sc_sv_del_group(std::string_view gname)
{
    auto group = s_lines.begin();
    while (group != s_lines.end())
    {
        if (!group->empty() && group->front() == '!'
            && std::string_view{*group}.substr(1) == gname)
        {
            break;
        }
        ++group;
    }
    if (group == s_lines.end())
    {
        return;         // group not found
    }
    auto next_group = group + 1;
    while (next_group != s_lines.end())
    {
        if (!next_group->empty() && next_group->front() == '!')
        {
            break;
        }
        ++next_group;
    }
    s_lines.erase(group, next_group);
}

// get the file containing scores into memory
static void sc_sv_get_file()
{
    s_lines.clear();

    std::ifstream input{file_exp(get_env_var("SAVESCOREFILE", "%+/savedscores"))};
    if (!input)
    {
// Debug
#if 0
        fmt::print("Could not open score save file for reading.\n");
#endif
        return;
    }
    std::string line;
    while (std::getline(input, line))
    {
        sc_sv_add(line);
    }
}

// save the memory into the score file
void sc_sv_save_file()
{
    if (s_lines.empty())
    {
        return;
    }

    g_waiting = true; // don't interrupt
    const fs::path savename{file_exp(get_env_var("SAVESCOREFILE", "%+/savedscores"))};
    fs::path       temp_name{savename};
    temp_name += ".tmp";
    std::FILE *tmpfp = std::fopen(temp_name.string().c_str(), "w");
    if (!tmpfp)
    {
// Debug
#if 0
        fmt::print("Could not open score save temp file {} for writing.\n", temp_name.string());
#endif
        g_waiting = false;
        return;
    }
    for (const std::string &line : s_lines)
    {
        std::fprintf(tmpfp, "%s\n", line.c_str());
        if (std::ferror(tmpfp))
        {
            std::fclose(tmpfp);
            fmt::print("\nWrite error in temporary save file {}\n", temp_name.string());
            fmt::print("(keeping old saved scores)\n");
            std::error_code error;
            fs::remove(temp_name, error);
            g_waiting = false;
            return;
        }
    }
    std::fclose(tmpfp);
    std::error_code error;
    fs::remove(savename, error);
    error.clear();
    fs::rename(temp_name, savename, error);
    g_waiting = false;
}

// returns the next article number (after the last one used)
// ART_NUM a;    // art number to start with
static ArticleNum sc_sv_use_line(std::string_view line, ArticleNum a)
{
    int        score = 0; // get rid of warning
    const auto digit_count = [](std::string_view text)
    {
        const std::size_t end = text.find_first_not_of("0123456789");
        return end == std::string_view::npos ? text.size() : end;
    };
    const auto parse_int = [](std::string_view text)
    {
        int value{};
        std::from_chars(text.data(), text.data() + text.size(), value);
        return value;
    };

    while (!line.empty())
    {
        const char command = line.front();
        switch (command)
        {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'I':
            // negative starting digit
            {
                const std::size_t score_length = 1 + digit_count(line.substr(1));
                std::string       score_text{line.substr(0, score_length)};
                score_text.front() = static_cast<char>('0' + ('J' - command)); // convert to first digit
                score = -parse_int(score_text);
                line.remove_prefix(score_length);
                s_loaded++;
                if (is_available(a) && article_unread(a))
                {
                    sc_set_score(a, score);
                    s_used++;
                }
                ++a;
                break;
            }

        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'N':
        case 'O':
        case 'P':
        case 'Q':
        case 'R':
        case 'S':
            // positive starting digit
            {
                const std::size_t score_length = 1 + digit_count(line.substr(1));
                std::string       score_text{line.substr(0, score_length)};
                score_text.front() = static_cast<char>('0' + (command - 'J')); // convert to first digit
                score = parse_int(score_text);
                line.remove_prefix(score_length);
                s_loaded++;
                if (is_available(a) && article_unread(a))
                {
                    sc_set_score(a, score);
                    s_used++;
                }
                ++a;
                break;
            }

        case 'r': // repeat
        {
            line.remove_prefix(1);
            const std::size_t repeat_digits = digit_count(line);
            int               repeat_count{};
            if (repeat_digits == 0)
            {
                // simple case, just "r"
                repeat_count = 1;
            }
            else
            {
                repeat_count = parse_int(line.substr(0, repeat_digits));
                line.remove_prefix(repeat_digits);
            }
            for (; repeat_count; repeat_count--)
            {
                s_loaded++;
                if (is_available(a) && article_unread(a))
                {
                    sc_set_score(a, score);
                    s_used++;
                }
                ++a;
            }
            break;
        }

        case 's': // skip
        {
            line.remove_prefix(1);
            const std::size_t skip_digits = digit_count(line);
            if (skip_digits == 0)
            {
                // simple case, just "s"
                ++a;
            }
            else
            {
                a += ArticleNum{parse_int(line.substr(0, skip_digits))};
                line.remove_prefix(skip_digits);
            }
            break;
        }

        default:
            return a;
        } // switch
    } // while
    return a;
}

static ArticleNum sc_sv_make_line(ArticleNum a)
{
    bool        lastscore_valid = false;
    int         num_output = 0;
    ArticleNum  next_start = article_after(g_last_art);
    std::string line{"."};
    int         lastscore = 0;

    for (ArticleNum art = article_first(a); art <= g_last_art && num_output < 50; art = article_next(art))
    {
        next_start = article_next(art);
        if (article_unread(art) && article_scored(art))
        {
            if (s_last != article_before(art))
            {
                if (s_last == article_before(art, 2))
                {
                    line += 's';
                    num_output++;
                }
                else
                {
                    line += fmt::format("s{}", (art.value_of() - s_last.value_of()) - 1);
                    num_output++;
                }
            }
            // print article's score
            int score = article_ptr(art)->m_score;
            // check for repeating scores
            if (score == lastscore && lastscore_valid)
            {
                int        repeat_count = 1;
                ArticleNum repeat_art = article_next(art);
                for (; repeat_art <= g_last_art && article_unread(repeat_art) && article_scored(repeat_art) &&
                       article_ptr(repeat_art)->m_score == score;
                     repeat_count++)
                {
                    repeat_art = article_next(repeat_art);
                }
                art = article_prev(repeat_art); // prepare for the for loop increment
                if (repeat_count == 1)
                {
                    line += 'r'; // repeat one
                    num_output++;
                }
                else
                {
                    line += fmt::format("r{}", repeat_count); // repeat >one
                    num_output++;
                }
                s_saved += repeat_count - 1;
            }
            else // not a repeat
            {
                int  score_value = score;
                bool neg_flag = score_value < 0;
                if (neg_flag)
                {
                    score_value = 0 - score_value;
                }
                std::string score_text = std::to_string(score_value);
                int         first_digit = score_text.front() - '0';
                if (neg_flag)
                {
                    score_text.front() = static_cast<char>('J' - first_digit);
                }
                else
                {
                    score_text.front() = static_cast<char>('J' + first_digit);
                }
                line += score_text;
                num_output++;
                lastscore_valid = true;
            }
            lastscore = score;
            s_last = art;
            next_start = article_next(art);
            s_saved++;
        } // if
    } // for
    if (num_output != 0)
    {
        sc_sv_add(line);
    }
    return next_start;
}

void sc_load_scores()
{
    // lots of cleanup needed here
    ArticleNum a{};

    s_sc_save_new = -1;         // just in case we exit early
    s_loaded = 0;
    s_used = 0;
    g_sc_loaded_count = 0;

    // verbosity is only really useful for debugging...
    bool verbose = false;

    if (s_lines.empty())
    {
        sc_sv_get_file();
    }

    const std::string gname = file_exp("%C");

    std::size_t i;
    for (i = 0; i < s_lines.size(); i++)
    {
        const std::string &line = s_lines[i];
        if (!line.empty() && line.front() == '!'
            && std::string_view{line}.substr(1) == gname)
        {
            break;
        }
    }
    if (i == s_lines.size())
    {
        return;         // no scores loaded
    }
    i++;

    if (verbose)
    {
        fmt::print("\nLoading scores...");
        std::fflush(stdout);
    }
    while (i < s_lines.size())
    {
        std::string &line = s_lines[i++];
        if (line.empty())
        {
            continue;
        }
        switch (line.front())
        {
        case ':':
        {
            long             value{};
            std::string_view text{line};
            text.remove_prefix(1);
            std::from_chars(text.data(), text.data() + text.size(), value);
            a = ArticleNum{value}; // set the article #
            break;
        }

        case '.': // longer score line
            a = sc_sv_use_line(std::string_view{line}.substr(1), a);
            break;

        case '!': // group of shared file
            i = s_lines.size();
            break;

        case 'v':  // version number
            break; // not used now

        case '\0': // empty string
        case '#':  // comment
            break;

        default:
            // don't even try to deal with it
            return;
        } // switch
    } // while

    g_sc_loaded_count = s_loaded;
    a = g_first_art;
    if (g_sa_mode_read_elig)
    {
        a = g_abs_first;
    }
    int total = 0;
    int scored = 0;
    for (ArticleNum art = article_first(a); art <= g_last_art; art = article_next(art))
    {
        if (!article_exists(art))
        {
            continue;
        }
        if (!article_unread(art) && !g_sa_mode_read_elig)
        {
            continue;
        }
        total++;
        if (article_scored(art))
        {
            scored++;
        }
    } // for

    // sloppy plurals (:-)
    if (verbose)
    {
        fmt::print("({}/{}/{} scores loaded/used/unscored)\n", s_loaded, s_used, total - scored);
    }

    s_sc_save_new = total - scored;
    if (g_sa_initialized)
    {
        g_s_top_ent = -1;       // reset top of page
    }
}

void sc_save_scores()
{
    s_saved = 0;
    s_last = ArticleNum{};

    g_waiting = true;   // DON'T interrupt
    const std::string gname = file_exp("%C");
    // not being able to open is OK
    if (!s_lines.empty())
    {
        sc_sv_del_group(gname);  // delete old group
    }
    else                // there was no old file
    {
        sc_sv_add("#STRN saved score file.");
        sc_sv_add("v1.0");
    }
    sc_sv_add(fmt::format("!{}", gname));  // add the header

    ArticleNum a = g_first_art;
    sc_sv_add(fmt::format(":{}", a.value_of()));
    s_last = article_before(a);
    while (a <= g_last_art)
    {
        a = sc_sv_make_line(a);
    }
    g_waiting = false;
}
