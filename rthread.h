/* rthread.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RTHREAD_H
#define TRN_RTHREAD_H
#include "kfile.h"

struct ARTICLE;
struct HASHTABLE;
struct SUBJECT;

extern ART_NUM    g_obj_count;
extern int        g_subject_count;
extern bool       g_output_chase_phrase;
extern HASHTABLE *g_msgid_hash;
extern bool       g_thread_always; /* -a */

/* Values to pass to count_subjects() */
enum cs_mode
{
    CS_RETAIN = 0,
    CS_NORM,
    CS_RESELECT,
    CS_UNSELECT,
    CS_UNSEL_STORE
};

void     thread_init();
void     thread_open();
void     thread_grow();
void     thread_close();
void     top_article();
ARTICLE *first_art(SUBJECT *sp);
ARTICLE *last_art(SUBJECT *sp);
void     inc_art(bool sel_flag, bool rereading);
void     dec_art(bool sel_flag, bool rereading);
ARTICLE *bump_art(ARTICLE *ap);
ARTICLE *next_art(ARTICLE *ap);
ARTICLE *prev_art(ARTICLE *ap);
bool     next_art_with_subj();
bool     prev_art_with_subj();
SUBJECT *next_subj(SUBJECT *sp, int subj_mask);
SUBJECT *prev_subj(SUBJECT *sp, int subj_mask);
void     select_article(ARTICLE *ap, autokill_flags auto_flags);
void     select_arts_subject(ARTICLE *ap, autokill_flags auto_flags);
void     select_subject(SUBJECT *subj, autokill_flags auto_flags);
void     select_arts_thread(ARTICLE *ap, autokill_flags auto_flags);
void     select_thread(ARTICLE *thread, autokill_flags auto_flags);
void     select_subthread(ARTICLE *ap, autokill_flags auto_flags);
void     deselect_article(ARTICLE *ap, autokill_flags auto_flags);
void     deselect_arts_subject(ARTICLE *ap);
void     deselect_subject(SUBJECT *subj);
void     deselect_arts_thread(ARTICLE *ap);
void     deselect_thread(ARTICLE *thread);
void     deselect_all();
void     kill_arts_subject(ARTICLE *ap, autokill_flags auto_flags);
void     kill_subject(SUBJECT *subj, autokill_flags auto_flags);
void     kill_arts_thread(ARTICLE *ap, autokill_flags auto_flags);
void     kill_thread(ARTICLE *thread, autokill_flags auto_flags);
void     kill_subthread(ARTICLE *ap, autokill_flags auto_flags);
void     unkill_subject(SUBJECT *subj);
void     unkill_thread(ARTICLE *thread);
void     unkill_subthread(ARTICLE *ap);
void     clear_subject(SUBJECT *subj);
void     clear_thread(ARTICLE *thread);
void     clear_subthread(ARTICLE *ap);
ARTICLE *subj_art(SUBJECT *sp);
void     visit_next_thread();
void     visit_prev_thread();
bool     find_parent(bool keep_going);
bool     find_leaf(bool keep_going);
bool     find_next_sib();
bool     find_prev_sib();
void     count_subjects(cs_mode cmode);
void     sort_subjects();
void     sort_articles();

#endif
