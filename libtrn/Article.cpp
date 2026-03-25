// This software is copyrighted as detailed in the LICENSE file.

#include <trn/Article.h>

#include <config/common.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/rt-process.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/string-algos.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

static Subject *s_fake_had_subj{}; // the fake-turned-real article had this subject

// Check if the string we've found looks like a valid message-id reference.
static char *valid_message_id(char *start, char *end)
{
    char* mid;

    if (start == end)
    {
        return nullptr;
    }

    if (*end != '>')
    {
        // Compensate for space cadets who include the header in their
        // substitution of all '>'s into another citation character.
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
    // Id must be "<...@...>"
    if (*start != '<' || *end != '>' || (mid = std::strchr(start, '@')) == nullptr //
        || mid == start + 1 || mid + 1 == end)
    {
        return nullptr;
    }
    return end;
}

// The article has all it's data in place, so add it to the list of articles
// with the same subject.
//
void Article::cache_article()
{
    Article* next;
    Article* ap2;

    if (!(next = m_subj->m_articles) || m_date < next->m_date)
    {
        m_subj->m_articles = this;
    }
    else
    {
        while ((next = (ap2 = next)->m_subj_next) && next->m_date <= m_date)
        {
        }
        ap2->m_subj_next = this;
    }
    m_subj_next = next;
    m_flags |= AF_CACHED;

    if (!!(m_flags & AF_UNREAD) ^ g_sel_rereading)
    {
        if (m_subj->m_flags & g_sel_mask)
        {
            select_article(AUTO_KILL_NONE);
        }
        else
        {
            if (m_subj->m_flags & SF_WAS_SELECTED)
            {
                select_article(AUTO_KILL_NONE);
            }
            m_subj->m_flags |= SF_VISIT;
        }
    }

    if (g_join_subject_len != 0)
    {
        check_for_near_subj();
    }
}

void Article::check_poster()
{
    if (g_auto_select_postings && (m_flags & AF_EXISTS) && m_from)
    {
        {
            char* s = g_cmd_buf;
            char* u;
            char* h;
            std::strcpy(s,m_from);
            if ((h=std::strchr(s,'<')) != nullptr)   // grab the good part
            {
                s = h+1;
                if ((h=std::strchr(s,'>')) != nullptr)
                {
                    *h = '\0';
                }
            }
            else if ((h = std::strchr(s, '(')) != nullptr)
            {
                while (h-- != s && *h == ' ')
                {
                }
                h[1] = '\0';            // or strip the comment
            }
            if ((h = std::strchr(s, '%')) != nullptr || (h = std::strchr(s, '@')))
            {
                *h++ = '\0';
                u = s;
            }
            else if ((u = std::strrchr(s, '!')) != nullptr)
            {
                *u++ = '\0';
                h = s;
            }
            else
            {
                h = s;
                u = s;
            }
            if (!std::strcmp(u, g_login_name.c_str()))
            {
                if (in_string(h, g_host_name, false))
                {
                    switch (g_auto_select_postings)
                    {
                    case '.':
                        select_sub_thread(this,AUTO_SEL_FOL);
                        break;

                    case '+':
                        select_articles_thread(AUTO_SEL_THD);
                        break;

                    case 'p':
                        if (m_parent)
                        {
                            select_sub_thread(m_parent,AUTO_SEL_FOL);
                        }
                        else
                        {
                            select_sub_thread(this,AUTO_SEL_FOL);
                        }
                        break;
                    }
                }
                else
                {
#ifdef REPLYTO_POSTER_CHECKING
                    char* reply_buf = fetch_lines(article_num(ap),REPLY_LINE);
                    if (in_string(reply_buf, g_login_name.c_str(), true))
                    {
                        select_sub_thread(ap,AUTO_SEL_FOL);
                    }
                    std::free(reply_buf);
#endif
                }
            }
        }
    }
}

// Return a pointer to a cached header line for the indicated article.
// Truncated headers (e.g. from a .thread file) are optionally ignored.
//
char *Article::get_cached_line(HeaderLineType which_line, bool no_truncs)
{
    char* s;

    switch (which_line)
    {
    case SUBJ_LINE:
        if (!m_subj || (no_truncs && (m_subj->m_flags & SF_SUBJ_TRUNCATED)))
        {
            s = nullptr;
        }
        else
        {
            s = m_subj->m_str + ((m_flags & AF_HAS_RE) ? 0 : 4);
        }
        break;

    case FROM_LINE:
        s = m_from;
        break;

    case XREF_LINE:
        s = m_xrefs;
        break;

    case MSG_ID_LINE:
        s = m_msg_id;
        break;

    case LINES_LINE:
    {
        static char lines_buf[32];
        std::sprintf(lines_buf, "%ld", m_lines);
        s = lines_buf;
        break;
    }

    case BYTES_LINE:
    {
        static char bytes_buf[32];
        std::sprintf(bytes_buf, "%ld", m_bytes);
        s = bytes_buf;
        break;
    }

    default:
        s = nullptr;
        break;
    }
    return s;
}

void Article::clear_article()
{
    if (m_from)
    {
        std::free(m_from);
    }
    if (m_msg_id)
    {
        std::free(m_msg_id);
    }
    if (!empty(m_xrefs))
    {
        std::free(m_xrefs);
    }
}

// mark an article unread, keeping track of to_read[]
//
void Article::one_more()
{
    if (!(m_flags & AF_UNREAD))
    {
        ArticleNum art_num = m_num;
        check_first(art_num);
        m_flags |= AF_UNREAD;
        m_flags &= ~AF_DEL;
        g_newsgroup_ptr->m_to_read++;
        if (m_subj)
        {
            if (g_selected_only)
            {
                if (m_subj->m_flags & static_cast<SubjectFlags>(g_sel_mask))
                {
                    m_flags |= static_cast<ArticleFlags>(g_sel_mask);
                    g_selected_count++;
                }
            }
            else
            {
                m_subj->m_flags |= SF_VISIT;
            }
        }
    }
}

// mark an article read, keeping track of to_read[]
//
void Article::one_less()
{
    if (m_flags & AF_UNREAD)
    {
        m_flags &= ~AF_UNREAD;
        // Keep g_selected_count accurate
        if (m_flags & static_cast<ArticleFlags>(g_sel_mask))
        {
            g_selected_count--;
            m_flags &= ~static_cast<ArticleFlags>(g_sel_mask);
        }
        if (g_newsgroup_ptr->m_to_read > TR_NONE)
        {
            g_newsgroup_ptr->m_to_read--;
        }
    }
}

void Article::one_missing()
{
    g_missing_count += (m_flags & AF_UNREAD) != 0;
    one_less();
    m_flags = (m_flags & ~(AF_HAS_RE|AF_YANK_BACK|AF_EXISTS))
              | AF_CACHED|AF_THREADED;
}

// mark an article as unread, with possible xref chasing
//
void Article::unmark_as_read()
{
    one_more();
#ifdef MCHASE
    if (!empty(m_xrefs) && !(m_flags & AF_MCHASE))
    {
        m_flags |= AF_MCHASE;
        s_chase_count++;
    }
#endif
}

// temporarily mark article as read.  When newsgroup is exited, articles
// will be marked as unread.  Called via M command
//
void Article::delay_unmark()
{
    if (!(m_flags & AF_YANK_BACK))
    {
        m_flags |= AF_YANK_BACK;
        g_dm_count++;
    }
}

// Bump the param to the next article in depth-first order.
//
Article *Article::bump_article()
{
    if (m_child1)
    {
        return m_child1;
    }
    Article *ap{this};
    while (!ap->m_sibling)
    {
        ap = ap->m_parent;
        if (!ap)
        {
            return nullptr;
        }
    }
    return ap->m_sibling;
}

// Bump the param to the next REAL article.  Uses subject order in a
// non-threaded group; honors the g_breadth_first flag in a threaded one.
//
Article *Article::next_article()
{
    Article *ap{this};
try_again:
    if (!g_threaded_group)
    {
        ap = ap->m_subj_next;
        goto done;
    }
    if (g_breadth_first)
    {
        if (ap->m_sibling)
        {
            ap = ap->m_sibling;
            goto done;
        }
        if (ap->m_parent)
        {
            ap = ap->m_parent->m_child1;
        }
        else
        {
            ap = ap->m_subj->m_thread;
        }
    }
    do
    {
        if (ap->m_child1)
        {
            ap = ap->m_child1;
            goto done;
        }
        while (!ap->m_sibling)
        {
            if (!(ap = ap->m_parent))
            {
                return nullptr;
            }
        }
        ap = ap->m_sibling;
    } while (g_breadth_first);
done:
    if (ap && !(ap->m_flags & AF_EXISTS))
    {
        ap->one_less();
        goto try_again;
    }
    return ap;
}

// Bump the param to the previous REAL article.  Uses subject order in a
// non-threaded group.
//
Article *Article::prev_article()
{
    Article *ap{this};
try_again:
    Article *initial_ap = ap;
    if (!g_threaded_group)
    {
        ap = ap->m_subj->m_articles;
        if (ap == initial_ap)
        {
            ap = nullptr;
        }
        else
        {
            while (ap->m_subj_next != initial_ap)
            {
                ap = ap->m_subj_next;
            }
        }
        goto done;
    }
    ap = (ap->m_parent ? ap->m_parent->m_child1 : ap->m_subj->m_thread);
    if (ap == initial_ap)
    {
        ap = ap->m_parent;
        goto done;
    }
    while (ap->m_sibling != initial_ap)
    {
        ap = ap->m_sibling;
    }
    while (ap->m_child1)
    {
        ap = ap->m_child1;
        while (ap->m_sibling)
        {
            ap = ap->m_sibling;
        }
    }
done:
    if (ap && !(ap->m_flags & AF_EXISTS))
    {
        ap->one_less();
        goto try_again;
    }
    return ap;
}

// Select a single article.
//
void Article::select_article(AutoKillFlags auto_flags)
{
    const ArticleFlags desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    bool echo = (auto_flags & ALSO_ECHO) != 0;
    auto_flags &= AUTO_SEL_MASK;
    if ((m_flags & (AF_EXISTS | AF_UNREAD)) == desired_flags)
    {
        if (!(m_flags & static_cast<ArticleFlags>(g_sel_mask)))
        {
            g_selected_count++;
            if (g_verbose && echo && g_general_mode != GM_SELECTOR)
            {
                std::fputs("\tSelected", stdout);
            }
        }
        m_flags = (m_flags & ~AF_DEL) | static_cast<ArticleFlags>(g_sel_mask);
    }
    if (auto_flags)
    {
        change_auto_flags(auto_flags);
    }
    if (m_subj)
    {
        if (!(m_subj->m_flags & g_sel_mask))
        {
            g_selected_subj_cnt++;
        }
        m_subj->m_flags = (m_subj->m_flags&~SF_DEL) | static_cast<SubjectFlags>(g_sel_mask) | SF_VISIT;
    }
    g_selected_only = (g_selected_only || g_selected_count != 0);
}

// Select this article's subject.
//
void Article::select_articles_subject(AutoKillFlags auto_flags)
{
    if (m_subj && m_subj->m_articles)
    {
        m_subj->select_subject(auto_flags);
    }
    else
    {
        select_article(auto_flags);
    }
}

// Select this article's thread.
//
void Article::select_articles_thread(AutoKillFlags auto_flags)
{
    if (m_subj && m_subj->m_thread)
    {
        m_subj->m_thread->select_thread(auto_flags);
    }
    else
    {
        select_articles_subject(auto_flags);
    }
}

// Select all the articles in a thread.
//
void Article::select_thread(AutoKillFlags auto_flags)
{
    Subject *sp = m_subj;
    do
    {
        sp->select_subject(auto_flags);
        sp = sp->m_thread_link;
    } while (sp != m_subj);
}

// Deselect a single article.
//
void Article::deselect_article(AutoKillFlags auto_flags)
{
    const bool echo = (auto_flags & ALSO_ECHO) != 0;
    auto_flags &= AUTO_SEL_MASK;
    if (m_flags & static_cast<ArticleFlags>(g_sel_mask))
    {
        m_flags &= ~static_cast<ArticleFlags>(g_sel_mask);
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
        m_flags |= AF_DEL;
    }
}

// Deselect this article's subject.
//
void Article::deselect_articles_subject()
{
    if (m_subj && m_subj->m_articles )
    {
        m_subj->deselect_subject();
    }
    else
    {
        deselect_article(AUTO_KILL_NONE);
    }
}

// Deselect this article's thread.
//
void Article::deselect_articles_thread()
{
    if (m_subj && m_subj->m_thread)
    {
        m_subj->m_thread->deselect_thread();
    }
    else
    {
        deselect_articles_subject();
    }
}

// Deselect all the articles in a thread.
//
void Article::deselect_thread()
{
    Subject *sp = m_subj;
    do
    {
        sp->deselect_subject();
        sp = sp->m_thread_link;
    } while (sp != m_subj);
}

// Kill all unread articles attached to this article's subject.
//
void Article::kill_articles_subject(AutoKillFlags auto_flags)
{
    if (m_subj && m_subj->m_articles)
    {
        m_subj->kill_subject(auto_flags);
    }
    else
    {
        if (auto_flags & SET_TO_RETURN)
        {
            delay_unmark();
        }
        set_read();
        auto_flags &= AUTO_KILL_MASK;
        if (auto_flags)
        {
            change_auto_flags(auto_flags);
        }
    }
}

// Kill all unread articles attached to this article's thread.
//
void Article::kill_articles_thread(AutoKillFlags auto_flags)
{
    if (m_subj && m_subj->m_thread)
    {
        m_subj->m_thread->kill_thread(auto_flags);
    }
    else
    {
        kill_articles_subject(auto_flags);
    }
}

// Kill all unread articles attached to the given thread.
//
void Article::kill_thread(AutoKillFlags auto_flags)
{
    Subject *sp = m_subj;
    do
    {
        sp->kill_subject(auto_flags);
        sp = sp->m_thread_link;
    } while (sp != m_subj);
}

// Unkill all the articles attached to the given thread.
//
void Article::unkill_thread()
{
    Subject *sp = m_subj;
    do
    {
        sp->unkill_subject();
        sp = sp->m_thread_link;
    } while (sp != m_subj);
}

// Clear the auto flags in all unread articles attached to the given thread.
//
void Article::clear_thread()
{
    Subject *sp = m_subj;
    do
    {
        sp->clear_subject();
        sp = sp->m_thread_link;
    } while (sp != m_subj);
}

void Article::change_auto_flags(AutoKillFlags auto_flag)
{
    if (auto_flag > (m_auto_flags & (AUTO_KILL_MASK | AUTO_SEL_MASK)))
    {
        if (m_auto_flags & AUTO_OLD)
        {
            g_kf_change_thread_cnt++;
        }
        m_auto_flags = auto_flag;
        g_kf_state |= g_kfs_thread_change_set;
    }
}

void Article::clear_auto_flags()
{
    if (m_auto_flags)
    {
        if (m_auto_flags & AUTO_OLD)
        {
            g_kf_change_thread_cnt++;
        }
        m_auto_flags = AUTO_KILL_NONE;
        g_kf_state |= g_kfs_thread_change_set;
    }
}

void Article::perform_auto_flags(AutoKillFlags thread_flags, AutoKillFlags subj_flags, AutoKillFlags chain_flags)
{
    if (thread_flags & AUTO_SEL_THD)
    {
        if (g_sel_mode == SM_THREAD)
        {
            select_articles_thread(AUTO_SEL_THD);
        }
        else
        {
            select_articles_subject(AUTO_SEL_THD);
        }
    }
    else if (subj_flags & AUTO_SEL_SBJ)
    {
        select_articles_subject(AUTO_SEL_SBJ);
    }
    else if (chain_flags & AUTO_SEL_FOL)
    {
        select_sub_thread(this, AUTO_SEL_FOL);
    }
    else if (m_auto_flags & AUTO_SEL_1)
    {
        select_article(AUTO_SEL_1);
    }

    if (thread_flags & AUTO_KILL_THD)
    {
        if (g_sel_mode == SM_THREAD)
        {
            kill_articles_thread(AFFECT_ALL | AUTO_KILL_THD);
        }
        else
        {
            kill_articles_subject(AFFECT_ALL | AUTO_KILL_THD);
        }
    }
    else if (subj_flags & AUTO_KILL_SBJ)
    {
        kill_articles_subject(AFFECT_ALL | AUTO_KILL_SBJ);
    }
    else if (chain_flags & AUTO_KILL_FOL)
    {
        kill_sub_thread(this, AFFECT_ALL | AUTO_KILL_FOL);
    }
    else if (m_auto_flags & AUTO_KILL_1)
    {
        mark_as_read();
    }
}

bool Article::valid_article()
{
    char* msgid = m_msg_id;

    if (msgid)
    {
        fix_msg_id(msgid);
        HashDatum data = hash_fetch(g_msg_id_hash, msgid, std::strlen(msgid));
        if (data.dat_len)
        {
            safe_free0(data.dat_ptr);
            m_auto_flags = static_cast<AutoKillFlags>(data.dat_len) & (AUTO_SEL_MASK | AUTO_KILL_MASK);
            if ((data.dat_len & KF_AGE_MASK) == 0)
            {
                m_auto_flags |= AUTO_OLD;
            }
            else
            {
                g_kf_change_thread_cnt++;
            }
            data.dat_len = 0;
        }
        Article *fake_ap = (Article*)data.dat_ptr;
        if (fake_ap == nullptr)
        {
            data.dat_ptr = (char*)this;
            hash_store_last(data);
            s_fake_had_subj = nullptr;
            return true;
        }
        if (fake_ap == this)
        {
            s_fake_had_subj = nullptr;
            return true;
        }

        // Whenever we replace a fake art with a real one, it's a lot of work
        // cleaning up the references.  Fortunately, this is not often.
        if (fake_ap && (fake_ap->m_flags & AF_TMP_MEM))
        {
            m_parent = fake_ap->m_parent;
            m_child1 = fake_ap->m_child1;
            m_sibling = fake_ap->m_sibling;
            s_fake_had_subj = fake_ap->m_subj;
            if (fake_ap->m_auto_flags)
            {
                m_auto_flags |= fake_ap->m_auto_flags;
                g_kf_state |= g_kfs_thread_change_set;
            }
            if (g_curr_artp == fake_ap)
            {
                g_curr_artp = this;
                g_curr_art = m_num;
            }
            if (g_recent_artp == fake_ap)
            {
                g_recent_artp = this;
                g_recent_art = m_num;
            }
            Article *ap = m_parent;
            if (ap != nullptr)
            {
                if (ap->m_child1 == fake_ap)
                {
                    ap->m_child1 = this;
                }
                else
                {
                    ap = ap->m_child1;
                    // This sibling-search code is duplicated below
                    while (ap->m_sibling)
                    {
                        if (ap->m_sibling == fake_ap)
                        {
                            ap->m_sibling = this;
                            break;
                        }
                        ap = ap->m_sibling;
                    }
                    // End of sibling-search code
                }
            }
            else if (s_fake_had_subj)
            {
                Subject* sp = s_fake_had_subj;
                ap = sp->m_thread;
                if (ap == fake_ap)
                {
                    do
                    {
                        sp->m_thread = this;
                        sp = sp->m_thread_link;
                    } while (sp != s_fake_had_subj);
                }
                else
                {
                    // This sibling-search code is duplicated above
                    while (ap->m_sibling)
                    {
                        if (ap->m_sibling == fake_ap)
                        {
                            ap->m_sibling = this;
                            break;
                        }
                        ap = ap->m_sibling;
                    }
                    // End of sibling-search code
                }
            }
            for (ap = m_child1; ap; ap = ap->m_sibling)
            {
                ap->m_parent = this;
            }
            fake_ap->clear_article();
            std::free(fake_ap);
            data.dat_ptr = (char*)this;
            hash_store_last(data);
            return true;
        }
    }
    // Forget about the duplicate message-id or bogus article.
    uncache_article(true);
    return false;
}

// Take all the data we've accumulated about the article and shove it into
// the article tree at the best place we can deduce.
//
void Article::thread_article(char *references)
{
    Article* ap;
    Article* prev;
    char* cp;
    char* end;
    AutoKillFlags chain_autofl =
        m_auto_flags | (m_subj->m_articles ? m_subj->m_articles->m_auto_flags : AUTO_KILL_NONE);
    AutoKillFlags subj_autofl = AUTO_KILL_NONE;
    const bool rethreading = (m_flags & AF_THREADED) != 0;

    // We're definitely not a fake anymore
    m_flags = (m_flags & ~AF_FAKE) | AF_THREADED;

    // If the article was already part of an existing thread, unlink it
    // to try to put it in the best possible spot.
    if (s_fake_had_subj)
    {
        if (s_fake_had_subj->m_thread != m_subj->m_thread)
        {
            merge_threads(s_fake_had_subj, m_subj);
        }
        // Check for a real or shared-fake parent
        ap = m_parent;
        while (ap && (ap->m_flags & AF_FAKE) && !ap->m_child1->m_sibling)
        {
            prev = ap;
            ap = ap->m_parent;
        }
        Article *stopper = ap;
        unlink_child(this);
        // We'll assume that this article has as good or better references
        // than the child that faked us initially.  Free the fake reference-
        // chain and process our references as usual.
        for (ap = m_parent; ap != stopper; ap = prev)
        {
            unlink_child(ap);
            prev = ap->m_parent;
            ap->m_date = 0;
            ap->m_subj = nullptr;
            ap->m_parent = nullptr;
            // don't free it until group exit since we probably re-use it
        }
        m_parent = nullptr;              // neaten up
        m_sibling = nullptr;
    }

    // If we have references, process them from the right end one at a time
    // until we either run into somebody, or we run out of references.
    if (references && *references)
    {
        prev = this;
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
            // Quit parsing references if this one is garbage.
            if (!(end = valid_message_id(cp, end)))
            {
                break;
            }
            // Dump all domains that end in '.', such as "..." & "1@DEL."
            if (end[-1] == '.')
            {
                break;
            }
            ap = get_article(cp);
            *cp = '\0';
            chain_autofl |= ap->m_auto_flags;
            if (ap->m_subj == m_subj)
            {
                subj_autofl |= ap->m_auto_flags;
            }

            // Check for duplicates on the reference line.  Brand-new data has
            // no date.  Data we just allocated earlier on this line has a
            // date but no subj.  Special-case the article itself, since it
            // does have a subj.
            if ((ap->m_date && !ap->m_subj) || ap == this)
            {
                ap = prev;
                if (ap == this)
                {
                    ap = nullptr;
                }
                goto next;
            }

            // When we're doing late processing of In-Reply-To: lines, we may
            // have to move an article from an old position.
            if (rethreading && prev->m_subj)
            {
                unlink_child(prev);
            }
            prev->m_parent = ap;
            prev->link_child();
            if (ap->m_subj)
            {
                break;
            }

            ap->m_date = m_date;
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

        // Check if we ran into anybody that was already linked.  If so, we
        // just use their thread.
        if (ap->m_subj)
        {
            // See if this article spans the gap between what we thought
            // were two different threads.
            if (m_subj->m_thread != ap->m_subj->m_thread)
            {
                merge_threads(ap->m_subj, m_subj);
            }
        }
        else
        {
            // We didn't find anybody we knew, so either create a new thread
            // or use the article's thread if it was previously faked.
            ap->m_subj = m_subj;
            ap->link_child();
        }
        // Set the subj of faked articles we created as references.
        for (ap = m_parent; ap && !ap->m_subj; ap = ap->m_parent)
        {
            ap->m_subj = m_subj;
        }

        // Make sure we didn't circularly link to a child article(!), by
        // ensuring that we run off the top before we run into ourself.
        while (ap && ap->m_parent != this)
        {
            ap = ap->m_parent;
        }
        if (ap)
        {
            // Ugh.  Someone's tweaked reference line with an incorrect
            // article-order arrived first, and one of our children is
            // really one of our ancestors. Cut off the bogus child branch
            // right where we are and link it to the thread.
            unlink_child(ap);
            ap->m_parent = nullptr;
            ap->link_child();
        }
    }
    else
    {
no_references:
        // The article has no references.  Either turn it into a new thread
        // or re-attach the fleshed-out article to its old thread.  Don't
        // touch it at all unless this is the first attempt at threading it.
        if (!rethreading)
        {
            link_child();
        }
    }
    if (!(m_flags & AF_CACHED))
    {
        cache_article();
    }
    AutoKillFlags thread_autofl = chain_autofl;
    if (g_sel_mode == SM_THREAD)
    {
        Subject* sp = m_subj->m_thread_link;
        while (sp != m_subj)
        {
            if (sp->m_articles)
            {
                thread_autofl |= sp->m_articles->m_auto_flags;
            }
            sp = sp->m_thread_link;
        }
    }
    subj_autofl |= m_subj->m_articles->m_auto_flags;

    perform_auto_flags(thread_autofl, subj_autofl, chain_autofl);
}

// Link an article to its parent article.  If its parent pointer is zero,
// link it to its thread.  Sorts siblings by date.
void Article::link_child()
{
    Article* ap;

    if (!(ap = m_parent))
    {
        Subject* sp = m_subj;
        ap = sp->m_thread;
        if (!ap || m_date < ap->m_date)
        {
            do
            {
                sp->m_thread = this;
                sp = sp->m_thread_link;
            } while (sp != m_subj);
            m_sibling = ap;
        }
        else
        {
            goto sibling_search;
        }
    }
    else
    {
        ap = ap->m_child1;
        if (!ap || m_date < ap->m_date)
        {
            m_sibling = ap;
            m_parent->m_child1 = this;
        }
        else
        {
sibling_search:
            while (ap->m_sibling && ap->m_sibling->m_date <= m_date)
            {
                ap = ap->m_sibling;
            }
            m_sibling = ap->m_sibling;
            ap->m_sibling = this;
        }
    }
}

// Fit the date in <max> chars.
char *Article::compress_date(int size) const
{
    char* t;

    std::strncpy(t = g_cmd_buf, std::ctime(&m_date), size);
    char *s = std::strchr(t, '\n');
    if (s != nullptr)
    {
        *s = '\0';
    }
    t[size] = '\0';
    return t;
}
