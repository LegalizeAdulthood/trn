/* nntp.c
*/
// This software is copyrighted as detailed in the LICENSE file.

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/nntp.h"

#include "nntp/nntpclient.h"
#include "trn/ngdata.h"
#include "trn/artio.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/head.h"
#include "trn/init.h"
#include "trn/rcstuff.h"
#include "trn/terminal.h"
#include "trn/trn.h"
#include "util/util2.h"

#include <cstdio>
#include <cstring>
#include <ctime>

static int nntp_copy_body(char *s, int limit, ArticlePosition pos);

static ArticlePosition s_body_pos{-1};
static ArticlePosition s_body_end{};
#ifdef SUPPORT_XTHREAD
static long s_raw_bytes{-1}; // bytes remaining to be transferred
#endif

int nntp_list(const char *type, const char *arg, int len)
{
    int ret;
#ifdef DEBUG
    if (len && (debug & 1) && string_case_equal(type, "active"))
    {
        return -1;
    }
#endif
    if (len)
    {
        std::sprintf(g_ser_line, "LIST %s %.*s", type, len, arg);
    }
    else if (string_case_equal(type, "active"))
    {
        std::strcpy(g_ser_line, "LIST");
    }
    else
    {
        std::sprintf(g_ser_line, "LIST %s", type);
    }
    if (nntp_command(g_ser_line) <= 0)
    {
        return -2;
    }
    ret = nntp_check();
    if (ret <= 0)
    {
        return ret ? ret : -1;
    }
    if (!len)
    {
        return 1;
    }
    if (nntp_gets(g_ser_line, sizeof g_ser_line) == NGSR_ERROR)
    {
        return -1;
    }
#if defined(DEBUG) && defined(FLUSH)
    if (debug & DEB_NNTP)
    {
        std::printf("<%s\n", g_ser_line);
    }
#endif
    if (nntp_at_list_end(g_ser_line))
    {
        return 0;
    }
    return 1;
}

void nntp_finish_list()
{
    NNTPGetsResult ret;
    do
    {
        while ((ret = nntp_gets(g_ser_line, sizeof g_ser_line)) == NGSR_PARTIAL_LINE)
        {
            // A line w/o a newline is too long to be the end of the
            // list, so grab the rest of this line and try again.
            while ((ret = nntp_gets(g_ser_line, sizeof g_ser_line)) == NGSR_PARTIAL_LINE)
            {
            }
            if (ret < 0)
            {
                return;
            }
        }
    } while (ret > 0 && !nntp_at_list_end(g_ser_line));
}

// try to access the specified group

int nntp_group(const char *group, NewsgroupData *gp)
{
    sprintf(g_ser_line, "GROUP %s", group);
    if (nntp_command(g_ser_line) <= 0)
    {
        return -2;
    }
    switch (nntp_check())
    {
    case -2:
        return -2;

    case -1:
    case 0:
    {
        int ser_int = std::atoi(g_ser_line);
          if (ser_int != NNTP_NOSUCHGROUP_VAL //
              && ser_int != NNTP_SYNTAX_VAL)
          {
              if (ser_int != NNTP_AUTH_NEEDED_VAL && ser_int != NNTP_ACCESS_VAL //
                  && ser_int != NNTP_AUTH_REJECT_VAL)
              {
                std::fprintf(stderr, "\nServer's response to GROUP %s:\n%s\n",
                        group, g_ser_line);
                return -1;
            }
        }
        return 0;
    }
    }
    if (gp)
    {
        long count;
        long first;
        long last;

        (void) std::sscanf(g_ser_line,"%*d%ld%ld%ld",&count,&first,&last);
        // NNTP mangles the high/low values when no articles are present.
        if (!count)
        {
            gp->abs_first = article_after(gp->ng_max);
        }
        else
        {
            gp->abs_first = ArticleNum{first};
            gp->ng_max = ArticleNum{last};
        }
    }
    return 1;
}

// check on an article's existence

int nntp_stat(ArticleNum art_num)
{
    std::sprintf(g_ser_line, "STAT %ld", (long)art_num.value_of());
    if (nntp_command(g_ser_line) <= 0)
    {
        return -2;
    }
    return nntp_check();
}

// check on an article's existence by its message id

ArticleNum nntp_stat_id(const char *msg_id)
{
    std::sprintf(g_ser_line, "STAT %s", msg_id);
    if (nntp_command(g_ser_line) <= 0)
    {
        return ArticleNum{-2};
    }
    long art_num{nntp_check()};
    if (art_num > 0 && std::sscanf(g_ser_line, "%*d%ld", &art_num) != 1)
    {
        art_num = 0;
    }
    return ArticleNum{art_num};
}

