/* util3.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

extern char* g_home_dir;

int doshell(const char *sh, const char *cmd);
[[noreturn]] void finalize(int);
#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE);
char *saferealloc(char *, MEM_SIZE);
#endif
char *dointerp(char *, int, char *, const char *, char *);
int nntp_handle_nested_lists();
char *get_auth_user();
char *get_auth_pass();
