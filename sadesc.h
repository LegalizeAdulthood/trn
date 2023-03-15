/* This file Copyright 1992,1995 by Clifford A. Adams */
/* sadesc.h
 *
 */

char* sa_get_statchars _((long,int));
char* sa_desc_subject _((long));
char *sa_get_desc(long e, int line, bool trunc);
int sa_ent_lines _((long));
