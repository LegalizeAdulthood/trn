/* head.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/head.h"

#include "nntp/nntpclient.h"
#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/artio.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/final.h"
#include "trn/mempool.h"
#include "trn/ng.h"
#include "trn/nntp.h"
#include "trn/rt-process.h"
#include "trn/rt-util.h"
#include "trn/string-algos.h"
#include "trn/util.h"
#include "util/util2.h"

#include <parsedate/parsedate.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define HIDDEN    (HT_HIDE|HT_DEF_HIDE)
#define MAGIC_ON  (HT_MAGIC_OK|HT_MAGIC|HT_DEF_MAGIC)
#define MAGIC_OFF (HT_MAGIC_OK)

#define XREF_CACHED HT_CACHED
#define NGS_CACHED  HT_NONE
#define FILT_CACHED HT_NONE

// This array must stay in the same order as the enum values header_line_type
// clang-format off
HeaderType g_header_type[HEAD_LAST] = {
    // name             minpos  maxpos  length   flag
    {"",/*BODY*/        0,      0,      0,      HT_NONE         },
    {"",/*SHOWN*/       0,      0,      0,      HT_NONE         },
    {"",/*HIDDEN*/      0,      0,      0,      HIDDEN          },
    {"",/*CUSTOM*/      0,      0,      0,      HT_NONE         },
    {"unrecognized",    0,      0,      12,     HIDDEN          },
    {"author",          0,      0,      6,      HT_NONE         },
    {"bytes",           0,      0,      5,      HIDDEN|HT_CACHED},
    {"content-name",    0,      0,      12,     HIDDEN          },
    {"content-disposition",
                        0,      0,      19,     HIDDEN          },
    {"content-length",  0,      0,      14,     HIDDEN          },
    {"content-transfer-encoding",
                        0,      0,      25,     HIDDEN          },
    {"content-type",    0,      0,      12,     HIDDEN          },
    {"distribution",    0,      0,      12,     HT_NONE         },
    {"date",            0,      0,      4,      MAGIC_ON        },
    {"expires",         0,      0,      7,      HIDDEN|MAGIC_ON },
    {"followup-to",     0,      0,      11,     HT_NONE         },
    {"from",            0,      0,      4,      MAGIC_OFF|HT_CACHED},
    {"in-reply-to",     0,      0,      11,     HIDDEN          },
    {"keywords",        0,      0,      8,      HT_NONE         },
    {"lines",           0,      0,      5,      HT_CACHED       },
    {"mime-version",    0,      0,      12,     MAGIC_ON|HIDDEN },
    {"message-id",      0,      0,      10,     HIDDEN|HT_CACHED},
    {"newsgroups",      0,      0,      10,     MAGIC_ON|HIDDEN|NGS_CACHED},
    {"path",            0,      0,      4,      HIDDEN          },
    {"relay-version",   0,      0,      13,     HIDDEN          },
    {"reply-to",        0,      0,      8,      HT_NONE         },
    {"references",      0,      0,      10,     HIDDEN|FILT_CACHED},
    {"summary",         0,      0,      7,      HT_NONE         },
    {"subject",         0,      0,      7,      MAGIC_ON|HT_CACHED},
    {"xref",            0,      0,      4,      HIDDEN|XREF_CACHED},
};
// clang-format on

#undef HIDDEN
#undef MAGIC_ON
#undef MAGIC_OFF
#undef NGS_CACHED
#undef XREF_CACHED

UserHeaderType *g_user_htype{};
short           g_user_htype_index[26];
int             g_user_htype_count{};
int             g_user_htype_max{};
ArticleNum      g_parsed_art{}; // the article number we've parsed
HeaderLineType  g_in_header{};  // are we decoding the header?
char           *g_head_buf;

static Article       *s_parsed_artp{}; // the article ptr we've parsed
static long           s_head_buf_size;
static bool           s_first_one; // is this the 1st occurrence of this header line?
static bool           s_reading_nntp_header;
static HeaderLineType s_htypeix[26]{};

void head_init()
{
    for (int i = HEAD_FIRST + 1; i < HEAD_LAST; i++)
    {
        s_htypeix[*g_header_type[i].name - 'a'] = static_cast<HeaderLineType>(i);
    }

    g_user_htype_max = 10;
    g_user_htype = (UserHeaderType*)safe_malloc(g_user_htype_max
                                            * sizeof (UserHeaderType));
    g_user_htype[g_user_htype_count++].name = "*";

    s_head_buf_size = LINE_BUF_LEN * 8;
    g_head_buf = safe_malloc(s_head_buf_size);
}

