/* trn/sadesc.h
 *
 */
// This file Copyright 1992,1995 by Clifford A. Adams
#ifndef TRN_SADESC_H
#define TRN_SADESC_H

#include <string>

const char *sa_get_stat_chars(long a, int line);
std::string sa_desc_subject(long e);
std::string sa_get_desc(long e, int line, bool trunc);
int sa_ent_lines(long e);

#endif
