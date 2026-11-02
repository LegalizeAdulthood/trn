/* head.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/head.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <trn/ngdata.h>
#include <trn/artio.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/ng.h>
#include <trn/nntp.h>
#include <trn/rt-process.h>
#include <trn/rt-util.h>
#include <trn/string-algos.h>
#include <trn/util.h>
#include <util/util2.h>
#include <parsedate/parsedate.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#define HIDDEN    (HT_HIDE|HT_DEF_HIDE)
#define MAGIC_ON  (HT_MAGIC_OK|HT_MAGIC|HT_DEF_MAGIC)
#define MAGIC_OFF (HT_MAGIC_OK)

#define XREF_CACHED HT_CACHED
#define NGS_CACHED  HT_NONE
#define FILT_CACHED HT_NONE

// This array must stay in the same order as the enum values header_line_type
// clang-format off
HeaderRules g_header_rules = {{
    // name             length   flag
    {"",/*BODY*/              0,      HT_NONE         },
    {"",/*SHOWN*/             0,      HT_NONE         },
    {"",/*HIDDEN*/            0,      HIDDEN          },
    {"",/*CUSTOM*/            0,      HT_NONE         },
    {"unrecognized",          12,     HIDDEN          },
    {"author",                6,      HT_NONE         },
    {"bytes",                 5,      HIDDEN|HT_CACHED},
    {"content-name",          12,     HIDDEN          },
    {"content-disposition",
                              19,     HIDDEN          },
    {"content-length",        14,     HIDDEN          },
    {"content-transfer-encoding",
                              25,     HIDDEN          },
    {"content-type",          12,     HIDDEN          },
    {"distribution",          12,     HT_NONE         },
    {"date",                  4,      MAGIC_ON        },
    {"expires",               7,      HIDDEN|MAGIC_ON },
    {"followup-to",           11,     HT_NONE         },
    {"from",                  4,      MAGIC_OFF|HT_CACHED},
    {"in-reply-to",           11,     HIDDEN          },
    {"keywords",              8,      HT_NONE         },
    {"lines",                 5,      HT_CACHED       },
    {"mime-version",          12,     MAGIC_ON|HIDDEN },
    {"message-id",            10,     HIDDEN|HT_CACHED},
    {"newsgroups",            10,     MAGIC_ON|HIDDEN|NGS_CACHED},
    {"path",                  4,      HIDDEN          },
    {"relay-version",         13,     HIDDEN          },
    {"reply-to",              8,      HT_NONE         },
    {"references",            10,     HIDDEN|FILT_CACHED},
    {"summary",               7,      HT_NONE         },
    {"subject",               7,      MAGIC_ON|HT_CACHED},
    {"xref",                  4,      HIDDEN|XREF_CACHED},
}};
// clang-format on

#undef HIDDEN
#undef MAGIC_ON
#undef MAGIC_OFF
#undef NGS_CACHED
#undef XREF_CACHED

ParsedHeaderContext         g_parsed_header;

static std::string lower_header_name(std::string_view header_name);

void HeaderRules::init()
{
    for (int i = HEAD_FIRST + 1; i < HEAD_LAST; i++)
    {
        type_idx[type[i].name[0] - 'a'] = static_cast<HeaderLineType>(i);
    }

    user_type_max = 10;
    user_type.resize(user_type_max);
    user_type[user_type_count++].name = "*";
}

void HeaderRules::clear()
{
    user_type.clear();
    user_type.shrink_to_fit();
    user_type_count = 0;
}

void head_init()
{
    g_header_rules.init();

    g_parsed_header.head_buf.clear();
    g_parsed_header.head_buf.reserve(LINE_BUF_LEN * 8);
}

void head_final()
{
    g_parsed_header.head_buf.clear();
    g_parsed_header.head_buf.shrink_to_fit();
    g_header_rules.clear();
}

