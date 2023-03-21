/* This file is Copyright 1993 by Clifford A. Adams */
/* smisc.h
 */
#ifndef SMISC_H
#define SMISC_H

extern bool g_s_default_cmd; /* true if the last command (run through setdef()) was the default */
extern bool g_s_follow_temp; /* explicitly follow until end of thread */

bool s_eligible(long ent);
void s_beep();
const char *s_get_statchars(long ent, int line);
const char *s_get_desc(long ent, int line, bool trunc);
int s_ent_lines(long ent);

#endif
