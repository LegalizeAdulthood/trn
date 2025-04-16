/* mime.c
 * vi: set sw=4 ts=8 ai sm noet:
 */

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/mime-internal.h"

#include "trn/List.h"
#include "trn/art.h"
#include "trn/artio.h"
#include "trn/artstate.h"
#include "trn/decode.h"
#include "trn/head.h"
#include "trn/ng.h"
#include "trn/respond.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/utf.h"
#include "trn/util.h"
#include "util/env.h"
#include "util/util2.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

MimeSection   g_mime_article{};
MimeSection  *g_mime_section{&g_mime_article};
MimeState  g_mime_state{};
std::string g_multipart_separator{"-=-=-=-=-=-"};
bool        g_auto_view_inline{};
char       *g_mime_getc_line{};

#ifdef USE_UTF_HACK
#define CODE_POINT_MAX  0x7FFFFFFFL
#else
#define CODE_POINT_MAX  0x7F
#endif

// clang-format off
static HtmlTag s_tagattr[LAST_TAG] = {
 /* name               length   flags */
    {"blockquote",      10,     TF_BLOCK | TF_P | TF_NL                 },
    {"br",               2,     TF_NL | TF_BR                           },
    {"div",              3,     TF_BLOCK | TF_NL                        },
    {"hr",               2,     TF_NL                                   },
    {"img",              3,     TF_NONE                                 },
    {"li",               2,     TF_NL                                   },
    {"ol",               2,     TF_BLOCK | TF_P | TF_NL | TF_LIST       },
    {"p",                1,     TF_HAS_CLOSE | TF_P | TF_NL             },
    {"pre",              3,     TF_BLOCK | TF_P | TF_NL                 },
    {"script",           6,     TF_BLOCK | TF_HIDE                      },
    {"style",            5,     TF_BLOCK | TF_HIDE                      },
    {"td",               2,     TF_TAB                                  },
    {"th",               2,     TF_TAB                                  },
    {"tr",               2,     TF_NL                                   },
    {"title",            5,     TF_BLOCK | TF_HIDE                      },
    {"ul",               2,     TF_BLOCK | TF_P | TF_NL | TF_LIST       },
    {"xml",              3,     TF_BLOCK | TF_HIDE                      }, /* non-standard but seen in the wild */
};
// clang-format on
static List        *s_mimecap_list{};
static char         s_text_plain[] = "text/plain";
static MimeExecutor s_executor;

constexpr bool CLOSING_TAG = false;
constexpr bool OPENING_TAG = true;

static char *mime_ParseEntryArg(char **cpp);
static int   mime_getc(std::FILE *fp);
static char *tag_action(char *t, char *word, bool opening_tag);
static char *output_prep(char *t);
static char *do_newline(char *t, HtmlFlags flag);
static int   do_indent(char *t);
static char *find_attr(char *str, const char *attr);

inline MIMECAP_ENTRY *mimecap_ptr(long n)
{
    return (MIMECAP_ENTRY *)listnum2listitem(s_mimecap_list, n);
}

void mime_set_executor(MimeExecutor executor)
{
    s_executor = std::move(executor);
}

void mime_init()
{
    s_executor = doshell;
    s_mimecap_list = new_list(0,-1,sizeof(MIMECAP_ENTRY),40,LF_ZERO_MEM,nullptr);

    char *mcname = get_val("MIMECAPS");
    if (mcname == nullptr)
    {
        mcname = get_val("MAILCAPS", MIMECAP);
    }
    mcname = savestr(mcname);
    char *s = mcname;
    do
    {
        char *t = std::strchr(s, ':');
        if (t != nullptr)
        {
            *t++ = '\0';
        }
        if (*s)
        {
            mime_ReadMimecap(s);
        }
        s = t;
    } while (s && *s);
    std::free(mcname);
}

void mime_final()
{
    if (s_mimecap_list)
    {
        delete_list(s_mimecap_list);
        s_mimecap_list = nullptr;
    }
}

void mime_ReadMimecap(const char *mcname)
{
    char*s;
    int  buflen = 2048;
    int  i;

    std::FILE *fp = std::fopen(filexp(mcname), "r");
    if (fp == nullptr)
    {
        return;
    }
    char *bp = safemalloc(buflen);
    for (i = s_mimecap_list->high; !std::feof(fp);)
    {
        *(s = bp) = '\0';
        int linelen = 0;
        while (std::fgets(s, buflen - linelen, fp))
        {
            if (*s == '#')
            {
                continue;
            }
            linelen += std::strlen(s);
            if (linelen == 0)
            {
                continue;
            }
            if (bp[linelen - 1] == '\n')
            {
                if (--linelen == 0)
                {
                    continue;
                }
                if (bp[linelen - 1] != '\\')
                {
                    bp[linelen] = '\0';
                    break;
                }
                bp[--linelen] = '\0';
            }

            if (linelen + 1024 > buflen)
            {
                buflen *= 2;
                bp = saferealloc(bp, buflen);
            }

            s = bp + linelen;
        }
        s = skip_space(bp);
        if (!*s)
        {
            continue;
        }
        char *t = mime_ParseEntryArg(&s);
        if (!s)
        {
            std::fprintf(stderr, "trn: Ignoring invalid mimecap entry: %s\n", bp);
            continue;
        }
        MIMECAP_ENTRY *mcp = mimecap_ptr(++i);
        mcp->contenttype = savestr(t);
        mcp->command = savestr(mime_ParseEntryArg(&s));
        while (s)
        {
            t = mime_ParseEntryArg(&s);
            char *arg = std::strchr(t, '=');
            if (arg != nullptr)
            {
                char* f = arg+1;
                while (arg != t && std::isspace(arg[-1]))
                {
                    arg--;
                }
                *arg++ = '\0';
                f = skip_space(f);
                if (*f == '"')
                {
                    f = cpytill(arg, f + 1, '"');
                }
                else
                {
                    arg = f;
                }
            }
            if (*t)
            {
                if (string_case_equal(t, "needsterminal"))
                {
                    mcp->flags |= MCF_NEEDSTERMINAL;
                }
                else if (string_case_equal(t, "copiousoutput"))
                {
                    mcp->flags |= MCF_COPIOUSOUTPUT;
                }
                else if (arg && string_case_equal(t, "test"))
                {
                    mcp->testcommand = savestr(arg);
                }
                else if (arg && (string_case_equal(t, "description") || string_case_equal(t, "label")))
                {
                    mcp->description = savestr(arg); /* 'label' is the legacy name for description */
                }
            }
        }
    }
    s_mimecap_list->high = i;
    std::free(bp);
    std::fclose(fp);
}

