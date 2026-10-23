// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <config/common.h>
#include <trn/datasrc.h>
#include <trn/ngdata.h>
#include <trn/only.h>
#include <trn/rcstuff.h>
#include <trn/rthread.h>
#include <trn/search.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

#include <ostream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

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
    NewsgroupData               *m_old_page{g_sel_page_np};
    NewsgroupData               *m_old_next{g_sel_next_np};
    NewsgroupNum                 m_old_count{g_newsgroup_count};
    ArticleUnread                m_old_minimum{g_newsgroup_min_to_read};
    ArticleUnread                m_old_selected{g_selected_count};
    ArticleNum                   m_old_objects{g_obj_count};
    SelectionMode                m_old_mode{g_sel_mode};
    SelectionSortMode            m_old_sort{g_sel_sort};
    int                          m_old_direction{g_sel_direction};
    AddGroupFlags                m_old_mask{g_sel_mask};
    bool                         m_old_rereading{g_sel_rereading};
    bool                         m_old_exclusive{g_sel_exclusive};
    bool                         m_old_paranoid{g_paranoid};
    bool                         m_old_verbose{g_verbose};
    int                          m_old_total{g_sel_total_obj_cnt};
    int                          m_old_prior{g_sel_prior_obj_cnt};
    int                          m_old_page_objects{g_sel_page_obj_cnt};
    int                          m_old_page_items{g_sel_page_item_cnt};
    int                          m_old_item_index{g_sel_item_index};
    int                          m_old_term_line{g_term_line};
    int                          m_old_lines{g_tc_LINES};
    int                          m_old_columns{g_tc_COLS};
    int                          m_old_mouse_width{g_mouse_bar_width};
    bool                         m_old_use_sel_num{g_use_sel_num};
    std::string                  m_old_sel_chars{g_sel_chars};
    std::string                  m_old_message{g_msg};
};

void NewsgroupPageStateTest::SetUp()
{
    m_groups.reserve(8);
    g_newsgroup_order.clear();
    g_first_newsgroup = nullptr;
    g_last_newsgroup = nullptr;
    g_sel_page_np = nullptr;
    g_sel_next_np = nullptr;
    g_newsgroup_count = NewsgroupNum{};
    g_newsgroup_min_to_read = TR_ONE;
    g_sel_mode = SM_NEWSGROUP;
    g_sel_sort = SS_NATURAL;
    g_sel_direction = 1;
    g_sel_exclusive = false;
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
    g_sel_page_np = m_old_page;
    g_sel_next_np = m_old_next;
    g_newsgroup_count = m_old_count;
    g_newsgroup_min_to_read = m_old_minimum;
    g_selected_count = m_old_selected;
    g_obj_count = m_old_objects;
    g_sel_mode = m_old_mode;
    g_sel_sort = m_old_sort;
    g_sel_direction = m_old_direction;
    g_sel_mask = m_old_mask;
    g_sel_rereading = m_old_rereading;
    g_sel_exclusive = m_old_exclusive;
    g_paranoid = m_old_paranoid;
    g_verbose = m_old_verbose;
    g_sel_total_obj_cnt = m_old_total;
    g_sel_prior_obj_cnt = m_old_prior;
    g_sel_page_obj_cnt = m_old_page_objects;
    g_sel_page_item_cnt = m_old_page_items;
    g_sel_item_index = m_old_item_index;
    g_term_line = m_old_term_line;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    g_mouse_bar_width = m_old_mouse_width;
    g_use_sel_num = m_old_use_sel_num;
    g_sel_chars = m_old_sel_chars;
    g_msg = m_old_message;
}

