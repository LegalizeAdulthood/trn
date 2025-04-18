// This file Copyright 1992 by Clifford A. Adams
/* trn/samisc.h
 *
 * smaller functions
 */
#ifndef TRN_SAMISC_H
#define TRN_SAMISC_H

#include <config/typedef.h>

bool sa_basic_elig(long a);
bool sa_eligible(long a);
long sa_artnum_to_ent(ArticleNum artnum);
void sa_sel_threads();
int sa_number_arts();
void sa_go_art(long a);
int sa_compare(long a, long b);

#endif
