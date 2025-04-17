/* rt-ov.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/rt-ov.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/rt-process.h"
#include "trn/rt-util.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <parsedate/parsedate.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

/* How many overview lines to read with one NNTP call */
enum
{
    OV_CHUNK_SIZE = 40
};

static HeaderLineType s_hdrnum[] = {
    PAST_HEADER, SUBJ_LINE, FROM_LINE, DATE_LINE, MSG_ID_LINE,
    REFS_LINE, BYTES_LINE, LINES_LINE, XREF_LINE
};

static void ov_parse(char *line, ArticleNum artnum, bool remote);
static const char *ov_name(const char *group);
static OverviewFieldNum ov_num(char *hdr, char *end);

bool ov_init()
{
    bool has_overview_fmt;
    OverviewFieldNum *fieldnum = g_data_source->field_num;
    FieldFlags  *fieldflags = g_data_source->field_flags;
    g_data_source->flags &= ~DF_TRY_OVERVIEW;
    std::FILE *overview;
    if (!g_data_source->over_dir)
    {
        /* Check if the server is XOVER compliant */
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
        /* Just in case... */
        if (*g_ser_line == NNTP_CLASS_OK)
        {
            nntp_finish_list();
        }
        int ret = nntp_list("overview.fmt", "", 0);
        if (ret < -1)
        {
            return false;
        }
        has_overview_fmt = ret > 0;
    }
    else
    {
        has_overview_fmt = g_data_source->over_fmt != nullptr
                        && (overview = std::fopen(g_data_source->over_fmt, "r")) != nullptr;
    }

    if (has_overview_fmt)
    {
        int i;
        fieldnum[0] = OV_NUM;
        fieldflags[OV_NUM] = FF_HAS_FIELD;
        for (i = 1;;)
        {
            if (!g_data_source->over_dir)
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
                char *s = std::strchr(g_buf,':');
                fieldnum[i] = ov_num(g_buf,s);
                fieldflags[fieldnum[i]] = FF_HAS_FIELD |
                    ((s && string_case_equal("full", s+1,4))? FF_HAS_HDR : FF_NONE);
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
    g_data_source->flags |= DF_TRY_OVERVIEW;
    return true;
}

OverviewFieldNum ov_num(char *hdr, char *end)
{
    if (!end)
    {
        end = hdr + std::strlen(hdr);
    }

    switch (set_line_type(hdr, end))
    {
    case SUBJ_LINE:
        return OV_SUBJ;

    case AUTHOR_LINE:         /* This hack is for the Baen NNTP server */
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

/* Process the data in the group's news-overview file.
*/
bool ov_data(ArticleNum first, ArticleNum last, bool cheating)
{
    ArticleNum  artnum;
    ArticleNum  an;
    char    * line;
    char* last_buf = g_buf;
    MemorySize last_buflen = LBUFLEN;
    bool success = true;
    ArticleNum real_first = first;
    ArticleNum real_last = last;
    int line_cnt;
    int ov_chunk_size = cheating? OV_CHUNK_SIZE : OV_CHUNK_SIZE * 8;
    std::time_t started_request;
    bool remote = !g_data_source->over_dir;

beginning:
    while (true)
    {
        artnum = article_first(first);
        if (artnum > first || !(article_ptr(artnum)->flags & AF_CACHED))
        {
            break;
        }
        g_spin_todo--;
        first++;
    }
    if (first > last)
    {
        goto exit;
    }
    if (remote)
    {
        if (last - first > ov_chunk_size + ov_chunk_size / 2 - 1)
        {
            last = first + ov_chunk_size - 1;
            line_cnt = 0;
        }
    }
    started_request = std::time(nullptr);
    while (true)
    {
        artnum = article_last(last);
        if (artnum < last || !(article_ptr(artnum)->flags & AF_CACHED))
        {
            break;
        }
        g_spin_todo--;
        last--;
    }

    if (remote)
    {
        std::sprintf(g_ser_line, "XOVER %ld-%ld", (long)first, (long)last);
        if (nntp_command(g_ser_line) <= 0 || nntp_check() <= 0)
        {
            success = false;
            goto exit;
        }
        if (g_verbose && !g_first_subject && !g_data_source->ov_opened)
        {
            std::printf("\nGetting overview file.");
            std::fflush(stdout);
        }
    }
    else if (g_data_source->ov_opened < started_request - 60 * 60)
    {
        ov_close();
        g_data_source->ov_in = std::fopen(ov_name(g_newsgroup_name.c_str()), "r");
        if (g_data_source->ov_in == nullptr)
        {
            return false;
        }
        if (g_verbose && !g_first_subject)
        {
            std::printf("\nReading overview file.");
            std::fflush(stdout);
        }
    }
    if (!g_data_source->ov_opened)
    {
        if (cheating)
        {
            set_spin(SPIN_BACKGROUND);
        }
        else
        {
            int lots2do = ((g_data_source->flags & DF_REMOTE)? g_net_speed : 20) * 100;
            g_spin_estimate = std::min(g_spin_estimate, g_spin_todo);
            set_spin(g_spin_estimate > lots2do? SPIN_BAR_GRAPH : SPIN_FOREGROUND);
        }
        g_data_source->ov_opened = started_request;
    }

    artnum = first-1;
    while (true)
    {
        if (remote)
        {
            line = nntp_get_a_line(last_buf,last_buflen,last_buf!=g_buf);
            if (nntp_at_list_end(line))
            {
                break;
            }
            line_cnt++;
        }
        else if (!(line = get_a_line(last_buf, last_buflen, last_buf != g_buf, g_data_source->ov_in)))
        {
            break;
        }

        last_buf = line;
        last_buflen = g_buf_len_last_line_got;
        an = std::atol(line);
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
        g_spin_todo -= an - artnum - 1;
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
            if (g_curr_artp != g_sentinel_artp)
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
        if (an > 0)
        {
            last = an - 1;
            g_spin_todo -= last - artnum;
            artnum = last;
        }
    }
    if (remote)
    {
        int cachemask = (g_threaded_group? AF_THREADED : AF_CACHED);
        for (Article *ap = article_ptr(article_first(real_first));
             ap && article_num(ap) <= artnum;
             ap = article_nextp(ap))
        {
            if (!(ap->flags & cachemask))
            {
                one_missing(ap);
            }
        }
        g_spin_todo -= last - artnum;
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
        if (cheating && g_curr_artp != g_sentinel_artp)
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
                first = last+1;
                last = real_last;
                goto beginning;
            }
            success = false;
        }
    }
    if (!cheating && g_data_source->ov_in)
    {
        std::fseek(g_data_source->ov_in, 0L, 0); /* rewind it for the cheating phase */
    }
    if (success && real_first <= g_first_cached)
    {
        g_first_cached = real_first;
        g_cached_all_in_range = true;
    }
    set_spin(SPIN_POP);
    if (last_buf != g_buf)
    {
        std::free(last_buf);
    }
    return success;
}

static void ov_parse(char *line, ArticleNum artnum, bool remote)
{
    OverviewFieldNum *fieldnum = g_data_source->field_num;
    FieldFlags  *fieldflags = g_data_source->field_flags;
    char         *fields[OV_MAX_FIELDS];
    char         *tab;

    Article *article = article_ptr(artnum);
    if (article->flags & AF_THREADED)
    {
        g_spin_todo--;
        return;
    }

    if (g_len_last_line_got > 0 && line[g_len_last_line_got - 1] == '\n')
    {
        if (g_len_last_line_got > 1 && line[g_len_last_line_got-2] == '\r')
        {
            line[g_len_last_line_got - 2] = '\0';
        }
        else
        {
            line[g_len_last_line_got - 1] = '\0';
        }
    }
    char *cp = line;

    std::memset(fields,0,sizeof fields);
    for (int i = 0; cp && i < OV_MAX_FIELDS; cp = tab)
    {
        tab = std::strchr(cp, '\t');
        if (tab != nullptr)
        {
            *tab++ = '\0';
        }
        int fn = fieldnum[i];
        if (!(fieldflags[fn] & (FF_HAS_FIELD | FF_CHECK_FOR_FIELD)))
        {
            break;
        }
        if (fieldflags[fn] & (FF_HAS_HDR | FF_CHECK_FOR_HEADER))
        {
            char* s = std::strchr(cp, ':');
            if (fieldflags[fn] & FF_CHECK_FOR_HEADER)
            {
                if (s)
                {
                    fieldflags[fn] |= FF_HAS_HDR;
                }
                fieldflags[fn] &= ~FF_CHECK_FOR_HEADER;
            }
            if (fieldflags[fn] & FF_HAS_HDR)
            {
                if (!s)
                {
                    break;
                }
                if (s - cp != g_htype[s_hdrnum[fn]].length
                 || string_case_compare(cp,g_htype[s_hdrnum[fn]].name,g_htype[s_hdrnum[fn]].length))
                {
                    continue;
                }
                cp = s;
                cp = skip_eq(++cp, ' ');
            }
        }
        fields[fn] = cp;
        i++;
    }
    if (!fields[OV_SUBJ] || !fields[OV_MSG_ID]
     || !fields[OV_FROM] || !fields[OV_DATE])
    {
        return;         /* skip this line if it's too short */
    }

    if (!article->subj)
    {
        set_subj_line(article, fields[OV_SUBJ], std::strlen(fields[OV_SUBJ]));
    }
    if (!article->msg_id)
    {
        set_cached_line(article, MSG_ID_LINE, save_str(fields[OV_MSG_ID]));
    }
    if (!article->from)
    {
        set_cached_line(article, FROM_LINE, save_str(fields[OV_FROM]));
    }
    if (!article->date)
    {
        article->date = parsedate(fields[OV_DATE]);
    }
    if (!article->bytes && fields[OV_BYTES])
    {
        set_cached_line(article, BYTES_LINE, fields[OV_BYTES]);
    }
    if (!article->lines && fields[OV_LINES])
    {
        set_cached_line(article, LINES_LINE, fields[OV_LINES]);
    }

    if (fieldflags[OV_XREF] & (FF_HAS_FIELD | FF_CHECK_FOR_FIELD))
    {
        if (!article->xrefs && fields[OV_XREF])
        {
            /* Exclude an xref for just this group */
            cp = std::strchr(fields[OV_XREF], ':');
            if (cp && std::strchr(cp+1, ':'))
            {
                article->xrefs = save_str(fields[OV_XREF]);
            }
        }

        if (fieldflags[OV_XREF] & FF_HAS_FIELD)
        {
            if (!article->xrefs)
            {
                article->xrefs = "";
            }
        }
        else if (fields[OV_XREF])
        {
            for (ArticleNum an = article_first(g_abs_first); an < artnum; an = article_next(an))
            {
                Article *ap = article_ptr(an);
                if (!ap->xrefs)
                {
                    ap->xrefs = "";
                }
            }
            fieldflags[OV_XREF] |= FF_HAS_FIELD;
        }
    }

    if (remote)
    {
        article->flags |= AF_EXISTS;
    }

    if (g_threaded_group)
    {
        if (valid_article(article))
        {
            thread_article(article, fields[OV_REFS]);
        }
    }
    else if (!(article->flags & AF_CACHED))
    {
        cache_article(article);
    }

    if (article->flags & AF_UNREAD)
    {
        check_poster(article);
    }
    spin(100);
}

/* Change a newsgroup name into the name of the overview data file.  We
** subsitute any '.'s in the group name into '/'s, prepend the path, and
** append the '/.overview' or '.ov') on to the end.
*/
static const char *ov_name(const char *group)
{
    std::strcpy(g_buf, g_data_source->over_dir);
    char *cp = g_buf + std::strlen(g_buf);
    *cp++ = '/';
    std::strcpy(cp, group);
    while ((cp = std::strchr(cp, '.')))
    {
        *cp = '/';
    }
    std::strcat(g_buf, OV_FILE_NAME);
    return g_buf;
}

void ov_close()
{
    if (g_data_source && g_data_source->ov_opened)
    {
        if (g_data_source->ov_in)
        {
            (void) std::fclose(g_data_source->ov_in);
            g_data_source->ov_in = nullptr;
        }
        g_data_source->ov_opened = 0;
    }
}

const char *ov_field_name(int num)
{
    return g_htype[s_hdrnum[num]].name;
}

const char *ov_field(Article *ap, int num)
{
    OverviewFieldNum fn = g_data_source->field_num[num];
    if (!(g_data_source->field_flags[fn] & (FF_HAS_FIELD | FF_CHECK_FOR_FIELD)))
    {
        return nullptr;
    }

    if (fn == OV_NUM)
    {
        std::sprintf(g_cmd_buf, "%ld", (long)ap->num);
        return g_cmd_buf;
    }

    if (fn == OV_DATE)
    {
        std::sprintf(g_cmd_buf, "%ld", (long)ap->date);
        return g_cmd_buf;
    }

    const char *s = get_cached_line(ap, s_hdrnum[fn], true);
    return s ? s : "";
}
