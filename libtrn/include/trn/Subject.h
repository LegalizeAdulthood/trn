/* trn/Subject.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_SUBJECT_H
#define TRN_SUBJECT_H

#include <trn/enum-flags.h>
#include <trn/kfile.h>

#include <cstdint>
#include <ctime>

enum SubjectFlags : std::uint16_t
{
    SF_NONE = 0x0000,
    SF_SEL = 0x0001,
    SF_DEL = 0x0002,
    SF_DEL_SEL = 0x0004,
    SF_OLD_VISIT = 0x0008,
    SF_INCLUDED = 0x0010,
    SF_VISIT = 0x0200,
    SF_WAS_SELECTED = 0x0400,
    SF_SUBJ_TRUNCATED = 0x1000,
    SF_ISO_SUBJECT = 0x2000
};
DECLARE_FLAGS_ENUM(SubjectFlags, std::uint16_t);

struct Article;

// Subjects get their own structure
struct Subject
{
    Article *first_art();
    Article *last_art();
    void select_subject(AutoKillFlags auto_flags);
    void deselect_subject();
    void     kill_subject(AutoKillFlags auto_flags);
    void     unkill_subject();
    void     clear_subject();
    Article *subj_article();

    Subject     *m_next;
    Subject     *m_prev;
    Article     *m_articles;
    Article     *m_thread;
    Subject     *m_thread_link;
    char        *m_str;
    std::time_t  m_date;
    SubjectFlags m_flags;
    short        m_misc; // used for temporary totals and subject numbers
};

#endif
