/* rthread.c
*/
// This software is copyrighted as detailed in the LICENSE file.

#include <config/string_case_compare.h>

#include "config/common.h"
#include "trn/rthread.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/artstate.h"
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/datasrc.h"
#include "trn/hash.h"
#include "trn/head.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/rt-ov.h"
#include "trn/rt-page.h"
#include "trn/rt-process.h"
#include "trn/rt-select.h"
#include "trn/rt-util.h"
#include "trn/rt-wumpus.h"
#include "trn/score.h"
#include <trn/Subject.h>
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>

ArticleNum g_obj_count{};
int        g_subject_count{};
bool       g_output_chase_phrase{};
HashTable *g_msg_id_hash{};
bool       g_thread_always{}; // -a
bool       g_breadth_first{}; // -b

static int      cleanup_msg_id_hash(int keylen, HashDatum *data, int extra);
static Article *first_sib(Article *ta, int depth);
static Article *last_sib(Article *ta, int depth, Article *limit);
static int      subject_order_subject(const Subject **spp1, const Subject **spp2);
static int      subject_order_date(const Subject **spp1, const Subject **spp2);
static int      subject_order_count(const Subject **spp1, const Subject **spp2);
static int      subject_order_lines(const Subject **spp1, const Subject **spp2);
static int      subject_score_high(const Subject *sp);
static int      subject_order_score(const Subject **spp1, const Subject **spp2);
static int      thread_order_subject(const Subject **spp1, const Subject **spp2);
static int      thread_order_date(const Subject **spp1, const Subject **spp2);
static int      thread_order_count(const Subject **spp1, const Subject **spp2);
static int      thread_order_lines(const Subject **spp1, const Subject **spp2);
static int      thread_score_high(const Subject *tp);
static int      thread_order_score(const Subject **spp1, const Subject **spp2);
static int      article_order_date(const Article **art1, const Article **art2);
static int      article_order_subject(const Article **art1, const Article **art2);
static int      article_order_author(const Article **art1, const Article **art2);
static int      article_order_number(const Article **art1, const Article **art2);
static int      article_order_groups(const Article **art1, const Article **art2);
static int      article_order_lines(const Article **art1, const Article **art2);
static int      article_order_score(const Article **art1, const Article **art2);
static void     build_article_ptrs();

void thread_init()
{
}

// Generate the thread data we need for this group.  We must call
// thread_close() before calling this again.
//
void thread_open()
{
    bool    find_existing = !g_first_subject;
    Article*ap;

    if (!g_msg_id_hash)
    {
        g_msg_id_hash = hash_create(1999, msg_id_cmp); // TODO: pick a better size
    }
    if (g_threaded_group)
    {
        // Parse input and use g_msgid_hash for quick article lookups.
        // If cached but not threaded articles exist, set up to thread them.
        if (g_first_subject)
        {
            g_first_cached = g_first_art;
            g_last_cached = article_before(g_first_art);
            g_parsed_art = ArticleNum{};
        }
    }

    if (g_sel_mode == SM_ARTICLE)
    {
        set_selector(g_sel_mode, g_sel_art_sort);
    }
    else
    {
        set_selector(g_sel_thread_mode, g_sel_thread_sort);
    }

    if ((g_data_source->m_flags & DF_TRY_OVERVIEW) && !g_cached_all_in_range)
    {
        if (g_thread_always)
        {
            g_spin_todo = (g_last_art - g_abs_first).value_of() + 1;
            g_spin_estimate = (g_last_art - g_abs_first).value_of() + 1;
            (void) ov_data(g_abs_first, g_last_art, false);
            if (g_data_source->m_ov_opened && find_existing && g_data_source->m_over_dir == nullptr)
            {
                mark_missing_articles();
                rc_to_bits();
                find_existing = false;
            }
        }
        else
        {
            g_spin_todo = (g_last_art - g_first_art).value_of() + 1;
            g_spin_estimate = g_newsgroup_ptr->m_to_read;
            if (g_first_art > g_last_art)
            {
                // If no unread articles, see if ov. exists as fast as possible
                (void) ov_data(g_abs_first, g_abs_first, false);
                g_cached_all_in_range = false;
            }
            else
            {
                (void) ov_data(g_first_art, g_last_art, false);
            }
        }
    }
    if (find_existing)
    {
        find_existing_articles();
        mark_missing_articles();
        rc_to_bits();
    }

    for (ap = article_ptr(article_first(g_first_art));
         ap && ap->article_num() <= g_last_cached;
         ap = article_nextp(ap))
    {
        if ((ap->m_flags & (AF_EXISTS|AF_CACHED)) == AF_EXISTS)
        {
            (void) parse_header(ap->article_num());
        }
    }

    if (g_last_cached > g_last_art)
    {
        g_newsgroup_ptr->m_to_read += (ArticleUnread)(g_last_cached-g_last_art).value_of();
        // ensure getngsize() knows the new maximum
        g_newsgroup_ptr->m_ng_max = g_last_cached;
        g_last_art = g_last_cached;
    }
    g_article_list->high = g_last_art.value_of();

    for (ap = article_ptr(article_first(g_abs_first));
         ap && ap->article_num() <= g_last_art;
         ap = article_nextp(ap))
    {
        if (ap->m_flags & AF_CACHED)
        {
            ap->check_poster();
        }
    }

    std::time_t save_ov_opened = g_data_source->m_ov_opened;
    g_data_source->m_ov_opened = 0; // avoid trying to call ov_data twice for high arts
    thread_grow();          // thread any new articles not yet in the database
    g_data_source->m_ov_opened = save_ov_opened;
    g_added_articles = 0;
    g_sel_page_sp = nullptr;
    g_sel_page_app = nullptr;
}

