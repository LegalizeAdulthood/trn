// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "NewsgroupPageState.h"

#include <trn/only.h>
#include <trn/search.h>

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace test_support
{

const std::array<std::string_view, 8> DISPLAY_GROUP_NAMES{"comp.alpha", "alt.hidden.with.a.long.name",
                                                          "comp.beta",  "comp.opposite",
                                                          "comp.gamma", "comp.invalid",
                                                          "comp.delta", "comp.next"};

void NewsgroupPageStateTest::SetUp()
{
    m_groups.reserve(8);
    g_newsgroup_order.clear();
    g_first_newsgroup = nullptr;
    g_last_newsgroup = nullptr;
    g_sel_page.page_ngd = nullptr;
    g_sel_page.next_ngd = nullptr;
    g_newsgroup_count = NewsgroupNum{};
    g_newsgroup_min_to_read = TR_ONE;
    g_sel.mode = SM_NEWSGROUP;
    g_sel.sort = SS_NATURAL;
    g_sel.direction = 1;
    g_sel.exclusive = false;
    g_paranoid = false;
    g_verbose = false;
    g_tc_LINES = 24;
    g_tc_COLS = 80;
    g_mouse_bar_width = 0;
    g_use_sel_num = false;
    set_read_scope(false);
    search_init();
    push_only();
}

void NewsgroupPageStateTest::TearDown()
{
    pop_only();
    g_newsgroup_order = std::move(m_old_order);
    g_first_newsgroup = m_old_first;
    g_last_newsgroup = m_old_last;
    g_sel_page.page_ngd = m_old_page;
    g_sel_page.next_ngd = m_old_next;
    g_newsgroup_count = m_old_count;
    g_newsgroup_min_to_read = m_old_minimum;
    g_selected_count = m_old_selected;
    g_obj_count = m_old_objects;
    g_sel.mode = m_old_mode;
    g_sel.sort = m_old_sort;
    g_sel.direction = m_old_direction;
    g_sel.mask = m_old_mask;
    g_sel.rereading = m_old_rereading;
    g_sel.exclusive = m_old_exclusive;
    g_paranoid = m_old_paranoid;
    g_verbose = m_old_verbose;
    g_sel_page.total_obj_cnt = m_old_total;
    g_sel_page.prior_obj_cnt = m_old_prior;
    g_sel_page.page_obj_cnt = m_old_page_objects;
    g_sel_page.page_item_cnt = m_old_page_items;
    g_sel_page.item_index = m_old_item_index;
    g_term_line = m_old_term_line;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    g_mouse_bar_width = m_old_mouse_width;
    g_use_sel_num = m_old_use_sel_num;
    g_sel_page.chars = m_old_sel_chars;
    g_msg = m_old_message;
}

void NewsgroupPageStateTest::set_read_scope(bool rereading)
{
    g_sel.rereading = rereading;
    g_sel.mask = rereading ? AGF_DEL_SEL : AGF_SEL;
}

NewsgroupData &NewsgroupPageStateTest::add_group(std::string_view name, ArticleUnread unread, NewsgroupFlags flags)
{
    EXPECT_LT(m_groups.size(), m_groups.capacity());
    NewsgroupData &group = m_groups.emplace_back();
    group.m_rc_line = std::string{name} + ": ";
    group.m_num_offset = static_cast<int>(name.size()) + 1;
    group.m_subscribe_char = ':';
    group.hide_subscribe_char();
    group.m_abs_first = ArticleNum{1};
    group.m_to_read = unread;
    group.m_flags = flags;
    append_newsgroup_order(&group);
    ++g_newsgroup_count;
    return group;
}

void NewsgroupPageStateTest::initialize_page()
{
    init_pages(PRESERVE_PAGE);
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
}

std::vector<std::string> NewsgroupPageStateTest::groups_with_flag(NewsgroupFlags flag) const
{
    std::vector<std::string> names;
    for (const NewsgroupData *group : g_newsgroup_order)
    {
        if (group->m_flags & flag)
        {
            names.emplace_back(group->rc_name());
        }
    }
    return names;
}

void NewsgroupPageDisplayState::SetUp()
{
    NewsgroupPageStateTest::SetUp();
    std::copy(std::begin(g_sel_page.items), std::end(g_sel_page.items), m_old_items.begin());
    m_env.expect_env_repeatedly("SELECTCHARS", "abcdef");
    m_source.m_name = "default";
    m_source.m_group_desc = "cached-newsgroups";
    m_newsrc.data_source = &m_source;
    m_newsrc.flags = RF_ACTIVE;
    m_multirc.m_first = &m_newsrc;
    g_multirc = &m_multirc;
    g_in_ng = false;
    g_sel.group_listing = false;
    g_erase_screen = false;
    g_erase_each_line = false;
    g_tc_HO = "<home>";
    g_tc_CD = "<clear>";
    g_tc_CR = "";
    g_tc_SO = "";
    g_tc_SE = "";
    g_tc_LINES = 9;
    g_sel_page.last_art = nullptr;
    g_sel_page.last_subj = nullptr;
}

void NewsgroupPageDisplayState::TearDown()
{
    m_source.m_desc_sf.close();
    std::copy(m_old_items.begin(), m_old_items.end(), std::begin(g_sel_page.items));
    g_multirc = m_old_multirc;
    g_newsgroup_ptr = m_old_group;
    g_in_ng = m_old_in_group;
    g_sel.group_listing = m_old_group_listing;
    g_erase_screen = m_old_erase_screen;
    g_erase_each_line = m_old_erase_each_line;
    g_sel_grp_display_mode = m_old_display_mode;
    g_sel_grp_display_mode_index = m_old_display_index;
    g_sel_page.last_line = m_old_last_line;
    g_sel_page.at_end = m_old_at_end;
    g_sel_page.last_art = m_old_last_article;
    g_sel_page.last_subj = m_old_last_subject;
    g_term_col = m_old_term_column;
    g_term_scrolled = m_old_scrolled;
    g_fire_is_out = m_old_fire;
    g_tc_HO = m_old_home;
    g_tc_CD = m_old_clear;
    g_tc_CR = m_old_return;
    g_tc_SO = m_old_standout;
    g_tc_SE = m_old_end_standout;
    NewsgroupPageStateTest::TearDown();
}

void NewsgroupPageDisplayState::prepare_display(bool rereading, char mode, bool exclusive)
{
    set_read_scope(rereading);
    const NewsgroupFlags                selected = static_cast<NewsgroupFlags>(g_sel.mask);
    const NewsgroupFlags                other_selection = rereading ? NF_SEL : NF_DEL_SEL;
    const std::array<ArticleUnread, 8>  unread{3, 4, 12, 0, 7, TR_UNSUB, 1, 9};
    const std::array<NewsgroupFlags, 8> flags{selected, selected, other_selection, other_selection,
                                              NF_NONE,  selected, selected,        NF_NONE};
    for (std::size_t index = 0; index < m_entries.size(); ++index)
    {
        const ArticleUnread count =
            index == 3 ? (rereading ? 7 : 0) : (rereading && unread[index] >= TR_NONE ? 0 : unread[index]);
        NewsgroupData &group = add_group(DISPLAY_GROUP_NAMES[index], count, flags[index]);
        group.m_rc = &m_newsrc;
        group.m_rc_line += "1-3";
        m_entries[index] = &group;
    }
    set_newsgroup_to_do("comp.*");
    g_sel.exclusive = exclusive;
    g_sel_grp_display_mode.assign(1, mode);
    g_sel_grp_display_mode_index = 0;
    initialize_page();
    m_entries[4]->m_flags |= NF_DEL;
    m_entries[6]->m_flags |= NF_DEL;
    g_newsgroup_ptr = m_entries[exclusive ? 6 : 4];
    g_sel_page.item_index = -1;
    cache_descriptions("Alpha discussion\n");
    for (std::size_t index = 0; index < m_entries.size(); ++index)
    {
        m_before[index] = *m_entries[index];
    }
}

void NewsgroupPageDisplayState::cache_descriptions(std::optional<std::string_view> alpha)
{
    m_source.m_desc_sf.close();
    ASSERT_EQ(1, m_source.m_desc_sf.open({}, "", ""));
    if (alpha)
    {
        (void) m_source.m_desc_sf.append("comp.alpha " + std::string{*alpha}, 10);
    }
    (void) m_source.m_desc_sf.append("comp.beta Beta discussion\n", 9);
    (void) m_source.m_desc_sf.append("comp.gamma Gamma discussion\n", 10);
    (void) m_source.m_desc_sf.append("comp.delta Delta discussion\n", 10);
    (void) m_source.m_desc_sf.append("comp.next Next page discussion\n", 9);
}

std::string NewsgroupPageDisplayState::render_page()
{
    testing::internal::CaptureStdout();
    display_page();
    return testing::internal::GetCapturedStdout();
}

std::string NewsgroupPageDisplayState::expected_rows(char mode, std::initializer_list<int> indices) const
{
    const std::array<std::string_view, 8> descriptions{
        "Alpha discussion", "", "Beta discussion",  "",
        "Gamma discussion", "", "Delta discussion", "Next page discussion"};
    std::ostringstream rows;
    char               label = 'a';
    for (const int index : indices)
    {
        const char mark = index == 0 ? '+' : index == 6 ? '*' : index == 4 || g_sel.rereading ? '-' : ' ';
        rows << label++ << mark << std::setw(5) << static_cast<long>(m_before[index].m_to_read) << ' ';
        if (mode == 's')
        {
            rows << DISPLAY_GROUP_NAMES[index];
        }
        else if (mode == 'm')
        {
            rows << descriptions[index];
        }
        else
        {
            rows << std::left << std::setw(12) << DISPLAY_GROUP_NAMES[index] << std::right << descriptions[index];
        }
        rows << '\n';
    }
    return rows.str();
}

void NewsgroupPageDisplayState::expect_group_state() const
{
    for (std::size_t index = 0; index < m_entries.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(m_before[index].m_flags, m_entries[index]->m_flags);
        EXPECT_EQ(m_before[index].m_to_read, m_entries[index]->m_to_read);
        EXPECT_EQ(m_before[index].m_rc_line, m_entries[index]->m_rc_line);
        EXPECT_EQ(m_before[index].m_abs_first, m_entries[index]->m_abs_first);
        EXPECT_EQ(&m_newsrc, m_entries[index]->m_rc);
    }
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(RF_ACTIVE, m_newsrc.flags);
}

} // namespace test_support
