/* artio.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artio.h>

#include <config/common.h>
#include <trn/art.h>
#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
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
#include <utility>

ArticleInput    g_art_input;
char            g_wrapped_nl{WRAPPED_NL}; //
int             g_word_wrap_offset{8};    // right-hand column size (0 is off)

static constexpr std::size_t INITIAL_ART_BUF_SIZE{8 * 1024};
static constexpr std::size_t ART_BUF_GROWTH{LINE_BUF_LEN * 4};

PresentationBuffer g_art_buf;

PreserveArticleBuffer::PreserveArticleBuffer() :
    m_buffer{std::move(g_art_buf.bytes)},
    m_position{g_art_buf.pos()},
    m_seek{g_art_buf.seek()},
    m_length{g_art_buf.len()}
{
    g_art_buf.clear_art_buf();
}

PreserveArticleBuffer::~PreserveArticleBuffer()
{
    g_art_buf.bytes = std::move(m_buffer);
    g_art_buf.position = m_position;
    g_art_buf.set_seek(m_seek);
    g_art_buf.length = m_length;
}

void PresentationBuffer::init()
{
    bytes.assign(INITIAL_ART_BUF_SIZE, '\0');
    clear_art_buf();
}

void PresentationBuffer::final()
{
    bytes.clear();
    bytes.shrink_to_fit();
    position = ArticlePosition{};
    seek_pos = ArticlePosition{};
    length = ArticlePosition{};
}

// open an article, unless it's already open

std::FILE *art_open(ArticleNum art_num, ArticlePosition pos)
{
    Article *ap = g_cache.find(art_num);

    if (!ap || !art_num.value_of() || (ap->m_flags & (AF_EXISTS | AF_FAKE)) != AF_EXISTS)
    {
        errno = ENOENT;
        return nullptr;
    }
    if (g_art_input.open_art == art_num) // this article is already open?
    {
        seek_art(pos);                   // yes: just seek the file
        return g_art_input.fp;           // and say we succeeded
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
            g_art_input.fp = std::fopen(art_name.c_str(), "r");
        }
        if (!g_art_input.fp)
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
            g_art_input.open_art = art_num; // remember what we did here
            seek_art(pos);
        }
        break;
    }
    return g_art_input.fp; // and return either fp or nullptr
}

void art_close()
{
    if (g_art_input.fp != nullptr) // article still open?
    {
        if (g_data_source->m_flags & DF_REMOTE)
        {
            nntp_finish_body(FB_DISCARD);
        }
        std::fclose(g_art_input.fp); // close it
        g_art_input.fp = nullptr;    // and tell the world
        g_art_input.open_art = ArticleNum{};
        g_art_buf.clear_art_buf();
    }
}

int seek_art(ArticlePosition pos)
{
    if (g_data_source->m_flags & DF_REMOTE)
    {
        return nntp_seek_art(pos);
    }
    return std::fseek(g_art_input.fp, pos.value_of(), 0);
}

ArticlePosition ArticleInput::ftell_art()
{
    return ArticlePosition{std::ftell(fp)};
}

ArticlePosition ftell_art()
{
    return g_art_input.ftell_art();
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
    if (std::fgets(line.data(), limit, g_art_input.fp) == nullptr)
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

void PresentationBuffer::clear_art_buf()
{
    if (!bytes.empty())
    {
        bytes.front() = '\0';
    }
    length = ArticlePosition{};
    seek_pos = ArticlePosition{};
    position = ArticlePosition{};
}

bool PresentationBuffer::empty()
{
    return bytes.empty() || bytes.front() == '\0';
}

bool PresentationBuffer::at_end()
{
    return position == length;
}

ArticlePosition PresentationBuffer::pos()
{
    return position;
}

ArticlePosition PresentationBuffer::len()
{
    return length;
}

ArticlePosition PresentationBuffer::seek()
{
    return seek_pos;
}

void PresentationBuffer::set_seek(ArticlePosition pos)
{
    seek_pos = pos;
}

ArticlePosition PresentationBuffer::article_pos()
{
    return position + g_parsed_header.positions[PAST_HEADER].min_pos;
}

ArticlePosition PresentationBuffer::size_from_raw(ArticlePosition raw_art_size)
{
    if (!g_do_hiding)
    {
        return raw_art_size;
    }
    return raw_art_size - seek_pos + length + g_parsed_header.positions[PAST_HEADER].min_pos;
}

std::string_view art_buf_view(ArticlePosition pos)
{
    if (pos < 0)
    {
        return {};
    }
    if (pos < g_parsed_header.positions[PAST_HEADER].min_pos)
    {
        const std::size_t offset = static_cast<std::size_t>(pos.value_of());
        return offset < g_parsed_header.head_buf.size() ? std::string_view{g_parsed_header.head_buf}.substr(offset)
                                                        : std::string_view{};
    }
    const ArticlePosition offset = pos - g_parsed_header.positions[PAST_HEADER].min_pos;
    if (offset >= g_art_buf.len())
    {
        return {};
    }
    return std::string_view{g_art_buf.bytes}.substr(static_cast<std::size_t>(offset.value_of()),
                                                    static_cast<std::size_t>((g_art_buf.len() - offset).value_of()));
}

char *PresentationBuffer::data()
{
    return bytes.data();
}

std::size_t PresentationBuffer::offset(const char *ptr)
{
    return static_cast<std::size_t>(ptr - data());
}

int seek_art_buf(ArticlePosition pos)
{
    if (!g_do_hiding)
    {
        return seek_art(pos);
    }

    pos -= g_parsed_header.positions[PAST_HEADER].min_pos;
    g_art_buf.position = g_art_buf.len();

    while (g_art_buf.pos() < pos)
    {
        if (!read_art_buf_raw(false))
        {
            return -1;
        }
    }

    g_art_buf.position = pos;

    return 0;
}

void PresentationBuffer::store_text(char *&buffer, std::size_t buff_offset, std::string_view text)
{
    const std::size_t base = offset(buffer);
    const std::size_t required = base + buff_offset + text.size() + 2;
    if (bytes.size() < required)
    {
        bytes.resize(required + ART_BUF_GROWTH);
        buffer = data() + base;
    }
    text.copy(buffer + buff_offset, text.size());
    buffer[buff_offset + text.size()] = '\0';
}

static std::string substitute_art_text(std::string_view text)
{
    const std::size_t last_newline = text.rfind('\n');
    if (last_newline == std::string_view::npos)
    {
        return std::string{text};
    }
    CharSubstitution substitution{g_subst.current_mode()};
    std::string      result = substitution.substitute(text.substr(0, last_newline + 1));
    result += substitution.finish();
    result += text.substr(last_newline + 1);
    return result;
}

static void finish_mime_article()
{
    if (g_data_source->m_flags & DF_REMOTE)
    {
        nntp_finish_body(FB_SILENT);
        g_raw_art_size = nntp_art_size();
    }
    seek_art(g_raw_art_size);
}

char *read_art_buf_raw(bool view_inline)
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
    int         converted_offset = 0;
    std::string description;
    const auto  write_newline = [](std::size_t offset)
    {
        g_art_buf.bytes[offset] = '\n';
        g_art_buf.bytes[offset + 1] = '\0';
    };

    if (!g_do_hiding)
    {
        if (!read_art(g_art_line))
        {
            return nullptr;
        }
        bp = g_art_line.data();
        const ArticlePosition art_pos = tell_art() - g_parsed_header.positions[PAST_HEADER].min_pos;
        g_art_buf.set_seek(art_pos);
        g_art_buf.position = art_pos;
        return bp;
    }
    if (g_art_buf.pos() == g_art_size - g_parsed_header.positions[PAST_HEADER].min_pos && !g_mime.has_pending_text())
    {
        return nullptr;
    }
    bp = g_art_buf.data() + g_art_buf.pos().value_of();
    if (*bp == '\001' || *bp == '\002')
    {
        bp++;
        ++g_art_buf.position;
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
        converted_offset = read_offset;
    }
    else
    {
        read_offset = 0;
        line_offset = 0;
        filter_offset = 0;
    }

read_more:
    extra_offset = g_mime.state == HTML_TEXT_MIME ? 1024 : 0;
    o = read_offset + extra_offset;
    if (g_art_buf.bytes.size() < static_cast<std::size_t>(g_art_buf.pos().value_of() + o + LINE_BUF_LEN))
    {
        g_art_buf.bytes.resize(g_art_buf.bytes.size() + ART_BUF_GROWTH);
        bp = g_art_buf.data() + g_art_buf.pos().value_of();
    }
    switch (g_mime.state)
    {
    case IMAGE_MIME:
    case AUDIO_MIME:
        break;

    default:
        read_something = 1;
        // The -1 leaves room for appending a newline, if needed
        const int         read_limit = static_cast<int>(g_art_buf.bytes.size() - g_art_buf.pos().value_of() - o - 1);
        const std::string chunk = read_art_chunk(read_limit);
        if (chunk.empty())
        {
            if (!read_offset && !g_mime.has_pending_text())
            {
                *bp = '\0';
                len = 0;
                bp = nullptr;
                goto done;
            }
            write_newline(g_art_buf.offset(bp + o));
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
            write_newline(g_art_buf.offset(bp + len++ + extra_offset));
        }
        if (!g_mime.is_mime)
        {
            goto mime_switch;
        }
        o = line_offset + extra_offset;
        if (g_mime.end_of_section(std::string_view{bp + o}) && (line_offset || g_mime.has_pending_text()))
        {
            const std::string boundary{bp + o};
            const std::size_t source = static_cast<std::size_t>(converted_offset + extra_offset);
            const std::string decoded =
                g_mime.decode_text_chunk({bp + source, static_cast<std::size_t>(line_offset - converted_offset)}, true);
            std::string tail = g_mime.state == HTML_TEXT_MIME
                                   ? g_mime.filter(decoded, {bp, static_cast<std::size_t>(filter_offset)},
                                                   static_cast<int>(g_art_buf.pos().value_of()))
                                   : std::string{bp, static_cast<std::size_t>(converted_offset)} + decoded;
            tail += '\n';
            tail = substitute_art_text(tail);
            const std::string separator = std::string(1, '\002') + g_multipart_separator + '\n';
            (void) mime_set_state(boundary);
            g_art_buf.store_text(bp, 0, tail + separator);
            const std::size_t newline = tail.find('\n');
            len = static_cast<int>(newline == std::string::npos ? tail.size() : newline + 1);
            extra_chars = static_cast<int>(tail.size() + separator.size()) - len;
            if (g_mime.state == END_OF_MIME && g_mime.at_top_level())
            {
                finish_mime_article();
            }
            goto done;
        }
        if (mime_set_state(std::string_view{bp + o}))
        {
            if (g_mime.state == TEXT_MIME || g_mime.state == HTML_TEXT_MIME || g_mime.state == ISO_TEXT_MIME)
            {
                read_offset = 0;
                line_offset = 0;
                filter_offset = 0;
                converted_offset = 0;
                *bp = '\0';
                goto read_more;
            }
            write_newline(g_art_buf.offset(bp + o));
            len = line_offset + 1;
        }
        break;
    }
mime_switch:
    switch (g_mime.state)
    {
    case ISO_TEXT_MIME:
        g_mime.state = TEXT_MIME;
        // FALL THROUGH

    case NOT_MIME:
    case TEXT_MIME:
    case HTML_TEXT_MIME:
        if (g_mime.section->m_encoding == MENCODE_QPRINT)
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
                write_newline(g_art_buf.offset(bp + len++ + extra_offset));
            }
        }
        else if (g_mime.section->m_encoding == MENCODE_BASE64)
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
                write_newline(g_art_buf.offset(bp + len++ + extra_offset));
            }
            else
            {
                extra_chars += len;
                len = line_offset + static_cast<int>(newline_pos) + 1;
                extra_chars -= len;
            }
        }
        {
            const std::size_t source = static_cast<std::size_t>(converted_offset + extra_offset);
            const std::string decoded = g_mime.decode_text_chunk(bp + source);
            g_art_buf.store_text(bp, source, decoded);
            len = converted_offset + static_cast<int>(decoded.size());
        }
        if (g_mime.state != HTML_TEXT_MIME)
        {
            const std::string presented = substitute_art_text(std::string_view{bp, static_cast<std::size_t>(len)});
            g_art_buf.store_text(bp, 0, presented);
            const std::size_t newline = presented.find('\n');
            len = static_cast<int>(newline == std::string::npos ? presented.size() : newline + 1);
            extra_chars = static_cast<int>(presented.size()) - len;
            break;
        }
        o = filter_offset + extra_offset;
        {
            std::string filtered = g_mime.filter(bp + o, std::string_view{bp, static_cast<std::size_t>(filter_offset)},
                                                 static_cast<int>(g_art_buf.pos().value_of()));
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
                    converted_offset = len;
                    goto read_more;
                }
                filtered += '\n';
            }
            const std::string presented = substitute_art_text(filtered);
            g_art_buf.store_text(bp, 0, presented);
            const std::size_t newline = presented.find('\n');
            len = static_cast<int>(newline == std::string::npos ? presented.size() : newline + 1);
            extra_chars = static_cast<int>(presented.size()) - len;
        }
        break;

    case DECODE_MIME:
    {
        MimeCapEntry* mcp;
        mcp = g_mimecap.find(*g_mime.section->m_type_name, MCF_NEEDS_TERMINAL | MCF_COPIOUS_OUTPUT);
        if (mcp)
        {
            int save_term_line = g_term_line;
            g_no_wait_fork = true;
            g_color.object(COLOR_MIME_DESC, true);
            // A failed decoder can already have consumed the next boundary.
            if (decode_piece(mcp, bp) || g_mime.end_of_section(g_art_line))
            {
                const std::size_t destination = static_cast<std::size_t>(g_art_buf.pos().value_of());
                if (g_art_buf.bytes.size() <= destination + g_art_line.size())
                {
                    g_art_buf.bytes.resize(destination + g_art_line.size() + 1);
                }
                bp = g_art_buf.data() + destination;
                g_art_line.copy(bp, g_art_line.size());
                bp[g_art_line.size()] = '\0';
                (void) mime_set_state(std::string_view{bp});
                if (g_mime.state == DECODE_MIME)
                {
                    g_mime.state = SKIP_MIME;
                }
            }
            else
            {
                g_mime.state = SKIP_MIME;
            }
            g_color.pop();
            chdir_news_dir();
            erase_line(false);
            g_no_wait_fork = false;
            g_first_view = g_art_input.line_num;
            g_term_line = save_term_line;
            if (g_mime.state != SKIP_MIME)
            {
                goto mime_switch;
            }
        }
        // FALL THROUGH
    }

    case SKIP_MIME:
    {
        MimeSection *mp = g_mime.section;
        while ((mp = mp->m_prev) != nullptr && !mp->m_boundary_len)
        {
        }
        if (!mp)
        {
            g_art_buf.length = g_art_buf.pos();
            g_art_size = g_art_buf.len() + g_parsed_header.positions[PAST_HEADER].min_pos;
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
            bp = nullptr;
        }
        len = 0;
        break;
    }

    case END_OF_MIME:
        if (!g_mime.at_top_level())
        {
            g_mime.state = SKIP_MIME;
        }
        else
        {
            finish_mime_article();
        }
        // FALL THROUGH

    case BETWEEN_MIME:
        len = static_cast<int>(g_multipart_separator.size()) + 1;
        if (extra_offset && filter_offset)
        {
            extra_chars = len + 1;
            len = read_offset + 1;
            o = read_offset + 1;
            g_art_buf.bytes[g_art_buf.offset(bp + o - 1)] = '\n';
        }
        else
        {
            o = -1;
            ++g_art_buf.position;
            bp++;
        }
        {
            const std::size_t separator_offset = g_art_buf.offset(bp + o);
            g_art_buf.bytes[separator_offset] = '\002';
            g_multipart_separator.copy(&g_art_buf.bytes[separator_offset + 1], g_multipart_separator.size());
            write_newline(separator_offset + g_multipart_separator.size() + 1);
        }
        break;

    case UNHANDLED_MIME:
        g_mime.state = SKIP_MIME;
        *bp++ = '\001';
        ++g_art_buf.position;
        description = g_mime.description();
        break;

    case ALTERNATE_MIME:
        g_mime.state = SKIP_MIME;
        *bp++ = '\001';
        ++g_art_buf.position;
        {
            const std::string alternative = fmt::format("[Alternative: {}]\n", *g_mime.section->m_type_name);
            alternative.copy(bp, alternative.size());
            bp[alternative.size()] = '\0';
            len = static_cast<int>(alternative.size());
        }
        break;

    case IMAGE_MIME:
    case AUDIO_MIME:
        if (!g_mime.article.m_total && !g_mime.multimedia_mime)
        {
            g_mime.multimedia_mime = true;
        }
        // FALL THROUGH

    default:
        if (view_inline && g_first_view < g_art_input.line_num //
            && (g_mime.section->m_flags & MSF_INLINE))
        {
            g_mime.state = DECODE_MIME;
        }
        else
        {
            g_mime.state = SKIP_MIME;
        }
        *bp++ = '\001';
        ++g_art_buf.position;
        description = g_mime.description();
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
            const std::size_t cp_offset = g_art_buf.offset(cp);
            const std::size_t newline_pos = g_art_buf.bytes.find('\n', cp_offset);
            const std::size_t nul_pos = g_art_buf.bytes.find('\0', cp_offset);
            if (newline_pos == std::string::npos || (nul_pos != std::string::npos && nul_pos < newline_pos))
            {
                break;
            }
            s = g_art_buf.data() + newline_pos;
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
                    // The first wrap can follow an overwide word.
                    if (t < bp + len)
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
                        const std::size_t source_offset = g_art_buf.offset(t);
                        const std::size_t dest_offset = g_art_buf.offset(t - spaces);
                        const std::size_t copy_limit = extra_chars > 0 ? static_cast<std::size_t>(extra_chars - 1) : 0;
                        const std::size_t source_end = source_offset + copy_limit;
                        const std::size_t nul_offset = g_art_buf.bytes.find('\0', source_offset);
                        const std::size_t copy_count =
                            nul_offset < source_end ? nul_offset - source_offset : copy_limit;
                        std::char_traits<char>::move(&g_art_buf.bytes[dest_offset], &g_art_buf.bytes[source_offset],
                                                     copy_count);
                        g_art_buf.bytes[dest_offset + copy_count] = '\0';
                        extra_chars -= spaces;
                        s -= spaces;
                        t -= spaces + 1;
                    }
                } while (s - (cp = t+1) > word_wrap);
            }
        }
    }
    g_art_buf.position += ArticlePosition{len};
    if (read_something)
    {
        g_art_buf.set_seek(tell_art());
        g_art_buf.length = g_art_buf.pos() + ArticlePosition{extra_chars};
        if (g_art_size >= 0)
        {
            g_art_size = g_art_buf.size_from_raw(g_raw_art_size);
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
