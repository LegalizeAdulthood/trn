/* trn/cache.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_CACHE_H
#define TRN_CACHE_H

#include <config/common.h>
#include <trn/Article.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/kfile.h>
#include <trn/ngdata.h>

#include <cstdint>
#include <ctime>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct Subject;

// See trn/kfile.h for the AUTO_* flags
enum : bool
{
    DONT_FILL_CACHE = false,
    FILL_CACHE = true
};

using ArticleWalker = bool(Article &article, int arg);

struct GroupArticleCache
{
    Article   *ptr(ArticleNum an);
    bool       has_data(ArticleNum an);
    Article   *find(ArticleNum an);
    bool       walk(ArticleWalker *callback, int arg);
    ArticleNum first(ArticleNum an);
    ArticleNum next(ArticleNum an);
    ArticleNum last(ArticleNum an);
    ArticleNum prev(ArticleNum an);
    Article   *nextp(Article *ap);
    void       clear();

    bool exists(ArticleNum an)
    {
        return ptr(an)->m_flags & AF_EXISTS;
    }
    bool unread(ArticleNum an)
    {
        return ptr(an)->m_flags & AF_UNREAD;
    }
    bool was_read(ArticleNum an)
    {
        return !has_data(an) || !unread(an);
    }
    bool is_available(ArticleNum an)
    {
        return an <= g_last_art && has_data(an) && exists(an);
    }
    bool is_unavailable(ArticleNum an)
    {
        return !is_available(an);
    }
    bool scored(ArticleNum an)
    {
        return ptr(an)->m_score_flags & SFLAG_SCORED;
    }

    std::map<ArticleNum, Article> article_list;
    ArticleNum                    first_cached{};
    ArticleNum                    last_cached{};
    bool                          cached_all_in_range{};
    NewsgroupData                *cached_ng{};
    std::time_t                   cached_time{};
};

extern GroupArticleCache g_cache;

struct SubjectIndex
{
    void     clear();
    void     clear_short_hash();
    void     check_for_near_subj(Subject *current_subj);
    void     rekey(Subject *subj, std::string &new_subj);
    Subject *find_or_create(std::string new_subj, std::string_view new_key);
    void     remove_empty(const Subject *subj);

    HashTable *hash{};
    HashTable *short_hash{};
    Subject   *first{};
    Subject   *last{};
    int        count{};
};

extern SubjectIndex g_subj;

struct ArticleOrder
{
    void sort(Article *anchor = nullptr);

    Article **begin()
    {
        return ptr_list.data();
    }

    Article **end()
    {
        return ptr_list.data() + ptr_list.size();
    }

    // TODO: private
    void build_ptrs();

    std::vector<Article *> ptr_list; // the article-selector creates this
    Article              **ptr{};    // ditto -- used for article order
};

extern ArticleOrder g_art_order;

extern ArticleNum                    g_search_ahead; // are we in subject scan mode? (if so, contains art # found or -1)
extern Article                      *g_sentinel_art_ptr;
extern bool                          g_untrim_cache;
extern int                           g_join_subject_len;     // -J
extern int                           g_olden_days;           // -o
extern char                          g_auto_select_postings; // -p

void        cache_init();
void        build_cache();
void        close_cache();
void        change_join_subject_len(int len);
std::string fetch_cache(ArticleNum art_num, HeaderLineType which_line, bool fill_cache);
std::string decode_header(std::string_view from);
void        dectrl(std::string &str);
#ifdef PENDING
void look_ahead();
#endif
void cache_until_key();
#ifdef PENDING
bool cache_subjects();
#endif
bool cache_range(ArticleNum first, ArticleNum last);

#endif