// Update the group's thread info.
//
void thread_grow()
{
    g_added_articles += (g_last_art - g_last_cached).value_of();
    if (g_added_articles && g_thread_always)
    {
        cache_range(article_after(g_last_cached), g_last_art);
    }
    count_subjects(CS_NORM);
    if (g_art_ptr_list)
    {
        sort_articles();
    }
    else
    {
        sort_subjects();
    }
}

void thread_close()
{
    g_curr_artp = nullptr;
    g_artp = nullptr;
    init_tree();                        // free any tree lines

    update_thread_kill_file();
    if (g_msg_id_hash)
    {
        hash_walk(g_msg_id_hash, cleanup_msg_id_hash, 0);
    }
    g_sel_page_sp = nullptr;
    g_sel_page_app = nullptr;
    g_sel_last_ap = nullptr;
    g_sel_last_sp = nullptr;
    g_selected_only = false;
    g_sel_exclusive = false;
    ov_close();
}

static int cleanup_msg_id_hash(int keylen, HashDatum *data, int extra)
{
    Article* ap = (Article*)data->dat_ptr;
    int ret = -1;

    if (ap)
    {
        if (data->dat_len)
        {
            return 0;
        }
        if ((g_kf_state & KFS_GLOBAL_THREAD_FILE) && ap->m_auto_flags)
        {
            data->dat_ptr = ap->m_msg_id;
            data->dat_len = ap->m_auto_flags;
            ret = 0;
            ap->m_msg_id = nullptr;
        }
        if (ap->m_flags & AF_TMP_MEM)
        {
            ap->clear_article();
            std::free(ap);
        }
    }
    return ret;
}

void top_article()
{
    g_art = article_after(g_last_art);
    g_artp = nullptr;
    inc_article(g_selected_only, false);
    if (g_art > g_last_art && g_last_cached < g_last_art)
    {
        g_art = g_first_art;
    }
}

// Bump g_art/g_artp to the next article, wrapping from thread to thread.
// If sel_flag is true, only stops at selected articles.
// If rereading is false, only stops at unread articles.
//
void inc_article(bool sel_flag, bool rereading)
{
    Article* ap = g_artp;
    int subj_mask = (rereading? 0 : SF_VISIT);

    // Use the explicit article-order if it exists
    if (g_art_ptr_list)
    {
        Article** limit = g_art_ptr_list + g_art_ptr_list_size.value_of();
        if (!ap)
        {
            g_art_ptr = g_art_ptr_list - 1;
        }
        else if (!g_art_ptr || *g_art_ptr != ap)
        {
            for (g_art_ptr = g_art_ptr_list; g_art_ptr < limit; g_art_ptr++)
            {
                if (*g_art_ptr == ap)
                {
                    break;
                }
            }
        }
        do
        {
            if (++g_art_ptr >= limit)
            {
                break;
            }
            ap = *g_art_ptr;
        } while ((!rereading && !(ap->m_flags & AF_UNREAD))
              || (sel_flag && !(ap->m_flags & AF_SEL)));
        if (g_art_ptr < limit)
        {
            g_artp = *g_art_ptr;
            g_art = g_artp->article_num();
        }
        else
        {
            g_artp = nullptr;
            g_art = article_after(g_last_art);
            g_art_ptr = g_art_ptr_list;
        }
        return;
    }

    // Use subject- or thread-order when possible
    if (g_threaded_group || g_search_ahead)
    {
        Subject* sp;
        if (ap)
        {
            sp = ap->m_subj;
        }
        else
        {
            sp = next_subject((Subject *) nullptr, subj_mask);
        }
        if (!sp)
        {
            goto num_inc;
        }
        do
        {
            if (ap)
            {
                ap = ap->next_article();
            }
            else
            {
                ap = sp->first_art();
            }
            while (!ap)
            {
                sp = next_subject(sp, subj_mask);
                if (!sp)
                {
                    break;
                }
                ap = sp->first_art();
            }
        } while (ap && ((!rereading && !(ap->m_flags & AF_UNREAD))
                     || (sel_flag && !(ap->m_flags & AF_SEL))));
        g_artp = ap;
        if (g_artp != nullptr)
        {
            g_art = ap->article_num();
        }
        else
        {
            if (g_art <= g_last_cached)
            {
                g_art = article_after(g_last_cached);
            }
            else
            {
                ++g_art;
            }
            if (g_art <= g_last_art)
            {
                g_artp = article_ptr(g_art);
            }
            else
            {
                g_art = article_after(g_last_art);
            }
        }
        return;
    }

    // Otherwise, just increment through the art numbers
num_inc:
    if (!ap)
    {
        g_art = article_before(g_first_art);
    }
    while (true)
    {
        g_art = article_next(g_art);
        if (g_art > g_last_art)
        {
            g_art = article_after(g_last_art);
            ap = nullptr;
            break;
        }
        ap = article_ptr(g_art);
        if (!(ap->m_flags & AF_EXISTS))
        {
            ap->one_less();
        }
        else if ((rereading || (ap->m_flags & AF_UNREAD))
              && (!sel_flag || (ap->m_flags & AF_SEL)))
        {
            break;
        }
    }
    g_artp = ap;
}

