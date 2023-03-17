/* intrp.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char* origdir INIT(NULL);		/* cwd when rn invoked */
EXT char* hostname INIT(NULL);		/* host name to match local postings */
EXT char* headname INIT(NULL);
EXT int perform_cnt;

#ifdef NEWS_ADMIN
EXT char newsadmin[] INIT(NEWS_ADMIN);/* news administrator */
EXT int newsuid INIT(0);
#endif

void intrp_init(char *, int);
char *dointerp(char *, int, char *, char *, char *);
char *interp_backslash(char *, char *);
char *interp(char *, int, char *);
char *interpsearch(char *, int, char *, char *);
void normalize_refs(char *);
