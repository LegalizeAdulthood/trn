/* intrp.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_INTRP_H
#define TRN_INTRP_H

#include <string>

extern char *g_origdir;  /* cwd when rn invoked */
extern char *g_hostname; /* host name to match local postings */
extern std::string g_headname;
extern int g_perform_cnt;

#ifdef HAS_NEWS_ADMIN
extern const std::string g_newsadmin; /* news administrator */
extern int g_newsuid;
#endif

void intrp_init(char *tcbuf, int tcbuf_len);
void intrp_final();
char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, char *cmd);
char *interp_backslash(char *dest, char *pattern);
char *interp(char *dest, int destsize, char *pattern);
char *interpsearch(char *dest, int destsize, char *pattern, char *cmd);
void normalize_refs(char *refs);

#endif