static char *mime_ParseEntryArg(char **cpp)
{
    char*f;
    char*t;

    char *s = skip_space(*cpp);

    for (f = t = s; *f && *f != ';';)
    {
        if (*f == '\\')
        {
            if (*++f == '%')
            {
                *t++ = '%';
            }
            else if (!*f)
            {
                break;
            }
        }
        *t++ = *f++;
    }
    while (std::isspace(*f) || *f == ';')
    {
        f++;
    }
    if (!*f)
    {
        f = nullptr;
    }
    while (t != s && std::isspace(t[-1]))
    {
        t--;
    }
    *t = '\0';
    *cpp = f;
    return s;
}

MIMECAP_ENTRY *mime_FindMimecapEntry(const char *contenttype, mimecap_flags skip_flags)
{
    for (int i = 0; i <= s_mimecap_list->high; i++)
    {
        MIMECAP_ENTRY *mcp = mimecap_ptr(i);
        if (!(mcp->flags & skip_flags) //
            && mime_TypesMatch(contenttype, mcp->contenttype))
        {
            if (!mcp->testcommand)
            {
                return mcp;
            }
            if (mime_Exec(mcp->testcommand) == 0)
            {
                return mcp;
            }
        }
    }
    return nullptr;
}

bool mime_TypesMatch(const char *ct, const char *pat)
{
    const char* s = std::strchr(pat,'/');
    int len = s? s - pat : std::strlen(pat);
    bool iswild = !s || !std::strcmp(s+1,"*");

    return string_case_equal(ct, pat)
        || (iswild && string_case_equal(ct, pat,len) && ct[len] == '/');
}

int mime_Exec(char *cmd)
{
    char* t = g_cmd_buf;

    for (char *f = cmd; *f && t - g_cmd_buf < CBUFLEN - 2; f++)
    {
        if (*f == '%')
        {
            switch (*++f)
            {
            case 's':
                safecpy(t, g_decode_filename, CBUFLEN-(t-g_cmd_buf));
                t += std::strlen(t);
                break;

            case 't':
                *t++ = '\'';
                safecpy(t, g_mime_section->type_name, CBUFLEN-(t-g_cmd_buf)-1);
                t += std::strlen(t);
                *t++ = '\'';
                break;

            case '{':
            {
                char* s = std::strchr(f, '}');
                if (!s)
                {
                    return -1;
                }
                f++;
                *s = '\0';
                char *p = mime_FindParam(g_mime_section->type_params, f);
                *s = '}'; /* restore */
                f = s;
                *t++ = '\'';
                safecpy(t, p, CBUFLEN-(t-g_cmd_buf)-1);
                t += std::strlen(t);
                *t++ = '\'';
                break;
            }

            case '%':
                *t++ = '%';
                break;

            case 'n':
            case 'F':
                return -1;
            }
        }
        else
        {
            *t++ = *f;
        }
    }
    *t = '\0';

    return s_executor(SH, g_cmd_buf);
}

void mime_InitSections()
{
    while (mime_PopSection())
    {
    }
    mime_ClearStruct(g_mime_section);
    g_mime_state = NOT_MIME;
}

void mime_PushSection()
{
    MimeSection* mp = (MimeSection*)safemalloc(sizeof (MimeSection));
    std::memset((char*)mp,0,sizeof (MimeSection));
    mp->prev = g_mime_section;
    g_mime_section = mp;
}

bool mime_PopSection()
{
    MimeSection* mp = g_mime_section->prev;
    if (mp)
    {
        mime_ClearStruct(g_mime_section);
        std::free((char*)g_mime_section);
        g_mime_section = mp;
        g_mime_state = mp->type;
        return true;
    }
    g_mime_state = g_mime_article.type;
    return false;
}

/* Free up this mime structure's resources */
void mime_ClearStruct(MimeSection *mp)
{
    safefree0(mp->filename);
    safefree0(mp->type_name);
    safefree0(mp->type_params);
    safefree0(mp->boundary);
    safefree0(mp->html_blks);
    mp->type = NOT_MIME;
    mp->encoding = MENCODE_NONE;
    mp->total = 0;
    mp->part = 0;
    mp->boundary_len = 0;
    mp->html_blkcnt = 0;
    mp->flags = MFS_NONE;
    mp->html = HF_NONE;
    mp->html_line_start = 0;
}

/* Setup g_mime_article structure based on article's headers */
void mime_SetArticle()
{
    mime_InitSections();
    /* TODO: Check mime version #? */
    g_multimedia_mime = false;
    g_is_mime = g_htype[MIMEVER_LINE].flags & HT_MAGIC
            && g_htype[MIMEVER_LINE].minpos >= 0;

    {
        char *s = fetchlines(g_art, CONTTYPE_LINE);
        mime_ParseType(g_mime_section,s);
        std::free(s);
    }

    if (g_is_mime)
    {
        char *s = fetchlines(g_art, CONTXFER_LINE);
        mime_ParseEncoding(g_mime_section,s);
        std::free(s);

        s = fetchlines(g_art,CONTDISP_LINE);
        mime_ParseDisposition(g_mime_section,s);
        std::free(s);

        g_mime_state = g_mime_section->type;
        if (g_mime_state == NOT_MIME
         || (g_mime_state == TEXT_MIME && g_mime_section->encoding == MENCODE_NONE))
        {
            g_is_mime = false;
        }
        else if (!g_mime_section->type_name)
        {
            g_mime_section->type_name = savestr(s_text_plain);
        }
    }
}

