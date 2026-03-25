// This software is copyrighted as detailed in the LICENSE file.
#include <trn/Subject.h>

#include <config/common.h>
#include <trn/Article.h>
#include <trn/artstate.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>

Article *Subject::first_art()
{
    Article * ap = (g_threaded_group? m_thread : m_articles);
    if (ap && !(ap->m_flags & AF_EXISTS))
    {
        ap->one_less();
        ap = ap->next_article();
    }
    return ap;
}

Article *Subject::last_art()
{
    Article * ap;

    if (!g_threaded_group)
    {
        ap = m_articles;
        while (ap->m_subj_next)
        {
            ap = ap->m_subj_next;
        }
        return ap;
    }

    ap = m_thread;
    if (ap)
    {
        while (true)
        {
            if (ap->m_sibling)
            {
                ap = ap->m_sibling;
            }
            else if (ap->m_child1)
            {
                ap = ap->m_child1;
            }
            else
            {
                break;
            }
        }
        if (!(ap->m_flags & AF_EXISTS))
        {
            ap->one_less();
            ap = ap->prev_article();
        }
    }
    return ap;
}

// Select all the articles in a subject.
//
void Subject::select_subject(AutoKillFlags auto_flags)
{
    int desired_flags = (g_sel_rereading? AF_EXISTS : (AF_EXISTS|AF_UNREAD));
    int old_count = g_selected_count;

    auto_flags &= AUTO_SEL_MASK;
    for (Article *ap = m_articles; ap; ap = ap->m_subj_next)
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
    if (g_selected_count > old_count)
    {
        if (!(m_flags & g_sel_mask))
        {
            g_selected_subj_cnt++;
        }
        m_flags = (m_flags & ~SF_DEL)
                    | static_cast<SubjectFlags>(g_sel_mask) | SF_VISIT | SF_WAS_SELECTED;
        g_selected_only = true;
    }
    else
    {
        m_flags |= SF_WAS_SELECTED;
    }
}

// Deselect all the articles in a subject.
//
void Subject::deselect_subject()
{
    for (Article *ap = m_articles; ap; ap = ap->m_subj_next)
    {
        if (ap->m_flags & g_sel_mask)
        {
            ap->m_flags &= ~static_cast<ArticleFlags>(g_sel_mask);
            if (!g_selected_count--)
            {
                g_selected_count = 0;
            }
        }
    }
    if (m_flags & g_sel_mask)
    {
        m_flags &= ~static_cast<SubjectFlags>(g_sel_mask);
        g_selected_subj_cnt--;
    }
    m_flags &= ~(SF_VISIT | SF_WAS_SELECTED);
    if (g_sel_rereading)
    {
        m_flags |= SF_DEL;
    }
    else
    {
        m_flags &= ~SF_DEL;
    }
}

// Kill all unread articles attached to the given subject.
//
void Subject::kill_subject(AutoKillFlags auto_flags)
{
    int killmask = (auto_flags & AFFECT_ALL)? 0 : g_sel_mask;
    const bool toreturn = (auto_flags & SET_TO_RETURN) != 0;

    auto_flags &= AUTO_KILL_MASK;
    for (Article *ap = m_articles; ap; ap = ap->m_subj_next)
    {
        if (toreturn)
        {
            ap->delay_unmark();
        }
        if ((ap->m_flags & (AF_UNREAD|killmask)) == AF_UNREAD)
        {
            ap->set_read();
        }
        if (auto_flags)
        {
            ap->change_auto_flags(auto_flags);
        }
    }
    m_flags &= ~(SF_VISIT | SF_WAS_SELECTED);
}

// Unkill all the articles attached to the given subject.
//
void Subject::unkill_subject()
{
    int save_sel_count = g_selected_count;

    for (Article *ap = m_articles; ap; ap = ap->m_subj_next)
    {
        if (g_sel_rereading)
        {
            if (all_bits(ap->m_flags, AF_DEL_SEL | AF_EXISTS))
            {
                if (!(ap->m_flags & AF_UNREAD))
                {
                    g_newsgroup_ptr->m_to_read++;
                }
                if (g_selected_only && !(ap->m_flags & AF_SEL))
                {
                    g_selected_count++;
                }
                ap->m_flags = (ap->m_flags & ~AF_DEL_SEL) | AF_SEL|AF_UNREAD;
            }
            else
            {
                ap->m_flags &= ~(AF_DEL | AF_DEL_SEL);
            }
        }
        else
        {
            if ((ap->m_flags & (AF_EXISTS|AF_UNREAD)) == AF_EXISTS)
            {
                ap->one_more();
            }
            if (g_selected_only && (ap->m_flags & (AF_SEL | AF_UNREAD)) == AF_UNREAD)
            {
                ap->m_flags = (ap->m_flags & ~AF_DEL) | AF_SEL;
                g_selected_count++;
            }
        }
    }
    if (g_selected_count != save_sel_count //
        && g_selected_only && !(m_flags & SF_SEL))
    {
        m_flags |= SF_SEL | SF_VISIT | SF_WAS_SELECTED;
        g_selected_subj_cnt++;
    }
    m_flags &= ~(SF_DEL|SF_DEL_SEL);
}

// Clear the auto flags in all unread articles attached to the given subject.
//
void Subject::clear_subject()
{
    for (Article *ap = m_articles; ap; ap = ap->m_subj_next)
    {
        ap->clear_auto_flags();
    }
}

Article *Subject::subj_article()
{
    int art_mask = (g_selected_only? (AF_SEL|AF_UNREAD) : AF_UNREAD);
    bool TG_save = g_threaded_group;

    g_threaded_group = (g_sel_mode == SM_THREAD);
    Article *ap = first_art();
    while (ap && (ap->m_flags & art_mask) != art_mask)
    {
        ap = ap->next_article();
    }
    if (!ap)
    {
        g_reread = true;
        ap = first_art();
        if (g_selected_only)
        {
            while (ap && !(ap->m_flags & AF_SEL))
            {
                ap = ap->next_article();
            }
            if (!ap)
            {
                ap = first_art();
            }
        }
    }
    g_threaded_group = TG_save;
    return ap;
}
