/* This file Copyright 1992,1995 by Clifford A. Adams */
/* sadesc.h
 *
 */

char *sa_get_statchars(long, int);
char *sa_desc_subject(long);
char *sa_get_desc(long e, int line, bool trunc);
int sa_ent_lines(long);
