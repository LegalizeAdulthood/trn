/* trn.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_TRN_H
#define TRN_TRN_H

#include <string>

enum input_newsgroup_result
{
    ING_NORM = 0,
    ING_ASK,
    ING_INPUT,
    ING_ERASE,
    ING_QUIT,
    ING_ERROR,
    ING_SPECIAL,
    ING_BREAK,
    ING_RESTART,
    ING_NOSERVER,
    ING_DISPLAY,
    ING_MESSAGE
};

extern char       *g_ngname;         /* name of current newsgroup */
extern int         g_ngnlen;         /* current malloced size of g_ngname */
extern int         g_ngname_len;     /* length of current g_ngname */
extern std::string g_ngdir;          /* same thing in directory name form */
extern bool        g_write_less;     /* write .newsrc less often */
extern char       *g_auto_start_cmd; /* command to auto-start with */
extern bool        g_auto_started;   /* have we auto-started? */
extern bool        g_is_strn;        /* Is this "strn", or trn/rn? */
extern char        g_patchlevel[];

void trn_init();
void do_multirc();
input_newsgroup_result input_newsgroup();
void check_active_refetch(bool force);
void trn_version();
void set_ngname(const char *what);
int trn_main(int argc, char *argv[]);

#endif
