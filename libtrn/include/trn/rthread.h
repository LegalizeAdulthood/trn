/* trn/rthread.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RTHREAD_H
#define TRN_RTHREAD_H

#include "trn/kfile.h"

struct Article;
struct HashTable;
struct Subject;

extern ArticleNum    g_obj_count;
extern int        g_subject_count;
extern bool       g_output_chase_phrase;
extern HashTable *g_msgid_hash;
extern bool       g_thread_always; /* -a */
extern bool       g_breadth_first; /* -b */

/* Values to pass to count_subjects() */
enum CountSubjectMode
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
Article *first_art(Subject *sp);
Article *last_art(Subject *sp);
void     inc_art(bool sel_flag, bool rereading);
void     dec_art(bool sel_flag, bool rereading);
Article *bump_art(Article *ap);
Article *next_art(Article *ap);
Article *prev_art(Article *ap);
bool     next_art_with_subj();
bool     prev_art_with_subj();
Subject *next_subj(Subject *sp, int subj_mask);
Subject *prev_subj(Subject *sp, int subj_mask);
void     select_article(Article *ap, AutoKillFlags auto_flags);
void     select_arts_subject(Article *ap, AutoKillFlags auto_flags);
void     select_subject(Subject *subj, AutoKillFlags auto_flags);
void     select_arts_thread(Article *ap, AutoKillFlags auto_flags);
void     select_thread(Article *thread, AutoKillFlags auto_flags);
void     select_subthread(Article *ap, AutoKillFlags auto_flags);
void     deselect_article(Article *ap, AutoKillFlags auto_flags);
void     deselect_arts_subject(Article *ap);
void     deselect_subject(Subject *subj);
void     deselect_arts_thread(Article *ap);
void     deselect_thread(Article *thread);
void     deselect_all();
void     kill_arts_subject(Article *ap, AutoKillFlags auto_flags);
void     kill_subject(Subject *subj, AutoKillFlags auto_flags);
void     kill_arts_thread(Article *ap, AutoKillFlags auto_flags);
void     kill_thread(Article *thread, AutoKillFlags auto_flags);
void     kill_subthread(Article *ap, AutoKillFlags auto_flags);
void     unkill_subject(Subject *subj);
void     unkill_thread(Article *thread);
void     unkill_subthread(Article *ap);
void     clear_subject(Subject *subj);
void     clear_thread(Article *thread);
void     clear_subthread(Article *ap);
Article *subj_art(Subject *sp);
void     visit_next_thread();
void     visit_prev_thread();
bool     find_parent(bool keep_going);
bool     find_leaf(bool keep_going);
bool     find_next_sib();
bool     find_prev_sib();
void     count_subjects(CountSubjectMode cmode);
void     sort_subjects();
void     sort_articles();

#endif
