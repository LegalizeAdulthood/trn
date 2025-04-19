// This file Copyright 1992 by Clifford A. Adams
/* samisc.c
 *
 * lower-level routines
 */

#include "config/common.h"
#include "trn/samisc.h"

#include "trn/list.h"
#include "trn/ngdata.h" // several
#include "trn/artio.h"         // g_openart
#include "trn/bits.h"
#include "trn/cache.h"
#include "trn/ng.h" // for "g_art"
#include "trn/rthread.h"
#include "trn/samain.h"
#include "trn/sathread.h"
#include "trn/scanart.h"
#include "trn/score.h"
#include "trn/sorder.h"

inline int check_article(ArticleNum a)
{
    return a >= g_abs_first && a <= g_last_art;
}

// ignoring "Fold" (or later recursive) mode(s), is this article eligible?
bool sa_basic_elig(long a)
{
    ArticleNum artnum = g_sa_ents[a].artnum;
    TRN_ASSERT(check_article(artnum));

    // "run the gauntlet" style (:-)
    if (!g_sa_mode_read_elig && was_read(artnum))
    {
        return false;
    }
    if (g_sa_mode_zoom && !sa_selected1(a))
    {
        return false;
    }
    if (g_sa_mode_order == SA_ORDER_DESCENDING) // score order
    {
        if (!article_scored(artnum))
        {
            return false;
        }
    }
    // now just check availability
    if (is_unavailable(artnum))
    {
        if (!was_read(artnum))
        {
            one_less_art_num(artnum);
        }
        return false;           // don't try positively unavailable
    }
    // consider later positively checking availability
    return true;        // passed all tests
}

bool sa_eligible(long a)
{
    TRN_ASSERT(check_article(g_sa_ents[a].artnum));
    if (!sa_basic_elig(a))
    {
        return false;           // must always be basic-eligible
    }
    if (!g_sa_mode_fold)
    {
        return true;            // just use basic-eligible
    }
    if (sa_subj_thread_prev(a))
    {
        return false;           // there was an earlier match
    }
    return true;                // no prior matches
}

// given an article number, return the entry number for that article
// (There is no easy mapping between entry numbers and article numbers.)
long sa_artnum_to_ent(ArticleNum artnum)
{
    for (long i = 1; i < g_sa_num_ents; i++)
    {
        if (g_sa_ents[i].artnum == artnum)
        {
            return i;
        }
    }
    // this had better not happen (complain?)
    return -1;
}

// select1 the articles picked in the TRN thread selector
void sa_sel_threads()
{
    bool want_unread;
#if 0
    // this does not work now, but maybe it will help debugging?
    int subj_mask = (g_sel_mode == SM_THREAD? (SF_THREAD|SF_VISIT) : SF_VISIT);
#endif
    int subj_mask = SF_VISIT;

    want_unread = !g_sa_mode_read_elig;

    // clear any old selections
    for (long i = 1; i < g_sa_num_ents; i++)
    {
        sa_clear_select1(i);
    }

    // Loop through all (selected) articles.
    for (Subject *sp = g_first_subject; sp; sp = sp->next)
    {
        if ((sp->flags & subj_mask) == subj_mask)
        {
            for (Article *ap = first_art(sp); ap; ap = next_article(ap))
            {
                ArticleNum art = article_num(ap);
                if ((ap->flags & AF_SEL) //
                    && (!(ap->flags & AF_UNREAD) ^ want_unread))
                {
                    // this was a trn-thread selected article
                    sa_select1(sa_artnum_to_ent(art));
                    // if scoring, make sure that this article is scored...
                    if (g_sa_mode_order == SA_ORDER_DESCENDING) // score order
                    {
                        sc_score_art(art,false);
                    }
                }
                }// for all articles
            }// if selected
        }// for all threads
    s_sort();
}

int sa_number_arts()
{
    int total = 0;
    for (int i = 1; i < g_sa_num_ents; i++)
    {
        ArticleNum a = g_sa_ents[i].artnum;
        if (is_unavailable(a))
        {
            continue;
        }
        if (!article_unread(a) && !g_sa_mode_read_elig)
        {
            continue;
        }
        total++;
    }
    return total;
}

// needed for a couple functions which act within the
// scope of an article.
//
void sa_go_art(ArticleNum a)
{
    g_art = a;
    (void)article_find(g_art);
    if (g_open_art != g_art)
    {
        art_open(g_art, (ArticlePosition) 0);
    }
}

// long a,b;            // the entry numbers to compare
int sa_compare(long a, long b)
{
    if (g_sa_mode_order == SA_ORDER_DESCENDING) // score order
    {
        // do not score the articles here--move the articles to
        // the end of the list if unscored.
        //
        if (!article_scored(g_sa_ents[a].artnum))                         // a unscored
        {
            if (!article_scored(g_sa_ents[b].artnum))   // a+b unscored
            {
                if (a < b)                                        // keep ordering consistent
                {
                    return -1;
                }
                return 1;
            }
            return 1;           // move unscored (a) to end
        }
        if (!article_scored(g_sa_ents[b].artnum))       // only b unscored
        {
            return -1;          // move unscored (b) to end
        }

        long i = sc_score_art(g_sa_ents[a].artnum, true);
        long j = sc_score_art(g_sa_ents[b].artnum, true);
        if (i < j)
        {
            return 1;
        }
        if (i > j)
        {
            return -1;
        }
        // i == j
        if (g_score_new_first)
        {
            if (a < b)
            {
                return 1;
            }
            return -1;
        }
        if (a < b)
        {
            return -1;
        }
        return 1;
    }
    if (a < b)
    {
        return -1;
    }
    return 1;
}
