/* This file Copyright 1993 by Clifford A. Adams */
/* sorder.c
 *
 * scan ordering
 */

#include "config/common.h"
#include "trn/sorder.h"

#include "trn/samisc.h"
#include "trn/scan.h"
#include "trn/smisc.h"
#include "trn/util.h"

#include <cstdlib>

bool g_s_order_changed{}; /* If true, resort next time order is considered */

#ifdef UNDEF
/* pointers to the two entries to be compared */
int s_compare(long *a, long *b)
{
    switch (g_s_cur_type)
    {
    case S_ART:
        return sa_compare(*a,*b);

    default:
        return *a - *b;
    }
}
#endif

/* the two entry numbers to be compared */
int s_compare(long a, long b)
{
    switch (g_s_cur_type)
    {
    case S_ART:
        return sa_compare(a,b);

    default:
        return a - b;
    }
}

/* sort offset--used so that the 1-offset algorithm is clear even
 * though the array is 0-offset.
 */
#define SOFF(a) ((a)-1)

/* Uses a heapsort algorithm with the heap readjustment inlined. */
void s_sort_basic()
{
    int t1;
    int j;

    int n = g_s_ent_sort_max + 1;
    if (n < 1)
    {
        return;         /* nothing to sort */
    }

    for (int i = n / 2; i >= 1; i--)
    {
        /* begin heap readjust */
        t1 = g_s_ent_sort[SOFF(i)];
        j = 2*i;
        while (j <= n)
        {
            if (j < n //
                && s_compare(g_s_ent_sort[SOFF(j)], g_s_ent_sort[SOFF(j + 1)]) < 0)
            {
                j++;
            }
            if (s_compare(t1,g_s_ent_sort[SOFF(j)]) > 0)
            {
                break;          /* out of while loop */
            }
            g_s_ent_sort[SOFF(j/2)] = g_s_ent_sort[SOFF(j)];
            j = j*2;
        } /* while */
        g_s_ent_sort[SOFF(j/2)] = t1;
        /* end heap readjust */
    } /* for */

    for (int i = n - 1; i >= 1; i--)
    {
        t1 = g_s_ent_sort[SOFF(i+1)];
        g_s_ent_sort[SOFF(i+1)] = g_s_ent_sort[SOFF(1)];
        g_s_ent_sort[SOFF(1)] = t1;
        /* begin heap readjust */
        j = 2;
        while (j <= i)
        {
            if (j < i //
                && s_compare(g_s_ent_sort[SOFF(j)], g_s_ent_sort[SOFF(j + 1)]) < 0)
            {
                j++;
            }
            if (s_compare(t1,g_s_ent_sort[SOFF(j)]) > 0)
            {
                break;  /* out of while */
            }
            g_s_ent_sort[SOFF(j/2)] = g_s_ent_sort[SOFF(j)];
            j = j*2;
        } /* while */
        g_s_ent_sort[SOFF(j/2)] = t1;
        /* end heap readjust */
    } /* for */
    /* end of heapsort */
}

void s_sort()
{
#ifdef UNDEF
    std::qsort((void*)g_s_ent_sort,(g_s_ent_sort_max)+1,sizeof(long),s_compare);
#endif
    s_sort_basic();
    g_s_ent_sorted_max = g_s_ent_sort_max;  /* whole array is now sorted */
    g_s_order_changed = false;
    /* rebuild the indexes */
    for (long i = 0; i <= g_s_ent_sort_max; i++)
    {
        g_s_ent_index[g_s_ent_sort[i]] = i;
    }
}

void s_order_clean()
{
    if (g_s_ent_sort)
    {
        std::free(g_s_ent_sort);
    }
    if (g_s_ent_index)
    {
        std::free(g_s_ent_index);
    }

    g_s_ent_sort = nullptr;
    g_s_contexts[g_s_cur_context].ent_sort = g_s_ent_sort;

    g_s_ent_index = nullptr;
    g_s_contexts[g_s_cur_context].ent_index = g_s_ent_index;

    g_s_ent_sort_max = -1;
    g_s_ent_sorted_max = -1;
    g_s_ent_index_max = -1;
}

/* adds the entry number to the current context */
void s_order_add(long ent)
{
    long size;

    if (ent < g_s_ent_index_max && g_s_ent_index[ent] >= 0)
    {
        return;         /* entry is already in the list */
    }

    /* add entry to end of sorted list */
    g_s_ent_sort_max += 1;
    if (g_s_ent_sort_max % 100 == 0)    /* be nice to realloc */
    {
        size = (g_s_ent_sort_max+100) * sizeof (long);
        g_s_ent_sort = (long*)saferealloc((char*)g_s_ent_sort,size);
        /* change the context too */
        g_s_contexts[g_s_cur_context].ent_sort = g_s_ent_sort;
    }
    g_s_ent_sort[g_s_ent_sort_max] = ent;

    /* grow index list if needed */
    if (ent > g_s_ent_index_max)
    {
        long old = g_s_ent_index_max;
        if (g_s_ent_index_max == -1)
        {
            g_s_ent_index_max += 1;
        }
        g_s_ent_index_max = (ent/100+1) * 100;  /* round up */
        size = (g_s_ent_index_max + 1) * sizeof (long);
        g_s_ent_index = (long*)saferealloc((char*)g_s_ent_index,size);
        /* change the context too */
        g_s_contexts[g_s_cur_context].ent_index = g_s_ent_index;
        /* initialize new indexes */
        for (long i = old + 1; i < g_s_ent_index_max; i++)
        {
            g_s_ent_index[i] = -1;      /* -1 == not a legal entry */
        }
    }
    g_s_ent_index[ent] = g_s_ent_sort_max;
    g_s_order_changed = true;
}

long s_prev(long ent)
{
    if (ent < 0 || ent > g_s_ent_index_max || g_s_ent_sorted_max < 0)
    {
        return 0;
    }
    if (g_s_order_changed)
    {
        s_sort();
    }
    long tmp = g_s_ent_index[ent];
    if (tmp <= 0)
    {
        return 0;
    }
    return g_s_ent_sort[tmp-1];
}

long s_next(long ent)
{
    if (ent < 0 || ent > g_s_ent_index_max || g_s_ent_sorted_max < 0)
    {
        return 0;
    }
    if (g_s_order_changed)
    {
        s_sort();
    }
    long tmp = g_s_ent_index[ent];
    if (tmp < 0 || tmp == g_s_ent_sorted_max)
    {
        return 0;
    }
    return g_s_ent_sort[tmp+1];
}

/* given an entry, returns previous eligible entry */
/* returns 0 if no previous eligible entry */
long s_prev_elig(long a)
{
    while ((a = s_prev(a)) != 0)
    {
        if (s_eligible(a))
        {
            return a;
        }
    }
    return 0L;
}

/* given an entry, returns next eligible entry */
/* returns 0 if no next eligible entry */
long s_next_elig(long a)
{
    while ((a = s_next(a)) != 0)
    {
        if (s_eligible(a))
        {
            return a;
        }
    }
    return 0L;
}

long s_first()
{
    if (g_s_order_changed)
    {
        s_sort();
    }
    if (g_s_ent_sorted_max < 0)
    {
        return 0;
    }
    return g_s_ent_sort[0];
}

long s_last()
{
    if (g_s_order_changed)
    {
        s_sort();
    }
    if (g_s_ent_sorted_max < 0)
    {
        return 0;
    }
    return g_s_ent_sort[g_s_ent_sorted_max];
}
