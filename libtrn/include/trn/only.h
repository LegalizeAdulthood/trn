/* trn/only.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_ONLY_H
#define TRN_ONLY_H

#include <config/common.h>

#ifndef NBRA
#include "trn/search.h"
#endif

constexpr int MAX_NG_TO_DO{512}; /* number of newsgroups allowed on command line */

extern char *g_newsgroup_to_do[MAX_NG_TO_DO]; /* restrictions in effect */
extern int   g_max_newsgroup_to_do;           /*  0 => no restrictions */
                                              /* >0 => # of entries in g_ngtodo */
extern char g_empty_only_char;

void only_init();
void set_newsgroup_to_do(const char *pat);
bool in_list(const char *newsgroup_name);
void end_only();
void push_only();
void pop_only();

#endif