// Bump g_art/g_artp to the previous article, wrapping from thread to thread.
// If sel_flag is true, only stops at selected articles.
// If rereading is false, only stops at unread articles.
//
void dec_article(bool sel_flag, bool rereading)
{
    Article* ap = g_artp;
    int subj_mask = (rereading? 0 : SF_VISIT);

    // Use the explicit article-order if it exists
    if (g_art_ptr_list)
    {
        Article** limit = g_art_ptr_list + g_art_ptr_list_size.value_of();
        if (!ap)
        {
            g_art_ptr = limit;
        }
        else if (!g_art_ptr || *g_art_ptr != ap)
        {
            for (g_art_ptr = g_art_ptr_list; g_art_ptr < limit; g_art_ptr++)
            {
                if (*g_art_ptr == ap)
                {
                    break;
                }
            }
        }
        do
        {
            if (g_art_ptr == g_art_ptr_list)
            {
                break;
            }
            ap = *--g_art_ptr;
        } while ((!rereading && !(ap->m_flags & AF_UNREAD))
              || (sel_flag && !(ap->m_flags & AF_SEL)));
        g_artp = *g_art_ptr;
        g_art = g_artp->article_num();
        return;
    }

    // Use subject- or thread-order when possible
    if (g_threaded_group || g_search_ahead)
    {
        Subject* sp;
        if (ap)
        {
            sp = ap->m_subj;
        }
        else
        {
            sp = prev_subject((Subject *) nullptr, subj_mask);
        }
        if (!sp)
        {
            goto num_dec;
        }
        do
        {
            if (ap)
            {
                ap = ap->prev_article();
            }
            else
            {
                ap = sp->last_art();
            }
            while (!ap)
            {
                sp = prev_subject(sp, subj_mask);
                if (!sp)
                {
                    break;
                }
                ap = sp->last_art();
            }
        } while (ap && ((!rereading && !(ap->m_flags & AF_UNREAD))
                     || (sel_flag && !(ap->m_flags & AF_SEL))));
        g_artp = ap;
        if (g_artp != nullptr)
        {
            g_art = ap->article_num();
        }
        else
        {
            g_art = article_before(g_abs_first);
        }
        return;
    }

    // Otherwise, just decrement through the art numbers
num_dec:
    while (true)
    {
        g_art = article_prev(g_art);
        if (g_art < g_abs_first)
        {
            g_art = article_before(g_abs_first);
            ap = nullptr;
            break;
        }
        ap = article_ptr(g_art);
        if (!(ap->m_flags & AF_EXISTS))
        {
            ap->one_less();
        }
        else if ((rereading || (ap->m_flags & AF_UNREAD))
              && (!sel_flag || (ap->m_flags & AF_SEL)))
        {
            break;
        }
    }
    g_artp = ap;
}

// Find the next g_art/g_artp with the same subject as this one.  Returns
// false if no such article exists.
//
bool next_article_with_subj()
{
    Article* ap = g_artp;

    if (!ap)
    {
        return false;
    }

    do
    {
        ap = ap->m_subj_next;
        if (!ap)
        {
            if (!g_art)
            {
                g_art = g_first_art;
            }
            return false;
        }
    } while (!all_bits(ap->m_flags, AF_EXISTS | AF_UNREAD)
          || (g_selected_only && !(ap->m_flags & AF_SEL)));
    g_artp = ap;
    g_art = ap->article_num();
    g_search_ahead = ArticleNum{-1};
    return true;
}

// Find the previous g_art/g_artp with the same subject as this one.  Returns
// false if no such article exists.
//
bool prev_article_with_subj()
{
    Article* ap = g_artp;

    if (!ap)
    {
        return false;
    }

    do
    {
        Article *ap2 = ap->m_subj->m_articles;
        if (ap2 == ap)
        {
            ap = nullptr;
        }
        else
        {
            while (ap2 && ap2->m_subj_next != ap)
            {
                ap2 = ap2->m_subj_next;
            }
            ap = ap2;
        }
        if (!ap)
        {
            if (!g_art)
            {
                g_art = g_last_art;
            }
            return false;
        }
    } while (!(ap->m_flags & AF_EXISTS)
          || (g_selected_only && !(ap->m_flags & AF_SEL)));
    g_artp = ap;
    g_art = ap->article_num();
    return true;
}

