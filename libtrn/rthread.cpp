/* rthread.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

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
#include "trn/terminal.h"
#include "trn/trn.h"
#include "trn/util.h"
#include "util/util2.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>

ArticleNum    g_obj_count{};
int        g_subject_count{};
bool       g_output_chase_phrase{};
HashTable *g_msgid_hash{};
bool       g_thread_always{}; /* -a */
bool       g_breadth_first{}; /* -b */

static int cleanup_msgid_hash(int keylen, HashDatum *data, int extra);
static Article *first_sib(Article *ta, int depth);
static Article *last_sib(Article *ta, int depth, Article *limit);
static int subjorder_subject(const Subject **spp1, const Subject **spp2);
static int subjorder_date(const Subject **spp1, const Subject **spp2);
static int subjorder_count(const Subject **spp1, const Subject **spp2);
static int subjorder_lines(const Subject **spp1, const Subject **spp2);
static int subject_score_high(const Subject *sp);
static int subjorder_score(const Subject **spp1, const Subject **spp2);
static int threadorder_subject(const Subject **spp1, const Subject **spp2);
static int threadorder_date(const Subject **spp1, const Subject **spp2);
static int threadorder_count(const Subject **spp1, const Subject **spp2);
static int threadorder_lines(const Subject **spp1, const Subject **spp2);
static int thread_score_high(const Subject *tp);
static int threadorder_score(const Subject **spp1, const Subject **spp2);
static int artorder_date(const Article **art1, const Article **art2);
static int artorder_subject(const Article **art1, const Article **art2);
static int artorder_author(const Article **art1, const Article **art2);
static int artorder_number(const Article **art1, const Article **art2);
static int artorder_groups(const Article **art1, const Article **art2);
static int artorder_lines(const Article **art1, const Article **art2);
static int artorder_score(const Article **art1, const Article **art2);
static void build_artptrs();

void thread_init()
{
}

/* Generate the thread data we need for this group.  We must call
** thread_close() before calling this again.
*/
void thread_open()
{
    bool    find_existing = !g_first_subject;
    Article*ap;

    if (!g_msgid_hash)
    {
        g_msgid_hash = hashcreate(1999, msgid_cmp); /*TODO: pick a better size */
    }
    if (g_threaded_group)
    {
        /* Parse input and use g_msgid_hash for quick article lookups. */
        /* If cached but not threaded articles exist, set up to thread them. */
        if (g_first_subject)
        {
            g_first_cached = g_firstart;
            g_last_cached = g_firstart - 1;
            g_parsed_art = 0;
        }
    }

    if (g_sel_mode == SM_ARTICLE)
    {
        set_selector(g_sel_mode, g_sel_artsort);
    }
    else
    {
        set_selector(g_sel_threadmode, g_sel_threadsort);
    }

    if ((g_data_source->flags & DF_TRY_OVERVIEW) && !g_cached_all_in_range)
    {
        if (g_thread_always)
        {
            g_spin_todo = g_lastart - g_absfirst + 1;
            g_spin_estimate = g_lastart - g_absfirst + 1;
            (void) ov_data(g_absfirst, g_lastart, false);
            if (g_data_source->ov_opened && find_existing && g_data_source->over_dir == nullptr)
            {
                mark_missing_articles();
                rc_to_bits();
                find_existing = false;
            }
        }
        else
        {
            g_spin_todo = g_lastart - g_firstart + 1;
            g_spin_estimate = g_ngptr->toread;
            if (g_firstart > g_lastart)
            {
                /* If no unread articles, see if ov. exists as fast as possible */
                (void) ov_data(g_absfirst, g_absfirst, false);
                g_cached_all_in_range = false;
            }
            else
            {
                (void) ov_data(g_firstart, g_lastart, false);
            }
        }
    }
    if (find_existing)
    {
        find_existing_articles();
        mark_missing_articles();
        rc_to_bits();
    }

    for (ap = article_ptr(article_first(g_firstart));
         ap && article_num(ap) <= g_last_cached;
         ap = article_nextp(ap))
    {
        if ((ap->flags & (AF_EXISTS|AF_CACHED)) == AF_EXISTS)
        {
            (void) parseheader(article_num(ap));
        }
    }

    if (g_last_cached > g_lastart)
    {
        g_ngptr->toread += (ArticleUnread)(g_last_cached-g_lastart);
        /* ensure getngsize() knows the new maximum */
        g_ngptr->ngmax = g_last_cached;
        g_lastart = g_last_cached;
    }
    g_article_list->high = g_lastart;

    for (ap = article_ptr(article_first(g_absfirst));
         ap && article_num(ap) <= g_lastart;
         ap = article_nextp(ap))
    {
        if (ap->flags & AF_CACHED)
        {
            check_poster(ap);
        }
    }

    std::time_t save_ov_opened = g_data_source->ov_opened;
    g_data_source->ov_opened = 0; /* avoid trying to call ov_data twice for high arts */
    thread_grow();          /* thread any new articles not yet in the database */
    g_data_source->ov_opened = save_ov_opened;
    g_added_articles = 0;
    g_sel_page_sp = nullptr;
    g_sel_page_app = nullptr;
}

