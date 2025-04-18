// This file Copyright 1993 by Clifford A. Adams
/* trn/scoresave.h
 *
 */
#ifndef TRN_SCORESAVE_H
#define TRN_SCORESAVE_H

#include <config/typedef.h>

extern int g_sc_loaded_count; // how many articles were loaded?

void       sc_sv_add(const char *str);
void       sc_sv_del_group(const char *gname);
void       sc_sv_get_file();
void       sc_sv_save_file();
ArticleNum sc_sv_use_line(char *line, ArticleNum a);
ArticleNum sc_sv_make_line(ArticleNum a);
void       sc_load_scores();
void       sc_save_scores();

#endif
