// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "MockNNTPConnection.h"
#include "support/NewsgroupPageState.h"

#include <nntp/nntpinit.h>
#include <trn/hash.h>
#include <trn/smisc.h>
#include <trn/univ.h>

#include <array>
#include <cerrno>
#include <memory>
#include <string>
#include <tuple>

namespace
{

using test_support::NewsgroupPageDisplayState;

class DeferredNewsgroupDisplayState : public NewsgroupPageDisplayState
{
protected:
    void           SetUp() override;
    void           TearDown() override;
    NewsgroupData &add_pending(std::string_view name, std::string_view numbers, bool exists = true);
    void           prepare_page(bool rereading, char mode);

private:
    NewsgroupNum     m_old_to_read{g_newsgroup_to_read};
    ArticleUnread    m_old_missing{g_missing_count};
    std::string      m_old_moderated{g_moderated};
    bool             m_old_redirected{g_redirected};
    std::string      m_old_redirected_to{g_redirected_to};
    std::string_view m_old_clear_eol{g_tc_CE};
};

void DeferredNewsgroupDisplayState::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    g_newsgroup_to_read = NewsgroupNum{};
    g_missing_count = 0;
    g_redirected = false;
    g_tc_CE = "";
    m_source.m_flags = DF_REMOTE | DF_NO_XGTITLE;
    m_source.m_group_desc.clear();
    m_source.m_act_sf.m_refetch_secs = 1;
    ASSERT_EQ(1, m_source.m_act_sf.open({}, "", ""));
}

void DeferredNewsgroupDisplayState::TearDown()
{
    m_source.m_act_sf.close();
    g_newsgroup_to_read = m_old_to_read;
    g_missing_count = m_old_missing;
    g_moderated = m_old_moderated;
    g_redirected = m_old_redirected;
    g_redirected_to = m_old_redirected_to;
    g_tc_CE = m_old_clear_eol;
    NewsgroupPageDisplayState::TearDown();
}

NewsgroupData &DeferredNewsgroupDisplayState::add_pending(std::string_view name, std::string_view numbers, bool exists)
{
    NewsgroupData &group = add_group(name, TR_NONE);
    group.m_rc = &m_newsrc;
    group.m_abs_first = ArticleNum{};
    group.m_rc_line.append(numbers);
    if (exists)
    {
        (void) m_source.m_act_sf.append(std::string{name} + " 10 1 y\n", static_cast<int>(name.size()));
    }
    ++g_newsgroup_to_read;
    return group;
}

void DeferredNewsgroupDisplayState::prepare_page(bool rereading, char mode)
{
    set_read_scope(rereading);
    g_sel_grp_display_mode.assign(1, mode);
    g_sel_grp_display_mode_index = 0;
    initialize_page();
}

class DeferredNewsgroupDisplayTest : public DeferredNewsgroupDisplayState,
                                     public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class DeferredNewsgroupRecoveryState : public DeferredNewsgroupDisplayState
{
protected:
    void        prepare_recovery(bool rereading, char mode, bool numeric, int capacity);
    void        expect_refreshed_state(bool rereading) const;
    void        expect_row(int row, NewsgroupData *group, int selected) const;
    std::string expected_row(int row, const NewsgroupData &group, int selected) const;

    std::vector<NewsgroupData *>   m_retained;
    std::array<NewsgroupData *, 2> m_removed{};
};

void DeferredNewsgroupRecoveryState::prepare_recovery(bool rereading, char mode, bool numeric, int capacity)
{
    const std::array<std::string_view, 3> names{"comp.first", "comp.middle", "comp.last"};
    const std::string                     numbers = rereading ? "1-10" : "1-7";
    set_read_scope(rereading);
    g_use_sel_num = numeric;
    g_tc_LINES = capacity + 5;
    for (int i = 0; i <= capacity; ++i)
    {
        m_retained.push_back(&add_pending(names[i], numbers));
    }
    m_retained.back()->m_flags = static_cast<NewsgroupFlags>(g_sel.mask);
    m_removed[0] = &add_pending("comp.opposite.first", rereading ? "1-7" : "1-10");
    m_removed[1] = &add_pending("comp.opposite.last", rereading ? "1-7" : "1-10");
    prepare_page(rereading, mode);
    (void) render_page();
    ASSERT_EQ(capacity, g_sel_page_item_cnt);
    ASSERT_EQ(ArticleNum{}, m_removed[0]->m_abs_first);
    ASSERT_EQ(ArticleNum{}, m_removed[1]->m_abs_first);
    ASSERT_TRUE(last_page());
    ASSERT_EQ(m_removed[capacity == 1 ? 1 : 0], g_sel_page_np);
    g_newsgroup_ptr = m_retained.back();
}

