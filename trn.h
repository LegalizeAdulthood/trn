/* trn.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_H
#define TRN_H

enum
{
    ING_NORM = 0,
    ING_ASK = 1,
    ING_INPUT = 2,
    ING_ERASE = 3,
    ING_QUIT = 4,
    ING_ERROR = 5,
    ING_SPECIAL = 6,
    ING_BREAK = 7,
    ING_RESTART = 8,
    ING_NOSERVER = 9,
    ING_DISPLAY = 10,
    ING_MESSAGE = 11
};

enum
{
    INGS_CLEAN = 0,
    INGS_DIRTY = 1
};

extern char *g_ngname;   /* name of current newsgroup */
extern int g_ngnlen;     /* current malloced size of g_ngname */
extern int g_ngname_len; /* length of current g_ngname */
extern char *g_ngdir;    /* same thing in directory name form */
extern int g_ngdlen;     /* current malloced size of g_ngdir */
extern int g_ing_state;
extern bool g_write_less;      /* write .newsrc less often */
extern char *g_auto_start_cmd; /* command to auto-start with */
extern bool g_auto_started;    /* have we auto-started? */
extern bool g_is_strn;         /* Is this "strn", or trn/rn? */
extern char g_patchlevel[];

void trn_init();
void do_multirc();
int input_newsgroup();
void check_active_refetch(bool force);
void trn_version();
void set_ngname(const char *what);
const char *getngdir(const char *ngnam);

#endif
