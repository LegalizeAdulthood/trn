/* artio.c
 */
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/artio.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/art.h"
#include "trn/artstate.h"
#include "trn/cache.h"
#include "trn/color.h"
#include "trn/datasrc.h"
#include "trn/decode.h"
#include "trn/head.h"
#include "trn/mime.h"
#include "trn/nntp.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

ArticlePosition g_art_pos{};              // byte position in article file
ArticleLine     g_art_line_num{};         // current line number in article file
std::FILE      *g_art_fp{};               // current article file pointer
ArticleNum      g_open_art{};             // the article number we have open
char           *g_art_buf{};              //
ArticlePosition g_art_buf_pos{};          //
ArticlePosition g_art_buf_seek{};         // TODO: ArticlePosition
ArticlePosition g_art_buf_len{};          //
char            g_wrapped_nl{WRAPPED_NL}; //
int             g_word_wrap_offset{8};    // right-hand column size (0 is off)

static long s_art_buf_size{};

void art_io_init()
{
    s_art_buf_size = 8 * 1024;
    g_art_buf = safe_malloc(s_art_buf_size);
    clear_art_buf();
}

void art_io_final()
{
    safe_free0(g_art_buf);
}

// open an article, unless it's already open

std::FILE *art_open(ArticleNum art_num, ArticlePosition pos)
{
    Article* ap = article_find(art_num);

    if (!ap || !art_num.value_of() || (ap->flags & (AF_EXISTS | AF_FAKE)) != AF_EXISTS)
    {
        errno = ENOENT;
        return nullptr;
    }
    if (g_open_art == art_num)            // this article is already open?
    {
        seek_art(pos);                   // yes: just seek the file
        return g_art_fp;                 // and say we succeeded
    }
    art_close();

    while (true)
    {
        if (g_data_source->flags & DF_REMOTE)
        {
            nntp_body(art_num);
        }
        else
        {
            char art_name[MAX_FILENAME]; // filename of current article
            std::sprintf(art_name, "%ld", art_num.value_of());
            g_art_fp = std::fopen(art_name, "r");
        }
        if (!g_art_fp)
        {
#ifdef ETIMEDOUT
            if (errno == ETIMEDOUT)
            {
                continue;
            }
#endif
            if (errno == EINTR)
            {
                continue;
            }
            uncache_article(ap, false);
        }
        else
        {
            g_open_art = art_num; // remember what we did here
            seek_art(pos);
        }
        break;
    }
    return g_art_fp;                     // and return either fp or nullptr
}

void art_close()
{
    if (g_art_fp != nullptr)             // article still open?
    {
        if (g_data_source->flags & DF_REMOTE)
        {
            nntp_finish_body(FB_DISCARD);
        }
        std::fclose(g_art_fp);                        // close it
        g_art_fp = nullptr;                      // and tell the world
        g_open_art = ArticleNum{};
        clear_art_buf();
    }
}

int seek_art(ArticlePosition pos)
{
    if (g_data_source->flags & DF_REMOTE)
    {
        return nntp_seek_art(pos);
    }
    return std::fseek(g_art_fp, pos.value_of(), 0);
}

ArticlePosition tell_art()
{
    if (g_data_source->flags & DF_REMOTE)
    {
        return nntp_tell_art();
    }
    return (ArticlePosition)std::ftell(g_art_fp);
}

char *read_art(char *s, int limit)
{
    if (g_data_source->flags & DF_REMOTE)
    {
        return nntp_read_art(s, limit);
    }
    return std::fgets(s,limit,g_art_fp);
}

void clear_art_buf()
{
    *g_art_buf = '\0';
    g_art_buf_len = ArticlePosition{};
    g_art_buf_seek = ArticlePosition{};
    g_art_buf_pos = ArticlePosition{};
}

int seek_art_buf(ArticlePosition pos)
{
    if (!g_do_hiding)
    {
        return seek_art(pos);
    }

    pos -= g_header_type[PAST_HEADER].min_pos;
    g_art_buf_pos = g_art_buf_len;

    while (g_art_buf_pos < pos)
    {
        if (!read_art_buf(false))
        {
            return -1;
        }
    }

    g_art_buf_pos = pos;

    return 0;
}

