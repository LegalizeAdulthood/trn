/* rthread.h
*/
/* This software is copyrighted as detailed in the LICENSE file. */

EXT ART_NUM obj_count INIT(0);
EXT int subject_count INIT(0);
EXT bool output_chase_phrase;

EXT HASHTABLE* msgid_hash INIT(NULL);

/* Values to pass to count_subjects() */
#define CS_RETAIN      0
#define CS_NORM        1
#define CS_RESELECT    2
#define CS_UNSELECT    3
#define CS_UNSEL_STORE 4

void thread_init(void);
void thread_open(void);
void thread_grow(void);
void thread_close(void);
void top_article(void);
ARTICLE *first_art(SUBJECT *);
ARTICLE *last_art(SUBJECT *);
void inc_art(bool sel_flag, bool rereading);
void dec_art(bool sel_flag, bool rereading);
ARTICLE *bump_art(ARTICLE *);
ARTICLE *next_art(ARTICLE *);
ARTICLE *prev_art(ARTICLE *);
bool next_art_with_subj(void);
bool prev_art_with_subj(void);
SUBJECT *next_subj(SUBJECT *, int);
SUBJECT *prev_subj(SUBJECT *, int);
void select_article(ARTICLE *, int);
void select_arts_subject(ARTICLE *, int);
void select_subject(SUBJECT *, int);
void select_arts_thread(ARTICLE *, int);
void select_thread(ARTICLE *, int);
void select_subthread(ARTICLE *, int);
void deselect_article(ARTICLE *, int);
void deselect_arts_subject(ARTICLE *);
void deselect_subject(SUBJECT *);
void deselect_arts_thread(ARTICLE *);
void deselect_thread(ARTICLE *);
void deselect_all(void);
void kill_arts_subject(ARTICLE *, int);
void kill_subject(SUBJECT *, int);
void kill_arts_thread(ARTICLE *, int);
void kill_thread(ARTICLE *, int);
void kill_subthread(ARTICLE *, int);
void unkill_subject(SUBJECT *);
void unkill_thread(ARTICLE *);
void unkill_subthread(ARTICLE *);
void clear_subject(SUBJECT *);
void clear_thread(ARTICLE *);
void clear_subthread(ARTICLE *);
ARTICLE *subj_art(SUBJECT *);
void visit_next_thread(void);
void visit_prev_thread(void);
bool find_parent(bool keep_going);
bool find_leaf(bool keep_going);
bool find_next_sib(void);
bool find_prev_sib(void);
void count_subjects(int);
void sort_subjects(void);
void sort_articles(void);