void DeferredNewsgroupRecoveryState::expect_refreshed_state(bool rereading) const
{
    const int retained = static_cast<int>(m_retained.size());
    EXPECT_EQ(retained, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{retained}, g_obj_count);
    EXPECT_EQ(NewsgroupNum{rereading ? 2 : retained}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? retained : 2, g_missing_count);
    EXPECT_EQ(1, g_selected_count);
    for (std::size_t i = 0; i < m_retained.size(); ++i)
    {
        const NewsgroupData &group = *m_retained[i];
        const bool           selected = i + 1 == m_retained.size();
        const NewsgroupFlags mark = selected ? static_cast<NewsgroupFlags>(g_sel.mask) : rereading ? NF_DEL : NF_NONE;
        EXPECT_EQ(NF_INCLUDED | mark, group.m_flags);
        EXPECT_EQ(rereading ? 0 : 3, group.m_to_read);
        EXPECT_EQ(rereading ? " 1-10" : " 1-7", group.rc_numbers());
    }
    for (const NewsgroupData *group : m_removed)
    {
        EXPECT_FALSE(group->m_flags & NF_INCLUDED);
        EXPECT_EQ(rereading ? 3 : 0, group->m_to_read);
        EXPECT_EQ(rereading ? " 1-7" : " 1-10", group->rc_numbers());
        EXPECT_EQ(ArticleNum{1}, group->m_abs_first);
        EXPECT_EQ(ArticleNum{10}, group->m_ng_max);
    }
    EXPECT_EQ(RF_ACTIVE, m_newsrc.flags);
    EXPECT_FALSE(g_paranoid);
}

void DeferredNewsgroupRecoveryState::expect_row(int row, NewsgroupData *group, int selected) const
{
    EXPECT_EQ(group, g_sel_items[row].u.np);
    EXPECT_EQ(row + 2, g_sel_items[row].line);
    EXPECT_EQ(selected, g_sel_items[row].sel);
}

std::string DeferredNewsgroupRecoveryState::expected_row(int row, const NewsgroupData &group, int selected) const
{
    const std::string label = g_use_sel_num ? "0" + std::to_string(row + 1) : std::string(1, "ab"[row]);
    return label + " +-*"[selected] + (g_sel.rereading ? "    0 " : "    3 ") + std::string{group.rc_name()} + "\n";
}

class DeferredNewsgroupRecoveryTest : public DeferredNewsgroupRecoveryState,
                                      public testing::WithParamInterface<std::tuple<bool, char, bool, int>>
{
protected:
    void SetUp() override;
};

void DeferredNewsgroupRecoveryTest::SetUp()
{
    DeferredNewsgroupRecoveryState::SetUp();
    const auto [rereading, mode, numeric, capacity] = GetParam();
    prepare_recovery(rereading, mode, numeric, capacity);
}

class DeferredNewsgroupRecoveryNavigationTest : public DeferredNewsgroupRecoveryState,
                                                public testing::WithParamInterface<std::tuple<bool, int>>
{
protected:
    void SetUp() override;
};

void DeferredNewsgroupRecoveryNavigationTest::SetUp()
{
    DeferredNewsgroupRecoveryState::SetUp();
    const auto [rereading, capacity] = GetParam();
    prepare_recovery(rereading, 's', false, capacity);
}