/* Use the Content-Type to set values in the mime structure */
void mime_ParseType(MimeSection *mp, char *s)
{
    safefree0(mp->type_name);
    safefree0(mp->type_params);

    mp->type_params = mime_ParseParams(s);
    if (!*s)
    {
        mp->type = NOT_MIME;
        return;
    }
    mp->type_name = savestr(s);
    char *t = mime_FindParam(mp->type_params, "name");
    if (t)
    {
        safefree(mp->filename);
        mp->filename = savestr(t);
    }

    if (string_case_equal(s, "text", 4))
    {
        mp->type = TEXT_MIME;
        s += 4;
        if (*s++ != '/')
        {
            return;
        }
#ifdef USE_UTF_HACK
        utf_init(mime_FindParam(mp->type_params,"charset"), CHARSET_NAME_UTF8); /*FIXME*/
#endif
        if (string_case_equal(s, "html", 4))
        {
            mp->type = HTMLTEXT_MIME;
        }
        else if (string_case_equal(s, "x-vcard", 7))
        {
            mp->type = UNHANDLED_MIME;
        }
        return;
    }

    if (string_case_equal(s, "message/", 8))
    {
        s += 8;
        mp->type = MESSAGE_MIME;
        if (string_case_equal(s, "partial"))
        {
            t = mime_FindParam(mp->type_params,"id");
            if (!t)
            {
                return;
            }
            safefree(mp->filename);
            mp->filename = savestr(t);
            t = mime_FindParam(mp->type_params,"number");
            if (t)
            {
                mp->part = (short) std::atoi(t);
            }
            t = mime_FindParam(mp->type_params,"total");
            if (t)
            {
                mp->total = (short) std::atoi(t);
            }
            if (!mp->total)
            {
                mp->part = 0;
                return;
            }
            return;
        }
        return;
    }

    if (string_case_equal(s, "multipart/", 10))
    {
        s += 10;
        t = mime_FindParam(mp->type_params,"boundary");
        if (!t)
        {
            mp->type = UNHANDLED_MIME;
            return;
        }
        if (string_case_equal(s, "alternative", 11))
        {
            mp->flags |= MSF_ALTERNATIVE;
        }
        safefree(mp->boundary);
        mp->boundary = savestr(t);
        mp->boundary_len = (short)std::strlen(t);
        mp->type = MULTIPART_MIME;
        return;
    }

    if (string_case_equal(s, "image/", 6))
    {
        mp->type = IMAGE_MIME;
        return;
    }

    if (string_case_equal(s, "audio/", 6))
    {
        mp->type = AUDIO_MIME;
        return;
    }

    mp->type = UNHANDLED_MIME;
}

/* Use the Content-Disposition to set values in the mime structure */
void mime_ParseDisposition(MimeSection *mp, char *s)
{
    char *params = mime_ParseParams(s);
    if (string_case_equal(s, "inline"))
    {
        mp->flags |= MSF_INLINE;
    }

    s = mime_FindParam(params,"filename");
    if (s)
    {
        safefree(mp->filename);
        mp->filename = savestr(s);
    }
    safefree(params);
}

/* Use the Content-Transfer-Encoding to set values in the mime structure */
void mime_ParseEncoding(MimeSection *mp, char *s)
{
    s = mime_SkipWhitespace(s);
    if (!*s)
    {
        mp->encoding = MENCODE_NONE;
        return;
    }
    if (*s == '7' || *s == '8')
    {
        if (string_case_equal(s + 1, "bit", 3))
        {
            s += 4;
            mp->encoding = MENCODE_NONE;
        }
    }
    else if (string_case_equal(s, "quoted-printable", 16))
    {
        s += 16;
        mp->encoding = MENCODE_QPRINT;
    }
    else if (string_case_equal(s, "binary", 6))
    {
        s += 6;
        mp->encoding = MENCODE_NONE;
    }
    else if (string_case_equal(s, "base64", 6))
    {
        s += 6;
        mp->encoding = MENCODE_BASE64;
    }
    else if (string_case_equal(s, "x-uue", 5))
    {
        s += 5;
        mp->encoding = MENCODE_UUE;
        if (string_case_equal(s, "ncode", 5))
        {
            s += 5;
        }
    }
    else
    {
        mp->encoding = MENCODE_UNHANDLED;
        return;
    }
    if (*s != '\0' && !isspace(*s) && *s != ';' && *s != '(')
    {
        mp->encoding = MENCODE_UNHANDLED;
    }
}

/* Parse a multipart mime header and affect the *g_mime_section structure */

