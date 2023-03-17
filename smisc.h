/* This file is Copyright 1993 by Clifford A. Adams */
/* smisc.h
 */

/* true if the last command (run through setdef()) was the default */
EXT bool s_default_cmd INIT(false);

/* explicitly follow until end of thread */
EXT bool s_follow_temp INIT(false);

bool s_eligible(long);
void s_beep();
char *s_get_statchars(long, int);
char *s_get_desc(long ent, int line, bool trunc);
int s_ent_lines(long);
