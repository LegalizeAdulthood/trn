/* This file Copyright 1992,1995 by Clifford A. Adams */
/* sathread.h
 *
 */

/* this define will save a *lot* of function calls. */
#define sa_subj_thread(e) \
 (sa_ents[e].subj_thread_num? sa_ents[e].subj_thread_num : \
 sa_get_subj_thread(e))

void sa_init_threads(void);
long sa_get_subj_thread(long);
int sa_subj_thread_count(long);
long sa_subj_thread_prev(long);
long sa_subj_thread_next(long);
