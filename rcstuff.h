/* rcstuff.h
 */
/* This software is copyrighted as detailed in the LICENSE file. */
#ifndef TRN_RCSTUFF_H
#define TRN_RCSTUFF_H

#include "enum-flags.h"
#include "list.h"

struct DATASRC;
struct HASHTABLE;
struct NGDATA;

#define TR_ONE ((ART_UNREAD) 1)
#define TR_NONE ((ART_UNREAD) 0)
#define TR_UNSUB ((ART_UNREAD) -1)
			/* keep this one as -1, some tests use >= TR_UNSUB */
#define TR_IGNORE ((ART_UNREAD) -2)
#define TR_BOGUS ((ART_UNREAD) -3)
#define TR_JUNK ((ART_UNREAD) -4)

enum
{
    ADDNEW_SUB = ':',
    ADDNEW_UNSUB = '!'
};

enum getnewsgroup_flags : int
{
    GNG_NONE = 0x00,
    GNG_RELOC = 0x01,
    GNG_FUZZY = 0x02
};
DECLARE_FLAGS_ENUM(getnewsgroup_flags, int);

enum newsrc_flags : int
{
    RF_NONE = 0x0000,
    RF_ADD_NEWGROUPS = 0x0001,
    RF_ADD_GROUPS = 0x0002,
    RF_OPEN = 0x0100,
    RF_ACTIVE = 0x0200,
    RF_RCCHANGED = 0x0400
};
DECLARE_FLAGS_ENUM(newsrc_flags, int);

struct NEWSRC
{
    NEWSRC      *next;
    DATASRC     *datasrc;
    char        *name;     /* the name of the associated newsrc */
    char        *oldname;  /* the backup of the newsrc */
    char        *newname;  /* our working newsrc file */
    char        *infoname; /* the time/size info file */
    char        *lockname; /* the lock file we created */
    newsrc_flags flags;
};

enum multirc_flags : int
{
    MF_NONE = 0x0000,
    MF_SEL = 0x0001,
    MF_INCLUDED = 0x0010
};
DECLARE_FLAGS_ENUM(multirc_flags, int);

struct MULTIRC
{
    NEWSRC       *first;
    int           num;
    multirc_flags flags;
};

extern HASHTABLE *g_newsrc_hash;
extern MULTIRC *g_sel_page_mp;
extern MULTIRC *g_sel_next_mp;
extern LIST *g_multirc_list; /* a list of all MULTIRCs */
extern MULTIRC *g_multirc;   /* the current MULTIRC */
extern bool g_paranoid;      /* did we detect some inconsistency in .newsrc? */
extern int g_addnewbydefault;

bool     rcstuff_init();
void     rcstuff_final();
NEWSRC  *new_newsrc(const char *name, const char *newsrc, const char *add_ok);
bool     use_multirc(MULTIRC *mp);
void     unuse_multirc(MULTIRC *mptr);
bool     use_next_multirc(MULTIRC *mptr);
bool     use_prev_multirc(MULTIRC *mptr);
char    *multirc_name(MULTIRC *mp);
void     abandon_ng(NGDATA *np);
bool     get_ng(const char *what, getnewsgroup_flags flags);
bool     relocate_newsgroup(NGDATA *move_np, NG_NUM newnum);
void     list_newsgroups();
NGDATA  *find_ng(const char *ngnam);
void     cleanup_newsrc(NEWSRC *rp);
void     sethash(NGDATA *np);
void     checkpoint_newsrcs();
bool     write_newsrcs(MULTIRC *mptr);
void     get_old_newsrcs(MULTIRC *mptr);

inline MULTIRC *multirc_ptr(long n)
{
    return (MULTIRC *) listnum2listitem(g_multirc_list, n);
}
inline MULTIRC *multirc_low()
{
    return (MULTIRC *) listnum2listitem(g_multirc_list, existing_listnum(g_multirc_list, 0L, 1));
}
inline MULTIRC *multirc_high()
{
    return (MULTIRC *) listnum2listitem(g_multirc_list, existing_listnum(g_multirc_list, g_multirc_list->high, -1));
}
inline MULTIRC *multirc_next(MULTIRC *p)
{
    return (MULTIRC *) next_listitem(g_multirc_list, (char *) p);
}
inline MULTIRC *multirc_prev(MULTIRC *p)
{
    return (MULTIRC *) prev_listitem(g_multirc_list, (char *) p);
}

#endif
