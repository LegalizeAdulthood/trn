/* This file Copyright 1993 by Clifford A. Adams */
/* scoresave.h
 *
 */

EXT long sc_save_new INIT(0);	/* new articles (unloaded) */

EXT int sc_loaded_count INIT(0);	/* how many articles were loaded? */

void sc_sv_add(const char *str);
void sc_sv_delgroup(const char *gname);
void sc_sv_getfile();
void sc_sv_savefile();
ART_NUM sc_sv_use_line(char *line, ART_NUM a);
ART_NUM sc_sv_make_line(ART_NUM a);
void sc_load_scores();
void sc_save_scores();
