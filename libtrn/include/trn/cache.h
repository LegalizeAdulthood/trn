/* trn/cache.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_CACHE_H
#define TRN_CACHE_H

#include <trn/Article.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/kfile.h>
#include <trn/list.h>
#include <trn/ngdata.h>
#include <trn/Subject.h>

#include <cstdint>
#include <ctime>

struct List;

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