#ifdef DEBUG
static void dump_header(const char *where)
{
    fmt::print("header: {} {}", g_parsed_header.parsed_art.value_of(), where);

    for (int i = HEAD_FIRST - 1; i < HEAD_LAST; i++)
    {
        fmt::print("{:>15} {:4} {:4} {:03o}\n", g_header_rules.type[i].name,
                   g_parsed_header.positions[i].min_pos.value_of(), g_parsed_header.positions[i].max_pos.value_of(),
                   static_cast<unsigned>(g_header_rules.type[i].flags));
    }
}
#endif

static std::string lower_header_name(std::string_view header_name)
{
    std::string result;
    result.reserve(header_name.size());
    for (char ch : header_name)
    {
        const unsigned char uch = static_cast<unsigned char>(ch);
        result += std::isupper(uch) ? static_cast<char>(std::tolower(uch)) : ch;
    }
    return result;
}

HeaderLineType HeaderRules::set_line_type(std::string_view header_name)
{
    for (char ch : header_name)
    {
        // guard against space before :
        if (std::isspace(static_cast<unsigned char>(ch)))
        {
            return SOME_LINE;
        }
    }
    const std::size_t len = header_name.size();

    // now scan the HeaderType table, backwards so we don't have to supply an
    // extra terminating value, using first letter as index, and length as
    // optimization to avoid calling subroutine strEQ unnecessarily.  Hauls.
    //
    if (!header_name.empty())
    {
        const unsigned char first_char = static_cast<unsigned char>(header_name[0]);
        const char first = std::isupper(first_char) ? static_cast<char>(std::tolower(first_char)) : header_name[0];
        if (first >= 'a' && first <= 'z')
        {
            for (int i = type_idx[first - 'a']; type[i].name[0] == first; i--)
            {
                if (len == static_cast<std::size_t>(type[i].length) && string_case_equal(header_name, type[i].name))
                {
                    return static_cast<HeaderLineType>(i);
                }
            }
            if (len == static_cast<std::size_t>(type[CUSTOM_LINE].length) &&
                string_case_equal(header_name, type[CUSTOM_LINE].name))
            {
                return CUSTOM_LINE;
            }
            for (int i = user_type_index[first - 'a']; user_type[i].name[0] == first; i--)
            {
                const std::size_t user_len = static_cast<std::size_t>(user_type[i].length);
                if (len >= user_len //
                    && string_case_equal(header_name.substr(0, user_len), user_type[i].name))
                {
                    if (user_type[i].flags & HT_HIDE)
                    {
                        return HIDDEN_LINE;
                    }
                    return SHOWN_LINE;
                }
            }
        }
    }
    return SOME_LINE;
}

HeaderLineType get_header_num(std::string_view header_name)
{
    HeaderLineType i = g_header_rules.set_line_type(header_name);

    if (i <= SOME_LINE && i != CUSTOM_LINE)
    {
        g_header_rules.type[CUSTOM_LINE].name = lower_header_name(header_name);
        g_header_rules.type[CUSTOM_LINE].length = static_cast<char>(header_name.size());
        g_header_rules.type[CUSTOM_LINE].flags = g_header_rules.type[i].flags;
        g_parsed_header.positions[CUSTOM_LINE].min_pos = ArticlePosition{-1};
        g_parsed_header.positions[CUSTOM_LINE].max_pos = ArticlePosition{};
        if (!g_parsed_header.head_buf.empty())
        {
            const std::string_view header_text{g_parsed_header.head_buf};
            for (std::size_t line_start{}; line_start < header_text.size();)
            {
                const std::size_t line_end = header_text.find('\n', line_start);
                if (line_end == std::string_view::npos || line_end == line_start)
                {
                    break;
                }
                const std::string_view line = header_text.substr(line_start, line_end - line_start);
                const std::size_t      colon = line.find(':');
                const std::size_t      next_line_start = line_end + 1;
                if (colon == std::string_view::npos ||
                    g_header_rules.set_line_type(line.substr(0, colon)) != CUSTOM_LINE)
                {
                    line_start = next_line_start;
                    continue;
                }
                g_parsed_header.positions[CUSTOM_LINE].min_pos = ArticlePosition{static_cast<long>(line_start)};
                std::size_t header_end = next_line_start;
                while (header_end < header_text.size() && is_hor_space(header_text[header_end]))
                {
                    const std::size_t continuation_end = header_text.find('\n', header_end);
                    if (continuation_end == std::string_view::npos)
                    {
                        header_end = header_text.size();
                        break;
                    }
                    header_end = continuation_end + 1;
                }
                g_parsed_header.positions[CUSTOM_LINE].max_pos = ArticlePosition{static_cast<long>(header_end)};
                break;
            }
        }
        i = CUSTOM_LINE;
    }
    return i;
}

