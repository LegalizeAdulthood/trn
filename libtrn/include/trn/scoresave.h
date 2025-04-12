/* This file Copyright 1993 by Clifford A. Adams */
/* trn/scoresave.h
 *
 */
#ifndef TRN_SCORESAVE_H
#define TRN_SCORESAVE_H

extern int g_sc_loaded_count; /* how many articles were loaded? */

void sc_sv_add(const char *str);
void sc_sv_delgroup(const char *gname);
void sc_sv_getfile();
void sc_sv_savefile();
ART_NUM sc_sv_use_line(char *line, ART_NUM a);
ART_NUM sc_sv_make_line(ART_NUM a);
void sc_load_scores();
void sc_save_scores();

#endif
