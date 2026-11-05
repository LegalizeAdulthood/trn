/* nntp.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/nntp.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <trn/artio.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <fmt/format.h>

#include <charconv>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

static ArticleNum  nntp_next_art();
static std::string read_art_file_chunk(int limit);

NntpArticleCache g_nntp_art;

int nntp_list(std::string_view type, std::string_view arg)
{
    std::string first_line;
    return nntp_list(type, arg, first_line);
}

int nntp_list(std::string_view type, std::string_view arg, std::string &first_line)
{
    int               ret;
    const std::string type_name{type};
    const bool        is_active = string_case_equal(type, "active");
    first_line.clear();
#ifdef DEBUG
    if (!arg.empty() && (g_debug & 1) && is_active)
    {
        return -1;
    }
#endif
    if (string_case_equal(type, "newsgroups"))
    {
        ret = nntp_group_descriptions(arg);
    }
    else
    {
        std::string command{"LIST"};
        if (!arg.empty())
        {
            fmt::format_to(std::back_inserter(command), " {} {}", type_name, arg);
        }
        else if (!is_active)
        {
            fmt::format_to(std::back_inserter(command), " {}", type_name);
        }
        if (g_nntp.command(command) <= 0)
        {
            return -2;
        }
        ret = g_nntp.check();
    }
    if (ret <= 0)
    {
        return ret ? ret : -1;
    }
    if (arg.empty())
    {
        return 1;
    }
    if (g_nntp.gets(first_line, NNTP_STRLEN) == NGSR_ERROR)
    {
        return -1;
    }
#if defined(DEBUG) && defined(FLUSH)
    if (g_debug & DEB_NNTP)
    {
        fmt::print("<{}\n", first_line);
    }
#endif
    if (g_nntp.at_list_end(first_line))
    {
        return 0;
    }
    return 1;
}

void nntp_finish_list()
{
    NNTPGetsResult ret;
    std::string    line;
    line.reserve(NNTP_STRLEN);
    do
    {
        while ((ret = g_nntp.gets(line, NNTP_STRLEN)) == NGSR_PARTIAL_LINE)
        {
            // A line w/o a newline is too long to be the end of the
            // list, so grab the rest of this line and try again.
            while ((ret = g_nntp.gets(line, NNTP_STRLEN)) == NGSR_PARTIAL_LINE)
            {
            }
            if (ret < 0)
            {
                return;
            }
        }
    } while (ret > 0 && !g_nntp.at_list_end(line));
}

// try to access the specified group

int nntp_group(std::string_view group, NewsgroupData *gp)
{
    const std::string group_name{group};
    const std::string command{fmt::format("GROUP {}", group_name)};
    if (g_nntp.command(command) <= 0)
    {
        return -2;
    }
    switch (g_nntp.check())
    {
    case -2:
        return -2;

    case -1:
    case 0:
    {
        const int ser_int = nntp_response_code(g_nntp.ser_line);
        if (ser_int != NNTP_NOSUCHGROUP_VAL //
            && ser_int != NNTP_SYNTAX_VAL)
        {
            if (ser_int != NNTP_AUTH_NEEDED_VAL && ser_int != NNTP_ACCESS_VAL //
                && ser_int != NNTP_AUTH_REJECT_VAL)
            {
                fmt::print(stderr, "\nServer's response to GROUP {}:\n{}\n", group_name, g_nntp.ser_line);
                return -1;
            }
        }
        return 0;
    }
    }
    if (gp)
    {
        const auto next_token = [](std::string_view &text)
        {
            const std::size_t start = text.find_first_not_of(" \t\r\n");
            if (start == std::string_view::npos)
            {
                text = {};
                return std::string_view{};
            }
            text.remove_prefix(start);
            const std::size_t      end = text.find_first_of(" \t\r\n");
            const std::string_view token = text.substr(0, end);
            text.remove_prefix(end == std::string_view::npos ? text.size() : end);
            return token;
        };
        const auto parse_long = [](std::string_view text, long &value)
        {
            if (text.empty())
            {
                return;
            }
            long                         parsed{};
            const char                  *begin = text.data();
            const char                  *end = begin + text.size();
            const std::from_chars_result result = std::from_chars(begin, end, parsed);
            if (result.ec == std::errc{} && result.ptr == end)
            {
                value = parsed;
            }
        };
        std::string_view response{g_nntp.ser_line};
        long             count{};
        long             first{};
        long             last{};

        (void) next_token(response);
        parse_long(next_token(response), count);
        parse_long(next_token(response), first);
        parse_long(next_token(response), last);
        // NNTP mangles the high/low values when no articles are present.
        if (!count)
        {
            gp->m_abs_first = article_after(gp->m_ng_max);
        }
        else
        {
            gp->m_abs_first = ArticleNum{first};
            gp->m_ng_max = ArticleNum{last};
        }
    }
    return 1;
}

// check on an article's existence

int nntp_stat(ArticleNum art_num)
{
    if (g_nntp.command(fmt::format("STAT {}", art_num.value_of())) <= 0)
    {
        return -2;
    }
    return g_nntp.check();
}

// check on an article's existence by its message id

ArticleNum nntp_stat_id(std::string_view msg_id)
{
    if (g_nntp.command(fmt::format("STAT {}", msg_id)) <= 0)
    {
        return ArticleNum{-2};
    }
    long art_num{g_nntp.check()};
    if (art_num > 0)
    {
        std::istringstream response{g_nntp.ser_line};
        long               status{};
        long               parsed_article_num{};
        art_num = response >> status >> parsed_article_num ? parsed_article_num : 0;
    }
    return ArticleNum{art_num};
}

static ArticleNum nntp_next_art()
{
    long artnum;

    if (g_nntp.command("NEXT") <= 0)
    {
        return ArticleNum{-2};
    }
    artnum = g_nntp.check();
    if (artnum > 0)
    {
        std::istringstream response{g_nntp.ser_line};
        long               status{};
        long               parsed_artnum{};
        artnum = response >> status >> parsed_artnum ? parsed_artnum : 0;
    }
    return ArticleNum{artnum};
}

// prepare to get the header

int nntp_header(ArticleNum art_num)
{
    if (g_nntp.command(fmt::format("HEAD {}", art_num.value_of())) <= 0)
    {
        return -2;
    }
    return g_nntp.check();
}

// copy the body of an article to a temporary file

void NntpArticleCache::body(ArticleNum art_num)
{
    std::string artname = art_name(art_num, false); // Is it already in a tmp file?
    if (!artname.empty())
    {
        if (body_pos >= 0)
        {
            finish_body(FB_DISCARD);
        }
        g_art_input.fp = std::fopen(artname.c_str(), "rb");
        stat_t art_stat{};
        if (g_art_input.fp && fstat(fileno(g_art_input.fp), &art_stat) == 0)
        {
            body_end = ArticlePosition{art_stat.st_size};
        }
        return;
    }

    artname = art_name(art_num, true); // Allocate a tmp file
    if (!(g_art_input.fp = std::fopen(artname.c_str(), "w+b")))
    {
        fmt::print(stderr, "\nUnable to write temporary file: '{}'.\n", artname);
        finalize(1);
    }
#ifdef SET_ARTICLE_TEMP_FILE_PERMISSIONS
    chmod(artname.c_str(), 0600);
#endif
    if (g_nntp.command(
            fmt::format("{} {}", g_parsed_header.parsed_art == art_num ? "BODY" : "ARTICLE", art_num.value_of())) <= 0)
    {
        finalize(1);
    }
    switch (g_nntp.check())
    {
    case -2:
    case -1:
        finalize(1);

    case 0:
        std::fclose(g_art_input.fp);
        g_art_input.fp = nullptr;
        errno = ENOENT; // Simulate file-not-found
        return;
    }
    body_pos = ArticlePosition{};
    if (g_parsed_header.parsed_art == art_num)
    {
        std::fwrite(g_parsed_header.head_buf.data(), 1, g_parsed_header.head_buf.size(), g_art_input.fp);
        body_end = ftell_art();
        g_parsed_header.positions[PAST_HEADER].min_pos = body_end;
    }
    else
    {
        std::string line;
        line.reserve(NNTP_STRLEN);
        body_end = ArticlePosition{};
        ArticlePosition prev_pos{};
        while (copy_body(line, NNTP_STRLEN, body_end + ArticlePosition{1}) > 0)
        {
            if (!line.empty() && line[0] == '\n' && (body_end - prev_pos).value_of() < NNTP_STRLEN)
            {
                break;
            }
            prev_pos = body_end;
        }
    }
    fseek(g_art_input.fp, 0L, 0);
    g_nntp.link.flags &= ~NNTP_NEW_CMD_OK;
}

ArticlePosition NntpArticleCache::art_size()
{
    return body_pos < 0 ? body_end : ArticlePosition{-1};
}

int NntpArticleCache::copy_body(std::string &line, int limit, ArticlePosition pos)
{
    bool had_nl = true;
    line.reserve(static_cast<std::size_t>(limit));

    while (pos > body_end || !had_nl)
    {
        const NNTPGetsResult result = g_nntp.gets(line, limit);
        if (result == NGSR_ERROR)
        {
            line = ".";
        }
        if (had_nl)
        {
            if (g_nntp.at_list_end(line))
            {
                std::fseek(g_art_input.fp, (long) body_pos.value_of(), 0);
                body_pos = ArticlePosition{-1};
                return 0;
            }
            if (!line.empty() && line[0] == '.')
            {
                line.erase(0, 1);
            }
        }
        if (result == NGSR_FULL_LINE)
        {
            line += '\n';
        }
        std::fwrite(line.data(), 1, line.size(), g_art_input.fp);
        body_end = ftell_art();
        had_nl = result == NGSR_FULL_LINE;
    }
    return 1;
}

int NntpArticleCache::finish_body(FinishBodyMode bmode)
{
    std::string line;
    line.reserve(NNTP_STRLEN);
    if (body_pos < 0)
    {
        return 0;
    }
    if (bmode == FB_DISCARD)
    {
    }
    else if (bmode == FB_OUTPUT)
    {
        if (g_verbose)
        {
            fmt::print("Receiving the rest of the article...");
        }
        else
        {
            fmt::print("Receiving...");
        }
        std::fflush(stdout);
    }
    if (body_end != body_pos)
    {
        std::fseek(g_art_input.fp, body_end.value_of(), 0);
    }
    if (bmode != FB_BACKGROUND)
    {
        copy_body(line, NNTP_STRLEN, ArticlePosition{0x7fffffffL});
    }
    else
    {
        while (copy_body(line, NNTP_STRLEN, body_end + ArticlePosition{1}))
        {
            if (input_pending())
            {
                break;
            }
        }
        if (body_pos >= 0)
        {
            std::fseek(g_art_input.fp, body_pos.value_of(), 0);
        }
    }
    if (bmode == FB_OUTPUT)
    {
        erase_line(false); // erase the prompt
    }
    return 1;
}

int NntpArticleCache::seek_art(ArticlePosition pos)
{
    if (body_pos >= 0)
    {
        if (body_end < pos)
        {
            std::string line;
            line.reserve(NNTP_STRLEN);
            std::fseek(g_art_input.fp, body_end.value_of(), 0);
            copy_body(line, NNTP_STRLEN, pos);
            if (body_pos >= 0)
            {
                body_pos = pos;
            }
        }
        else
        {
            body_pos = pos;
        }
    }
    return std::fseek(g_art_input.fp, pos.value_of(), 0);
}

ArticlePosition NntpArticleCache::tell_art()
{
    return body_pos < 0 ? ftell_art() : body_pos;
}

static std::string read_art_file_chunk(int limit)
{
    if (limit <= 1)
    {
        return {};
    }

    std::string line(static_cast<std::size_t>(limit), '\0');
    if (std::fgets(line.data(), limit, g_art_input.fp) == nullptr)
    {
        return {};
    }
    const std::size_t terminator = line.find('\0');
    line.resize(terminator == std::string::npos ? line.size() : terminator);
    return line;
}

std::string NntpArticleCache::read_art(int limit)
{
    if (limit <= 1)
    {
        return {};
    }

    if (body_pos >= 0)
    {
        if (body_pos == body_end)
        {
            std::string line;
            line.reserve(static_cast<std::size_t>(limit));
            if (copy_body(line, limit, body_pos + ArticlePosition{1}) <= 0)
            {
                return {};
            }
            if (body_end - body_pos < ArticlePosition{limit})
            {
                body_pos = body_end;
                return line;
            }
            std::fseek(g_art_input.fp, body_pos.value_of(), 0);
        }
        std::string line = read_art_file_chunk(limit);
        if (line.empty())
        {
            return {};
        }
        body_pos = ftell_art();
        if (body_pos == body_end)
        {
            std::fseek(g_art_input.fp, body_pos.value_of(), 0); // Prepare for coming write
        }
        return line;
    }
    return read_art_file_chunk(limit);
}

// This is a 1-relative list
static int s_maxdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

std::time_t nntp_time()
{
    if (g_nntp.command("DATE") <= 0)
    {
        return -2;
    }
    if (g_nntp.check() <= 0)
    {
        return std::time(nullptr);
    }

    const std::string_view response{g_nntp.ser_line};
    const std::size_t      date_start = response.find_last_of(' ');
    const std::string_view date_text =
        date_start == std::string_view::npos ? response : response.substr(date_start + 1);
    if (date_text.size() < 14)
    {
        return std::time(nullptr);
    }
    const auto parse_int = [](std::string_view text)
    {
        int                          result{};
        const char                  *begin = text.data();
        const char                  *end = begin + text.size();
        const std::from_chars_result parse_result = std::from_chars(begin, end, result);
        return parse_result.ec == std::errc{} ? result : 0;
    };
    int         month = parse_int(date_text.substr(4, 2));
    int         day = parse_int(date_text.substr(6, 2));
    const int   hh = parse_int(date_text.substr(8, 2));
    const int   mm = parse_int(date_text.substr(10, 2));
    std::time_t ss = parse_int(date_text.substr(12, 2));
    const int   year = parse_int(date_text.substr(0, 4));

    // This simple algorithm will be valid until the year 2100
    if (year % 4)
    {
        s_maxdays[2] = 28;
    }
    else
    {
        s_maxdays[2] = 29;
    }
    if (month < 1 || month > 12 || day < 1 || day > s_maxdays[month]
     || hh < 0 || hh > 23 || mm < 0 || mm > 59
     || ss < 0 || ss > 59)
    {
        return std::time(nullptr);
    }

    for (month--; month; month--)
    {
        day += s_maxdays[month];
    }

    ss = ((((year-1970) * 365 + (year-1969)/4 + day - 1) * 24L + hh) * 60
          + mm) * 60 + ss;

    return ss;
}

int nntp_new_groups(std::time_t t)
{
    std::tm *ts = std::gmtime(&t);
    if (g_nntp.command(fmt::format("NEWGROUPS {:02}{:02}{:02} {:02}{:02}{:02} GMT", ts->tm_year % 100, ts->tm_mon + 1,
                                   ts->tm_mday, ts->tm_hour, ts->tm_min, ts->tm_sec)) <= 0)
    {
        return -2;
    }
    return g_nntp.check();
}

int nntp_art_nums()
{
    if (g_data_source->m_flags & DF_NO_LIST_GROUP)
    {
        return 0;
    }
    if (g_nntp.command("LISTGROUP") <= 0)
    {
        return -2;
    }
    if (g_nntp.check() <= 0)
    {
        g_data_source->m_flags |= DF_NO_LIST_GROUP;
        return 0;
    }
    return 1;
}

ArticleNum nntp_find_real_art(ArticleNum after)
{
    ArticleNum an;

    if (g_cache.last_cached > after || g_cache.last_cached < g_abs_first //
        || nntp_stat(g_cache.last_cached) <= 0)
    {
        if (nntp_stat_id("") > after)
        {
            return {};
        }
    }

    while ((an = nntp_next_art()) > 0)
    {
        if (an > after)
        {
            return an;
        }
        if (after - an > 10)
        {
            break;
        }
    }

    return {};
}

std::string NntpArticleCache::art_name(ArticleNum art_num, bool allocate)
{
    std::time_t         lowage;

    if (!art_num)
    {
        for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
        {
            art_nums[i] = ArticleNum{};
            art_ages[i] = 0;
        }
        return {};
    }

    std::time_t now = std::time(nullptr);

    int j = 0;
    lowage = now;
    for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
    {
        if (art_nums[i] == art_num)
        {
            art_ages[i] = now;
            return nntp_tmp_name(i);
        }
        if (art_ages[i] <= lowage)
        {
            j = i;
            lowage = art_ages[j];
        }
    }

    if (allocate)
    {
        art_nums[j] = art_num;
        art_ages[j] = now;
        return nntp_tmp_name(j);
    }

    return {};
}

std::string nntp_tmp_name(int ndx)
{
    return fmt::format("rrn.{}.{}", g_our_pid, ndx);
}

int nntp_handle_nested_lists()
{
    if (string_case_equal(g_nntp.last_command, "quit"))
    {
        return 0; // TODO: flush data needed?
    }
    if (g_nntp_art.finish_body(FB_DISCARD))
    {
        return 1;
    }
    fmt::print(stderr, "Programming error! Nested NNTP calls detected.\n");
    return -1;
}

int nntp_handle_timeout()
{
    static bool handling_timeout = false;

    if (string_case_equal(g_nntp.last_command, "quit"))
    {
        return 0;
    }
    if (handling_timeout)
    {
        return -1;
    }
    handling_timeout = true;
    const std::string last_command_save{g_nntp.last_command};
    g_nntp.close(false);
    g_data_source->m_nntp_link = g_nntp.link;
    if (nntp_connect(g_data_source->m_news_id, false) <= 0)
    {
        return -2;
    }
    g_data_source->m_nntp_link = g_nntp.link;
    if (g_in_ng && nntp_group(g_newsgroup_name.c_str(), (NewsgroupData*)nullptr) <= 0)
    {
        return -2;
    }
    if (g_nntp.command(last_command_save) <= 0)
    {
        return -1;
    }
    g_nntp.last_command = last_command_save; // TODO: Is this really needed?
    handling_timeout = false;
    return 1;
}

void DataSource::nntp_server_died()
{
    Multirc *mp = g_multirc;
    close();
    m_flags |= DF_UNAVAILABLE;
    unuse_multirc(mp);
    if (!mp->use_multirc())
    {
        g_multirc = nullptr;
    }
    fmt::print(stderr, "\n{}\n", g_nntp.ser_line);
    get_anything();
}