char *read_art_buf(bool view_inline)
{
    char* bp;
    char *s;
    int   read_offset;
    int   line_offset;
    int   filter_offset;
    int   extra_offset;
    int   len;
    int   o;
    int   word_wrap;
    int   extra_chars = 0;
    int read_something = 0;

    if (!g_do_hiding)
    {
        bp = read_art(g_art_line, (sizeof g_art_line) - 1);
        const ArticlePosition art_pos = tell_art() - g_header_type[PAST_HEADER].min_pos;
        g_art_buf_seek = art_pos;
        g_art_buf_pos = art_pos;
        return bp;
    }
    if (g_art_buf_pos == g_art_size - g_header_type[PAST_HEADER].min_pos)
    {
        return nullptr;
    }
    bp = g_art_buf + g_art_buf_pos.value_of();
    if (*bp == '\001' || *bp == '\002')
    {
        bp++;
        ++g_art_buf_pos;
    }
    if (*bp)
    {
        for (s = bp; *s && !at_nl(*s); s++)
        {
        }
        if (*s)
        {
            len = s - bp + 1;
            goto done;
        }
        read_offset = s - bp;
        line_offset = s - bp;
        filter_offset = s - bp;
    }
    else
    {
        read_offset = 0;
        line_offset = 0;
        filter_offset = 0;
    }

read_more:
    extra_offset = g_mime_state == HTML_TEXT_MIME? 1024 : 0;
    o = read_offset + extra_offset;
    if (s_art_buf_size < g_art_buf_pos.value_of() + o + LINE_BUF_LEN)
    {
        s_art_buf_size += LINE_BUF_LEN * 4;
        g_art_buf = safe_realloc(g_art_buf,s_art_buf_size);
        bp = g_art_buf + g_art_buf_pos.value_of();
    }
    switch (g_mime_state)
    {
    case IMAGE_MIME:
    case AUDIO_MIME:
        break;

    default:
        read_something = 1;
        // The -1 leaves room for appending a newline, if needed
        if (!read_art(bp + o, s_art_buf_size - g_art_buf_pos.value_of() - o - 1))
        {
            if (!read_offset)
            {
                *bp = '\0';
                len = 0;
                bp = nullptr;
                goto done;
            }
            std::strcpy(bp+o, "\n");
            read_something = -1;
        }
        len = std::strlen(bp+o) + read_offset;
        if (bp[len + extra_offset - 1] != '\n')
        {
            if (read_something >= 0)
            {
                read_offset = len;
                goto read_more;
            }
            std::strcpy(bp + len++ + extra_offset, "\n");
        }
        if (!g_is_mime)
        {
            goto done;
        }
        o = line_offset + extra_offset;
        mime_set_state(bp+o);
        if (bp[o] == '\0')
        {
            std::strcpy(bp+o, "\n");
            len = line_offset+1;
        }
        break;
    }
mime_switch:
    switch (g_mime_state)
    {
    case ISO_TEXT_MIME:
        g_mime_state = TEXT_MIME;
        // FALL THROUGH

    case TEXT_MIME:
    case HTML_TEXT_MIME:
        if (g_mime_section->encoding == MENCODE_QPRINT)
        {
            o = line_offset + extra_offset;
            len = qp_decode_string(bp+o, bp+o, false) + line_offset;
            if (len == line_offset || bp[len + extra_offset - 1] != '\n')
            {
                if (read_something >= 0)
                {
                    read_offset = len;
                    line_offset = len;
                    goto read_more;
                }
                std::strcpy(bp + len++ + extra_offset, "\n");
            }
        }
        else if (g_mime_section->encoding == MENCODE_BASE64)
        {
            o = line_offset + extra_offset;
            len = b64_decode_string(bp+o, bp+o) + line_offset;
            s = std::strchr(bp + o, '\n');
            if (s == nullptr)
            {
                if (read_something >= 0)
                {
                    read_offset = len;
                    line_offset = len;
                    goto read_more;
                }
                std::strcpy(bp + len++ + extra_offset, "\n");
            }
            else
            {
                extra_chars += len;
                len = s - bp - extra_offset + 1;
                extra_chars -= len;
            }
        }
        if (g_mime_state != HTML_TEXT_MIME)
        {
            break;
        }
        o = filter_offset + extra_offset;
        len = filter_html(bp+filter_offset, bp+o) + filter_offset;
        if (len == filter_offset || (s = std::strchr(bp, '\n')) == nullptr)
        {
            if (read_something >= 0)
            {
                read_offset = len;
                line_offset = len;
                filter_offset = len;
                goto read_more;
            }
            std::strcpy(bp + len++, "\n");
            extra_chars = 0;
        }
        else
        {
            extra_chars = len;
            len = s - bp + 1;
            extra_chars -= len;
        }
        break;

    case DECODE_MIME:
    {
        MimeCapEntry* mcp;
        mcp = mime_find_mimecap_entry(g_mime_section->type_name,
                                    MCF_NEEDS_TERMINAL |MCF_COPIOUS_OUTPUT);
        if (mcp)
        {
            int save_term_line = g_term_line;
            g_no_wait_fork = true;
            color_object(COLOR_MIME_DESC, true);
            if (decode_piece(mcp, bp))
            {
                bp = g_art_buf + g_art_buf_pos.value_of();
                std::strcpy(bp, g_art_line);
                mime_set_state(bp);
                if (g_mime_state == DECODE_MIME)
                {
                    g_mime_state = SKIP_MIME;
                }
            }
            else
            {
                g_mime_state = SKIP_MIME;
            }
            color_pop();
            chdir_news_dir();
            erase_line(false);
            g_no_wait_fork = false;
            g_first_view = g_art_line_num;
            g_term_line = save_term_line;
            if (g_mime_state != SKIP_MIME)
            {
                goto mime_switch;
            }
        }
        // FALL THROUGH
    }

    case SKIP_MIME:
    {
        MimeSection* mp = g_mime_section;
        while ((mp = mp->prev) != nullptr && !mp->boundary_len)
        {
        }
        if (!mp)
        {
            g_art_buf_len = g_art_buf_pos;
            g_art_size = g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
            read_something = 0;
            bp = nullptr;
        }
        else if (read_something >= 0)
        {
            *bp = '\0';
            read_offset = 0;
            line_offset = 0;
            filter_offset = 0;
            goto read_more;
        }
        else
        {
            *bp = '\0';
        }
        len = 0;
        break;
    }

    case END_OF_MIME:
        if (g_mime_section->prev)
        {
            g_mime_state = SKIP_MIME;
        }
        else
        {
            if (g_data_source->flags & DF_REMOTE)
            {
                nntp_finish_body(FB_SILENT);
                g_raw_art_size = nntp_art_size();
            }
            seek_art(g_raw_art_size);
        }
        // FALL THROUGH

    case BETWEEN_MIME:
        len = std::strlen(g_multipart_separator.c_str()) + 1;
        if (extra_offset && filter_offset)
        {
            extra_chars = len + 1;
            len = read_offset + 1;
            o = read_offset + 1;
            bp[o-1] = '\n';
        }
        else
        {
            o = -1;
            ++g_art_buf_pos;
            bp++;
        }
        std::sprintf(bp+o,"\002%s\n",g_multipart_separator.c_str());
        break;

    case UNHANDLED_MIME:
        g_mime_state = SKIP_MIME;
        *bp++ = '\001';
        ++g_art_buf_pos;
        mime_description(g_mime_section,bp,g_tc_COLS);
        len = std::strlen(bp);
        break;

    case ALTERNATE_MIME:
        g_mime_state = SKIP_MIME;
        *bp++ = '\001';
        ++g_art_buf_pos;
        std::sprintf(bp,"[Alternative: %s]\n", g_mime_section->type_name);
        len = std::strlen(bp);
        break;

    case IMAGE_MIME:
    case AUDIO_MIME:
        if (!g_mime_article.total && !g_multimedia_mime)
        {
            g_multimedia_mime = true;
        }
        // FALL THROUGH

    default:
        if (view_inline && g_first_view < g_art_line_num //
          && (g_mime_section->flags & MSF_INLINE))
        {
            g_mime_state = DECODE_MIME;
        }
        else
        {
            g_mime_state = SKIP_MIME;
        }
        *bp++ = '\001';
        ++g_art_buf_pos;
        mime_description(g_mime_section,bp,g_tc_COLS);
        len = std::strlen(bp);
        break;
    }

done:
    word_wrap = g_tc_COLS - g_word_wrap_offset;
    if (read_something && g_word_wrap_offset >= 0 && word_wrap > 20 && bp)
    {
        for (char *cp = bp; *cp && (s = std::strchr(cp, '\n')) != nullptr; cp = s + 1)
        {
            if (s - cp > g_tc_COLS)
            {
                char* t;
                do
                {
                    for (t = cp+word_wrap; !is_hor_space(*t) && t > cp; t--)
                    {
                    }
                    if (t == cp)
                    {
                        for (t = cp+word_wrap; !is_hor_space(*t) && t<=cp+g_tc_COLS; t++)
                        {
                        }
                        if (t > cp + g_tc_COLS)
                        {
                            t = cp + g_tc_COLS - 1;
                            continue;
                        }
                    }
                    if (cp == bp)
                    {
                        extra_chars += len;
                        len = t - bp + 1;
                        extra_chars -= len;
                    }
                    *t = g_wrapped_nl;
                    if (is_hor_space(t[1]))
                    {
                        int spaces = 1;
                        for (t++; *++t == ' ' || *t == '\t'; spaces++)
                        {
                        }
                        safe_copy(t-spaces,t,extra_chars);
                        extra_chars -= spaces;
                        t -= spaces + 1;
                    }
                } while (s - (cp = t+1) > word_wrap);
            }
        }
    }
    g_art_buf_pos += ArticlePosition{len};
    if (read_something)
    {
        g_art_buf_seek = tell_art();
        g_art_buf_len = g_art_buf_pos + ArticlePosition{extra_chars};
        if (g_art_size >= ArticlePosition{})
        {
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
        }
    }

    return bp;
}
