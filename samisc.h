/* This file Copyright 1992 by Clifford A. Adams */
/* samisc.h
 *
 * smaller functions
 */

#ifdef UNDEF
int check_article(long);
#endif
bool sa_basic_elig(long);
bool sa_eligible(long);
long sa_artnum_to_ent(ART_NUM);
void sa_selthreads(void);
int sa_number_arts(void);
void sa_go_art(long);
int sa_compare(long, long);
