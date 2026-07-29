/* artio.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artio.h>

#include <config/common.h>
#include <trn/art.h>
#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/decode.h>
#include <trn/head.h>
#include <trn/mime.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

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

static constexpr std::size_t INITIAL_ART_BUF_SIZE{8 * 1024};
static constexpr std::size_t ART_BUF_GROWTH{LINE_BUF_LEN * 4};

static std::string s_art_buf;

static char *read_art_buf_raw(bool view_inline);

void art_io_init()
{
    s_art_buf.assign(INITIAL_ART_BUF_SIZE, '\0');
    g_art_buf = s_art_buf.data();
    clear_art_buf();
}

void art_io_final()
{
    s_art_buf.clear();
    s_art_buf.shrink_to_fit();
    g_art_buf = nullptr;
}

// open an article, unless it's already open

std::FILE *art_open(ArticleNum art_num, ArticlePosition pos)
{
    Article* ap = article_find(art_num);

    if (!ap || !art_num.value_of() || (ap->m_flags & (AF_EXISTS | AF_FAKE)) != AF_EXISTS)
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
        if (g_data_source->m_flags & DF_REMOTE)
        {
            nntp_body(art_num);
        }
        else
        {
            const std::string art_name{std::to_string(art_num.value_of())};
            g_art_fp = std::fopen(art_name.c_str(), "r");
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
            ap->uncache_article(false);
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
        if (g_data_source->m_flags & DF_REMOTE)
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
    if (g_data_source->m_flags & DF_REMOTE)
    {
        return nntp_seek_art(pos);
    }
    return std::fseek(g_art_fp, pos.value_of(), 0);
}

ArticlePosition ftell_art()
{
    return ArticlePosition{std::ftell(g_art_fp)};
}

ArticlePosition tell_art()
{
    if (g_data_source->m_flags & DF_REMOTE)
    {
        return nntp_tell_art();
    }
    return ftell_art();
}

static std::string read_art_chunk(int limit)
{
    if (g_data_source->m_flags & DF_REMOTE)
    {
        return nntp_read_art(limit);
    }

    if (limit <= 1)
    {
        return {};
    }

    std::string line(static_cast<std::size_t>(limit), '\0');
    if (std::fgets(line.data(), limit, g_art_fp) == nullptr)
    {
        return {};
    }
    const std::size_t terminator = line.find('\0');
    line.resize(terminator == std::string::npos ? line.size() : terminator);
    return line;
}

bool read_art(std::string &line)
{
    line.clear();
    line.reserve(LINE_BUF_LEN);
    while (true)
    {
        const std::string chunk = read_art_chunk(LINE_BUF_LEN);
        if (chunk.empty())
        {
            return !line.empty();
        }

        line += chunk;
        if (line.back() == '\n' || chunk.size() < LINE_BUF_LEN - 1)
        {
            return true;
        }
    }
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
        if (!read_art_buf_raw(false))
        {
            return -1;
        }
    }

    g_art_buf_pos = pos;

    return 0;
}

static char *read_art_buf_raw(bool view_inline)
{
    char       *bp;
    char       *s;
    int         read_offset;
    int   line_offset;
    int   filter_offset;
    int   extra_offset;
    int   len;
    int   o;
    int   word_wrap;
    int   extra_chars = 0;
    int read_something = 0;
    std::string description;
    const auto  write_newline = [](std::size_t offset)
    {
        s_art_buf[offset] = '\n';
        s_art_buf[offset + 1] = '\0';
    };

    if (!g_do_hiding)
    {
        if (!read_art(g_art_line))
        {
            return nullptr;
        }
        bp = g_art_line.data();
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
    if (s_art_buf.size() < static_cast<std::size_t>(g_art_buf_pos.value_of() + o + LINE_BUF_LEN))
    {
        s_art_buf.resize(s_art_buf.size() + ART_BUF_GROWTH);
        g_art_buf = s_art_buf.data();
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
        const int         read_limit = static_cast<int>(s_art_buf.size() - g_art_buf_pos.value_of() - o - 1);
        const std::string chunk = read_art_chunk(read_limit);
        if (chunk.empty())
        {
            if (!read_offset)
            {
                *bp = '\0';
                len = 0;
                bp = nullptr;
                goto done;
            }
            write_newline(static_cast<std::size_t>(bp + o - g_art_buf));
            read_something = -1;
            len = read_offset + 1;
        }
        else
        {
            chunk.copy(bp + o, chunk.size());
            bp[o + chunk.size()] = '\0';
            len = static_cast<int>(chunk.size()) + read_offset;
        }
        if (bp[len + extra_offset - 1] != '\n')
        {
            if (read_something >= 0)
            {
                read_offset = len;
                goto read_more;
            }
            write_newline(static_cast<std::size_t>(bp + len++ + extra_offset - g_art_buf));
        }
        if (!g_is_mime)
        {
            goto done;
        }
        o = line_offset + extra_offset;
        mime_set_state(bp+o);
        if (bp[o] == '\0')
        {
            write_newline(static_cast<std::size_t>(bp + o - g_art_buf));
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
        if (g_mime_section->m_encoding == MENCODE_QPRINT)
        {
            o = line_offset + extra_offset;
            std::string decoded = qp_decode_string(bp + o, false);
            decoded.copy(bp + o, decoded.size());
            bp[o + decoded.size()] = '\0';
            len = static_cast<int>(decoded.size()) + line_offset;
            if (len == line_offset || bp[len + extra_offset - 1] != '\n')
            {
                if (read_something >= 0)
                {
                    read_offset = len;
                    line_offset = len;
                    goto read_more;
                }
                write_newline(static_cast<std::size_t>(bp + len++ + extra_offset - g_art_buf));
            }
        }
        else if (g_mime_section->m_encoding == MENCODE_BASE64)
        {
            o = line_offset + extra_offset;
            std::string decoded = b64_decode_string(bp + o);
            decoded.copy(bp + o, decoded.size());
            bp[o + decoded.size()] = '\0';
            len = static_cast<int>(decoded.size()) + line_offset;
            const std::size_t newline_pos = decoded.find('\n');
            if (newline_pos == std::string::npos)
            {
                if (read_something >= 0)
                {
                    read_offset = len;
                    line_offset = len;
                    goto read_more;
                }
                write_newline(static_cast<std::size_t>(bp + len++ + extra_offset - g_art_buf));
            }
            else
            {
                extra_chars += len;
                len = line_offset + static_cast<int>(newline_pos) + 1;
                extra_chars -= len;
            }
        }
        if (g_mime_state != HTML_TEXT_MIME)
        {
            break;
        }
        o = filter_offset + extra_offset;
        {
            std::string filtered = filter_html(bp + o, std::string_view{bp, static_cast<std::size_t>(filter_offset)},
                                               static_cast<int>(g_art_buf_pos.value_of()));
            filtered.copy(bp, filtered.size());
            bp[filtered.size()] = '\0';
            len = static_cast<int>(filtered.size());
            const std::size_t newline_pos = filtered.find('\n');
            if (len == filter_offset || newline_pos == std::string::npos)
            {
                if (read_something >= 0)
                {
                    read_offset = len;
                    line_offset = len;
                    filter_offset = len;
                    goto read_more;
                }
                write_newline(static_cast<std::size_t>(bp + len++ - g_art_buf));
                extra_chars = 0;
            }
            else
            {
                extra_chars = len;
                len = static_cast<int>(newline_pos) + 1;
                extra_chars -= len;
            }
        }
        break;

    case DECODE_MIME:
    {
        MimeCapEntry* mcp;
        mcp = mime_find_mimecap_entry(*g_mime_section->m_type_name,
                                    MCF_NEEDS_TERMINAL |MCF_COPIOUS_OUTPUT);
        if (mcp)
        {
            int save_term_line = g_term_line;
            g_no_wait_fork = true;
            color_object(COLOR_MIME_DESC, true);
            if (decode_piece(mcp, bp))
            {
                const std::size_t destination = static_cast<std::size_t>(g_art_buf_pos.value_of());
                if (s_art_buf.size() <= destination + g_art_line.size())
                {
                    s_art_buf.resize(destination + g_art_line.size() + 1);
                    g_art_buf = s_art_buf.data();
                }
                bp = g_art_buf + destination;
                g_art_line.copy(bp, g_art_line.size());
                bp[g_art_line.size()] = '\0';
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
        while ((mp = mp->m_prev) != nullptr && !mp->m_boundary_len)
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
        if (g_mime_section->m_prev)
        {
            g_mime_state = SKIP_MIME;
        }
        else
        {
            if (g_data_source->m_flags & DF_REMOTE)
            {
                nntp_finish_body(FB_SILENT);
                g_raw_art_size = nntp_art_size();
            }
            seek_art(g_raw_art_size);
        }
        // FALL THROUGH

    case BETWEEN_MIME:
        len = static_cast<int>(g_multipart_separator.size()) + 1;
        if (extra_offset && filter_offset)
        {
            extra_chars = len + 1;
            len = read_offset + 1;
            o = read_offset + 1;
            s_art_buf[static_cast<std::size_t>(bp + o - 1 - g_art_buf)] = '\n';
        }
        else
        {
            o = -1;
            ++g_art_buf_pos;
            bp++;
        }
        {
            const std::size_t separator_offset = static_cast<std::size_t>(bp + o - g_art_buf);
            s_art_buf[separator_offset] = '\002';
            g_multipart_separator.copy(&s_art_buf[separator_offset + 1], g_multipart_separator.size());
            write_newline(separator_offset + g_multipart_separator.size() + 1);
        }
        break;

    case UNHANDLED_MIME:
        g_mime_state = SKIP_MIME;
        *bp++ = '\001';
        ++g_art_buf_pos;
        description = g_mime_section->mime_description();
        break;

    case ALTERNATE_MIME:
        g_mime_state = SKIP_MIME;
        *bp++ = '\001';
        ++g_art_buf_pos;
        {
            const std::string alternative = fmt::format("[Alternative: {}]\n", *g_mime_section->m_type_name);
            alternative.copy(bp, alternative.size());
            bp[alternative.size()] = '\0';
            len = static_cast<int>(alternative.size());
        }
        break;

    case IMAGE_MIME:
    case AUDIO_MIME:
        if (!g_mime_article.m_total && !g_multimedia_mime)
        {
            g_multimedia_mime = true;
        }
        // FALL THROUGH

    default:
        if (view_inline && g_first_view < g_art_line_num //
          && (g_mime_section->m_flags & MSF_INLINE))
        {
            g_mime_state = DECODE_MIME;
        }
        else
        {
            g_mime_state = SKIP_MIME;
        }
        *bp++ = '\001';
        ++g_art_buf_pos;
        description = g_mime_section->mime_description();
        break;
    }

    if (!description.empty())
    {
        const std::size_t limit = g_tc_COLS > 0 ? static_cast<std::size_t>(g_tc_COLS) : 0;
        if (description.size() > limit)
        {
            if (limit >= 5)
            {
                description.replace(limit - 5, std::string::npos, "...]\n");
            }
            else
            {
                description.resize(limit);
            }
        }
        description.copy(bp, description.size());
        len = static_cast<int>(description.size());
        bp[len] = '\0';
    }

done:
    word_wrap = g_tc_COLS - g_word_wrap_offset;
    if (read_something && g_word_wrap_offset >= 0 && word_wrap > 20 && bp)
    {
        for (char *cp = bp; *cp; cp = s + 1)
        {
            const std::size_t cp_offset = static_cast<std::size_t>(cp - g_art_buf);
            const std::size_t newline_pos = s_art_buf.find('\n', cp_offset);
            const std::size_t nul_pos = s_art_buf.find('\0', cp_offset);
            if (newline_pos == std::string::npos || (nul_pos != std::string::npos && nul_pos < newline_pos))
            {
                break;
            }
            s = g_art_buf + newline_pos;
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
                        const std::size_t source_offset = static_cast<std::size_t>(t - g_art_buf);
                        const std::size_t dest_offset = static_cast<std::size_t>(t - spaces - g_art_buf);
                        const std::size_t copy_limit = extra_chars > 0 ? static_cast<std::size_t>(extra_chars - 1) : 0;
                        const std::size_t source_end = source_offset + copy_limit;
                        const std::size_t nul_offset = s_art_buf.find('\0', source_offset);
                        const std::size_t copy_count =
                            nul_offset < source_end ? nul_offset - source_offset : copy_limit;
                        std::char_traits<char>::move(&s_art_buf[dest_offset], &s_art_buf[source_offset], copy_count);
                        s_art_buf[dest_offset + copy_count] = '\0';
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
        if (g_art_size >= 0)
        {
            g_art_size = g_raw_art_size - g_art_buf_seek + g_art_buf_len + g_header_type[PAST_HEADER].min_pos;
        }
    }

    return bp;
}

bool read_art_buf(std::string &line, bool view_inline)
{
    const char *const buffer = read_art_buf_raw(view_inline);
    if (buffer == nullptr)
    {
        line.clear();
        return false;
    }
    const std::string_view text{buffer};
    const std::size_t      line_end = text.find_first_of(std::string_view{"\n\003", 2});
    line = line_end == std::string_view::npos ? text : text.substr(0, line_end + 1);
    return true;
}