/* Update the group's thread info.
*/
void thread_grow()
{
    g_added_articles += g_lastart - g_last_cached;
    if (g_added_articles && g_thread_always)
    {
        cache_range(g_last_cached + 1, g_lastart);
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
    init_tree();                        /* free any tree lines */

    update_thread_kfile();
    if (g_msgid_hash)
    {
        hashwalk(g_msgid_hash, cleanup_msgid_hash, 0);
    }
    g_sel_page_sp = nullptr;
    g_sel_page_app = nullptr;
    g_sel_last_ap = nullptr;
    g_sel_last_sp = nullptr;
    g_selected_only = false;
    g_sel_exclusive = false;
    ov_close();
}

static int cleanup_msgid_hash(int keylen, HashDatum *data, int extra)
{
    Article* ap = (Article*)data->dat_ptr;
    int ret = -1;

    if (ap)
    {
        if (data->dat_len)
        {
            return 0;
        }
        if ((g_kf_state & KFS_GLOBAL_THREADFILE) && ap->auto_flags)
        {
            data->dat_ptr = ap->msg_id;
            data->dat_len = ap->auto_flags;
            ret = 0;
            ap->msg_id = nullptr;
        }
        if (ap->flags & AF_TMP_MEM)
        {
            clear_article(ap);
            std::free(ap);
        }
    }
    return ret;
}

void top_article()
{
    g_art = g_lastart+1;
    g_artp = nullptr;
    inc_art(g_selected_only, false);
    if (g_art > g_lastart && g_last_cached < g_lastart)
    {
        g_art = g_firstart;
    }
}

Article *first_art(Subject *sp)
{
    Article* ap = (g_threaded_group? sp->thread : sp->articles);
    if (ap && !(ap->flags & AF_EXISTS))
    {
        one_less(ap);
        ap = next_art(ap);
    }
    return ap;
}

Article *last_art(Subject *sp)
{
    Article* ap;

    if (!g_threaded_group)
    {
        ap = sp->articles;
        while (ap->subj_next)
        {
            ap = ap->subj_next;
        }
        return ap;
    }

    ap = sp->thread;
    if (ap)
    {
        while (true)
        {
            if (ap->sibling)
            {
                ap = ap->sibling;
            }
            else if (ap->child1)
            {
                ap = ap->child1;
            }
            else
            {
                break;
            }
        }
        if (!(ap->flags & AF_EXISTS))
        {
            one_less(ap);
            ap = prev_art(ap);
        }
    }
    return ap;
}

/* Bump g_art/g_artp to the next article, wrapping from thread to thread.
** If sel_flag is true, only stops at selected articles.
** If rereading is false, only stops at unread articles.
*/
void inc_art(bool sel_flag, bool rereading)
{
    Article* ap = g_artp;
    int subj_mask = (rereading? 0 : SF_VISIT);

    /* Use the explicit article-order if it exists */
    if (g_art_ptr_list)
    {
        Article** limit = g_art_ptr_list + g_art_ptr_list_size;
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
        } while ((!rereading && !(ap->flags & AF_UNREAD))
              || (sel_flag && !(ap->flags & AF_SEL)));
        if (g_art_ptr < limit)
        {
            g_artp = *g_art_ptr;
            g_art = article_num(g_artp);
        }
        else
        {
            g_artp = nullptr;
            g_art = g_lastart+1;
            g_art_ptr = g_art_ptr_list;
        }
        return;
    }

    /* Use subject- or thread-order when possible */
    if (g_threaded_group || g_search_ahead)
    {
        Subject* sp;
        if (ap)
        {
            sp = ap->subj;
        }
        else
        {
            sp = next_subj((Subject *) nullptr, subj_mask);
        }
        if (!sp)
        {
            goto num_inc;
        }
        do
        {
            if (ap)
            {
                ap = next_art(ap);
            }
            else
            {
                ap = first_art(sp);
            }
            while (!ap)
            {
                sp = next_subj(sp, subj_mask);
                if (!sp)
                {
                    break;
                }
                ap = first_art(sp);
            }
        } while (ap && ((!rereading && !(ap->flags & AF_UNREAD))
                     || (sel_flag && !(ap->flags & AF_SEL))));
        g_artp = ap;
        if (g_artp != nullptr)
        {
            g_art = article_num(ap);
        }
        else
        {
            if (g_art <= g_last_cached)
            {
                g_art = g_last_cached + 1;
            }
            else
            {
                g_art++;
            }
            if (g_art <= g_lastart)
            {
                g_artp = article_ptr(g_art);
            }
            else
            {
                g_art = g_lastart + 1;
            }
        }
        return;
    }

    /* Otherwise, just increment through the art numbers */
  num_inc:
    if (!ap)
    {
        g_art = g_firstart-1;
    }
    while (true)
    {
        g_art = article_next(g_art);
        if (g_art > g_lastart)
        {
            g_art = g_lastart+1;
            ap = nullptr;
            break;
        }
        ap = article_ptr(g_art);
        if (!(ap->flags & AF_EXISTS))
        {
            one_less(ap);
        }
        else if ((rereading || (ap->flags & AF_UNREAD))
              && (!sel_flag || (ap->flags & AF_SEL)))
        {
            break;
        }
    }
    g_artp = ap;
}

/* Bump g_art/g_artp to the previous article, wrapping from thread to thread.
** If sel_flag is true, only stops at selected articles.
** If rereading is false, only stops at unread articles.
*/
void dec_art(bool sel_flag, bool rereading)
{
    Article* ap = g_artp;
    int subj_mask = (rereading? 0 : SF_VISIT);

    /* Use the explicit article-order if it exists */
    if (g_art_ptr_list)
    {
        Article** limit = g_art_ptr_list + g_art_ptr_list_size;
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
        } while ((!rereading && !(ap->flags & AF_UNREAD))
              || (sel_flag && !(ap->flags & AF_SEL)));
        g_artp = *g_art_ptr;
        g_art = article_num(g_artp);
        return;
    }

    /* Use subject- or thread-order when possible */
    if (g_threaded_group || g_search_ahead)
    {
        Subject* sp;
        if (ap)
        {
            sp = ap->subj;
        }
        else
        {
            sp = prev_subj((Subject *) nullptr, subj_mask);
        }
        if (!sp)
        {
            goto num_dec;
        }
        do
        {
            if (ap)
            {
                ap = prev_art(ap);
            }
            else
            {
                ap = last_art(sp);
            }
            while (!ap)
            {
                sp = prev_subj(sp, subj_mask);
                if (!sp)
                {
                    break;
                }
                ap = last_art(sp);
            }
        } while (ap && ((!rereading && !(ap->flags & AF_UNREAD))
                     || (sel_flag && !(ap->flags & AF_SEL))));
        g_artp = ap;
        if (g_artp != nullptr)
        {
            g_art = article_num(ap);
        }
        else
        {
            g_art = g_absfirst - 1;
        }
        return;
    }

    /* Otherwise, just decrement through the art numbers */
  num_dec:
    while (true)
    {
        g_art = article_prev(g_art);
        if (g_art < g_absfirst)
        {
            g_art = g_absfirst-1;
            ap = nullptr;
            break;
        }
        ap = article_ptr(g_art);
        if (!(ap->flags & AF_EXISTS))
        {
            one_less(ap);
        }
        else if ((rereading || (ap->flags & AF_UNREAD))
              && (!sel_flag || (ap->flags & AF_SEL)))
        {
            break;
        }
    }
    g_artp = ap;
}

