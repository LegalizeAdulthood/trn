/* trn/cache.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_CACHE_H
#define TRN_CACHE_H

#include "trn/enum-flags.h"
#include "trn/hash.h"
#include "trn/head.h"
#include "trn/kfile.h"
#include "trn/list.h"
#include "trn/ngdata.h"

#include <cstdint>
#include <ctime>

struct Article;
struct List;

enum SubjectFlags : std::uint16_t
{
    SF_NONE = 0x0000,
    SF_SEL = 0x0001,
    SF_DEL = 0x0002,
    SF_DEL_SEL = 0x0004,
    SF_OLD_VISIT = 0x0008,
    SF_INCLUDED = 0x0010,
    SF_VISIT = 0x0200,
    SF_WAS_SELECTED = 0x0400,
    SF_SUBJ_TRUNCATED = 0x1000,
    SF_ISO_SUBJECT = 0x2000
};
DECLARE_FLAGS_ENUM(SubjectFlags, std::uint16_t);

// Subjects get their own structure
struct Subject
{
    Subject     *next;
    Subject     *prev;
    Article     *articles;
    Article     *thread;
    Subject     *thread_link;
    char        *str;
    std::time_t  date;
    SubjectFlags flags;
    short        misc; // used for temporary totals and subject numbers
};

enum ArticleFlags : std::uint16_t
{
    AF_NONE = 0,
    AF_SEL = 0x0001,
    AF_DEL = 0x0002,
    AF_DEL_SEL = 0x0004,
    AF_OLD_SEL = 0x0008,
    AF_INCLUDED = 0x0010,
    AF_UNREAD = 0x0020,
    AF_CACHED = 0x0040,
    AF_THREADED = 0x0080,
    AF_EXISTS = 0x0100,
    AF_HAS_RE = 0x0200,
    AF_K_CHASE = 0x0400,
    AF_M_CHASE = 0x0800,
    AF_YANK_BACK = 0x1000,
    AF_TMP_MEM = 0x4000,
    AF_FAKE = 0x8000,
};
DECLARE_FLAGS_ENUM(ArticleFlags, std::uint16_t);

enum ArticleFlags2 : std::uint16_t
{
    AF2_NONE = 0x0000,
    AF2_WAS_UNREAD = 0x0001,
    AF2_NODE_DRAWN = 0x0002,
    AF2_CHANGED = 0x0004,
    AF2_BOGUS = 0x0008
};
DECLARE_FLAGS_ENUM(ArticleFlags2, std::uint16_t);

// specific score flag meanings:  (note: bad placement, but where else?)
enum ScoreFlags
{
    SFLAG_NONE = 0x00,
    SFLAG_AUTHOR = 0x01, // author has a score (match on FROM: line)
    SFLAG_SCORED = 0x10  // if true, the article has been scored
};
DECLARE_FLAGS_ENUM(ScoreFlags, std::uint16_t)

// This is our article-caching structure
struct Article
{
    void       cache_article();
    void       check_for_near_subj();
    void       check_poster();
    void       uncache_article(bool remove_empties);
    char      *get_cached_line(HeaderLineType which_line, bool no_truncs);
    void       set_subj_line(char *subj, int size);
    void       set_cached_line(int which_line, char *s);
    void       clear_article();
    void       one_more();
    void       one_less();
    void       one_missing();
    void       unmark_as_read();
    void       set_read();
    void       delay_unmark();
    void       mark_as_read();
    Article   *bump_article();
    Article   *next_article();
    Article   *prev_article();
    void       select_article(AutoKillFlags auto_flags);
    void       select_articles_subject(AutoKillFlags auto_flags);
    void       select_articles_thread(AutoKillFlags auto_flags);
    void       select_thread(AutoKillFlags auto_flags);
    void       deselect_article(AutoKillFlags auto_flags);
    void       deselect_articles_subject();
    void       deselect_articles_thread();
    void       deselect_thread();
    void       kill_articles_subject(AutoKillFlags auto_flags);
    void       kill_articles_thread(AutoKillFlags auto_flags);
    void       kill_thread(AutoKillFlags auto_flags);
    void       unkill_thread();
    void       clear_thread();
    void       change_auto_flags(AutoKillFlags auto_flag);
    void       clear_auto_flags();
    void       perform_auto_flags(AutoKillFlags thread_flags, AutoKillFlags subj_flags, AutoKillFlags chain_flags);
    bool       valid_article();
    void       thread_article(char *references);
    void       link_child();
    char      *compress_date(int size) const;
    char       thread_letter();
    ArticleNum article_num() const
    {
        return m_num;
    }

    ArticleNum    m_num;
    std::time_t   m_date;
    Subject      *m_subj;
    char         *m_from;
    char         *m_msg_id;
    char         *m_xrefs;
    Article      *m_parent;    // parent article
    Article      *m_child1;    // first child of a chain
    Article      *m_sibling;   // our next sibling
    Article      *m_subj_next; // next article in subject order
    long          m_bytes;
    long          m_lines;
    int           m_score;
    ScoreFlags    m_score_flags;
    ArticleFlags  m_flags;      // article state flags
    ArticleFlags2 m_flags2;     // more state flags
    AutoKillFlags m_auto_flags; // auto-processing flags
};

// See trn/kfile.h for the AUTO_* flags
enum : bool
{
    DONT_FILL_CACHE = false,
    FILL_CACHE = true
};

extern List      *g_article_list; // a list of Articles
extern Article  **g_art_ptr_list; // the article-selector creates this
extern Article  **g_art_ptr;      // ditto -- used for article order
extern ArticleNum g_art_ptr_list_size;
extern ArticleNum g_search_ahead; // are we in subject scan mode? (if so, contains art # found or -1)
extern ArticleNum g_first_cached;
extern ArticleNum g_last_cached;
extern bool       g_cached_all_in_range;
extern Article   *g_sentinel_art_ptr;
extern Subject   *g_first_subject;
extern Subject   *g_last_subject;
extern bool       g_untrim_cache;
extern int        g_join_subject_len;     // -J
extern int        g_olden_days;           // -o
extern char       g_auto_select_postings; // -p

void  cache_init();
void  build_cache();
void  close_cache();
void  change_join_subject_len(int len);
char *fetch_cache(ArticleNum art_num, HeaderLineType which_line, bool fill_cache);
int   decode_header(char *to, char *from, int size);
void  dectrl(char *str);
int   subject_cmp(const char *key, int key_len, HashDatum data);
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
bool art_data(ArticleNum first, ArticleNum last, bool cheating, bool all_articles);
bool cache_range(ArticleNum first, ArticleNum last);

inline Article *article_ptr(ArticleNum an)
{
    return (Article *) list_get_item(g_article_list, an.value_of());
}
inline bool article_hasdata(ArticleNum an)
{
    return existing_list_index(g_article_list, an.value_of(), 0) != 0;
}
inline Article *article_find(ArticleNum an)
{
    return an <= g_last_art && article_hasdata(an) ? article_ptr(an) : nullptr;
}
inline bool article_walk(bool (*callback)(char *, int), int arg)
{
    return walk_list(g_article_list, callback, arg);
}
inline ArticleNum article_first(ArticleNum an)
{
    return ArticleNum{existing_list_index(g_article_list, an.value_of(), 1)};
}
inline ArticleNum article_next(ArticleNum an)
{
    return ArticleNum{existing_list_index(g_article_list, an.value_of() + 1, 1)};
}
inline ArticleNum article_last(ArticleNum an)
{
    return ArticleNum{existing_list_index(g_article_list, an.value_of(), -1)};
}
inline ArticleNum article_prev(ArticleNum an)
{
    return ArticleNum{existing_list_index(g_article_list, an.value_of() - 1, -1)};
}
inline Article *article_nextp(Article *ap)
{
    return (Article *) next_list_item(g_article_list, (char *) ap);
}
inline bool article_exists(ArticleNum an)
{
    return article_ptr(an)->m_flags & AF_EXISTS;
}
inline bool article_unread(ArticleNum an)
{
    return article_ptr(an)->m_flags & AF_UNREAD;
}
inline bool was_read(ArticleNum an)
{
    return !article_hasdata(an) || !article_unread(an);
}
inline bool is_available(ArticleNum an)
{
    return an <= g_last_art && article_hasdata(an) && article_exists(an);
}
inline bool is_unavailable(ArticleNum an)
{
    return !is_available(an);
}
inline bool article_scored(ArticleNum an)
{
    return article_ptr(an)->m_score_flags & SFLAG_SCORED;
}

#endif
