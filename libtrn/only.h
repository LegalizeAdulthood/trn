/* only.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ONLY_H
#define TRN_ONLY_H

#ifndef NBRA
#include "search.h"
#endif

extern char *g_ngtodo[MAXNGTODO];       /* restrictions in effect */
extern int g_maxngtodo;                 /*  0 => no restrictions */
                                      /* >0 => # of entries in g_ngtodo */
extern char g_empty_only_char;

void only_init();
void setngtodo(const char *pat);
bool inlist(const char *ngnam);
void end_only();
void push_only();
void pop_only();

#endif