// TODO: why is sp tested against nullptr?
//
Subject *next_subject(Subject *sp, int subj_mask)
{
    if (!sp)
    {
        sp = g_first_subject;
    }
    else if (g_sel_mode == SM_THREAD)
    {
        Article* ap = sp->m_thread;
        do
        {
            sp = sp->m_next;
        } while (sp && sp->m_thread == ap);
    }
    else
    {
        sp = sp->m_next;
    }

    while (sp && (sp->m_flags & subj_mask) != subj_mask)
    {
        sp = sp->m_next;
    }
    return sp;
}

// TODO: why is sp tested against nullptr?
//
Subject *prev_subject(Subject *sp, int subj_mask)
{
    if (!sp)
    {
        sp = g_last_subject;
    }
    else if (g_sel_mode == SM_THREAD)
    {
        Article* ap = sp->m_thread;
        do
        {
            sp = sp->m_prev;
        } while (sp && sp->m_thread == ap);
    }
    else
    {
        sp = sp->m_prev;
    }

    while (sp && (sp->m_flags & subj_mask) != subj_mask)
    {
        sp = sp->m_prev;
    }
    return sp;
}

// Select the subthread attached to this article.
//
// TODO: why does this check ap for nullptr?
//
void select_sub_thread(Article *ap, AutoKillFlags auto_flags)
{
    Article*limit;
    int     desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int     old_count = g_selected_count;

    if (!ap)
    {
        return;
    }
    Subject *subj = ap->m_subj;
    for (limit = ap; limit; limit = limit->m_parent)
    {
        if (limit->m_sibling)
        {
            limit = limit->m_sibling;
            break;
        }
    }

    auto_flags &= AUTO_SEL_MASK;
    for (; ap != limit; ap = ap->bump_article())
    {
        if ((ap->m_flags & (AF_EXISTS | AF_UNREAD | g_sel_mask)) == desired_flags)
        {
            ap->m_flags |= static_cast<ArticleFlags>(g_sel_mask);
            g_selected_count++;
        }
        if (auto_flags)
        {
            ap->change_auto_flags(auto_flags);
        }
    }
    if (subj && g_selected_count > old_count)
    {
        if (!(subj->m_flags & g_sel_mask))
        {
            g_selected_subj_cnt++;
        }
        subj->m_flags = (subj->m_flags & ~SF_DEL) | static_cast<SubjectFlags>(g_sel_mask) | SF_VISIT;
        g_selected_only = true;
    }
}

// Deselect everything.
//
void deselect_all()
{
    for (Subject *sp = g_first_subject; sp; sp = sp->m_next)
    {
        sp->deselect_subject();
    }
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    g_sel_page_sp = nullptr;
    g_sel_page_app = nullptr;
    g_sel_last_ap = nullptr;
    g_sel_last_sp = nullptr;
    g_selected_only = false;
}

// Kill the subthread attached to this article.
//
// TODO: why does this check ap for nullptr?
//
void kill_sub_thread(Article *ap, AutoKillFlags auto_flags)
{
    Article* limit;
    const bool toreturn = (auto_flags & SET_TO_RETURN) != 0;

    if (!ap)
    {
        return;
    }
    for (limit = ap; limit; limit = limit->m_parent)
    {
        if (limit->m_sibling)
        {
            limit = limit->m_sibling;
            break;
        }
    }

    auto_flags &= AUTO_KILL_MASK;
    for (; ap != limit; ap = ap->bump_article())
    {
        if (toreturn)
        {
            ap->delay_unmark();
        }
        if (all_bits(ap->m_flags, AF_EXISTS | AF_UNREAD))
        {
            ap->set_read();
        }
        if (auto_flags)
        {
            ap->change_auto_flags(auto_flags);
        }
    }
}

// Unkill the subthread attached to this article.
//
// TODO: why does this check ap for nullptr?
//
void unkill_sub_thread(Article *ap)
{
    Article* limit;

    if (!ap)
    {
        return;
    }
    for (limit = ap; limit; limit = limit->m_parent)
    {
        if (limit->m_sibling)
        {
            limit = limit->m_sibling;
            break;
        }
    }

    Subject *sp = ap->m_subj;
    for (; ap != limit; ap = ap->bump_article())
    {
        if ((ap->m_flags & (AF_EXISTS|AF_UNREAD)) == AF_EXISTS)
        {
            ap->one_more();
        }
        if (g_selected_only && !(ap->m_flags & AF_SEL))
        {
            ap->m_flags |= AF_SEL;
            g_selected_count++;
        }
    }
    if (!(sp->m_flags & g_sel_mask))
    {
        g_selected_subj_cnt++;
    }
    sp->m_flags = (sp->m_flags & ~SF_DEL) | SF_SEL | SF_VISIT;
    g_selected_only = (g_selected_only || g_selected_count != 0);
}

// Clear the auto flags in the subthread attached to this article.
//
// TODO: why does this check ap for nullptr?
//
void clear_sub_thread(Article *ap)
{
    Article* limit;

    if (!ap)
    {
        return;
    }
    for (limit = ap; limit; limit = limit->m_parent)
    {
        if (limit->m_sibling)
        {
            limit = limit->m_sibling;
            break;
        }
    }

    for (; ap != limit; ap = ap->bump_article())
    {
        ap->clear_auto_flags();
    }
}