void NewsgroupPageStateTest::set_read_scope(bool rereading)
{
    g_sel_rereading = rereading;
    g_sel_mask = rereading ? AGF_DEL_SEL : AGF_SEL;
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

struct NewsgroupEligibility
{
    ArticleUnread unread;
    ArticleUnread minimum;
    bool          rereading;
    bool          selected;
    bool          included;
};

void PrintTo(const NewsgroupEligibility &state, std::ostream *output)
{
    *output << "unread=" << state.unread << ", minimum=" << state.minimum << ", rereading=" << state.rereading
            << ", selected=" << state.selected << ", included=" << state.included;
}

class NewsgroupPageEligibilityTest : public NewsgroupPageStateTest,
                                     public testing::WithParamInterface<NewsgroupEligibility>
{
};

class NewsgroupPageReadScopeTest : public NewsgroupPageStateTest, public testing::WithParamInterface<bool>
{
};

class NewsgroupPageInvalidTest : public NewsgroupPageStateTest,
                                 public testing::WithParamInterface<std::tuple<ArticleUnread, bool>>
{
};

class NewsgroupPageFallbackTest : public NewsgroupPageStateTest,
                                  public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupPageLoadingTest : public NewsgroupPageReadScopeTest
{
protected:
    void           SetUp() override;
    void           TearDown() override;
    NewsgroupData &add_pending_group(std::string_view name, std::string_view numbers);

private:
    DataSource    m_source{};
    Newsrc        m_newsrc{};
    NewsgroupNum  m_old_to_read{g_newsgroup_to_read};
    ArticleUnread m_old_missing{g_missing_count};
    bool          m_old_in_ng{g_in_ng};
    std::string   m_old_moderated{g_moderated};
    bool          m_old_redirected{g_redirected};
    std::string   m_old_redirected_to{g_redirected_to};
};

void NewsgroupPageLoadingTest::SetUp()
{
    NewsgroupPageReadScopeTest::SetUp();
    set_read_scope(GetParam());
    g_newsgroup_to_read = NewsgroupNum{};
    g_missing_count = 0;
    g_in_ng = false;
    g_redirected = false;
    m_source.m_flags = DF_REMOTE;
    m_source.m_act_sf.m_refetch_secs = 1;
    ASSERT_EQ(1, m_source.m_act_sf.open({}, "", ""));
    m_newsrc.data_source = &m_source;
}

void NewsgroupPageLoadingTest::TearDown()
{
    m_source.m_act_sf.close();
    g_newsgroup_to_read = m_old_to_read;
    g_missing_count = m_old_missing;
    g_in_ng = m_old_in_ng;
    g_moderated = m_old_moderated;
    g_redirected = m_old_redirected;
    g_redirected_to = m_old_redirected_to;
    NewsgroupPageReadScopeTest::TearDown();
}

NewsgroupData &NewsgroupPageLoadingTest::add_pending_group(std::string_view name, std::string_view numbers)
{
    NewsgroupData &group = add_group(name, TR_NONE);
    group.m_rc = &m_newsrc;
    group.m_abs_first = ArticleNum{};
    group.m_rc_line.append(numbers);
    (void) m_source.m_act_sf.append(std::string{name} + " 10 1 y\n", static_cast<int>(name.size()));
    return group;
}

} // namespace