void mime_ParseSubheader(std::FILE *ifp, char *next_line)
{
    static char* line = nullptr;
    static int line_size = 0;
    mime_ClearStruct(g_mime_section);
    g_mime_section->type = TEXT_MIME;
    while (true)
    {
        for (int pos = 0;; pos += std::strlen(line + pos))
        {
            int len = pos + (next_line ? std::strlen(next_line) : 0) + LBUFLEN;
            if (line_size < len)
            {
                line_size = len + LBUFLEN;
                line = saferealloc(line, line_size);
            }
            if (next_line)
            {
                safecpy(line+pos, next_line, line_size - pos);
                next_line = nullptr;
            }
            else if (ifp)
            {
                if (!std::fgets(line + pos, LBUFLEN, ifp))
                {
                    break;
                }
            }
            else if (!readart(line + pos, LBUFLEN))
            {
                break;
            }
            if (line[0] == '\n')
            {
                break;
            }
            if (pos && !is_hor_space(line[pos]))
            {
                next_line = line + pos;
                line[pos-1] = '\0';
                break;
            }
        }
        char *s = std::strchr(line, ':');
        if (s == nullptr)
        {
            break;
        }

        int linetype = set_line_type(line, s);
        switch (linetype)
        {
        case CONTTYPE_LINE:
            mime_ParseType(g_mime_section,s+1);
            break;

        case CONTXFER_LINE:
            mime_ParseEncoding(g_mime_section,s+1);
            break;

        case CONTDISP_LINE:
            mime_ParseDisposition(g_mime_section,s+1);
            break;

        case CONTNAME_LINE:
            safefree(g_mime_section->filename);
            s = mime_SkipWhitespace(s+1);
            g_mime_section->filename = savestr(s);
            break;

#if 0
        case CONTLEN_LINE:
            g_mime_section->content_len = atol(s+1);
            break;
#endif
        }
    }
    g_mime_state = g_mime_section->type;
    if (!g_mime_section->type_name)
    {
        g_mime_section->type_name = savestr(s_text_plain);
    }
}

void mime_SetState(char *bp)
{
    if (g_mime_state == BETWEEN_MIME)
    {
        mime_ParseSubheader(nullptr, bp);
        *bp = '\0';
        if (g_mime_section->prev->flags & MSF_ALTERNADONE)
        {
            g_mime_state = ALTERNATE_MIME;
        }
        else if (g_mime_section->prev->flags & MSF_ALTERNATIVE)
        {
            g_mime_section->prev->flags |= MSF_ALTERNADONE;
        }
    }

    while (g_mime_state == MESSAGE_MIME)
    {
        mime_PushSection();
        mime_ParseSubheader(nullptr, *bp ? bp : nullptr);
        *bp = '\0';
    }

    if (g_mime_state == MULTIPART_MIME)
    {
        mime_PushSection();
        g_mime_state = SKIP_MIME;               /* Skip anything before 1st part */
    }

    int ret = mime_EndOfSection(bp);
    switch (ret)
    {
    case 0:
        break;

    case 1:
        while (!g_mime_section->prev->boundary_len)
        {
            mime_PopSection();
        }
        g_mime_state = BETWEEN_MIME;
        break;

    case 2:
        while (!g_mime_section->prev->boundary_len)
        {
            mime_PopSection();
        }
        mime_PopSection();
        g_mime_state = END_OF_MIME;
        break;
    }
}

int mime_EndOfSection(char *bp)
{
    MimeSection* mp = g_mime_section->prev;
    while (mp && !mp->boundary_len)
    {
        mp = mp->prev;
    }
    if (mp)
    {
        /* have we read all the data in this part? */
        if (bp[0] == '-' && bp[1] == '-' //
            && !std::strncmp(bp + 2, mp->boundary, mp->boundary_len))
        {
            int len = 2 + mp->boundary_len;
            /* have we found the last boundary? */
            if (bp[len] == '-' && bp[len+1] == '-'
             && (bp[len+2] == '\n' || bp[len+2] == '\0'))
            {
                return 2;
            }
            return bp[len] == '\n' || bp[len] == '\0';
        }
    }
    return 0;
}

/* Return a saved string of all the extra parameters on this mime
 * header line.  The passed-in string is transformed into just the
 * first word on the line.
 */
char *mime_ParseParams(char *str)
{
    char *e = mime_SkipWhitespace(str);
    char *s = e;
    while (*e && *e != ';' && !std::isspace(*e) && *e != '(')
    {
        e++;
    }
    char *t = savestr(mime_SkipWhitespace(e));
    *e = '\0';
    if (s != str)
    {
        safecpy(str, s, e - s + 1);
    }
    str = t;
    s = t;
    while (*s == ';')
    {
        s = mime_SkipWhitespace(s+1);
        while (*s && *s != ';' && *s != '(' && *s != '=' && !std::isspace(*s))
        {
            *t++ = *s++;
        }
        s = mime_SkipWhitespace(s);
        if (*s == '=')
        {
            *t++ = *s;
            s = mime_SkipWhitespace(s+1);
            if (*s == '"')
            {
                s = cpytill(t,s+1,'"');
                if (*s == '"')
                {
                    s++;
                }
                t += std::strlen(t);
            }
            else
            {
                while (*s && *s != ';' && !std::isspace(*s) && *s != '(')
                {
                    *t++ = *s++;
                }
            }
        }
        *t++ = '\0';
    }
    *t = '\0';
    return str;
}

char *mime_FindParam(char *s, const char *param)
{
    int param_len = std::strlen(param);
    while (s && *s)
    {
        if (string_case_equal(s, param, param_len) && s[param_len] == '=')
        {
            return s + param_len + 1;
        }
        s += std::strlen(s) + 1;
    }
    return nullptr;
}

/* Skip whitespace and RFC-822 comments. */

