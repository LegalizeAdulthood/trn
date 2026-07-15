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

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

using UniversalGroupVisitor = int (*)(const char *);
using UniversalItemIndex = int;

enum UniversalItemType
{
    UN_NONE = 0,          //
    UN_TXT = 1,           // textual placeholder
    UN_NEWSGROUP = 3,     //
    UN_GROUP_MASK = 4,    //
    UN_ARTICLE = 5,       // an individual article
    UN_CONFIG_FILE = 6,   // filename for a configuration file
    UN_VGROUP = 8,        // virtual newsgroup marker (for pass 2)
    UN_TEXT_FILE = 9,     // text file
    UN_HELP_KEY = 10,     // keystroke help functions from help.cpp
    UN_DEBUG1 = -1        // quick debugging: just has data
};

enum UniversalItemState
{
    UIS_NORMAL,
    UIS_DESELECTED
};

struct UniversalGroupMaskData
{
    std::string title;
    std::string mask_list;
};

struct UniversalConfigFileData
{
    std::string title;
    std::string fname;
    std::string label;
};

struct UniversalVirtualData
{
    std::string ng;
    std::string id;
    std::string from;
    std::string subj;
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
    std::string       ng;
    int               min_score;
    int               max_score;
    VirtualGroupFlags flags;
};

struct UniversalNewsgroup
{
    std::string ng;
};

struct UniversalTextFile
{
    std::string fname;
};

struct UniversalDebugData
{
    std::string text;
};

struct UniversalNoData
{
};

struct UniversalTextPlaceholder
{
};

using UniversalData = std::variant<UniversalNoData,
    UniversalTextPlaceholder,
    UniversalNewsgroup,
    UniversalGroupMaskData,
    UniversalVirtualData,
    UniversalConfigFileData,
    UniversalVirtualGroup,
    UniversalTextFile,
    HelpLocation,
    UniversalDebugData>;

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
    int                m_num{};             // natural order (for sort)
    UniversalItemFlags m_flags{UF_NONE};    // for selector
    UniversalItemState m_state{UIS_NORMAL}; // current selector state
    std::string        m_desc;              // default description
    int                m_score{};
    UniversalData      m_data{UniversalNoData{}}; // describes the object
    UniversalItemType  type() const;
    UniversalNewsgroup &group();
    const UniversalNewsgroup &group() const;
    UniversalVirtualGroup &vgroup();
    const UniversalVirtualGroup &vgroup() const;
    UniversalVirtualData &article();
    const UniversalVirtualData &article() const;
    UniversalConfigFileData &config_file();
    const UniversalConfigFileData &config_file() const;
    UniversalGroupMaskData &group_mask();
    const UniversalGroupMaskData &group_mask() const;
    UniversalTextFile &text_file();
    const UniversalTextFile &text_file() const;
    std::string &debug_string();
    const std::string &debug_string() const;
    HelpLocation &help_location();
    HelpLocation help_location() const;
    const char        *univ_article_desc() const;
    const char        *univ_key_help_mode_str() const;
};

class UniversalItemIterator
{
public:
    explicit UniversalItemIterator(UniversalItem *item);
    UniversalItem         &operator*() const;
    UniversalItem         *operator->() const;
    UniversalItemIterator &operator++();
    bool                   operator==(const UniversalItemIterator &other) const;
    bool                   operator!=(const UniversalItemIterator &other) const;

private:
    std::size_t m_index;
};

class UniversalItems
{
public:
    explicit UniversalItems(UniversalItem *first);
    UniversalItemIterator begin() const;
    UniversalItemIterator end() const;

private:
    std::size_t m_first;
};

using UniversalItemList = std::vector<UniversalItem>;
using UniversalNameSet = std::unordered_set<std::string>;

extern int  g_univ_level;          // How deep are we in the tree?
extern bool g_univ_ng_virt_flag;   // if true, we are in the "virtual group" second pass
extern bool g_univ_read_virt_flag; // if true, we are reading an article from a "virtual group"
extern bool g_univ_default_cmd;    // "follow"-related stuff (virtual groups)
extern bool g_univ_follow;
extern bool g_univ_follow_temp;

// items which must be saved in context
extern UniversalItemList  g_univ_items;
extern UniversalNameSet   g_univ_ng_names;
extern UniversalNameSet   g_univ_vg_names;
extern UniversalItemIndex g_sel_page_univ_index;
extern UniversalItemIndex g_sel_next_univ_index;
extern std::string    g_univ_fname;    // current filename (may be empty)
extern std::string    g_univ_label;    // current label (may be empty)
extern std::string    g_univ_title;    // title of current level
extern std::string    g_univ_tmp_file; // temp. file (may be empty)
// end of items that must be saved

void           univ_init();
void           univ_startup();
void           univ_close();
bool           univ_file_load(const char *fname, const char *title, const char *label);
void           univ_mask_load(std::string_view mask, const char *title);
void           univ_redo_file();
void           univ_edit();
void           univ_page_file(std::string_view fname);
void           univ_newsgroup_virtual();
int            univ_visit_group_main(std::string_view gname);
void           univ_virt_pass(UniversalGroupVisitor visit_group);
void           univ_virt_pass();
void           sort_univ();
UniversalItems univ_items();
UniversalItems     univ_items(UniversalItemIndex first);
UniversalItems     univ_items(UniversalItem *first);
UniversalItem     *univ_first_item();
UniversalItem     *univ_last_item();
UniversalItem     *univ_next_item(const UniversalItem *item);
UniversalItem     *univ_prev_item(const UniversalItem *item);
UniversalItem     *univ_item(UniversalItemIndex item_index);
UniversalItemIndex univ_index(const UniversalItem *item);
void           univ_help_main(HelpLocation where);
void           univ_help(HelpLocation where);

#endif