class DeferredNewsgroupCleanupTest : public DeferredNewsgroupDisplayState,
                                     public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    HashTable        *m_old_hash{g_newsrc_hash};
    NewsgroupData    *m_old_current{g_current_newsgroup};
    NewsgroupData    *m_old_recent{g_recent_newsgroup};
    NewsgroupData    *m_old_start{g_start_here};
    SelectionSortMode m_old_group_sort{g_sel.newsgroup_sort};
    bool              m_old_verify{g_verify};
    bool              m_old_default{g_s_default_cmd};
    bool              m_old_universal_default{g_univ_default_cmd};
    GeneralMode       m_old_general_mode{g_general_mode};
    MinorMode         m_old_mode{g_mode};
    int               m_old_errno{errno};
};

void DeferredNewsgroupCleanupTest::SetUp()
{
    DeferredNewsgroupDisplayState::SetUp();
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    g_newsrc_hash = hash_create(17, nullptr);
    g_current_newsgroup = nullptr;
    g_recent_newsgroup = nullptr;
    g_start_here = nullptr;
    g_sel.newsgroup_sort = SS_NATURAL;
    g_verify = false;
    errno = 0;
}

void DeferredNewsgroupCleanupTest::TearDown()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    hash_destroy(g_newsrc_hash);
    g_newsrc_hash = m_old_hash;
    g_current_newsgroup = m_old_current;
    g_recent_newsgroup = m_old_recent;
    g_start_here = m_old_start;
    g_sel.newsgroup_sort = m_old_group_sort;
    g_verify = m_old_verify;
    g_s_default_cmd = m_old_default;
    g_univ_default_cmd = m_old_universal_default;
    g_general_mode = m_old_general_mode;
    g_mode = m_old_mode;
    errno = m_old_errno;
    DeferredNewsgroupDisplayState::TearDown();
}

class DeferredNewsgroupDescriptionTest : public DeferredNewsgroupDisplayState,
                                         public testing::WithParamInterface<std::tuple<bool, char>>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_description(std::string_view name, std::string_view description);

    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;

private:
    DataSource *m_old_source{g_data_source};
    NNTPLink    m_old_link{g_nntp_link};
};

void DeferredNewsgroupDescriptionTest::SetUp()
{
    DeferredNewsgroupDisplayState::SetUp();
    g_data_source = nullptr;
    nntp_gets_clear_buffer();
    m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    m_source.m_flags = DF_REMOTE;
    m_source.m_group_desc = "cached-newsgroups";
    ASSERT_EQ(1, m_source.m_desc_sf.open({}, "", ""));
    m_source.m_nntp_link.connection = m_connection;
    m_source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
    m_source.m_nntp_link.capabilities.available = true;
    m_source.m_nntp_link.capabilities.add("LIST ACTIVE NEWSGROUPS");
}

void DeferredNewsgroupDescriptionTest::TearDown()
{
    g_data_source = m_old_source;
    g_nntp_link = m_old_link;
    nntp_gets_clear_buffer();
    DeferredNewsgroupDisplayState::TearDown();
}

void DeferredNewsgroupDescriptionTest::expect_description(std::string_view name, std::string_view description)
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST NEWSGROUPS " + std::string{name}), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return(std::string{name} + " " + std::string{description}))
        .WillOnce(testing::Return("."));
}

} // namespace

