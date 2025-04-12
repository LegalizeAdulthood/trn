/* This file Copyright 1992 by Clifford A. Adams */
/* trn/samisc.h
 *
 * smaller functions
 */
#ifndef TRN_SAMISC_H
#define TRN_SAMISC_H

bool sa_basic_elig(long a);
bool sa_eligible(long a);
long sa_artnum_to_ent(ART_NUM artnum);
void sa_selthreads();
int sa_number_arts();
void sa_go_art(long a);
int sa_compare(long a, long b);

#endif
