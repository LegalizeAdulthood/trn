// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#ifndef TEST_SUPPORT_NEWSGROUP_PAGE_STATE_H
#define TEST_SUPPORT_NEWSGROUP_PAGE_STATE_H

#include <trn/datasrc.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>
#include <trn/rt-page.h>
#include <trn/rthread.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <mock_env.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test_support
{

extern const std::array<std::string_view, 8> DISPLAY_GROUP_NAMES;

class NewsgroupPageStateTest : public testing::Test
{
protected:
    void                     SetUp() override;
    void                     TearDown() override;
    void                     set_read_scope(bool rereading);
    NewsgroupData           &add_group(std::string_view name, ArticleUnread unread, NewsgroupFlags flags = NF_NONE);
    void                     initialize_page();
    std::vector<std::string> groups_with_flag(NewsgroupFlags flag) const;

private:
    std::vector<NewsgroupData>   m_groups;
    std::vector<NewsgroupData *> m_old_order{std::move(g_newsgroup_order)};
    NewsgroupData               *m_old_first{g_first_newsgroup};
    NewsgroupData               *m_old_last{g_last_newsgroup};
    NewsgroupData               *m_old_page{g_sel_page.page_ngd};
    NewsgroupData               *m_old_next{g_sel_page.next_ngd};
    NewsgroupNum                 m_old_count{g_newsgroup_count};
    ArticleUnread                m_old_minimum{g_newsgroup_min_to_read};
    ArticleUnread                m_old_selected{g_selected_count};
    ArticleNum                   m_old_objects{g_obj_count};
    SelectionMode                m_old_mode{g_sel.mode};
    SelectionSortMode            m_old_sort{g_sel.sort};
    int                          m_old_direction{g_sel.direction};
    AddGroupFlags                m_old_mask{g_sel.mask};
    bool                         m_old_rereading{g_sel.rereading};
    bool                         m_old_exclusive{g_sel.exclusive};
    bool                         m_old_paranoid{g_paranoid};
    bool                         m_old_verbose{g_verbose};
    int                          m_old_total{g_sel_page.total_obj_cnt};
    int                          m_old_prior{g_sel_page.prior_obj_cnt};
    int                          m_old_page_objects{g_sel_page.page_obj_cnt};
    int                          m_old_page_items{g_sel_page.page_item_cnt};
    int                          m_old_item_index{g_sel_page.item_index};
    int                          m_old_term_line{g_term_line};
    int                          m_old_lines{g_tc_LINES};
    int                          m_old_columns{g_tc_COLS};
    int                          m_old_mouse_width{g_mouse_bar_width};
    bool                         m_old_use_sel_num{g_use_sel_num};
    std::string                  m_old_sel_chars{g_sel_page.chars};
    std::string                  m_old_message{g_msg};
};

class NewsgroupPageDisplayState : public NewsgroupPageStateTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        prepare_display(bool rereading, char mode, bool exclusive = false);
    void        cache_descriptions(std::optional<std::string_view> alpha);
    std::string render_page();
    std::string expected_rows(char mode, std::initializer_list<int> indices) const;
    void        expect_group_state() const;

    std::array<NewsgroupData *, 8> m_entries{};
    DataSource                     m_source{};
    Newsrc                         m_newsrc{};

private:
    Multirc                            m_multirc{};
    trn::testing::MockEnvironment      m_env;
    std::array<NewsgroupData, 8>       m_before{};
    std::array<SelectionItem, MAX_SEL> m_old_items{};
    Multirc                           *m_old_multirc{g_multirc};
    NewsgroupData                     *m_old_group{g_newsgroup_ptr};
    bool                               m_old_in_group{g_in_ng};
    bool                               m_old_group_listing{g_sel.group_listing};
    bool                               m_old_erase_screen{g_erase_screen};
    bool                               m_old_erase_each_line{g_erase_each_line};
    std::string                        m_old_display_mode{g_sel_grp_display_mode};
    std::size_t                        m_old_display_index{g_sel_grp_display_mode_index};
    int                                m_old_last_line{g_sel_page.last_line};
    bool                               m_old_at_end{g_sel_page.at_end};
    Article                           *m_old_last_article{g_sel_page.last_art};
    Subject                           *m_old_last_subject{g_sel_page.last_subj};
    int                                m_old_term_column{g_term_col};
    int                                m_old_scrolled{g_term_scrolled};
    int                                m_old_fire{g_fire_is_out};
    std::string_view                   m_old_home{g_tc_HO};
    std::string_view                   m_old_clear{g_tc_CD};
    std::string_view                   m_old_return{g_tc_CR};
    std::string_view                   m_old_standout{g_tc_SO};
    std::string_view                   m_old_end_standout{g_tc_SE};
};

} // namespace test_support

#endif