// Find the next thread (first if g_art > g_lastart).  If articles are selected,
// only choose from threads with selected articles.
//
void visit_next_thread()
{
    Article* ap = g_artp;

    Subject *sp = (ap ? ap->m_subj : nullptr);
    while ((sp = next_subject(sp, SF_VISIT)) != nullptr)
    {
        ap = sp->subj_article();
        if (ap != nullptr)
        {
            g_art = ap->article_num();
            g_artp = ap;
            return;
        }
        g_reread = false;
    }
    g_artp = nullptr;
    g_art = article_after(g_last_art);
    g_force_last = true;
}

// Find previous thread (or last if g_artp == nullptr).  If articles are selected,
// only choose from threads with selected articles.
//
void visit_prev_thread()
{
    Article* ap = g_artp;

    Subject *sp = (ap ? ap->m_subj : nullptr);
    while ((sp = prev_subject(sp, SF_VISIT)) != nullptr)
    {
        ap = sp->subj_article();
        if (ap != nullptr)
        {
            g_art = ap->article_num();
            g_artp = ap;
            return;
        }
        g_reread = false;
    }
    g_artp = nullptr;
    g_art = article_after(g_last_art);
    g_force_last = true;
}

// Find g_artp's parent or oldest ancestor.  Returns false if no such
// article.  Sets g_art and g_artp otherwise.
//
bool find_parent(bool keep_going)
{
    Article* ap = g_artp;

    if (!ap->m_parent)
    {
        return false;
    }

    do
    {
        ap = ap->m_parent;
    } while (keep_going && ap->m_parent);

    g_artp = ap;
    g_art = ap->article_num();
    return true;
}

// Find g_artp's first child or youngest descendant.  Returns false if no
// such article.  Sets g_art and g_artp otherwise.
//
bool find_leaf(bool keep_going)
{
    Article* ap = g_artp;

    if (!ap->m_child1)
    {
        return false;
    }

    do
    {
        ap = ap->m_child1;
    } while (keep_going && ap->m_child1);

    g_artp = ap;
    g_art = ap->article_num();
    return true;
}

// Find the next "sibling" of g_artp, including cousins that are the
// same distance down the thread as we are.  Returns false if no such
// article.  Sets g_art and g_artp otherwise.
//
bool find_next_sib()
{
    int      ascent = 0;
    Article *ta = g_artp;
    while (true)
    {
        while (ta->m_sibling)
        {
            ta = ta->m_sibling;
            Article *tb = first_sib(ta, ascent);
            if (tb != nullptr)
            {
                g_artp = tb;
                g_art = tb->article_num();
                return true;
            }
        }
        if (!(ta = ta->m_parent))
        {
            break;
        }
        ascent++;
    }
    return false;
}

// A recursive routine to find the first node at the proper depth.  This
// article is at depth 0.
//
static Article *first_sib(Article *ta, int depth)
{
    Article* tb;

    if (!depth)
    {
        return ta;
    }

    while (true)
    {
        if (ta->m_child1 && (tb = first_sib(ta->m_child1, depth-1)))
        {
            return tb;
        }

        if (!ta->m_sibling)
        {
            return nullptr;
        }

        ta = ta->m_sibling;
    }
}

// Find the previous "sibling" of g_artp, including cousins that are
// the same distance down the thread as we are.  Returns false if no
// such article.  Sets g_art and g_artp otherwise.
//
bool find_prev_sib()
{
    int      ascent = 0;
    Article *ta = g_artp;
    while (true)
    {
        Article *tb = ta;
        if (ta->m_parent)
        {
            ta = ta->m_parent->m_child1;
        }
        else
        {
            ta = ta->m_subj->m_thread;
        }
        tb = last_sib(ta, ascent, tb);
        if (tb != nullptr)
        {
            g_artp = tb;
            g_art = tb->article_num();
            return true;
        }
        if (!(ta = ta->m_parent))
        {
            break;
        }
        ascent++;
    }
    return false;
}

// A recursive routine to find the last node at the proper depth.  This
// article is at depth 0.
//
static Article *last_sib(Article *ta, int depth, Article *limit)
{
    Article* tb;

    if (ta == limit)
    {
        return nullptr;
    }

    if (ta->m_sibling)
    {
        Article *tc = ta->m_sibling;
        if (tc != limit && (tb = last_sib(tc,depth,limit)))
        {
            return tb;
        }
    }
    if (!depth)
    {
        return ta;
    }
    if (ta->m_child1)
    {
        return last_sib(ta->m_child1, depth - 1, limit);
    }
    return nullptr;
}

