/* nntpclient.h
*/ 
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_NNTPCLIENT_H
#define TRN_NNTPCLIENT_H

#include "enum-flags.h"

enum nntp_flags
{
    NNTP_NONE = 0x0000,
    NNTP_NEW_CMD_OK = 0x0001,
    NNTP_FORCE_AUTH_NEEDED = 0x0002,
    NNTP_FORCE_AUTH_NOW = 0x0004
};
DECLARE_FLAGS_ENUM(nntp_flags, int);

struct NNTPLINK
{
    FILE      *rd_fp;
    FILE      *wr_fp;
    time_t     last_command;
    int        port_number;
    nntp_flags flags;
    bool       trailing_CR;
};

/* RFC 977 defines these, so don't change them */
enum
{
    NNTP_CLASS_INF = '1',
    NNTP_CLASS_OK = '2',
    NNTP_CLASS_CONT = '3',
    NNTP_CLASS_ERR = '4',
    NNTP_CLASS_FATAL = '5'
};

enum
{
    NNTP_POSTOK_VAL = 200,       /* Hello -- you can post */
    NNTP_NOPOSTOK_VAL = 201,     /* Hello -- you can't post */
    NNTP_LIST_FOLLOWS_VAL = 215, /* There's a list a-comin' next */
    NNTP_GOODBYE_VAL = 400,      /* Have to hang up for some reason */
    NNTP_NOSUCHGROUP_VAL = 411,  /* No such newsgroup */
    NNTP_NONEXT_VAL = 421,       /* No next article */
    NNTP_NOPREV_VAL = 422,       /* No previous article */
    NNTP_POSTFAIL_VAL = 441,     /* Posting failed */
    NNTP_AUTH_NEEDED_VAL = 480,  /* Authorization Failed */
    NNTP_AUTH_REJECT_VAL = 482,  /* Authorization data rejected */
    NNTP_BAD_COMMAND_VAL = 500,  /* Command not recognized */
    NNTP_SYNTAX_VAL = 501,       /* Command syntax error */
    NNTP_ACCESS_VAL = 502,       /* Access to server denied */
    NNTP_TMPERR_VAL = 503,       /* Program fault, command not performed */
    NNTP_AUTH_BAD_VAL = 580      /* Authorization Failed */
};

enum
{
    NNTP_STRLEN = 512
};

extern NNTPLINK g_nntplink; /* the current server's file handles */
extern bool g_nntp_allow_timeout;
extern char g_ser_line[NNTP_STRLEN];
extern char g_last_command[NNTP_STRLEN];

#define nntp_get_a_line(buf, len, realloc) get_a_line(buf, len, realloc, g_nntplink.rd_fp)

int nntp_connect(const char *machine, bool verbose);
char *nntp_servername(char *name);
int nntp_command(const char *bp);
int nntp_check();
bool nntp_at_list_end(const char *s);
int nntp_gets(char *bp, int len);
void nntp_close(bool send_quit);

#endif
