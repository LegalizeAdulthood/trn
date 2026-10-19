/* trn/addng.h
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#ifndef TRN_ADDNG_H
#define TRN_ADDNG_H

#include <config/typedef.h>
#include <trn/enum-flags.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct DataSource;

enum AddGroupFlags : std::uint8_t
{
    AGF_NONE = 0x00,
    AGF_SEL = 0x01,
    AGF_DEL = 0x02,
    AGF_DEL_SEL = 0x04,
    AGF_INCLUDED = 0x10,
    AGF_EXCLUDED = 0x20
};
DECLARE_FLAGS_ENUM(AddGroupFlags, std::uint8_t);

struct AddGroup
{
    DataSource   *m_data_src;
    ArticleNum    m_to_read; // number of articles to be read (for sorting)
    NewsgroupNum  m_num;     // a possible sort order for this group
    AddGroupFlags m_flags;
    std::string   m_name;
    int           add_group_perform(std::string_view cmdlst, int output_level);
};

// Zero is the sentinel; other values identify insertion slots until clear().
enum class AddGroupIndex : std::size_t
{
    None
};

class AddGroupTable
{
public:
    AddGroupIndex add(std::string_view name, int to_read, int subscription, DataSource *source);
    void          clear();
    void          sort(int (*compare)(const AddGroup *, const AddGroup *));
    bool          empty() const
    {
        return m_groups.empty();
    }
    std::size_t size() const
    {
        return m_groups.size();
    }
    AddGroup &operator[](AddGroupIndex index)
    {
        return m_groups[static_cast<std::size_t>(index) - 1];
    }
    const AddGroup &operator[](AddGroupIndex index) const
    {
        return m_groups[static_cast<std::size_t>(index) - 1];
    }
    const std::vector<AddGroupIndex> &indices() const
    {
        return m_order;
    }
    AddGroupIndex first() const;
    AddGroupIndex last() const;
    AddGroupIndex next(AddGroupIndex index) const;
    AddGroupIndex prev(AddGroupIndex index) const;

private:
    std::vector<AddGroup>      m_groups;
    std::vector<AddGroupIndex> m_order;
    std::vector<std::size_t>   m_positions;
    std::vector<AddGroupIndex> m_names;
};

extern AddGroupTable g_add_groups;
extern AddGroupIndex g_sel_page_gp;
extern AddGroupIndex g_sel_next_gp;
extern bool      g_quick_start;      // -q
extern bool      g_use_add_selector; //

void add_ng_init();
bool find_new_groups();
bool scan_active(bool add_matching);
void sort_add_groups();

#endif
