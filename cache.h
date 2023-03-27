/* cache.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_CACHE_H
#define TRN_CACHE_H

#include "enum-flags.h"
#include "hash.h"
#include "head.h"
#include "list.h"
#include "ngdata.h"

#include <cstdint>

struct ARTICLE;
struct LIST;

/* subject flags */
enum subject_flags : std::uint16_t
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
DECLARE_FLAGS_ENUM(subject_flags, std::uint16_t);

/* Subjects get their own structure */
struct SUBJECT
{
    SUBJECT      *next;
    SUBJECT      *prev;
    ARTICLE      *articles;
    ARTICLE      *thread;
    SUBJECT      *thread_link;
    char         *str;
    time_t        date;
    subject_flags flags;
    short         misc; /* used for temporary totals and subject numbers */
};

/* This is our article-caching structure */
struct ARTICLE
{
    ART_NUM num;
    time_t date;
    SUBJECT* subj;
    char* from;
    char* msgid;
    char* xrefs;
    ARTICLE* parent;		/* parent article */
    ARTICLE* child1;		/* first child of a chain */
    ARTICLE* sibling;		/* our next sibling */
    ARTICLE* subj_next;		/* next article in subject order */
    long bytes;
    long lines;
    int score;
    unsigned short scoreflags;
    unsigned short flags;	/* article state flags */
    unsigned short flags2;	/* more state flags */
    unsigned short autofl;	/* auto-processing flags */
};

/* article flags */
enum
{
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
    AF_FROMTRUNCED = 0x2000,
    AF_TMPMEM = 0x4000,
    AF_FAKE = 0x8000,
    AF2_WASUNREAD = 0x0001,
    AF2_NODEDRAWN = 0x0002,
    AF2_CHANGED = 0x0004,
    AF2_BOGUS = 0x0008
};

/* See kfile.h for the AUTO_* flags */

#define DONT_FILL_CACHE	false
#define FILL_CACHE	true

extern LIST *g_article_list;    /* a list of ARTICLEs */
extern ARTICLE **g_artptr_list; /* the article-selector creates this */
extern ARTICLE **g_artptr;      /* ditto -- used for article order */
extern ART_NUM g_artptr_list_size;
extern ART_NUM g_srchahead; /* are we in subject scan mode? (if so, contains art # found or -1) */
extern ART_NUM g_first_cached;
extern ART_NUM g_last_cached;
extern bool g_cached_all_in_range;
extern ARTICLE *g_sentinel_artp;
extern SUBJECT *g_first_subject;
extern SUBJECT *g_last_subject;
extern bool g_untrim_cache;

void cache_init();
void build_cache();
void close_cache();
void cache_article(ARTICLE *ap);
void check_for_near_subj(ARTICLE *ap);
void change_join_subject_len(int len);
void check_poster(ARTICLE *ap);
void uncache_article(ARTICLE *ap, bool remove_empties);
char *fetchcache(ART_NUM artnum, header_line_type which_line, bool fill_cache);
char *get_cached_line(ARTICLE *ap, header_line_type which_line, bool no_truncs);
void set_subj_line(ARTICLE *ap, char *subj, int size);
int decode_header(char *to, char *from, int size);
void dectrl(char *str);
void set_cached_line(ARTICLE *ap, int which_line, char *s);
int subject_cmp(const char *key, int keylen, HASHDATUM data);
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
void clear_article(ARTICLE *ap);

inline ARTICLE *article_ptr(ART_NUM an)
{
    return (ARTICLE *) listnum2listitem(g_article_list, an);
}
inline ART_NUM article_num(const ARTICLE *ap)
{
    return ap->num;
}
inline bool article_hasdata(ART_NUM an)
{
    return existing_listnum(g_article_list, an, 0) != 0;
}
inline ARTICLE *article_find(ART_NUM an)
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
inline ARTICLE *article_nextp(ARTICLE *ap)
{
    return (ARTICLE *) next_listitem(g_article_list, (char *) ap);
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

#endif
