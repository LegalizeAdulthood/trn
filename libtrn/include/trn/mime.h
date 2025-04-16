/* trn/mime.h
 */
#ifndef TRN_MIME_H
#define TRN_MIME_H

#include "trn/decode.h"
#include "trn/enum-flags.h"

#include <cstdint>
#include <cstdio>
#include <string>

struct HBlock
{
    int     tnum;
    short   cnt;
    char    indent;
};

enum MimeState
{
    NOT_MIME = 0,
    TEXT_MIME,
    ISOTEXT_MIME,
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
    HTMLTEXT_MIME,
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

/* Only used with HTMLTEXT_MIME */
enum html_flags : std::uint16_t
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
DECLARE_FLAGS_ENUM(html_flags, std::uint16_t);

struct MIME_SECT
{
    MIME_SECT *prev;
    char *filename;
    char *type_name;
    char *type_params;
    char *boundary;
    int html_line_start;
    HBlock *html_blks;
    MimeState type;
    MimeEncoding encoding;
    short part;
    short total;
    short boundary_len;
    MimeSectionFlags flags;
    html_flags html;
    short html_blkcnt;
};

enum
{
    HTML_MAX_BLOCKS = 256
};

enum tag_flags : std::uint16_t
{
    TF_NONE = 0x0000,
    TF_BLOCK = 0x0001, /* This implies TF_HAS_CLOSE */
    TF_HAS_CLOSE = 0x0002,
    TF_NL = 0x0004,
    TF_P = 0x0008,
    TF_BR = 0x0010,
    TF_LIST = 0x0020,
    TF_HIDE = 0x0040,
    TF_SPACE = 0x0080,
    TF_TAB = 0x0100
};
DECLARE_FLAGS_ENUM(tag_flags, std::uint16_t);

/* NOTE: This must match tagattr in mime.cpp */
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

struct HTML_TAGS
{
    char* name;
    char length;
    tag_flags flags;
};

extern MIME_SECT g_mime_article;
extern MIME_SECT *g_mime_section;
extern MimeState g_mime_state;
extern std::string g_multipart_separator;
extern bool g_auto_view_inline;
extern char *g_mime_getc_line;

enum mimecap_flags : std::uint8_t
{
    MCF_NONE = 0x00,
    MCF_NEEDSTERMINAL = 0x01,
    MCF_COPIOUSOUTPUT = 0x02
};
DECLARE_FLAGS_ENUM(mimecap_flags, std::uint8_t);

struct MIMECAP_ENTRY
{
    char         *contenttype;
    char         *command;
    char         *testcommand;
    char         *description;
    mimecap_flags flags;
};

void           mime_init();
void           mime_final();
void           mime_ReadMimecap(const char *mcname);
MIMECAP_ENTRY *mime_FindMimecapEntry(const char *contenttype, mimecap_flags skip_flags);
bool           mime_TypesMatch(const char *ct, const char *pat);
int            mime_Exec(char *cmd);
void           mime_InitSections();
void           mime_PushSection();
bool           mime_PopSection();
void           mime_ClearStruct(MIME_SECT *mp);
void           mime_SetArticle();
void           mime_ParseType(MIME_SECT *mp, char *s);
void           mime_ParseDisposition(MIME_SECT *mp, char *s);
void           mime_ParseEncoding(MIME_SECT *mp, char *s);
void           mime_ParseSubheader(std::FILE *ifp, char *next_line);
void           mime_SetState(char *bp);
int            mime_EndOfSection(char *bp);
char *         mime_ParseParams(char *str);
char *         mime_FindParam(char *s, const char *param);
char *         mime_SkipWhitespace(char *s);
void           mime_DecodeArticle(bool view);
void           mime_Description(MIME_SECT *mp, char *s, int limit);
int            qp_decodestring(char *t, const char *f, bool in_header);
DecodeState   qp_decode(std::FILE *ifp, DecodeState state);
int            b64_decodestring(char *t, const char *f);
DecodeState   b64_decode(std::FILE *ifp, DecodeState state);
DecodeState   cat_decode(std::FILE *ifp, DecodeState state);
int            filter_html(char *t, const char *f);

#endif
