/* trn/rcstuff.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_RCSTUFF_H
#define TRN_RCSTUFF_H

#include <config/typedef.h>
#include <trn/enum-flags.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

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
    std::string name;      // the name of the associated newsrc
    std::string old_name;  // the backup of the newsrc
    std::string new_name;  // our working newsrc file
    std::string info_name; // the time/size info file
    std::string lock_name; // the lock file we created
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
    bool        use_multirc();
    bool        use_next_multirc();
    bool        use_prev_multirc();
    const char *multirc_name() const;

    Newsrc      *m_first;
    int          m_num;
    MultircFlags m_flags;
};

extern HashTable           *g_newsrc_hash;
extern Multirc             *g_sel_page_mp;
extern Multirc             *g_sel_next_mp;
extern std::vector<Multirc> g_multircs;           // all MULTIRCs
extern Multirc             *g_multirc;            // the current MULTIRC
extern bool                 g_paranoid;           // did we detect some inconsistency in .newsrc?
extern AddNewType           g_add_new_by_default; //
extern bool                 g_check_flag;         // -c
extern bool                 g_suppress_cn;        // -s
extern int                  g_countdown;          // how many lines to list before invoking -s
extern bool                 g_fuzzy_get;          // -G
extern bool                 g_append_unsub;       // -I

bool           rcstuff_init();
void           rcstuff_final();
void           unuse_multirc(Multirc *mptr);
bool           get_newsgroup(const char *what, GetNewsgroupFlags flags);
void           list_newsgroups();
NewsgroupData *find_newsgroup(std::string_view ngnam);
void           cleanup_newsrc(Newsrc *rp);
void           checkpoint_newsrcs();
bool           write_newsrcs(Multirc *mptr);
void           get_old_newsrcs(Multirc *mptr);

inline Multirc *multirc_ptr(long n)
{
    auto it = std::lower_bound(g_multircs.begin(), g_multircs.end(), n,
                               [](const Multirc &mp, long num) { return mp.m_num < num; });
    return it != g_multircs.end() && it->m_num == n ? &*it : nullptr;
}
inline Multirc *multirc_low()
{
    return g_multircs.empty() ? nullptr : &g_multircs.front();
}
inline Multirc *multirc_high()
{
    return g_multircs.empty() ? nullptr : &g_multircs.back();
}
inline Multirc *multirc_next(Multirc *p)
{
    if (!p)
    {
        return nullptr;
    }
    auto it = std::upper_bound(g_multircs.begin(), g_multircs.end(), p->m_num,
                               [](long num, const Multirc &mp) { return num < mp.m_num; });
    return it == g_multircs.end() ? nullptr : &*it;
}
inline Multirc *multirc_prev(Multirc *p)
{
    if (!p)
    {
        return nullptr;
    }
    auto it = std::lower_bound(g_multircs.begin(), g_multircs.end(), p->m_num,
                               [](const Multirc &mp, long num) { return mp.m_num < num; });
    if (it == g_multircs.begin())
    {
        return nullptr;
    }
    --it;
    return &*it;
}

#endif
