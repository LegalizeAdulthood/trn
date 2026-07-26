/* mime.cpp
 * vi: set sw=4 ts=8 ai sm noet:
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/mime-internal.h>

#include <config/common.h>
#include <config/env.h>
#include <config/string_case_compare.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/decode.h>
#include <trn/head.h>
#include <trn/ng.h>
#include <trn/respond.h>
#include <trn/size_cast.h>
#include <trn/string-algos.h>
#include <trn/terminal.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

MimeSection  g_mime_article{};
MimeSection *g_mime_section{&g_mime_article};
MimeState    g_mime_state{};
std::string  g_multipart_separator{"-=-=-=-=-=-"};
bool         g_auto_view_inline{};
std::string_view g_mime_getc_line;

#ifdef USE_UTF_HACK
#define CODE_POINT_MAX  0x7FFFFFFFL
#else
#define CODE_POINT_MAX  0x7F
#endif

// clang-format off
static const HtmlTag s_tag_attr[LAST_TAG] = {
    // name        flags
    {"blockquote", TF_BLOCK | TF_P | TF_NL                 },
    {"br",         TF_NL | TF_BR                           },
    {"div",        TF_BLOCK | TF_NL                        },
    {"hr",         TF_NL                                   },
    {"img",        TF_NONE                                 },
    {"li",         TF_NL                                   },
    {"ol",         TF_BLOCK | TF_P | TF_NL | TF_LIST       },
    {"p",          TF_HAS_CLOSE | TF_P | TF_NL             },
    {"pre",        TF_BLOCK | TF_P | TF_NL                 },
    {"script",     TF_BLOCK | TF_HIDE                      },
    {"style",      TF_BLOCK | TF_HIDE                      },
    {"td",         TF_TAB                                  },
    {"th",         TF_TAB                                  },
    {"tr",         TF_NL                                   },
    {"title",      TF_BLOCK | TF_HIDE                      },
    {"ul",         TF_BLOCK | TF_P | TF_NL | TF_LIST       },
    {"xml",        TF_BLOCK | TF_HIDE                      }, // non-standard but seen in the wild
};
// clang-format on
static std::vector<MimeCapEntry> s_mimecap_entries;
static MimeExecutor              s_executor;

constexpr bool CLOSING_TAG = false;
constexpr bool OPENING_TAG = true;

static std::string mime_parse_entry_arg(std::string_view &text);
static std::string mime_parse_entry_value(std::string_view text);
static int         mime_getc(std::FILE *fp);
static void        mime_init_sections();
static bool        mime_pop_section();
static std::size_t mime_skip_whitespace(std::string_view text, std::size_t pos);
static char       *tag_action(char *t, const char *word, bool opening_tag);
static char *output_prep(char *t);
static char *do_newline(char *t, HtmlFlags flag);
static int         do_indent(char *t);
static std::string_view find_attr(std::string_view str, std::string_view attr);

void mime_set_executor(MimeExecutor executor)
{
    s_executor = std::move(executor);
}

void mime_init()
{
    s_executor = do_shell;
    s_mimecap_entries.clear();

    std::string mcname = get_env_var("MIMECAPS");
    if (mcname.empty())
    {
        mcname = get_env_var("MAILCAPS", MIMECAP);
    }
    std::string_view mcname_list{mcname};
    while (!mcname_list.empty())
    {
        const std::size_t      next = mcname_list.find(':');
        const std::string_view item = mcname_list.substr(0, next);
        if (!item.empty())
        {
            mime_read_mimecap(item);
        }
        if (next == std::string_view::npos)
        {
            break;
        }
        mcname_list.remove_prefix(next + 1);
    }
}

void mime_final()
{
    s_mimecap_entries.clear();
}

void mime_read_mimecap(std::string_view mcname)
{
    std::ifstream input{file_exp(mcname)};
    if (!input)
    {
        return;
    }

    std::string entry;
    entry.reserve(2048);
    std::string line;
    while (input)
    {
        entry.clear();
        while (std::getline(input, line))
        {
            if (!line.empty() && line.front() == '#')
            {
                continue;
            }
            const bool continued = !input.eof() && !line.empty() && line.back() == '\\';
            if (continued)
            {
                line.pop_back();
            }
            entry += line;
            if (!continued)
            {
                break;
            }
        }

        std::string_view args{entry};
        while (!args.empty() && std::isspace(static_cast<unsigned char>(args.front())))
        {
            args.remove_prefix(1);
        }
        if (args.empty())
        {
            continue;
        }
        std::string content_type = mime_parse_entry_arg(args);
        if (args.empty())
        {
            fmt::print(stderr, "trn: Ignoring invalid mimecap entry: {}\n", entry);
            continue;
        }
        MimeCapEntry &mcp = s_mimecap_entries.emplace_back();
        mcp.content_type = std::move(content_type);
        mcp.command = mime_parse_entry_arg(args);
        while (!args.empty())
        {
            std::string       argument = mime_parse_entry_arg(args);
            std::string_view  name{argument};
            std::string_view  value;
            const std::size_t equal = name.find('=');
            const bool        has_value = equal != std::string_view::npos;
            if (has_value)
            {
                value = name.substr(equal + 1);
                while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
                {
                    value.remove_prefix(1);
                }
                name = name.substr(0, equal);
                while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
                {
                    name.remove_suffix(1);
                }
            }
            if (!name.empty())
            {
                if (string_case_equal(name, "needsterminal"))
                {
                    mcp.flags |= MCF_NEEDS_TERMINAL;
                }
                else if (string_case_equal(name, "copiousoutput"))
                {
                    mcp.flags |= MCF_COPIOUS_OUTPUT;
                }
                else if (has_value && string_case_equal(name, "test"))
                {
                    mcp.test_command = mime_parse_entry_value(value);
                }
                else if (has_value && (string_case_equal(name, "description") || string_case_equal(name, "label")))
                {
                    // 'label' is the legacy name for description.
                    mcp.description = mime_parse_entry_value(value);
                }
            }
        }
    }
}

static std::string mime_parse_entry_arg(std::string_view &text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
    {
        text.remove_prefix(1);
    }

    std::string result;
    result.reserve(text.size());
    while (!text.empty())
    {
        const char ch = text.front();
        text.remove_prefix(1);
        if (ch == '\\')
        {
            if (text.empty())
            {
                break;
            }
            const char escaped = text.front();
            if (escaped == '%')
            {
                result.push_back('%');
            }
            result.push_back(escaped);
            text.remove_prefix(1);
            continue;
        }
        if (ch == ';')
        {
            break;
        }
        result.push_back(ch);
    }

    while (!text.empty() && (std::isspace(static_cast<unsigned char>(text.front())) || text.front() == ';'))
    {
        text.remove_prefix(1);
    }
    while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back())))
    {
        result.pop_back();
    }
    return result;
}

static std::string mime_parse_entry_value(std::string_view text)
{
    if (text.empty() || text.front() != '"')
    {
        return std::string{text};
    }

    text.remove_prefix(1);
    std::string result;
    result.reserve(text.size());
    for (std::size_t pos = 0; pos < text.size(); pos++)
    {
        if (text[pos] == '\\' && pos + 1 < text.size() && text[pos + 1] == '"')
        {
            pos++;
        }
        else if (text[pos] == '"')
        {
            break;
        }
        result.push_back(text[pos]);
    }
    return result;
}

MimeCapEntry *mime_find_mimecap_entry(std::string_view contenttype, MimeCapFlags skip_flags)
{
    for (MimeCapEntry &mcp : s_mimecap_entries)
    {
        if (!(mcp.flags & skip_flags) //
            && mime_types_match(contenttype, mcp.content_type))
        {
            if (mcp.test_command.empty())
            {
                return &mcp;
            }
            if (mime_exec(mcp.test_command) == 0)
            {
                return &mcp;
            }
        }
    }
    return nullptr;
}

bool mime_types_match(std::string_view ct, std::string_view pat)
{
    const std::size_t slash = pat.find('/');
    const std::size_t len = slash == std::string_view::npos ? pat.size() : slash;
    const bool        iswild = slash == std::string_view::npos || pat.substr(slash + 1) == "*";

    return string_case_equal(ct, pat) ||
           (iswild && ct.size() > len && string_case_equal(ct.substr(0, len), pat.substr(0, len)) && ct[len] == '/');
}

static std::size_t mime_skip_whitespace(std::string_view text, std::size_t pos)
{
    while (pos < text.size())
    {
        if (text[pos] == '(')
        {
            int comment_level = 1;
            pos++;
            while (comment_level != 0 && pos < text.size())
            {
                switch (text[pos++])
                {
                case '\\':
                    if (pos < text.size())
                    {
                        pos++;
                    }
                    break;

                case '(':
                    comment_level++;
                    break;

                case ')':
                    comment_level--;
                    break;
                }
            }
        }
        else if (!std::isspace(static_cast<unsigned char>(text[pos])))
        {
            break;
        }
        else
        {
            pos++;
        }
    }
    return pos;
}

template <typename Params>
static std::string mime_find_param(const Params &params, std::string_view param)
{
    for (const auto &raw_param : params)
    {
        const std::string_view text{raw_param};
        std::size_t            pos = mime_skip_whitespace(text, 0);
        const std::size_t      name_begin = pos;
        while (pos < text.size() && text[pos] != ';' && text[pos] != '(' && text[pos] != '=' &&
               !std::isspace(static_cast<unsigned char>(text[pos])))
        {
            pos++;
        }
        const std::string_view name = text.substr(name_begin, pos - name_begin);
        pos = mime_skip_whitespace(text, pos);
        if (name.size() != param.size() || pos == text.size() || text[pos] != '=' ||
            !string_case_equal(name, param))
        {
            continue;
        }

        pos = mime_skip_whitespace(text, pos + 1);
        std::string value;
        if (pos < text.size() && text[pos] == '"')
        {
            pos++;
            value.reserve(text.size() - pos);
            while (pos < text.size() && text[pos] != '"')
            {
                if (text[pos] == '\\' && pos + 1 < text.size() && text[pos + 1] == '"')
                {
                    pos++;
                }
                value.push_back(text[pos++]);
            }
        }
        else
        {
            const std::size_t value_begin = pos;
            while (pos < text.size() && text[pos] != ';' && text[pos] != '(' &&
                   !std::isspace(static_cast<unsigned char>(text[pos])))
            {
                pos++;
            }
            value.assign(text.substr(value_begin, pos - value_begin));
        }
        return value;
    }
    return {};
}

int mime_exec(std::string_view cmd)
{
    std::string command;
    command.reserve(CMD_BUF_LEN);

    for (std::size_t pos = 0; pos < cmd.size(); pos++)
    {
        if (cmd[pos] == '%')
        {
            pos++;
            if (pos >= cmd.size())
            {
                continue;
            }
            switch (cmd[pos])
            {
            case 's':
                command += g_decode_filename;
                break;

            case 't':
                command += '\'';
                command += *g_mime_section->m_type_name;
                command += '\'';
                break;

            case '{':
            {
                const std::size_t param_begin = pos + 1;
                const std::size_t param_end = cmd.find('}', param_begin);
                if (param_end == std::string_view::npos)
                {
                    return -1;
                }
                command += '\'';
                command +=
                    mime_find_param(g_mime_section->m_type_params, cmd.substr(param_begin, param_end - param_begin));
                pos = param_end;
                command += '\'';
                break;
            }

            case '%':
                command += '%';
                break;

            case 'n':
            case 'F':
                return -1;
            }
        }
        else
        {
            command += cmd[pos];
        }
    }

    return s_executor(SH, command.c_str());
}

static void mime_init_sections()
{
    while (mime_pop_section())
    {
    }
    g_mime_section->mime_clear_struct();
    g_mime_state = NOT_MIME;
}

void mime_push_section()
{
    MimeSection* mp = new MimeSection{};
    mp->m_prev = g_mime_section;
    g_mime_section = mp;
}

static bool mime_pop_section()
{
    MimeSection* mp = g_mime_section->m_prev;
    if (mp)
    {
        g_mime_section->mime_clear_struct();
        delete g_mime_section;
        g_mime_section = mp;
        g_mime_state = g_mime_section->m_type;
        return true;
    }
    g_mime_state = g_mime_article.m_type;
    return false;
}

// Free up this mime structure's resources
void MimeSection::mime_clear_struct()
{
    m_filename.reset();
    m_type_name.reset();
    m_type_params.clear();
    m_boundary.reset();
    m_html_blocks.clear();
    m_html_tag_word.clear();
    m_type = NOT_MIME;
    m_encoding = MENCODE_NONE;
    m_total = 0;
    m_part = 0;
    m_boundary_len = 0;
    m_flags = MFS_NONE;
    m_html = HF_NONE;
    m_html_line_start = 0;
}

// Setup g_mime_article structure based on article's headers
void mime_set_article()
{
    mime_init_sections();
    // TODO: Check mime version #?
    g_multimedia_mime = false;
    g_is_mime = g_header_type[MIME_VER_LINE].flags & HT_MAGIC
            && g_header_type[MIME_VER_LINE].min_pos >= 0;

    {
        std::string s = fetch_lines(g_art, CONT_TYPE_LINE);
        g_mime_section->mime_parse_type(s);
    }

    if (g_is_mime)
    {
        std::string s = fetch_lines(g_art, CONT_XFER_LINE);
        g_mime_section->mime_parse_encoding(s);

        s = fetch_lines(g_art, CONT_DISP_LINE);
        g_mime_section->mime_parse_disposition(s);

        g_mime_state = g_mime_section->m_type;
        if (g_mime_state == NOT_MIME
         || (g_mime_state == TEXT_MIME && g_mime_section->m_encoding == MENCODE_NONE))
        {
            g_is_mime = false;
        }
        else if (!g_mime_section->m_type_name)
        {
            g_mime_section->m_type_name = "text/plain";
        }
    }
}

// Use the Content-Type to set values in the mime structure
void MimeSection::mime_parse_type(std::string_view text)
{
    m_type_name.reset();
    m_type_params.clear();

    const MimeParamViews parsed = mime_parse_params(text);
    m_type_params.reserve(parsed.params.size());
    for (const std::string_view param : parsed.params)
    {
        m_type_params.emplace_back(param);
    }
    if (parsed.value.empty())
    {
        m_type = NOT_MIME;
        return;
    }
    m_type_name = parsed.value;
    const char *s = m_type_name->c_str();
    std::string t = mime_find_param(m_type_params, "name");
    if (!t.empty())
    {
        m_filename = t;
    }

    if (string_case_equal(s, "text", 4))
    {
        m_type = TEXT_MIME;
        s += 4;
        if (*s++ != '/')
        {
            return;
        }
#ifdef USE_UTF_HACK
        t = mime_find_param(m_type_params, "charset");
        utf_init(t, CHARSET_NAME_UTF8); // FIXME
#endif
        if (string_case_equal(s, "html", 4))
        {
            m_type = HTML_TEXT_MIME;
        }
        else if (string_case_equal(s, "x-vcard", 7))
        {
            m_type = UNHANDLED_MIME;
        }
        return;
    }

    if (string_case_equal(s, "message/", 8))
    {
        s += 8;
        m_type = MESSAGE_MIME;
        if (string_case_equal(s, "partial"))
        {
            t = mime_find_param(m_type_params, "id");
            if (t.empty())
            {
                return;
            }
            m_filename = t;
            t = mime_find_param(m_type_params, "number");
            if (!t.empty())
            {
                m_part = (short) std::atoi(t.c_str());
            }
            t = mime_find_param(m_type_params, "total");
            if (!t.empty())
            {
                m_total = (short) std::atoi(t.c_str());
            }
            if (!m_total)
            {
                m_part = 0;
                return;
            }
            return;
        }
        return;
    }

    if (string_case_equal(s, "multipart/", 10))
    {
        s += 10;
        t = mime_find_param(m_type_params, "boundary");
        if (t.empty())
        {
            m_type = UNHANDLED_MIME;
            return;
        }
        if (string_case_equal(s, "alternative", 11))
        {
            m_flags |= MSF_ALTERNATIVE;
        }
        m_boundary = t;
        m_boundary_len = static_cast<short>(t.size());
        m_type = MULTIPART_MIME;
        return;
    }

    if (string_case_equal(s, "image/", 6))
    {
        m_type = IMAGE_MIME;
        return;
    }

    if (string_case_equal(s, "audio/", 6))
    {
        m_type = AUDIO_MIME;
        return;
    }

    m_type = UNHANDLED_MIME;
}

// Use the Content-Disposition to set values in the mime structure
void MimeSection::mime_parse_disposition(std::string_view text)
{
    const MimeParamViews parsed = mime_parse_params(text);
    if (string_case_equal(parsed.value, "inline"))
    {
        m_flags |= MSF_INLINE;
    }

    const std::string filename = mime_find_param(parsed.params, "filename");
    if (!filename.empty())
    {
        m_filename = filename;
    }
}

// Use the Content-Transfer-Encoding to set values in the mime structure
void MimeSection::mime_parse_encoding(std::string_view text)
{
    const std::size_t token_begin = mime_skip_whitespace(text, 0);
    std::string_view  token = text.substr(token_begin);
    const auto        consume_token = [&token](std::string_view prefix)
    {
        if (token.size() < prefix.size() || !string_case_equal(token.substr(0, prefix.size()), prefix))
        {
            return false;
        }
        token.remove_prefix(prefix.size());
        return true;
    };
    if (token.empty())
    {
        m_encoding = MENCODE_NONE;
        return;
    }
    if (token.front() == '7' || token.front() == '8')
    {
        if (token.size() >= 4 && string_case_equal(token.substr(1, 3), "bit"))
        {
            token.remove_prefix(4);
            m_encoding = MENCODE_NONE;
        }
    }
    else if (consume_token("quoted-printable"))
    {
        m_encoding = MENCODE_QPRINT;
    }
    else if (consume_token("binary"))
    {
        m_encoding = MENCODE_NONE;
    }
    else if (consume_token("base64"))
    {
        m_encoding = MENCODE_BASE64;
    }
    else if (consume_token("x-uue"))
    {
        m_encoding = MENCODE_UUE;
        consume_token("ncode");
    }
    else
    {
        m_encoding = MENCODE_UNHANDLED;
        return;
    }
    if (!token.empty() && !std::isspace(static_cast<unsigned char>(token.front())) && token.front() != ';' &&
        token.front() != '(')
    {
        m_encoding = MENCODE_UNHANDLED;
    }
}

// Parse a multipart mime header and affect the *g_mime_section structure

void mime_parse_sub_header(std::FILE *ifp, std::string_view first_line)
{
    std::string line;
    line.reserve(2 * LINE_BUF_LEN);
    line.assign(first_line);
    std::string input_line;
    input_line.reserve(LINE_BUF_LEN);
    std::size_t next_pos = 0;

    g_mime_section->mime_clear_struct();
    g_mime_section->m_type = TEXT_MIME;
    while (true)
    {
        line.erase(0, next_pos);
        next_pos = std::string::npos;
        std::size_t header_end = std::string::npos;
        for (std::size_t pos = 0;; pos = line.size())
        {
            if (pos == line.size())
            {
                input_line.clear();
                if (ifp != nullptr)
                {
                    input_line = get_a_line(ifp);
                    if (input_line.empty())
                    {
                        break;
                    }
                }
                else if (!read_art(input_line))
                {
                    break;
                }
                const std::size_t input_end = input_line.find('\0');
                input_line.resize(input_end == std::string::npos ? input_line.size() : input_end);
                if (input_line.empty())
                {
                    continue;
                }
                line += input_line;
            }
            if (line.front() == '\n')
            {
                break;
            }
            if (pos && !is_hor_space(line[pos]))
            {
                next_pos = pos;
                header_end = pos - 1;
                break;
            }
        }
        const std::string_view header_line{line.data(), header_end == std::string::npos ? line.size() : header_end};
        const std::size_t      colon = header_line.find(':');
        if (colon == std::string_view::npos)
        {
            break;
        }

        int linetype = set_line_type(header_line.substr(0, colon));
        switch (linetype)
        {
        case CONT_TYPE_LINE:
            g_mime_section->mime_parse_type(header_line.substr(colon + 1));
            break;

        case CONT_XFER_LINE:
            g_mime_section->mime_parse_encoding(header_line.substr(colon + 1));
            break;

        case CONT_DISP_LINE:
            g_mime_section->mime_parse_disposition(header_line.substr(colon + 1));
            break;

        case CONT_NAME_LINE:
        {
            const std::string_view content_name = header_line.substr(colon + 1);
            g_mime_section->m_filename =
                std::string{content_name.substr(mime_skip_whitespace(content_name, 0))};
            break;
        }
        }
    }
    g_mime_state = g_mime_section->m_type;
    if (!g_mime_section->m_type_name)
    {
        g_mime_section->m_type_name = "text/plain";
    }
}

void mime_set_state(char *bp)
{
    if (g_mime_state == BETWEEN_MIME)
    {
        mime_parse_sub_header(nullptr, bp);
        *bp = '\0';
        if (g_mime_section->m_prev->m_flags & MSF_ALTERNADONE)
        {
            g_mime_state = ALTERNATE_MIME;
        }
        else if (g_mime_section->m_prev->m_flags & MSF_ALTERNATIVE)
        {
            g_mime_section->m_prev->m_flags |= MSF_ALTERNADONE;
        }
    }

    while (g_mime_state == MESSAGE_MIME)
    {
        mime_push_section();
        mime_parse_sub_header(nullptr, bp);
        *bp = '\0';
    }

    if (g_mime_state == MULTIPART_MIME)
    {
        mime_push_section();
        g_mime_state = SKIP_MIME;               // Skip anything before 1st part
    }

    int ret = mime_end_of_section(bp);
    switch (ret)
    {
    case 0:
        break;

    case 1:
        while (!g_mime_section->m_prev->m_boundary_len)
        {
            mime_pop_section();
        }
        g_mime_state = BETWEEN_MIME;
        break;

    case 2:
        while (!g_mime_section->m_prev->m_boundary_len)
        {
            mime_pop_section();
        }
        mime_pop_section();
        g_mime_state = END_OF_MIME;
        break;
    }
}

void mime_set_state(std::string &bp)
{
    if (bp.empty())
    {
        bp.resize(1);
        bp[0] = '\0';
    }
    mime_set_state(bp.data());
    const std::size_t terminator = bp.find('\0');
    bp.resize(terminator == std::string::npos ? bp.size() : terminator);
}

int mime_end_of_section(std::string_view bp)
{
    MimeSection *mp = g_mime_section->m_prev;
    while (mp && !mp->m_boundary_len)
    {
        mp = mp->m_prev;
    }
    if (mp)
    {
        // have we read all the data in this part?
        const std::string_view boundary{*mp->m_boundary};
        const std::size_t      marker_len = 2 + boundary.size();
        if (bp.size() >= marker_len && bp[0] == '-' && bp[1] == '-' && bp.substr(2, boundary.size()) == boundary)
        {
            const std::string_view rest = bp.substr(marker_len);
            // have we found the last boundary?
            if (rest.size() >= 2 && rest[0] == '-' && rest[1] == '-' && (rest.size() == 2 || rest[2] == '\n'))
            {
                return 2;
            }
            return rest.empty() || rest[0] == '\n';
        }
    }
    return 0;
}

MimeParamViews mime_parse_params(std::string_view text)
{
    MimeParamViews    result;
    std::size_t       pos = mime_skip_whitespace(text, 0);
    const std::size_t value_begin = pos;
    while (pos < text.size() && text[pos] != ';' && text[pos] != '(' &&
           !std::isspace(static_cast<unsigned char>(text[pos])))
    {
        pos++;
    }
    result.value = text.substr(value_begin, pos - value_begin);
    pos = mime_skip_whitespace(text, pos);

    while (pos < text.size() && text[pos] == ';')
    {
        pos = mime_skip_whitespace(text, pos + 1);
        const std::size_t param_begin = pos;
        bool              quoted = false;
        int               comment_level = 0;
        while (pos < text.size())
        {
            if ((quoted || comment_level != 0) && text[pos] == '\\' && pos + 1 < text.size())
            {
                pos += 2;
                continue;
            }
            if (comment_level != 0)
            {
                if (text[pos] == '(')
                {
                    comment_level++;
                }
                else if (text[pos] == ')')
                {
                    comment_level--;
                }
                pos++;
                continue;
            }
            if (text[pos] == '"')
            {
                quoted = !quoted;
            }
            else if (!quoted && text[pos] == '(')
            {
                comment_level = 1;
            }
            else if (!quoted && text[pos] == ';')
            {
                break;
            }
            pos++;
        }
        std::size_t param_end = pos;
        while (param_end > param_begin && std::isspace(static_cast<unsigned char>(text[param_end - 1])))
        {
            param_end--;
        }
        result.params.emplace_back(text.substr(param_begin, param_end - param_begin));
    }
    return result;
}

void mime_decode_article(bool view)
{
    MimeCapEntry* mcp = nullptr;

    seek_art(g_save_from);
    g_art_line.clear();

    while (true)
    {
        if (g_mime_state != MESSAGE_MIME || !g_mime_section->m_total)
        {
            if (!read_art(g_art_line))
            {
                break;
            }
            mime_set_state(g_art_line);
        }
        switch (g_mime_state)
        {
        case BETWEEN_MIME:
        case END_OF_MIME:
            break;

        case TEXT_MIME:
        case HTML_TEXT_MIME:
        case ISO_TEXT_MIME:
        case MESSAGE_MIME:
            // TODO: Check for uuencoded file here?
            g_mime_state = SKIP_MIME;
            // FALL THROUGH

        case SKIP_MIME:
        {
            MimeSection* mp = g_mime_section;
            while ((mp = mp->m_prev) != nullptr && !mp->m_boundary_len)
            {
            }
            if (!mp)
            {
                return;
            }
            break;
        }

        default:
            if (view)
            {
                mcp = mime_find_mimecap_entry(*g_mime_section->m_type_name, MCF_NONE);
                if (!mcp)
                {
                    fmt::print("No view method for {} -- skipping.\n", *g_mime_section->m_type_name);
                    g_mime_state = SKIP_MIME;
                    break;
                }
            }
            g_mime_state = DECODE_MIME;
            if (decode_piece(mcp,
                             g_art_line.empty() || g_art_line.front() == '\n'
                                 ? std::string_view{}
                                 : std::string_view{g_art_line}))
            {
                mime_set_state(g_art_line);
                if (g_mime_state == DECODE_MIME)
                {
                    g_mime_state = SKIP_MIME;
                }
            }
            else
            {
                if (!g_msg.empty())
                {
                    newline();
                    std::fputs(g_msg.c_str(),stdout);
                }
                g_mime_state = SKIP_MIME;
            }
            newline();
            break;
        }
    }
}

std::string MimeSection::mime_description() const
{
    return fmt::format("[Attachment type={}, name={}]\n", *m_type_name,
                       decode_fix_filename(m_filename ? *m_filename : "unknown"));
}

#define XX 255
static Uchar s_index_hex[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,10,11,12, 13,14,15,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

int qp_decode_string(char *t, const char *f, bool in_header)
{
    char* save_t = t;
    while (*f)
    {
        switch (*f)
        {
        case '_':
            if (in_header)
            {
                *t++ = ' ';
                f++;
            }
            else
            {
                *t++ = *f++;
            }
            break;

        case '=':     // decode a hex-value
            if (f[1] == '\n')
            {
                f += 2;
                break;
            }
            if (s_index_hex[(Uchar) f[1]] != XX && s_index_hex[(Uchar) f[2]] != XX)
            {
                *t = (s_index_hex[(Uchar)f[1]] << 4) + s_index_hex[(Uchar)f[2]];
                f += 3;
                if (*t != '\r')
                {
                    t++;
                }
                break;
            }
            // FALL THROUGH

        default:
            *t++ = *f++;
            break;
        }
    }
    *t = '\0';
    return t - save_t;
}

DecodeState qp_decode(std::FILE *ifp, DecodeState state)
{
    static std::FILE* ofp = nullptr;
    int c1;

    if (state == DECODE_DONE)
    {
        if (ofp)
        {
            std::fclose(ofp);
        }
        ofp = nullptr;
        return state;
    }

    if (state == DECODE_START)
    {
        const std::string filename =
            decode_fix_filename(g_mime_section->m_filename ? *g_mime_section->m_filename : "unknown");
        g_decode_filename = filename;
        ofp = std::fopen(filename.c_str(), "wb");
        if (!ofp)
        {
            return DECODE_ERROR;
        }
        erase_line(false);
        std::printf("Decoding %s", filename.c_str());
        if (g_no_wait_fork)
        {
            std::fflush(stdout);
        }
        else
        {
            newline();
        }
    }

    while ((c1 = mime_getc(ifp)) != EOF)
    {
check_c1:
        if (c1 == '=')
        {
            c1 = mime_getc(ifp);
            if (c1 == '\n')
            {
                continue;
            }
            if (s_index_hex[(Uchar) c1] == XX)
            {
                std::putc('=', ofp);
                goto check_c1;
            }
            int c2 = mime_getc(ifp);
            if (s_index_hex[(Uchar) c2] == XX)
            {
                std::putc('=', ofp);
                std::putc(c1, ofp);
                c1 = c2;
                goto check_c1;
            }
            c1 = s_index_hex[(Uchar)c1] << 4 | s_index_hex[(Uchar)c2];
            if (c1 != '\r')
            {
                std::putc(c1, ofp);
            }
        }
        else
        {
            std::putc(c1, ofp);
        }
    }

    return DECODE_MAYBE_DONE;
}

static Uchar s_index_b64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,XX,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};

int b64_decode_string(char *t, const char *f)
{
    char* save_t = t;
    Uchar ch2;

    while (*f && *f != '=')
    {
        Uchar ch1 = s_index_b64[(Uchar)*f++];
        if (ch1 == XX)
        {
            continue;
        }
        do
        {
            if (!*f || *f == '=')
            {
                goto dbl_break;
            }
            ch2 = s_index_b64[(Uchar)*f++];
        } while (ch2 == XX);
        *t++ = ch1 << 2 | ch2 >> 4;
        do
        {
            if (!*f || *f == '=')
            {
                goto dbl_break;
            }
            ch1 = s_index_b64[(Uchar)*f++];
        } while (ch1 == XX);
        *t++ = (ch2 & 0x0f) << 4 | ch1 >> 2;
        do
        {
            if (!*f || *f == '=')
            {
                goto dbl_break;
            }
            ch2 = s_index_b64[(Uchar)*f++];
        } while (ch2 == XX);
        *t++ = (ch1 & 0x03) << 6 | ch2;
    }
dbl_break:
    *t = '\0';
    return t - save_t;
}

DecodeState b64_decode(std::FILE *ifp, DecodeState state)
{
    static std::FILE *ofp = nullptr;
    int          c1;
    int          c2;
    int          c3;
    int          c4;

    if (state == DECODE_DONE)
    {
all_done:
        if (ofp)
        {
            std::fclose(ofp);
        }
        ofp = nullptr;
        return state;
    }

    if (state == DECODE_START)
    {
        const std::string filename =
            decode_fix_filename(g_mime_section->m_filename ? *g_mime_section->m_filename : "unknown");
        g_decode_filename = filename;
        ofp = std::fopen(filename.c_str(), "wb");
        if (!ofp)
        {
            return DECODE_ERROR;
        }
        std::printf("Decoding %s", filename.c_str());
        if (g_no_wait_fork)
        {
            std::fflush(stdout);
        }
        else
        {
            newline();
        }
        state = DECODE_ACTIVE;
    }

    while ((c1 = mime_getc(ifp)) != EOF)
    {
        if (c1 != '=' && s_index_b64[c1] == XX)
        {
            continue;
        }
        do
        {
            c2 = mime_getc(ifp);
            if (c2 == EOF)
            {
                return state;
            }
        } while (c2 != '=' && s_index_b64[c2] == XX);
        do
        {
            c3 = mime_getc(ifp);
            if (c3 == EOF)
            {
                return state;
            }
        } while (c3 != '=' && s_index_b64[c3] == XX);
        do
        {
            c4 = mime_getc(ifp);
            if (c4 == EOF)
            {
                return state;
            }
        } while (c4 != '=' && s_index_b64[c4] == XX);
        if (c1 == '=' || c2 == '=')
        {
            state = DECODE_DONE;
            break;
        }
        c1 = s_index_b64[c1];
        c2 = s_index_b64[c2];
        c1 = c1 << 2 | c2 >> 4;
        std::putc(c1, ofp);
        if (c3 == '=')
        {
            state = DECODE_DONE;
            break;
        }
        c3 = s_index_b64[c3];
        c2 = (c2 & 0x0f) << 4 | c3 >> 2;
        std::putc(c2, ofp);
        if (c4 == '=')
        {
            state = DECODE_DONE;
            break;
        }
        c4 = s_index_b64[c4];
        c3 = (c3 & 0x03) << 6 | c4;
        std::putc(c3, ofp);
    }

    if (state == DECODE_DONE)
    {
        goto all_done;
    }

    return DECODE_MAYBE_DONE;
}

static int mime_getc(std::FILE *fp)
{
    if (fp)
    {
        return std::fgetc(fp);
    }

    if (g_mime_getc_line.empty())
    {
        if (!read_art(g_art_line))
        {
            return EOF;
        }
        if (mime_end_of_section(g_art_line))
        {
            return EOF;
        }
        g_mime_getc_line = g_art_line;
    }
    const char ch = g_mime_getc_line.front();
    g_mime_getc_line.remove_prefix(1);
    return ch;
}

DecodeState cat_decode(std::FILE *ifp, DecodeState state)
{
    static std::FILE* ofp = nullptr;

    if (state == DECODE_DONE)
    {
        if (ofp)
        {
            std::fclose(ofp);
        }
        ofp = nullptr;
        return state;
    }

    if (state == DECODE_START)
    {
        const std::string filename =
            decode_fix_filename(g_mime_section->m_filename ? *g_mime_section->m_filename : "unknown");
        g_decode_filename = filename;
        ofp = std::fopen(filename.c_str(), "wb");
        if (!ofp)
        {
            return DECODE_ERROR;
        }
        fmt::print("Decoding {}", filename);
        if (g_no_wait_fork)
        {
            std::fflush(stdout);
        }
        else
        {
            newline();
        }
    }

    std::string line;
    line.reserve(LINE_BUF_LEN);
    if (ifp)
    {
        while (!(line = get_a_line(ifp)).empty())
        {
            line.resize(std::min(line.find('\0'), line.size()));
            fmt::print(ofp, "{}", line);
        }
    }
    else
    {
        while (read_art(line))
        {
            if (mime_end_of_section(line))
            {
                break;
            }
            fmt::print(ofp, "{}", line);
        }
    }

    return DECODE_MAYBE_DONE;
}

static int s_word_wrap_in_pre{};
static int s_normal_word_wrap{};
static int s_word_wrap{};

struct NamedEntity
{
    std::string_view name;
    std::string_view replacement;
};

// clang-format off
static constexpr NamedEntity s_named_entities[] = {
    {"lt",       "<"},
    {"gt",       ">"},
    {"amp",      "&"},
    {"quot",     "\""},
    {"apo",      "'"},    // non-standard but seen in the wild
#ifndef USE_UTF_HACK
    {"nbsp",     " "},
    {"ensp",     " "},    // seen in the wild
    {"lsquo",    "'"},
    {"rsquo",    "'"},
    {"ldquo",    "\""},
    {"rdquo",    "\""},
    {"ndash",    "-"},
    {"mdash",    "-"},
    {"copy",     "(C)"},
    {"trade",    "(TM)"},
    {"zwsp",     ""},
    {"zwnj",     ""},
    {"ccedil",   "c"},    // per charsubst.cpp
    {"eacute",   "e"},
#else // USE_UTF_HACK
    {"nbsp",     "\xC2\xA0"},
    {"ensp",     "\xE2\x80\x82"},    // U+2002
    {"lsquo",    "\xE2\x80\x98"},
    {"rsquo",    "\xE2\x80\x99"},
    {"ldquo",    "\xE2\x80\x9C"},
    {"rdquo",    "\xE2\x80\x9D"},
    {"ndash",    "\xE2\x80\x93"},
    {"mdash",    "\xE2\x80\x94"},
    {"copy",     "\xC2\xA9"},
    {"trade",    "\xE2\x84\xA2"},
    {"zwsp",     "\xE2\x80\x8B"},
    {"zwnj",     "\xE2\x80\x8C"},
    {"ccedil",   "\xC3\xA7"},
    {"eacute",   "\xC3\xA9"},
#endif
};
// clang-format on

static bool named_entity_matches(const char *text, std::string_view name)
{
    for (std::size_t index = 0; index < name.size(); index++)
    {
        if (text[index] == '\0' ||
            static_cast<char>(std::tolower(static_cast<unsigned char>(text[index]))) != name[index])
        {
            return false;
        }
    }
    return true;
}

int filter_html(char *t, const char *f)
{
    char        *bp;
    char        *cp;
    std::string &tag_word = g_mime_section->m_html_tag_word;

    if (g_word_wrap_offset < 0)
    {
        s_normal_word_wrap = g_tc_COLS - 8;
        s_word_wrap_in_pre = 0;
    }
    else
    {
        s_normal_word_wrap = g_tc_COLS - g_word_wrap_offset;
        s_word_wrap_in_pre = s_normal_word_wrap;
    }

    if (s_normal_word_wrap <= 20)
    {
        s_normal_word_wrap = 0;
    }
    if (s_word_wrap_in_pre <= 20)
    {
        s_word_wrap_in_pre = 0;
    }
    s_word_wrap = g_mime_section->m_html & HF_IN_PRE? s_word_wrap_in_pre
                                                : s_normal_word_wrap;
    if (!g_mime_section->m_html_line_start)
    {
        g_mime_section->m_html_line_start = t - g_art_buf;
    }

    for (bp = t; *f; f++)
    {
        if (g_mime_section->m_html & HF_IN_DQUOTE)
        {
            if (*f == '"')
            {
                g_mime_section->m_html &= ~HF_IN_DQUOTE;
            }
            else
            {
                tag_word += *f;
            }
        }
        else if (g_mime_section->m_html & HF_IN_SQUOTE)
        {
            if (*f == '\'')
            {
                g_mime_section->m_html &= ~HF_IN_SQUOTE;
            }
            else
            {
                tag_word += *f;
            }
        }
        else if (g_mime_section->m_html & HF_IN_TAG)
        {
            if (*f == '>')
            {
                g_mime_section->m_html &= ~(HF_IN_TAG | HF_IN_COMMENT);
                if (!tag_word.empty() && tag_word.front() == '/')
                {
                    t = tag_action(t, tag_word.c_str() + 1, CLOSING_TAG);
                }
                else
                {
                    t = tag_action(t, tag_word.c_str(), OPENING_TAG);
                }
                tag_word.clear();
            }
            else if (*f == '-' && f[1] == '-')
            {
                f++;
                g_mime_section->m_html |= HF_IN_COMMENT;
            }
            else if (*f == '"')
            {
                g_mime_section->m_html |= HF_IN_DQUOTE;
            }
            else if (*f == '\'')
            {
                g_mime_section->m_html |= HF_IN_SQUOTE;
            }
            else
            {
                tag_word += at_grey_space(f) ? ' ' : *f;
            }
        }
        else if (g_mime_section->m_html & HF_IN_COMMENT)
        {
            if (*f == '-' && f[1] == '-')
            {
                f++;
                g_mime_section->m_html &= ~HF_IN_COMMENT;
            }
        }
        else if (*f == '<')
        {
            tag_word.clear();
            tag_word.reserve(32);
            g_mime_section->m_html |= HF_IN_TAG;
        }
        else if (g_mime_section->m_html & HF_IN_HIDING)
        {
        }
        else if (*f == '&' && f[1] == '#')
        {
            long int ncr = 0;
            int ncr_found = 0;
            int is_hex = f[2] == 'x';
            int base = is_hex? 16: 10;
            int i;
            for (i = 0;; i++)
            {
                int c = f[2 + is_hex + i];
                int v = s_index_hex[c];
                if (c == '\0' || v == XX || v > base)
                {
                    break;
                }
                ncr *= base;
                ncr += v;
            }
            if (i)
            {
                char det = f[2 + is_hex + i];
                if (det == ';')
                {
                    ncr_found = 2 + is_hex + i;
                }
                else if (!(det == '-' || std::isalnum(det))) // see html-spec.txt 3.2.1
                {
                    ncr_found = 1 + is_hex + i;
                }
            }
            if (ncr_found && ncr <= CODE_POINT_MAX)
            {
                if (ncr)
                {
                    t += insert_unicode_at(t, ncr);
                }
                f += ncr_found;
            }
            else
            {
                *t++ = *f;
            }
        }
        else if (*f == '&' && std::isalpha(f[1])) // see html-spec.txt 3.2.1
        {
            int              entity_found = 0;
            std::string_view entity_replacement;
            t = output_prep(t);
            for (const NamedEntity &entity : s_named_entities)
            {
                const int n = static_cast<int>(entity.name.size());
                if (named_entity_matches(f + 1, entity.name))
                {
                    char det = f[n + 1];
                    if (det == ';')
                    {
                        entity_found = n + 1;
                    }
                    else if (!(det == '-' || std::isalnum(static_cast<unsigned char>(det)))) // see html-spec.txt 3.2.1
                    {
                        entity_found = n;
                    }
                }
                if (entity_found)
                {
                    entity_replacement = entity.replacement;
                    break;
                }
            }
            if (entity_found)
            {
                for (const char c : entity_replacement)
                {
                    *t++ = c;
                }
                f += entity_found;
            }
            else
            {
                *t++ = *f;
            }
            g_mime_section->m_html |= HF_NL_OK|HF_P_OK|HF_SPACE_OK;
        }
        else if ((*f == ' ' || at_grey_space(f)) && !(g_mime_section->m_html & HF_IN_PRE))
        {
            // We don't want to call output_prep() here.
            if (*f == ' ' || (g_mime_section->m_html & HF_SPACE_OK))
            {
                g_mime_section->m_html &= ~HF_SPACE_OK;
                *t++ = ' ';
            }
            // In non-PRE mode spaces should be collapsed
            while (true)
            {
                int w = byte_length_at(f);
                if (w == 0 || f[w] == '\0' || !(f[w] == ' ' || at_grey_space(f+w)))
                {
                    break;
                }
                f += w;
            }
        }
        else if (*f == '\n')   // Handle the HF_IN_PRE case
        {
            t = output_prep(t);
            g_mime_section->m_html |= HF_NL_OK;
            t = do_newline(t, HF_NL_OK);
        }
        else
        {
            int w = byte_length_at(f);
            t = output_prep(t);
            for (int i = 0; i < w; i++)
            {
                *t++ = *f++;
            }
            f--;
            g_mime_section->m_html |= HF_NL_OK|HF_P_OK|HF_SPACE_OK;
        }

        if (s_word_wrap && t - g_art_buf - g_mime_section->m_html_line_start > g_tc_COLS)
        {
            char* line_start = g_mime_section->m_html_line_start + g_art_buf;
            for (cp = line_start + s_word_wrap;
                 cp > line_start && !is_hor_space(*cp);
                 cp--)
            {
            }
            if (cp == line_start)
            {
                for (cp = line_start + s_word_wrap;
                     cp - line_start <= g_tc_COLS && !is_hor_space(*cp);
                     cp++)
                {
                }
                if (cp - line_start > g_tc_COLS)
                {
                    g_mime_section->m_html_line_start += g_tc_COLS;
                    cp = nullptr;
                }
            }
            if (cp)
            {
                const HtmlFlags flag_save = g_mime_section->m_html;
                g_mime_section->m_html |= HF_NL_OK;
                line_start = do_newline(cp, HF_NL_OK);
                int fudge = do_indent(nullptr);
                cp = skip_hor_space(line_start);
                if ((fudge -= cp - line_start) != 0)
                {
                    if (fudge < 0)
                    {
                        if (t - cp > 0)
                        {
                            std::memcpy(cp + fudge, cp, t - cp);
                        }
                    }
                    else
                    {
                        for (char *s = t; s-- != cp;)
                        {
                            s[fudge] = *s;
                        }
                    }
                    (void) do_indent(line_start);
                    t += fudge;
                }
                g_mime_section->m_html = flag_save;
            }
        }
    }
    *t = '\0';

    return t - bp;
}
#undef XX

static constexpr char s_letters[2] = {'a', 'A'};
static constexpr int  s_roman_values[] = {1000, 500, 100, 50, 10, 5, 1};

static char *tag_action(char *t, const char *word, bool opening_tag)
{
    int   j;
    int   tnum;
    int   itype;
    int   cnt;
    int   num;
    char  ch;
    bool match = false;
    std::string_view roman_letters;
    std::vector<HtmlBlock> &blks = g_mime_section->m_html_blocks;

    const char *tmp;
    for (tmp = word; *tmp && *tmp != ' '; tmp++)
    {
    }
    const std::string_view tag_name{word, static_cast<std::size_t>(tmp - word)};

    if (tag_name.empty() || !std::isalpha(static_cast<unsigned char>(tag_name.front())))
    {
        return t;
    }
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(tag_name.front())));
    for (tnum = 0; tnum < LAST_TAG && s_tag_attr[tnum].name.front() != ch; tnum++)
    {
    }
    for (; tnum < LAST_TAG && s_tag_attr[tnum].name.front() == ch; tnum++)
    {
        if (string_case_equal(tag_name, s_tag_attr[tnum].name))
        {
            match = true;
            break;
        }
    }
    if (!match)
    {
        return t;
    }

    if (!opening_tag && !(s_tag_attr[tnum].flags & (TF_BLOCK|TF_HAS_CLOSE)))
    {
        return t;
    }

    if (g_mime_section->m_html & HF_IN_HIDING
     && (opening_tag || blks.empty() || tnum != blks.back().tag_num))
    {
        return t;
    }

    if (s_tag_attr[tnum].flags & TF_BR)
    {
        g_mime_section->m_html |= HF_NL_OK;
    }

    if (opening_tag)
    {
        if (s_tag_attr[tnum].flags & TF_NL)
        {
            t = output_prep(t);
            t = do_newline(t, HF_NL_OK);
        }
        if ((num = s_tag_attr[tnum].flags & (TF_P | TF_LIST)) == TF_P //
            || (num == (TF_P | TF_LIST) && !(g_mime_section->m_html & HF_COMPACT)))
        {
            t = output_prep(t);
            t = do_newline(t, HF_P_OK);
        }
        if (s_tag_attr[tnum].flags & TF_SPACE)
        {
            if (g_mime_section->m_html & HF_SPACE_OK)
            {
                g_mime_section->m_html &= ~HF_SPACE_OK;
                *t++ = ' ';
            }
        }
        if (s_tag_attr[tnum].flags & TF_TAB)
        {
            if (g_mime_section->m_html & HF_NL_OK)
            {
                g_mime_section->m_html &= ~HF_SPACE_OK;
                *t++ = '\t';
            }
        }

        if (s_tag_attr[tnum].flags & TF_BLOCK)
        {
            blks.push_back({tnum, 0, 0});
            j = size_cast<int>(blks) - 1;

            if (s_tag_attr[tnum].flags & TF_LIST)
            {
                g_mime_section->m_html |= HF_COMPACT;
            }
            else
            {
                g_mime_section->m_html &= ~HF_COMPACT;
            }
        }
        else
        {
            j = size_cast<int>(blks) - 1;
        }

        if ((s_tag_attr[tnum].flags & (TF_BLOCK|TF_HIDE)) == (TF_BLOCK|TF_HIDE))
        {
            g_mime_section->m_html |= HF_IN_HIDING;
        }

        switch (tnum)
        {
        case TAG_BLOCKQUOTE:
        {
            const std::string_view type_attr = find_attr(word, "type");
            const std::string_view style_attr = find_attr(word, "style");
            if ((type_attr.size() >= 4 && string_case_equal(type_attr.substr(0, 4), "cite")) ||
                (style_attr.size() >= 12 && string_case_equal(style_attr.substr(0, 12), "border-left:")))
            {
                blks[j].indent = '>';
            }
            else
            {
                blks[j].indent = ' ';
            }
            break;
        }

        case TAG_HR:
            t = output_prep(t);
              *t++ = '-';
              *t++ = '-';
            g_mime_section->m_html |= HF_NL_OK;
            t = do_newline(t, HF_NL_OK);
            break;

        case TAG_IMG:
            t = output_prep(t);
            if (g_mime_section->m_html & HF_SPACE_OK)
            {
                *t++ = ' ';
            }
            t = fmt::format_to(t, "[Image] ");
            g_mime_section->m_html &= ~HF_SPACE_OK;
            break;

        case TAG_OL:
        {
            itype = 4;
            const std::string_view type_attr = find_attr(word, "type");
            if (!type_attr.empty())
            {
                switch (type_attr.front())
                {
                case 'a':  itype = 5;  break;
                case 'A':  itype = 6;  break;
                case 'i':  itype = 7;  break;
                case 'I':  itype = 8;  break;
                default:   itype = 4;  break;
                }
            }
            blks[j].indent = itype;
            break;
        }

        case TAG_UL:
        {
            itype = 1;
            const std::string_view type_attr = find_attr(word, "type");
            if (!type_attr.empty())
            {
                switch (type_attr.front())
                {
                case 'd': case 'D':  itype = 1;  break;
                case 'c': case 'C':  itype = 2;  break;
                case 's': case 'S':  itype = 3;  break;
                }
            }
            else
            {
                for (const HtmlBlock &block : blks)
                {
                    if (block.indent && block.indent < ' ')
                    {
                        if (++itype == 3)
                        {
                            break;
                        }
                    }
                }
            }
            blks[j].indent = itype;
            break;
        }

        case TAG_LI:
            t = output_prep(t);
            ch = j < 0? ' ' : blks[j].indent;
            switch (ch)
            {
            case 1: case 2: case 3:
                t[-2] = "*o+"[ch-1];
                break;

            case 4:
                t = fmt::format_to(t - 4, "{:2}. ", ++blks[j].count);
                break;

            case 5: case 6:
                cnt = blks[j].count++;
                if (cnt >= 26*26)
                {
                    cnt = 0;
                    blks[j].count = 0;
                }
                if (cnt >= 26)
                {
                    t[-4] = s_letters[ch - 5] + cnt / 26 - 1;
                }
                t[-3] = s_letters[ch-5] + cnt % 26;
                t[-2] = '.';
                break;

            case 7:
                roman_letters = "mdclxvi";
                goto roman_numerals;

            case 8:
                roman_letters = "MDCLXVI";

roman_numerals:
            {
                char *tcp = t - 6;
                cnt = ++blks[j].count;
                for (int i = 0; cnt && i < 7; i++)
                {
                    num = s_roman_values[i];
                    while (cnt >= num)
                    {
                        *tcp++ = roman_letters[i];
                        cnt -= num;
                    }
                    j = (i | 1) + 1;
                    if (j < 7)
                    {
                        num -= s_roman_values[j];
                        if (cnt >= num)
                        {
                            *tcp++ = roman_letters[j];
                            *tcp++ = roman_letters[i];
                            cnt -= num;
                        }
                    }
                }
                if (tcp < t - 2)
                {
                    t -= 2;
                    for (cnt = t - tcp; tcp-- != t - 4; )
                    {
                        tcp[cnt] = *tcp;
                    }
                    while (cnt--)
                    {
                        *++tcp = ' ';
                    }
                }
                else
                {
                    t = tcp;
                }
                *t++ = '.';
                *t++ = ' ';
                break;
            }

            default:
                *t++ = '*';
                *t++ = ' ';
                break;
            }
            g_mime_section->m_html |= HF_NL_OK|HF_P_OK;
            break;

        case TAG_PRE:
            g_mime_section->m_html |= HF_IN_PRE;
            s_word_wrap = s_word_wrap_in_pre;
            break;
        }
    }
    else
    {
        if (s_tag_attr[tnum].flags & TF_BLOCK)
        {
            for (j = size_cast<int>(blks); j--;)
            {
                if (blks[j].tag_num == tnum)
                {
                    for (int i = size_cast<int>(blks); --i > j;)
                    {
                        t = tag_action(t, s_tag_attr[blks[i].tag_num].name.data(), CLOSING_TAG);
                    }
                    blks.resize(j);
                    break;
                }
            }
            g_mime_section->m_html &= ~HF_IN_HIDING;
            while (j-- > 0)
            {
                if (s_tag_attr[blks[j].tag_num].flags & TF_HIDE)
                {
                    g_mime_section->m_html |= HF_IN_HIDING;
                    break;
                }
            }
        }

        j = size_cast<int>(blks) - 1;
        if (j >= 0 && s_tag_attr[blks[j].tag_num].flags & TF_LIST)
        {
            g_mime_section->m_html |= HF_COMPACT;
        }
        else
        {
            g_mime_section->m_html &= ~HF_COMPACT;
        }

        if (s_tag_attr[tnum].flags & TF_NL && g_mime_section->m_html & HF_NL_OK)
        {
            g_mime_section->m_html |= HF_QUEUED_NL;
            g_mime_section->m_html &= ~HF_SPACE_OK;
        }
        if ((num = s_tag_attr[tnum].flags & (TF_P | TF_LIST)) == TF_P //
            || (num == (TF_P | TF_LIST) && !(g_mime_section->m_html & HF_COMPACT)))
        {
            if (g_mime_section->m_html & HF_P_OK)
            {
                g_mime_section->m_html |= HF_QUEUED_P;
                g_mime_section->m_html &= ~HF_SPACE_OK;
            }
        }

        switch (tnum)
        {
        case TAG_PRE:
            g_mime_section->m_html &= ~HF_IN_PRE;
            s_word_wrap = s_normal_word_wrap;
            break;
        }
    }

#ifdef DEBUGGING
                                                std::printf("%*s %% -> ", 4 + 25, "");
    if (g_mime_section->m_html == 0)              std::printf("0 ");
    if (g_mime_section->m_html & HF_IN_TAG)       std::printf("HF_IN_TAG ");
    if (g_mime_section->m_html & HF_IN_COMMENT)   std::printf("HF_IN_COMMENT ");
    if (g_mime_section->m_html & HF_IN_HIDING)    std::printf("HF_IN_HIDING ");
    if (g_mime_section->m_html & HF_IN_PRE)       std::printf("HF_IN_PRE ");
    if (g_mime_section->m_html & HF_IN_DQUOTE)    std::printf("HF_IN_DQUOTE ");
    if (g_mime_section->m_html & HF_IN_SQUOTE)    std::printf("HF_IN_SQUOTE ");
    if (g_mime_section->m_html & HF_QUEUED_P)     std::printf("HF_QUEUED_P ");
    if (g_mime_section->m_html & HF_P_OK)         std::printf("HF_P_OK ");
    if (g_mime_section->m_html & HF_QUEUED_NL)    std::printf("HF_QUEUED_NL ");
    if (g_mime_section->m_html & HF_NL_OK)        std::printf("HF_NL_OK ");
    if (g_mime_section->m_html & HF_NEED_INDENT)  std::printf("HF_NEED_INDENT ");
    if (g_mime_section->m_html & HF_SPACE_OK)     std::printf("HF_SPACE_OK ");
    if (g_mime_section->m_html & HF_COMPACT)      std::printf("HF_COMPACT ");
    std::printf("\n");
#endif
    return t;
}

static char *output_prep(char *t)
{
    if (g_mime_section->m_html & HF_QUEUED_P)
    {
        g_mime_section->m_html &= ~HF_QUEUED_P;
        t = do_newline(t, HF_P_OK);
    }
    if (g_mime_section->m_html & HF_QUEUED_NL)
    {
        g_mime_section->m_html &= ~HF_QUEUED_NL;
        t = do_newline(t, HF_NL_OK);
    }
    return t + do_indent(t);
}

static char *do_newline(char *t, HtmlFlags flag)
{
    if (g_mime_section->m_html & flag)
    {
        g_mime_section->m_html &= ~(flag|HF_SPACE_OK);
        t += do_indent(t);
        *t++ = '\n';
        g_mime_section->m_html_line_start = t - g_art_buf;
        g_mime_section->m_html |= HF_NEED_INDENT;
    }
    return t;
}

static int do_indent(char *t)
{
    int spaces;
    int len = 0;

    if (!(g_mime_section->m_html & HF_NEED_INDENT))
    {
        return len;
    }

    if (t)
    {
        g_mime_section->m_html &= ~HF_NEED_INDENT;
    }

    const std::vector<HtmlBlock> &blks = g_mime_section->m_html_blocks;
    for (const HtmlBlock &block : blks)
    {
        int ch = block.indent;
        if (ch != 0)
        {
            switch (ch)
            {
            case '>':
                spaces = 1;
                break;

            case ' ':
                spaces = 3;
                break;

            case 7:  case 8:
                ch = ' ';
                spaces = 5;
                break;

            default:
                ch = ' ';
                spaces = 3;
                break;
            }
            len += spaces + 1;
            if (len > 64)
            {
                len -= spaces + 1;
                break;
            }
            if (t)
            {
                *t++ = ch;
                while (spaces--)
                {
                    *t++ = ' ';
                }
            }
        }
    }

    return len;
}

static std::string_view find_attr(std::string_view str, std::string_view attr)
{
    std::size_t pos = 0;
    while ((pos = str.find('=', pos + 1)) != std::string_view::npos)
    {
        std::size_t attr_end = pos;
        while (attr_end > 0 && str[attr_end - 1] == ' ')
        {
            --attr_end;
        }
        std::size_t value_begin = pos + 1;
        while (value_begin < str.size() && str[value_begin] == ' ')
        {
            ++value_begin;
        }
        if (attr_end > attr.size() && str[attr_end - attr.size() - 1] == ' ' &&
            string_case_equal(str.substr(attr_end - attr.size(), attr.size()), attr))
        {
            const std::size_t value_end = str.find(' ', value_begin);
            return str.substr(value_begin,
                              value_end == std::string_view::npos ? value_end : value_end - value_begin);
        }
    }
    return {};
}
