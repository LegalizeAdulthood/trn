/* util2.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_UTIL2_H
#define TRN_UTIL2_H

char *savestr(const char *str);
char *safecpy(char *to, const char *from, int len);
char *cpytill(char *to, char *from, int delim);
char *filexp(const char *text);
char *in_string(char *big, const char *little, bool case_matters);
char *read_auth_file(const char *file, char **pass_ptr);

#endif
