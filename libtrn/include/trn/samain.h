/* trn/samain.h
 *
 * Main routines for article-scan mode.
 */
// This file Copyright 1992,1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SAMAIN_H
#define TRN_SAMAIN_H

#include <config/typedef.h>

extern bool g_sa_mode_zoom;    // true if in "zoom" (display only selected) mode
extern int g_sa_scan_context;  // contains the scan-context number for the current article scan

void sa_init();
void sa_clean_main();
void sa_grow_arts(ArticleNum oldlast, ArticleNum last);
void sa_lookahead();
long sa_read_marked_elig();

#endif
