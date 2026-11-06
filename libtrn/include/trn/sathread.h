/* trn/sathread.h
 *
 */
// This file Copyright 1992,1995 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SATHREAD_H
#define TRN_SATHREAD_H

#include <trn/scanart.h>

#include <string>
#include <unordered_map>

struct ArticleScanThreads
{
    void init_threads();
    long subj_thread(long e);

    // TODO: private
    long get_subj_thread(long e);

    long                                  num_threads{};
    std::unordered_map<std::string, long> thread_nums;
};

extern ArticleScanThreads g_sa_threads;

int sa_subj_thread_count(long a);
long sa_subj_thread_prev(long a);
long sa_subj_thread_next(long a);

#endif
