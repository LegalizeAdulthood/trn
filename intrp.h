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

void intrp_init(char *tcbuf, int tcbuf_len);
char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, char *cmd);
char *interp_backslash(char *dest, char *pattern);
char *interp(char *dest, int destsize, char *pattern);
char *interpsearch(char *dest, int destsize, char *pattern, char *cmd);
void normalize_refs(char *refs);