char *mime_SkipWhitespace(char *s)
{
    int comment_level = 0;

    while (*s)
    {
        if (*s == '(')
        {
            s++;
            comment_level++;
            while (comment_level)
            {
                switch (*s++)
                {
                case '\0':
                    return s-1;

                case '\\':
                    s++;
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
        else if (!std::isspace(*s))
        {
            break;
        }
        else
        {
            s++;
        }
    }
    return s;
}

void mime_DecodeArticle(bool view)
{
    MIMECAP_ENTRY* mcp = nullptr;

    seekart(g_savefrom);
    *g_art_line = '\0';

    while (true)
    {
        if (g_mime_state != MESSAGE_MIME || !g_mime_section->total)
        {
            if (!readart(g_art_line,sizeof g_art_line))
            {
                break;
            }
            mime_SetState(g_art_line);
        }
        switch (g_mime_state)
        {
        case BETWEEN_MIME:
        case END_OF_MIME:
            break;

        case TEXT_MIME:
        case HTMLTEXT_MIME:
        case ISOTEXT_MIME:
        case MESSAGE_MIME:
            /* TODO: Check for uuencoded file here? */
            g_mime_state = SKIP_MIME;
            /* FALL THROUGH */

        case SKIP_MIME:
        {
            MimeSection* mp = g_mime_section;
            while ((mp = mp->prev) != nullptr && !mp->boundary_len)
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
                mcp = mime_FindMimecapEntry(g_mime_section->type_name, MCF_NONE);
                if (!mcp)
                {
                    std::printf("No view method for %s -- skipping.\n",
                           g_mime_section->type_name);
                    g_mime_state = SKIP_MIME;
                    break;
                }
            }
            g_mime_state = DECODE_MIME;
            if (decode_piece(mcp, *g_art_line == '\n' ? nullptr : g_art_line))
            {
                mime_SetState(g_art_line);
                if (g_mime_state == DECODE_MIME)
                {
                    g_mime_state = SKIP_MIME;
                }
            }
            else
            {
                if (*g_msg)
                {
                    newline();
                    std::fputs(g_msg,stdout);
                }
                g_mime_state = SKIP_MIME;
            }
            newline();
            break;
        }
    }
}

void mime_Description(MimeSection *mp, char *s, int limit)
{
    char* fn = decode_fix_fname(mp->filename);
    int flen = std::strlen(fn);

    limit -= 2;  /* leave room for the trailing ']' and '\n' */
    std::sprintf(s, "[Attachment type=%s, name=", mp->type_name);
    int len = std::strlen(s);
    if (len + flen <= limit)
    {
        std::sprintf(s + len, "%s]\n", fn);
    }
    else if (len+3 >= limit)
    {
        std::strcpy(s + limit - 3, "...]\n");
    }
    else
    {
#if 0
        std::sprintf(s+len, "...%s]\n", fn + flen - (limit-(len+3)));
#else
        safecpy(s+len, fn, limit - (len+3));
        std::strcat(s, "...]\n");
#endif
    }
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

int qp_decodestring(char *t, const char *f, bool in_header)
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

        case '=':     /* decode a hex-value */
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
            /* FALL THROUGH */

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
        char* filename = decode_fix_fname(g_mime_section->filename);
        ofp = std::fopen(filename, "wb");
        if (!ofp)
        {
            return DECODE_ERROR;
        }
        erase_line(false);
        std::printf("Decoding %s", filename);
        if (g_nowait_fork)
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

    return DECODE_MAYBEDONE;
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

int b64_decodestring(char *t, const char *f)
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
        char* filename = decode_fix_fname(g_mime_section->filename);
        ofp = std::fopen(filename, "wb");
        if (!ofp)
        {
            return DECODE_ERROR;
        }
        std::printf("Decoding %s", filename);
        if (g_nowait_fork)
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

    return DECODE_MAYBEDONE;
}

static int mime_getc(std::FILE *fp)
{
    if (fp)
    {
        return std::fgetc(fp);
    }

    if (!g_mime_getc_line || !*g_mime_getc_line)
    {
        g_mime_getc_line = readart(g_art_line,sizeof g_art_line);
        if (mime_EndOfSection(g_art_line))
        {
            return EOF;
        }
        if (!g_mime_getc_line)
        {
            return EOF;
        }
    }
    return *g_mime_getc_line++;
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
        char* filename = decode_fix_fname(g_mime_section->filename);
        ofp = std::fopen(filename, "wb");
        if (!ofp)
        {
            return DECODE_ERROR;
        }
        std::printf("Decoding %s", filename);
        if (g_nowait_fork)
        {
            std::fflush(stdout);
        }
        else
        {
            newline();
        }
    }

    if (ifp)
    {
        while (std::fgets(g_buf, sizeof g_buf, ifp))
        {
            std::fputs(g_buf, ofp);
        }
    }
    else
    {
        while (readart(g_buf, sizeof g_buf))
        {
            if (mime_EndOfSection(g_buf))
            {
                break;
            }
            std::fputs(g_buf, ofp);
        }
    }

    return DECODE_MAYBEDONE;
}

static int s_word_wrap_in_pre{};
static int s_normal_word_wrap{};
static int s_word_wrap{};

// clang-format off
static const char* s_named_entities[] = {
    "lt",       "<",
    "gt",       ">",
    "amp",      "&",
    "quot",     "\"",
    "apo",      "'",    /* non-standard but seen in the wild */
#ifndef USE_UTF_HACK
    "nbsp",     " ",
    "ensp",     " ",    /* seen in the wild */
    "lsquo",    "'",
    "rsquo",    "'",
    "ldquo",    "\"",
    "rdquo",    "\"",
    "ndash",    "-",
    "mdash",    "-",
    "copy",     "(C)",
    "trade",    "(TM)",
    "zwsp",     "",
    "zwnj",     "",
    "ccedil",   "c",    /* per charsubst.c */
    "eacute",   "e",
#else /* USE_UTF_HACK */
    "nbsp",     " ",
    "ensp",     " ",    /* U+2002 */
    "lsquo",    "‘",
    "rsquo",    "’",
    "ldquo",    "“",
    "rdquo",    "”",
    "ndash",    "–",
    "mdash",    "—",
    "copy",     "©",
    "trade",    "™",
    "zwsp",     "​",
    "zwnj",     "‌",
    "ccedil",   "ç",
    "eacute",   "é",
#endif
    nullptr,    nullptr,
};
// clang-format on

int filter_html(char *t, const char *f)
{
    static char tagword[32];
    static int tagword_len;
    char* bp;
    char* cp;

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
    s_word_wrap = g_mime_section->html & HF_IN_PRE? s_word_wrap_in_pre
                                                : s_normal_word_wrap;
    if (!g_mime_section->html_line_start)
    {
        g_mime_section->html_line_start = t - g_artbuf;
    }

    if (!g_mime_section->html_blks)
    {
        g_mime_section->html_blks = (HBlock*)safemalloc(HTML_MAX_BLOCKS
                                                  * sizeof (HBlock));
    }

    for (bp = t; *f; f++)
    {
        if (g_mime_section->html & HF_IN_DQUOTE)
        {
            if (*f == '"')
            {
                g_mime_section->html &= ~HF_IN_DQUOTE;
            }
            else if (tagword_len < sizeof tagword - 1)
            {
                tagword[tagword_len++] = *f;
            }
        }
        else if (g_mime_section->html & HF_IN_SQUOTE)
        {
            if (*f == '\'')
            {
                g_mime_section->html &= ~HF_IN_SQUOTE;
            }
            else if (tagword_len < sizeof tagword - 1)
            {
                tagword[tagword_len++] = *f;
            }
        }
        else if (g_mime_section->html & HF_IN_TAG)
        {
            if (*f == '>')
            {
                g_mime_section->html &= ~(HF_IN_TAG | HF_IN_COMMENT);
                tagword[tagword_len] = '\0';
                if (*tagword == '/')
                {
                    t = tag_action(t, tagword + 1, CLOSING_TAG);
                }
                else
                {
                    t = tag_action(t, tagword, OPENING_TAG);
                }
            }
            else if (*f == '-' && f[1] == '-')
            {
                f++;
                g_mime_section->html |= HF_IN_COMMENT;
            }
            else if (*f == '"')
            {
                g_mime_section->html |= HF_IN_DQUOTE;
            }
            else if (*f == '\'')
            {
                g_mime_section->html |= HF_IN_SQUOTE;
            }
            else if (tagword_len < sizeof tagword - 1)
            {
                tagword[tagword_len++] = at_grey_space(f)? ' ' : *f;
            }
        }
        else if (g_mime_section->html & HF_IN_COMMENT)
        {
            if (*f == '-' && f[1] == '-')
            {
                f++;
                g_mime_section->html &= ~HF_IN_COMMENT;
            }
        }
        else if (*f == '<')
        {
            tagword_len = 0;
            g_mime_section->html |= HF_IN_TAG;
        }
        else if (g_mime_section->html & HF_IN_HIDING)
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
                else if (!(det == '-' || std::isalnum(det))) /* see html-spec.txt 3.2.1 */
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
        else if (*f == '&' && std::isalpha(f[1]))   /* see html-spec.txt 3.2.1 */
        {
            int i;
            int entity_found = 0;
            t = output_prep(t);
            for (i = 0; s_named_entities[i] != nullptr; i += 2)
            {
                int n = std::strlen(s_named_entities[i]);
                if (string_case_equal(f + 1, s_named_entities[i], n))
                {
                    char det = f[n + 1];
                    if (det == ';')
                    {
                        entity_found = n + 1;
                    }
                    else if (!(det == '-' || isalnum(det))) /* see html-spec.txt 3.2.1 */
                    {
                        entity_found = n;
                    }
                }
                if (entity_found)
                {
                    break;
                }
            }
            if (entity_found)
            {
                for (int j = 0;; j++)
                {
                    char c = s_named_entities[i + 1][j];
                    if (c == '\0')
                    {
                        break;
                    }
                    *t++ = c;
                }
                f += entity_found;
            }
            else
            {
                *t++ = *f;
            }
            g_mime_section->html |= HF_NL_OK|HF_P_OK|HF_SPACE_OK;
        }
        else if ((*f == ' ' || at_grey_space(f)) && !(g_mime_section->html & HF_IN_PRE))
        {
            /* We don't want to call output_prep() here. */
            if (*f == ' ' || (g_mime_section->html & HF_SPACE_OK))
            {
                g_mime_section->html &= ~HF_SPACE_OK;
                *t++ = ' ';
            }
            /* In non-PRE mode spaces should be collapsed */
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
        else if (*f == '\n')   /* Handle the HF_IN_PRE case */
        {
            t = output_prep(t);
            g_mime_section->html |= HF_NL_OK;
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
            g_mime_section->html |= HF_NL_OK|HF_P_OK|HF_SPACE_OK;
        }

        if (s_word_wrap && t - g_artbuf - g_mime_section->html_line_start > g_tc_COLS)
        {
            char* line_start = g_mime_section->html_line_start + g_artbuf;
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
                    g_mime_section->html_line_start += g_tc_COLS;
                    cp = nullptr;
                }
            }
            if (cp)
            {
                const HtmlFlags flag_save = g_mime_section->html;
                g_mime_section->html |= HF_NL_OK;
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
                g_mime_section->html = flag_save;
            }
        }
    }
    *t = '\0';

    return t - bp;
}
#undef XX

static char s_bullets[3] = {'*', 'o', '+'};
static char s_letters[2] = {'a', 'A'};
static char s_roman_letters[] = { 'M', 'D', 'C', 'L', 'X', 'V', 'I'};
static int  s_roman_values[]  = {1000, 500, 100,  50, 10,   5,   1 };

static char *tag_action(char *t, char *word, bool opening_tag)
{
    char *cp;
    int   j;
    int   tnum;
    int   itype;
    int   cnt;
    int   num;
    bool match = false;
    HBlock* blks = g_mime_section->html_blks;

    for (cp = word; *cp && *cp != ' '; cp++)
    {
    }
    int len = cp - word;

    if (!std::isalpha(*word))
    {
        return t;
    }
    int ch = std::isupper(*word) ? std::tolower(*word) : *word;
    for (tnum = 0; tnum < LAST_TAG && *s_tagattr[tnum].name != ch; tnum++)
    {
    }
    for (; tnum < LAST_TAG && *s_tagattr[tnum].name == ch; tnum++)
    {
        if (len == s_tagattr[tnum].length //
            && string_case_equal(word, s_tagattr[tnum].name, len))
        {
            match = true;
            break;
        }
    }
    if (!match)
    {
        return t;
    }

    if (!opening_tag && !(s_tagattr[tnum].flags & (TF_BLOCK|TF_HAS_CLOSE)))
    {
        return t;
    }

    if (g_mime_section->html & HF_IN_HIDING
     && (opening_tag || tnum != blks[g_mime_section->html_blkcnt-1].tnum))
    {
        return t;
    }

    if (s_tagattr[tnum].flags & TF_BR)
    {
        g_mime_section->html |= HF_NL_OK;
    }

    if (opening_tag)
    {
        if (s_tagattr[tnum].flags & TF_NL)
        {
            t = output_prep(t);
            t = do_newline(t, HF_NL_OK);
        }
        if ((num = s_tagattr[tnum].flags & (TF_P | TF_LIST)) == TF_P //
            || (num == (TF_P | TF_LIST) && !(g_mime_section->html & HF_COMPACT)))
        {
            t = output_prep(t);
            t = do_newline(t, HF_P_OK);
        }
        if (s_tagattr[tnum].flags & TF_SPACE)
        {
            if (g_mime_section->html & HF_SPACE_OK)
            {
                g_mime_section->html &= ~HF_SPACE_OK;
                *t++ = ' ';
            }
        }
        if (s_tagattr[tnum].flags & TF_TAB)
        {
            if (g_mime_section->html & HF_NL_OK)
            {
                g_mime_section->html &= ~HF_SPACE_OK;
                *t++ = '\t';
            }
        }

        if (s_tagattr[tnum].flags & TF_BLOCK //
            && g_mime_section->html_blkcnt < HTML_MAX_BLOCKS)
        {
            j = g_mime_section->html_blkcnt++;
            blks[j].tnum = tnum;
            blks[j].indent = 0;
            blks[j].cnt = 0;

            if (s_tagattr[tnum].flags & TF_LIST)
            {
                g_mime_section->html |= HF_COMPACT;
            }
            else
            {
                g_mime_section->html &= ~HF_COMPACT;
            }
        }
        else
        {
            j = g_mime_section->html_blkcnt - 1;
        }

        if ((s_tagattr[tnum].flags & (TF_BLOCK|TF_HIDE)) == (TF_BLOCK|TF_HIDE))
        {
            g_mime_section->html |= HF_IN_HIDING;
        }

        switch (tnum)
        {
        case TAG_BLOCKQUOTE:
            if (((cp = find_attr(word, "type")) != nullptr && string_case_equal(cp, "cite", 4)) ||
                ((cp = find_attr(word, "style")) != nullptr && string_case_equal(cp, "border-left:", 12)))
            {
                blks[j].indent = '>';
            }
            else
            {
                blks[j].indent = ' ';
            }
            break;

        case TAG_HR:
            t = output_prep(t);
              *t++ = '-';
              *t++ = '-';
            g_mime_section->html |= HF_NL_OK;
            t = do_newline(t, HF_NL_OK);
            break;

        case TAG_IMG:
            t = output_prep(t);
            if (g_mime_section->html & HF_SPACE_OK)
            {
                *t++ = ' ';
            }
            std::strcpy(t, "[Image] ");
            t += 8;
            g_mime_section->html &= ~HF_SPACE_OK;
            break;

        case TAG_OL:
            itype = 4;
            cp = find_attr(word, "type");
            if (cp != nullptr)
            {
                switch (*cp)
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

        case TAG_UL:
            itype = 1;
            cp = find_attr(word, "type");
            if (cp != nullptr)
            {
                switch (*cp)
                {
                case 'd': case 'D':  itype = 1;  break;
                case 'c': case 'C':  itype = 2;  break;
                case 's': case 'S':  itype = 3;  break;
                }
            }
            else
            {
                for (int i = 0; i < g_mime_section->html_blkcnt; i++)
                {
                    if (blks[i].indent && blks[i].indent < ' ')
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

        case TAG_LI:
            t = output_prep(t);
            ch = j < 0? ' ' : blks[j].indent;
            switch (ch)
            {
            case 1: case 2: case 3:
                t[-2] = s_bullets[ch-1];
                break;

            case 4:
                std::sprintf(t-4, "%2d. ", ++blks[j].cnt);
                if (*t)
                {
                    t += std::strlen(t);
                }
                break;

            case 5: case 6:
                cnt = blks[j].cnt++;
                if (cnt >= 26*26)
                {
                    cnt = 0;
                    blks[j].cnt = 0;
                }
                if (cnt >= 26)
                {
                    t[-4] = s_letters[ch - 5] + cnt / 26 - 1;
                }
                t[-3] = s_letters[ch-5] + cnt % 26;
                t[-2] = '.';
                break;

            case 7:
                for (int i = 0; i < 7; i++)
                {
                    if (std::isupper(s_roman_letters[i]))
                    {
                        s_roman_letters[i] = std::tolower(s_roman_letters[i]);
                    }
                }
                goto roman_numerals;

            case 8:
                for (int i = 0; i < 7; i++)
                {
                    if (std::islower(s_roman_letters[i]))
                    {
                        s_roman_letters[i] = std::toupper(s_roman_letters[i]);
                    }
                }
              roman_numerals:
                cp = t - 6;
                cnt = ++blks[j].cnt;
                for (int i = 0; cnt && i < 7; i++)
                {
                    num = s_roman_values[i];
                    while (cnt >= num)
                    {
                        *cp++ = s_roman_letters[i];
                        cnt -= num;
                    }
                    j = (i | 1) + 1;
                    if (j < 7)
                    {
                        num -= s_roman_values[j];
                        if (cnt >= num)
                        {
                            *cp++ = s_roman_letters[j];
                            *cp++ = s_roman_letters[i];
                            cnt -= num;
                        }
                    }
                }
                if (cp < t - 2)
                {
                    t -= 2;
                    for (cnt = t - cp; cp-- != t - 4; )
                    {
                        cp[cnt] = *cp;
                    }
                    while (cnt--)
                    {
                        *++cp = ' ';
                    }
                }
                else
                {
                    t = cp;
                }
                *t++ = '.';
                *t++ = ' ';
                break;

            default:
                *t++ = '*';
                *t++ = ' ';
                break;
            }
            g_mime_section->html |= HF_NL_OK|HF_P_OK;
            break;

        case TAG_PRE:
            g_mime_section->html |= HF_IN_PRE;
            s_word_wrap = s_word_wrap_in_pre;
            break;
        }
    }
    else
    {
        if (s_tagattr[tnum].flags & TF_BLOCK)
        {
            for (j = g_mime_section->html_blkcnt; j--;)
            {
                if (blks[j].tnum == tnum)
                {
                    for (int i = g_mime_section->html_blkcnt; --i > j;)
                    {
                        t = tag_action(t, s_tagattr[blks[i].tnum].name,
                                       CLOSING_TAG);
                    }
                    g_mime_section->html_blkcnt = j;
                    break;
                }
            }
            g_mime_section->html &= ~HF_IN_HIDING;
            while (j-- > 0)
            {
                if (s_tagattr[blks[j].tnum].flags & TF_HIDE)
                {
                    g_mime_section->html |= HF_IN_HIDING;
                    break;
                }
            }
        }

        j = g_mime_section->html_blkcnt - 1;
        if (j >= 0 && s_tagattr[blks[j].tnum].flags & TF_LIST)
        {
            g_mime_section->html |= HF_COMPACT;
        }
        else
        {
            g_mime_section->html &= ~HF_COMPACT;
        }

        if (s_tagattr[tnum].flags & TF_NL && g_mime_section->html & HF_NL_OK)
        {
            g_mime_section->html |= HF_QUEUED_NL;
            g_mime_section->html &= ~HF_SPACE_OK;
        }
        if ((num = s_tagattr[tnum].flags & (TF_P | TF_LIST)) == TF_P //
            || (num == (TF_P | TF_LIST) && !(g_mime_section->html & HF_COMPACT)))
        {
            if (g_mime_section->html & HF_P_OK)
            {
                g_mime_section->html |= HF_QUEUED_P;
                g_mime_section->html &= ~HF_SPACE_OK;
            }
        }

        switch (tnum)
        {
        case TAG_PRE:
            g_mime_section->html &= ~HF_IN_PRE;
            s_word_wrap = s_normal_word_wrap;
            break;
        }
    }

#ifdef DEBUGGING
                                                std::printf("%*s %% -> ", 4 + 25, "");
    if (g_mime_section->html == 0)              std::printf("0 ");
    if (g_mime_section->html & HF_IN_TAG)       std::printf("HF_IN_TAG ");
    if (g_mime_section->html & HF_IN_COMMENT)   std::printf("HF_IN_COMMENT ");
    if (g_mime_section->html & HF_IN_HIDING)    std::printf("HF_IN_HIDING ");
    if (g_mime_section->html & HF_IN_PRE)       std::printf("HF_IN_PRE ");
    if (g_mime_section->html & HF_IN_DQUOTE)    std::printf("HF_IN_DQUOTE ");
    if (g_mime_section->html & HF_IN_SQUOTE)    std::printf("HF_IN_SQUOTE ");
    if (g_mime_section->html & HF_QUEUED_P)     std::printf("HF_QUEUED_P ");
    if (g_mime_section->html & HF_P_OK)         std::printf("HF_P_OK ");
    if (g_mime_section->html & HF_QUEUED_NL)    std::printf("HF_QUEUED_NL ");
    if (g_mime_section->html & HF_NL_OK)        std::printf("HF_NL_OK ");
    if (g_mime_section->html & HF_NEED_INDENT)  std::printf("HF_NEED_INDENT ");
    if (g_mime_section->html & HF_SPACE_OK)     std::printf("HF_SPACE_OK ");
    if (g_mime_section->html & HF_COMPACT)      std::printf("HF_COMPACT ");
    std::printf("\n");
#endif
    return t;
}

static char *output_prep(char *t)
{
    if (g_mime_section->html & HF_QUEUED_P)
    {
        g_mime_section->html &= ~HF_QUEUED_P;
        t = do_newline(t, HF_P_OK);
    }
    if (g_mime_section->html & HF_QUEUED_NL)
    {
        g_mime_section->html &= ~HF_QUEUED_NL;
        t = do_newline(t, HF_NL_OK);
    }
    return t + do_indent(t);
}

static char *do_newline(char *t, HtmlFlags flag)
{
    if (g_mime_section->html & flag)
    {
        g_mime_section->html &= ~(flag|HF_SPACE_OK);
        t += do_indent(t);
        *t++ = '\n';
        g_mime_section->html_line_start = t - g_artbuf;
        g_mime_section->html |= HF_NEED_INDENT;
    }
    return t;
}

static int do_indent(char *t)
{
    int spaces;
    int len = 0;

    if (!(g_mime_section->html & HF_NEED_INDENT))
    {
        return len;
    }

    if (t)
    {
        g_mime_section->html &= ~HF_NEED_INDENT;
    }

    HBlock *blks = g_mime_section->html_blks;
    if (blks != nullptr)
    {
        for (int j = 0; j < g_mime_section->html_blkcnt; j++)
        {
            int ch = blks[j].indent;
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
    }

    return len;
}

static char *find_attr(char *str, const char *attr)
{
    int len = std::strlen(attr);
    char* cp = str;
    char* s;

    while ((cp = std::strchr(cp + 1, '=')) != nullptr)
    {
        for (s = cp; s[-1] == ' '; s--)
        {
        }
        while (cp[1] == ' ')
        {
            cp++;
        }
        if (s - str > len && s[-len-1] == ' ' && string_case_equal(s-len, attr,len))
        {
            return cp + 1;
        }
    }
    return nullptr;
}