void head_final()
{
    safe_free0(g_head_buf);
    safe_free0(g_user_htype);
    g_user_htype_count = 0;
}

#ifdef DEBUG
static void dump_header(char *where)
{
    std::printf("header: %ld %s", (long)g_parsed_art, where);

    for (int i = HEAD_FIRST - 1; i < HEAD_LAST; i++)
    {
        std::printf("%15s %4ld %4ld %03o\n",g_header_type[i].name,
               (long)g_htype[i].minpos, (long)g_header_type[i].maxpos,
               g_header_type[i].flags);
    }
}
#endif

HeaderLineType set_line_type(char *bufptr, const char *colon)
{
    char* t;
    char* f;

    if (colon-bufptr > sizeof g_msg)
    {
        return SOME_LINE;
    }

    for (t = g_msg, f = bufptr; f < colon; f++, t++)
    {
        // guard against space before :
        if (std::isspace(*f))
        {
            return SOME_LINE;
        }
        *t = std::isupper(*f) ? static_cast<char>(std::tolower(*f)) : *f;
    }
    *t = '\0';
    f = g_msg;                          // get g_msg into a register
    int len = t - f;

    /* now scan the HEADTYPE table, backwards so we don't have to supply an
     * extra terminating value, using first letter as index, and length as
     * optimization to avoid calling subroutine strEQ unnecessarily.  Hauls.
     */
    if (*f >= 'a' && *f <= 'z')
    {
        for (int i = s_htypeix[*f - 'a']; *g_header_type[i].name == *f; i--)
        {
            if (len == g_header_type[i].length && !std::strcmp(f, g_header_type[i].name))
            {
                return static_cast<HeaderLineType>(i);
            }
        }
        if (len == g_header_type[CUSTOM_LINE].length
         && !std::strcmp(f,g_header_type[(((0+1)+1)+1)].name))
        {
            return CUSTOM_LINE;
        }
        for (int i = g_user_htype_index[*f - 'a']; *g_user_htype[i].name == *f; i--)
        {
            if (len >= g_user_htype[i].length //
                && !std::strncmp(f, g_user_htype[i].name, g_user_htype[i].length))
            {
                if (g_user_htype[i].flags & HT_HIDE)
                {
                    return HIDDEN_LINE;
                }
                return SHOWN_LINE;
            }
        }
    }
    return SOME_LINE;
}

HeaderLineType get_header_num(char *s)
{
    char* end = s + std::strlen(s);

    HeaderLineType i = set_line_type(s, end);     // Sets g_msg to lower-cased header name

    if (i <= SOME_LINE && i != CUSTOM_LINE)
    {
        if (!empty(g_header_type[CUSTOM_LINE].name))
        {
            std::free(g_header_type[CUSTOM_LINE].name);
        }
        g_header_type[CUSTOM_LINE].name = save_str(g_msg);
        g_header_type[CUSTOM_LINE].length = end - s;
        g_header_type[CUSTOM_LINE].flags = g_header_type[i].flags;
        g_header_type[CUSTOM_LINE].min_pos = -1;
        g_header_type[CUSTOM_LINE].max_pos = 0;
        for (char *bp = g_head_buf; *bp; bp = end)
        {
            if (!(end = std::strchr(bp,'\n')) || end == bp)
            {
                break;
            }
            char ch = *++end;
            *end = '\0';
            s = std::strchr(bp,':');
            *end = ch;
            if (!s || (i = set_line_type(bp,s)) != CUSTOM_LINE)
            {
                continue;
            }
            g_header_type[CUSTOM_LINE].min_pos = bp - g_head_buf;
            while (is_hor_space(*end))
            {
                if (!(end = std::strchr(end, '\n')))
                {
                    end = bp + std::strlen(bp);
                    break;
                }
                end++;
            }
            g_header_type[CUSTOM_LINE].max_pos = end - g_head_buf;
            break;
        }
        i = CUSTOM_LINE;
    }
    return i;
}

void start_header(ArticleNum artnum)
{
#ifdef DEBUG
    if (debug & DEB_HEADER)
    {
        dump_header("start_header\n");
    }
#endif
    for (HeaderType &i : g_header_type)
    {
        i.min_pos = -1;
        i.max_pos = 0;
    }
    g_in_header = SOME_LINE;
    s_first_one = false;
    g_parsed_art = artnum;
    s_parsed_artp = article_ptr(artnum);
}

