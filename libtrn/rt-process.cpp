/* rt-process.cpp
*/
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-process.h>

#include <config/common.h>
#include <trn/cache.h>
#include <trn/hash.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/util2.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>

static Article *allocate_article(ArticleNum artnum);

namespace
{

struct PendingMessageId
{
    std::string msg_id;
};

PendingMessageId *pending_msg_id(HashDatum data)
{
    return reinterpret_cast<PendingMessageId *>(data.dat_ptr);
}

} // namespace

// This depends on art being set to the current article number.
static Article *allocate_article(ArticleNum artnum)
{
    Article* article;

    // create an new article
    if (artnum >= g_abs_first)
    {
        article = article_ptr(artnum);
    }
    else
    {
        article = new Article{};
        article->m_flags |= AF_FAKE|AF_TMP_MEM;
    }
    return article;
}

HashDatum make_pending_msg_id(std::string_view msg_id, unsigned flags)
{
    return {reinterpret_cast<char *>(new PendingMessageId{std::string{msg_id}}), flags};
}

std::string_view hash_msg_id_view(HashDatum data)
{
    return data.dat_len ? pending_msg_id(data)->msg_id : reinterpret_cast<Article *>(data.dat_ptr)->msg_id_view();
}

std::string take_pending_msg_id(HashDatum *data)
{
    PendingMessageId *pending = pending_msg_id(*data);
    std::string       msg_id = std::move(pending->msg_id);

    delete pending;
    data->dat_ptr = nullptr;
    return msg_id;
}

void free_pending_msg_id(HashDatum *data)
{
    delete pending_msg_id(*data);
    data->dat_ptr = nullptr;
}

std::string fix_msg_id(std::string_view msgid)
{
    std::string       normalized{msgid};
    const std::size_t domain = normalized.find('@');
    if (domain != std::string::npos)
    {
        for (std::size_t idx = domain + 1; idx < normalized.size(); idx++)
        {
            const unsigned char ch = static_cast<unsigned char>(normalized[idx]);
            if (std::isupper(ch))
            {
                normalized[idx] = static_cast<char>(std::tolower(ch)); // lower-case domain portion
            }
        }
    }
    return normalized;
}

int msg_id_cmp(std::string_view key, HashDatum data)
{
    return key.compare(hash_msg_id_view(data));
}

// Take a message-id and see if we already know about it.  If so, return
// the article, otherwise create a fake one.
//
Article *get_article(std::string_view msgid)
{
    Article* article;
    std::string normalized_msg_id = fix_msg_id(msgid);

    HashDatum data = hash_fetch(g_msg_id_hash, normalized_msg_id);
    if (data.dat_len)
    {
        const unsigned hash_flags = data.dat_len;
        article = allocate_article(ArticleNum{});
        article->m_auto_flags = static_cast<AutoKillFlags>(hash_flags) & (AUTO_SEL_MASK | AUTO_KILL_MASK);
        if ((hash_flags & KF_AGE_MASK) == 0)
        {
            article->m_auto_flags |= AUTO_OLD;
        }
        else
        {
            g_kf_change_thread_cnt++;
        }
        article->m_msg_id = take_pending_msg_id(&data);
        data.dat_ptr = (char *) article;
        data.dat_len = 0;
        hash_store_last(data);
    }
    else if (!(article = (Article *) data.dat_ptr))
    {
        article = allocate_article(ArticleNum{});
        data.dat_ptr = (char *) article;
        article->m_msg_id = std::move(normalized_msg_id);
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
