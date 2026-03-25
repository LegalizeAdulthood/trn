/* rt-process.c
*/
// This software is copyrighted as detailed in the LICENSE file.

#include "config/common.h"
#include "trn/rt-process.h"

#include "trn/list.h"
#include "trn/ngdata.h"
#include "trn/cache.h"
#include "trn/hash.h"
#include "trn/kfile.h"
#include "trn/ng.h"
#include "trn/rt-select.h"
#include "trn/rthread.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

// This depends on art being set to the current article number.
Article *allocate_article(ArticleNum artnum)
{
    Article* article;

    // create an new article
    if (artnum >= g_abs_first)
    {
        article = article_ptr(artnum);
    }
    else
    {
        article = (Article*)safe_malloc(sizeof (Article));
        std::memset((char*)article,0,sizeof (Article));
        article->m_flags |= AF_FAKE|AF_TMP_MEM;
    }
    return article;
}

void fix_msg_id(char *msgid)
{
    char *cp = std::strchr(msgid, '@');
    if (cp != nullptr)
    {
        while (*++cp)
        {
            if (std::isupper(*cp))
            {
                *cp = std::tolower(*cp);     // lower-case domain portion
            }
        }
    }
}

int msg_id_cmp(const char *key, int key_len, HashDatum data)
{
    // We already know that the lengths are equal, just compare the strings
    if (data.dat_len)
    {
        return std::memcmp(key, data.dat_ptr, key_len);
    }
    return std::memcmp(key, ((Article*)data.dat_ptr)->m_msg_id, key_len);
}

// Take a message-id and see if we already know about it.  If so, return
// the article, otherwise create a fake one.
//
Article *get_article(char *msgid)
{
    Article* article;

    fix_msg_id(msgid);

    HashDatum data = hash_fetch(g_msg_id_hash, msgid, std::strlen(msgid));
    if (data.dat_len)
    {
        article = allocate_article(ArticleNum{});
        article->m_auto_flags = static_cast<AutoKillFlags>(data.dat_len) & (AUTO_SEL_MASK | AUTO_KILL_MASK);
        if ((data.dat_len & KF_AGE_MASK) == 0)
        {
            article->m_auto_flags |= AUTO_OLD;
        }
        else
        {
            g_kf_change_thread_cnt++;
        }
        article->m_msg_id = data.dat_ptr;
        data.dat_ptr = (char*)article;
        data.dat_len = 0;
        hash_store_last(data);
    }
    else if (!(article = (Article *) data.dat_ptr))
    {
        article = allocate_article(ArticleNum{});
        data.dat_ptr = (char*)article;
        article->m_msg_id = save_str(msgid);
        hash_store_last(data);
    }
    return article;
}

// Remove an article from its parent/siblings.  Leave parent pointer intact.
void unlink_child(Article *child)
{
    Article* last;

    if (!(last = child->m_parent))
    {
        Subject* sp = child->m_subj;
        last = sp->m_thread;
        if (last == child)
        {
            do
            {
                sp->m_thread = child->m_sibling;
                sp = sp->m_thread_link;
            } while (sp != child->m_subj);
        }
        else
        {
            goto sibling_search;
        }
    }
    else
    {
        if (last->m_child1 == child)
        {
            last->m_child1 = child->m_sibling;
        }
        else
        {
            last = last->m_child1;
sibling_search:
            while (last && last->m_sibling != child)
            {
                last = last->m_sibling;
            }
            if (last)
            {
                last->m_sibling = child->m_sibling;
            }
        }
    }
}

// Merge all of s2's thread into s1's thread.
void merge_threads(Subject *s1, Subject *s2)
{
    Article *t1 = s1->m_thread;
    Article *t2 = s2->m_thread;
    // Change all of t2's thread pointers to a common lead article
    Subject *sp = s2;
    do
    {
        sp->m_thread = t1;
        sp = sp->m_thread_link;
    } while (sp != s2);

    // Join the two circular lists together
    sp = s2->m_thread_link;
    s2->m_thread_link = s1->m_thread_link;
    s1->m_thread_link = sp;

    // If thread mode is set, ensure the subjects are adjacent in the list.
    // Don't do this if the selector is active, because it gets messed up.
    if (g_sel_mode == SM_THREAD && g_general_mode != GM_SELECTOR)
    {
        for (sp = s2; sp->m_prev && sp->m_prev->m_thread == t1;)
        {
            sp = sp->m_prev;
            if (sp == s1)
            {
                goto artlink;
            }
        }
        while (s2->m_next && s2->m_next->m_thread == t1)
        {
            s2 = s2->m_next;
            if (s2 == s1)
            {
                goto artlink;
            }
        }
        // Unlink the s2 chunk of subjects from the list
        if (!sp->m_prev)
        {
            g_first_subject = s2->m_next;
        }
        else
        {
            sp->m_prev->m_next = s2->m_next;
        }
        if (!s2->m_next)
        {
            g_last_subject = sp->m_prev;
        }
        else
        {
            s2->m_next->m_prev = sp->m_prev;
        }
        // Link the s2 chunk after s1
        sp->m_prev = s1;
        s2->m_next = s1->m_next;
        if (!s1->m_next)
        {
            g_last_subject = s2;
        }
        else
        {
            s1->m_next->m_prev = s2;
        }
        s1->m_next = sp;
    }

artlink:
    // Link each article that was attached to t2 to t1.
      for (t1 = t2; t1; t1 = t2)
      {
        t2 = t2->m_sibling;
        t1->link_child();      // parent is null, thread is newly set
    }
}
