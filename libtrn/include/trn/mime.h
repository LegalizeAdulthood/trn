/* trn/mime.h
 */
#ifndef TRN_MIME_H
#define TRN_MIME_H

#include "trn/decode.h"
#include "trn/enum-flags.h"

#include <cstdint>
#include <cstdio>
#include <string>

struct HtmlBlock
{
    int   tag_num;
    short count;
    char  indent;
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

struct MimeSection
{
    MimeSection     *prev;
    char            *filename;
    char            *type_name;
    char            *type_params;
    char            *boundary;
    int              html_line_start;
    HtmlBlock       *html_blocks;
    MimeState        type;
    MimeEncoding     encoding;
    short            part;
    short            total;
    short            boundary_len;
    MimeSectionFlags flags;
    HtmlFlags        html;
    short            html_block_count;
};

enum
{
    HTML_MAX_BLOCKS = 256
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
    char        *content_type;
    char        *command;
    char        *test_command;
    char        *description;
    MimeCapFlags flags;
};

void          mime_init();
void          mime_final();
void          mime_read_mimecap(const char *mcname);
MimeCapEntry *mime_find_mimecap_entry(const char *contenttype, MimeCapFlags skip_flags);
bool          mime_types_match(const char *ct, const char *pat);
int           mime_exec(char *cmd);
void          mime_init_sections();
void          mime_push_section();
bool          mime_pop_section();
void          mime_clear_struct(MimeSection *mp);
void          mime_set_article();
void          mime_parse_type(MimeSection *mp, char *s);
void          mime_parse_disposition(MimeSection *mp, char *s);
void          mime_parse_encoding(MimeSection *mp, char *s);
void          mime_parse_sub_header(std::FILE *ifp, char *next_line);
void          mime_set_state(char *bp);
int           mime_end_of_section(char *bp);
char         *mime_parse_params(char *str);
char         *mime_find_param(char *s, const char *param);
char         *mime_skip_whitespace(char *s);
void          mime_decode_article(bool view);
void          mime_description(MimeSection *mp, char *s, int limit);
int           qp_decode_string(char *t, const char *f, bool in_header);
DecodeState   qp_decode(std::FILE *ifp, DecodeState state);
int           b64_decode_string(char *t, const char *f);
DecodeState   b64_decode(std::FILE *ifp, DecodeState state);
DecodeState   cat_decode(std::FILE *ifp, DecodeState state);
int           filter_html(char *t, const char *f);

#endif
