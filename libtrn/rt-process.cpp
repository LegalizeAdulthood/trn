/* rt-process.c
*/
/* This software is copyrighted as detailed in the LICENSE file. */

#include "config/common.h"
#include "trn/rt-process.h"

#include "trn/cache.h"
#include "trn/hash.h"
#include "trn/kfile.h"
#include "trn/list.h"
#include "trn/ng.h"
#include "trn/ngdata.h"
#include "trn/rt-select.h"
#include "trn/rthread.h"
#include "trn/string-algos.h"
#include "trn/terminal.h"
#include "trn/util.h"
#include "util/util2.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

static void fix_msgid(char *msgid);
static char *valid_message_id(char *start, char *end);
static void unlink_child(ARTICLE *child);

/* This depends on art being set to the current article number.
*/
ARTICLE *allocate_article(ART_NUM artnum)
{
    ARTICLE* article;

    /* create an new article */
    if (artnum >= g_absfirst)
    {
        article = article_ptr(artnum);
    }
    else
    {
        article = (ARTICLE*)safemalloc(sizeof (ARTICLE));
        std::memset((char*)article,0,sizeof (ARTICLE));
        article->flags |= AF_FAKE|AF_TMPMEM;
    }
    return article;
}

static void fix_msgid(char *msgid)
{
    char *cp = std::strchr(msgid, '@');
    if (cp != nullptr)
    {
        while (*++cp)
        {
            if (std::isupper(*cp))
            {
                *cp = std::tolower(*cp);     /* lower-case domain portion */
            }
        }
    }
}

int msgid_cmp(const char *key, int keylen, HASHDATUM data)
{
    /* We already know that the lengths are equal, just compare the strings */
    if (data.dat_len)
    {
        return std::memcmp(key, data.dat_ptr, keylen);
    }
    return std::memcmp(key, ((ARTICLE*)data.dat_ptr)->msgid, keylen);
}

static Subject *s_fake_had_subj; /* the fake-turned-real article had this subject */

bool valid_article(ARTICLE *article)
{
    char* msgid = article->msgid;

    if (msgid)
    {
        fix_msgid(msgid);
        HASHDATUM data = hashfetch(g_msgid_hash, msgid, std::strlen(msgid));
        if (data.dat_len)
        {
            safefree0(data.dat_ptr);
            article->autofl = static_cast<autokill_flags>(data.dat_len) & (AUTO_SEL_MASK | AUTO_KILL_MASK);
            if ((data.dat_len & KF_AGE_MASK) == 0)
            {
                article->autofl |= AUTO_OLD;
            }
            else
            {
                g_kf_changethd_cnt++;
            }
            data.dat_len = 0;
        }
        ARTICLE *fake_ap = (ARTICLE*)data.dat_ptr;
        if (fake_ap == nullptr)
        {
            data.dat_ptr = (char*)article;
            hashstorelast(data);
            s_fake_had_subj = nullptr;
            return true;
        }
        if (fake_ap == article)
        {
            s_fake_had_subj = nullptr;
            return true;
        }

        /* Whenever we replace a fake art with a real one, it's a lot of work
        ** cleaning up the references.  Fortunately, this is not often. */
        if (fake_ap && (fake_ap->flags & AF_TMPMEM))
        {
            article->parent = fake_ap->parent;
            article->child1 = fake_ap->child1;
            article->sibling = fake_ap->sibling;
            s_fake_had_subj = fake_ap->subj;
            if (fake_ap->autofl)
            {
                article->autofl |= fake_ap->autofl;
                g_kf_state |= g_kfs_thread_change_set;
            }
            if (g_curr_artp == fake_ap)
            {
                g_curr_artp = article;
                g_curr_art = article_num(article);
            }
            if (g_recent_artp == fake_ap)
            {
                g_recent_artp = article;
                g_recent_art = article_num(article);
            }
            ARTICLE *ap = article->parent;
            if (ap != nullptr)
            {
                if (ap->child1 == fake_ap)
                {
                    ap->child1 = article;
                }
                else
                {
                    ap = ap->child1;
                    /* This sibling-search code is duplicated below */
                    while (ap->sibling)
                    {
                        if (ap->sibling == fake_ap)
                        {
                            ap->sibling = article;
                            break;
                        }
                        ap = ap->sibling;
                    }
                    /* End of slibling-search code */
                }
            }
            else if (s_fake_had_subj)
            {
                Subject* sp = s_fake_had_subj;
                ap = sp->thread;
                if (ap == fake_ap)
                {
                    do
                    {
                        sp->thread = article;
                        sp = sp->thread_link;
                    } while (sp != s_fake_had_subj);
                }
                else
                {
                    /* This sibling-search code is duplicated above */
                    while (ap->sibling)
                    {
                        if (ap->sibling == fake_ap)
                        {
                            ap->sibling = article;
                            break;
                        }
                        ap = ap->sibling;
                    }
                    /* End of slibling-search code */
                }
            }
            for (ap = article->child1; ap; ap = ap->sibling)
            {
                ap->parent = article;
            }
            clear_article(fake_ap);
            std::free(fake_ap);
            data.dat_ptr = (char*)article;
            hashstorelast(data);
            return true;
        }
    }
    /* Forget about the duplicate message-id or bogus article. */
    uncache_article(article,true);
    return false;
}

