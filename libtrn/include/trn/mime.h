/* trn/mime.h
 */
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_MIME_H
#define TRN_MIME_H

#include <trn/decode.h>
#include <trn/enum-flags.h>

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct HtmlBlock
{
    int tag_num;
    int count;
    int indent;
};

enum MimeState
{
    NOT_MIME = 0,
    TEXT_MIME,
    ISO_TEXT_MIME,
    MESSAGE_MIME,
    MULTIPART_MIME,
    IMAGE_MIME,
    AUDIO_MIME,
    APP_MIME,
    UNHANDLED_MIME,
    SKIP_MIME,
    DECODE_MIME,
    BETWEEN_MIME,
    END_OF_MIME,
    HTML_TEXT_MIME,
    ALTERNATE_MIME
};

enum MimeSectionFlags : std::uint16_t
{
    MFS_NONE = 0x0000,
    MSF_INLINE = 0x0001,
    MSF_ALTERNATIVE = 0x0002,
    MSF_ALTERNADONE = 0x0004
};
DECLARE_FLAGS_ENUM(MimeSectionFlags, std::uint16_t);

// Only used with HTMLTEXT_MIME
enum HtmlFlags : std::uint16_t
{
    HF_NONE = 0x0000,
    HF_IN_TAG = 0x0001,
    HF_IN_COMMENT = 0x0002,
    HF_IN_HIDING = 0x0004,
    HF_IN_PRE = 0x0008,
    HF_IN_DQUOTE = 0x0010,
    HF_IN_SQUOTE = 0x0020,
    HF_QUEUED_P = 0x0040,
    HF_P_OK = 0x0080,
    HF_QUEUED_NL = 0x0100,
    HF_NL_OK = 0x0200,
    HF_NEED_INDENT = 0x0400,
    HF_SPACE_OK = 0x0800,
    HF_COMPACT = 0x1000
};
DECLARE_FLAGS_ENUM(HtmlFlags, std::uint16_t);

struct MimeParamViews
{
    std::string_view              value;
    std::vector<std::string_view> params;
};

struct MimeSection
{
    void mime_clear_struct();
    void mime_parse_type(std::string_view text);
    void mime_parse_disposition(std::string_view text);
    void mime_parse_encoding(char *s);
    std::string mime_description() const;

    MimeSection     *m_prev;
    std::optional<std::string> m_filename;
    std::optional<std::string> m_type_name;
    std::vector<std::string>   m_type_params;
    std::optional<std::string> m_boundary;
    int                        m_html_line_start;
    std::vector<HtmlBlock>     m_html_blocks;
    std::string                m_html_tag_word;
    MimeState                  m_type;
    MimeEncoding     m_encoding;
    short            m_part;
    short            m_total;
    short            m_boundary_len;
    MimeSectionFlags m_flags;
    HtmlFlags        m_html;
};

enum TagFlags : std::uint16_t
{
    TF_NONE = 0x0000,
    TF_BLOCK = 0x0001, // This implies TF_HAS_CLOSE
    TF_HAS_CLOSE = 0x0002,
    TF_NL = 0x0004,
    TF_P = 0x0008,
    TF_BR = 0x0010,
    TF_LIST = 0x0020,
    TF_HIDE = 0x0040,
    TF_SPACE = 0x0080,
    TF_TAB = 0x0100
};
DECLARE_FLAGS_ENUM(TagFlags, std::uint16_t);

// NOTE: This must match tagattr in mime.cpp
enum
{
    TAG_BLOCKQUOTE = 0,
    TAG_BR,
    TAG_DIV,
    TAG_HR,
    TAG_IMG,
    TAG_LI,
    TAG_OL,
    TAG_P,
    TAG_PRE,
    TAG_SCRIPT,
    TAG_STYLE,
    TAG_TD,
    TAG_TH,
    TAG_TR,
    TAG_TITLE,
    TAG_UL,
    TAG_XML,
    LAST_TAG,
};

struct HtmlTag
{
    const char *name;
    char        length;
    TagFlags    flags;
};

extern MimeSection  g_mime_article;
extern MimeSection *g_mime_section;
extern MimeState    g_mime_state;
extern std::string  g_multipart_separator;
extern bool         g_auto_view_inline;
extern char        *g_mime_getc_line;

enum MimeCapFlags : std::uint8_t
{
    MCF_NONE = 0x00,
    MCF_NEEDS_TERMINAL = 0x01,
    MCF_COPIOUS_OUTPUT = 0x02
};
DECLARE_FLAGS_ENUM(MimeCapFlags, std::uint8_t);

struct MimeCapEntry
{
    std::string  content_type;
    std::string  command;
    std::string  test_command;
    std::string  description;
    MimeCapFlags flags{};
};

void          mime_init();
void          mime_final();
void          mime_read_mimecap(std::string_view mcname);
MimeCapEntry *mime_find_mimecap_entry(std::string_view contenttype, MimeCapFlags skip_flags);
bool          mime_types_match(std::string_view ct, std::string_view pat);
int            mime_exec(std::string_view cmd);
void          mime_push_section();
void          mime_set_article();
void          mime_parse_sub_header(std::FILE *ifp, const char *next_line);
void          mime_set_state(char *bp);
int           mime_end_of_section(char *bp);
MimeParamViews mime_parse_params(std::string_view text);
void          mime_decode_article(bool view);
int           qp_decode_string(char *t, const char *f, bool in_header);
DecodeState   qp_decode(std::FILE *ifp, DecodeState state);
int           b64_decode_string(char *t, const char *f);
DecodeState   b64_decode(std::FILE *ifp, DecodeState state);
DecodeState   cat_decode(std::FILE *ifp, DecodeState state);
int           filter_html(char *t, const char *f);

#endif
