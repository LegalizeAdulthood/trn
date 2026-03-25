/* trn/rthread.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RTHREAD_H
#define TRN_RTHREAD_H

#include "trn/kfile.h"

struct Article;
struct HashTable;
struct Subject;

extern ArticleNum g_obj_count;
extern int        g_subject_count;
extern bool       g_output_chase_phrase;
extern HashTable *g_msg_id_hash;
extern bool       g_thread_always; // -a
extern bool       g_breadth_first; // -b

// Values to pass to count_subjects()
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
void     inc_article(bool sel_flag, bool rereading);
void     dec_article(bool sel_flag, bool rereading);
bool     next_article_with_subj();
bool     prev_article_with_subj();
Subject *next_subject(Subject *sp, int subj_mask);
Subject *prev_subject(Subject *sp, int subj_mask);
void     select_sub_thread(Article *ap, AutoKillFlags auto_flags);
void     deselect_all();
void     kill_sub_thread(Article *ap, AutoKillFlags auto_flags);
void     unkill_sub_thread(Article *ap);
void     clear_sub_thread(Article *ap);
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
