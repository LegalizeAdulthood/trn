/* sorder.cpp
 *
 * scan ordering
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson

#include <trn/sorder.h>

#include <config/common.h>
#include <trn/samisc.h>
#include <trn/scan.h>
#include <trn/size_cast.h>
#include <trn/smisc.h>

ScanContext &ArticleScanSession::current_context()
{
    return contexts[cur_context];
}

std::vector<long> &ArticleScanSession::ent_sort()
{
    return current_context().ent_sort;
}

std::vector<long> &ArticleScanSession::ent_index()
{
    return current_context().ent_index;
}

void ScanContext::save_order_bounds()
{
    ent_sort_max = g_art_scan.ent_sort_max;
    ent_sorted_max = g_art_scan.ent_sorted_max;
    ent_index_max = g_art_scan.ent_index_max;
}

#ifdef UNDEF
// pointers to the two entries to be compared
int s_compare(long *a, long *b)
{
    switch (g_art_scan.cur_type)
    {
    case S_ART:
        return sa_compare(*a,*b);

    default:
        return *a - *b;
    }
}
#endif

// the two entry numbers to be compared
int s_compare(long a, long b)
{
    switch (g_art_scan.cur_type)
    {
    case S_ART:
        return sa_compare(a,b);

    default:
        return a - b;
    }
}

// sort offset--used so that the 1-offset algorithm is clear even
// though the array is 0-offset.
//
#define SOFF(a) ((a)-1)

// Uses a heapsort algorithm with the heap readjustment inlined.
void ScanContext::sort_basic()
{
    int t1;
    int j;

    int n = g_art_scan.ent_sort_max + 1;
    if (n < 1)
    {
        return;         // nothing to sort
    }

    for (int i = n / 2; i >= 1; i--)
    {
        // begin heap readjust
        t1 = ent_sort[SOFF(i)];
        j = 2*i;
        while (j <= n)
        {
            if (j < n //
                && s_compare(ent_sort[SOFF(j)], ent_sort[SOFF(j + 1)]) < 0)
            {
                j++;
            }
            if (s_compare(t1,ent_sort[SOFF(j)]) > 0)
            {
                break;          // out of while loop
            }
            ent_sort[SOFF(j/2)] = ent_sort[SOFF(j)];
            j = j*2;
        } // while
        ent_sort[SOFF(j/2)] = t1;
        // end heap readjust
    } // for

    for (int i = n - 1; i >= 1; i--)
    {
        t1 = ent_sort[SOFF(i+1)];
        ent_sort[SOFF(i+1)] = ent_sort[SOFF(1)];
        ent_sort[SOFF(1)] = t1;
        // begin heap readjust
        j = 2;
        while (j <= i)
        {
            if (j < i //
                && s_compare(ent_sort[SOFF(j)], ent_sort[SOFF(j + 1)]) < 0)
            {
                j++;
            }
            if (s_compare(t1,ent_sort[SOFF(j)]) > 0)
            {
                break;  // out of while
            }
            ent_sort[SOFF(j/2)] = ent_sort[SOFF(j)];
            j = j*2;
        } // while
        ent_sort[SOFF(j/2)] = t1;
        // end heap readjust
    } // for
    // end of heapsort
}

void ScanContext::sort()
{
#ifdef UNDEF
    std::qsort((void *) ent_sort.data(), (g_art_scan.ent_sort_max) + 1, sizeof(long), s_compare);
#endif
    sort_basic();
    g_art_scan.ent_sorted_max = g_art_scan.ent_sort_max; // whole array is now sorted
    g_art_scan.order_changed = false;
    // rebuild the indexes
    for (long i = 0; i <= g_art_scan.ent_sort_max; i++)
    {
        ent_index[ent_sort[i]] = i;
    }
    save_order_bounds();
}

void ScanContext::order_clean()
{
    ent_sort.clear();
    ent_index.clear();

    g_art_scan.ent_sort_max = -1;
    g_art_scan.ent_sorted_max = -1;
    g_art_scan.ent_index_max = -1;
    save_order_bounds();
}

// adds the entry number to the current context
void ScanContext::order_add(long ent)
{
    if (ent >= 0 && ent < size_cast<long>(ent_index) && ent_index[ent] >= 0)
    {
        return;         // entry is already in the list
    }

    // add entry to end of sorted list
    ent_sort.push_back(ent);
    g_art_scan.ent_sort_max = size_cast<long>(ent_sort) - 1;

    // grow index list if needed
    if (ent >= size_cast<long>(ent_index))
    {
        const long new_size = (ent/100+1) * 100 + 1; // round up
        ent_index.resize(new_size, -1);              // -1 == not a legal entry
        g_art_scan.ent_index_max = size_cast<long>(ent_index) - 1;
    }
    ent_index[ent] = g_art_scan.ent_sort_max;
    g_art_scan.order_changed = true;
    save_order_bounds();
}

long ScanContext::prev(long ent)
{
    if (ent < 0 || ent > g_art_scan.ent_index_max || g_art_scan.ent_sorted_max < 0)
    {
        return 0;
    }
    if (g_art_scan.order_changed)
    {
        sort();
    }
    long tmp = ent_index[ent];
    if (tmp <= 0)
    {
        return 0;
    }
    return ent_sort[tmp-1];
}

long ScanContext::next(long ent)
{
    if (ent < 0 || ent > g_art_scan.ent_index_max || g_art_scan.ent_sorted_max < 0)
    {
        return 0;
    }
    if (g_art_scan.order_changed)
    {
        sort();
    }
    long tmp = ent_index[ent];
    if (tmp < 0 || tmp == g_art_scan.ent_sorted_max)
    {
        return 0;
    }
    return ent_sort[tmp+1];
}

// given an entry, returns previous eligible entry
// returns 0 if no previous eligible entry
long ScanContext::prev_elig(long a)
{
    while ((a = prev(a)) != 0)
    {
        if (s_eligible(a))
        {
            return a;
        }
    }
    return 0L;
}

// given an entry, returns next eligible entry
// returns 0 if no next eligible entry
long ScanContext::next_elig(long a)
{
    while ((a = next(a)) != 0)
    {
        if (s_eligible(a))
        {
            return a;
        }
    }
    return 0L;
}

long ScanContext::first()
{
    if (g_art_scan.order_changed)
    {
        sort();
    }
    if (g_art_scan.ent_sorted_max < 0)
    {
        return 0;
    }
    return ent_sort[0];
}

long s_last()
{
    std::vector<long> &ent_sort = g_art_scan.ent_sort();

    if (g_art_scan.order_changed)
    {
        g_art_scan.current_context().sort();
    }
    if (g_art_scan.ent_sorted_max < 0)
    {
        return 0;
    }
    return ent_sort[g_art_scan.ent_sorted_max];
}
