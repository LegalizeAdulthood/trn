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
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <variant>
#include <vector>

using UniversalGroupVisitor = int (*)(std::string_view);
using UniversalInputPending = bool (*)();
using UniversalItemIndex = int;

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

struct UniversalVirtualArticle
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
    std::filesystem::path fname;
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
    UniversalVirtualArticle,
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
    UniversalNewsgroup &group();
    const UniversalNewsgroup &group() const;
    UniversalVirtualGroup &vgroup();
    const UniversalVirtualGroup &vgroup() const;
    UniversalVirtualArticle &article();
    const UniversalVirtualArticle &article() const;
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
    std::string       univ_article_desc() const;
    std::string_view               univ_key_help_mode_str() const;
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

extern bool g_univ_ng_virt_flag;   // if true, we are in the "virtual group" second pass
extern bool g_univ_read_virt_flag; // if true, we are reading an article from a "virtual group"
extern bool g_univ_default_cmd;    // "follow"-related stuff (virtual groups)
extern bool g_univ_follow;
extern bool g_univ_follow_temp;

struct UniversalMenuContext
{
    UniversalItem *first_item();
    UniversalItem *last_item();
    UniversalItem *next_item(const UniversalItem *item);
    UniversalItem *prev_item(const UniversalItem *item);
    UniversalItem *item(UniversalItemIndex item_index);
    void           sort();
    void           close();

    void add_virtual_group(UniversalVirtualGroup group);
    // Returns the continuation position if erased; retains remembered names.
    std::optional<std::size_t> erase(UniversalItemIndex item_index);

    // TODO: private
    void           open();
    std::size_t position(const UniversalItem *item);
    std::size_t position(UniversalItemIndex item_index);
    UniversalItem *add(UniversalData data, std::string_view desc);
    void           add_group(std::string_view desc, std::string_view grpname);

    UniversalItemList items;
    UniversalNameSet  ng_names;
    UniversalNameSet  vg_names;
    std::string       fname;    // current filename (may be empty)
    std::string       label;    // current label (may be empty)
    std::string       title;    // title of current level
    std::string       tmp_file; // temp. file (may be empty)
    int               level{};  // How deep are we in the tree?
    int               item_counter{1};
};

extern UniversalMenuContext g_univ;

struct UniversalMenuLoader
{
    // TODO: private
    void use_pattern(std::string_view pattern, int type);
    void use_group_line(std::string_view line, int type);
    void do_line_ext1(std::string_view desc, std::string_view line);
    bool do_line(std::string_view line);
    bool use_file(std::string_view fname, std::string_view label);
    bool include_file(std::string_view fname);

    bool        begin_found{};
    std::string begin_label; // label to start working with
    std::string line_desc;   // description (printing name) of the entry
};

extern UniversalMenuLoader g_univ_loader;

struct VirtualGroupBuilder
{
    void virt_pass(UniversalGroupVisitor visit_group, UniversalInputPending input_is_pending);
    void virt_pass(UniversalGroupVisitor visit_group);
    void virt_pass();

    // TODO: private
    void add_virtual_group(std::string_view grpname);
    void vg_add_article(ArticleNum a);
    void vg_add_group();

    bool               virt_pass_needed{};
    UniversalItemIndex current_vg_ui_index{};
    int                min_score{}; // this score is part of the line format, so it is not ifdefed
    bool               use_min_score{};
};

extern VirtualGroupBuilder g_vg_builder;

struct UniversalConfiguration
{
    void startup();

    // TODO: private
    std::string edit_new_user_file();

    bool done_startup{};
    bool user_top{}; // if true, the user has loaded their own top univ. config file
};

extern UniversalConfiguration g_univ_config;

void           univ_init();
bool           univ_file_load(std::string_view fname, std::string_view title, std::string_view label);
void           univ_mask_load(std::string_view mask, std::string_view title);
void           univ_redo_file();
void           univ_edit();
void           univ_page_file(const std::filesystem::path &fname);
void           univ_newsgroup_virtual();
int            univ_visit_group_main(std::string_view gname);
UniversalItems univ_items();
UniversalItems     univ_items(UniversalItemIndex first);
UniversalItems     univ_items(UniversalItem *first);
UniversalItemIndex univ_index(const UniversalItem *item);
void           univ_help_main(HelpLocation where);
void           univ_help(HelpLocation where);

#endif