ArticleNum nntp_next_art()
{
    long artnum;

    if (nntp_command("NEXT") <= 0)
    {
        return ArticleNum{-2};
    }
    artnum = nntp_check();
    if (artnum > 0 && std::sscanf(g_ser_line, "%*d %ld", &artnum) != 1)
    {
        artnum = 0;
    }
    return ArticleNum{artnum};
}

// prepare to get the header

int nntp_header(ArticleNum art_num)
{
    std::sprintf(g_ser_line, "HEAD %ld", (long)art_num.value_of());
    if (nntp_command(g_ser_line) <= 0)
    {
        return -2;
    }
    return nntp_check();
}

// copy the body of an article to a temporary file

void nntp_body(ArticleNum art_num)
{
    char *artname = nntp_art_name(art_num, false); // Is it already in a tmp file?
    if (artname)
    {
        if (s_body_pos >= ArticlePosition{})
        {
            nntp_finish_body(FB_DISCARD);
        }
        g_art_fp = std::fopen(artname, "r");
        stat_t art_stat{};
        if (g_art_fp && fstat(fileno(g_art_fp), &art_stat) == 0)
        {
            s_body_end = ArticlePosition{art_stat.st_size};
        }
        return;
    }

    artname = nntp_art_name(art_num, true); // Allocate a tmp file
    if (!(g_art_fp = std::fopen(artname, "w+")))
    {
        std::fprintf(stderr, "\nUnable to write temporary file: '%s'.\n", artname);
        finalize(1);
    }
#ifndef MSDOS
    chmod(artname, 0600);
#endif
    if (g_parsed_art == art_num)
    {
        std::sprintf(g_ser_line, "BODY %ld", (long) art_num.value_of());
    }
    else
    {
        std::sprintf(g_ser_line, "ARTICLE %ld", (long) art_num.value_of());
    }
    if (nntp_command(g_ser_line) <= 0)
    {
        finalize(1);
    }
    switch (nntp_check())
    {
    case -2:
    case -1:
        finalize(1);

    case 0:
        std::fclose(g_art_fp);
        g_art_fp = nullptr;
        errno = ENOENT; // Simulate file-not-found
        return;
    }
    s_body_pos = ArticlePosition{};
    if (g_parsed_art == art_num)
    {
        std::fwrite(g_head_buf, 1, std::strlen(g_head_buf), g_art_fp);
        s_body_end = (ArticlePosition) std::ftell(g_art_fp);
        g_header_type[PAST_HEADER].min_pos = s_body_end;
    }
    else
    {
        char b[NNTP_STRLEN];
        s_body_end = ArticlePosition{};
        ArticlePosition prev_pos{};
        while (nntp_copy_body(b, sizeof b, s_body_end + ArticlePosition{1}) > 0)
        {
            if (*b == '\n' && (s_body_end - prev_pos).value_of() < sizeof b)
            {
                break;
            }
            prev_pos = s_body_end;
        }
    }
    fseek(g_art_fp, 0L, 0);
    g_nntp_link.flags &= ~NNTP_NEW_CMD_OK;
}

ArticlePosition nntp_art_size()
{
    return s_body_pos < ArticlePosition{} ? s_body_end : ArticlePosition{-1};
}

static int nntp_copy_body(char *s, int limit, ArticlePosition pos)
{
    bool had_nl = true;

    while (pos > s_body_end || !had_nl)
    {
        const int result = nntp_gets(s, limit);
        if (result == NGSR_ERROR)
        {
            std::strcpy(s, ".");
        }
        if (had_nl)
        {
            if (nntp_at_list_end(s))
            {
                std::fseek(g_art_fp, (long)s_body_pos.value_of(), 0);
                s_body_pos = ArticlePosition{-1};
                return 0;
            }
            if (s[0] == '.')
            {
                safe_copy(s, s + 1, limit);
            }
        }
        int len = std::strlen(s);
        if (result == NGSR_FULL_LINE)
        {
            std::strcpy(s + len, "\n");
        }
        std::fputs(s, g_art_fp);
        s_body_end = ArticlePosition{std::ftell(g_art_fp)};
        had_nl = result == NGSR_FULL_LINE;
    }
    return 1;
}

