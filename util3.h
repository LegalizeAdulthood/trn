/* util3.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef UTIL3_H
#define UTIL3_H

extern char *g_home_dir;

int doshell(const char *sh, const char *cmd);
[[noreturn]] void finalize(int num);
#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE size);
char *saferealloc(char *where, MEM_SIZE size);
#endif
char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, char *cmd);
int nntp_handle_nested_lists();
char *get_auth_user();
char *get_auth_pass();

#endif
