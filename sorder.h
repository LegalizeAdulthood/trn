/* This file Copyright 1993 by Clifford A. Adams */
/* sorder.h
 *
 * scan ordering
 */
#ifndef SORDER_H
#define SORDER_H

extern bool g_s_order_changed; /* If true, resort next time order is considered */

int s_compare(long a, long b);
void s_sort_basic();
void s_sort();
void s_order_clean();
void s_order_add(long ent);
long s_prev(long ent);
long s_next(long ent);
long s_prev_elig(long a);
long s_next_elig(long a);
long s_first();
long s_last();

#endif
