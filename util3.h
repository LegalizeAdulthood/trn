/* util3.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */

extern char* g_home_dir;

int doshell(char *, char *);
void finalize(int);
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR >= 5)
  __attribute__((noreturn))
#endif

#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE);
char *saferealloc(char *, MEM_SIZE);
#endif
char *dointerp(char *, int, char *, char *, char *);
#ifdef SUPPORT_NNTP
int nntp_handle_nested_lists(void);
char *get_auth_user(void);
char *get_auth_pass(void);
#endif
#if defined(USE_GENAUTH) && defined(SUPPORT_NNTP)
char *get_auth_command(void);
#endif
