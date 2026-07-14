/* trn/univ.h
 */
/* Universal selector
 *
 */
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_UNIV_H
#define TRN_UNIV_H

#include <config/typedef.h>
#include <trn/enum-flags.h>
#include <trn/help.h>

#include <cstdint>
#include <string>
#include <string_view>

struct HashTable;

using UniversalGroupVisitor = int (*)(const char *);

enum UniversalItemType
{
    UN_NONE = 0,          //
    UN_TXT = 1,           // textual placeholder
    UN_DATA_SOURCE = 2,   //
    UN_NEWSGROUP = 3,     //
    UN_GROUP_MASK = 4,    //
    UN_ARTICLE = 5,       // an individual article
    UN_CONFIG_FILE = 6,   // filename for a configuration file
    UN_VIRTUAL1 = 7,      // Virtual newsgroup file (reserved for compatibility with strn)
    UN_VGROUP = 8,        // virtual newsgroup marker (for pass 2)
    UN_TEXT_FILE = 9,     // text file
    UN_HELP_KEY = 10,     // keystroke help functions from help.cpp
    UN_DEBUG1 = -1        // quick debugging: just has data
};

enum UniversalItemState
{
    UIS_NORMAL,
    UIS_DESELECTED,
    UIS_DELETED
};

struct UniversalGroupMaskData
{
    char *title;
    char *mask_list;
};

struct UniversalConfigFileData
{
    char *title;
    char *fname;
    char *label;
};

struct UniversalVirtualData
{
    char      *ng;
    char      *id;
    char      *from;
    char      *subj;
    ArticleNum num;
};

// virtual/merged group flags (UNIV_VIRT_GROUP.flags)
enum VirtualGroupFlags : std::uint8_t
{
    UF_VG_NONE = 0x00,
    UF_VG_MIN_SCORE = 0x01, // articles use minimum score
    UF_VG_MAX_SCORE = 0x02  // articles use maximum score
};
DECLARE_FLAGS_ENUM(VirtualGroupFlags, std::uint8_t);

struct UniversalVirtualGroup
{
    char             *ng;
    int               min_score;
    int               max_score;
    VirtualGroupFlags flags;
};

struct UniversalNewsgroup
{
    char *ng;
};

struct UniversalTextFile
{
    char *fname;
};

union UniversalData
{
    char                   *str;
    HelpLocation            i;
    UniversalGroupMaskData  gmask;
    UniversalConfigFileData cfile;
    UniversalNewsgroup      group;
    UniversalVirtualData    virt;
    UniversalVirtualGroup   vgroup;
    UniversalTextFile       text_file;
};

// selector flags
enum UniversalItemFlags
{
    UF_NONE = 0x00,
    UF_SEL = 0x01,
    UF_DEL = 0x02,
    UF_DEL_SEL = 0x04,
    UF_INCLUDED = 0x10,
    UF_EXCLUDED = 0x20
};
DECLARE_FLAGS_ENUM(UniversalItemFlags, int);

struct UniversalItem
{
    UniversalItem     *m_next;
    UniversalItem     *m_prev;
    int                m_num;   // natural order (for sort)
    UniversalItemFlags m_flags; // for selector
    UniversalItemType  m_type;  // what kind of object is it?
    UniversalItemState m_state; // current selector state
    char              *m_desc;  // default description
    int                m_score;
    UniversalData      m_data; // describes the object
    UniversalNewsgroup &group();
    const UniversalNewsgroup &group() const;
    UniversalVirtualGroup &vgroup();
    const UniversalVirtualGroup &vgroup() const;
    UniversalVirtualData &article();
    const UniversalVirtualData &article() const;
    UniversalConfigFileData &config_file();
    const UniversalConfigFileData &config_file() const;
    const char        *univ_article_desc() const;
    const char        *univ_key_help_mode_str() const;
};

extern int  g_univ_level;          // How deep are we in the tree?
extern bool g_univ_ng_virt_flag;   // if true, we are in the "virtual group" second pass
extern bool g_univ_read_virt_flag; // if true, we are reading an article from a "virtual group"
extern bool g_univ_default_cmd;    // "follow"-related stuff (virtual groups)
extern bool g_univ_follow;
extern bool g_univ_follow_temp;

// items which must be saved in context
extern UniversalItem *g_first_univ;
extern UniversalItem *g_last_univ;
extern UniversalItem *g_sel_page_univ;
extern UniversalItem *g_sel_next_univ;
extern std::string    g_univ_fname;    // current filename (may be empty)
extern std::string    g_univ_label;    // current label (may be empty)
extern std::string    g_univ_title;    // title of current level
extern std::string    g_univ_tmp_file; // temp. file (may be empty)
extern HashTable     *g_univ_ng_hash;
extern HashTable     *g_univ_vg_hash;
// end of items that must be saved

void           univ_init();
void           univ_startup();
void           univ_close();
bool           univ_file_load(const char *fname, const char *title, const char *label);
void           univ_mask_load(std::string_view mask, const char *title);
void           univ_redo_file();
void           univ_edit();
void           univ_page_file(char *fname);
void           univ_newsgroup_virtual();
int            univ_visit_group_main(std::string_view gname);
void           univ_virt_pass(UniversalGroupVisitor visit_group);
void           univ_virt_pass();
void           sort_univ();
void           univ_help_main(HelpLocation where);
void           univ_help(HelpLocation where);

#endif
