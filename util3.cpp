/* util3.c
 */
/* This software is copyrighted as detailed in the LICENSE file. */


#include <stdio.h>
#include "config.h"
#include "config2.h"
#include "typedef.h"
#include "EXTERN.h"
#include "config.h"
#include "config2.h"
#include "nntpclient.h"
#include "util2.h"
#include "INTERN.h"
#include "util3.h"

bool export_nntp_fds = false;

char* nntp_password = nullptr;

int doshell(const char *sh, const char *cmd)
{
    return system(cmd);
}

void finalize(int num)
{
    nntp_close(true);
    exit(num);
}

static char nomem[] = "trn: out of memory!\n";

/* paranoid version of malloc */

#ifndef USE_DEBUGGING_MALLOC
char *safemalloc(MEM_SIZE size)
{
    char* ptr;

    ptr = (char*) malloc(size ? size : (MEM_SIZE)1);
    if (!ptr) {
	fputs(nomem,stdout);
	finalize(1);
    }
    return ptr;
}
#endif

/* paranoid version of realloc.  If where is nullptr, call malloc */

#ifndef USE_DEBUGGING_MALLOC
char *saferealloc(char *where, MEM_SIZE size)
{
    char* ptr;

    ptr = (char*) realloc(where, size ? size : (MEM_SIZE)1);
    if (!ptr) {
	fputs(nomem,stdout);
	finalize(1);
    }
    return ptr;
}
#endif

char *dointerp(char *dest, int destsize, char *pattern, const char *stoppers, char *cmd)
{
    extern char* g_dot_dir;
    if (*pattern == '%' && pattern[1] == '.') {
	int len = strlen(g_dot_dir);
	safecpy(dest, g_dot_dir, destsize);
	if (len < destsize)
	    safecpy(dest+len, pattern+2, destsize - len);
    }
    else
	safecpy(dest, pattern, destsize);
    return nullptr; /* This is wrong on purpose */
}

int nntp_handle_nested_lists()
{
    fputs("Programming error! Nested NNTP calls detected.\n",stderr);
    return -1;
}

char *get_auth_user()
{
    extern char* nntp_auth_file;
    return read_auth_file(nntp_auth_file, &nntp_password);
}

char *get_auth_pass()
{
    return nntp_password;
}