TEST_P(NewsgroupPageEligibilityTest, cachedCountsAndCurrentSelectionDetermineEligibility)
{
    const NewsgroupEligibility &state = GetParam();
    set_read_scope(state.rereading);
    g_newsgroup_min_to_read = state.minimum;
    const NewsgroupFlags selection = static_cast<NewsgroupFlags>(g_sel_mask);
    const NewsgroupFlags inactive_selection = state.rereading ? NF_SEL : NF_DEL_SEL;
    NewsgroupData       &group =
        add_group("comp.test", state.unread, NF_INCLUDED | inactive_selection | (state.selected ? selection : NF_NONE));

    initialize_page();

    const std::vector<std::string> expected =
        state.included ? std::vector<std::string>{"comp.test"} : std::vector<std::string>{};
    EXPECT_EQ(expected, groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(state.included ? &group : nullptr, g_sel_page_np);
    EXPECT_EQ(state.included ? 1 : 0, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{state.included ? 1 : 0}, g_obj_count);
    EXPECT_EQ(state.selected ? 1 : 0, g_selected_count);
    EXPECT_EQ(state.included ? 1 : 0, g_sel_page_item_cnt);
    EXPECT_EQ(state.included ? 1 : 0, g_sel_page_obj_cnt);
    EXPECT_EQ(state.unread, group.m_to_read);
    EXPECT_EQ(state.rereading && state.included && !state.selected, !!(group.m_flags & NF_DEL));
    EXPECT_EQ(inactive_selection, group.m_flags & inactive_selection);
}

TEST_P(NewsgroupPageInvalidTest, selectionsCannotIncludeUnsubscribedOrInvalidEntries)
{
    const auto &[unread, rereading] = GetParam();
    set_read_scope(rereading);
    NewsgroupData &group = add_group("comp.invalid", unread, NF_INCLUDED | static_cast<NewsgroupFlags>(g_sel_mask));
    group.m_abs_first = ArticleNum{};

    initialize_page();

    EXPECT_TRUE(groups_with_flag(NF_INCLUDED).empty());
    EXPECT_EQ(0, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_page_np);
    EXPECT_EQ(unread, group.m_to_read);
    EXPECT_EQ(ArticleNum{}, group.m_abs_first);
}

TEST_P(NewsgroupPageReadScopeTest, selectionsCannotBypassNameRestrictions)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    const NewsgroupFlags selected = static_cast<NewsgroupFlags>(g_sel_mask);
    NewsgroupData       &included = add_group("comp.keep", rereading ? 0 : 3, selected);
    NewsgroupData       &excluded = add_group("alt.hidden", rereading ? 0 : 3, selected | NF_INCLUDED);
    excluded.m_abs_first = ArticleNum{};
    set_newsgroup_to_do("comp.*");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.keep"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&included, g_sel_page_np);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_max_newsgroup_to_do);
    EXPECT_EQ(ArticleNum{}, excluded.m_abs_first);
}

TEST_P(NewsgroupPageReadScopeTest, exclusiveDisplayCountsEligibleGroupsButIncludesOnlySelectedGroups)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    g_sel_exclusive = true;
    add_group("comp.hidden", rereading ? 0 : 3, NF_INCLUDED);
    NewsgroupData &selected = add_group("comp.selected", rereading ? 3 : 0, static_cast<NewsgroupFlags>(g_sel_mask));
    add_group("comp.invalid", TR_UNSUB, static_cast<NewsgroupFlags>(g_sel_mask));

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.selected"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&selected, g_sel_page_np);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page_obj_cnt);
    EXPECT_TRUE(groups_with_flag(NF_DEL).empty());
    EXPECT_TRUE(g_sel_exclusive);
}

TEST_P(NewsgroupPageReadScopeTest, emptyExclusiveSelectionRebuildsFromTheFirstEligibleGroup)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    NewsgroupData &eligible = add_group("comp.eligible", rereading ? 0 : 3);
    NewsgroupData &ineligible = add_group("comp.ineligible", rereading ? 3 : 0, NF_INCLUDED);
    g_sel_page_np = &ineligible;
    g_sel_exclusive = true;

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&eligible, g_sel_page_np);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(1, g_sel_page_obj_cnt);
    EXPECT_FALSE(g_sel_exclusive);
}

TEST_P(NewsgroupPageReadScopeTest, aPopulationWithNoEligibleGroupsProducesAnEmptyPage)
{
    set_read_scope(GetParam());
    add_group("comp.other-scope", GetParam() ? 3 : 0, NF_INCLUDED);
    add_group("comp.unsubscribed", TR_UNSUB, NF_INCLUDED);

    initialize_page();

    EXPECT_TRUE(groups_with_flag(NF_INCLUDED).empty());
    EXPECT_EQ(nullptr, g_sel_page_np);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_EQ(0, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_sel_page_item_cnt);
    EXPECT_EQ(0, g_sel_page_obj_cnt);
}

TEST_P(NewsgroupPageFallbackTest, anEmptyCatalogProducesAnEmptyPageAndClearsExclusiveDisplay)
{
    const auto &[rereading, exclusive] = GetParam();
    set_read_scope(rereading);
    g_sel_exclusive = exclusive;

    initialize_page();

    EXPECT_EQ(nullptr, g_sel_page_np);
    EXPECT_EQ(nullptr, g_sel_next_np);
    EXPECT_EQ(0, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page_item_cnt);
    EXPECT_EQ(0, g_sel_page_obj_cnt);
    EXPECT_FALSE(g_sel_exclusive);
}

