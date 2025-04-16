/* trn/rcstuff.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RCSTUFF_H
#define TRN_RCSTUFF_H

#include <config/typedef.h>

#include "trn/List.h"
#include "trn/enum-flags.h"

struct DataSource;
struct HASHTABLE;
struct NewsgroupData;

enum : ART_UNREAD
{
    TR_ONE = (ART_UNREAD) 1,
    TR_NONE = (ART_UNREAD) 0,
    TR_UNSUB = (ART_UNREAD) -1, /* keep this one as -1, some tests use >= TR_UNSUB */
    TR_IGNORE = (ART_UNREAD) -2,
    TR_BOGUS = (ART_UNREAD) -3,
    TR_JUNK = (ART_UNREAD) -4,
};

enum AddNewType : char
{
    ADDNEW_ASK = '\0',
    ADDNEW_SUB = ':',
    ADDNEW_UNSUB = '!'
};

enum GetNewsgroupFlags : int
{
    GNG_NONE = 0x00,
    GNG_RELOC = 0x01,
    GNG_FUZZY = 0x02
};
DECLARE_FLAGS_ENUM(GetNewsgroupFlags, int);

enum NewsrcFlags : int
{
    RF_NONE = 0x0000,
    RF_ADD_NEWGROUPS = 0x0001,
    RF_ADD_GROUPS = 0x0002,
    RF_OPEN = 0x0100,
    RF_ACTIVE = 0x0200,
    RF_RCCHANGED = 0x0400
};
DECLARE_FLAGS_ENUM(NewsrcFlags, int);

struct Newsrc
{
    Newsrc      *next;
    DataSource  *datasrc;
    char        *name;     /* the name of the associated newsrc */
    char        *oldname;  /* the backup of the newsrc */
    char        *newname;  /* our working newsrc file */
    char        *infoname; /* the time/size info file */
    char        *lockname; /* the lock file we created */
    NewsrcFlags flags;
};

enum MultircFlags : int
{
    MF_NONE = 0x0000,
    MF_SEL = 0x0001,
    MF_INCLUDED = 0x0010
};
DECLARE_FLAGS_ENUM(MultircFlags, int);

struct Multirc
{
    Newsrc       *first;
    int           num;
    MultircFlags flags;
};

extern HASHTABLE  *g_newsrc_hash;
extern Multirc    *g_sel_page_mp;
extern Multirc    *g_sel_next_mp;
extern List       *g_multirc_list;    /* a list of all MULTIRCs */
extern Multirc    *g_multirc;         /* the current MULTIRC */
extern bool        g_paranoid;        /* did we detect some inconsistency in .newsrc? */
extern AddNewType g_addnewbydefault; //
extern bool        g_checkflag;       /* -c */
extern bool        g_suppress_cn;     /* -s */
extern int         g_countdown;       /* how many lines to list before invoking -s */
extern bool        g_fuzzy_get;       /* -G */
extern bool        g_append_unsub;    /* -I */

bool     rcstuff_init();
void     rcstuff_final();
Newsrc  *new_newsrc(const char *name, const char *newsrc, const char *add_ok);
bool     use_multirc(Multirc *mp);
void     unuse_multirc(Multirc *mptr);
bool     use_next_multirc(Multirc *mptr);
bool     use_prev_multirc(Multirc *mptr);
char    *multirc_name(Multirc *mp);
void     abandon_ng(NewsgroupData *np);
bool     get_ng(const char *what, GetNewsgroupFlags flags);
bool     relocate_newsgroup(NewsgroupData *move_np, NewsgroupNum newnum);
void     list_newsgroups();
NewsgroupData  *find_ng(const char *ngnam);
void     cleanup_newsrc(Newsrc *rp);
void     sethash(NewsgroupData *np);
void     checkpoint_newsrcs();
bool     write_newsrcs(Multirc *mptr);
void     get_old_newsrcs(Multirc *mptr);

inline Multirc *multirc_ptr(long n)
{
    return (Multirc *) listnum2listitem(g_multirc_list, n);
}
inline Multirc *multirc_low()
{
    return (Multirc *) listnum2listitem(g_multirc_list, existing_listnum(g_multirc_list, 0L, 1));
}
inline Multirc *multirc_high()
{
    return (Multirc *) listnum2listitem(g_multirc_list, existing_listnum(g_multirc_list, g_multirc_list->high, -1));
}
inline Multirc *multirc_next(Multirc *p)
{
    return (Multirc *) next_listitem(g_multirc_list, (char *) p);
}
inline Multirc *multirc_prev(Multirc *p)
{
    return (Multirc *) prev_listitem(g_multirc_list, (char *) p);
}

#endif
