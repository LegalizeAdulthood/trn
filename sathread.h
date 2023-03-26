/* This file Copyright 1992,1995 by Clifford A. Adams */
/* sathread.h
 *
 */
#ifndef TRN_SATHREAD_H
#define TRN_SATHREAD_H

#include "scanart.h"

void sa_init_threads();
long sa_get_subj_thread(long e);
int sa_subj_thread_count(long a);
long sa_subj_thread_prev(long a);
long sa_subj_thread_next(long a);
inline long sa_subj_thread(long e)
{
    return g_sa_ents[e].subj_thread_num ? g_sa_ents[e].subj_thread_num : sa_get_subj_thread(e);
}

#endif
