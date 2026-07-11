/* trn/sathread.h
 *
 */
// This file Copyright 1992,1995 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SATHREAD_H
#define TRN_SATHREAD_H

#include <trn/scanart.h>

void sa_init_threads();
int sa_subj_thread_count(long a);
long sa_subj_thread_prev(long a);
long sa_subj_thread_next(long a);
long sa_subj_thread(long e);

#endif