TEST_P(DeferredNewsgroupDisplayTest, refreshesPendingCountsAndReplacesTheApproximateHeading)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    NewsgroupData    &first = add_pending("comp.first", numbers);
    NewsgroupData    &second = add_pending("comp.second", numbers);
    NewsgroupData    &third = add_pending("comp.third", numbers);
    prepare_page(rereading, mode);
    ASSERT_EQ(ArticleNum{}, second.m_abs_first);
    ASSERT_EQ(ArticleNum{}, third.m_abs_first);

    const std::string output = render_page();

    ASSERT_EQ(3, g_sel_page_item_cnt);
    const std::array<NewsgroupData *, 3> groups{&first, &second, &third};
    for (std::size_t i = 0; i < groups.size(); ++i)
    {
        EXPECT_EQ(ArticleNum{1}, groups[i]->m_abs_first);
        EXPECT_EQ(ArticleNum{10}, groups[i]->m_ng_max);
        EXPECT_EQ(rereading ? 0 : 3, groups[i]->m_to_read);
        EXPECT_EQ(" " + numbers, groups[i]->rc_numbers());
        EXPECT_EQ(groups[i], g_sel_items[i].u.np);
        EXPECT_EQ(static_cast<int>(i) + 2, g_sel_items[i].line);
    }
    EXPECT_THAT(output.substr(output.rfind("Newsgroups")), testing::HasSubstr("3 groups"));
    EXPECT_THAT(output.substr(output.rfind("Newsgroups")), testing::Not(testing::HasSubstr("~")));
    EXPECT_EQ(3, g_sel_page_obj_cnt);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 3}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? 3 : 0, g_missing_count);
    EXPECT_EQ(6, g_sel_last_line);
    EXPECT_TRUE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupDisplayTest, removesNewlyIneligibleRowsWithoutCountingTheirReadStateTwice)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    NewsgroupData    &first = add_pending("comp.first", numbers);
    NewsgroupData    &opposite = add_pending("comp.opposite", rereading ? "1-7" : "1-10");
    NewsgroupData    &last = add_pending("comp.last", numbers);
    prepare_page(rereading, mode);
    g_newsgroup_ptr = &last;

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.opposite")));
    EXPECT_FALSE(opposite.m_flags & NF_INCLUDED);
    EXPECT_EQ(rereading ? 3 : 0, opposite.m_to_read);
    ASSERT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(&first, g_sel_items[0].u.np);
    EXPECT_EQ(&last, g_sel_items[1].u.np);
    EXPECT_EQ(3, g_sel_items[1].line);
    EXPECT_EQ(1, g_sel_item_index);
    EXPECT_EQ(2, g_sel_total_obj_cnt);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(NewsgroupNum{rereading ? 1 : 2}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? 2 : 1, g_missing_count);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_TRUE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupDisplayTest, keepsTheApproximateHeadingWhileLaterPagesStillHavePendingCounts)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    NewsgroupData    &first = add_pending("comp.first", numbers);
    NewsgroupData    &second = add_pending("comp.second", numbers);
    NewsgroupData    &third = add_pending("comp.third", numbers);
    NewsgroupData    &fourth = add_pending("comp.fourth", numbers);
    g_tc_LINES = 7;
    prepare_page(rereading, mode);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("~4 groups"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.fourth")));
    ASSERT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(&first, g_sel_items[0].u.np);
    EXPECT_EQ(&second, g_sel_items[1].u.np);
    EXPECT_EQ(&third, g_sel_next_np);
    EXPECT_EQ(ArticleNum{}, fourth.m_abs_first);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_FALSE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupDisplayTest, finishesLoadingOnTheFinalPageWithoutRepeatingEarlierRows)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    (void) add_pending("comp.first", numbers);
    (void) add_pending("comp.second", numbers);
    NewsgroupData &third = add_pending("comp.third", numbers);
    NewsgroupData &fourth = add_pending("comp.fourth", numbers);
    g_tc_LINES = 7;
    prepare_page(rereading, mode);
    (void) render_page();
    ASSERT_TRUE(next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.first")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.second")));
    EXPECT_THAT(output.substr(output.rfind("Newsgroups")), testing::HasSubstr("4 groups"));
    EXPECT_THAT(output.substr(output.rfind("Newsgroups")), testing::Not(testing::HasSubstr("~")));
    ASSERT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(&third, g_sel_items[0].u.np);
    EXPECT_EQ(&fourth, g_sel_items[1].u.np);
    EXPECT_EQ(ArticleNum{1}, fourth.m_abs_first);
    EXPECT_EQ(2, g_sel_prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 4}, g_newsgroup_to_read);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_TRUE(g_sel_at_end);
    EXPECT_FALSE(next_page());
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPresentation, DeferredNewsgroupDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values('s', 'm', 'l')));