void end_header_line()
{
    if (s_first_one)            // did we just pass 1st occurance?
    {
        s_first_one = false;
        // remember where line left off
        g_header_type[g_in_header].max_pos = g_art_pos;
        if (g_header_type[g_in_header].flags & HT_CACHED)
        {
            if (!get_cached_line(s_parsed_artp, g_in_header, true))
            {
                int start = g_header_type[g_in_header].min_pos
                          + g_header_type[g_in_header].length + 1;
                while (is_hor_space(g_head_buf[start]))
                {
                    start++;
                }
                MemorySize size = g_art_pos - start + 1 - 1;   // pre-strip newline
                if (g_in_header == SUBJ_LINE)
                {
                    set_subj_line(s_parsed_artp, g_head_buf + start, size - 1);
                }
                else
                {
                    char* s = safe_malloc(size);
                    safe_copy(s,g_head_buf+start,size);
                    set_cached_line(s_parsed_artp,g_in_header,s);
                }
            }
        }
    }
}

bool parse_line(char *art_buf, int new_hide, int old_hide)
{
    if (is_hor_space(*art_buf)) // continuation line?
    {
        return old_hide;
    }

    end_header_line();
    char *s = std::strchr(art_buf, ':');
    if (s == nullptr)   // is it the end of the header?
    {
            // Did NNTP ship us a mal-formed header line?
            if (s_reading_nntp_header && *art_buf && *art_buf != '\n')
            {
                g_in_header = SOME_LINE;
                return new_hide;
            }
            g_in_header = PAST_HEADER;
    }
    else                // it is a new header line
    {
            g_in_header = set_line_type(art_buf,s);
            s_first_one = (g_header_type[g_in_header].min_pos < 0);
            if (s_first_one)
            {
                g_header_type[g_in_header].min_pos = g_art_pos;
                if (g_in_header == DATE_LINE)
                {
                    if (!s_parsed_artp->date)
                    {
                        s_parsed_artp->date = parsedate(art_buf + 6);
                    }
                }
            }
#ifdef DEBUG
            if (debug & DEB_HEADER)
            {
                dump_header(art_buf);
            }
#endif
            if (g_header_type[g_in_header].flags & HT_HIDE)
            {
                return new_hide;
            }
    }
    return false;                       // don't hide this line
}

void end_header()
{
    Article* ap = s_parsed_artp;

    end_header_line();
    g_in_header = PAST_HEADER;  // just to be sure

    if (!ap->subj)
    {
        set_subj_line(ap, "<NONE>", 6);
    }

    if (s_reading_nntp_header)
    {
        s_reading_nntp_header = false;
        g_header_type[PAST_HEADER].min_pos = g_art_pos + 1;     // nntp_body will fix this
    }
    else
    {
        g_header_type[PAST_HEADER].min_pos = tell_art();
    }

    /* If there's no References: line, then the In-Reply-To: line may give us
    ** more information.
    */
    if (g_threaded_group //
        && (!(ap->flags & AF_THREADED) || g_header_type[IN_REPLY_LINE].min_pos >= 0))
    {
        if (valid_article(ap))
        {
            Article* artp_hold = g_artp;
            char* references = fetch_lines(g_parsed_art, REFS_LINE);
            char* inreply = fetch_lines(g_parsed_art, IN_REPLY_LINE);
            int reflen = std::strlen(references) + 1;
            grow_str(&references, &reflen, reflen + std::strlen(inreply) + 1);
            safe_cat(references, inreply, reflen);
            thread_article(ap, references);
            std::free(inreply);
            std::free(references);
            g_artp = artp_hold;
            check_poster(ap);
        }
    }
    else if (!(ap->flags & AF_CACHED))
    {
        cache_article(ap);
        check_poster(ap);
    }
}

// read the header into memory and parse it if we haven't already

