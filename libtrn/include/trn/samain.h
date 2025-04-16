/* This file Copyright 1992,1993 by Clifford A. Adams */
/* trn/samain.h
 *
 * Main routines for article-scan mode.
 */
#ifndef TRN_SAMAIN_H
#define TRN_SAMAIN_H

#include <config/typedef.h>

extern bool g_sa_mode_zoom;    /* true if in "zoom" (display only selected) mode */
extern int g_sa_scan_context;  /* contains the scan-context number for the current article scan */

void sa_init();
void sa_init_ents();
void sa_clean_ents();
long sa_add_ent(ArticleNum artnum);
void sa_cleanmain();
void sa_growarts(long oldlast, long last);
void sa_init_context();
bool sa_initarts();
void sa_initmode();
void sa_lookahead();
long sa_readmarked_elig();

#endif