// restart_header may insert into the active g_cache.article_list.
void ParsedHeaderContext::start_header(ArticleNum artnum)
{
#ifdef DEBUG
    if (g_debug & DEB_HEADER)
    {
        dump_header("start_header\n");
    }
#endif
    for (HeaderPosition &i : positions)
    {
        i.min_pos = ArticlePosition{-1};
        i.max_pos = ArticlePosition{};
    }
    restart_header(artnum);
}

// article_ptr looks up or creates an entry in the active g_cache.article_list.
void ParsedHeaderContext::restart_header(ArticleNum artnum)
{
    in_header = SOME_LINE;
    first_one = false;
    parsed_art = artnum;
    parsed_artp = article_ptr(artnum);
}

void ParsedHeaderContext::end_line()
{
    if (first_one) // did we just pass 1st occurrence?
    {
        first_one = false;
        // remember where line left off
        positions[in_header].max_pos = g_art_input.pos;
        if (g_header_rules.type[in_header].flags & HT_CACHED)
        {
            if (parsed_artp->get_cached_line_view(in_header, true).empty())
            {
                int start = positions[in_header].min_pos.value_of() + g_header_rules.type[in_header].length + 1;
                while (is_hor_space(head_buf[start]))
                {
                    start++;
                }
                MemorySize size = g_art_input.pos.value_of() - start + 1 - 1; // pre-strip newline
                if (in_header == SUBJ_LINE)
                {
                    const std::size_t subj_size =
                            size > 0 ? static_cast<std::size_t>(size - 1) : 0;
                    parsed_artp->set_subj_line(std::string_view{head_buf.data() + start, subj_size});
                }
                else
                {
                    const std::size_t line_size = size > 0 ? static_cast<std::size_t>(size - 1) : 0;
                    parsed_artp->set_cached_line(in_header, std::string_view{head_buf.data() + start, line_size});
                }
            }
        }
    }
}

bool ParsedHeaderContext::parse_line(std::string_view art_buf, int new_hide, int old_hide)
{
    if (!art_buf.empty() && is_hor_space(art_buf.front())) // continuation line?
    {
        return old_hide;
    }

    end_line();
    const std::size_t colon = art_buf.find(':');
    if (colon == std::string_view::npos) // is it the end of the header?
    {
        // Did NNTP ship us a mal-formed header line?
        if (reading_nntp_header && !art_buf.empty() && art_buf.front() != '\n')
        {
            in_header = SOME_LINE;
            return new_hide;
        }
        in_header = PAST_HEADER;
    }
    else // it is a new header line
    {
        in_header = g_header_rules.set_line_type(art_buf.substr(0, colon));
        first_one = (positions[in_header].min_pos < ArticlePosition{});
        if (first_one)
        {
            positions[in_header].min_pos = g_art_input.pos;
            if (in_header == DATE_LINE)
            {
                if (!parsed_artp->m_date)
                {
                    const std::size_t date_start = std::min<std::size_t>(6, art_buf.size());
                    const std::string date_text{art_buf.substr(date_start)};
                    parsed_artp->m_date = parsedate(date_text.c_str());
                }
            }
        }
#ifdef DEBUG
        if (g_debug & DEB_HEADER)
        {
            std::string header_text{art_buf};
            dump_header(header_text.data());
        }
#endif
        if (g_header_rules.type[in_header].flags & HT_HIDE)
        {
            return new_hide;
        }
    }
    return false;                       // don't hide this line
}