TEST_P(DeferredNewsgroupRecoveryTest, replacesAnEmptyDeferredPageWithEarlierIncludedGroups)
{
    const auto [rereading, mode, numeric, capacity] = GetParam();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.opposite")));
    ASSERT_EQ(capacity, g_sel_page_item_cnt);
    for (int row = 0; row < capacity; ++row)
    {
        NewsgroupData *group = m_retained[row + 1];
        const int      selected = row + 1 == capacity ? 1 : rereading ? 2 : 0;
        expect_row(row, group, selected);
        EXPECT_THAT(output, testing::HasSubstr(expected_row(row, *group, selected)));
    }
    expect_refreshed_state(rereading);
    EXPECT_EQ(m_retained[1], g_sel_page_np);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_EQ(1, g_sel_prior_obj_cnt);
    EXPECT_EQ(capacity, g_sel_page_obj_cnt);
    EXPECT_EQ(capacity - 1, g_sel_item_index);
    EXPECT_EQ(capacity + 3, g_sel_last_line);
    EXPECT_TRUE(g_sel_at_end);
    EXPECT_FALSE(next_page());
    const std::size_t heading = output.rfind("Newsgroups");
    ASSERT_NE(std::string::npos, heading);
    EXPECT_THAT(output.substr(heading), testing::HasSubstr(std::to_string(m_retained.size()) + " groups"));
    EXPECT_THAT(output.substr(heading), testing::Not(testing::HasSubstr("~")));
}

INSTANTIATE_TEST_SUITE_P(ScopePresentationLabelAndCapacity, DeferredNewsgroupRecoveryTest,
                         testing::Combine(testing::Bool(), testing::Values('s', 'm', 'l'), testing::Bool(),
                                          testing::Values(1, 2)));