bool parse_header(ArticleNum art_num)
{
    int len;
    bool had_nl = true;
    int found_nl;

    if (g_parsed_art == art_num)
    {
        return true;
    }
    if (art_num > g_last_art)
    {
        return false;
    }
    spin(20);
    if (g_data_source->flags & DF_REMOTE)
    {
        char *s = nntp_art_name(art_num, false);
        if (s)
        {
            if (!art_open(art_num,(ArticlePosition)0))
            {
                return false;
            }
        }
        else if (nntp_header(art_num) <= 0)
        {
            uncache_article(article_ptr(art_num),false);
            return false;
        }
        else
        {
            s_reading_nntp_header = true;
        }
    }
    else if (!art_open(art_num,(ArticlePosition)0))
    {
        return false;
    }

    start_header(art_num);
    g_art_pos = 0;
    char *bp = g_head_buf;
    while (g_in_header)
    {
        if (s_head_buf_size < g_art_pos + LINE_BUF_LEN)
        {
            len = bp - g_head_buf;
            s_head_buf_size += LINE_BUF_LEN * 4;
            g_head_buf = safe_realloc(g_head_buf,s_head_buf_size);
            bp = g_head_buf + len;
        }
        if (s_reading_nntp_header)
        {
            found_nl = nntp_gets(bp,LINE_BUF_LEN) == NGSR_FULL_LINE;
            if (found_nl < 0)
            {
                std::strcpy(bp, ".");
            }
            if (had_nl && *bp == '.')
            {
                if (!bp[1])
                {
                    *bp++ = '\n';       // tag the end with an empty line
                    break;
                }
                std::strcpy(bp,bp+1);
            }
            len = std::strlen(bp);
            if (found_nl)
            {
                bp[len++] = '\n';
            }
            bp[len] = '\0';
        }
        else
        {
            if (read_art(bp,LINE_BUF_LEN) == nullptr)
            {
                break;
            }
            len = std::strlen(bp);
            found_nl = (bp[len-1] == '\n');
        }
        if (had_nl)
        {
            parse_line(bp, false, false);
        }
        had_nl = found_nl;
        g_art_pos += len;
        bp += len;
    }
    *bp = '\0';
    end_header();
    return true;
}

// get a header line from an article

