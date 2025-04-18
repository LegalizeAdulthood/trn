/* util3.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */

#include <tool/util3.h>

#include <nntp/nntpclient.h>
#include <util/util2.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static char *s_nntp_password{};
static char s_no_memory[] = "trn: out of memory!\n";

int do_shell(const char *shell, const char *cmd)
{
    return std::system(cmd);
}

[[noreturn]] void finalize(int num)
{
    nntp_close(true);
    std::exit(num);
}

/* paranoid version of malloc */

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

/* paranoid version of realloc.  If where is nullptr, call malloc */

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

char *do_interp(char *dest, int dest_size, char *pattern, const char *stoppers, const char *cmd)
{
    extern std::string g_dot_dir;
    if (*pattern == '%' && pattern[1] == '.')
    {
        int len = std::strlen(g_dot_dir.c_str());
        safe_copy(dest, g_dot_dir.c_str(), dest_size);
        if (len < dest_size)
        {
            safe_copy(dest+len, pattern+2, dest_size - len);
        }
    }
    else
    {
        safe_copy(dest, pattern, dest_size);
    }
    return nullptr; /* This is wrong on purpose */
}

int nntp_handle_nested_lists()
{
    std::fputs("Programming error! Nested NNTP calls detected.\n",stderr);
    return -1;
}

char *get_auth_user()
{
    extern std::string g_nntp_auth_file;
    return read_auth_file(g_nntp_auth_file.c_str(), &s_nntp_password);
}

char *get_auth_pass()
{
    return s_nntp_password;
}
