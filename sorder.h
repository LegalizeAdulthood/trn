/* This file Copyright 1993 by Clifford A. Adams */
/* sorder.h
 *
 * scan ordering
 */

/* If true, resort next time order is considered */
EXT bool s_order_changed INIT(false);

int s_compare(long, long);
void s_sort_basic();
void s_sort();
void s_order_clean();
void s_order_add(long);
long s_prev(long);
long s_next(long);
long s_prev_elig(long);
long s_next_elig(long);
long s_first();
long s_last();