/* Take a message-id and see if we already know about it.  If so, return
** the article, otherwise create a fake one.
*/
ARTICLE *get_article(char *msgid)
{
    ARTICLE* article;

    fix_msgid(msgid);

    HASHDATUM data = hashfetch(g_msgid_hash, msgid, std::strlen(msgid));
    if (data.dat_len)
    {
        article = allocate_article(0);
        article->autofl = static_cast<autokill_flags>(data.dat_len) & (AUTO_SEL_MASK | AUTO_KILL_MASK);
        if ((data.dat_len & KF_AGE_MASK) == 0)
        {
            article->autofl |= AUTO_OLD;
        }
        else
        {
            g_kf_changethd_cnt++;
        }
        article->msgid = data.dat_ptr;
        data.dat_ptr = (char*)article;
        data.dat_len = 0;
        hashstorelast(data);
    }
    else if (!(article = (ARTICLE *) data.dat_ptr))
    {
        article = allocate_article(0);
        data.dat_ptr = (char*)article;
        article->msgid = savestr(msgid);
        hashstorelast(data);
    }
    return article;
}

/* Take all the data we've accumulated about the article and shove it into
** the article tree at the best place we can deduce.
*/
void thread_article(ARTICLE *article, char *references)
{
    ARTICLE* ap;
    ARTICLE* prev;
    char* cp;
    char* end;
    autokill_flags chain_autofl =
        article->autofl | (article->subj->articles ? article->subj->articles->autofl : AUTO_KILL_NONE);
    autokill_flags subj_autofl = AUTO_KILL_NONE;
    const bool rethreading = (article->flags & AF_THREADED) != 0;

    /* We're definitely not a fake anymore */
    article->flags = (article->flags & ~AF_FAKE) | AF_THREADED;

    /* If the article was already part of an existing thread, unlink it
    ** to try to put it in the best possible spot.
    */
    if (s_fake_had_subj)
    {
        if (s_fake_had_subj->thread != article->subj->thread)
        {
            merge_threads(s_fake_had_subj, article->subj);
        }
        /* Check for a real or shared-fake parent */
        ap = article->parent;
        while (ap && (ap->flags & AF_FAKE) && !ap->child1->sibling)
        {
            prev = ap;
            ap = ap->parent;
        }
        ARTICLE *stopper = ap;
        unlink_child(article);
        /* We'll assume that this article has as good or better references
        ** than the child that faked us initially.  Free the fake reference-
        ** chain and process our references as usual.
        */
        for (ap = article->parent; ap != stopper; ap = prev)
        {
            unlink_child(ap);
            prev = ap->parent;
            ap->date = 0;
            ap->subj = nullptr;
            ap->parent = nullptr;
            /* don't free it until group exit since we probably re-use it */
        }
        article->parent = nullptr;              /* neaten up */
        article->sibling = nullptr;
    }

    /* If we have references, process them from the right end one at a time
    ** until we either run into somebody, or we run out of references.
    */
    if (references && *references)
    {
        prev = article;
        ap = nullptr;
        if ((cp = std::strrchr(references, '<')) == nullptr //
            || (end = std::strchr(cp + 1, ' ')) == nullptr)
        {
            end = references + std::strlen(references) - 1;
        }
        while (cp)
        {
            while (end >= cp && end > references //
                   && (*(unsigned char *) end <= ' ' || *end == ','))
            {
                end--;
            }
            if (end <= cp)
            {
                break;
            }
            end[1] = '\0';
            /* Quit parsing references if this one is garbage. */
            if (!(end = valid_message_id(cp, end)))
            {
                break;
            }
            /* Dump all domains that end in '.', such as "..." & "1@DEL." */
            if (end[-1] == '.')
            {
                break;
            }
            ap = get_article(cp);
            *cp = '\0';
            chain_autofl |= ap->autofl;
            if (ap->subj == article->subj)
            {
                subj_autofl |= ap->autofl;
            }

            /* Check for duplicates on the reference line.  Brand-new data has
            ** no date.  Data we just allocated earlier on this line has a
            ** date but no subj.  Special-case the article itself, since it
            ** does have a subj.
            */
            if ((ap->date && !ap->subj) || ap == article)
            {
                ap = prev;
                if (ap == article)
                {
                    ap = nullptr;
                }
                goto next;
            }

            /* When we're doing late processing of In-Reply-To: lines, we may
            ** have to move an article from an old position.
            */
            if (rethreading && prev->subj)
            {
                unlink_child(prev);
            }
            prev->parent = ap;
            link_child(prev);
            if (ap->subj)
            {
                break;
            }

            ap->date = article->date;
            prev = ap;
          next:
            if (cp > references)
            {
                end = cp-1;
            }
            else
            {
                end = cp;
            }
            cp = std::strrchr(references, '<');
        }
        if (!ap)
        {
            goto no_references;
        }

        /* Check if we ran into anybody that was already linked.  If so, we
        ** just use their thread.
        */
        if (ap->subj)
        {
            /* See if this article spans the gap between what we thought
            ** were two different threads.
            */
            if (article->subj->thread != ap->subj->thread)
            {
                merge_threads(ap->subj, article->subj);
            }
        }
        else
        {
            /* We didn't find anybody we knew, so either create a new thread
            ** or use the article's thread if it was previously faked.
            */
            ap->subj = article->subj;
            link_child(ap);
        }
        /* Set the subj of faked articles we created as references. */
        for (ap = article->parent; ap && !ap->subj; ap = ap->parent)
        {
            ap->subj = article->subj;
        }

        /* Make sure we didn't circularly link to a child article(!), by
        ** ensuring that we run off the top before we run into ourself.
        */
        while (ap && ap->parent != article)
        {
            ap = ap->parent;
        }
        if (ap)
        {
            /* Ugh.  Someone's tweaked reference line with an incorrect
            ** article-order arrived first, and one of our children is
            ** really one of our ancestors. Cut off the bogus child branch
            ** right where we are and link it to the thread.
            */
            unlink_child(ap);
            ap->parent = nullptr;
            link_child(ap);
        }
    }
    else
    {
      no_references:
        /* The article has no references.  Either turn it into a new thread
        ** or re-attach the fleshed-out article to its old thread.  Don't
        ** touch it at all unless this is the first attempt at threading it.
        */
        if (!rethreading)
        {
            link_child(article);
        }
    }
    if (!(article->flags & AF_CACHED))
    {
        cache_article(article);
    }
    autokill_flags thread_autofl = chain_autofl;
    if (g_sel_mode == SM_THREAD)
    {
        Subject* sp = article->subj->thread_link;
        while (sp != article->subj)
        {
            if (sp->articles)
            {
                thread_autofl |= sp->articles->autofl;
            }
            sp = sp->thread_link;
        }
    }
    subj_autofl |= article->subj->articles->autofl;

    perform_auto_flags(article, thread_autofl, subj_autofl, chain_autofl);
}

