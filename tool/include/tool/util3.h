/* util3.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_UTIL3_H
#define TRN_UTIL3_H

#include <config/typedef.h>

#include <string>
#include <string_view>

extern std::string g_home_dir;

int do_shell(const char *shell, const char *cmd);
[[noreturn]] void finalize(int num);
#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size);
char *safe_realloc(char *where, MemorySize size);
#endif
std::string do_interp(std::string_view pattern);
int   nntp_handle_nested_lists();
std::string get_auth_user();
std::string get_auth_pass();

#endif