// Get each subject's article count; count total articles and selected
// articles (use g_sel_rereading to determine whether to count read or
// unread articles); deselect any subjects we find that are empty if
// CS_UNSELECT or CS_UNSEL_STORE is specified.  If g_mode is CS_RESELECT
// is specified, the selections from the last CS_UNSEL_STORE are
// reselected.
//
void count_subjects(CountSubjectMode cmode)
{
    Subject*sp;
    int     desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));

    g_obj_count = ArticleNum{};
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    if (g_last_cached >= g_last_art)
    {
        g_first_art = article_after(g_last_art);
    }

    switch (cmode)
    {
    case CS_RETAIN:
        break;

    case CS_UNSEL_STORE:
        for (sp = g_first_subject; sp; sp = sp->m_next)
        {
            if (sp->m_flags & SF_VISIT)
            {
                sp->m_flags = (sp->m_flags & ~SF_VISIT) | SF_OLD_VISIT;
            }
            else
            {
                sp->m_flags &= ~SF_OLD_VISIT;
            }
        }
        break;

    case CS_RESELECT:
        for (sp = g_first_subject; sp; sp = sp->m_next)
        {
            if (sp->m_flags & SF_OLD_VISIT)
            {
                sp->m_flags |= SF_VISIT;
            }
            else
            {
                sp->m_flags &= ~SF_VISIT;
            }
        }
        break;

    default:
        for (sp = g_first_subject; sp; sp = sp->m_next)
        {
            sp->m_flags &= ~SF_VISIT;
        }
    }

    for (sp = g_first_subject; sp; sp = sp->m_next)
    {
        std::time_t subjdate = 0;
        int    count = 0;
        int    sel_count = 0;
        for (Article *ap = sp->m_articles; ap; ap = ap->m_subj_next)
        {
            if ((ap->m_flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
            {
                count++;
                if (ap->m_flags & g_sel_mask)
                {
                    sel_count++;
                }
                if (!subjdate)
                {
                    subjdate = ap->m_date;
                }
                g_first_art = std::min(ap->article_num(), g_first_art);
            }
        }
        sp->m_misc = count;
        if (subjdate)
        {
            sp->m_date = subjdate;
        }
        else if (!sp->m_date && sp->m_articles)
        {
            sp->m_date = sp->m_articles->m_date;
        }
        g_obj_count += ArticleNum{count};
        if (cmode == CS_RESELECT)
        {
            if (sp->m_flags & SF_VISIT)
            {
                sp->m_flags = (sp->m_flags & ~(SF_SEL|SF_DEL)) | static_cast<SubjectFlags>(g_sel_mask);
                g_selected_count += sel_count;
                g_selected_subj_cnt++;
            }
            else
            {
                sp->m_flags &= ~static_cast<SubjectFlags>(g_sel_mask);
            }
        }
        else
        {
            if (sel_count //
                && (cmode >= CS_UNSEL_STORE || (sp->m_flags & g_sel_mask)))
            {
                sp->m_flags = (sp->m_flags & ~(SF_SEL|SF_DEL)) | static_cast<SubjectFlags>(g_sel_mask);
                g_selected_count += sel_count;
                g_selected_subj_cnt++;
            }
            else if (cmode >= CS_UNSELECT)
            {
                sp->m_flags &= ~static_cast<SubjectFlags>(g_sel_mask);
            }
            else if (sp->m_flags & g_sel_mask)
            {
                sp->m_flags &= ~SF_DEL;
                g_selected_subj_cnt++;
            }
            if (count && (!g_selected_only || (sp->m_flags & g_sel_mask)))
            {
                sp->m_flags |= SF_VISIT;
            }
        }
    }
    if (cmode != CS_RETAIN && cmode != CS_RESELECT //
        && !g_obj_count && !g_selected_only)
    {
        for (sp = g_first_subject; sp; sp = sp->m_next)
        {
            sp->m_flags |= SF_VISIT;
        }
    }
}

static int subject_order_subject(const Subject **spp1, const Subject**spp2)
{
    return string_case_compare((*spp1)->m_str+4, (*spp2)->m_str+4) * g_sel_direction;
}

static int subject_order_date(const Subject **spp1, const Subject**spp2)
{
    std::time_t eq = (*spp1)->m_date - (*spp2)->m_date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int subject_order_count(const Subject **spp1, const Subject**spp2)
{
    short eq = (*spp1)->m_misc - (*spp2)->m_misc;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : subject_order_date(spp1,spp2);
}

static int subject_order_lines(const Subject **spp1, const Subject**spp2)
{
    long l1 = (*spp1)->m_articles ? (*spp1)->m_articles->m_lines : 0;
    long l2 = (*spp2)->m_articles ? (*spp2)->m_articles->m_lines : 0;
    long eq = l1 - l2;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : subject_order_date(spp1,spp2);
}

// for now, highest eligible article score
static int subject_score_high(const Subject *sp)
{
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int hiscore = 0;
    int hiscore_found = 0;

    // find highest score of desired articles
    for (Article *ap = sp->m_articles; ap; ap = ap->m_subj_next)
    {
        if ((ap->m_flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
        {
            int sc = sc_score_art(ap->article_num(), false);
            if ((!hiscore_found) || (sc > hiscore))
            {
                hiscore_found = 1;
                hiscore = sc;
            }
        }
    }
    return hiscore;
}

// later make a subject-thread score routine
static int subject_order_score(const Subject** spp1, const Subject** spp2)
{
    int sc1 = subject_score_high(*spp1);
    int sc2 = subject_score_high(*spp2);

    if (sc1 != sc2)
    {
        return (sc2 - sc1) * g_sel_direction;
    }
    return subject_order_date(spp1, spp2);
}

static int thread_order_subject(const Subject **spp1, const Subject**spp2)
{
    Article* t1 = (*spp1)->m_thread;
    Article* t2 = (*spp2)->m_thread;
    if (t1 != t2 && t1 && t2)
    {
        return string_case_compare(t1->m_subj->m_str + 4, t2->m_subj->m_str + 4) * g_sel_direction;
    }
    return subject_order_date(spp1, spp2);
}

static int thread_order_date(const Subject **spp1, const Subject**spp2)
{
    Article* t1 = (*spp1)->m_thread;
    Article* t2 = (*spp2)->m_thread;
    if (t1 != t2 && t1 && t2)
    {
        Subject* sp1;
        Subject* sp2;
        long eq;
        if (!(sp1 = t1->m_subj)->m_misc)
        {
            for (sp1 = sp1->m_thread_link; sp1 != t1->m_subj; sp1 = sp1->m_thread_link)
            {
                if (sp1->m_misc)
                {
                    break;
                }
            }
        }
        if (!(sp2 = t2->m_subj)->m_misc)
        {
            for (sp2 = sp2->m_thread_link; sp2 != t2->m_subj; sp2 = sp2->m_thread_link)
            {
                if (sp2->m_misc)
                {
                    break;
                }
            }
        }
        if (!(eq = sp1->m_date - sp2->m_date))
        {
            return string_case_compare(sp1->m_str + 4, sp2->m_str + 4);
        }
        return eq > 0? g_sel_direction : -g_sel_direction;
    }
    return subject_order_date(spp1, spp2);
}

static int thread_order_count(const Subject **spp1, const Subject**spp2)
{
    int size1 = (*spp1)->m_misc;
    int size2 = (*spp2)->m_misc;
    if ((*spp1)->m_thread != (*spp2)->m_thread)
    {
        Subject* sp;
        for (sp = (*spp1)->m_thread_link; sp != *spp1; sp = sp->m_thread_link)
        {
            size1 += sp->m_misc;
        }
        for (sp = (*spp2)->m_thread_link; sp != *spp2; sp = sp->m_thread_link)
        {
            size2 += sp->m_misc;
        }
    }
    if (size1 != size2)
    {
        return (size1 - size2) * g_sel_direction;
    }
    return thread_order_date(spp1, spp2);
}

static int thread_order_lines(const Subject **spp1, const Subject**spp2)
{
    Article* t1 = (*spp1)->m_thread;
    Article* t2 = (*spp2)->m_thread;
    if (t1 != t2 && t1 && t2)
    {
        Subject*sp1;
        Subject*sp2;
        if (!(sp1 = t1->m_subj)->m_misc)
        {
            for (sp1 = sp1->m_thread_link; sp1 != t1->m_subj; sp1 = sp1->m_thread_link)
            {
                if (sp1->m_misc)
                {
                    break;
                }
            }
        }
        if (!(sp2 = t2->m_subj)->m_misc)
        {
            for (sp2 = sp2->m_thread_link; sp2 != t2->m_subj; sp2 = sp2->m_thread_link)
            {
                if (sp2->m_misc)
                {
                    break;
                }
            }
        }
        long l1 = sp1->m_articles ? sp1->m_articles->m_lines : 0;
        long l2 = sp2->m_articles ? sp2->m_articles->m_lines : 0;
        long eq = l1 - l2;
        if (eq != 0)
        {
            return eq > 0 ? g_sel_direction : -g_sel_direction;
        }
        eq = sp1->m_date - sp2->m_date;
        return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
    }
    return subject_order_date(spp1, spp2);
}

static int thread_score_high(const Subject *tp)
{
    int hiscore = 0;
    int hiscore_found = 0;

    for (const Subject *sp = tp->m_thread_link;; sp = sp->m_thread_link)
    {
        int sc = subject_score_high(sp);
        if ((!hiscore_found) || (sc > hiscore))
        {
            hiscore_found = 1;
            hiscore = sc;
        }
        // break *after* doing the last item
        if (tp == sp)
        {
            break;
        }
    }
    return hiscore;
}

static int thread_order_score(const Subject** spp1, const Subject** spp2)
{
    int sc1 = 0;
    int sc2 = 0;

    if ((*spp1)->m_thread != (*spp2)->m_thread)
    {
        sc1 = thread_score_high(*spp1);
        sc2 = thread_score_high(*spp2);
    }
    if (sc1 != sc2)
    {
        return (sc2 - sc1) * g_sel_direction;
    }
    return thread_order_date(spp1, spp2);
}

// Sort the subjects according to the chosen order.
//
void sort_subjects()
{
    Subject* sp;
    Subject**lp;
    int (*   sort_procedure)(const Subject **spp1, const Subject**spp2);

    // If we don't have at least two subjects, we're done!
    if (!g_first_subject || !g_first_subject->m_next)
    {
        return;
    }

    switch (g_sel_sort)
    {
    case SS_DATE:
    default:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          thread_order_date : subject_order_date);
        break;

    case SS_STRING:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          thread_order_subject : subject_order_subject);
        break;

    case SS_COUNT:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          thread_order_count : subject_order_count);
        break;

    case SS_LINES:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          thread_order_lines : subject_order_lines);
        break;

    // if SCORE is undefined, use the default above
    case SS_SCORE:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          thread_order_score : subject_order_score);
        break;
    }

    Subject **subj_list = (Subject**)safe_malloc(g_subject_count * sizeof(Subject*));
    for (lp = subj_list, sp = g_first_subject; sp; sp = sp->m_next)
    {
        *lp++ = sp;
    }
    TRN_ASSERT(lp - subj_list == g_subject_count);

    std::qsort(subj_list, g_subject_count, sizeof (Subject*), ((int(*)(const void *, const void *))sort_procedure));

    g_first_subject = subj_list[0];
    sp = subj_list[0];
    sp->m_prev = nullptr;
    lp = subj_list;
    for (int i = g_subject_count; --i; lp++)
    {
        lp[0]->m_next = lp[1];
        lp[1]->m_prev = lp[0];
        if (g_sel_mode == SM_THREAD)
        {
            if (lp[0]->m_thread && lp[0]->m_thread == lp[1]->m_thread)
            {
                lp[0]->m_thread_link = lp[1];
            }
            else
            {
                lp[0]->m_thread_link = sp;
                sp = lp[1];
            }
        }
    }
    g_last_subject = lp[0];
    g_last_subject->m_next = nullptr;
    if (g_sel_mode == SM_THREAD)
    {
        g_last_subject->m_thread_link = sp;
    }
    std::free((char*)subj_list);
}

