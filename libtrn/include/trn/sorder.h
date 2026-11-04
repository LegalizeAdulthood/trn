/* trn/sorder.h
 *
 * scan ordering
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SORDER_H
#define TRN_SORDER_H

int s_compare(long a, long b);
void s_order_add(long ent);
long s_prev(long ent);
long s_next(long ent);
long s_prev_elig(long a);
long s_next_elig(long a);
long s_first();
long s_last();

#endif
