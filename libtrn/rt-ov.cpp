/* rt-ov.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-ov.h>

#include <config/common.h>
#include <config/string_case_compare.h>
#include <nntp/nntpclient.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/rt-process.h>
#include <trn/rt-util.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <parsedate/parsedate.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>

// How many overview lines to read with one NNTP call
enum
{
    OV_CHUNK_SIZE = 40
};

static HeaderLineType s_header_num[] = {
    PAST_HEADER, SUBJ_LINE, FROM_LINE, DATE_LINE, MSG_ID_LINE,
    REFS_LINE, BYTES_LINE, LINES_LINE, XREF_LINE
};

static void             ov_parse(std::string_view line, ArticleNum artnum, bool remote);
static std::string      ov_name(std::string_view group);
static OverviewFieldNum ov_num(std::string_view header_name);
static const char      *ov_field_name(int num);

bool ov_init()
{
    bool has_overview_fmt;
    OverviewFieldNum *fieldnum = g_data_source->m_field_num;
    FieldFlags  *fieldflags = g_data_source->m_field_flags;
    g_data_source->m_flags &= ~DF_TRY_OVERVIEW;
    std::FILE *overview;
    if (g_data_source->m_over_dir.empty())
    {
        // Check if the server is XOVER compliant
        if (nntp_command("XOVER") <= 0)
        {
            return false;
        }
        if (nntp_check() < 0)
        {
            return false;
        }
        if (std::atoi(g_ser_line) == NNTP_BAD_COMMAND_VAL)
        {
            return false;
        }
        // Just in case...
        if (*g_ser_line == NNTP_CLASS_OK)
        {
            nntp_finish_list();
        }
        int ret = nntp_list("overview.fmt", "");
        if (ret < -1)
        {
            return false;
        }
        has_overview_fmt = ret > 0;
    }
    else
    {
        has_overview_fmt = !g_data_source->m_over_fmt.empty() &&
                           (overview = std::fopen(g_data_source->m_over_fmt.c_str(), "r")) != nullptr;
    }

    if (has_overview_fmt)
    {
        int i;
        fieldnum[0] = OV_NUM;
        fieldflags[OV_NUM] = FF_HAS_FIELD;
        for (i = 1;;)
        {
            if (g_data_source->m_over_dir.empty())
            {
                if (nntp_gets(g_buf, sizeof g_buf) == NGSR_ERROR)
                {
                    break;
                }
                if (nntp_at_list_end(g_buf))
                {
                    break;
                }
            }
            else if (!std::fgets(g_buf, sizeof g_buf, overview))
            {
                std::fclose(overview);
                break;
            }
            if (*g_buf == '#')
            {
                continue;
            }
            if (i < OV_MAX_FIELDS)
            {
                const std::string_view line{g_buf};
                const std::size_t      colon = line.find(':');
                const OverviewFieldNum field = ov_num(line.substr(0, colon));
                fieldnum[i] = field;
                fieldflags[field] = FF_HAS_FIELD | ((colon != std::string_view::npos && line.size() - colon > 4 &&
                                                     string_case_equal(line.substr(colon + 1, 4), "full"))
                                                        ? FF_HAS_HDR
                                                        : FF_NONE);
                i++;
            }
        }
        if (!fieldflags[OV_SUBJ] || !fieldflags[OV_MSG_ID]
         || !fieldflags[OV_FROM] || !fieldflags[OV_DATE])
        {
            return false;
        }
        if (i < OV_MAX_FIELDS)
        {
            int j;
            for (j = OV_MAX_FIELDS; j--;)
            {
                if (!fieldflags[j])
                {
                    break;
                }
            }
            while (i < OV_MAX_FIELDS)
            {
                fieldnum[i++] = static_cast<OverviewFieldNum>(j);
            }
        }
    }
    else
    {
        for (int i = 0; i < OV_MAX_FIELDS; i++)
        {
            fieldnum[i] = static_cast<OverviewFieldNum>(i);
            fieldflags[i] = FF_HAS_FIELD;
        }
        fieldflags[OV_XREF] = FF_CHECK_FOR_FIELD | FF_CHECK_FOR_HEADER;
    }
    g_data_source->m_flags |= DF_TRY_OVERVIEW;
    return true;
}

OverviewFieldNum ov_num(std::string_view header_name)
{
    switch (set_line_type(header_name))
    {
    case SUBJ_LINE:
        return OV_SUBJ;

    case AUTHOR_LINE:         // This hack is for the Baen NNTP server
    case FROM_LINE:
        return OV_FROM;

    case DATE_LINE:
        return OV_DATE;

    case MSG_ID_LINE:
        return OV_MSG_ID;

    case REFS_LINE:
        return OV_REFS;

    case BYTES_LINE:
        return OV_BYTES;

    case LINES_LINE:
        return OV_LINES;

    case XREF_LINE:
        return OV_XREF;
    }
    return OV_NUM;
}

// Process the data in the group's news-overview file.
bool ov_data(ArticleNum first, ArticleNum last, bool cheating)
{
    ArticleNum  artnum;
    ArticleNum  an;
    std::string line;
    bool success = true;
    ArticleNum real_first = first;
    ArticleNum real_last = last;
    int line_cnt;
    int ov_chunk_size = cheating? OV_CHUNK_SIZE : OV_CHUNK_SIZE * 8;
    std::time_t started_request;
    bool        remote = g_data_source->m_over_dir.empty();

beginning:
    while (true)
    {
        artnum = article_first(first);
        if (artnum > first || !(article_ptr(artnum)->m_flags & AF_CACHED))
        {
            break;
        }
        g_spin_todo--;
        ++first;
    }
    if (first > last)
    {
        goto exit;
    }
    if (remote)
    {
        if ((last - first).value_of() > ov_chunk_size + ov_chunk_size / 2 - 1)
        {
            last = first + ArticleNum{ov_chunk_size - 1};
            line_cnt = 0;
        }
    }
    started_request = std::time(nullptr);
    while (true)
    {
        artnum = article_last(last);
        if (artnum < last || !(article_ptr(artnum)->m_flags & AF_CACHED))
        {
            break;
        }
        g_spin_todo--;
        --last;
    }

    if (remote)
    {
        std::sprintf(g_ser_line, "XOVER %ld-%ld", (long)first.value_of(), (long)last.value_of());
        if (nntp_command(g_ser_line) <= 0 || nntp_check() <= 0)
        {
            success = false;
            goto exit;
        }
        if (g_verbose && !g_first_subject && !g_data_source->m_ov_opened)
        {
            std::printf("\nGetting overview file.");
            std::fflush(stdout);
        }
    }
    else if (g_data_source->m_ov_opened < started_request - 60 * 60)
    {
        ov_close();
        g_data_source->m_ov_in = std::fopen(ov_name(g_newsgroup_name).c_str(), "r");
        if (g_data_source->m_ov_in == nullptr)
        {
            return false;
        }
        if (g_verbose && !g_first_subject)
        {
            std::printf("\nReading overview file.");
            std::fflush(stdout);
        }
    }
    if (!g_data_source->m_ov_opened)
    {
        if (cheating)
        {
            set_spin(SPIN_BACKGROUND);
        }
        else
        {
            int lots2do = ((g_data_source->m_flags & DF_REMOTE)? g_net_speed : 20) * 100;
            g_spin_estimate = std::min(g_spin_estimate, g_spin_todo);
            set_spin(g_spin_estimate > lots2do? SPIN_BAR_GRAPH : SPIN_FOREGROUND);
        }
        g_data_source->m_ov_opened = started_request;
    }

    artnum = article_before(first);
    while (true)
    {
        if (remote)
        {
            line = nntp_get_a_line();
            if (nntp_at_list_end(line.c_str()))
            {
                break;
            }
            line_cnt++;
        }
        else if ((line = get_a_line(g_data_source->m_ov_in)).empty())
        {
            break;
        }

        an = ArticleNum{std::atol(line.c_str())};
        if (an < first)
        {
            continue;
        }
        if (an > last)
        {
            artnum = last;
            if (remote)
            {
                continue;
            }
            break;
        }
        g_spin_todo -= (an - artnum).value_of() - 1;
        ov_parse(line, artnum = an, remote);
        if (g_int_count)
        {
            g_int_count = 0;
            success = false;
            if (!remote)
            {
                break;
            }
        }
        if (!remote && cheating)
        {
            if (input_pending())
            {
                success = false;
                break;
            }
            if (g_curr_artp != g_sentinel_art_ptr)
            {
                push_char('\f' | 0200);
                success = false;
                break;
            }
        }
    }
    if (remote && line_cnt == 0 && last < real_last)
    {
        an = nntp_find_real_art(last);
        if (an.value_of() > 0)
        {
            last = article_before(an);
            g_spin_todo -= (last - artnum).value_of();
            artnum = last;
        }
    }
    if (remote)
    {
        int cachemask = (g_threaded_group? AF_THREADED : AF_CACHED);
        for (Article *ap = article_ptr(article_first(real_first));
             ap && ap->article_num() <= artnum;
             ap = article_nextp(ap))
        {
            if (!(ap->m_flags & cachemask))
            {
                ap->one_missing();
            }
        }
        g_spin_todo -= (last - artnum).value_of();
    }
    if (artnum > g_last_cached && artnum >= first)
    {
        g_last_cached = artnum;
    }
exit:
    if (g_int_count || !success)
    {
        g_int_count = 0;
        success = false;
    }
    else if (remote)
    {
        if (cheating && g_curr_artp != g_sentinel_art_ptr)
        {
            push_char('\f' | 0200);
            success = false;
        }
        else if (last < real_last)
        {
            if (!cheating || !input_pending())
            {
                long elapsed_time = std::time(nullptr) - started_request;
                long expected_time = cheating? 2 : 10;
                int max_chunk_size = cheating? 500 : 2000;
                ov_chunk_size += (expected_time - elapsed_time) * OV_CHUNK_SIZE;
                if (ov_chunk_size <= OV_CHUNK_SIZE / 2)
                {
                    ov_chunk_size = OV_CHUNK_SIZE / 2 + 1;
                }
                else if (ov_chunk_size > max_chunk_size)
                {
                    ov_chunk_size = max_chunk_size;
                }
                first = article_after(last);
                last = real_last;
                goto beginning;
            }
            success = false;
        }
    }
    if (!cheating && g_data_source->m_ov_in)
    {
        std::fseek(g_data_source->m_ov_in, 0L, 0); // rewind it for the cheating phase
    }
    if (success && real_first <= g_first_cached)
    {
        g_first_cached = real_first;
        g_cached_all_in_range = true;
    }
    set_spin(SPIN_POP);
    return success;
}

static void ov_parse(std::string_view line, ArticleNum artnum, bool remote)
{
    OverviewFieldNum *fieldnum = g_data_source->m_field_num;
    FieldFlags  *fieldflags = g_data_source->m_field_flags;
    std::array<std::string_view, OV_MAX_FIELDS> fields{};
    std::array<bool, OV_MAX_FIELDS>             field_present{};

    Article *article = article_ptr(artnum);
    if (article->m_flags & AF_THREADED)
    {
        g_spin_todo--;
        return;
    }

    if (!line.empty() && line.back() == '\n')
    {
        line.remove_suffix(1);
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
    }

    std::size_t field_start = 0;
    for (int i = 0; i < OV_MAX_FIELDS && field_start <= line.size();)
    {
        const std::size_t tab = line.find('\t', field_start);
        std::string_view  field =
            line.substr(field_start, tab == std::string_view::npos ? std::string_view::npos : tab - field_start);
        int fn = fieldnum[i];
        if (!(fieldflags[fn] & (FF_HAS_FIELD | FF_CHECK_FOR_FIELD)))
        {
            break;
        }
        if (fieldflags[fn] & (FF_HAS_HDR | FF_CHECK_FOR_HEADER))
        {
            const std::size_t colon = field.find(':');
            if (fieldflags[fn] & FF_CHECK_FOR_HEADER)
            {
                if (colon != std::string_view::npos)
                {
                    fieldflags[fn] |= FF_HAS_HDR;
                }
                fieldflags[fn] &= ~FF_CHECK_FOR_HEADER;
            }
            if (fieldflags[fn] & FF_HAS_HDR)
            {
                if (colon == std::string_view::npos)
                {
                    break;
                }
                const std::string_view header_name = field.substr(0, colon);
                if (string_case_compare(header_name, g_header_type[s_header_num[fn]].name) != 0)
                {
                    if (tab == std::string_view::npos)
                    {
                        break;
                    }
                    field_start = tab + 1;
                    continue;
                }
                field.remove_prefix(colon + 1);
                while (!field.empty() && field.front() == ' ')
                {
                    field.remove_prefix(1);
                }
            }
        }
        fields[fn] = field;
        field_present[fn] = true;
        i++;
        if (tab == std::string_view::npos)
        {
            break;
        }
        field_start = tab + 1;
    }
    if (!field_present[OV_SUBJ] || !field_present[OV_MSG_ID]
     || !field_present[OV_FROM] || !field_present[OV_DATE])
    {
        return;         // skip this line if it's too short
    }

    if (!article->m_subj)
    {
        article->set_subj_line(fields[OV_SUBJ]);
    }
    if (!article->m_msg_id)
    {
        article->set_cached_line(MSG_ID_LINE, fields[OV_MSG_ID]);
    }
    if (!article->m_from)
    {
        article->set_cached_line(FROM_LINE, fields[OV_FROM]);
    }
    if (!article->m_date)
    {
        article->m_date = parsedate(std::string{fields[OV_DATE]}.c_str());
    }
    if (!article->m_bytes && field_present[OV_BYTES])
    {
        article->set_cached_line(BYTES_LINE, fields[OV_BYTES]);
    }
    if (!article->m_lines && field_present[OV_LINES])
    {
        article->set_cached_line(LINES_LINE, fields[OV_LINES]);
    }

    if (fieldflags[OV_XREF] & (FF_HAS_FIELD | FF_CHECK_FOR_FIELD))
    {
        if (!article->m_xrefs && field_present[OV_XREF])
        {
            // Exclude an xref for just this group
            const std::size_t colon = fields[OV_XREF].find(':');
            if (colon != std::string_view::npos && fields[OV_XREF].find(':', colon + 1) != std::string_view::npos)
            {
                article->m_xrefs = std::string{fields[OV_XREF]};
            }
        }

        if (fieldflags[OV_XREF] & FF_HAS_FIELD)
        {
            if (!article->m_xrefs)
            {
                article->m_xrefs = "";
            }
        }
        else if (field_present[OV_XREF])
        {
            for (ArticleNum an = article_first(g_abs_first); an < artnum; an = article_next(an))
            {
                Article *ap = article_ptr(an);
                if (!ap->m_xrefs)
                {
                    ap->m_xrefs = "";
                }
            }
            fieldflags[OV_XREF] |= FF_HAS_FIELD;
        }
    }

    if (remote)
    {
        article->m_flags |= AF_EXISTS;
    }

    if (g_threaded_group)
    {
        if (article->valid_article())
        {
            article->thread_article(field_present[OV_REFS] ? fields[OV_REFS] : std::string_view{});
        }
    }
    else if (!(article->m_flags & AF_CACHED))
    {
        article->cache_article();
    }

    if (article->m_flags & AF_UNREAD)
    {
        article->check_poster();
    }
    spin(100);
}

// Change a newsgroup name into the name of the overview data file.  We
// substitute any '.'s in the group name into '/'s, prepend the path, and
// append the '/.overview' or '.ov') on to the end.
//
static std::string ov_name(std::string_view group)
{
    std::string filename{g_data_source->m_over_dir};
    filename += '/';
    const std::string::size_type group_start = filename.size();
    filename += group;
    for (std::string::size_type i = group_start; i < filename.size(); ++i)
    {
        if (filename[i] == '.')
        {
            filename[i] = '/';
        }
    }
    filename += OV_FILE_NAME;
    return filename;
}

void ov_close()
{
    if (g_data_source && g_data_source->m_ov_opened)
    {
        if (g_data_source->m_ov_in)
        {
            (void) std::fclose(g_data_source->m_ov_in);
            g_data_source->m_ov_in = nullptr;
        }
        g_data_source->m_ov_opened = 0;
    }
}

static const char *ov_field_name(int num)
{
    return g_header_type[s_header_num[num]].name.c_str();
}
