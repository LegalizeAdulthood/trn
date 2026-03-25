/* trn/Article.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_ARTICLE_H
#define TRN_ARTICLE_H

#include <trn/enum-flags.h>
#include <trn/head.h>
#include <trn/kfile.h>

#include <cstdint>
#include <ctime>

struct Subject;

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

#endif
