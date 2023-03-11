/* This file Copyright 1992 by Clifford A. Adams */
/* samisc.h
 *
 * smaller functions
 */

EXTERN_C_BEGIN

#ifdef UNDEF
int check_article _((long));
#endif
bool sa_basic_elig _((long));
bool sa_eligible _((long));
long sa_artnum_to_ent _((ART_NUM));
void sa_selthreads _((void));
int sa_number_arts _((void));
void sa_go_art _((long));
int sa_compare _((long,long));

EXTERN_C_END
