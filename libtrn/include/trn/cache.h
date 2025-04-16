/* trn/cache.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_CACHE_H
#define TRN_CACHE_H

#include "trn/List.h"
#include "trn/ngdata.h"
#include "trn/enum-flags.h"
#include "trn/hash.h"
#include "trn/head.h"
#include "trn/kfile.h"

#include <cstdint>
#include <ctime>

struct Article;
struct List;

enum SubjectFlags : std::uint16_t
{
    SF_NONE = 0x0000,
    SF_SEL = 0x0001,
    SF_DEL = 0x0002,
    SF_DELSEL = 0x0004,
    SF_OLDVISIT = 0x0008,
    SF_INCLUDED = 0x0010,
    SF_VISIT = 0x0200,
    SF_WASSELECTED = 0x0400,
    SF_SUBJTRUNCED = 0x1000,
    SF_ISOSUBJ = 0x2000
};
DECLARE_FLAGS_ENUM(SubjectFlags, std::uint16_t);

/* Subjects get their own structure */
struct Subject
{
    Subject      *next;
    Subject      *prev;
    Article      *articles;
    Article      *thread;
    Subject      *thread_link;
    char         *str;
    std::time_t   date;
    SubjectFlags flags;
    short         misc; /* used for temporary totals and subject numbers */
};

enum ArticleFlags : std::uint16_t
{
    AF_NONE = 0,
    AF_SEL = 0x0001,
    AF_DEL = 0x0002,
    AF_DELSEL = 0x0004,
    AF_OLDSEL = 0x0008,
    AF_INCLUDED = 0x0010,
    AF_UNREAD = 0x0020,
    AF_CACHED = 0x0040,
    AF_THREADED = 0x0080,
    AF_EXISTS = 0x0100,
    AF_HAS_RE = 0x0200,
    AF_KCHASE = 0x0400,
    AF_MCHASE = 0x0800,
    AF_YANKBACK = 0x1000,
    AF_TMPMEM = 0x4000,
    AF_FAKE = 0x8000,
};
DECLARE_FLAGS_ENUM(ArticleFlags, std::uint16_t);

enum ArticleFlags2 : std::uint16_t
{
    AF2_NONE = 0x0000,
    AF2_WASUNREAD = 0x0001,
    AF2_NODEDRAWN = 0x0002,
    AF2_CHANGED = 0x0004,
    AF2_BOGUS = 0x0008
};
DECLARE_FLAGS_ENUM(ArticleFlags2, std::uint16_t);

/* specific scoreflag meanings:  (note: bad placement, but where else?) */
enum ScoreFlags
{
    SFLAG_NONE = 0x00,
    SFLAG_AUTHOR = 0x01, /* author has a score (match on FROM: line) */
    SFLAG_SCORED = 0x10 /* if true, the article has been scored */
};
DECLARE_FLAGS_ENUM(ScoreFlags, std::uint16_t)

/* This is our article-caching structure */
struct Article
{
    ART_NUM        num;
    std::time_t         date;
    Subject       *subj;
    char          *from;
    char          *msgid;
    char          *xrefs;
    Article       *parent;    /* parent article */
    Article       *child1;    /* first child of a chain */
    Article       *sibling;   /* our next sibling */
    Article       *subj_next; /* next article in subject order */
    long           bytes;
    long           lines;
    int            score;
    ScoreFlags    scoreflags;
    ArticleFlags  flags;  /* article state flags */
    ArticleFlags2 flags2; /* more state flags */
    AutoKillFlags autofl; /* auto-processing flags */
};

/* See trn/kfile.h for the AUTO_* flags */
enum : bool
{
    DONT_FILL_CACHE = false,
    FILL_CACHE = true
};

extern List     *g_article_list; /* a list of ARTICLEs */
extern Article **g_artptr_list;  /* the article-selector creates this */
extern Article **g_artptr;       /* ditto -- used for article order */
extern ART_NUM   g_artptr_list_size;
extern ART_NUM   g_srchahead; /* are we in subject scan mode? (if so, contains art # found or -1) */
extern ART_NUM   g_first_cached;
extern ART_NUM   g_last_cached;
extern bool      g_cached_all_in_range;
extern Article  *g_sentinel_artp;
extern Subject  *g_first_subject;
extern Subject  *g_last_subject;
extern bool      g_untrim_cache;
extern int       g_join_subject_len; /* -J */
extern int       g_olden_days;       /* -o */
extern char      g_auto_select_postings; /* -p */

void cache_init();
void build_cache();
void close_cache();
void cache_article(Article *ap);
void check_for_near_subj(Article *ap);
void change_join_subject_len(int len);
void check_poster(Article *ap);
void uncache_article(Article *ap, bool remove_empties);
char *fetchcache(ART_NUM artnum, HeaderLineType which_line, bool fill_cache);
char *get_cached_line(Article *ap, HeaderLineType which_line, bool no_truncs);
void set_subj_line(Article *ap, char *subj, int size);
int decode_header(char *to, char *from, int size);
void dectrl(char *str);
void set_cached_line(Article *ap, int which_line, char *s);
int subject_cmp(const char *key, int keylen, HashDatum data);
#ifdef PENDING
void look_ahead();
#endif
void cache_until_key();
#ifdef PENDING
bool cache_subjects();
#endif
bool cache_xrefs();
bool cache_all_arts();
bool cache_unread_arts();
bool art_data(ART_NUM first, ART_NUM last, bool cheating, bool all_articles);
bool cache_range(ART_NUM first, ART_NUM last);
void clear_article(Article *ap);

inline Article *article_ptr(ART_NUM an)
{
    return (Article *) listnum2listitem(g_article_list, an);
}
inline ART_NUM article_num(const Article *ap)
{
    return ap->num;
}
inline bool article_hasdata(ART_NUM an)
{
    return existing_listnum(g_article_list, an, 0) != 0;
}
inline Article *article_find(ART_NUM an)
{
    return an <= g_lastart && article_hasdata(an) ? article_ptr(an) : nullptr;
}
inline bool article_walk(bool (*callback)(char *, int), int arg)
{
    return walk_list(g_article_list, callback, arg);
}
inline ART_NUM article_first(ART_NUM an)
{
    return existing_listnum(g_article_list, an, 1);
}
inline ART_NUM article_next(ART_NUM an)
{
    return existing_listnum(g_article_list, an + 1, 1);
}
inline ART_NUM article_last(ART_NUM an)
{
    return existing_listnum(g_article_list, an, -1);
}
inline ART_NUM article_prev(ART_NUM an)
{
    return existing_listnum(g_article_list, an - 1, -1);
}
inline Article *article_nextp(Article *ap)
{
    return (Article *) next_listitem(g_article_list, (char *) ap);
}
inline bool article_exists(ART_NUM an)
{
    return article_ptr(an)->flags & AF_EXISTS;
}
inline bool article_unread(ART_NUM an)
{
    return article_ptr(an)->flags & AF_UNREAD;
}
inline bool was_read(ART_NUM an)
{
    return !article_hasdata(an) || !article_unread(an);
}
inline bool is_available(ART_NUM an)
{
    return an <= g_lastart && article_hasdata(an) && article_exists(an);
}
inline bool is_unavailable(ART_NUM an)
{
    return !is_available(an);
}
inline bool article_scored(ART_NUM an)
{
    return article_ptr(an)->scoreflags & SFLAG_SCORED;
}

#endif
