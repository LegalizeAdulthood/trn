/* util2.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */


/* DON'T EDIT BELOW THIS LINE OR YOUR CHANGES WILL BE LOST! */

char* savestr _((char*));
char* safecpy _((char*,char*,int));
char* cpytill _((char*,char*,int));
char* filexp _((char*));
char* in_string(const char*,const char*,bool_int);
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