static int article_order_date(const Article **art1, const Article **art2)
{
    long eq = (*art1)->m_date - (*art2)->m_date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int article_order_subject(const Article **art1, const Article **art2)
{
    if ((*art1)->m_subj == (*art2)->m_subj)
    {
        return article_order_date(art1, art2);
    }
    return string_case_compare((*art1)->m_subj->m_str + 4, (*art2)->m_subj->m_str + 4)
        * g_sel_direction;
}

static int article_order_author(const Article **art1, const Article **art2)
{
    int eq = string_case_compare((*art1)->m_from, (*art2)->m_from);
    return eq? eq * g_sel_direction : article_order_date(art1, art2);
}

static int article_order_number(const Article **art1, const Article **art2)
{
    const long eq{((*art1)->article_num() - (*art2)->article_num()).value_of()};
    return eq > 0 ? g_sel_direction : -g_sel_direction;
}

static int article_order_groups(const Article **art1, const Article **art2)
{
    long eq;
#ifdef DEBUG
    TRN_ASSERT((*art1)->m_subj != nullptr);
    TRN_ASSERT((*art2)->m_subj != nullptr);
#endif
    if ((*art1)->m_subj == (*art2)->m_subj)
    {
        return article_order_date(art1, art2);
    }
    eq = (*art1)->m_subj->m_date - (*art2)->m_subj->m_date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int article_order_lines(const Article **art1, const Article **art2)
{
    long eq = (*art1)->m_lines - (*art2)->m_lines;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : article_order_date(art1,art2);
}

static int article_order_score(const Article **art1, const Article **art2)
{
    int eq = sc_score_art((*art2)->article_num(),false)
           - sc_score_art((*art1)->article_num(),false);
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

// Sort the articles according to the chosen order.
//
void sort_articles()
{
    int (*sort_procedure)(const Article **art1, const Article **art2);

    build_article_ptrs();

    // If we don't have at least two articles, we're done!
    if (g_art_ptr_list_size.value_of() < 2)
    {
        return;
    }

    switch (g_sel_sort)
    {
    case SS_DATE:
    default:
        sort_procedure = article_order_date;
        break;

    case SS_STRING:
        sort_procedure = article_order_subject;
        break;

    case SS_AUTHOR:
        sort_procedure = article_order_author;
        break;

    case SS_NATURAL:
        sort_procedure = article_order_number;
        break;

    case SS_GROUPS:
        sort_procedure = article_order_groups;
        break;

    case SS_LINES:
        sort_procedure = article_order_lines;
        break;

    case SS_SCORE:
        sort_procedure = article_order_score;
        break;
    }
    g_sel_page_app = nullptr;
    std::qsort(g_art_ptr_list, g_art_ptr_list_size.value_of(), sizeof (Article*), ((int(*)(const void *, const void *))sort_procedure));
}

static void build_article_ptrs()
{
    ArticleNum count = g_obj_count;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));

    if (!g_art_ptr_list || g_art_ptr_list_size != count)
    {
        g_art_ptr_list =
            (Article **) safe_realloc((char *) g_art_ptr_list, (MemorySize) count.value_of() * sizeof(Article *));
        g_art_ptr_list_size = count;
    }
    Article **app = g_art_ptr_list;
    for (ArticleNum an = article_first(g_abs_first); count; an = article_next(an))
    {
        Article *ap = article_ptr(an);
        if ((ap->m_flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
        {
            *app++ = ap;
            --count;
        }
    }
}
