/* intrp.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

EXT char* origdir INIT(nullptr);		/* cwd when rn invoked */
EXT char* hostname INIT(nullptr);		/* host name to match local postings */
EXT char* headname INIT(nullptr);
EXT int perform_cnt;

#ifdef NEWS_ADMIN
EXT char newsadmin[] INIT(NEWS_ADMIN);/* news administrator */
EXT int newsuid INIT(0);
#endif

void intrp_init(char *, int);
char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, char *cmd);
char *interp_backslash(char *, char *);
char *interp(char *, int, char *);
char *interpsearch(char *, int, char *, char *);
void normalize_refs(char *);