int nntp_finish_body(FinishBodyMode bmode)
{
    char b[NNTP_STRLEN];
    if (s_body_pos < ArticlePosition{})
    {
        return 0;
    }
    if (bmode == FB_DISCARD)
    {
#if 0
        // Implement this if flushing the data becomes possible
        nntp_artname(g_openart, -1); // Or something...
        g_openart = 0;  // Since we didn't finish the art, forget its number
#endif
    }
    else if (bmode == FB_OUTPUT)
    {
        if (g_verbose)
        {
            std::printf("Receiving the rest of the article...");
        }
        else
        {
            std::printf("Receiving...");
        }
        std::fflush(stdout);
    }
    if (s_body_end != s_body_pos)
    {
        std::fseek(g_art_fp, s_body_end.value_of(), 0);
    }
    if (bmode != FB_BACKGROUND)
    {
        nntp_copy_body(b, sizeof b, ArticlePosition{0x7fffffffL});
    }
    else
    {
        while (nntp_copy_body(b, sizeof b, s_body_end + ArticlePosition{1}))
        {
            if (input_pending())
            {
                break;
            }
        }
        if (s_body_pos >= ArticlePosition{})
        {
            std::fseek(g_art_fp, s_body_pos.value_of(), 0);
        }
    }
    if (bmode == FB_OUTPUT)
    {
        erase_line(false); // erase the prompt
    }
    return 1;
}

int nntp_seek_art(ArticlePosition pos)
{
    if (s_body_pos >= ArticlePosition{})
    {
        if (s_body_end < pos)
        {
            char b[NNTP_STRLEN];
            std::fseek(g_art_fp, s_body_end.value_of(), 0);
            nntp_copy_body(b, sizeof b, pos);
            if (s_body_pos >= ArticlePosition{})
            {
                s_body_pos = pos;
            }
        }
        else
        {
            s_body_pos = pos;
        }
    }
    return std::fseek(g_art_fp, pos.value_of(), 0);
}

ArticlePosition nntp_tell_art()
{
    return s_body_pos < ArticlePosition{} ? ArticlePosition{std::ftell(g_art_fp)} : s_body_pos;
}

char *nntp_read_art(char *s, int limit)
{
    if (s_body_pos >= ArticlePosition{})
    {
        if (s_body_pos == s_body_end)
        {
            if (nntp_copy_body(s, limit, s_body_pos + ArticlePosition{1}) <= 0)
            {
                return nullptr;
            }
            if (s_body_end - s_body_pos < ArticlePosition{limit})
            {
                s_body_pos = s_body_end;
                return s;
            }
            std::fseek(g_art_fp, s_body_pos.value_of(), 0);
        }
        s = std::fgets(s, limit, g_art_fp);
        s_body_pos = ArticlePosition{std::ftell(g_art_fp)};
        if (s_body_pos == s_body_end)
        {
            std::fseek(g_art_fp, s_body_pos.value_of(), 0); // Prepare for coming write
        }
        return s;
    }
    return std::fgets(s, limit, g_art_fp);
}

