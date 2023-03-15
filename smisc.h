/* This file is Copyright 1993 by Clifford A. Adams */
/* smisc.h
 */

/* true if the last command (run through setdef()) was the default */
EXT bool s_default_cmd INIT(false);

/* explicitly follow until end of thread */
EXT bool s_follow_temp INIT(false);

/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

bool s_eligible _((long));
void s_beep _((void));
char* s_get_statchars _((long,int));
char *s_get_desc(long ent, int line, bool trunc);
int s_ent_lines _((long));
