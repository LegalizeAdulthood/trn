/* trn/rcstuff.h
 */
// This software is copyrighted as detailed in the LICENSE file.
#ifndef TRN_RCSTUFF_H
#define TRN_RCSTUFF_H

#include <config/typedef.h>

#include "trn/list.h"
#include "trn/enum-flags.h"

struct DataSource;
struct HashTable;
struct NewsgroupData;

enum : ArticleUnread
{
    TR_ONE = (ArticleUnread) 1,
    TR_NONE = (ArticleUnread) 0,
    TR_UNSUB = (ArticleUnread) -1, // keep this one as -1, some tests use >= TR_UNSUB
    TR_IGNORE = (ArticleUnread) -2,
    TR_BOGUS = (ArticleUnread) -3,
    TR_JUNK = (ArticleUnread) -4,
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
    RF_ADD_NEW_GROUPS = 0x0001,
    RF_ADD_GROUPS = 0x0002,
    RF_OPEN = 0x0100,
    RF_ACTIVE = 0x0200,
    RF_RC_CHANGED = 0x0400
};
DECLARE_FLAGS_ENUM(NewsrcFlags, int);

struct Newsrc
{
    Newsrc     *next;
    DataSource *data_source;
    char       *name;      // the name of the associated newsrc
    char       *old_name;  // the backup of the newsrc
    char       *new_name;  // our working newsrc file
    char       *info_name; // the time/size info file
    char       *lock_name; // the lock file we created
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
    Newsrc      *first;
    int          num;
    MultircFlags flags;
};

extern HashTable *g_newsrc_hash;
extern Multirc   *g_sel_page_mp;
extern Multirc   *g_sel_next_mp;
extern List      *g_multirc_list;       // a list of all MULTIRCs
extern Multirc   *g_multirc;            // the current MULTIRC
extern bool       g_paranoid;           // did we detect some inconsistency in .newsrc?
extern AddNewType g_add_new_by_default; //
extern bool       g_check_flag;         // -c
extern bool       g_suppress_cn;        // -s
extern int        g_countdown;          // how many lines to list before invoking -s
extern bool       g_fuzzy_get;          // -G
extern bool       g_append_unsub;       // -I

bool           rcstuff_init();
void           rcstuff_final();
Newsrc        *new_newsrc(const char *name, const char *newsrc, const char *add_ok);
bool           use_multirc(Multirc *mp);
void           unuse_multirc(Multirc *mptr);
bool           use_next_multirc(Multirc *mptr);
bool           use_prev_multirc(Multirc *mptr);
const char    *multirc_name(const Multirc *mp);
void           abandon_newsgroup(NewsgroupData *np);
bool           get_newsgroup(const char *what, GetNewsgroupFlags flags);
bool           relocate_newsgroup(NewsgroupData *move_np, NewsgroupNum new_num);
void           list_newsgroups();
NewsgroupData *find_newsgroup(const char *ngnam);
void           cleanup_newsrc(Newsrc *rp);
void           set_hash(NewsgroupData *np);
void           checkpoint_newsrcs();
bool           write_newsrcs(Multirc *mptr);
void           get_old_newsrcs(Multirc *mptr);

inline Multirc *multirc_ptr(long n)
{
    return (Multirc *) list_get_item(g_multirc_list, n);
}
inline Multirc *multirc_low()
{
    return (Multirc *) list_get_item(g_multirc_list, existing_list_index(g_multirc_list, 0L, 1));
}
inline Multirc *multirc_high()
{
    return (Multirc *) list_get_item(g_multirc_list, existing_list_index(g_multirc_list, g_multirc_list->high, -1));
}
inline Multirc *multirc_next(Multirc *p)
{
    return (Multirc *) next_list_item(g_multirc_list, (char *) p);
}
inline Multirc *multirc_prev(Multirc *p)
{
    return (Multirc *) prev_list_item(g_multirc_list, (char *) p);
}

#endif