void end_header()
{
    Article *ap = g_parsed_header.parsed_artp;

    g_parsed_header.end_line();
    g_parsed_header.in_header = PAST_HEADER; // just to be sure

    if (!ap->m_subj)
    {
        ap->set_subj_line("<NONE>");
    }

    if (g_parsed_header.reading_nntp_header)
    {
        g_parsed_header.reading_nntp_header = false;
        g_parsed_header.positions[PAST_HEADER].min_pos =
            g_art_input.pos + ArticlePosition{1}; // nntp_body will fix this
    }
    else
    {
        g_parsed_header.positions[PAST_HEADER].min_pos = tell_art();
    }

    // If there's no References: line, then the In-Reply-To: line may give us
    // more information.
    //
    if (g_threaded_group //
        && (!(ap->m_flags & AF_THREADED) || g_parsed_header.positions[IN_REPLY_LINE].min_pos >= 0))
    {
        if (ap->valid_article())
        {
            Article    *artp_hold = g_nav.target.art;
            std::string references = fetch_lines(g_parsed_header.parsed_art, REFS_LINE);
            references += fetch_lines(g_parsed_header.parsed_art, IN_REPLY_LINE);
            ap->thread_article(references);
            g_nav.target.art = artp_hold;
            ap->check_poster();
        }
    }
    else if (!(ap->m_flags & AF_CACHED))
    {
        ap->cache_article();
        ap->check_poster();
    }
}

// read the header into memory and parse it if we haven't already

bool parse_header(ArticleNum art_num)
{
    int len;
    bool had_nl = true;
    int found_nl;

    if (g_parsed_header.parsed_art == art_num)
    {
        return true;
    }
    if (art_num > g_last_art)
    {
        return false;
    }
    spin(20);
    if (g_data_source->m_flags & DF_REMOTE)
    {
        if (!nntp_art_name(art_num, false).empty())
        {
            if (!art_open(art_num,(ArticlePosition)0))
            {
                return false;
            }
        }
        else if (nntp_header(art_num) <= 0)
        {
            article_ptr(art_num)->uncache_article(false);
            return false;
        }
        else
        {
            g_parsed_header.reading_nntp_header = true;
        }
    }
    else if (!art_open(art_num,(ArticlePosition)0))
    {
        return false;
    }

    g_parsed_header.start_header(art_num);
    g_art_input.pos = ArticlePosition{};
    g_parsed_header.head_buf.clear();
    g_parsed_header.head_buf.reserve(LINE_BUF_LEN * 8);
    std::string nntp_header_line;
    std::string article_line;
    article_line.reserve(LINE_BUF_LEN);
    if (g_parsed_header.reading_nntp_header)
    {
        nntp_header_line.reserve(LINE_BUF_LEN);
    }
    while (g_parsed_header.in_header)
    {
        const std::size_t line_start = g_parsed_header.head_buf.size();
        if (g_parsed_header.reading_nntp_header)
        {
            const NNTPGetsResult result = g_nntp.gets(nntp_header_line, LINE_BUF_LEN);
            found_nl = result == NGSR_FULL_LINE;
            if (result == NGSR_ERROR)
            {
                nntp_header_line = ".";
            }
            if (had_nl && !nntp_header_line.empty() && nntp_header_line.front() == '.')
            {
                if (nntp_header_line.size() == 1)
                {
                    g_parsed_header.head_buf.push_back('\n'); // tag the end with an empty line
                    break;
                }
                nntp_header_line.erase(0, 1);
            }
            len = static_cast<int>(nntp_header_line.size());
            g_parsed_header.head_buf.append(nntp_header_line);
            if (found_nl)
            {
                g_parsed_header.head_buf.push_back('\n');
                len++;
            }
        }
        else
        {
            if (!read_art(article_line))
            {
                break;
            }
            len = static_cast<int>(article_line.size());
            found_nl = (!article_line.empty() && article_line.back() == '\n');
            g_parsed_header.head_buf.append(article_line);
        }
        if (had_nl)
        {
            g_parsed_header.parse_line(std::string_view{g_parsed_header.head_buf.data() + line_start,
                                                        g_parsed_header.head_buf.size() - line_start},
                                       false, false);
        }
        had_nl = found_nl;
        g_art_input.pos += ArticlePosition{len};
    }
    end_header();
    return true;
}

