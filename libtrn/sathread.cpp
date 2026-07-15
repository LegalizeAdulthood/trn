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
#include <trn/sorder.h>

#include <string>
#include <unordered_map>

static long                                  s_sa_num_threads{};
static std::unordered_map<std::string, long> s_sa_thread_nums;

static long sa_get_subj_thread(long e);

void sa_init_threads()
{
    s_sa_num_threads = 0;
    s_sa_thread_nums.clear();
}

long sa_subj_thread(long e)
{
    return g_sa_ents[e].subj_thread_num ? g_sa_ents[e].subj_thread_num : sa_get_subj_thread(e);
}

// called only if the macro didn't find a value
// XXX: dependent on hash feature that data.dat_len is not used in
// the default comparison function, so it can be used for a number.
// later: write a custom comparison function.
//
//long e;                       // entry number
static long sa_get_subj_thread(long e)
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

    const std::unordered_map<std::string, long>::const_iterator existing = s_sa_thread_nums.find(subject);
    if (existing != s_sa_thread_nums.end())
    {
        return existing->second;
    }
    s_sa_num_threads++;
    s_sa_thread_nums.emplace(subject, s_sa_num_threads);
    g_sa_ents[e].subj_thread_num = s_sa_num_threads;
    return s_sa_num_threads;
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

    int i = sa_subj_thread(a);
    while ((a = s_prev(a)) != 0)
    {
        if (!sa_basic_elig(a))
        {
            continue;
        }
        if (!(j = g_sa_ents[a].subj_thread_num))
        {
            j = sa_subj_thread(a);
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

    int i = sa_subj_thread(a);
    while ((a = s_next(a)) != 0)
    {
        if (!sa_basic_elig(a))
        {
            continue;
        }
        if (!(j = g_sa_ents[a].subj_thread_num))
        {
            j = sa_subj_thread(a);
        }
        if (i == j)
        {
            return a;
        }
    }
    return 0L;
}