TEST_P(DeferredNewsgroupRecoveryNavigationTest, redrawingTheRecoveredPageDoesNotRepeatReadAccounting)
{
    const auto [rereading, capacity] = GetParam();
    (void) render_page();

    const std::string output = render_page();

    ASSERT_EQ(capacity, g_sel_page_item_cnt);
    for (int row = 0; row < capacity; ++row)
    {
        NewsgroupData *group = m_retained[row + 1];
        const int      selected = row + 1 == capacity ? 1 : rereading ? 2 : 0;
        expect_row(row, group, selected);
        EXPECT_THAT(output, testing::HasSubstr(expected_row(row, *group, selected)));
    }
    expect_refreshed_state(rereading);
    EXPECT_EQ(1, g_sel_prior_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_TRUE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupRecoveryNavigationTest, movesBackToTheEarlierPageAfterDeferredRecovery)
{
    const auto [rereading, capacity] = GetParam();
    (void) render_page();
    ASSERT_TRUE(prev_page());

    const std::string output = render_page();

    ASSERT_EQ(capacity, g_sel_page_item_cnt);
    for (int row = 0; row < capacity; ++row)
    {
        expect_row(row, m_retained[row], rereading ? 2 : 0);
        EXPECT_THAT(output, testing::HasSubstr(expected_row(row, *m_retained[row], rereading ? 2 : 0)));
    }
    expect_refreshed_state(rereading);
    EXPECT_EQ(m_retained.front(), g_sel_page_np);
    EXPECT_EQ(m_retained.back(), g_sel_next_np);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_FALSE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupRecoveryNavigationTest, advancesFromTheEarlierPageToTheFinalRetainedGroup)
{
    const auto [rereading, capacity] = GetParam();
    (void) render_page();
    ASSERT_TRUE(prev_page());
    (void) render_page();

    ASSERT_TRUE(next_page());
    const std::string output = render_page();

    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, m_retained.back(), 1);
    EXPECT_THAT(output, testing::HasSubstr(expected_row(0, *m_retained.back(), 1)));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.opposite")));
    expect_refreshed_state(rereading);
    EXPECT_EQ(capacity, g_sel_prior_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_TRUE(g_sel_at_end);
    EXPECT_FALSE(next_page());
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCapacity, DeferredNewsgroupRecoveryNavigationTest,
                         testing::Combine(testing::Bool(), testing::Values(1, 2)));

TEST_P(DeferredNewsgroupCleanupTest, rebuildsRowsAndPointersAfterCleanupMovesOrDeletesABogusGroup)
{
    const auto [rereading, remove] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    NewsgroupData    &first = add_pending("comp.first", numbers);
    NewsgroupData    &bogus = add_pending("comp.bogus", numbers, false);
    NewsgroupData    &last = add_pending("comp.last", numbers);
    prepare_page(rereading, 's');
    g_recent_newsgroup = &bogus;
    push_string(remove ? "y" : "n", 0200);

    const std::string output = render_page();

    const std::string visible = output.substr(output.rfind("<clear>"));
    EXPECT_THAT(output, testing::HasSubstr("Moving boguses to the end."));
    EXPECT_THAT(visible, testing::HasSubstr("comp.first\n"));
    EXPECT_THAT(visible, testing::HasSubstr("comp.last\n"));
    EXPECT_THAT(visible, testing::Not(testing::HasSubstr("comp.bogus")));
    ASSERT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(&first, g_sel_items[0].u.np);
    EXPECT_EQ(&last, g_sel_items[1].u.np);
    EXPECT_EQ(&first, g_sel_page_np);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_EQ(NewsgroupNum{remove ? 2 : 3}, g_newsgroup_count);
    ASSERT_EQ(remove ? 2U : 3U, g_newsgroup_order.size());
    EXPECT_EQ(&first, g_newsgroup_order[0]);
    EXPECT_EQ(&last, g_newsgroup_order[1]);
    if (remove)
    {
        EXPECT_TRUE(bogus.m_rc_line.empty());
        EXPECT_EQ(&first, g_current_newsgroup);
        EXPECT_EQ(&first, g_newsgroup_ptr);
        EXPECT_EQ(&first, g_recent_newsgroup);
    }
    else
    {
        EXPECT_EQ(&bogus, g_newsgroup_order[2]);
        EXPECT_EQ(TR_BOGUS, bogus.m_to_read);
    }
    EXPECT_TRUE(m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_FALSE(g_paranoid);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(2, g_sel_total_obj_cnt);
    EXPECT_TRUE(g_sel_at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCleanupChoice, DeferredNewsgroupCleanupTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(DeferredNewsgroupDescriptionTest, fetchesDescriptionsOnlyForVisibleRows)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    (void) add_pending("comp.first", numbers);
    (void) add_pending("comp.second", numbers);
    (void) add_pending("comp.third", numbers);
    (void) add_pending("comp.fourth", numbers);
    g_tc_LINES = 7;
    prepare_page(rereading, mode);
    testing::InSequence sequence;
    expect_description("comp.first", "First discussion");
    expect_description("comp.second", "Second discussion");

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("First discussion\n"));
    EXPECT_THAT(output, testing::HasSubstr("Second discussion\n"));
    EXPECT_EQ(2U, m_source.m_desc_sf.m_lines.size());
    EXPECT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_FALSE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupDescriptionTest, reusesFetchedDescriptionsWhenThePageIsRedrawn)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    (void) add_pending("comp.first", numbers);
    prepare_page(rereading, mode);
    expect_description("comp.first", "First discussion");
    (void) render_page();
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(m_connection.get()));

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("First discussion\n"));
    EXPECT_EQ(1U, m_source.m_desc_sf.m_lines.size());
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_TRUE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupDescriptionTest, fallsBackToTheGroupNameWhenTheServerReturnsNoDescription)
{
    const auto [rereading, mode] = GetParam();
    (void) add_pending("comp.first", rereading ? "1-10" : "1-7");
    prepare_page(rereading, mode);
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST NEWSGROUPS comp.first"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return("."));

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("comp.first\n"));
    EXPECT_EQ(1U, m_source.m_desc_sf.m_lines.size());
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_TRUE(g_sel_at_end);
}

TEST_P(DeferredNewsgroupDescriptionTest, fallsBackToNamesWhenDescriptionsAreUnsupported)
{
    const auto [rereading, mode] = GetParam();
    const std::string numbers = rereading ? "1-10" : "1-7";
    (void) add_pending("comp.first", numbers);
    (void) add_pending("comp.second", numbers);
    prepare_page(rereading, mode);
    m_source.m_nntp_link.capabilities = {};
    m_source.m_nntp_link.capabilities.available = true;
    m_source.m_nntp_link.capabilities.add("READER");

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("comp.first\n"));
    EXPECT_THAT(output, testing::HasSubstr("comp.second\n"));
    EXPECT_EQ(2, g_sel_page_item_cnt);
    EXPECT_TRUE(g_sel_at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPresentation, DeferredNewsgroupDescriptionTest,
                         testing::Combine(testing::Bool(), testing::Values('m', 'l')));
