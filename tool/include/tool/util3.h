/* util3.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_UTIL3_H
#define TRN_UTIL3_H

#include <config/typedef.h>

extern char *g_home_dir;

int do_shell(const char *shell, const char *cmd);
[[noreturn]] void finalize(int num);
#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size);
char *safe_realloc(char *where, MemorySize size);
#endif
char *do_interp(char *dest, int dest_size, char *pattern, const char *stoppers, const char *cmd);
int   nntp_handle_nested_lists();
char *get_auth_user();
char *get_auth_pass();

#endif
