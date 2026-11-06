/* sathread.cpp
 *
 */
// This file Copyright 1992 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/sathread.h>

#include <config/common.h>
#include <trn/cache.h>
#include <trn/sadesc.h> // sa_desc_subject()
#include <trn/samisc.h>
#include <trn/scan.h>
#include <trn/sorder.h>

#include <string>
#include <unordered_map>

ArticleScanThreads g_sa_threads;

void ArticleScanThreads::init_threads()
{
    num_threads = 0;
    thread_nums.clear();
}

long ArticleScanThreads::subj_thread(long e)
{
    return g_sa_ents[e].subj_thread_num ? g_sa_ents[e].subj_thread_num : get_subj_thread(e);
}

// called only if the macro didn't find a value
// XXX: dependent on hash feature that data.dat_len is not used in
// the default comparison function, so it can be used for a number.
// later: write a custom comparison function.
//
//long e;                       // entry number
long ArticleScanThreads::get_subj_thread(long e)
{
    bool old_untrim = g_untrim_cache;
    g_untrim_cache = true;
    std::string subject = sa_desc_subject(e);
    g_untrim_cache = old_untrim;

    if (subject.empty())
    {
        return -2;
    }
    if (subject.size() >= 2 && subject[0] == '>' && subject[1] == ' ')
    {
        subject.erase(0, 2);
    }

    const auto existing = thread_nums.find(subject);
    if (existing != thread_nums.end())
    {
        return existing->second;
    }
    num_threads++;
    thread_nums.emplace(subject, num_threads);
    g_sa_ents[e].subj_thread_num = num_threads;
    return num_threads;
}

int sa_subj_thread_count(long a)
{
    int  count = 1;
    long b = a;

    while ((b = sa_subj_thread_next(b)) != 0)
    {
        if (sa_basic_elig(b))
        {
            count++;
        }
    }
    return count;
}

// returns basic_elig previous subject thread
long sa_subj_thread_prev(long a)
{
    int j;

    int i = g_sa_threads.subj_thread(a);
    while ((a = g_art_scan.prev(a)) != 0)
    {
        if (!sa_basic_elig(a))
        {
            continue;
        }
        if (!(j = g_sa_ents[a].subj_thread_num))
        {
            j = g_sa_threads.subj_thread(a);
        }
        if (i == j)
        {
            return a;
        }
    }
    return 0L;
}

long sa_subj_thread_next(long a)
{
    int j;

    int i = g_sa_threads.subj_thread(a);
    while ((a = g_art_scan.next(a)) != 0)
    {
        if (!sa_basic_elig(a))
        {
            continue;
        }
        if (!(j = g_sa_ents[a].subj_thread_num))
        {
            j = g_sa_threads.subj_thread(a);
        }
        if (i == j)
        {
            return a;
        }
    }
    return 0L;
}