TEST_P(NewsgroupPageFallbackTest, restrictionsWithNoEligibleGroupsAreRemovedBeforeRebuilding)
{
    const auto &[rereading, exclusive] = GetParam();
    set_read_scope(rereading);
    g_sel_exclusive = exclusive;
    g_newsgroup_min_to_read = 2;
    add_group("comp.other-scope", rereading ? 3 : 0);
    NewsgroupData &eligible = add_group("alt.eligible", rereading ? 0 : 3);
    set_newsgroup_to_do("comp.*");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"alt.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&eligible, g_sel_page_np);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(1, g_sel_page_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_max_newsgroup_to_do);
    EXPECT_EQ(TR_ONE, g_newsgroup_min_to_read);
    EXPECT_EQ("Exiting \"only\".", g_msg);
    EXPECT_FALSE(g_sel_exclusive);
}

TEST_P(NewsgroupPageLoadingTest, theFirstEligibleUninitializedGroupDefersRemainingCounts)
{
    const bool     rereading = GetParam();
    NewsgroupData &first = add_pending_group("comp.first", rereading ? "1-10" : "1-7");
    NewsgroupData &deferred = add_pending_group("comp.deferred", "1-10");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.first", "comp.deferred"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&first, g_sel_page_np);
    EXPECT_EQ(ArticleNum{1}, first.m_abs_first);
    EXPECT_EQ(rereading ? 0 : 3, first.m_to_read);
    EXPECT_EQ(ArticleNum{}, deferred.m_abs_first);
    EXPECT_EQ(rereading ? 0 : 1, deferred.m_to_read);
    EXPECT_EQ(2, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_FALSE(g_paranoid);
}

TEST_P(NewsgroupPageLoadingTest, anIneligibleUninitializedGroupDoesNotDeferLaterCounts)
{
    const bool     rereading = GetParam();
    NewsgroupData &ineligible = add_pending_group("comp.other-scope", rereading ? "1-7" : "1-10");
    NewsgroupData &eligible = add_pending_group("comp.eligible", rereading ? "1-10" : "1-7");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&eligible, g_sel_page_np);
    EXPECT_EQ(ArticleNum{1}, ineligible.m_abs_first);
    EXPECT_EQ(ArticleNum{1}, eligible.m_abs_first);
    EXPECT_EQ(rereading ? 3 : 0, ineligible.m_to_read);
    EXPECT_EQ(rereading ? 0 : 3, eligible.m_to_read);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(1, g_sel_page_obj_cnt);
    EXPECT_FALSE(g_paranoid);
}

INSTANTIATE_TEST_SUITE_P(
    CountReadScopeAndSelection, NewsgroupPageEligibilityTest,
    testing::Values(NewsgroupEligibility{0, 1, false, false, false}, NewsgroupEligibility{1, 1, false, false, true},
                    NewsgroupEligibility{1, 2, false, false, false}, NewsgroupEligibility{2, 2, false, false, true},
                    NewsgroupEligibility{0, 2, false, true, true}, NewsgroupEligibility{0, 1, true, false, true},
                    NewsgroupEligibility{1, 1, true, false, false}, NewsgroupEligibility{2, 2, true, false, false},
                    NewsgroupEligibility{2, 1, true, true, true}));

INSTANTIATE_TEST_SUITE_P(StatusAndReadScope, NewsgroupPageInvalidTest,
                         testing::Combine(testing::Values(ArticleUnread{TR_UNSUB}, ArticleUnread{TR_IGNORE},
                                                          ArticleUnread{TR_BOGUS}, ArticleUnread{TR_JUNK}),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupPageReadScopeTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadScopeAndExclusiveDisplay, NewsgroupPageFallbackTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupPageLoadingTest, testing::Bool());
