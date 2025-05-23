// This file Copyright 1992,1995 by Clifford A. Adams
/* trn/sadesc.h
 *
 */
#ifndef TRN_SADESC_H
#define TRN_SADESC_H

const char *sa_get_stat_chars(long a, int line);
const char *sa_desc_subject(long e);
const char *sa_get_desc(long e, int line, bool trunc);
int sa_ent_lines(long e);

#endif
