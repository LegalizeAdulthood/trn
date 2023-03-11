/* This file Copyright 1992,1995 by Clifford A. Adams */
/* sadesc.h
 *
 */

EXTERN_C_BEGIN

char* sa_get_statchars _((long,int));
char* sa_desc_subject _((long));
char* sa_get_desc _((long,int,bool_int));
int sa_ent_lines _((long));

EXTERN_C_END
