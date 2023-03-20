/* util2.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

char *savestr(const char *str);
char *safecpy(char *to, const char *from, int len);
char *cpytill(char *to, char *from, int delim);
char *filexp(char *s);
char *in_string(char *big, const char *little, bool case_matters);
#ifndef HAS_STRCASECMP
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, int len);
#endif
char *read_auth_file(const char *file, char **pass_ptr);
#ifdef MSDOS
int getuid();
#endif