void rover_thread(ARTICLE *article, char *s)
{
    ARTICLE*prev = article;

    while (true)
    {
        s = skip_eq(++s, ' ');
        if (std::isdigit(*s))
        {
            article = article_ptr(std::atol(s));
            prev->parent = article;
            link_child(prev);
            break;
        }
        char *end = std::strchr(s, '>');
        if (!end)
        {
            return;                             /* Impossible! */
        }
        char ch = end[1];
        end[1] = '\0';
        article = get_article(s);
        prev->parent = article;
        link_child(prev);
        if (!ch)
        {
            break;
        }
        end[1] = ch;
        s = end;
    }
}

/* Check if the string we've found looks like a valid message-id reference.
*/
static char *valid_message_id(char *start, char *end)
{
    char* mid;

    if (start == end)
    {
        return nullptr;
    }

    if (*end != '>')
    {
        /* Compensate for space cadets who include the header in their
        ** subsitution of all '>'s into another citation character.
        */
        if (*end == '<' || *end == '-' || *end == '!' || *end == '%'    //
            || *end == ')' || *end == '|' || *end == ':' || *end == '}' //
            || *end == '*' || *end == '+' || *end == '#' || *end == ']' //
            || *end == '@' || *end == '$')
        {
            *end = '>';
        }
    }
    else if (end[-1] == '>')
    {
        *(end--) = '\0';
    }
    /* Id must be "<...@...>" */
    if (*start != '<' || *end != '>' || (mid = std::strchr(start, '@')) == nullptr //
        || mid == start + 1 || mid + 1 == end)
    {
        return nullptr;
    }
    return end;
}