// article to get line from
// type of line desired
char *fetch_lines(ArticleNum art_num, HeaderLineType which_line)
{
    char* s;

    // Only return a cached line if it isn't the current article
    if (g_parsed_art != art_num)
    {
        // If the line is not in the cache, this will parse the header
        s = fetch_cache(art_num,which_line, FILL_CACHE);
        if (s)
        {
            return save_str(s);
        }
    }
    ArticlePosition firstpos = g_header_type[which_line].min_pos;
    if (firstpos < 0)
    {
        return save_str("");
    }

    firstpos += g_header_type[which_line].length + 1;
    ArticlePosition lastpos = g_header_type[which_line].max_pos;
    int size = lastpos - firstpos;
    char *t = g_head_buf + firstpos;
    while (is_hor_space(*t))
    {
        t++;
        size--;
    }
#ifdef DEBUG
    if (debug && (size < 1 || size > 1000))
    {
        std::printf("Firstpos = %ld, lastpos = %ld\n",(long)firstpos,(long)lastpos);
        std::fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    s = safe_malloc((MemorySize)size);
    safe_copy(s,t,size);
    return s;
}

// (strn) like fetchlines, but for memory pools
// ART_NUM artnum   article to get line from
// int which_line   type of line desired
// int pool         which memory pool to use
char *mp_fetch_lines(ArticleNum art_num, HeaderLineType which_line, MemoryPool pool)
{
    char* s;

    // Only return a cached line if it isn't the current article
    if (g_parsed_art != art_num)
    {
        // If the line is not in the cache, this will parse the header
        s = fetch_cache(art_num,which_line, FILL_CACHE);
        if (s)
        {
            return mp_save_str(s, pool);
        }
    }
    ArticlePosition firstpos = g_header_type[which_line].min_pos;
    if (firstpos < 0)
    {
        return mp_save_str("", pool);
    }

    firstpos += g_header_type[which_line].length + 1;
    ArticlePosition lastpos = g_header_type[which_line].max_pos;
    int size = lastpos - firstpos;
    char *t = g_head_buf + firstpos;
    while (is_hor_space(*t))
    {
        t++;
        size--;
    }
#ifdef DEBUG
    if (debug && (size < 1 || size > 1000))
    {
        std::printf("Firstpos = %ld, lastpos = %ld\n",(long)firstpos,(long)lastpos);
        std::fgets(g_cmd_buf, sizeof g_cmd_buf, stdin);
    }
#endif
    s = mp_malloc(size,pool);
    safe_copy(s,t,size);
    return s;
}

static int nntp_xhdr(HeaderLineType which_line, ArticleNum artnum)
{
    std::sprintf(g_ser_line,"XHDR %s %ld",g_header_type[which_line].name,artnum);
    return nntp_command(g_ser_line);
}

static int nntp_xhdr(HeaderLineType which_line, ArticleNum artnum, ArticleNum lastnum)
{
    std::sprintf(g_ser_line, "XHDR %s %ld-%ld", g_header_type[which_line].name, artnum, lastnum);
    return nntp_command(g_ser_line);
}

// prefetch a header line from one or more articles

// ART_NUM artnum   article to get line from */
// int which_line   type of line desired */
// bool copy    do you want it save_str()ed? */
char *prefetch_lines(ArticleNum art_num, HeaderLineType which_line, bool copy)
{
    char* s;
    char* t;
    ArticlePosition firstpos;

    if ((g_data_source->flags & DF_REMOTE) && g_parsed_art != art_num)
    {
        Article*ap;
        int     size;
        ArticleNum num;
        ArticleNum lastnum;
        bool    hasxhdr = true;

        s = fetch_cache(art_num,which_line, DONT_FILL_CACHE);
        if (s)
        {
            if (copy)
            {
                s = save_str(s);
            }
            return s;
        }

        spin(20);
        if (copy)
        {
            size = LINE_BUF_LEN;
            s = safe_malloc((MemorySize) size);
        }
        else
        {
            s = g_cmd_buf;
            size = sizeof g_cmd_buf;
        }
        *s = '\0';
        ArticleNum priornum = art_num - 1;
        bool    cached = (g_header_type[which_line].flags & HT_CACHED);
        int     status;
        if (cached != 0)
        {
            lastnum = art_num + PREFETCH_SIZE - 1;
            lastnum = std::min(lastnum, g_last_art);
            status = nntp_xhdr(which_line, art_num, lastnum);
        }
        else
        {
            lastnum = art_num;
            status = nntp_xhdr(which_line, art_num);
        }
        if (status <= 0)
        {
            finalize(1);
        }
        if (nntp_check() > 0)
        {
            char* last_buf = g_ser_line;
            MemorySize last_buflen = sizeof g_ser_line;
            while (true)
            {
                char *line = nntp_get_a_line(last_buf, last_buflen, last_buf!=g_ser_line);
# ifdef DEBUG
                if (debug & DEB_NNTP)
                    std::printf("<%s", line? line : "<EOF>");
# endif
                if (nntp_at_list_end(line))
                {
                    break;
                }
                last_buf = line;
                last_buflen = g_buf_len_last_line_got;
                t = std::strchr(line, '\r');
                if (t != nullptr)
                {
                    *t = '\0';
                }
                if (!(t = std::strchr(line, ' ')))
                {
                    continue;
                }
                t++;
                num = std::atol(line);
                if (num < art_num || num > lastnum)
                {
                    continue;
                }
                if (!(g_data_source->flags & DF_XHDR_BROKEN))
                {
                    while ((priornum = article_next(priornum)) < num)
                    {
                        uncache_article(article_ptr(priornum), false);
                    }
                }
                ap = article_find(num);
                if (which_line == SUBJ_LINE)
                {
                    set_subj_line(ap, t, std::strlen(t));
                }
                else if (cached)
                {
                    set_cached_line(ap, which_line, save_str(t));
                }
                if (num == art_num)
                {
                    safe_cat(s, t, size);
                }
            }
            if (last_buf != g_ser_line)
            {
                std::free(last_buf);
            }
        }
        else
        {
            hasxhdr = false;
            lastnum = art_num;
            if (!parse_header(art_num))
            {
                std::fprintf(stderr,"\nBad NNTP response.\n");
                finalize(1);
            }
            s = fetch_lines(art_num,which_line);
        }
        if (hasxhdr && !(g_data_source->flags & DF_XHDR_BROKEN))
        {
            for (priornum = article_first(priornum); priornum < lastnum; priornum = article_next(priornum))
            {
                uncache_article(article_ptr(priornum), false);
            }
        }
        if (copy)
        {
            s = safe_realloc(s, (MemorySize) std::strlen(s) + 1);
        }
        return s;
    }

    // Only return a cached line if it isn't the current article
    s = nullptr;
    if (g_parsed_art != art_num)
    {
        s = fetch_cache(art_num, which_line, FILL_CACHE);
    }
    if (g_parsed_art == art_num && (firstpos = g_header_type[which_line].min_pos) < 0)
    {
        s = "";
    }
    if (s)
    {
        if (copy)
        {
            s = save_str(s);
        }
        return s;
    }

    firstpos += g_header_type[which_line].length + 1;
    ArticlePosition lastpos = g_header_type[which_line].max_pos;
    int size = lastpos - firstpos;
    t = g_head_buf + firstpos;
    while (is_hor_space(*t))
    {
        t++;
        size--;
    }
    if (copy)
    {
        s = safe_malloc((MemorySize) size);
    }
    else                                // hope this is okay--we're
    {
        s = g_cmd_buf;                  // really scraping for space here
        size = std::min(size, static_cast<int>(sizeof g_cmd_buf));
    }
    safe_copy(s,t,size);
    return s;
}
