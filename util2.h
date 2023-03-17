/* util2.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

char *savestr(char *);
char *safecpy(char *, char *, int);
char *cpytill(char *, char *, int);
char *filexp(char *);
char *in_string(char *big, char *little, bool case_matters);
#ifndef HAS_STRCASECMP
int trn_casecmp(const char *, const char *);
int trn_ncasecmp(const char *, const char *, int);
#endif
#ifdef SUPPORT_NNTP
char *read_auth_file(char *, char **);
#endif
#ifdef MSDOS
int getuid();
#endif
