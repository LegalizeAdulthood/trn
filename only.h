/* only.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#ifndef NBRA
#include "search.h"
#endif

EXT char* ngtodo[MAXNGTODO];		/* restrictions in effect */
EXT COMPEX* compextodo[MAXNGTODO];	/* restrictions in compiled form */

EXT int maxngtodo INIT(0);		/*  0 => no restrictions */
					/* >0 => # of entries in ngtodo */

EXT char empty_only_char INIT('o');

void only_init();
void setngtodo(const char *pat);
bool inlist(char *ngnam);
void end_only();
void push_only();
void pop_only();
