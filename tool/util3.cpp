/* util3.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <tool/util3.h>

#include <nntp/nntpclient.h>
#include <util/util2.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

static std::string    s_nntp_password;
static constexpr char s_no_memory[] = "trn: out of memory!\n";

int do_shell(const char *shell, const char *cmd)
{
    return std::system(cmd);
}

[[noreturn]] void finalize(int num)
{
    nntp_close(true);
    std::exit(num);
}

// paranoid version of malloc

#ifndef USE_DEBUGGING_MALLOC
char *safe_malloc(MemorySize size)
{
    char *ptr = (char*)std::malloc(size ? size : (MemorySize)1);
    if (!ptr)
    {
        std::fputs(s_no_memory,stdout);
        finalize(1);
    }
    return ptr;
}
#endif

// paranoid version of realloc.  If where is nullptr, call malloc

#ifndef USE_DEBUGGING_MALLOC
char *safe_realloc(char *where, MemorySize size)
{
    char *ptr = (char*)std::realloc(where, size ? size : (MemorySize)1);
    if (!ptr)
    {
        std::fputs(s_no_memory,stdout);
        finalize(1);
    }
    return ptr;
}
#endif

std::string do_interp(std::string_view pattern)
{
    extern std::string g_dot_dir;
    if (pattern.size() >= 2 && pattern[0] == '%' && pattern[1] == '.')
    {
        std::string result{g_dot_dir};
        result.append(pattern.data() + 2, pattern.size() - 2);
        return result;
    }
    return std::string{pattern};
}

int nntp_handle_nested_lists()
{
    std::fputs("Programming error! Nested NNTP calls detected.\n",stderr);
    return -1;
}

std::string get_auth_user()
{
    extern std::string    g_nntp_auth_file;
    const AuthCredentials credentials = read_auth_file(g_nntp_auth_file.c_str());

    s_nntp_password = credentials.password;
    return credentials.user;
}

std::string get_auth_pass()
{
    return s_nntp_password;
}
