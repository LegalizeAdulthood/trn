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
static char s_nomem[] = "trn: out of memory!\n";

int doshell(const char *shell, const char *cmd)
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
char *safemalloc(MEM_SIZE size)
{
    char *ptr = (char*)std::malloc(size ? size : (MEM_SIZE)1);
    if (!ptr) {
        std::fputs(s_nomem,stdout);
        finalize(1);
    }
    return ptr;
}
#endif

/* paranoid version of realloc.  If where is nullptr, call malloc */

#ifndef USE_DEBUGGING_MALLOC
char *saferealloc(char *where, MEM_SIZE size)
{
    char *ptr = (char*)std::realloc(where, size ? size : (MEM_SIZE)1);
    if (!ptr) {
        std::fputs(s_nomem,stdout);
        finalize(1);
    }
    return ptr;
}
#endif

char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, const char *cmd)
{
    extern std::string g_dot_dir;
    if (*pattern == '%' && pattern[1] == '.') {
        int len = std::strlen(g_dot_dir.c_str());
        safecpy(dest, g_dot_dir.c_str(), destsize);
        if (len < destsize)
        {
            safecpy(dest+len, pattern+2, destsize - len);
        }
    }
    else
    {
        safecpy(dest, pattern, destsize);
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