// This is a 1-relative list
static int s_maxdays[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

std::time_t nntp_time()
{
    if (nntp_command("DATE") <= 0)
    {
        return -2;
    }
    if (nntp_check() <= 0)
    {
        return std::time(nullptr);
    }

    char * s = std::strrchr(g_ser_line, ' ') + 1;
    int    month = (s[4] - '0') * 10 + (s[5] - '0');
    int    day = (s[6] - '0') * 10 + (s[7] - '0');
    int    hh = (s[8] - '0') * 10 + (s[9] - '0');
    int    mm = (s[10] - '0') * 10 + (s[11] - '0');
    std::time_t ss = (s[12] - '0') * 10 + (s[13] - '0');
    s[4] = '\0';
    int year = std::atoi(s);

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
    std::sprintf(g_ser_line, "NEWGROUPS %02d%02d%02d %02d%02d%02d GMT",
        ts->tm_year % 100, ts->tm_mon+1, ts->tm_mday,
        ts->tm_hour, ts->tm_min, ts->tm_sec);
    if (nntp_command(g_ser_line) <= 0)
    {
        return -2;
    }
    return nntp_check();
}

int nntp_art_nums()
{
    if (g_data_source->flags & DF_NO_LIST_GROUP)
    {
        return 0;
    }
    if (nntp_command("LISTGROUP") <= 0)
    {
        return -2;
    }
    if (nntp_check() <= 0)
    {
        g_data_source->flags |= DF_NO_LIST_GROUP;
        return 0;
    }
    return 1;
}

#if 0
int nntp_rover()
{
    if (g_datasrc->flags & DF_NOXROVER)
    {
        return 0;
    }
    if (nntp_command("XROVER 1-") <= 0)
    {
        return -2;
    }
    if (nntp_check() <= 0)
    {
        g_datasrc->flags |= DF_NOXROVER;
        return 0;
    }
    return 1;
}
#endif

ArticleNum nntp_find_real_art(ArticleNum after)
{
    ArticleNum an;

    if (g_last_cached > after || g_last_cached < g_abs_first //
        || nntp_stat(g_last_cached) <= 0)
    {
        if (nntp_stat_id("") > after)
        {
            return {};
        }
    }

    while ((an = nntp_next_art()) > ArticleNum{})
    {
        if (an > after)
        {
            return an;
        }
        if ((after - an) > ArticleNum{10})
        {
            break;
        }
    }

    return {};
}

char *nntp_art_name(ArticleNum art_num, bool allocate)
{
    static ArticleNum artnums[MAX_NNTP_ARTICLES];
    static std::time_t  artages[MAX_NNTP_ARTICLES];
    std::time_t         lowage;

    if (!art_num)
    {
        for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
        {
            artnums[i] = ArticleNum{};
            artages[i] = 0;
        }
        return nullptr;
    }

    std::time_t now = std::time(nullptr);

    int j = 0;
    lowage = now;
    for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
    {
        if (artnums[i] == art_num)
        {
            artages[i] = now;
            return nntp_tmp_name(i);
        }
        if (artages[i] <= lowage)
        {
            j = i;
            lowage = artages[j];
        }
    }

    if (allocate)
    {
        artnums[j] = art_num;
        artages[j] = now;
        return nntp_tmp_name(j);
    }

    return nullptr;
}

char *nntp_tmp_name(int ndx)
{
    static char artname[20];
    std::sprintf(artname,"rrn.%ld.%d",g_our_pid,ndx);
    return artname;
}

int nntp_handle_nested_lists()
{
    if (string_case_equal(g_last_command, "quit"))
    {
        return 0; // TODO: flush data needed?
    }
    if (nntp_finish_body(FB_DISCARD))
    {
        return 1;
    }
    std::fprintf(stderr,"Programming error! Nested NNTP calls detected.\n");
    return -1;
}

int nntp_handle_timeout()
{
    static bool handling_timeout = false;
    char last_command_save[NNTP_STRLEN];

    if (string_case_equal(g_last_command, "quit"))
    {
        return 0;
    }
    if (handling_timeout)
    {
        return -1;
    }
    handling_timeout = true;
    std::strcpy(last_command_save, g_last_command);
    nntp_close(false);
    g_data_source->nntp_link = g_nntp_link;
    if (nntp_connect(g_data_source->news_id, false) <= 0)
    {
        return -2;
    }
    g_data_source->nntp_link = g_nntp_link;
    if (g_in_ng && nntp_group(g_newsgroup_name.c_str(), (NewsgroupData*)nullptr) <= 0)
    {
        return -2;
    }
    if (nntp_command(last_command_save) <= 0)
    {
        return -1;
    }
    std::strcpy(g_last_command, last_command_save); // TODO: Is this really needed?
    handling_timeout = false;
    return 1;
}

void nntp_server_died(DataSource *dp)
{
    Multirc* mp = g_multirc;
    close_data_source(dp);
    dp->flags |= DF_UNAVAILABLE;
    unuse_multirc(mp);
    if (!use_multirc(mp))
    {
        g_multirc = nullptr;
    }
    std::fprintf(stderr,"\n%s\n", g_ser_line);
    get_anything();
}

// nntp_read_check -- get a line of text from the server, interpreting
// it as a status message for a binary command.  Call this once
// before calling nntp_read() for the actual data transfer.
//
#ifdef SUPPORT_XTHREAD
long nntp_read_check()
{
    // try to get the status line and the status code
    switch (nntp_check())
    {
    case -2:
        return -2;

    case -1:
    case 0:
        return s_raw_bytes = -1;
    }

    // try to get the number of bytes being transferred
    if (std::sscanf(g_ser_line, "%*d%ld", &s_raw_bytes) != 1)
    {
        return s_raw_bytes = -1;
    }
    return s_raw_bytes;
}
#endif

// nntp_read -- read data from the server in binary format.  This call must
// be preceded by an appropriate binary command and an nntp_read_check call.
//
#ifdef SUPPORT_XTHREAD
long nntp_read(char *buf, long n)
{
    // if no bytes to read, then just return EOF
    if (s_raw_bytes < 0)
    {
        return 0;
    }

#ifdef HAS_SIGHOLD
    sighold(SIGINT);
#endif

    // try to read some data from the server
    if (s_raw_bytes)
    {
        boost::system::error_code ec;
        n = g_nntplink.connection->read(buf, n > s_raw_bytes ? s_raw_bytes : n, ec);
        s_raw_bytes -= n;
    }
    else
    {
        n = 0;
    }

    // if no more left, then fetch the end-of-command signature
    if (s_raw_bytes == 0)
    {
        char buf[5];    // "\r\n.\r\n"

        boost::system::error_code ec;
        int num_remaining = 5;
        while (num_remaining > 0)
        {
            num_remaining -= g_nntplink.connection->read(buf, num_remaining, ec);
        }
        s_raw_bytes = -1;
    }
#ifdef HAS_SIGHOLD
    sigrelse(SIGINT);
#endif
    return n;
}
#endif // SUPPORT_XTHREAD