/* Remove an article from its parent/siblings.  Leave parent pointer intact.
*/
static void unlink_child(ARTICLE *child)
{
    ARTICLE* last;

    if (!(last = child->parent))
    {
        Subject* sp = child->subj;
        last = sp->thread;
        if (last == child)
        {
            do
            {
                sp->thread = child->sibling;
                sp = sp->thread_link;
            } while (sp != child->subj);
        }
        else
        {
            goto sibling_search;
        }
    }
    else
    {
        if (last->child1 == child)
        {
            last->child1 = child->sibling;
        }
        else
        {
            last = last->child1;
          sibling_search:
            while (last && last->sibling != child)
            {
                last = last->sibling;
            }
            if (last)
            {
                last->sibling = child->sibling;
            }
        }
    }
}

/* Link an article to its parent article.  If its parent pointer is zero,
** link it to its thread.  Sorts siblings by date.
*/
void link_child(ARTICLE *child)
{
    ARTICLE* ap;

    if (!(ap = child->parent))
    {
        Subject* sp = child->subj;
        ap = sp->thread;
        if (!ap || child->date < ap->date)
        {
            do
            {
                sp->thread = child;
                sp = sp->thread_link;
            } while (sp != child->subj);
            child->sibling = ap;
        }
        else
        {
            goto sibling_search;
        }
    }
    else
    {
        ap = ap->child1;
        if (!ap || child->date < ap->date)
        {
            child->sibling = ap;
            child->parent->child1 = child;
        }
        else
        {
          sibling_search:
            while (ap->sibling && ap->sibling->date <= child->date)
            {
                ap = ap->sibling;
            }
            child->sibling = ap->sibling;
            ap->sibling = child;
        }
    }
}

/* Merge all of s2's thread into s1's thread.
*/
void merge_threads(Subject *s1, Subject *s2)
{
    ARTICLE *t1 = s1->thread;
    ARTICLE *t2 = s2->thread;
    /* Change all of t2's thread pointers to a common lead article */
    Subject *sp = s2;
    do
    {
        sp->thread = t1;
        sp = sp->thread_link;
    } while (sp != s2);

    /* Join the two circular lists together */
    sp = s2->thread_link;
    s2->thread_link = s1->thread_link;
    s1->thread_link = sp;

    /* If thread mode is set, ensure the subjects are adjacent in the list. */
    /* Don't do this if the selector is active, because it gets messed up. */
    if (g_sel_mode == SM_THREAD && g_general_mode != GM_SELECTOR)
    {
        for (sp = s2; sp->prev && sp->prev->thread == t1;)
        {
            sp = sp->prev;
            if (sp == s1)
            {
                goto artlink;
            }
        }
        while (s2->next && s2->next->thread == t1)
        {
            s2 = s2->next;
            if (s2 == s1)
            {
                goto artlink;
            }
        }
        /* Unlink the s2 chunk of subjects from the list */
        if (!sp->prev)
        {
            g_first_subject = s2->next;
        }
        else
        {
            sp->prev->next = s2->next;
        }
        if (!s2->next)
        {
            g_last_subject = sp->prev;
        }
        else
        {
            s2->next->prev = sp->prev;
        }
        /* Link the s2 chunk after s1 */
        sp->prev = s1;
        s2->next = s1->next;
        if (!s1->next)
        {
            g_last_subject = s2;
        }
        else
        {
            s1->next->prev = s2;
        }
        s1->next = sp;
    }

  artlink:
    /* Link each article that was attached to t2 to t1. */
      for (t1 = t2; t1; t1 = t2)
      {
        t2 = t2->sibling;
        link_child(t1);      /* parent is null, thread is newly set */
    }
}
