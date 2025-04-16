/* trn/head.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_HEAD_H
#define TRN_HEAD_H

#include <config/typedef.h>

#include "trn/enum-flags.h"
#include "trn/mempool.h"

#include <cstdint>

struct Article;

/* types of header lines
 * (These must stay in alphabetic order at least in the first letter.
 * Within each letter it helps to arrange in increasing likelihood.)
 */
enum HeaderLineType
{
    PAST_HEADER = 0,        /* body */
    SHOWN_LINE,             /* unrecognized but shown */
    HIDDEN_LINE,            /* unrecognized but hidden */
    CUSTOM_LINE,            /* to isolate a custom line */
    SOME_LINE,              /* default for unrecognized */
    HEAD_FIRST = SOME_LINE, /* first header line */
    AUTHOR_LINE,            /* Author */
    BYTES_LINE,             /* Bytes */
    CONTNAME_LINE,          /* Content-Name */
    CONTDISP_LINE,          /* Content-Disposition */
    CONTLEN_LINE,           /* Content-Length */
    CONTXFER_LINE,          /* Content-Transfer-Encoding */
    CONTTYPE_LINE,          /* Content-Type */
    DIST_LINE,              /* distribution */
    DATE_LINE,              /* date */
    EXPIR_LINE,             /* expires */
    FOLLOW_LINE,            /* followup-to */
    FROM_LINE,              /* from */
    INREPLY_LINE,           /* in-reply-to */
    KEYW_LINE,              /* keywords */
    LINES_LINE,             /* lines */
    MIMEVER_LINE,           /* mime-version */
    MSGID_LINE,             /* message-id */
    NEWSGROUPS_LINE,        /* newsgroups */
    PATH_LINE,              /* path */
    RVER_LINE,              /* relay-version */
    REPLY_LINE,             /* reply-to */
    REFS_LINE,              /* references */
    SUMRY_LINE,             /* summary */
    SUBJ_LINE,              /* subject */
    XREF_LINE,              /* xref */
    HEAD_LAST,              /* total # of headers */
};

enum HeaderTypeFlags : std::uint8_t
{
    HT_NONE = 0x0,
    HT_HIDE = 0x01,     /* hide this line */
    HT_MAGIC = 0x02,    /* do any special processing on this line */
    HT_CACHED = 0x04,   /* this information is cached article data */
    HT_DEFHIDE = 0x08,  /* hidden by default */
    HT_DEFMAGIC = 0x10, /* magic by default */
    HT_MAGICOK = 0x20,  /* magic even possible for line */
};
DECLARE_FLAGS_ENUM(HeaderTypeFlags, std::uint8_t);

struct HeaderType
{
    char          *name;   /* header line identifier */
    ArticlePosition        minpos; /* pointer to beginning of line in article */
    ArticlePosition        maxpos; /* pointer to end of line in article */
    char           length; /* the header's string length */
    HeaderTypeFlags flags;  /* the header's flags */
};

struct UserHeaderType
{
    char* name;             /* user-defined headers */
    char length;            /* the header's string length */
    char flags;             /* the header's flags */
};

extern HeaderType g_htype[HEAD_LAST];
extern UserHeaderType *g_user_htype;
extern short g_user_htypeix[26];
extern int g_user_htype_cnt;
extern int g_user_htype_max;
extern ArticleNum g_parsed_art;         /* the article number we've parsed */
extern HeaderLineType g_in_header; /* are we decoding the header? */
extern char *g_headbuf;

enum
{
    PREFETCH_SIZE = 5
};

void head_init();
void head_final();

#ifdef DEBUG
void dumpheader(char *where);
#endif
HeaderLineType set_line_type(char *bufptr, const char *colon);
HeaderLineType get_header_num(char *s);
void start_header(ArticleNum artnum);
void end_header_line();
bool parseline(char *art_buf, int newhide, int oldhide);
void end_header();
bool parseheader(ArticleNum artnum);
char *fetchlines(ArticleNum artnum, HeaderLineType which_line);
char *mp_fetchlines(ArticleNum artnum, HeaderLineType which_line, MemoryPool pool);
char *prefetchlines(ArticleNum artnum, HeaderLineType which_line, bool copy);
inline char *fetchsubj(ArticleNum artnum, bool copy)
{
    return prefetchlines(artnum, SUBJ_LINE, copy);
}
inline char *fetchfrom(ArticleNum artnum, bool copy)
{
    return prefetchlines(artnum, FROM_LINE, copy);
}
inline char *fetchxref(ArticleNum artnum, bool copy)
{
    return prefetchlines(artnum, XREF_LINE, copy);
}

#endif
