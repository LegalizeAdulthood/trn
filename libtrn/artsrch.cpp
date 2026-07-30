/* artsrch.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artsrch.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngstuff.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/search.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>

static CompiledRegex s_sub_compex{}; // last compiled subject search
static CompiledRegex s_art_compex{}; // last compiled normal search

static ArtSearchResult art_search_impl(std::string_view command, bool get_cmd);
static void            truncate_notes_pattern(std::string &pattern_text, std::size_t text_start);
static bool wanted(CompiledRegex *compex, ArticleNum art_num, ArtScope scope);

std::string    g_last_pat;                  // last search pattern
CompiledRegex *g_bra_compex{&s_art_compex}; // current compex with brackets
const std::string_view g_scope_str{"sfHhbBa"}; //
ArtScope       g_art_how_much{};            // search scope
HeaderLineType g_art_srch_hdr{};            // specific header number to search
bool           g_art_do_read{};             // search read articles?
bool           g_kill_thru_kludge{true};    // -k

void art_search_init()
{
    s_sub_compex.init_compex();
    s_art_compex.init_compex();
}

// search for an article containing some pattern

ArtSearchResult art_search(std::string_view command, bool get_cmd)
{
    return art_search_impl(command, get_cmd);
}

static ArtSearchResult art_search_impl(std::string_view command, bool get_cmd)
{
    std::string completed_command;
    char        cmd_chr = command.empty() ? '\0' : command.front(); // what kind of search?
    bool backward = cmd_chr == '?' || cmd_chr == Ctl('p');          // direction of search
    CompiledRegex* compex;               // which compiled expression
    std::string     cmd_lst;             // list of commands to do
    std::string     pattern_text;
    ArtSearchResult ret = SRCH_NOT_FOUND; // assume no commands
    int salt_away = 0;                   // store in KILL file?
    ArtScope how_much;                  // search scope: subj/from/Hdr/head/art
    HeaderLineType search_header;           // header to search if Hdr scope
    bool top_start = false;
    bool do_read;                        // search read articles?
    bool fold_case = true;               // fold upper and lower case?
    int ignore_thru = 0;                 // should we ignore the thru line?
    bool output_level = (!g_use_threads && g_general_mode != GM_SELECTOR);
    ArticleNum search_first;

    g_int_count = 0;
    if (command.empty())
    {
        return SRCH_ABORT;
    }
    if (cmd_chr == '/' || cmd_chr == '?') // normal search?
    {
        if (get_cmd)
        {
            completed_command = finish_command(command.substr(0, 1), false); // get rest of command
            if (completed_command.empty())
            {
                return SRCH_ABORT;
            }
            command = completed_command;
        }
        compex = &s_art_compex;
        const std::string_view search_text = command.substr(1);
        if (!search_text.empty())
        {
            how_much = ARTSCOPE_SUBJECT;
            search_header = SOME_LINE;
            do_read = false;
        }
        else
        {
            how_much = g_art_how_much;
            search_header = g_art_srch_hdr;
            do_read = g_art_do_read;
        }
        pattern_text.reserve(search_text.size());
        std::size_t tail_start{};
        while (tail_start < search_text.size())
        {
            if (search_text[tail_start] == '\\' && tail_start + 1 < search_text.size() &&
                search_text[tail_start + 1] == cmd_chr)
            {
                ++tail_start;
            }
            else if (search_text[tail_start] == cmd_chr)
            {
                break;
            }
            pattern_text += search_text[tail_start];
            ++tail_start;
        }
        if (!pattern_text.empty())
        {
            g_last_pat = pattern_text;
        }
        std::string_view modifier_tail = search_text.substr(tail_start);
        if (!modifier_tail.empty()) // modifiers or commands?
        {
            modifier_tail.remove_prefix(1);
            std::size_t modifier_pos{};
            bool        done_modifiers{};
            while (modifier_pos < modifier_tail.size() && !done_modifiers)
            {
                switch (modifier_tail[modifier_pos])
                {
                case 'f': // scan the From line
                    how_much = ARTSCOPE_FROM;
                    ++modifier_pos;
                    break;

                case 'H': // scan a specific header
                    how_much = ARTSCOPE_ONE_HDR;
                    {
                        std::string header_name;
                        ++modifier_pos;
                        while (modifier_pos < modifier_tail.size())
                        {
                            if (modifier_tail[modifier_pos] == '\\' && modifier_pos + 1 < modifier_tail.size() &&
                                modifier_tail[modifier_pos + 1] == ':')
                            {
                                ++modifier_pos;
                            }
                            else if (modifier_tail[modifier_pos] == ':')
                            {
                                break;
                            }
                            header_name += modifier_tail[modifier_pos];
                            ++modifier_pos;
                        }
                        search_header = get_header_num(header_name);
                    }
                    done_modifiers = true;
                    break;

                case 'h': // scan header
                    how_much = ARTSCOPE_HEAD;
                    ++modifier_pos;
                    break;

                case 'b': // scan body sans signature
                    how_much = ARTSCOPE_BODY_NO_SIG;
                    ++modifier_pos;
                    break;

                case 'B': // scan body
                    how_much = ARTSCOPE_BODY;
                    ++modifier_pos;
                    break;

                case 'a': // scan article
                    how_much = ARTSCOPE_ARTICLE;
                    ++modifier_pos;
                    break;

                case 't': // start from the top
                    top_start = true;
                    ++modifier_pos;
                    break;

                case 'r': // scan read articles
                    do_read = true;
                    ++modifier_pos;
                    break;

                case 'K': // put into KILL file
                    salt_away = 1;
                    ++modifier_pos;
                    break;

                case 'c': // make search case sensitive
                    fold_case = false;
                    ++modifier_pos;
                    break;

                case 'I': // ignore the kill file thru line
                    ignore_thru = 1;
                    ++modifier_pos;
                    break;

                case 'N': // override ignore if -k was used
                    ignore_thru = -1;
                    ++modifier_pos;
                    break;

                default:
                    done_modifiers = true;
                    break;
                }
            }
            modifier_tail.remove_prefix(modifier_pos);
        }
        const std::string_view::const_iterator command_begin =
            std::find_if_not(modifier_tail.begin(), modifier_tail.end(),
                             [](char ch) { return std::isspace(static_cast<unsigned char>(ch)) || ch == ':'; });
        modifier_tail.remove_prefix(static_cast<std::size_t>(command_begin - modifier_tail.begin()));
        if (!modifier_tail.empty())
        {
            if (modifier_tail.front() == 'm')
            {
                do_read = true;
            }
            cmd_lst = modifier_tail;
            if (cmd_lst[0] == 'k') // grandfather clause
            {
                cmd_lst[0] = 'j';
            }
            ret = SRCH_DONE;
        }
        g_art_how_much = how_much;
        g_art_srch_hdr = search_header;
        g_art_do_read = do_read;
        if (g_search_ahead)
        {
            g_search_ahead = ArticleNum{-1};
        }
    }
    else
    {
        int salt_mode = command.size() > 2 && command[2] == 'g' ? 2 : 1;
        const std::string_view finding_str =
            command.size() > 1 && command[1] == 'f' ? "author" : "subject";

        how_much = command.size() > 1 && command[1] == 'f' ? ARTSCOPE_FROM : ARTSCOPE_SUBJECT;
        search_header = SOME_LINE;
        do_read = (cmd_chr == Ctl('p'));
        if (cmd_chr == Ctl('n'))
        {
            ret = SRCH_SUBJ_DONE;
        }
        compex = &s_sub_compex;
        std::size_t finding_start{};
        if (how_much == ARTSCOPE_SUBJECT)
        {
            constexpr std::string_view prefix{": *"};
            pattern_text = fmt::format("{}{}", prefix, do_interp("%\\s"));
            finding_start = prefix.size();
        }
        else
        {
            // TODO: if using thread files, make this "%\\)f"
            pattern_text = do_interp("%\\>f");
            finding_start = 0;
        }
        if (cmd_chr == 'k' || cmd_chr == 'K' || cmd_chr == ',' //
            || cmd_chr == '+' || cmd_chr == '.' || cmd_chr == 's')
        {
            if (cmd_chr != 'k')
            {
                salt_away = salt_mode;
            }
            ret = SRCH_DONE;
            if (cmd_chr == '+')
            {
                cmd_lst = "+";
                if (!ignore_thru && g_kill_thru_kludge)
                {
                    ignore_thru = 1;
                }
            }
            else if (cmd_chr == '.')
            {
                cmd_lst = ".";
                if (!ignore_thru && g_kill_thru_kludge)
                {
                    ignore_thru = 1;
                }
            }
            else if (cmd_chr == 's')
            {
                cmd_lst = cmd_chr + pattern_text;
            }
            else
            {
                if (cmd_chr == ',')
                {
                    cmd_lst = ",";
                }
                else
                {
                    cmd_lst = "j";
                }
                article_ptr(g_art)->mark_as_read();       // this article needs to die
            }
            const std::string finding_text = pattern_text.substr(finding_start);
            if (finding_text.empty())
            {
                if (g_verbose)
                {
                    g_msg = fmt::format("Current article has no {}.", finding_str);
                }
                else
                {
                    g_msg = fmt::format("Null {}.", finding_str);
                }
                error_msg(g_msg);
                ret = SRCH_ABORT;
                goto exit;
            }
            if (g_verbose)
            {
                if (cmd_chr != '+' && cmd_chr != '.')
                {
                    fmt::print("\nMarking {} \"{}\" as read.\n", finding_str, finding_text);
                }
                else
                {
                    fmt::print("\nSelecting {} \"{}\".\n", finding_str, finding_text);
                }
                term_down(2);
            }
        }
        else if (!g_search_ahead)
        {
            g_search_ahead = ArticleNum{-1};
        }

        truncate_notes_pattern(pattern_text, finding_start);
#ifdef DEBUG
        if (g_debug)
        {
            fmt::print("\npattern = {}\n", pattern_text);
            term_down(2);
        }
#endif
    }
    {
        const std::string_view compile_error = compex->compile(pattern_text, true, fold_case);
        if (!compile_error.empty())
        {
            // compile regular expression
            error_msg(compile_error);
            ret = SRCH_ABORT;
            goto exit;
        }
    }
    if (!cmd_lst.empty() && cmd_lst.find('=') != std::string::npos)
    {
        ret = SRCH_ERROR;               // listing subjects is an error?
    }
    if (g_general_mode == GM_SELECTOR)
    {
        if (cmd_lst.empty())
        {
            if (g_sel_mode == SM_ARTICLE)// set the selector's default command
            {
                cmd_lst = "+";
            }
            else
            {
                cmd_lst = "++";
            }
        }
        ret = SRCH_DONE;
    }
    if (salt_away)
    {
        std::string salt_command;
        salt_command.reserve(LINE_BUF_LEN);
        salt_command += '/';
        for (const char ch : pattern_text)
        {
            if (ch == '/')
            {
                salt_command += '\\';
            }
            salt_command += ch;
        }
        salt_command += '/';
        if (do_read)
        {
            salt_command += 'r';
        }
        if (!fold_case)
        {
            salt_command += 'c';
        }
        if (ignore_thru)
        {
            salt_command += (ignore_thru == 1 ? 'I' : 'N');
        }
        if (how_much != ARTSCOPE_SUBJECT)
        {
            salt_command += g_scope_str[how_much];
            if (how_much == ARTSCOPE_ONE_HDR)
            {
                salt_command += g_header_type[search_header].name;
            }
        }
        salt_command += ':';
        if (cmd_lst.empty())
        {
            cmd_lst = "j";
        }
        salt_command += cmd_lst;
        kill_file_append(salt_command, salt_away == 2 ? KF_GLOBAL : KF_LOCAL);
    }
    if (get_cmd)
    {
        if (g_use_threads)
        {
            newline();
        }
        else
        {
            fmt::print("\nSearching...\n");
            term_down(2);
        }
        // give them something to read
    }
    if (ignore_thru == 0 && g_kill_thru_kludge && !cmd_lst.empty() //
        && (cmd_lst[0] == '+' || cmd_lst[0] == '.'))
    {
        ignore_thru = 1;
    }
    search_first = do_read || g_sel_rereading? g_abs_first
                      : (g_mode != MM_PROCESSING_KILL || ignore_thru > 0)? g_first_art : g_kill_first;
    if (top_start || g_art == 0)
    {
        g_art = article_after(g_last_art);
        top_start = false;
    }
    if (backward)
    {
        if (!cmd_lst.empty() && g_art <= g_last_art)
        {
            ++g_art;                // include current article
        }
    }
    else
    {
        if (g_art > g_last_art)
        {
            g_art = article_before(search_first);
        }
        else if (!cmd_lst.empty() && g_art >= g_abs_first)
        {
            --g_art;                // include current article
        }
    }
    if (g_search_ahead > 0)
    {
        if (!backward)
        {
            g_art = article_before(g_search_ahead);
        }
        g_search_ahead = ArticleNum{-1};
    }
    TRN_ASSERT(cmd_lst.empty() || cmd_lst[0] != '\0');
    perform_status_init(g_newsgroup_ptr->m_to_read);
    while (true)
    {
        // check if we're out of articles
        if (backward? ((g_art = article_prev(g_art)) < search_first)
                    : ((g_art = article_next(g_art)) > g_last_art))
        {
            break;
        }
        if (g_int_count)
        {
            g_int_count = 0;
            ret = SRCH_INTR;
            break;
        }
        g_artp = article_ptr(g_art);
        if (do_read || (!(g_artp->m_flags & AF_UNREAD) ^ (!g_sel_rereading ? AF_SEL : AF_NONE)))
        {
            if (wanted(compex, g_art, how_much))
            {
                                    // does the shoe fit?
                if (cmd_lst.empty())
                {
                    return SRCH_FOUND;
                }
                if (perform(cmd_lst, output_level && g_page_line == 1) < 0)
                {
                    return SRCH_INTR;
                }
            }
            else if (output_level && cmd_lst.empty() && !(g_art % ArticleNum{50}))
            {
                fmt::print("...{}", g_art.value_of());
                std::fflush(stdout);
            }
        }
        if (!output_level && g_page_line == 1)
        {
            perform_status(g_newsgroup_ptr->m_to_read, 60 / (how_much + 1));
        }
    }
exit:
    return ret;
}

static void truncate_notes_pattern(std::string &pattern_text, std::size_t text_start)
{
    std::size_t pattern_pos = text_start;
    for (int i = 24; pattern_pos < pattern_text.size() && i--; ++pattern_pos)
    {
        if (pattern_text[pattern_pos] == '\\' && pattern_pos + 1 < pattern_text.size())
        {
            ++pattern_pos;
        }
    }
    pattern_text.resize(pattern_pos);
}

// determine if article fits pattern
// returns true if it exists and fits pattern, false otherwise

static bool wanted(CompiledRegex *compex, ArticleNum art_num, ArtScope scope)
{
    Article* ap = article_find(art_num);

    if (!ap || !(ap->m_flags & AF_EXISTS))
    {
        return false;
    }

    std::string search_text;
    switch (scope)
    {
    case ARTSCOPE_SUBJECT:
    {
        search_text = "Subject: " + fetch_subj_copy(art_num);
#ifdef DEBUG
        if (g_debug & DEB_SEARCH_AHEAD)
        {
            fmt::print("{}\n", search_text);
        }
#endif
        break;
    }

    case ARTSCOPE_FROM:
    {
        search_text = "From: " + prefetch_lines_copy(art_num, FROM_LINE);
        break;
    }

    case ARTSCOPE_ONE_HDR:
    {
        g_untrim_cache = true;
        const std::string header = prefetch_lines_copy(art_num, g_art_srch_hdr);
        g_untrim_cache = false;
        search_text = g_header_type[g_art_srch_hdr].name + ": " + header;
        break;
    }

    default:
    {
        std::string article_line;
        bool        success = false;
        bool        in_sig = false;
        if (scope != ARTSCOPE_BODY && scope != ARTSCOPE_BODY_NO_SIG)
        {
            if (!parse_header(art_num))
            {
                return false;
            }
            // see if it's in the header
            if (compex->execute(g_head_buf)) // does it match?
            {
                return true; // say, "Eureka!"
            }
            if (scope < ARTSCOPE_ARTICLE)
            {
                return false;
            }
        }
        if (g_parsed_art == art_num)
        {
            if (!art_open(art_num, g_header_type[PAST_HEADER].min_pos))
            {
                return false;
            }
        }
        else
        {
            if (!art_open(art_num, (ArticlePosition) 0))
            {
                return false;
            }
            if (!parse_header(art_num))
            {
                return false;
            }
        }
        // loop through each line of the article
        seek_art_buf(g_header_type[PAST_HEADER].min_pos);
        article_line.reserve(LINE_BUF_LEN);
        while (read_art_buf(article_line, false))
        {
            if (scope == ARTSCOPE_BODY_NO_SIG && article_line.size() >= 3 && article_line[0] == '-' &&
                article_line[1] == '-' &&
                (article_line[2] == '\n' ||
                 (article_line[2] == ' ' && article_line.size() >= 4 && article_line[3] == '\n')))
            {
                if (in_sig && success)
                {
                    return true;
                }
                in_sig = true;
            }
            const std::size_t newline_pos = article_line.find('\n');
            if (newline_pos != std::string::npos)
            {
                article_line.resize(newline_pos + 1);
            }
            success = success || compex->execute(article_line);
            if (success && !in_sig) // does it match?
            {
                return true; // say, "Eureka!"
            }
        }
        return false; // out of article, so no match
    }
    }
    return compex->execute(search_text);
}