static std::string header_line_text(std::string_view line)
{
    if (line.size() <= 1)
    {
        return {};
    }

    line.remove_suffix(1);
    const std::size_t line_end = line.find('\0');
    return std::string{line.substr(0, line_end)};
}

bool ParsedHeaderContext::line_span(HeaderLineType which_line, char *&line, int &size)
{
    ArticlePosition firstpos = positions[which_line].min_pos;
    if (firstpos < 0)
    {
        line = nullptr;
        size = 0;
        return false;
    }

    firstpos += ArticlePosition{g_header_rules.type[which_line].length + 1};
    ArticlePosition lastpos = positions[which_line].max_pos;
    size = (lastpos - firstpos).value_of();
    line = head_buf.data() + firstpos.value_of();
    while (is_hor_space(*line))
    {
        line++;
        size--;
    }
#ifdef DEBUG
    if (g_debug && (size < 1 || size > 1000))
    {
        fmt::print(stdout, "Firstpos = {}, lastpos = {}\n", firstpos.value_of(), lastpos.value_of());
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
#endif
    return true;
}

std::string ParsedHeaderContext::current_line_text(HeaderLineType which_line)
{
    char *line;
    int   size;

    if (!line_span(which_line, line, size))
    {
        return {};
    }

    return header_line_text(std::string_view{line, static_cast<std::size_t>(std::max(size, 0))});
}

static void append_header_line(std::string &text, std::string_view line)
{
    if (!text.empty())
    {
        text.push_back(' ');
    }
    text.append(line);
    if (!text.empty() && text.back() == '\n')
    {
        text.pop_back();
    }
}

// get a header line from an article

// article to get line from
// type of line desired
std::string fetch_lines(ArticleNum art_num, HeaderLineType which_line)
{
    // Only return a cached line if it isn't the current article
    if (g_parsed_header.parsed_art != art_num)
    {
        // If the line is not in the cache, this will parse the header
        const std::string cached_line = fetch_cache(art_num, which_line, FILL_CACHE);
        if (!cached_line.empty())
        {
            return cached_line;
        }
        if (g_parsed_header.parsed_art != art_num)
        {
            return {};
        }
    }

    char *line;
    int   size;
    if (!g_parsed_header.line_span(which_line, line, size))
    {
        return {};
    }
    return header_line_text(std::string_view{line, static_cast<std::size_t>(std::max(size, 0))});
}

static void prefetch_remote_lines(ArticleNum art_num, HeaderLineType which_line, std::string *owned_result)
{
    Article   *ap;
    ArticleNum num;
    ArticleNum lastnum;
    bool       has_header_values = true;

    if (owned_result != nullptr)
    {
        owned_result->clear();
    }

    spin(20);
    ArticleNum prior_num{article_before(art_num)};
    bool       cached = (g_header_rules.type[which_line].flags & HT_CACHED);
    int        status;
    if (cached != 0)
    {
        lastnum = art_num + ArticleNum{PREFETCH_SIZE - 1};
        lastnum = std::min(lastnum, g_last_art);
        status = nntp_header_values(g_header_rules.type[which_line].name,
                                    fmt::format("{}-{}", art_num.value_of(), lastnum.value_of()));
    }
    else
    {
        lastnum = art_num;
        status = nntp_header_values(g_header_rules.type[which_line].name, std::to_string(art_num.value_of()));
    }
    if (status == -2)
    {
        finalize(1);
    }
    if (status > 0)
    {
        while (true)
        {
            std::string line = nntp_get_a_line();
#ifdef DEBUG
            if (g_debug & DEB_NNTP)
                fmt::print("<{}", line.empty() ? "<EOF>" : line);
#endif
            if (g_nntp.link.at_list_end(line))
            {
                break;
            }
            std::string_view  line_text{line};
            const std::size_t cr = line_text.find('\r');
            if (cr != std::string_view::npos)
            {
                line_text = line_text.substr(0, cr);
            }
            const std::size_t            space = line_text.find(' ');
            const std::string_view       number_text = line_text.substr(0, space);
            long                         article_number{};
            const std::from_chars_result result =
                std::from_chars(number_text.data(), number_text.data() + number_text.size(), article_number);
            if (result.ec != std::errc{} || result.ptr != number_text.data() + number_text.size())
            {
                continue;
            }
            const std::string_view header_line =
                space == std::string_view::npos ? std::string_view{} : line_text.substr(space + 1);
            num = ArticleNum{article_number};
            if (num < art_num || num > lastnum)
            {
                continue;
            }
            if (!(g_data_source->m_flags & DF_XHDR_BROKEN))
            {
                while ((prior_num = article_next(prior_num)) < num)
                {
                    article_ptr(prior_num)->uncache_article(false);
                }
            }
            ap = article_find(num);
            if (which_line == SUBJ_LINE)
            {
                ap->set_subj_line(header_line);
            }
            else if (cached)
            {
                ap->set_cached_line(which_line, header_line);
            }
            if (num == art_num)
            {
                if (owned_result != nullptr)
                {
                    append_header_line(*owned_result, header_line);
                }
            }
        }
    }
    else
    {
        has_header_values = false;
        lastnum = art_num;
        if (!parse_header(art_num))
        {
            fmt::print(stderr, "\nBad NNTP response.\n");
            finalize(1);
        }
        if (owned_result != nullptr)
        {
            *owned_result = g_parsed_header.current_line_text(which_line);
        }
    }
    if (has_header_values && !(g_data_source->m_flags & DF_XHDR_BROKEN))
    {
        for (prior_num = article_first(prior_num); prior_num < lastnum; prior_num = article_next(prior_num))
        {
            article_ptr(prior_num)->uncache_article(false);
        }
    }
}

// prefetch a header line from one or more articles

// ArticleNum art_num           article to get line from
// HeaderLineType which_line    type of line desired
void prefetch_lines(ArticleNum art_num, HeaderLineType which_line)
{
    if ((g_data_source->m_flags & DF_REMOTE) && g_parsed_header.parsed_art != art_num)
    {
        if (!fetch_cache(art_num, which_line, DONT_FILL_CACHE).empty())
        {
            return;
        }
        if (Article *ap = article_find(art_num); ap == nullptr || !(ap->m_flags & AF_EXISTS))
        {
            return;
        }

        prefetch_remote_lines(art_num, which_line, nullptr);
        return;
    }

    // Only return a cached line if it isn't the current article
    if (g_parsed_header.parsed_art != art_num)
    {
        (void) fetch_cache(art_num, which_line, FILL_CACHE);
    }
}

std::string prefetch_lines_copy(ArticleNum art_num, HeaderLineType which_line)
{
    if ((g_data_source->m_flags & DF_REMOTE) && g_parsed_header.parsed_art != art_num)
    {
        const std::string cached_line = fetch_cache(art_num, which_line, DONT_FILL_CACHE);
        if (!cached_line.empty())
        {
            return cached_line;
        }
        if (Article *ap = article_find(art_num); ap == nullptr || !(ap->m_flags & AF_EXISTS))
        {
            return {};
        }

        std::string result;
        prefetch_remote_lines(art_num, which_line, &result);
        return result;
    }

    return fetch_lines(art_num, which_line);
}
