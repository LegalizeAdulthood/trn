/* util2.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */


EXTERN_C_BEGIN

char* savestr _((char*));
char* safecpy _((char*,char*,int));
char* cpytill _((char*,char*,int));
char* filexp _((char*));
char *in_string(char *big, char *little, bool_int case_matters);
#ifndef HAS_STRCASECMP
int trn_casecmp(const char*,const char*);
int trn_ncasecmp(const char*,const char*,int);
#endif
#ifdef SUPPORT_NNTP
char* read_auth_file _((char*,char**));
#endif
#ifdef MSDOS
int getuid _((void));
#endif

EXTERN_C_END
