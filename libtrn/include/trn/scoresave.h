/* trn/scoresave.h
 *
 */
// This file Copyright 1993 by Clifford A. Adams
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_SCORESAVE_H
#define TRN_SCORESAVE_H

#include <config/typedef.h>

extern int g_sc_loaded_count; // how many articles were loaded?

void       sc_sv_save_file();
void       sc_load_scores();
void       sc_save_scores();

#endif