/* Bump the param to the next article in depth-first order.
*/
Article *bump_art(Article *ap)
{
    if (ap->child1)
    {
        return ap->child1;
    }
    while (!ap->sibling)
    {
        if (!(ap = ap->parent))
        {
            return nullptr;
        }
    }
    return ap->sibling;
}

/* Bump the param to the next REAL article.  Uses subject order in a
** non-threaded group; honors the g_breadth_first flag in a threaded one.
*/
Article *next_art(Article *ap)
{
try_again:
    if (!g_threaded_group)
    {
        ap = ap->subj_next;
        goto done;
    }
    if (g_breadth_first)
    {
        if (ap->sibling)
        {
            ap = ap->sibling;
            goto done;
        }
        if (ap->parent)
        {
            ap = ap->parent->child1;
        }
        else
        {
            ap = ap->subj->thread;
        }
    }
    do
    {
        if (ap->child1)
        {
            ap = ap->child1;
            goto done;
        }
        while (!ap->sibling)
        {
            if (!(ap = ap->parent))
            {
                return nullptr;
            }
        }
        ap = ap->sibling;
    } while (g_breadth_first);
done:
    if (ap && !(ap->flags & AF_EXISTS))
    {
        one_less(ap);
        goto try_again;
    }
    return ap;
}

/* Bump the param to the previous REAL article.  Uses subject order in a
** non-threaded group.
*/
Article *prev_art(Article *ap)
{
try_again:
    Article *initial_ap = ap;
    if (!g_threaded_group)
    {
        ap = ap->subj->articles;
        if (ap == initial_ap)
        {
            ap = nullptr;
        }
        else
        {
            while (ap->subj_next != initial_ap)
            {
                ap = ap->subj_next;
            }
        }
        goto done;
    }
    ap = (ap->parent ? ap->parent->child1 : ap->subj->thread);
    if (ap == initial_ap)
    {
        ap = ap->parent;
        goto done;
    }
    while (ap->sibling != initial_ap)
    {
        ap = ap->sibling;
    }
    while (ap->child1)
    {
        ap = ap->child1;
        while (ap->sibling)
        {
            ap = ap->sibling;
        }
    }
done:
    if (ap && !(ap->flags & AF_EXISTS))
    {
        one_less(ap);
        goto try_again;
    }
    return ap;
}

/* Find the next g_art/g_artp with the same subject as this one.  Returns
** false if no such article exists.
*/
bool next_art_with_subj()
{
    Article* ap = g_artp;

    if (!ap)
    {
        return false;
    }

    do
    {
        ap = ap->subj_next;
        if (!ap)
        {
            if (!g_art)
            {
                g_art = g_firstart;
            }
            return false;
        }
    } while (!all_bits(ap->flags, AF_EXISTS | AF_UNREAD)
          || (g_selected_only && !(ap->flags & AF_SEL)));
    g_artp = ap;
    g_art = article_num(ap);
    g_search_ahead = -1;
    return true;
}

/* Find the previous g_art/g_artp with the same subject as this one.  Returns
** false if no such article exists.
*/
bool prev_art_with_subj()
{
    Article* ap = g_artp;

    if (!ap)
    {
        return false;
    }

    do
    {
        Article *ap2 = ap->subj->articles;
        if (ap2 == ap)
        {
            ap = nullptr;
        }
        else
        {
            while (ap2 && ap2->subj_next != ap)
            {
                ap2 = ap2->subj_next;
            }
            ap = ap2;
        }
        if (!ap)
        {
            if (!g_art)
            {
                g_art = g_lastart;
            }
            return false;
        }
    } while (!(ap->flags & AF_EXISTS)
          || (g_selected_only && !(ap->flags & AF_SEL)));
    g_artp = ap;
    g_art = article_num(ap);
    return true;
}

Subject *next_subj(Subject *sp, int subj_mask)
{
    if (!sp)
    {
        sp = g_first_subject;
    }
    else if (g_sel_mode == SM_THREAD)
    {
        Article* ap = sp->thread;
        do
        {
            sp = sp->next;
        } while (sp && sp->thread == ap);
    }
    else
    {
        sp = sp->next;
    }

    while (sp && (sp->flags & subj_mask) != subj_mask)
    {
        sp = sp->next;
    }
    return sp;
}

Subject *prev_subj(Subject *sp, int subj_mask)
{
    if (!sp)
    {
        sp = g_last_subject;
    }
    else if (g_sel_mode == SM_THREAD)
    {
        Article* ap = sp->thread;
        do
        {
            sp = sp->prev;
        } while (sp && sp->thread == ap);
    }
    else
    {
        sp = sp->prev;
    }

    while (sp && (sp->flags & subj_mask) != subj_mask)
    {
        sp = sp->prev;
    }
    return sp;
}

