/* mime.h
 */
#ifndef TRN_MIME_H
#define TRN_MIME_H

#include "decode.h"
#include "enum-flags.h"

#include <cstdint>

struct HBLK
{
    int	    tnum;
    short   cnt;
    char    indent;
};

enum mime_state
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

enum mimesection_flags : std::uint16_t
{
    MFS_NONE = 0x0000,
    MSF_INLINE = 0x0001,
    MSF_ALTERNATIVE = 0x0002,
    MSF_ALTERNADONE = 0x0004
};
DECLARE_FLAGS_ENUM(mimesection_flags, std::uint16_t);

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
    HBLK *html_blks;
    mime_state type;
    mime_encoding encoding;
    short part;
    short total;
    short boundary_len;
    mimesection_flags flags;
    html_flags html;
    short html_blkcnt;
};

enum
{
    HTML_MAX_BLOCKS = 256
};

enum
{
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
    int flags;
};

extern MIME_SECT g_mime_article;
extern MIME_SECT *g_mime_section;
extern mime_state g_mime_state;
extern char *g_multipart_separator;
extern bool g_auto_view_inline;
extern char *g_mime_getc_line;

struct MIMECAP_ENTRY
{
    char* contenttype;
    char* command;
    char* testcommand;
    char* label;
    int flags;
};

enum
{
    MCF_NEEDSTERMINAL = 0x0001,
    MCF_COPIOUSOUTPUT = 0x0002
};

void mime_init();
void mime_ReadMimecap(char *mcname);
MIMECAP_ENTRY *mime_FindMimecapEntry(const char *contenttype, int skip_flags);
bool mime_TypesMatch(const char *ct, const char *pat);
int mime_Exec(char *cmd);
void mime_InitSections();
void mime_PushSection();
bool mime_PopSection();
void mime_ClearStruct(MIME_SECT *mp);
void mime_SetArticle();
void mime_ParseType(MIME_SECT *mp, char *s);
void mime_ParseDisposition(MIME_SECT *mp, char *s);
void mime_ParseEncoding(MIME_SECT *mp, char *s);
void mime_ParseSubheader(FILE *ifp, char *next_line);
void mime_SetState(char *bp);
int mime_EndOfSection(char *bp);
char *mime_ParseParams(char *str);
char *mime_FindParam(char *s, char *param);
char *mime_SkipWhitespace(char *s);
void mime_DecodeArticle(bool view);
void mime_Description(MIME_SECT *mp, char *s, int limit);
int qp_decodestring(char *t, char *f, bool in_header);
decode_state qp_decode(FILE *ifp, decode_state state);
int b64_decodestring(char *t, char *f);
decode_state b64_decode(FILE *ifp, decode_state state);
decode_state cat_decode(FILE *ifp, decode_state state);
int filter_html(char *t, char *f);

#endif