/* Select a single article.
*/
void select_article(Article *ap, AutoKillFlags auto_flags)
{
    const ArticleFlags desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    bool echo = (auto_flags & ALSO_ECHO) != 0;
    auto_flags &= AUTO_SEL_MASK;
    if ((ap->flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
    {
        if (!(ap->flags & static_cast<ArticleFlags>(g_sel_mask)))
        {
            g_selected_count++;
            if (g_verbose && echo && g_general_mode != GM_SELECTOR)
            {
                std::fputs("\tSelected", stdout);
            }
        }
        ap->flags = (ap->flags & ~AF_DEL) | static_cast<ArticleFlags>(g_sel_mask);
    }
    if (auto_flags)
    {
        change_auto_flags(ap, auto_flags);
    }
    if (ap->subj)
    {
        if (!(ap->subj->flags & g_sel_mask))
        {
            g_selected_subj_cnt++;
        }
        ap->subj->flags = (ap->subj->flags&~SF_DEL) | static_cast<SubjectFlags>(g_sel_mask) | SF_VISIT;
    }
    g_selected_only = (g_selected_only || g_selected_count != 0);
}

/* Select this article's subject.
*/
void select_arts_subject(Article *ap, AutoKillFlags auto_flags)
{
    if (ap->subj && ap->subj->articles)
    {
        select_subject(ap->subj, auto_flags);
    }
    else
    {
        select_article(ap, auto_flags);
    }
}

/* Select all the articles in a subject.
*/
void select_subject(Subject *subj, AutoKillFlags auto_flags)
{
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int old_count = g_selected_count;

    auto_flags &= AUTO_SEL_MASK;
    for (Article *ap = subj->articles; ap; ap = ap->subj_next)
    {
        if ((ap->flags & (AF_EXISTS | AF_UNREAD | g_sel_mask)) == desired_flags)
        {
            ap->flags |= static_cast<ArticleFlags>(g_sel_mask);
            g_selected_count++;
        }
        if (auto_flags)
        {
            change_auto_flags(ap, auto_flags);
        }
    }
    if (g_selected_count > old_count)
    {
        if (!(subj->flags & g_sel_mask))
        {
            g_selected_subj_cnt++;
        }
        subj->flags = (subj->flags & ~SF_DEL)
                    | static_cast<SubjectFlags>(g_sel_mask) | SF_VISIT | SF_WAS_SELECTED;
        g_selected_only = true;
    }
    else
    {
        subj->flags |= SF_WAS_SELECTED;
    }
}

/* Select this article's thread.
*/
void select_arts_thread(Article *ap, AutoKillFlags auto_flags)
{
    if (ap->subj && ap->subj->thread)
    {
        select_thread(ap->subj->thread, auto_flags);
    }
    else
    {
        select_arts_subject(ap, auto_flags);
    }
}

/* Select all the articles in a thread.
*/
void select_thread(Article *thread, AutoKillFlags auto_flags)
{
    Subject *sp = thread->subj;
    do
    {
        select_subject(sp, auto_flags);
        sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Select the subthread attached to this article.
*/
void select_subthread(Article *ap, AutoKillFlags auto_flags)
{
    Article*limit;
    int     desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int     old_count = g_selected_count;

    if (!ap)
    {
        return;
    }
    Subject *subj = ap->subj;
    for (limit = ap; limit; limit = limit->parent)
    {
        if (limit->sibling)
        {
            limit = limit->sibling;
            break;
        }
    }

    auto_flags &= AUTO_SEL_MASK;
    for (; ap != limit; ap = bump_art(ap))
    {
        if ((ap->flags & (AF_EXISTS | AF_UNREAD | g_sel_mask)) == desired_flags)
        {
            ap->flags |= static_cast<ArticleFlags>(g_sel_mask);
            g_selected_count++;
        }
        if (auto_flags)
        {
            change_auto_flags(ap, auto_flags);
        }
    }
    if (subj && g_selected_count > old_count)
    {
        if (!(subj->flags & g_sel_mask))
        {
            g_selected_subj_cnt++;
        }
        subj->flags = (subj->flags & ~SF_DEL) | static_cast<SubjectFlags>(g_sel_mask) | SF_VISIT;
        g_selected_only = true;
    }
}

/* Deselect a single article.
*/
void deselect_article(Article *ap, AutoKillFlags auto_flags)
{
    const bool echo = (auto_flags & ALSO_ECHO) != 0;
    auto_flags &= AUTO_SEL_MASK;
    if (ap->flags & static_cast<ArticleFlags>(g_sel_mask))
    {
        ap->flags &= ~static_cast<ArticleFlags>(g_sel_mask);
        if (!g_selected_count--)
        {
            g_selected_count = 0;
        }
        if (g_verbose && echo && g_general_mode != GM_SELECTOR)
        {
            std::fputs("\tDeselected", stdout);
        }
    }
    if (g_sel_rereading && g_sel_mode == SM_ARTICLE)
    {
        ap->flags |= AF_DEL;
    }
}

/* Deselect this article's subject.
*/
void deselect_arts_subject(Article *ap)
{
    if (ap->subj && ap->subj->articles)
    {
        deselect_subject(ap->subj);
    }
    else
    {
        deselect_article(ap, AUTO_KILL_NONE);
    }
}

/* Deselect all the articles in a subject.
*/
void deselect_subject(Subject *subj)
{
    for (Article *ap = subj->articles; ap; ap = ap->subj_next)
    {
        if (ap->flags & g_sel_mask)
        {
            ap->flags &= ~static_cast<ArticleFlags>(g_sel_mask);
            if (!g_selected_count--)
            {
                g_selected_count = 0;
            }
        }
    }
    if (subj->flags & g_sel_mask)
    {
        subj->flags &= ~static_cast<SubjectFlags>(g_sel_mask);
        g_selected_subj_cnt--;
    }
    subj->flags &= ~(SF_VISIT | SF_WAS_SELECTED);
    if (g_sel_rereading)
    {
        subj->flags |= SF_DEL;
    }
    else
    {
        subj->flags &= ~SF_DEL;
    }
}

/* Deselect this article's thread.
*/
void deselect_arts_thread(Article *ap)
{
    if (ap->subj && ap->subj->thread)
    {
        deselect_thread(ap->subj->thread);
    }
    else
    {
        deselect_arts_subject(ap);
    }
}

/* Deselect all the articles in a thread.
*/
void deselect_thread(Article *thread)
{
    Subject *sp = thread->subj;
    do
    {
        deselect_subject(sp);
        sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Deselect everything.
*/
void deselect_all()
{
    for (Subject *sp = g_first_subject; sp; sp = sp->next)
    {
        deselect_subject(sp);
    }
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    g_sel_page_sp = nullptr;
    g_sel_page_app = nullptr;
    g_sel_last_ap = nullptr;
    g_sel_last_sp = nullptr;
    g_selected_only = false;
}

/* Kill all unread articles attached to this article's subject.
*/
void kill_arts_subject(Article *ap, AutoKillFlags auto_flags)
{
    if (ap->subj && ap->subj->articles)
    {
        kill_subject(ap->subj, auto_flags);
    }
    else
    {
        if (auto_flags & SET_TORETURN)
        {
            delay_unmark(ap);
        }
        set_read(ap);
        auto_flags &= AUTO_KILL_MASK;
        if (auto_flags)
        {
            change_auto_flags(ap, auto_flags);
        }
    }
}

/* Kill all unread articles attached to the given subject.
*/
void kill_subject(Subject *subj, AutoKillFlags auto_flags)
{
    int killmask = (auto_flags & AFFECT_ALL)? 0 : g_sel_mask;
    const bool toreturn = (auto_flags & SET_TORETURN) != 0;

    auto_flags &= AUTO_KILL_MASK;
    for (Article *ap = subj->articles; ap; ap = ap->subj_next)
    {
        if (toreturn)
        {
            delay_unmark(ap);
        }
        if ((ap->flags & (AF_UNREAD|killmask)) == AF_UNREAD)
        {
            set_read(ap);
        }
        if (auto_flags)
        {
            change_auto_flags(ap, auto_flags);
        }
    }
    subj->flags &= ~(SF_VISIT | SF_WAS_SELECTED);
}

/* Kill all unread articles attached to this article's thread.
*/
void kill_arts_thread(Article *ap, AutoKillFlags auto_flags)
{
    if (ap->subj && ap->subj->thread)
    {
        kill_thread(ap->subj->thread, auto_flags);
    }
    else
    {
        kill_arts_subject(ap, auto_flags);
    }
}

/* Kill all unread articles attached to the given thread.
*/
void kill_thread(Article *thread, AutoKillFlags auto_flags)
{
    Subject *sp = thread->subj;
    do
    {
        kill_subject(sp, auto_flags);
        sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Kill the subthread attached to this article.
*/
void kill_subthread(Article *ap, AutoKillFlags auto_flags)
{
    Article* limit;
    const bool toreturn = (auto_flags & SET_TORETURN) != 0;

    if (!ap)
    {
        return;
    }
    for (limit = ap; limit; limit = limit->parent)
    {
        if (limit->sibling)
        {
            limit = limit->sibling;
            break;
        }
    }

    auto_flags &= AUTO_KILL_MASK;
    for (; ap != limit; ap = bump_art(ap))
    {
        if (toreturn)
        {
            delay_unmark(ap);
        }
        if (all_bits(ap->flags, AF_EXISTS | AF_UNREAD))
        {
            set_read(ap);
        }
        if (auto_flags)
        {
            change_auto_flags(ap, auto_flags);
        }
    }
}

/* Unkill all the articles attached to the given subject.
*/
void unkill_subject(Subject *subj)
{
    int save_sel_count = g_selected_count;

    for (Article *ap = subj->articles; ap; ap = ap->subj_next)
    {
        if (g_sel_rereading)
        {
            if (all_bits(ap->flags, AF_DEL_SEL | AF_EXISTS))
            {
                if (!(ap->flags & AF_UNREAD))
                {
                    g_ngptr->toread++;
                }
                if (g_selected_only && !(ap->flags & AF_SEL))
                {
                    g_selected_count++;
                }
                ap->flags = (ap->flags & ~AF_DEL_SEL) | AF_SEL|AF_UNREAD;
            }
            else
            {
                ap->flags &= ~(AF_DEL | AF_DEL_SEL);
            }
        }
        else
        {
            if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == AF_EXISTS)
            {
                one_more(ap);
            }
            if (g_selected_only && (ap->flags & (AF_SEL | AF_UNREAD)) == AF_UNREAD)
            {
                ap->flags = (ap->flags & ~AF_DEL) | AF_SEL;
                g_selected_count++;
            }
        }
    }
    if (g_selected_count != save_sel_count //
        && g_selected_only && !(subj->flags & SF_SEL))
    {
        subj->flags |= SF_SEL | SF_VISIT | SF_WAS_SELECTED;
        g_selected_subj_cnt++;
    }
    subj->flags &= ~(SF_DEL|SF_DEL_SEL);
}

/* Unkill all the articles attached to the given thread.
*/
void unkill_thread(Article *thread)
{
    Subject *sp = thread->subj;
    do
    {
        unkill_subject(sp);
        sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Unkill the subthread attached to this article.
*/
void unkill_subthread(Article *ap)
{
    Article* limit;

    if (!ap)
    {
        return;
    }
    for (limit = ap; limit; limit = limit->parent)
    {
        if (limit->sibling)
        {
            limit = limit->sibling;
            break;
        }
    }

    Subject *sp = ap->subj;
    for (; ap != limit; ap = bump_art(ap))
    {
        if ((ap->flags & (AF_EXISTS|AF_UNREAD)) == AF_EXISTS)
        {
            one_more(ap);
        }
        if (g_selected_only && !(ap->flags & AF_SEL))
        {
            ap->flags |= AF_SEL;
            g_selected_count++;
        }
    }
    if (!(sp->flags & g_sel_mask))
    {
        g_selected_subj_cnt++;
    }
    sp->flags = (sp->flags & ~SF_DEL) | SF_SEL | SF_VISIT;
    g_selected_only = (g_selected_only || g_selected_count != 0);
}

/* Clear the auto flags in all unread articles attached to the given subject.
*/
void clear_subject(Subject *subj)
{
    for (Article *ap = subj->articles; ap; ap = ap->subj_next)
    {
        clear_auto_flags(ap);
    }
}

/* Clear the auto flags in all unread articles attached to the given thread.
*/
void clear_thread(Article *thread)
{
    Subject *sp = thread->subj;
    do
    {
        clear_subject(sp);
        sp = sp->thread_link;
    } while (sp != thread->subj);
}

/* Clear the auto flags in the subthread attached to this article.
*/
void clear_subthread(Article *ap)
{
    Article* limit;

    if (!ap)
    {
        return;
    }
    for (limit = ap; limit; limit = limit->parent)
    {
        if (limit->sibling)
        {
            limit = limit->sibling;
            break;
        }
    }

    for (; ap != limit; ap = bump_art(ap))
    {
        clear_auto_flags(ap);
    }
}

Article *subj_art(Subject *sp)
{
    int art_mask = (g_selected_only? (AF_SEL|AF_UNREAD) : AF_UNREAD);
    bool TG_save = g_threaded_group;

    g_threaded_group = (g_sel_mode == SM_THREAD);
    Article *ap = first_art(sp);
    while (ap && (ap->flags & art_mask) != art_mask)
    {
        ap = next_art(ap);
    }
    if (!ap)
    {
        g_reread = true;
        ap = first_art(sp);
        if (g_selected_only)
        {
            while (ap && !(ap->flags & AF_SEL))
            {
                ap = next_art(ap);
            }
            if (!ap)
            {
                ap = first_art(sp);
            }
        }
    }
    g_threaded_group = TG_save;
    return ap;
}

/* Find the next thread (first if g_art > g_lastart).  If articles are selected,
** only choose from threads with selected articles.
*/
void visit_next_thread()
{
    Article* ap = g_artp;

    Subject *sp = (ap ? ap->subj : nullptr);
    while ((sp = next_subj(sp, SF_VISIT)) != nullptr)
    {
        ap = subj_art(sp);
        if (ap != nullptr)
        {
            g_art = article_num(ap);
            g_artp = ap;
            return;
        }
        g_reread = false;
    }
    g_artp = nullptr;
    g_art = g_lastart+1;
    g_forcelast = true;
}

/* Find previous thread (or last if g_artp == nullptr).  If articles are selected,
** only choose from threads with selected articles.
*/
void visit_prev_thread()
{
    Article* ap = g_artp;

    Subject *sp = (ap ? ap->subj : nullptr);
    while ((sp = prev_subj(sp, SF_VISIT)) != nullptr)
    {
        ap = subj_art(sp);
        if (ap != nullptr)
        {
            g_art = article_num(ap);
            g_artp = ap;
            return;
        }
        g_reread = false;
    }
    g_artp = nullptr;
    g_art = g_lastart+1;
    g_forcelast = true;
}

/* Find g_artp's parent or oldest ancestor.  Returns false if no such
** article.  Sets g_art and g_artp otherwise.
*/
bool find_parent(bool keep_going)
{
    Article* ap = g_artp;

    if (!ap->parent)
    {
        return false;
    }

    do
    {
        ap = ap->parent;
    } while (keep_going && ap->parent);

    g_artp = ap;
    g_art = article_num(ap);
    return true;
}

/* Find g_artp's first child or youngest decendent.  Returns false if no
** such article.  Sets g_art and g_artp otherwise.
*/
bool find_leaf(bool keep_going)
{
    Article* ap = g_artp;

    if (!ap->child1)
    {
        return false;
    }

    do
    {
        ap = ap->child1;
    } while (keep_going && ap->child1);

    g_artp = ap;
    g_art = article_num(ap);
    return true;
}

/* Find the next "sibling" of g_artp, including cousins that are the
** same distance down the thread as we are.  Returns false if no such
** article.  Sets g_art and g_artp otherwise.
*/
bool find_next_sib()
{
    int      ascent = 0;
    Article *ta = g_artp;
    while (true)
    {
        while (ta->sibling)
        {
            ta = ta->sibling;
            Article *tb = first_sib(ta, ascent);
            if (tb != nullptr)
            {
                g_artp = tb;
                g_art = article_num(tb);
                return true;
            }
        }
        if (!(ta = ta->parent))
        {
            break;
        }
        ascent++;
    }
    return false;
}

/* A recursive routine to find the first node at the proper depth.  This
** article is at depth 0.
*/
static Article *first_sib(Article *ta, int depth)
{
    Article* tb;

    if (!depth)
    {
        return ta;
    }

    while (true)
    {
        if (ta->child1 && (tb = first_sib(ta->child1, depth-1)))
        {
            return tb;
        }

        if (!ta->sibling)
        {
            return nullptr;
        }

        ta = ta->sibling;
    }
}

/* Find the previous "sibling" of g_artp, including cousins that are
** the same distance down the thread as we are.  Returns false if no
** such article.  Sets g_art and g_artp otherwise.
*/
bool find_prev_sib()
{
    int      ascent = 0;
    Article *ta = g_artp;
    while (true)
    {
        Article *tb = ta;
        if (ta->parent)
        {
            ta = ta->parent->child1;
        }
        else
        {
            ta = ta->subj->thread;
        }
        tb = last_sib(ta, ascent, tb);
        if (tb != nullptr)
        {
            g_artp = tb;
            g_art = article_num(tb);
            return true;
        }
        if (!(ta = ta->parent))
        {
            break;
        }
        ascent++;
    }
    return false;
}

/* A recursive routine to find the last node at the proper depth.  This
** article is at depth 0.
*/
static Article *last_sib(Article *ta, int depth, Article *limit)
{
    Article* tb;

    if (ta == limit)
    {
        return nullptr;
    }

    if (ta->sibling)
    {
        Article *tc = ta->sibling;
        if (tc != limit && (tb = last_sib(tc,depth,limit)))
        {
            return tb;
        }
    }
    if (!depth)
    {
        return ta;
    }
    if (ta->child1)
    {
        return last_sib(ta->child1, depth - 1, limit);
    }
    return nullptr;
}

/* Get each subject's article count; count total articles and selected
** articles (use g_sel_rereading to determine whether to count read or
** unread articles); deselect any subjects we find that are empty if
** CS_UNSELECT or CS_UNSEL_STORE is specified.  If g_mode is CS_RESELECT
** is specified, the selections from the last CS_UNSEL_STORE are
** reselected.
*/
void count_subjects(CountSubjectMode cmode)
{
    Subject*sp;
    int     desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));

    g_obj_count = 0;
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    if (g_last_cached >= g_lastart)
    {
        g_firstart = g_lastart + 1;
    }

    switch (cmode)
    {
    case CS_RETAIN:
        break;

    case CS_UNSEL_STORE:
        for (sp = g_first_subject; sp; sp = sp->next)
        {
            if (sp->flags & SF_VISIT)
            {
                sp->flags = (sp->flags & ~SF_VISIT) | SF_OLD_VISIT;
            }
            else
            {
                sp->flags &= ~SF_OLD_VISIT;
            }
        }
        break;

    case CS_RESELECT:
        for (sp = g_first_subject; sp; sp = sp->next)
        {
            if (sp->flags & SF_OLD_VISIT)
            {
                sp->flags |= SF_VISIT;
            }
            else
            {
                sp->flags &= ~SF_VISIT;
            }
        }
        break;

    default:
        for (sp = g_first_subject; sp; sp = sp->next)
        {
            sp->flags &= ~SF_VISIT;
        }
    }

    for (sp = g_first_subject; sp; sp = sp->next)
    {
        std::time_t subjdate = 0;
        int    count = 0;
        int    sel_count = 0;
        for (Article *ap = sp->articles; ap; ap = ap->subj_next)
        {
            if ((ap->flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
            {
                count++;
                if (ap->flags & g_sel_mask)
                {
                    sel_count++;
                }
                if (!subjdate)
                {
                    subjdate = ap->date;
                }
                g_firstart = std::min(article_num(ap), g_firstart);
            }
        }
        sp->misc = count;
        if (subjdate)
        {
            sp->date = subjdate;
        }
        else if (!sp->date && sp->articles)
        {
            sp->date = sp->articles->date;
        }
        g_obj_count += count;
        if (cmode == CS_RESELECT)
        {
            if (sp->flags & SF_VISIT)
            {
                sp->flags = (sp->flags & ~(SF_SEL|SF_DEL)) | static_cast<SubjectFlags>(g_sel_mask);
                g_selected_count += sel_count;
                g_selected_subj_cnt++;
            }
            else
            {
                sp->flags &= ~static_cast<SubjectFlags>(g_sel_mask);
            }
        }
        else
        {
            if (sel_count //
                && (cmode >= CS_UNSEL_STORE || (sp->flags & g_sel_mask)))
            {
                sp->flags = (sp->flags & ~(SF_SEL|SF_DEL)) | static_cast<SubjectFlags>(g_sel_mask);
                g_selected_count += sel_count;
                g_selected_subj_cnt++;
            }
            else if (cmode >= CS_UNSELECT)
            {
                sp->flags &= ~static_cast<SubjectFlags>(g_sel_mask);
            }
            else if (sp->flags & g_sel_mask)
            {
                sp->flags &= ~SF_DEL;
                g_selected_subj_cnt++;
            }
            if (count && (!g_selected_only || (sp->flags & g_sel_mask)))
            {
                sp->flags |= SF_VISIT;
            }
        }
    }
    if (cmode != CS_RETAIN && cmode != CS_RESELECT //
        && !g_obj_count && !g_selected_only)
    {
        for (sp = g_first_subject; sp; sp = sp->next)
        {
            sp->flags |= SF_VISIT;
        }
    }
}

static int subjorder_subject(const Subject **spp1, const Subject**spp2)
{
    return string_case_compare((*spp1)->str+4, (*spp2)->str+4) * g_sel_direction;
}

static int subjorder_date(const Subject **spp1, const Subject**spp2)
{
    std::time_t eq = (*spp1)->date - (*spp2)->date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int subjorder_count(const Subject **spp1, const Subject**spp2)
{
    short eq = (*spp1)->misc - (*spp2)->misc;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : subjorder_date(spp1,spp2);
}

static int subjorder_lines(const Subject **spp1, const Subject**spp2)
{
    long l1 = (*spp1)->articles ? (*spp1)->articles->lines : 0;
    long l2 = (*spp2)->articles ? (*spp2)->articles->lines : 0;
    long eq = l1 - l2;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : subjorder_date(spp1,spp2);
}

/* for now, highest eligible article score */
static int subject_score_high(const Subject *sp)
{
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int hiscore = 0;
    int hiscore_found = 0;

    /* find highest score of desired articles */
    for (Article *ap = sp->articles; ap; ap = ap->subj_next)
    {
        if ((ap->flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
        {
            int sc = sc_score_art(article_num(ap), false);
            if ((!hiscore_found) || (sc > hiscore))
            {
                hiscore_found = 1;
                hiscore = sc;
            }
        }
    }
    return hiscore;
}

/* later make a subject-thread score routine */
static int subjorder_score(const Subject** spp1, const Subject** spp2)
{
    int sc1 = subject_score_high(*spp1);
    int sc2 = subject_score_high(*spp2);

    if (sc1 != sc2)
    {
        return (sc2 - sc1) * g_sel_direction;
    }
    return subjorder_date(spp1, spp2);
}

static int threadorder_subject(const Subject **spp1, const Subject**spp2)
{
    Article* t1 = (*spp1)->thread;
    Article* t2 = (*spp2)->thread;
    if (t1 != t2 && t1 && t2)
    {
        return string_case_compare(t1->subj->str + 4, t2->subj->str + 4) * g_sel_direction;
    }
    return subjorder_date(spp1, spp2);
}

static int threadorder_date(const Subject **spp1, const Subject**spp2)
{
    Article* t1 = (*spp1)->thread;
    Article* t2 = (*spp2)->thread;
    if (t1 != t2 && t1 && t2)
    {
        Subject* sp1;
        Subject* sp2;
        long eq;
        if (!(sp1 = t1->subj)->misc)
        {
            for (sp1 = sp1->thread_link; sp1 != t1->subj; sp1 = sp1->thread_link)
            {
                if (sp1->misc)
                {
                    break;
                }
            }
        }
        if (!(sp2 = t2->subj)->misc)
        {
            for (sp2 = sp2->thread_link; sp2 != t2->subj; sp2 = sp2->thread_link)
            {
                if (sp2->misc)
                {
                    break;
                }
            }
        }
        if (!(eq = sp1->date - sp2->date))
        {
            return string_case_compare(sp1->str + 4, sp2->str + 4);
        }
        return eq > 0? g_sel_direction : -g_sel_direction;
    }
    return subjorder_date(spp1, spp2);
}

static int threadorder_count(const Subject **spp1, const Subject**spp2)
{
    int size1 = (*spp1)->misc;
    int size2 = (*spp2)->misc;
    if ((*spp1)->thread != (*spp2)->thread)
    {
        Subject* sp;
        for (sp = (*spp1)->thread_link; sp != *spp1; sp = sp->thread_link)
        {
            size1 += sp->misc;
        }
        for (sp = (*spp2)->thread_link; sp != *spp2; sp = sp->thread_link)
        {
            size2 += sp->misc;
        }
    }
    if (size1 != size2)
    {
        return (size1 - size2) * g_sel_direction;
    }
    return threadorder_date(spp1, spp2);
}

static int threadorder_lines(const Subject **spp1, const Subject**spp2)
{
    Article* t1 = (*spp1)->thread;
    Article* t2 = (*spp2)->thread;
    if (t1 != t2 && t1 && t2)
    {
        Subject*sp1;
        Subject*sp2;
        if (!(sp1 = t1->subj)->misc)
        {
            for (sp1 = sp1->thread_link; sp1 != t1->subj; sp1 = sp1->thread_link)
            {
                if (sp1->misc)
                {
                    break;
                }
            }
        }
        if (!(sp2 = t2->subj)->misc)
        {
            for (sp2 = sp2->thread_link; sp2 != t2->subj; sp2 = sp2->thread_link)
            {
                if (sp2->misc)
                {
                    break;
                }
            }
        }
        long l1 = sp1->articles ? sp1->articles->lines : 0;
        long l2 = sp2->articles ? sp2->articles->lines : 0;
        long eq = l1 - l2;
        if (eq != 0)
        {
            return eq > 0 ? g_sel_direction : -g_sel_direction;
        }
        eq = sp1->date - sp2->date;
        return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
    }
    return subjorder_date(spp1, spp2);
}

static int thread_score_high(const Subject *tp)
{
    int hiscore = 0;
    int hiscore_found = 0;

    for (const Subject *sp = tp->thread_link;; sp = sp->thread_link)
    {
        int sc = subject_score_high(sp);
        if ((!hiscore_found) || (sc > hiscore))
        {
            hiscore_found = 1;
            hiscore = sc;
        }
        /* break *after* doing the last item */
        if (tp == sp)
        {
            break;
        }
    }
    return hiscore;
}

static int threadorder_score(const Subject** spp1, const Subject** spp2)
{
    int sc1 = 0;
    int sc2 = 0;

    if ((*spp1)->thread != (*spp2)->thread)
    {
        sc1 = thread_score_high(*spp1);
        sc2 = thread_score_high(*spp2);
    }
    if (sc1 != sc2)
    {
        return (sc2 - sc1) * g_sel_direction;
    }
    return threadorder_date(spp1, spp2);
}

/* Sort the subjects according to the chosen order.
*/
void sort_subjects()
{
    Subject* sp;
    Subject**lp;
    int (*   sort_procedure)(const Subject **spp1, const Subject**spp2);

    /* If we don't have at least two subjects, we're done! */
    if (!g_first_subject || !g_first_subject->next)
    {
        return;
    }

    switch (g_sel_sort)
    {
    case SS_DATE:
    default:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          threadorder_date : subjorder_date);
        break;

    case SS_STRING:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          threadorder_subject : subjorder_subject);
        break;

    case SS_COUNT:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          threadorder_count : subjorder_count);
        break;

    case SS_LINES:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          threadorder_lines : subjorder_lines);
        break;

    /* if SCORE is undefined, use the default above */
    case SS_SCORE:
        sort_procedure = (g_sel_mode == SM_THREAD?
                          threadorder_score : subjorder_score);
        break;
    }

    Subject **subj_list = (Subject**)safemalloc(g_subject_count * sizeof(Subject*));
    for (lp = subj_list, sp = g_first_subject; sp; sp = sp->next)
    {
        *lp++ = sp;
    }
    TRN_ASSERT(lp - subj_list == g_subject_count);

    std::qsort(subj_list, g_subject_count, sizeof (Subject*), ((int(*)(void const *, void const *))sort_procedure));

    g_first_subject = subj_list[0];
    sp = subj_list[0];
    sp->prev = nullptr;
    lp = subj_list;
    for (int i = g_subject_count; --i; lp++)
    {
        lp[0]->next = lp[1];
        lp[1]->prev = lp[0];
        if (g_sel_mode == SM_THREAD)
        {
            if (lp[0]->thread && lp[0]->thread == lp[1]->thread)
            {
                lp[0]->thread_link = lp[1];
            }
            else
            {
                lp[0]->thread_link = sp;
                sp = lp[1];
            }
        }
    }
    g_last_subject = lp[0];
    g_last_subject->next = nullptr;
    if (g_sel_mode == SM_THREAD)
    {
        g_last_subject->thread_link = sp;
    }
    std::free((char*)subj_list);
}

static int artorder_date(const Article **art1, const Article **art2)
{
    long eq = (*art1)->date - (*art2)->date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int artorder_subject(const Article **art1, const Article **art2)
{
    if ((*art1)->subj == (*art2)->subj)
    {
        return artorder_date(art1, art2);
    }
    return string_case_compare((*art1)->subj->str + 4, (*art2)->subj->str + 4)
        * g_sel_direction;
}

static int artorder_author(const Article **art1, const Article **art2)
{
    int eq = string_case_compare((*art1)->from, (*art2)->from);
    return eq? eq * g_sel_direction : artorder_date(art1, art2);
}

static int artorder_number(const Article **art1, const Article **art2)
{
    ArticleNum eq = article_num(*art1) - article_num(*art2);
    return eq > 0? g_sel_direction : -g_sel_direction;
}

static int artorder_groups(const Article **art1, const Article **art2)
{
    long eq;
#ifdef DEBUG
    TRN_ASSERT((*art1)->subj != nullptr);
    TRN_ASSERT((*art2)->subj != nullptr);
#endif
    if ((*art1)->subj == (*art2)->subj)
    {
        return artorder_date(art1, art2);
    }
    eq = (*art1)->subj->date - (*art2)->subj->date;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

static int artorder_lines(const Article **art1, const Article **art2)
{
    long eq = (*art1)->lines - (*art2)->lines;
    return eq? eq > 0? g_sel_direction : -g_sel_direction : artorder_date(art1,art2);
}

static int artorder_score(const Article **art1, const Article **art2)
{
    int eq = sc_score_art(article_num(*art2),false)
           - sc_score_art(article_num(*art1),false);
    return eq? eq > 0? g_sel_direction : -g_sel_direction : 0;
}

/* Sort the articles according to the chosen order.
*/
void sort_articles()
{
    int (*sort_procedure)(const Article **art1, const Article **art2);

    build_artptrs();

    /* If we don't have at least two articles, we're done! */
    if (g_art_ptr_list_size < 2)
    {
        return;
    }

    switch (g_sel_sort)
    {
    case SS_DATE:
    default:
        sort_procedure = artorder_date;
        break;

    case SS_STRING:
        sort_procedure = artorder_subject;
        break;

    case SS_AUTHOR:
        sort_procedure = artorder_author;
        break;

    case SS_NATURAL:
        sort_procedure = artorder_number;
        break;

    case SS_GROUPS:
        sort_procedure = artorder_groups;
        break;

    case SS_LINES:
        sort_procedure = artorder_lines;
        break;

    case SS_SCORE:
        sort_procedure = artorder_score;
        break;
    }
    g_sel_page_app = nullptr;
    std::qsort(g_art_ptr_list, g_art_ptr_list_size, sizeof (Article*), ((int(*)(void const *, void const *))sort_procedure));
}

static void build_artptrs()
{
    ArticleNum count = g_obj_count;
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));

    if (!g_art_ptr_list || g_art_ptr_list_size != count)
    {
        g_art_ptr_list = (Article**)saferealloc((char*)g_art_ptr_list,
                (MemorySize)count * sizeof (Article*));
        g_art_ptr_list_size = count;
    }
    Article **app = g_art_ptr_list;
    for (ArticleNum an = article_first(g_absfirst); count; an = article_next(an))
    {
        Article *ap = article_ptr(an);
        if ((ap->flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
        {
            *app++ = ap;
            count--;
        }
    }
}
