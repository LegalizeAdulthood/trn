// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <config/common.h>
#include <config/fdio.h>
#include <trn/addng.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngsrch.h>
#include <trn/ngstuff.h>
#include <trn/only.h>
#include <trn/rcstuff.h>
#include <trn/rt-select-internal.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/search.h>
#include <trn/smisc.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>

#include "support/NewsgroupPageState.h"
#include <mock_env.h>

#include <gtest/gtest.h>

#ifdef I_UNISTD
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using test_support::DISPLAY_GROUP_NAMES;
using test_support::NewsgroupPageDisplayState;
using test_support::NewsgroupPageStateTest;

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

class NewsgroupPageDisplayTest : public NewsgroupPageDisplayState,
                                 public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class ExclusiveNewsgroupPageDisplayTest : public NewsgroupPageDisplayState, public testing::WithParamInterface<bool>
{
};

class NewsgroupPageLayoutTest : public NewsgroupPageDisplayState,
                                public testing::WithParamInterface<std::tuple<bool, bool, char>>
{
protected:
    std::string row_prefix(int index, std::string_view label, char marker) const;
};

std::string NewsgroupPageLayoutTest::row_prefix(int index, std::string_view label, char marker) const
{
    std::ostringstream row;
    row << label << marker << std::setw(5) << static_cast<long>(m_entries[index]->m_to_read) << ' ';
    return row.str();
}

class NewsgroupPageResizeTest : public NewsgroupPageDisplayState,
                                public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupPageBoundaryState : public NewsgroupPageDisplayState
{
protected:
    void        prepare_empty(bool rereading, bool populated);
    void        leave_read_scope();
    std::string page_to_end();
    void        expect_empty(const std::string &output, bool rereading) const;
};

void NewsgroupPageBoundaryState::prepare_empty(bool rereading, bool populated)
{
    if (populated)
    {
        prepare_display(rereading, 's');
        leave_read_scope();
    }
    else
    {
        set_read_scope(rereading);
    }
    init_pages(PRESERVE_PAGE);
}

void NewsgroupPageBoundaryState::leave_read_scope()
{
    for (NewsgroupData *group : m_entries)
    {
        group->m_flags &= ~(NF_SEL | NF_DEL_SEL);
        if (group->m_to_read >= TR_NONE)
        {
            group->m_to_read = g_sel.rereading ? 7 : 0;
        }
    }
}

std::string NewsgroupPageBoundaryState::page_to_end()
{
    std::string output = render_page();
    while (g_sel_page.next_page())
    {
        output = render_page();
    }
    return output;
}

void NewsgroupPageBoundaryState::expect_empty(const std::string &output, bool rereading) const
{
    EXPECT_THAT(output, testing::HasSubstr("0 groups"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.")));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_FALSE(g_sel_page.next_page());
}

class EmptyNewsgroupPageDisplayTest : public NewsgroupPageBoundaryState,
                                      public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class FinalNewsgroupPageDisplayTest : public NewsgroupPageBoundaryState,
                                      public testing::WithParamInterface<std::tuple<bool, int>>
{
};

class RemovedNewsgroupPageDisplayTest : public NewsgroupPageBoundaryState, public testing::WithParamInterface<bool>
{
};

struct GroupDescriptionFallback
{
    const char                     *name;
    std::optional<std::string_view> description;
};

void PrintTo(const GroupDescriptionFallback &value, std::ostream *output)
{
    *output << value.name;
}

class NewsgroupDescriptionFallbackTest : public NewsgroupPageDisplayState,
                                         public testing::WithParamInterface<std::tuple<char, GroupDescriptionFallback>>
{
};

class NewsgroupSelectorCommandStateTest : public NewsgroupPageStateTest
{
protected:
    void           SetUp() override;
    void           TearDown() override;
    NewsgroupData &add_selector_group(std::string_view name, ArticleUnread unread, NewsgroupFlags flags = NF_NONE);
    void           restart_page();

    Newsrc m_newsrc{};

private:
    DataSource   m_source{};
    Multirc     *m_old_multirc{g_multirc};
    NewsgroupNum m_old_to_read{g_newsgroup_to_read};
    bool         m_old_in_ng{g_in_ng};
};

void NewsgroupSelectorCommandStateTest::SetUp()
{
    NewsgroupPageStateTest::SetUp();
    g_multirc = nullptr;
    g_in_ng = true;
    g_newsgroup_to_read = NewsgroupNum{};
    m_source.m_flags = DF_REMOTE;
    m_source.m_act_sf.m_refetch_secs = 1;
    ASSERT_EQ(1, m_source.m_act_sf.open({}, "", ""));
    m_newsrc.data_source = &m_source;
}

void NewsgroupSelectorCommandStateTest::TearDown()
{
    m_source.m_act_sf.close();
    g_multirc = m_old_multirc;
    g_newsgroup_to_read = m_old_to_read;
    g_in_ng = m_old_in_ng;
    NewsgroupPageStateTest::TearDown();
}

NewsgroupData &NewsgroupSelectorCommandStateTest::add_selector_group(std::string_view name, ArticleUnread unread,
                                                                     NewsgroupFlags flags)
{
    NewsgroupData &group = add_group(name, unread, flags);
    group.m_rc = &m_newsrc;
    group.m_rc_line.append("1-3");
    group.m_ng_max = ArticleNum{10};
    (void) m_source.m_act_sf.append(std::string{name} + " 10 1 y\n", static_cast<int>(name.size()));
    if (unread > TR_NONE)
    {
        ++g_newsgroup_to_read;
    }
    return group;
}

void NewsgroupSelectorCommandStateTest::restart_page()
{
    g_sel.mask = g_sel.rereading ? AGF_DEL_SEL : AGF_SEL;
    initialize_page();
}

class NewsgroupSelectorToggleTest : public NewsgroupSelectorCommandStateTest, public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorCatchupToggleTest : public NewsgroupSelectorCommandStateTest,
                                           public testing::WithParamInterface<bool>
{
};

struct NewsgroupReadSelection
{
    std::string_view name;
    NewsgroupFlags   flags;
};

void PrintTo(const NewsgroupReadSelection &selection, std::ostream *output)
{
    *output << selection.name;
}

const NewsgroupReadSelection NEWSGROUP_READ_SELECTIONS[] = {{"unmarked", NF_NONE},
                                                            {"pending deletion", NF_DEL},
                                                            {"reread selection", NF_DEL_SEL},
                                                            {"unread selection", NF_SEL},
                                                            {"selected in both scopes", NF_SEL | NF_DEL_SEL}};

class NewsgroupSelectorReadCleanupTest : public NewsgroupSelectorCommandStateTest,
                                         public testing::WithParamInterface<std::tuple<NewsgroupReadSelection, bool>>
{
};

struct NewsgroupSortChoice
{
    char               key;
    SelectionSortMode  sort;
    int                direction;
    std::array<int, 4> unread_order;
    std::array<int, 4> read_order;
    std::string_view   label;
};

void PrintTo(const NewsgroupSortChoice &choice, std::ostream *output)
{
    *output << choice.key;
}

const NewsgroupSortChoice NEWSGROUP_SORT_CHOICES[] = {{'n', SS_NATURAL, 1, {0, 1, 2, 3}, {0, 1, 2, 3}, "natural"},
                                                      {'N', SS_NATURAL, -1, {3, 2, 1, 0}, {3, 2, 1, 0}, "natural"},
                                                      {'g', SS_STRING, 1, {2, 0, 1, 3}, {2, 0, 1, 3}, "group name"},
                                                      {'G', SS_STRING, -1, {3, 1, 0, 2}, {3, 1, 0, 2}, "group name"},
                                                      {'c', SS_COUNT, 1, {1, 2, 0, 3}, {0, 1, 2, 3}, "count"},
                                                      {'C', SS_COUNT, -1, {3, 0, 2, 1}, {0, 1, 2, 3}, "count"}};

class NewsgroupSelectorPromptStateTest : public NewsgroupSelectorCommandStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    DisplayState run_command(char command, std::string_view input = {});

    std::string m_output;

private:
    int                           m_old_errno{errno};
    SelectionSortMode             m_old_group_sort{g_sel.newsgroup_sort};
    std::string_view              m_old_mode_string{g_sel.mode_string};
    std::string_view              m_old_sort_string{g_sel.sort_string};
    bool                          m_old_default_cmd{g_s_default_cmd};
    bool                          m_old_univ_default_cmd{g_univ_default_cmd};
    bool                          m_old_verify{g_verify};
    bool                          m_old_can_home{g_can_home};
    int                           m_old_mouse_bar_count{g_mouse_bar_cnt};
    int                           m_old_term_col{g_term_col};
    GeneralMode                   m_old_general_mode{g_general_mode};
    MinorMode                     m_old_minor_mode{g_mode};
    std::string_view              m_old_tc_cr{g_tc_CR};
    std::string_view              m_old_tc_ce{g_tc_CE};
};

void NewsgroupSelectorPromptStateTest::SetUp()
{
    NewsgroupSelectorCommandStateTest::SetUp();
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    errno = 0;
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    g_verify = false;
    g_can_home = false;
    g_mouse_bar_cnt = 0;
    g_term_col = 0;
    g_general_mode = GM_SELECTOR;
    g_mode = MM_NEWSGROUP_SELECTOR;
    g_tc_CR = "\r";
    g_tc_CE = "";
    g_tc_LINES = 7;
}

void NewsgroupSelectorPromptStateTest::TearDown()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    errno = m_old_errno;
    g_sel.newsgroup_sort = m_old_group_sort;
    g_sel.mode_string = m_old_mode_string;
    g_sel.sort_string = m_old_sort_string;
    g_s_default_cmd = m_old_default_cmd;
    g_univ_default_cmd = m_old_univ_default_cmd;
    g_verify = m_old_verify;
    g_can_home = m_old_can_home;
    g_mouse_bar_cnt = m_old_mouse_bar_count;
    g_term_col = m_old_term_col;
    g_general_mode = m_old_general_mode;
    g_mode = m_old_minor_mode;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    NewsgroupSelectorCommandStateTest::TearDown();
}

DisplayState NewsgroupSelectorPromptStateTest::run_command(char command, std::string_view input)
{
    push_string(input, 0200);
    testing::internal::CaptureStdout();
    const DisplayState result = newsgroup_commands(command);
    if (result == DS_DISPLAY)
    {
        Selection no_search{};
        no_search.op = OI_NONE;
        (void) calc_page(no_search);
    }
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

class NewsgroupSelectorSortStateTest : public NewsgroupSelectorPromptStateTest
{
protected:
    void prepare_sort(bool rereading, SelectionSortMode initial);
    void expect_group_preserved(int index) const;
    void expect_selections_preserved() const;
    void expect_sorted(const NewsgroupSortChoice &choice) const;

    std::array<NewsgroupData *, 5> m_objects{};

private:
    std::array<NewsgroupFlags, 5> m_flags{};
    std::array<ArticleUnread, 5>  m_counts{};
    std::array<std::string, 5>    m_numbers;
    bool                          m_rereading{};
};

void NewsgroupSelectorSortStateTest::prepare_sort(bool rereading, SelectionSortMode initial)
{
    m_rereading = rereading;
    set_read_scope(rereading);
    const std::array<std::string_view, 5> names{"comp.Bravo", "comp.charlie", "comp.alpha", "comp.delta",
                                                "comp.hidden"};
    const std::array<ArticleUnread, 5>    counts{7, 2, 4, 9, 0};
    const NewsgroupFlags                  inactive = rereading ? NF_SEL : NF_DEL_SEL;
    for (int index = 0; index < 5; ++index)
    {
        const ArticleUnread count = index == 4 ? (rereading ? 7 : 0) : (rereading ? 0 : counts[index]);
        NewsgroupFlags      flags = inactive | NF_VISIT | NF_UNTHREADED;
        if (index == 0 || index == 2 || index == 3)
        {
            flags |= static_cast<NewsgroupFlags>(g_sel.mask);
        }
        NewsgroupData &group = add_selector_group(names[index], count, flags);
        group.m_rc_line.resize(group.m_num_offset);
        group.m_rc_line += " 1-" + std::to_string(10 - count);
        m_objects[index] = &group;
    }
    g_sel.set_selector(SM_NEWSGROUP, initial);
    g_sel_page.page_ngd = m_objects[2];
    initialize_page();
    for (int index = 0; index < 5; ++index)
    {
        m_flags[index] = m_objects[index]->m_flags;
        m_counts[index] = m_objects[index]->m_to_read;
        m_numbers[index] = m_objects[index]->rc_numbers();
    }
}

void NewsgroupSelectorSortStateTest::expect_group_preserved(int index) const
{
    const NewsgroupData &group = *m_objects[index];
    EXPECT_EQ(m_flags[index], group.m_flags) << index;
    EXPECT_EQ(m_counts[index], group.m_to_read) << index;
    EXPECT_EQ(m_numbers[index], group.rc_numbers()) << index;
    EXPECT_EQ(NewsgroupNum{index}, group.m_num);
    EXPECT_EQ(':', group.m_subscribe_char);
    EXPECT_EQ(&m_newsrc, group.m_rc);
}

void NewsgroupSelectorSortStateTest::expect_selections_preserved() const
{
    for (int index = 0; index < 5; ++index)
    {
        expect_group_preserved(index);
    }
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_rereading, g_sel.rereading);
    EXPECT_EQ(m_rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(m_rereading ? NewsgroupNum{1} : NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    for (std::size_t index = 0; index < g_newsgroup_order.size(); ++index)
    {
        EXPECT_EQ(index ? g_newsgroup_order[index - 1] : nullptr, g_newsgroup_order[index]->m_prev);
        EXPECT_EQ(index + 1 < g_newsgroup_order.size() ? g_newsgroup_order[index + 1] : nullptr,
                  g_newsgroup_order[index]->m_next);
    }
    EXPECT_EQ(g_newsgroup_order.front(), g_first_newsgroup);
    EXPECT_EQ(g_newsgroup_order.back(), g_last_newsgroup);
}

void NewsgroupSelectorSortStateTest::expect_sorted(const NewsgroupSortChoice &choice) const
{
    const std::array<int, 4> &order = m_rereading ? choice.read_order : choice.unread_order;
    std::vector<std::string>  expected;
    for (int index : order)
    {
        expected.emplace_back(m_objects[index]->rc_name());
    }
    EXPECT_EQ(expected, groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(choice.sort, g_sel.sort);
    EXPECT_EQ(choice.direction, g_sel.direction);
    EXPECT_EQ(static_cast<SelectionSortMode>(choice.sort * choice.direction), g_sel.newsgroup_sort);
    EXPECT_EQ(choice.label, g_sel.sort_string);
    EXPECT_EQ(m_objects[order[0]], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_objects[order[2]], g_sel_page.next_ngd);
    expect_selections_preserved();
}

class NewsgroupSelectorSortChoiceTest : public NewsgroupSelectorSortStateTest,
                                        public testing::WithParamInterface<std::tuple<bool, NewsgroupSortChoice>>
{
};

class NewsgroupSelectorSortPromptTest : public NewsgroupSelectorSortStateTest, public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorSortCancelTest : public NewsgroupSelectorSortStateTest,
                                        public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorSortHelpTest : public NewsgroupSelectorSortStateTest,
                                      public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorSortCleanupTest : public NewsgroupSelectorSortStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorEmptySortTest : public NewsgroupSelectorSortStateTest,
                                       public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorCatchupStateTest : public NewsgroupSelectorPromptStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_groups();
    void expect_target_current(NewsgroupData *recent) const;
    void expect_other_groups_preserved() const;
    void expect_page_preserved() const;
    void expect_input_preserved() const;

    NewsgroupData *m_first{};
    NewsgroupData *m_target{};
    NewsgroupData *m_recent{};

private:
    NewsgroupData               *m_old_pointer{g_newsgroup_ptr};
    NewsgroupData               *m_old_current{g_current_newsgroup};
    NewsgroupData               *m_old_recent{g_recent_newsgroup};
    std::string                  m_old_name{g_newsgroup_name};
    std::string                  m_old_directory{g_newsgroup_dir};
    std::string                  m_old_moderated{g_moderated};
    bool                         m_old_redirected{g_redirected};
    std::string                  m_old_redirected_to{g_redirected_to};
    std::array<SelectionItem, 2> m_old_items{g_sel_page.items[0], g_sel_page.items[1]};
};

void NewsgroupSelectorCatchupStateTest::SetUp()
{
    NewsgroupSelectorPromptStateTest::SetUp();
    g_in_ng = false;
    g_newsgroup_ptr = nullptr;
    g_current_newsgroup = nullptr;
    g_recent_newsgroup = nullptr;
    g_newsgroup_name.clear();
    g_newsgroup_dir.clear();
    push_char('Q');
}

void NewsgroupSelectorCatchupStateTest::TearDown()
{
    g_newsgroup_ptr = m_old_pointer;
    g_current_newsgroup = m_old_current;
    g_recent_newsgroup = m_old_recent;
    g_newsgroup_name = m_old_name;
    g_newsgroup_dir = m_old_directory;
    g_moderated = m_old_moderated;
    g_redirected = m_old_redirected;
    g_redirected_to = m_old_redirected_to;
    for (int index = 0; index < 2; ++index)
    {
        g_sel_page.items[index] = m_old_items[index];
    }
    NewsgroupSelectorPromptStateTest::TearDown();
}

void NewsgroupSelectorCatchupStateTest::prepare_groups()
{
    m_first = &add_selector_group("comp.first", 7, NF_SEL | NF_VISIT | NF_UNTHREADED);
    m_target = &add_selector_group("comp.target", 7, NF_SEL | NF_DEL_SEL | NF_VISIT | NF_UNTHREADED);
    m_recent = &add_selector_group("comp.recent", 7, NF_DEL | NF_VISIT);
    initialize_page();
    for (int index = 0; index < 2; ++index)
    {
        g_sel_page.items[index] = {};
        g_sel_page.items[index].u.np = index ? m_target : m_first;
        g_sel_page.items[index].line = index + 2;
        g_sel_page.items[index].sel = 1;
    }
    g_sel_page.item_index = 1;
    set_newsgroup(m_first);
    g_current_newsgroup = m_first;
    g_recent_newsgroup = m_recent;
}

void NewsgroupSelectorCatchupStateTest::expect_target_current(NewsgroupData *recent) const
{
    EXPECT_EQ(m_target, g_newsgroup_ptr);
    EXPECT_EQ(m_target, g_current_newsgroup);
    EXPECT_EQ(recent, g_recent_newsgroup);
    EXPECT_EQ("comp.target", g_newsgroup_name);
    EXPECT_EQ("comp/target", g_newsgroup_dir);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
}

void NewsgroupSelectorCatchupStateTest::expect_other_groups_preserved() const
{
    const NewsgroupFlags included = g_sel.rereading ? NF_NONE : NF_INCLUDED;
    EXPECT_EQ(7, m_first->m_to_read);
    EXPECT_EQ(" 1-3", m_first->rc_numbers());
    EXPECT_EQ(':', m_first->m_subscribe_char);
    EXPECT_EQ(NF_SEL | NF_VISIT | NF_UNTHREADED | included, m_first->m_flags);
    EXPECT_EQ(7, m_recent->m_to_read);
    EXPECT_EQ(" 1-3", m_recent->rc_numbers());
    EXPECT_EQ(':', m_recent->m_subscribe_char);
    EXPECT_EQ(NF_DEL | NF_VISIT | included, m_recent->m_flags);
    EXPECT_EQ((std::vector<NewsgroupData *>{m_first, m_target, m_recent}), g_newsgroup_order);
    EXPECT_EQ(m_target, m_first->m_next);
    EXPECT_EQ(m_target, m_recent->m_prev);
    EXPECT_EQ(m_first, m_target->m_prev);
    EXPECT_EQ(m_recent, m_target->m_next);
}

void NewsgroupSelectorCatchupStateTest::expect_page_preserved() const
{
    EXPECT_EQ(m_first, g_sel_page.page_ngd);
    EXPECT_EQ(m_recent, g_sel_page.next_ngd);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(NF_SEL | NF_DEL_SEL | NF_VISIT | NF_UNTHREADED | NF_INCLUDED, m_target->m_flags);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_FALSE(g_sel.exclusive);
}

void NewsgroupSelectorCatchupStateTest::expect_input_preserved() const
{
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('Q', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

class NewsgroupSelectorCatchupConfirmTest : public NewsgroupSelectorCatchupStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorCatchupCancelTest : public NewsgroupSelectorCatchupStateTest,
                                           public testing::WithParamInterface<std::tuple<char, bool>>
{
};

class NewsgroupSelectorCatchupHelpTest : public NewsgroupSelectorCatchupStateTest,
                                         public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorMissingCatchupTest : public NewsgroupSelectorCatchupStateTest,
                                            public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorReadCatchupTest : public NewsgroupSelectorCatchupStateTest,
                                         public testing::WithParamInterface<char>
{
};

class NewsgroupSelectorNumericCatchupStateTest : public NewsgroupSelectorCatchupStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_catchup(ArticleUnread unread, std::string_view numbers) const;
    void expect_cancelled() const;
    void expect_repeated_prompt() const;

private:
    char             m_old_erase{g_erase_char};
    char             m_old_kill{g_kill_char};
    std::string_view m_old_backspace{g_tc_BC};
    std::string_view m_old_bell{g_tc_VB};
    bool             m_old_mouse_down{g_mouse_is_down};
};

void NewsgroupSelectorNumericCatchupStateTest::SetUp()
{
    NewsgroupSelectorCatchupStateTest::SetUp();
    g_erase_char = '\b';
    g_kill_char = Ctl('u');
    g_tc_BC = "\b";
    g_tc_VB = "";
    g_mouse_is_down = false;
    prepare_groups();
}

void NewsgroupSelectorNumericCatchupStateTest::TearDown()
{
    g_erase_char = m_old_erase;
    g_kill_char = m_old_kill;
    g_tc_BC = m_old_backspace;
    g_tc_VB = m_old_bell;
    g_mouse_is_down = m_old_mouse_down;
    NewsgroupSelectorCatchupStateTest::TearDown();
}

void NewsgroupSelectorNumericCatchupStateTest::expect_catchup(ArticleUnread unread, std::string_view numbers) const
{
    EXPECT_EQ(unread, m_target->m_to_read);
    EXPECT_EQ(numbers, m_target->rc_numbers());
    EXPECT_EQ(':', m_target->m_subscribe_char);
    EXPECT_EQ(unread ? NewsgroupNum{3} : NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_page_preserved();
}

void NewsgroupSelectorNumericCatchupStateTest::expect_cancelled() const
{
    EXPECT_EQ(7, m_target->m_to_read);
    EXPECT_EQ(" 1-3", m_target->rc_numbers());
    EXPECT_EQ(':', m_target->m_subscribe_char);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_page_preserved();
}

void NewsgroupSelectorNumericCatchupStateTest::expect_repeated_prompt() const
{
    const std::size_t first_prompt = m_output.find("Catchup comp.target?");
    ASSERT_NE(std::string::npos, first_prompt);
    EXPECT_NE(std::string::npos, m_output.find("Catchup comp.target?", first_prompt + 1));
}

class NewsgroupSelectorCatchupTailTest : public NewsgroupSelectorNumericCatchupStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, std::string>>
{
};

struct NewsgroupCatchupTail
{
    std::string_view input;
    ArticleUnread    unread;
    std::string_view numbers;
};

void PrintTo(const NewsgroupCatchupTail &tail, std::ostream *output)
{
    *output << tail.input.substr(0, tail.input.find('\n'));
}

class NewsgroupSelectorCatchupTailBoundaryTest : public NewsgroupSelectorNumericCatchupStateTest,
                                                 public testing::WithParamInterface<NewsgroupCatchupTail>
{
};

class NewsgroupSelectorCatchupZeroTest : public NewsgroupSelectorNumericCatchupStateTest,
                                         public testing::WithParamInterface<std::string>
{
};

using NewsgroupCatchupEdit = std::pair<std::string_view, std::string_view>;

class NewsgroupSelectorCatchupEditTest : public NewsgroupSelectorNumericCatchupStateTest,
                                         public testing::WithParamInterface<NewsgroupCatchupEdit>
{
};

class NewsgroupSelectorCatchupRetryTest : public NewsgroupSelectorNumericCatchupStateTest,
                                          public testing::WithParamInterface<std::string_view>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_answers(std::string_view answers);
    void expect_next_command() const;

private:
    std::array<int, 2> m_input{-1, -1};
    int                m_saved_stdin{-1};
    bool               m_old_typeahead{g_allow_typeahead};
};

void NewsgroupSelectorCatchupRetryTest::SetUp()
{
    NewsgroupSelectorNumericCatchupStateTest::SetUp();
#ifdef I_UNISTD
    ASSERT_EQ(0, pipe(m_input.data()));
#else
    ASSERT_EQ(0, _pipe(m_input.data(), 4096, _O_BINARY));
#endif
    m_saved_stdin = dup(fileno(stdin));
    ASSERT_GE(m_saved_stdin, 0);
    ASSERT_GE(dup2(m_input[0], fileno(stdin)), 0);
    std::clearerr(stdin);
    g_allow_typeahead = true;
    ASSERT_EQ('Q', read_tty_char());
    ASSERT_FALSE(macro_pending());
}

void NewsgroupSelectorCatchupRetryTest::TearDown()
{
    if (m_saved_stdin >= 0)
    {
        EXPECT_GE(dup2(m_saved_stdin, fileno(stdin)), 0);
        close(m_saved_stdin);
        std::clearerr(stdin);
    }
    for (int fd : m_input)
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
    g_allow_typeahead = m_old_typeahead;
    NewsgroupSelectorNumericCatchupStateTest::TearDown();
}

void NewsgroupSelectorCatchupRetryTest::set_answers(std::string_view answers)
{
    ASSERT_EQ(static_cast<int>(answers.size()), write(m_input[1], answers.data(), answers.size()));
    close(m_input[1]);
    m_input[1] = -1;
}

void NewsgroupSelectorCatchupRetryTest::expect_next_command() const
{
    EXPECT_EQ('Q', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

class NewsgroupSelectorExclusiveTest : public NewsgroupSelectorCommandStateTest,
                                       public testing::WithParamInterface<bool>
{
protected:
    void           SetUp() override;
    NewsgroupData &add_exclusive_group(std::string_view name, ArticleUnread unread, NewsgroupFlags flags = NF_NONE);
    void           prepare_groups(bool selected = true, bool exclusive = false);
    DisplayState   toggle_exclusive();
    void           expect_group_preserved(int index) const;
    void           expect_read_state_preserved() const;

    std::array<NewsgroupData *, 6> m_objects{};

private:
    std::array<NewsgroupFlags, 6> m_flags{};
    std::array<ArticleUnread, 6>  m_counts{};
};

void NewsgroupSelectorExclusiveTest::SetUp()
{
    NewsgroupSelectorCommandStateTest::SetUp();
    set_read_scope(GetParam());
    g_tc_LINES = 7;
}

NewsgroupData &NewsgroupSelectorExclusiveTest::add_exclusive_group(std::string_view name, ArticleUnread unread,
                                                                   NewsgroupFlags flags)
{
    NewsgroupData &group = add_selector_group(name, unread, flags);
    group.m_rc_line.resize(group.m_num_offset);
    group.m_rc_line += unread == TR_NONE ? " 1-10" : " 1-3";
    return group;
}

void NewsgroupSelectorExclusiveTest::prepare_groups(bool selected, bool exclusive)
{
    const std::array<std::string_view, 6> names{"comp.alpha", "comp.bravo", "comp.charlie",
                                                "comp.delta", "comp.echo",  "comp.hidden"};
    for (int index = 0; index < 6; ++index)
    {
        m_counts[index] = index == 5 ? (GetParam() ? 7 : 0) : (GetParam() ? 0 : 7);
        m_flags[index] = (GetParam() ? NF_SEL : NF_DEL_SEL) | NF_VISIT | NF_UNTHREADED;
        if (selected && (index == 1 || index == 3 || index == 4))
        {
            m_flags[index] |= static_cast<NewsgroupFlags>(g_sel.mask);
        }
        m_objects[index] = &add_exclusive_group(names[index], m_counts[index], m_flags[index]);
    }
    g_sel.exclusive = exclusive;
    g_sel_page.page_ngd = m_objects[3];
    initialize_page();
    g_sel_page.item_index = 1;
}

DisplayState NewsgroupSelectorExclusiveTest::toggle_exclusive()
{
    const DisplayState result = newsgroup_commands('E');
    Selection          no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
    return result;
}

void NewsgroupSelectorExclusiveTest::expect_group_preserved(int index) const
{
    const NewsgroupData &group = *m_objects[index];
    EXPECT_EQ(m_flags[index], group.m_flags & ~(NF_INCLUDED | NF_DEL)) << index;
    EXPECT_EQ(m_counts[index], group.m_to_read) << index;
    EXPECT_EQ(m_counts[index] ? " 1-3" : " 1-10", group.rc_numbers()) << index;
    EXPECT_EQ(NewsgroupNum{index}, group.m_num);
    EXPECT_EQ(':', group.m_subscribe_char);
    EXPECT_EQ(&m_newsrc, group.m_rc);
    EXPECT_EQ(index ? m_objects[index - 1] : nullptr, group.m_prev);
    EXPECT_EQ(index + 1 < 6 ? m_objects[index + 1] : nullptr, group.m_next);
}

void NewsgroupSelectorExclusiveTest::expect_read_state_preserved() const
{
    for (int index = 0; index < 6; ++index)
    {
        expect_group_preserved(index);
    }
    EXPECT_EQ(GetParam(), g_sel.rereading);
    EXPECT_EQ(GetParam() ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(GetParam() ? NewsgroupNum{1} : NewsgroupNum{5}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_EQ(SS_NATURAL, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    EXPECT_EQ(m_objects.front(), g_first_newsgroup);
    EXPECT_EQ(m_objects.back(), g_last_newsgroup);
}

class NewsgroupSelectorBulkStateTest : public NewsgroupSelectorPromptStateTest
{
protected:
    enum GroupIndex
    {
        BEFORE_UNSELECTED,
        BEFORE_SELECTED,
        PAGE_UNSELECTED,
        UNSUBSCRIBED,
        PAGE_SELECTED,
        AFTER_UNSELECTED,
        AFTER_SELECTED,
        GROUP_COUNT
    };

    void SetUp() override;
    void prepare_groups(bool rereading = false);
    void remember_groups();
    void clear_selections();
    void expect_identity(int index, bool unsubscribed = false) const;
    void expect_preserved(int index, NewsgroupFlags cleared = NF_NONE, NewsgroupFlags added = NF_NONE) const;
    void expect_caught_up(int index, bool included = true) const;
    void expect_context() const;

    std::array<NewsgroupData *, GROUP_COUNT> m_groups{};
    std::array<NewsgroupFlags, GROUP_COUNT>  m_flags{};
    std::array<ArticleUnread, GROUP_COUNT>   m_counts{};
    std::array<std::string, GROUP_COUNT>     m_numbers;

private:
    bool m_rereading{};
};

void NewsgroupSelectorBulkStateTest::SetUp()
{
    NewsgroupSelectorPromptStateTest::SetUp();
    g_tc_LINES = 7;
}

void NewsgroupSelectorBulkStateTest::prepare_groups(bool rereading)
{
    m_rereading = rereading;
    set_read_scope(rereading);
    const std::array<std::string_view, GROUP_COUNT> names{
        "comp.before-unselected", "alt.before-selected",  "comp.page-unselected", "comp.unsubscribed",
        "comp.page-selected",     "alt.after-unselected", "comp.after-selected"};
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        NewsgroupFlags flags = (rereading ? NF_SEL : NF_DEL_SEL) | NF_VISIT | NF_UNTHREADED;
        if (index == BEFORE_SELECTED || index == PAGE_SELECTED || index == AFTER_SELECTED || index == UNSUBSCRIBED)
        {
            flags |= static_cast<NewsgroupFlags>(g_sel.mask);
        }
        const ArticleUnread unread = index == UNSUBSCRIBED ? TR_UNSUB : (rereading ? 0 : 7);
        NewsgroupData      &group = add_selector_group(names[index], unread, flags);
        group.m_rc_line.resize(group.m_num_offset);
        group.m_rc_line += unread == TR_NONE ? " 1-10" : " 1-3";
        if (index == UNSUBSCRIBED)
        {
            group.m_subscribe_char = '!';
        }
        m_groups[index] = &group;
    }
    g_sel_page.page_ngd = m_groups[PAGE_UNSELECTED];
    initialize_page();
    g_sel_page.item_index = 1;
    remember_groups();
}

void NewsgroupSelectorBulkStateTest::remember_groups()
{
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        m_flags[index] = m_groups[index]->m_flags;
        m_counts[index] = m_groups[index]->m_to_read;
        m_numbers[index] = m_groups[index]->rc_numbers();
    }
}

void NewsgroupSelectorBulkStateTest::clear_selections()
{
    for (NewsgroupData *group : m_groups)
    {
        group->m_flags &= ~static_cast<NewsgroupFlags>(g_sel.mask);
    }
    initialize_page();
    remember_groups();
}

void NewsgroupSelectorBulkStateTest::expect_identity(int index, bool unsubscribed) const
{
    const NewsgroupData &group = *m_groups[index];
    EXPECT_EQ(NewsgroupNum{index}, group.m_num);
    EXPECT_EQ(unsubscribed || index == UNSUBSCRIBED ? '!' : ':', group.m_subscribe_char);
    EXPECT_EQ(&m_newsrc, group.m_rc);
    EXPECT_EQ(ArticleNum{10}, group.m_ng_max);
    EXPECT_EQ(ArticleNum{1}, group.m_abs_first);
    EXPECT_EQ(index ? m_groups[index - 1] : nullptr, group.m_prev);
    EXPECT_EQ(index + 1 < GROUP_COUNT ? m_groups[index + 1] : nullptr, group.m_next);
}

void NewsgroupSelectorBulkStateTest::expect_preserved(int index, NewsgroupFlags cleared, NewsgroupFlags added) const
{
    EXPECT_EQ(m_counts[index], m_groups[index]->m_to_read) << index;
    EXPECT_EQ(m_numbers[index], m_groups[index]->rc_numbers()) << index;
    EXPECT_EQ((m_flags[index] & ~cleared) | added, m_groups[index]->m_flags) << index;
    expect_identity(index);
}

void NewsgroupSelectorBulkStateTest::expect_caught_up(int index, bool included) const
{
    EXPECT_EQ(TR_NONE, m_groups[index]->m_to_read) << index;
    EXPECT_EQ(" 1-10", m_groups[index]->rc_numbers()) << index;
    EXPECT_EQ((m_flags[index] & ~(NF_DEL | NF_SEL | NF_INCLUDED)) | (included ? NF_INCLUDED : NF_NONE),
              m_groups[index]->m_flags)
        << index;
    expect_identity(index);
}

void NewsgroupSelectorBulkStateTest::expect_context() const
{
    EXPECT_EQ(m_rereading, g_sel.rereading);
    EXPECT_EQ(m_rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_EQ(SS_NATURAL, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    EXPECT_EQ(NewsgroupNum{GROUP_COUNT}, g_newsgroup_count);
    EXPECT_EQ(m_groups.front(), g_first_newsgroup);
    EXPECT_EQ(m_groups.back(), g_last_newsgroup);
}

class NewsgroupSelectorExclusiveBulkTest : public NewsgroupSelectorBulkStateTest,
                                           public testing::WithParamInterface<char>
{
};

class NewsgroupSelectorReadBulkTest : public NewsgroupSelectorBulkStateTest, public testing::WithParamInterface<char>
{
};

class NewsgroupSelectorUnsubscribedBulkTest : public NewsgroupSelectorBulkStateTest,
                                              public testing::WithParamInterface<std::tuple<char, bool>>
{
};

class NewsgroupSelectorEmptyBulkTest : public NewsgroupSelectorBulkStateTest,
                                       public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorInlineStateTest : public NewsgroupSelectorBulkStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         prepare_inline(bool rereading = false);
    DisplayState run_inline(std::string_view input);
    void         expect_unsubscribed(int index) const;
    void         expect_inline_context() const;

    static constexpr std::array<int, 3> SELECTED{BEFORE_SELECTED, PAGE_SELECTED, AFTER_SELECTED};
    static constexpr std::array<int, 3> UNSELECTED{BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED};

private:
    NewsgroupData             *m_old_pointer{g_newsgroup_ptr};
    NewsgroupData             *m_old_current{g_current_newsgroup};
    NewsgroupData             *m_old_recent{g_recent_newsgroup};
    std::string                m_old_name{g_newsgroup_name};
    int                        m_old_page_line{g_page_line};
    int                        m_old_perform_count{g_perform_count};
    int                        m_old_spin_count{g_spin_count};
    bool                       m_old_performed_loop{g_performed_article_loop};
    bool                       m_old_error{g_error_occurred};
    bool                       m_old_one_command{g_one_command};
    std::optional<std::string> m_old_subject_line{g_subj_line};
    DataSource                *m_old_data_source{g_data_source};
    std::string_view           m_old_standout{g_tc_SO};
    std::string_view           m_old_end_standout{g_tc_SE};
};

void NewsgroupSelectorInlineStateTest::SetUp()
{
    NewsgroupSelectorBulkStateTest::SetUp();
    g_data_source = m_newsrc.data_source;
    g_tc_SO = "";
    g_tc_SE = "";
    g_newsgroup_ptr = nullptr;
    g_current_newsgroup = nullptr;
    g_recent_newsgroup = nullptr;
    g_page_line = 1;
    g_perform_count = 0;
    g_error_occurred = false;
    g_one_command = false;
}

void NewsgroupSelectorInlineStateTest::TearDown()
{
    g_newsgroup_ptr = m_old_pointer;
    g_current_newsgroup = m_old_current;
    g_recent_newsgroup = m_old_recent;
    g_newsgroup_name = m_old_name;
    g_page_line = m_old_page_line;
    g_perform_count = m_old_perform_count;
    g_spin_count = m_old_spin_count;
    g_performed_article_loop = m_old_performed_loop;
    g_error_occurred = m_old_error;
    g_one_command = m_old_one_command;
    g_subj_line = m_old_subject_line;
    g_data_source = m_old_data_source;
    g_tc_SO = m_old_standout;
    g_tc_SE = m_old_end_standout;
    NewsgroupSelectorBulkStateTest::TearDown();
}

void NewsgroupSelectorInlineStateTest::prepare_inline(bool rereading)
{
    prepare_groups(rereading);
    g_current_newsgroup = m_groups[PAGE_SELECTED];
    g_recent_newsgroup = m_groups[BEFORE_SELECTED];
    set_newsgroup(g_current_newsgroup);
}

DisplayState NewsgroupSelectorInlineStateTest::run_inline(std::string_view input)
{
    return run_command(':', std::string{input} + "\n\n");
}

void NewsgroupSelectorInlineStateTest::expect_unsubscribed(int index) const
{
    const NewsgroupData &group = *m_groups[index];
    EXPECT_EQ(TR_UNSUB, group.m_to_read) << index;
    EXPECT_EQ(m_numbers[index], group.rc_numbers()) << index;
    EXPECT_EQ(m_flags[index] & ~(static_cast<NewsgroupFlags>(g_sel.mask) | NF_INCLUDED), group.m_flags) << index;
    expect_identity(index, true);
}

void NewsgroupSelectorInlineStateTest::expect_inline_context() const
{
    expect_context();
    EXPECT_EQ(m_groups[PAGE_SELECTED], g_current_newsgroup);
    EXPECT_EQ(m_groups[BEFORE_SELECTED], g_recent_newsgroup);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ(0, g_sel_page.item_index);
}

class NewsgroupSelectorInlineReadScopeTest : public NewsgroupSelectorInlineStateTest,
                                             public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorInlineScopeTest : public NewsgroupSelectorInlineStateTest,
                                         public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorInlineEmptyTest : public NewsgroupSelectorInlineStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorDotScopeTest : public NewsgroupSelectorInlineStateTest,
                                      public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void         prepare_dot(int target = PAGE_SELECTED);
    DisplayState run_dot(std::string_view commands);
    bool         affects(int index) const;
    void         expect_dot_context(int cursor) const;

    int m_target{PAGE_SELECTED};
};

void NewsgroupSelectorDotScopeTest::prepare_dot(int target)
{
    prepare_inline(std::get<0>(GetParam()));
    m_target = target;
    g_current_newsgroup = m_groups[BEFORE_UNSELECTED];
    set_newsgroup(m_groups[target]);
    g_sel_page.item_index = target == PAGE_SELECTED ? 1 : 0;
}

DisplayState NewsgroupSelectorDotScopeTest::run_dot(std::string_view commands)
{
    return run_inline(std::string{std::get<1>(GetParam()) ? ":." : "."} + std::string{commands});
}

bool NewsgroupSelectorDotScopeTest::affects(int index) const
{
    return index != UNSUBSCRIBED && ((index == m_target) != std::get<1>(GetParam()));
}

void NewsgroupSelectorDotScopeTest::expect_dot_context(int cursor) const
{
    expect_context();
    EXPECT_EQ(m_groups[BEFORE_UNSELECTED], g_current_newsgroup);
    EXPECT_EQ(m_groups[BEFORE_SELECTED], g_recent_newsgroup);
    EXPECT_EQ(m_groups[m_target], g_newsgroup_ptr);
    EXPECT_EQ(m_groups[m_target]->rc_name(), g_newsgroup_name);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ(cursor, g_sel_page.item_index);
}

class NewsgroupSelectorSearchStateTest : public NewsgroupSelectorInlineStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         prepare_search(bool rereading);
    void         prepare_mixed_search(bool rereading);
    void         seed_search(bool rereading, std::string_view input);
    DisplayState run_search(std::string_view input);
    void         expect_search_context() const;
    void         expect_search_selection(const std::vector<int> &matches) const;

private:
    AddGroupTable m_old_add_groups{std::move(g_add_groups)};
    std::string   m_old_directory{g_newsgroup_dir};
};

void NewsgroupSelectorSearchStateTest::SetUp()
{
    NewsgroupSelectorInlineStateTest::SetUp();
    g_add_groups = {};
    newsgroup_search_init();
}

void NewsgroupSelectorSearchStateTest::TearDown()
{
    newsgroup_search_init();
    g_add_groups = std::move(m_old_add_groups);
    g_newsgroup_dir = m_old_directory;
    NewsgroupSelectorInlineStateTest::TearDown();
}

void NewsgroupSelectorSearchStateTest::prepare_search(bool rereading)
{
    prepare_inline(rereading);
    g_current_newsgroup = m_groups[PAGE_UNSELECTED];
    set_newsgroup(m_groups[PAGE_SELECTED]);
}

void NewsgroupSelectorSearchStateTest::prepare_mixed_search(bool rereading)
{
    prepare_search(rereading);
    NewsgroupData &opposite = *m_groups[PAGE_UNSELECTED];
    opposite.m_to_read = rereading ? 7 : 0;
    opposite.m_rc_line.resize(opposite.m_num_offset);
    opposite.m_rc_line += rereading ? " 1-3" : " 1-10";
    m_groups[AFTER_UNSELECTED]->m_to_read = TR_BOGUS;
    g_newsgroup_to_read = NewsgroupNum{rereading ? 1 : 4};
    initialize_page();
    remember_groups();
}

void NewsgroupSelectorSearchStateTest::seed_search(bool rereading, std::string_view input)
{
    prepare_mixed_search(rereading);
    ASSERT_EQ(DS_DISPLAY, run_search(input));
    clear_selections();
    set_newsgroup(m_groups[PAGE_SELECTED]);
}

DisplayState NewsgroupSelectorSearchStateTest::run_search(std::string_view input)
{
    const std::string command = std::string{input} + "\n\n";
    for (auto ch = command.rbegin(); ch != command.rend(); ++ch)
    {
        push_char(static_cast<unsigned char>(*ch) | 0200);
    }
    return run_command('/');
}

void NewsgroupSelectorSearchStateTest::expect_search_context() const
{
    expect_context();
    EXPECT_EQ(m_groups[PAGE_UNSELECTED], g_current_newsgroup);
    EXPECT_EQ(m_groups[BEFORE_SELECTED], g_recent_newsgroup);
    EXPECT_EQ(g_current_newsgroup, g_newsgroup_ptr);
    EXPECT_EQ(g_current_newsgroup->rc_name(), g_newsgroup_name);
    EXPECT_EQ("comp/page-unselected", g_newsgroup_dir);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ(0, g_sel_page.item_index);
}

void NewsgroupSelectorSearchStateTest::expect_search_selection(const std::vector<int> &matches) const
{
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool matched = std::find(matches.begin(), matches.end(), index) != matches.end();
        if (matched && !(m_flags[index] & static_cast<NewsgroupFlags>(g_sel.mask)))
        {
            expect_preserved(index, NF_DEL, static_cast<NewsgroupFlags>(g_sel.mask) | NF_INCLUDED);
        }
        else
        {
            expect_preserved(index);
        }
    }
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

struct NewsgroupSearchPattern
{
    std::string_view input;
    std::vector<int> matches;
    int              selected_count;
};

void PrintTo(const NewsgroupSearchPattern &pattern, std::ostream *output)
{
    *output << pattern.input;
}

class NewsgroupSelectorSearchPatternTest : public NewsgroupSelectorSearchStateTest,
                                           public testing::WithParamInterface<std::tuple<bool, NewsgroupSearchPattern>>
{
};

class NewsgroupSelectorSearchReadScopeTest : public NewsgroupSelectorSearchStateTest,
                                             public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorSearchModifierTest : public NewsgroupSelectorSearchStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorSearchDisplayTest : public NewsgroupSelectorSearchStateTest,
                                           public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorSearchActionStateTest : public NewsgroupSelectorSearchStateTest
{
protected:
    void TearDown() override;
    void prepare_actions(bool rereading, bool clean, bool exclusive = false);
    void prepare_screen(bool clean);
    void expect_read_target(int index, bool rereading) const;

private:
    int              m_old_last_line{g_sel_page.last_line};
    int              m_old_scrolled{g_term_scrolled};
    bool             m_old_chase_phrase{g_output_chase_phrase};
    std::string_view m_old_up{g_tc_UP};
};

void NewsgroupSelectorSearchActionStateTest::TearDown()
{
    g_sel_page.last_line = m_old_last_line;
    g_term_scrolled = m_old_scrolled;
    g_output_chase_phrase = m_old_chase_phrase;
    g_tc_UP = m_old_up;
    NewsgroupSelectorSearchStateTest::TearDown();
}

void NewsgroupSelectorSearchActionStateTest::prepare_actions(bool rereading, bool clean, bool exclusive)
{
    prepare_search(rereading);
    g_sel.exclusive = exclusive;
    initialize_page();
    remember_groups();
    prepare_screen(clean);
}

void NewsgroupSelectorSearchActionStateTest::prepare_screen(bool clean)
{
    g_tc_UP = "";
    g_sel_page.item_index = g_sel_page.page_item_cnt;
    ASSERT_EQ(DS_DISPLAY, sel_command(Ctl('l')));
    g_sel_page.item_index = 1;
    set_newsgroup(m_groups[PAGE_SELECTED]);
    g_sel_page.last_line = 3;
    g_term_line = clean ? 3 : 1;
    g_term_scrolled = 0;
}

void NewsgroupSelectorSearchActionStateTest::expect_read_target(int index, bool rereading) const
{
    if (rereading)
    {
        const bool selected = m_flags[index] & NF_DEL_SEL;
        expect_preserved(index, selected ? NF_DEL_SEL : NF_NONE, selected ? NF_DEL : NF_NONE);
    }
    else
    {
        expect_caught_up(index, false);
    }
}

class NewsgroupSelectorSearchActionTest : public NewsgroupSelectorSearchActionStateTest,
                                          public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorSearchActionModifierTest : public NewsgroupSelectorSearchActionStateTest,
                                                  public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorSearchActionFallbackTest : public NewsgroupSelectorSearchActionStateTest,
                                                  public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorRecoveryStateTest : public NewsgroupSelectorSearchActionStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         prepare_prompt(char command, bool rereading, bool clean);
    DisplayState run_prompt(char command, std::string_view input);
    void         expect_catalog_preserved() const;

private:
    char             m_old_erase{g_erase_char};
    char             m_old_kill{g_kill_char};
    char             m_old_interrupts{g_int_count};
    std::string_view m_old_backspace{g_tc_BC};
};

void NewsgroupSelectorRecoveryStateTest::SetUp()
{
    NewsgroupSelectorSearchActionStateTest::SetUp();
    g_erase_char = '\b';
    g_kill_char = Ctl('u');
    g_int_count = 0;
    g_tc_BC = "\b";
}

void NewsgroupSelectorRecoveryStateTest::TearDown()
{
    g_erase_char = m_old_erase;
    g_kill_char = m_old_kill;
    g_int_count = m_old_interrupts;
    g_tc_BC = m_old_backspace;
    NewsgroupSelectorSearchActionStateTest::TearDown();
}

void NewsgroupSelectorRecoveryStateTest::prepare_prompt(char command, bool rereading, bool clean)
{
    prepare_actions(rereading, clean);
    if (command == ':')
    {
        g_current_newsgroup = m_groups[PAGE_SELECTED];
    }
}

DisplayState NewsgroupSelectorRecoveryStateTest::run_prompt(char command, std::string_view input)
{
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(static_cast<unsigned char>(*ch) | 0200);
    }
    return run_command(command);
}

void NewsgroupSelectorRecoveryStateTest::expect_catalog_preserved() const
{
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_context();
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
}

class NewsgroupSelectorPromptCancelTest : public NewsgroupSelectorRecoveryStateTest,
                                          public testing::WithParamInterface<std::tuple<char, std::string_view>>
{
};

class NewsgroupSelectorPromptCompletionTest : public NewsgroupSelectorRecoveryStateTest,
                                              public testing::WithParamInterface<std::tuple<char, bool>>
{
};

class NewsgroupSelectorPromptErrorTest : public NewsgroupSelectorRecoveryStateTest,
                                         public testing::WithParamInterface<std::tuple<char, bool>>
{
};

class NewsgroupSelectorSearchPromptErrorTest
    : public NewsgroupSelectorRecoveryStateTest,
      public testing::WithParamInterface<std::tuple<bool, std::pair<std::string_view, std::string_view>>>
{
};

class NewsgroupSelectorInlineRecoveryTest : public NewsgroupSelectorRecoveryStateTest,
                                            public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorPromptFollowupTest : public NewsgroupSelectorRecoveryStateTest,
                                            public testing::WithParamInterface<std::tuple<char, char>>
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

TEST_P(NewsgroupSelectorToggleTest, toggleRequestsRestartInTheOtherReadScopeWithoutRebuildingThePage)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    NewsgroupData &group = add_group("comp.anchor", TR_NONE, NF_SEL | NF_DEL_SEL | NF_UNTHREADED);
    g_sel_page.page_ngd = &group;
    g_sel.exclusive = true;
    g_selected_count = 1;
    const AddGroupFlags mask = g_sel.mask;

    const DisplayState result = newsgroup_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(!rereading, g_sel.rereading);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(mask, g_sel.mask);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(TR_NONE, group.m_to_read);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, NewsgroupSelectorToggleTest, testing::Bool());

TEST_P(NewsgroupSelectorToggleTest, toggleRestartsAnEmptyCatalogWithoutInventingRowsOrSelections)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    initialize_page();

    const DisplayState result = newsgroup_commands('U');
    restart_page();

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(!rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_SEL : AGF_DEL_SEL, g_sel.mask);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorToggleTest, restartRebuildsTheNewReadScopeAndRecountsItsSelections)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    NewsgroupData &unread = add_selector_group("comp.unread", 7, NF_SEL | NF_UNTHREADED);
    NewsgroupData &read = add_selector_group("comp.read", TR_NONE, NF_VISIT);
    initialize_page();
    ASSERT_EQ(rereading ? &read : &unread, g_sel_page.page_ngd);

    const DisplayState result = newsgroup_commands('U');
    restart_page();

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(!rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_SEL : AGF_DEL_SEL, g_sel.mask);
    EXPECT_EQ(rereading ? &unread : &read, g_sel_page.page_ngd);
    const std::vector<std::string> expected =
        rereading ? std::vector<std::string>{"comp.unread"} : std::vector<std::string>{"comp.read"};
    EXPECT_EQ(expected, groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 0, g_selected_count);
    EXPECT_EQ(7, unread.m_to_read);
    EXPECT_EQ(TR_NONE, read.m_to_read);
    EXPECT_EQ(" 1-3", unread.rc_numbers());
    EXPECT_EQ(" 1-3", read.rc_numbers());
    EXPECT_EQ(NF_SEL | NF_UNTHREADED | (rereading ? NF_INCLUDED : NF_NONE), unread.m_flags);
    EXPECT_EQ(rereading ? NF_VISIT : NF_VISIT | NF_INCLUDED | NF_DEL, read.m_flags);
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorCatchupToggleTest, leavingUnreadModeAppliesCatchupMarksAcrossTheCatalog)
{
    const bool     included = GetParam();
    NewsgroupData &marked =
        add_selector_group("comp.marked", 7, NF_DEL | NF_UNTHREADED | NF_VISIT | (included ? NF_INCLUDED : NF_NONE));
    NewsgroupData &kept = add_selector_group("comp.kept", 7, NF_SEL | NF_INCLUDED);
    g_sel_page.page_ngd = included ? &marked : &kept;
    g_sel.exclusive = !included;
    g_selected_count = 1;

    const DisplayState result = newsgroup_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(NF_UNTHREADED | NF_VISIT | (included ? NF_INCLUDED : NF_NONE), marked.m_flags);
    EXPECT_EQ(TR_NONE, marked.m_to_read);
    EXPECT_EQ(" 1-10", marked.rc_numbers());
    EXPECT_EQ(NF_SEL | NF_INCLUDED, kept.m_flags);
    EXPECT_EQ(7, kept.m_to_read);
    EXPECT_EQ(" 1-3", kept.rc_numbers());
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
}

INSTANTIATE_TEST_SUITE_P(NormalAndExclusiveDisplay, NewsgroupSelectorCatchupToggleTest, testing::Bool());

TEST_P(NewsgroupSelectorReadCleanupTest, leavingReadModePromotesSelectionsAndClearsPendingDeletion)
{
    const auto [selection, included] = GetParam();
    const NewsgroupFlags flags = selection.flags;
    set_read_scope(true);
    const NewsgroupFlags preserved = NF_UNTHREADED | NF_VISIT | (included ? NF_INCLUDED : NF_NONE);
    NewsgroupData       &read = add_selector_group("comp.read", TR_NONE, flags | preserved);
    NewsgroupData       &kept = add_selector_group("comp.kept", TR_NONE, NF_SEL | NF_DEL | NF_INCLUDED);
    g_sel_page.page_ngd = included ? &read : &kept;
    g_selected_count = flags & NF_DEL_SEL ? 1 : 0;
    const bool selected = flags & (NF_SEL | NF_DEL_SEL);

    const DisplayState result = newsgroup_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(preserved | (selected ? NF_SEL : NF_NONE), read.m_flags);
    EXPECT_EQ(TR_NONE, read.m_to_read);
    EXPECT_EQ(" 1-3", read.rc_numbers());
    EXPECT_EQ(NF_SEL | NF_INCLUDED, kept.m_flags);
    EXPECT_EQ(TR_NONE, kept.m_to_read);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

INSTANTIATE_TEST_SUITE_P(SelectionFlagsAndInclusion, NewsgroupSelectorReadCleanupTest,
                         testing::Combine(testing::ValuesIn(NEWSGROUP_READ_SELECTIONS), testing::Bool()));

TEST_F(NewsgroupSelectorCommandStateTest, restartPromotesRereadSelectionsAlongsideExistingUnreadSelections)
{
    set_read_scope(true);
    NewsgroupData &unread = add_selector_group("comp.unread", 7, NF_SEL | NF_UNTHREADED);
    NewsgroupData &read = add_selector_group("comp.read", TR_NONE, NF_DEL_SEL | NF_VISIT);
    initialize_page();

    const DisplayState result = newsgroup_commands('U');
    restart_page();

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_EQ((std::vector<std::string>{"comp.unread", "comp.read"}), groups_with_flag(NF_SEL));
    EXPECT_EQ((std::vector<std::string>{"comp.unread", "comp.read"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&unread, g_sel_page.page_ngd);
    EXPECT_EQ(NF_SEL | NF_INCLUDED | NF_UNTHREADED, unread.m_flags);
    EXPECT_EQ(NF_SEL | NF_INCLUDED | NF_VISIT, read.m_flags);
    EXPECT_EQ(7, unread.m_to_read);
    EXPECT_EQ(TR_NONE, read.m_to_read);
    EXPECT_EQ(" 1-3", read.rc_numbers());
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_F(NewsgroupSelectorCommandStateTest, roundTripRetainsNormalAndRereadSelectionsWithoutDoubleCounting)
{
    NewsgroupData &unread = add_selector_group("comp.unread", 7, NF_SEL);
    NewsgroupData &read = add_selector_group("comp.read", TR_NONE, NF_SEL | NF_DEL_SEL);
    initialize_page();

    const DisplayState to_read = newsgroup_commands('U');
    restart_page();
    const DisplayState to_unread = newsgroup_commands('U');
    restart_page();

    EXPECT_EQ(DS_RESTART, to_read);
    EXPECT_EQ(DS_RESTART, to_unread);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_EQ((std::vector<std::string>{"comp.unread", "comp.read"}), groups_with_flag(NF_SEL));
    EXPECT_EQ((std::vector<std::string>{"comp.unread", "comp.read"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(NF_SEL | NF_INCLUDED, unread.m_flags);
    EXPECT_EQ(NF_SEL | NF_INCLUDED, read.m_flags);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&unread, g_sel_page.page_ngd);
    EXPECT_EQ(7, unread.m_to_read);
    EXPECT_EQ(TR_NONE, read.m_to_read);
    EXPECT_EQ(" 1-3", read.rc_numbers());
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorSortChoiceTest, choosingAnOrderRebuildsTheFirstPageWithoutChangingSelections)
{
    const auto &[rereading, choice] = GetParam();
    prepare_sort(rereading, static_cast<SelectionSortMode>(-SS_COUNT));

    const DisplayState result = run_command('O', std::string(1, choice.key));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_sorted(choice);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

TEST_P(NewsgroupSelectorSortChoiceTest, reverseFlipsTheCurrentDirectionWithoutChangingSelections)
{
    const auto &[rereading, choice] = GetParam();
    prepare_sort(rereading, static_cast<SelectionSortMode>(-choice.sort * choice.direction));

    const DisplayState result = run_command('R');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_sorted(choice);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndOrder, NewsgroupSelectorSortChoiceTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(NEWSGROUP_SORT_CHOICES)));

TEST_P(NewsgroupSelectorSortPromptTest, blankChoiceUsesForwardNewsrcOrder)
{
    prepare_sort(GetParam(), static_cast<SelectionSortMode>(-SS_STRING));

    const DisplayState result = run_command('O', " ");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_sorted(NEWSGROUP_SORT_CHOICES[0]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, NewsgroupSelectorSortPromptTest, testing::Bool());

TEST_P(NewsgroupSelectorSortCancelTest, rejectedChoicesRetainTheOrderAndPageAnchor)
{
    const auto [rereading, key] = GetParam();
    prepare_sort(rereading, static_cast<SelectionSortMode>(-SS_STRING));
    g_can_home = true;
    const std::vector<NewsgroupData *> order = g_newsgroup_order;
    const NewsgroupData               *anchor = g_sel_page.page_ngd;
    const int                          prior = g_sel_page.prior_obj_cnt;
    const int                          rows = g_sel_page.page_item_cnt;

    const DisplayState result = run_command('O', std::string(1, key));

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(order, g_newsgroup_order);
    EXPECT_EQ(anchor, g_sel_page.page_ngd);
    EXPECT_EQ(prior, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(rows, g_sel_page.page_item_cnt);
    EXPECT_EQ(SS_STRING, g_sel.sort);
    EXPECT_EQ(-1, g_sel.direction);
    EXPECT_EQ(static_cast<SelectionSortMode>(-SS_STRING), g_sel.newsgroup_sort);
    expect_selections_preserved();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndResponse, NewsgroupSelectorSortCancelTest,
                         testing::Combine(testing::Bool(), testing::Values('q', '\b', 'x')));

TEST_P(NewsgroupSelectorSortHelpTest, helpDescribesEachOrderAndAcceptsAChoiceAtTheRepeatedPrompt)
{
    const auto [rereading, verbose] = GetParam();
    prepare_sort(rereading, SS_NATURAL);
    g_verbose = verbose;

    const DisplayState result = run_command('O', verbose ? "Hg" : "hg");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_sorted(NEWSGROUP_SORT_CHOICES[2]);
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type n or SP" : "n or SP sorts"));
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type g" : "g sorts"));
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type c" : "c sorts"));
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "upper case" : "Upper case"));
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type q" : "q does nothing"));
    const std::size_t first_prompt = m_output.find("Order by Newsrc, Group name, or Count?");
    ASSERT_NE(std::string::npos, first_prompt);
    EXPECT_NE(std::string::npos, m_output.find("Order by Newsrc, Group name, or Count?", first_prompt + 1));
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndVerbosity, NewsgroupSelectorSortHelpTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorSortCleanupTest, sortingAppliesPendingCatchupOnlyInUnreadMode)
{
    const auto [rereading, command] = GetParam();
    prepare_sort(rereading, SS_NATURAL);
    m_objects[1]->m_flags |= NF_DEL;

    const DisplayState result = run_command(command, command == 'O' ? "n" : "");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {0, 2, 3, 4})
    {
        expect_group_preserved(index);
    }
    if (rereading)
    {
        expect_group_preserved(1);
    }
    else
    {
        EXPECT_EQ(NF_DEL_SEL | NF_UNTHREADED | NF_VISIT, m_objects[1]->m_flags);
    }
    EXPECT_EQ(TR_NONE, m_objects[1]->m_to_read);
    EXPECT_EQ(" 1-10", m_objects[1]->rc_numbers());
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(rereading ? 4 : 3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{1} : NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? RF_NONE : RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCommand, NewsgroupSelectorSortCleanupTest,
                         testing::Combine(testing::Bool(), testing::Values('O', 'R')));

TEST_P(NewsgroupSelectorEmptySortTest, sortingAnEmptyCatalogRetainsAnEmptyPageAndNoSelections)
{
    const auto [rereading, command] = GetParam();
    set_read_scope(rereading);
    g_sel.set_selector(SM_NEWSGROUP, static_cast<SelectionSortMode>(-SS_STRING));
    initialize_page();

    const DisplayState result = run_command(command, command == 'O' ? "c" : "");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(command == 'O' ? SS_COUNT : SS_STRING, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCommand, NewsgroupSelectorEmptySortTest,
                         testing::Combine(testing::Bool(), testing::Values('O', 'R')));

TEST_P(NewsgroupSelectorExclusiveTest, enablingExclusiveDisplayRebuildsTheFirstPageWithOnlyActiveSelections)
{
    prepare_groups();

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.bravo", "comp.delta", "comp.echo"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_objects[1], g_sel_page.page_ngd);
    EXPECT_EQ(m_objects[4], g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(GetParam() ? (std::vector<std::string>{"comp.alpha", "comp.charlie"}) : std::vector<std::string>{},
              groups_with_flag(NF_DEL));
    expect_read_state_preserved();
}

TEST_P(NewsgroupSelectorExclusiveTest, disablingExclusiveDisplayRestoresUnselectedGroupsFromTheFirstPage)
{
    prepare_groups(true, true);

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.alpha", "comp.bravo", "comp.charlie", "comp.delta", "comp.echo"}),
              groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_objects[0], g_sel_page.page_ngd);
    EXPECT_EQ(m_objects[2], g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(GetParam() ? (std::vector<std::string>{"comp.alpha", "comp.charlie"}) : std::vector<std::string>{},
              groups_with_flag(NF_DEL));
    expect_read_state_preserved();
}

TEST_P(NewsgroupSelectorExclusiveTest, selectionsInTheInactiveScopeDoNotPreventFallbackToNormalDisplay)
{
    prepare_groups(false);

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.alpha", "comp.bravo", "comp.charlie", "comp.delta", "comp.echo"}),
              groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_objects[0], g_sel_page.page_ngd);
    EXPECT_EQ(m_objects[2], g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    expect_read_state_preserved();
}

TEST_P(NewsgroupSelectorExclusiveTest, activeSelectionRetainsAGroupOutsideTheCurrentReadScope)
{
    NewsgroupData &selected =
        add_exclusive_group("comp.selected", GetParam() ? 7 : 0, static_cast<NewsgroupFlags>(g_sel.mask));
    initialize_page();

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.selected"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(&selected, g_sel_page.page_ngd);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(GetParam() ? 7 : 0, selected.m_to_read);
    EXPECT_EQ(GetParam() ? " 1-3" : " 1-10", selected.rc_numbers());
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorExclusiveTest, selectedGroupsOutsideARestrictionDoNotPreventFallbackWithinIt)
{
    prepare_groups();
    set_newsgroup_to_do("comp.alpha");

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.alpha"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(m_objects[0], g_sel_page.page_ngd);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(1, g_only.max_to_do);
    EXPECT_EQ("comp.alpha", newsgroup_restriction(0));
    expect_read_state_preserved();
}

TEST_P(NewsgroupSelectorExclusiveTest, anUnsubscribedSelectionDoesNotPreventFallbackToEligibleGroups)
{
    NewsgroupData &eligible = add_exclusive_group("comp.eligible", GetParam() ? 0 : 7);
    NewsgroupData &unsubscribed =
        add_exclusive_group("comp.unsubscribed", TR_UNSUB, static_cast<NewsgroupFlags>(g_sel.mask));
    unsubscribed.m_subscribe_char = '!';
    initialize_page();

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(&eligible, g_sel_page.page_ngd);
    EXPECT_EQ(TR_UNSUB, unsubscribed.m_to_read);
    EXPECT_EQ('!', unsubscribed.m_subscribe_char);
    EXPECT_EQ(static_cast<NewsgroupFlags>(g_sel.mask), unsubscribed.m_flags);
    EXPECT_EQ(" 1-3", unsubscribed.rc_numbers());
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorExclusiveTest, togglingAppliesPendingCatchupOnlyInUnreadMode)
{
    prepare_groups();
    m_objects[0]->m_flags |= NF_DEL;

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ((std::vector<std::string>{"comp.bravo", "comp.delta", "comp.echo"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(TR_NONE, m_objects[0]->m_to_read);
    EXPECT_EQ(" 1-10", m_objects[0]->rc_numbers());
    EXPECT_EQ((GetParam() ? NF_SEL | NF_DEL : NF_DEL_SEL) | NF_VISIT | NF_UNTHREADED, m_objects[0]->m_flags);
    EXPECT_EQ(GetParam() ? NewsgroupNum{1} : NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(GetParam() ? RF_NONE : RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(GetParam() ? ArticleNum{5} : ArticleNum{4}, g_obj_count);
    EXPECT_EQ(GetParam(), g_sel.rereading);
    EXPECT_EQ(GetParam() ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    for (int index = 1; index < 6; ++index)
    {
        expect_group_preserved(index);
    }
}

TEST_P(NewsgroupSelectorExclusiveTest, anEmptyCatalogFallsBackToAnEmptyNormalDisplay)
{
    initialize_page();
    g_sel_page.item_index = 1;

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorExclusiveTest, aCatalogWithOnlyIneligibleGroupsFallsBackToAnEmptyNormalDisplay)
{
    NewsgroupData &other_scope = add_exclusive_group("comp.other-scope", GetParam() ? 7 : 0);
    initialize_page();
    g_sel_page.item_index = 1;

    const DisplayState result = toggle_exclusive();

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_TRUE(groups_with_flag(NF_INCLUDED).empty());
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(GetParam() ? 7 : 0, other_scope.m_to_read);
    EXPECT_EQ(GetParam() ? " 1-3" : " 1-10", other_scope.rc_numbers());
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, NewsgroupSelectorExclusiveTest, testing::Bool());

TEST_P(NewsgroupSelectorCatchupConfirmTest, confirmationCatchesUpTheHighlightedGroupWithoutChangingOtherGroups)
{
    const auto [verbose, answer] = GetParam();
    g_verbose = verbose;
    prepare_groups();

    const DisplayState result = run_command('c', std::string(1, answer));

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(TR_NONE, m_target->m_to_read);
    EXPECT_EQ(" 1-10", m_target->rc_numbers());
    EXPECT_EQ(':', m_target->m_subscribe_char);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_NE(std::string::npos,
              m_output.find(verbose ? "Mark everything in comp.target as read?" : "Catchup comp.target?"));
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Marking comp.target as all read." : "Marked read"));
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_page_preserved();
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndAnswer, NewsgroupSelectorCatchupConfirmTest,
                         testing::Combine(testing::Bool(), testing::Values('y', ' ')));

TEST_P(NewsgroupSelectorCatchupCancelTest, cancellationPreservesReadStateAndTheHighlightedRow)
{
    const auto [answer, can_home] = GetParam();
    prepare_groups();
    g_can_home = can_home;

    const DisplayState result = run_command('c', std::string(1, answer));

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(7, m_target->m_to_read);
    EXPECT_EQ(" 1-3", m_target->rc_numbers());
    EXPECT_EQ(':', m_target->m_subscribe_char);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_NE(std::string::npos, m_output.find("Catchup comp.target?"));
    EXPECT_EQ(std::string::npos, m_output.find("Marked read"));
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_page_preserved();
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(AnswerAndCursorAddressing, NewsgroupSelectorCatchupCancelTest,
                         testing::Combine(testing::Values('n', 'q'), testing::Bool()));

TEST_P(NewsgroupSelectorCatchupHelpTest, helpRepeatsThePromptAndAllowsCancellationWithoutCatchingUp)
{
    g_verbose = GetParam();
    prepare_groups();

    const DisplayState result = run_command('c', GetParam() ? "Hn" : "hn");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find(GetParam() ? "Type n to leave articles" : "n to forget it."));
    EXPECT_NE(std::string::npos,
              m_output.find(GetParam() ? "Type u to mark everything" : "u to mark all and unsubscribe."));
    const std::string prompt = GetParam() ? "Mark everything in comp.target as read?" : "Catchup comp.target?";
    const std::size_t first_prompt = m_output.find(prompt);
    ASSERT_NE(std::string::npos, first_prompt);
    EXPECT_NE(std::string::npos, m_output.find(prompt, first_prompt + 1));
    EXPECT_EQ(7, m_target->m_to_read);
    EXPECT_EQ(" 1-3", m_target->rc_numbers());
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_page_preserved();
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(Verbosity, NewsgroupSelectorCatchupHelpTest, testing::Bool());

TEST_F(NewsgroupSelectorCatchupStateTest, catchingUpTheCurrentGroupKeepsThePreviousRecentGroup)
{
    prepare_groups();
    set_newsgroup(m_target);
    g_current_newsgroup = m_target;

    const DisplayState result = run_command('c', "y");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(TR_NONE, m_target->m_to_read);
    EXPECT_EQ(" 1-10", m_target->rc_numbers());
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_target_current(m_recent);
    expect_other_groups_preserved();
    expect_page_preserved();
    expect_input_preserved();
}

TEST_F(NewsgroupSelectorCatchupStateTest, unsubscribeCatchesUpTheHighlightedGroupAndRemovesItFromTheUnreadCountOnce)
{
    prepare_groups();

    const DisplayState result = run_command('c', "u");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(TR_NONE, m_target->m_to_read);
    EXPECT_EQ(" 1-10", m_target->rc_numbers());
    EXPECT_EQ('!', m_target->m_subscribe_char);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_NE(std::string::npos, m_output.find("Unsubscribed to newsgroup comp.target"));
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_page_preserved();
    expect_input_preserved();
}

TEST_P(NewsgroupSelectorMissingCatchupTest, anAbsentHighlightedRowReportsTheProblemWithoutAskingForInput)
{
    if (GetParam())
    {
        prepare_groups();
    }
    else
    {
        initialize_page();
    }
    g_sel_page.item_index = g_sel_page.page_item_cnt;
    NewsgroupData *const current = g_current_newsgroup;
    NewsgroupData *const pointer = g_newsgroup_ptr;
    NewsgroupData *const recent = g_recent_newsgroup;

    const DisplayState result = run_command('c');

    EXPECT_EQ(DS_UPDATE, result);
    EXPECT_EQ("No newsgroup to catchup.", g_msg);
    EXPECT_TRUE(m_output.empty());
    EXPECT_EQ(current, g_current_newsgroup);
    EXPECT_EQ(pointer, g_newsgroup_ptr);
    EXPECT_EQ(recent, g_recent_newsgroup);
    EXPECT_EQ(GetParam() ? NewsgroupNum{3} : NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    if (GetParam())
    {
        EXPECT_EQ(7, m_target->m_to_read);
        EXPECT_EQ(" 1-3", m_target->rc_numbers());
        expect_other_groups_preserved();
    }
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(PopulatedPage, NewsgroupSelectorMissingCatchupTest, testing::Bool());

TEST_P(NewsgroupSelectorReadCatchupTest, catchingUpAnAlreadyReadGroupPreservesTheUnreadPopulation)
{
    prepare_groups();
    m_target->m_to_read = TR_NONE;
    m_target->m_rc_line.resize(m_target->m_num_offset);
    m_target->m_rc_line += " 1-10";
    --g_newsgroup_to_read;
    set_read_scope(true);
    g_sel_page.page_ngd = nullptr;
    initialize_page();
    g_sel_page.items[0].u.np = m_target;
    g_sel_page.item_index = 0;

    const DisplayState result = run_command('c', std::string(1, GetParam()));

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(TR_NONE, m_target->m_to_read);
    EXPECT_EQ(" 1-10", m_target->rc_numbers());
    EXPECT_EQ(GetParam() == 'u' ? '!' : ':', m_target->m_subscribe_char);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_EQ(NF_SEL | NF_DEL_SEL | NF_VISIT | NF_UNTHREADED | NF_INCLUDED, m_target->m_flags);
    EXPECT_EQ(AGF_DEL_SEL, g_sel.mask);
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_target, g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.item_index);
    expect_target_current(m_first);
    expect_other_groups_preserved();
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(Answer, NewsgroupSelectorReadCatchupTest, testing::Values('y', 'u'));

TEST_P(NewsgroupSelectorCatchupTailTest, numericInputMarksOlderArticlesReadAndKeepsTheRequestedTail)
{
    const auto &[verbose, input] = GetParam();
    g_verbose = verbose;

    const DisplayState result = run_command('c', input);

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Marking all but 3 articles in comp.target as read."
                                                       : "All but 3 marked as read."));
    expect_catchup(3, " 1-7");
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndInput, NewsgroupSelectorCatchupTailTest,
                         testing::Combine(testing::Bool(), testing::Values(std::string{"3\n"}, std::string{"#3\n"},
                                                                           std::string{"04\b3\n"})));

TEST_P(NewsgroupSelectorCatchupTailBoundaryTest, theRequestedTailDoesNotMarkAlreadyReadArticlesUnread)
{
    const NewsgroupCatchupTail &tail = GetParam();

    const DisplayState result = run_command('c', tail.input);

    EXPECT_EQ(DS_DISPLAY, result);
    expect_catchup(tail.unread, tail.numbers);
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(TailSize, NewsgroupSelectorCatchupTailBoundaryTest,
                         testing::Values(NewsgroupCatchupTail{"1\n", 1, " 1-9"}, NewsgroupCatchupTail{"7\n", 7, " 1-3"},
                                         NewsgroupCatchupTail{"10\n", 7, " 1-3"},
                                         NewsgroupCatchupTail{"99\n", 7, " 1-3"}));

TEST_F(NewsgroupSelectorNumericCatchupStateTest, partialCatchupPreservesExistingReadRangesWithinTheTail)
{
    m_target->m_rc_line.resize(m_target->m_num_offset);
    m_target->m_rc_line += " 1-3,9-10";
    m_target->m_to_read = 5;

    const DisplayState result = run_command('c', "3\n");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_catchup(1, " 1-7,9-10");
    expect_input_preserved();
}

TEST_P(NewsgroupSelectorCatchupZeroTest, zeroOrTheDefaultNumericAnswerMarksTheWholeGroupRead)
{
    const DisplayState result = run_command('c', GetParam());

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find("Marked read"));
    expect_catchup(TR_NONE, " 1-10");
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(Input, NewsgroupSelectorCatchupZeroTest,
                         testing::Values(std::string{"0\n"}, std::string{"#0"}, std::string{"# "}));

TEST_P(NewsgroupSelectorCatchupEditTest, erasingTheNumericAnswerReturnsToConfirmationAndAllowsRefusal)
{
    const auto &[prefix, edit] = GetParam();
    const std::string input = std::string{prefix} + std::string{edit} + 'n';

    const DisplayState result = run_command('c', input);

    EXPECT_EQ(DS_DISPLAY, result);
    expect_repeated_prompt();
    expect_cancelled();
    expect_input_preserved();
}

TEST_P(NewsgroupSelectorCatchupEditTest, replacingAnErasedNumericAnswerUsesTheNewTailCount)
{
    const auto &[prefix, edit] = GetParam();
    const std::string input = std::string{prefix} + std::string{edit} + "3\n";

    const DisplayState result = run_command('c', input);

    EXPECT_EQ(DS_DISPLAY, result);
    expect_repeated_prompt();
    expect_catchup(3, " 1-7");
    expect_input_preserved();
}

INSTANTIATE_TEST_SUITE_P(InputAndEditing, NewsgroupSelectorCatchupEditTest,
                         testing::Values(NewsgroupCatchupEdit{"4", "\b"}, NewsgroupCatchupEdit{"42", "\x15"},
                                         NewsgroupCatchupEdit{"#4", "\b"}, NewsgroupCatchupEdit{"#42", "\x15"}));

TEST_P(NewsgroupSelectorCatchupRetryTest, anInvalidReplyRepeatsConfirmationAndAllowsRefusal)
{
    set_answers(std::string{GetParam()} + "nQ");

    const DisplayState result = run_command('c');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find("Type h for help."));
    expect_repeated_prompt();
    expect_cancelled();
    expect_next_command();
}

TEST_P(NewsgroupSelectorCatchupRetryTest, anInvalidReplyRepeatsConfirmationAndAllowsCatchup)
{
    set_answers(std::string{GetParam()} + "yQ");

    const DisplayState result = run_command('c');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find("Type h for help."));
    expect_repeated_prompt();
    expect_catchup(TR_NONE, " 1-10");
    expect_next_command();
}

INSTANTIATE_TEST_SUITE_P(InvalidReply, NewsgroupSelectorCatchupRetryTest, testing::Values("x", "#x"));

TEST_F(NewsgroupSelectorBulkStateTest, XReadsUnselectedGroupsAcrossPagesAndRetainsSelections)
{
    prepare_groups();

    const DisplayState result = newsgroup_commands('X');

    EXPECT_EQ(DS_QUIT, result);
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED})
    {
        expect_caught_up(index);
    }
    for (int index : {BEFORE_SELECTED, PAGE_SELECTED, AFTER_SELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, DReadsOnlyUnselectedGroupsOnTheCurrentPage)
{
    prepare_groups();
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    ASSERT_EQ(m_groups[AFTER_UNSELECTED], g_sel_page.next_ngd);

    const DisplayState result = newsgroup_commands('D');

    EXPECT_EQ(DS_QUIT, result);
    expect_caught_up(PAGE_UNSELECTED);
    for (int index :
         {BEFORE_UNSELECTED, BEFORE_SELECTED, UNSUBSCRIBED, PAGE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{5}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, JReadsSelectedGroupsAcrossPagesAndRebuildsTheRemainingPopulation)
{
    prepare_groups();

    const DisplayState result = newsgroup_commands('J');

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_SELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_caught_up(index, false);
    }
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(m_groups[PAGE_UNSELECTED], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, XPreservesUnselectedGroupsHiddenByANameRestriction)
{
    prepare_groups();
    set_newsgroup_to_do("comp.*");
    initialize_page();
    remember_groups();

    const DisplayState result = newsgroup_commands('X');

    EXPECT_EQ(DS_QUIT, result);
    expect_caught_up(BEFORE_UNSELECTED);
    expect_caught_up(PAGE_UNSELECTED);
    for (int index : {BEFORE_SELECTED, PAGE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_only.max_to_do);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, JIncludesSelectionsHiddenByANameRestriction)
{
    prepare_groups();
    set_newsgroup_to_do("comp.*");
    initialize_page();
    remember_groups();

    const DisplayState result = newsgroup_commands('J');

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_SELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_caught_up(index, false);
    }
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_groups[BEFORE_UNSELECTED], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(1, g_only.max_to_do);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_P(NewsgroupSelectorExclusiveBulkTest, unselectedGroupsOutsideAnExclusiveDisplayRemainUnread)
{
    prepare_groups();
    g_sel.exclusive = true;
    g_sel_page.page_ngd = nullptr;
    initialize_page();
    remember_groups();

    const DisplayState result = newsgroup_commands(GetParam());

    EXPECT_EQ(DS_QUIT, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_context();
}

INSTANTIATE_TEST_SUITE_P(Command, NewsgroupSelectorExclusiveBulkTest, testing::Values('X', 'D'));

TEST_F(NewsgroupSelectorBulkStateTest, JFallsBackToNormalDisplayAfterReadingAllExclusiveSelections)
{
    prepare_groups();
    g_sel.exclusive = true;
    g_sel_page.page_ngd = nullptr;
    initialize_page();
    remember_groups();

    const DisplayState result = newsgroup_commands('J');

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_SELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_caught_up(index, false);
    }
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED})
    {
        expect_preserved(index, NF_NONE, NF_INCLUDED);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_groups[BEFORE_UNSELECTED], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, JAlsoAppliesPendingKillMarksOutsideTheCurrentPage)
{
    prepare_groups();
    m_groups[BEFORE_UNSELECTED]->m_flags |= NF_DEL;
    remember_groups();

    const DisplayState result = newsgroup_commands('J');

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_UNSELECTED, BEFORE_SELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_caught_up(index, false);
    }
    for (int index : {PAGE_UNSELECTED, AFTER_UNSELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, DRetainsPendingKillMarksOutsideTheCurrentPageForLaterCleanup)
{
    prepare_groups();
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED})
    {
        m_groups[index]->m_flags |= NF_DEL;
    }
    remember_groups();

    const DisplayState result = newsgroup_commands('D');

    EXPECT_EQ(DS_QUIT, result);
    expect_caught_up(PAGE_UNSELECTED);
    for (int index :
         {BEFORE_UNSELECTED, BEFORE_SELECTED, UNSUBSCRIBED, PAGE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{5}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, DWithNoSelectionsRebuildsThePageFromRemainingUnreadGroups)
{
    prepare_groups();
    clear_selections();

    const DisplayState result = newsgroup_commands('D');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_caught_up(PAGE_UNSELECTED, false);
    expect_caught_up(PAGE_SELECTED, false);
    for (int index : {BEFORE_UNSELECTED, BEFORE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_groups[AFTER_UNSELECTED], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, DExitsWhenReadingTheWholePageLeavesNoUnreadGroups)
{
    NewsgroupData &first = add_selector_group("comp.first", 7);
    NewsgroupData &last = add_selector_group("comp.last", 7);
    initialize_page();

    const DisplayState result = newsgroup_commands('D');

    EXPECT_EQ(DS_QUIT, result);
    EXPECT_EQ(TR_NONE, first.m_to_read);
    EXPECT_EQ(TR_NONE, last.m_to_read);
    EXPECT_EQ(" 1-10", first.rc_numbers());
    EXPECT_EQ(" 1-10", last.rc_numbers());
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
}

TEST_F(NewsgroupSelectorBulkStateTest, XWithNoSelectionsReadsEveryIncludedGroup)
{
    prepare_groups();
    clear_selections();

    const DisplayState result = newsgroup_commands('X');

    EXPECT_EQ(DS_QUIT, result);
    for (int index :
         {BEFORE_UNSELECTED, BEFORE_SELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED})
    {
        expect_caught_up(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, JWithNoSelectionsRebuildsWithoutChangingReadState)
{
    prepare_groups();
    clear_selections();

    const DisplayState result = newsgroup_commands('J');

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_context();
}

TEST_F(NewsgroupSelectorBulkStateTest, JExitsWhenAllUnreadGroupsAreSelected)
{
    prepare_groups();
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED})
    {
        m_groups[index]->m_flags |= NF_SEL;
    }
    initialize_page();
    remember_groups();

    const DisplayState result = newsgroup_commands('J');

    EXPECT_EQ(DS_QUIT, result);
    for (int index :
         {BEFORE_UNSELECTED, BEFORE_SELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED})
    {
        expect_caught_up(index, false);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
}

TEST_P(NewsgroupSelectorReadBulkTest, readModePreservesReadRangesAndOnlyJDeselectsRereadGroups)
{
    prepare_groups(true);

    const DisplayState result = newsgroup_commands(GetParam());

    EXPECT_EQ(GetParam() == 'J' ? DS_DISPLAY : DS_QUIT, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index, GetParam() == 'J' ? NF_DEL_SEL : NF_NONE);
    }
    EXPECT_EQ(GetParam() == 'J' ? 0 : 3, g_selected_count);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_context();
}

INSTANTIATE_TEST_SUITE_P(Command, NewsgroupSelectorReadBulkTest, testing::Values('X', 'D', 'J'));

TEST_P(NewsgroupSelectorUnsubscribedBulkTest, bulkCommandsPreserveUnsubscribedReadStateEvenWithPendingKillMarks)
{
    const auto [command, pending] = GetParam();
    prepare_groups();
    if (pending || command != 'J')
    {
        m_groups[UNSUBSCRIBED]->m_flags &= ~NF_SEL;
    }
    if (pending)
    {
        m_groups[UNSUBSCRIBED]->m_flags |= NF_DEL;
    }
    remember_groups();

    const DisplayState result = newsgroup_commands(command);

    EXPECT_EQ(command == 'J' ? DS_DISPLAY : DS_QUIT, result);
    expect_preserved(UNSUBSCRIBED, pending ? NF_DEL : NF_NONE);
    expect_context();
}

INSTANTIATE_TEST_SUITE_P(CommandAndPendingKill, NewsgroupSelectorUnsubscribedBulkTest,
                         testing::Combine(testing::Values('X', 'D', 'J'), testing::Bool()));

TEST_P(NewsgroupSelectorEmptyBulkTest, anEmptyCatalogKeepsAnEmptyPopulationWithoutChangingReadState)
{
    const auto [rereading, command] = GetParam();
    set_read_scope(rereading);
    initialize_page();

    const DisplayState result = newsgroup_commands(command);

    EXPECT_EQ(rereading && command == 'J' ? DS_DISPLAY : DS_QUIT, result);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCommand, NewsgroupSelectorEmptyBulkTest,
                         testing::Combine(testing::Bool(), testing::Values('X', 'D', 'J')));

TEST_P(NewsgroupSelectorInlineReadScopeTest, deselectionKeepsReadRangesAndInactiveSelectionsAcrossPages)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline("-");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : SELECTED)
    {
        expect_preserved(index, static_cast<NewsgroupFlags>(g_sel.mask), GetParam() ? NF_DEL : NF_NONE);
    }
    for (int index : UNSELECTED)
    {
        expect_preserved(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(GetParam() ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("3 newsgroups deselected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineReadScopeTest, doubledColonSelectsUnselectedGroupsAcrossPagesAndClearsTheirKillMarks)
{
    prepare_inline(GetParam());
    m_groups[PAGE_UNSELECTED]->m_flags |= NF_DEL;
    remember_groups();

    const DisplayState result = run_inline(":+");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : UNSELECTED)
    {
        expect_preserved(index, NF_DEL, static_cast<NewsgroupFlags>(g_sel.mask));
    }
    for (int index : SELECTED)
    {
        expect_preserved(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(6, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(GetParam() ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("3 newsgroups selected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineReadScopeTest, selectingAlreadySelectedGroupsPreservesTheirState)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline("+");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 newsgroups matched.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineReadScopeTest, deselectingAlreadyUnselectedGroupsPreservesTheirState)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline(":-");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 newsgroups matched.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineScopeTest, catchupUsesTheRequestedSelectionScopeAcrossPages)
{
    prepare_inline();
    const std::array<int, 3> &targets = GetParam() ? UNSELECTED : SELECTED;
    const std::array<int, 3> &others = GetParam() ? SELECTED : UNSELECTED;

    const DisplayState result = run_inline(GetParam() ? ":c" : "c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : targets)
    {
        expect_caught_up(index, false);
    }
    for (int index : others)
    {
        expect_preserved(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(GetParam() ? 3 : 0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(GetParam() ? "3 newsgroups less." : "3 newsgroups less, 3 deselected.", g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_inline_context();
}

TEST_F(NewsgroupSelectorInlineStateTest, catchupOfSelectedReadGroupsDeselectsThemWithoutChangingReadRanges)
{
    prepare_inline(true);

    const DisplayState result = run_inline("c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : SELECTED)
    {
        expect_preserved(index, NF_DEL_SEL, NF_DEL);
    }
    for (int index : UNSELECTED)
    {
        expect_preserved(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 newsgroups deselected.", g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineScopeTest, unsubscribePreservesReadRangesAndUpdatesOnlyTheRequestedUnreadGroups)
{
    prepare_inline();
    const std::array<int, 3> &targets = GetParam() ? UNSELECTED : SELECTED;
    const std::array<int, 3> &others = GetParam() ? SELECTED : UNSELECTED;

    const DisplayState result = run_inline(GetParam() ? ":u" : "u");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : targets)
    {
        expect_unsubscribed(index);
    }
    for (int index : others)
    {
        expect_preserved(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(GetParam() ? 3 : 0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(GetParam() ? "3 newsgroups less." : "3 newsgroups less, 3 deselected.", g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineScopeTest, unsubscribingReadGroupsKeepsTheUnreadPopulationAndReportsSelectionChanges)
{
    prepare_inline(true);
    m_groups[BEFORE_UNSELECTED]->m_to_read = 7;
    m_groups[BEFORE_UNSELECTED]->m_rc_line.resize(m_groups[BEFORE_UNSELECTED]->m_num_offset);
    m_groups[BEFORE_UNSELECTED]->m_rc_line += " 1-3";
    ++g_newsgroup_to_read;
    initialize_page();
    remember_groups();

    const DisplayState result = run_inline(GetParam() ? ":u" : "u");

    EXPECT_EQ(DS_DISPLAY, result);
    if (GetParam())
    {
        expect_unsubscribed(PAGE_UNSELECTED);
        expect_unsubscribed(AFTER_UNSELECTED);
        for (int index : SELECTED)
        {
            expect_preserved(index);
        }
    }
    else
    {
        for (int index : SELECTED)
        {
            expect_unsubscribed(index);
        }
        expect_preserved(PAGE_UNSELECTED);
        expect_preserved(AFTER_UNSELECTED);
    }
    expect_preserved(BEFORE_UNSELECTED);
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(GetParam() ? 3 : 0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(GetParam() ? 3 : 2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(GetParam() ? 2 : 3, g_perform_count);
    EXPECT_EQ(GetParam() ? "2 newsgroups matched." : "3 newsgroups deselected.", g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_inline_context();
}

TEST_F(NewsgroupSelectorInlineStateTest, selectedCommandsRetainSelectionsOutsideANameRestriction)
{
    prepare_inline();
    set_newsgroup_to_do("comp.*");
    initialize_page();
    remember_groups();

    const DisplayState result = run_inline("c");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_caught_up(PAGE_SELECTED, false);
    expect_caught_up(AFTER_SELECTED, false);
    for (int index : {BEFORE_UNSELECTED, BEFORE_SELECTED, PAGE_UNSELECTED, AFTER_UNSELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(1, g_only.max_to_do);
    expect_inline_context();
}

TEST_F(NewsgroupSelectorInlineStateTest, doubledColonAddressesUnselectedGroupsOutsideANameRestriction)
{
    prepare_inline();
    set_newsgroup_to_do("comp.*");
    initialize_page();
    remember_groups();

    const DisplayState result = run_inline(":c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : UNSELECTED)
    {
        expect_caught_up(index, false);
    }
    for (int index : SELECTED)
    {
        expect_preserved(index);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(1, g_only.max_to_do);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineScopeTest, catchupDistinguishesSelectedDisplayFromExplicitUnselectedScope)
{
    prepare_inline();
    g_sel.exclusive = true;
    g_sel_page.page_ngd = nullptr;
    initialize_page();
    remember_groups();
    const std::array<int, 3> &targets = GetParam() ? UNSELECTED : SELECTED;
    const std::array<int, 3> &others = GetParam() ? SELECTED : UNSELECTED;

    const DisplayState result = run_inline(GetParam() ? ":c" : "c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : targets)
    {
        expect_caught_up(index, false);
    }
    for (int index : others)
    {
        expect_preserved(index, NF_NONE, GetParam() ? NF_NONE : NF_INCLUDED);
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(GetParam(), g_sel.exclusive);
    EXPECT_EQ(GetParam() ? 3 : 0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_perform_count);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineScopeTest, commandsSkipGroupsOutsideTheUnreadThresholdAndSubscriptionScope)
{
    prepare_inline();
    m_groups[BEFORE_UNSELECTED]->m_to_read = 2;
    m_groups[BEFORE_SELECTED]->m_to_read = TR_NONE;
    m_groups[AFTER_UNSELECTED]->m_to_read = TR_BOGUS;
    g_newsgroup_to_read -= NewsgroupNum{2};
    g_newsgroup_min_to_read = 3;
    initialize_page();
    remember_groups();

    const DisplayState result = run_inline(GetParam() ? ":c" : "c");

    EXPECT_EQ(DS_DISPLAY, result);
    if (GetParam())
    {
        expect_caught_up(PAGE_UNSELECTED, false);
        expect_preserved(PAGE_SELECTED);
        expect_preserved(AFTER_SELECTED);
    }
    else
    {
        expect_caught_up(PAGE_SELECTED, false);
        expect_caught_up(AFTER_SELECTED, false);
        expect_preserved(PAGE_UNSELECTED);
    }
    for (int index : {BEFORE_UNSELECTED, BEFORE_SELECTED, AFTER_UNSELECTED, UNSUBSCRIBED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(GetParam() ? NewsgroupNum{3} : NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(GetParam() ? 1 : 2, g_perform_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineReadScopeTest, selectedCommandWithNoSelectionLeavesEveryGroupAlone)
{
    prepare_inline(GetParam());
    clear_selections();

    const DisplayState result = run_inline("c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineReadScopeTest, doubledColonWithNoSelectionSelectsEveryEligibleGroup)
{
    prepare_inline(GetParam());
    clear_selections();

    const DisplayState result = run_inline(":+");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index :
         {BEFORE_UNSELECTED, BEFORE_SELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_UNSELECTED, AFTER_SELECTED})
    {
        expect_preserved(index, NF_DEL, static_cast<NewsgroupFlags>(g_sel.mask));
    }
    expect_preserved(UNSUBSCRIBED);
    EXPECT_EQ(6, g_selected_count);
    EXPECT_EQ(6, g_perform_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineReadScopeTest, doubledColonWithEveryEligibleGroupSelectedLeavesGroupsAlone)
{
    prepare_inline(GetParam());
    for (int index : UNSELECTED)
    {
        m_groups[index]->m_flags |= static_cast<NewsgroupFlags>(g_sel.mask);
    }
    initialize_page();
    remember_groups();

    const DisplayState result = run_inline(":c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(6, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_F(NewsgroupSelectorInlineStateTest, eachCommandInAListRunsForTheOriginallySelectedGroups)
{
    prepare_inline();

    const DisplayState result = run_inline("- : +");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 newsgroups matched.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_inline_context();
}

TEST_P(NewsgroupSelectorInlineEmptyTest, anEmptyCatalogReportsNoAffectedGroupsWithoutChangingState)
{
    const auto [rereading, unselected] = GetParam();
    set_read_scope(rereading);
    initialize_page();

    const DisplayState result = run_inline(unselected ? ":+" : "+");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorInlineReadScopeTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(UnselectedScope, NewsgroupSelectorInlineScopeTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadAndSelectionScope, NewsgroupSelectorInlineEmptyTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorDotScopeTest, deselectionUsesTheHighlightedGroupRatherThanTheLastVisitedGroup)
{
    const auto [rereading, others] = GetParam();
    prepare_dot();
    const NewsgroupFlags selection = static_cast<NewsgroupFlags>(g_sel.mask);

    const DisplayState result = run_dot("-");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool selected = affects(index) && (m_flags[index] & selection);
        expect_preserved(index, selected ? selection : NF_NONE, selected && rereading ? NF_DEL : NF_NONE);
    }
    EXPECT_EQ(others ? 1 : 2, g_selected_count);
    EXPECT_EQ(others ? 5 : 1, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(others ? "5 newsgroups matched, 2 deselected." : "1 newsgroup deselected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_dot_context(1);
}

TEST_P(NewsgroupSelectorDotScopeTest, selectionActsOnTheRequestedGroupsRegardlessOfTheirPriorSelection)
{
    const auto [rereading, others] = GetParam();
    prepare_dot(PAGE_UNSELECTED);
    m_groups[PAGE_UNSELECTED]->m_flags |= NF_DEL;
    m_groups[AFTER_UNSELECTED]->m_flags |= NF_DEL;
    remember_groups();

    const DisplayState result = run_dot("+");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool selected = affects(index) && !(m_flags[index] & static_cast<NewsgroupFlags>(g_sel.mask));
        expect_preserved(index, selected ? NF_DEL : NF_NONE,
                         selected ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE);
    }
    EXPECT_EQ(others ? 5 : 4, g_selected_count);
    EXPECT_EQ(others ? 5 : 1, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(others ? "5 newsgroups matched, 2 selected." : "1 newsgroup selected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_dot_context(0);
}

TEST_P(NewsgroupSelectorDotScopeTest, catchupChangesOnlyTheRequestedReadRangesAndSelections)
{
    const auto [rereading, others] = GetParam();
    prepare_dot();

    const DisplayState result = run_dot("c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        if (!affects(index))
        {
            expect_preserved(index);
        }
        else if (rereading)
        {
            const bool selected = m_flags[index] & NF_DEL_SEL;
            expect_preserved(index, selected ? NF_DEL_SEL : NF_NONE, selected ? NF_DEL : NF_NONE);
        }
        else
        {
            expect_caught_up(index, false);
        }
    }
    EXPECT_EQ(others ? 1 : 2, g_selected_count);
    EXPECT_EQ(others ? 5 : 1, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{others ? 1 : 5}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? (others ? "5 newsgroups matched, 2 deselected." : "1 newsgroup deselected.")
                        : (others ? "5 newsgroups less, 2 deselected." : "1 newsgroup less, 1 deselected."),
              g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_dot_context(rereading ? 1 : 0);
}

TEST_P(NewsgroupSelectorDotScopeTest, unsubscribeRetainsSavedRangesAndKeepsTheOtherSubscriptions)
{
    const auto [rereading, others] = GetParam();
    prepare_dot();

    const DisplayState result = run_dot("u");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        if (affects(index))
        {
            expect_unsubscribed(index);
        }
        else
        {
            expect_preserved(index);
        }
    }
    EXPECT_EQ(others ? 1 : 2, g_selected_count);
    EXPECT_EQ(others ? 5 : 1, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{others ? 1 : 5}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? (others ? "5 newsgroups matched, 2 deselected." : "1 newsgroup deselected.")
                        : (others ? "5 newsgroups less, 2 deselected." : "1 newsgroup less, 1 deselected."),
              g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_dot_context(0);
}

TEST_P(NewsgroupSelectorDotScopeTest, aCommandListRetainsItsOriginalCurrentGroupScope)
{
    const auto [rereading, others] = GetParam();
    prepare_dot();
    for (NewsgroupData *group : m_groups)
    {
        group->m_flags |= static_cast<NewsgroupFlags>(g_sel.mask);
    }
    initialize_page();
    remember_groups();

    const DisplayState result = run_dot("- : +");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index, affects(index) ? NF_DEL : NF_NONE);
    }
    EXPECT_EQ(6, g_selected_count);
    EXPECT_EQ(others ? 5 : 1, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(others ? "5 newsgroups matched." : "1 newsgroup matched.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_dot_context(1);
}

TEST_P(NewsgroupSelectorDotScopeTest, anEmptyCatalogDoesNotReuseThePreviousOperationCount)
{
    const auto [rereading, others] = GetParam();
    set_read_scope(rereading);
    initialize_page();
    g_perform_count = 23;

    const DisplayState result = run_dot("+");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorDotScopeTest, anEndCursorDoesNotApplyDotCommandsToAnyGroup)
{
    const auto [rereading, others] = GetParam();
    prepare_dot();
    g_sel_page.item_index = g_sel_page.page_item_cnt;
    g_newsgroup_ptr = nullptr;
    g_perform_count = 23;

    const DisplayState result = run_dot("u");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

TEST_P(NewsgroupSelectorDotScopeTest, otherGroupCatchupSkipsTheOppositeReadScopeAndInvalidGroups)
{
    const auto [rereading, others] = GetParam();
    prepare_dot();
    m_groups[BEFORE_UNSELECTED]->m_to_read = rereading ? 7 : 0;
    m_groups[BEFORE_UNSELECTED]->m_rc_line.resize(m_groups[BEFORE_UNSELECTED]->m_num_offset);
    m_groups[BEFORE_UNSELECTED]->m_rc_line += rereading ? " 1-3" : " 1-10";
    m_groups[AFTER_UNSELECTED]->m_to_read = TR_BOGUS;
    g_newsgroup_to_read = NewsgroupNum{rereading ? 1 : 4};
    initialize_page();
    remember_groups();

    const DisplayState result = run_dot("c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        if (!affects(index) || index == BEFORE_UNSELECTED || index == AFTER_UNSELECTED)
        {
            expect_preserved(index);
        }
        else if (rereading)
        {
            const bool selected = m_flags[index] & NF_DEL_SEL;
            expect_preserved(index, selected ? NF_DEL_SEL : NF_NONE, selected ? NF_DEL : NF_NONE);
        }
        else
        {
            expect_caught_up(index, false);
        }
    }
    EXPECT_EQ(others ? 1 : 2, g_selected_count);
    EXPECT_EQ(others ? 3 : 1, g_perform_count);
    EXPECT_EQ(NewsgroupNum{rereading ? 1 : (others ? 1 : 3)}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_dot_context(rereading ? 1 : 0);
}

TEST_P(NewsgroupSelectorDotScopeTest, aSingleGroupCatalogHasNoOtherGroupTargets)
{
    const auto [rereading, others] = GetParam();
    set_read_scope(rereading);
    NewsgroupData &group = add_selector_group("comp.only", rereading ? 0 : 7, NF_VISIT | NF_UNTHREADED);
    set_newsgroup(&group);
    initialize_page();
    const NewsgroupFlags flags = group.m_flags;
    const std::string    numbers{group.rc_numbers()};

    const DisplayState result = run_dot("+");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(others ? 0 : 1, g_perform_count);
    EXPECT_EQ(others ? 0 : 1, g_selected_count);
    EXPECT_EQ((flags & ~(others ? NF_NONE : NF_DEL)) | (others ? NF_NONE : static_cast<NewsgroupFlags>(g_sel.mask)),
              group.m_flags);
    EXPECT_EQ(rereading ? 0 : 7, group.m_to_read);
    EXPECT_EQ(numbers, group.rc_numbers());
    EXPECT_EQ(':', group.m_subscribe_char);
    EXPECT_EQ(&group, g_newsgroup_ptr);
    EXPECT_EQ("comp.only", g_newsgroup_name);
    EXPECT_EQ(nullptr, g_current_newsgroup);
    EXPECT_EQ(nullptr, g_recent_newsgroup);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(others ? "No newsgroups affected." : "1 newsgroup selected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
}

INSTANTIATE_TEST_SUITE_P(ReadAndOtherGroupScope, NewsgroupSelectorDotScopeTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorSearchPatternTest, aDefaultSearchSelectsEligibleMatchesAcrossPages)
{
    const auto &[rereading, pattern] = GetParam();
    prepare_search(rereading);

    const DisplayState result = run_search(pattern.input);

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection(pattern.matches);
    EXPECT_EQ(pattern.selected_count, g_selected_count);
    EXPECT_EQ(pattern.matches.size(), g_perform_count) << m_output;
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    expect_search_context();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPattern, NewsgroupSelectorSearchPatternTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(NewsgroupSearchPattern{"comp*", {0, 2, 4, 6}, 5},
                                                          NewsgroupSearchPattern{"selected/", {0, 1, 2, 4, 5, 6}, 6},
                                                          NewsgroupSearchPattern{"^comp.page-selected$/", {4}, 3},
                                                          NewsgroupSearchPattern{"^comp.page-?nselected$/", {2}, 4},
                                                          NewsgroupSearchPattern{"*page*/", {2, 4}, 4},
                                                          NewsgroupSearchPattern{"^comp.[ap]*-selected$/", {4, 6}, 3},
                                                          NewsgroupSearchPattern{"^COMP.PAGE-SELECTED$/", {4}, 3},
                                                          NewsgroupSearchPattern{"^missing$/", {}, 3})));

TEST_P(NewsgroupSelectorSearchModifierTest, theReadModifierIncludesOppositeScopeGroupsWithoutSelectingInvalidGroups)
{
    const auto [rereading, include_read] = GetParam();
    prepare_mixed_search(rereading);

    const DisplayState result = run_search(include_read ? "comp*/r" : "comp*/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection(include_read ? std::vector<int>{0, 2, 4, 6} : std::vector<int>{0, 4, 6});
    EXPECT_EQ(include_read ? 5 : 4, g_selected_count);
    EXPECT_EQ(include_read ? 4 : 3, g_perform_count);
    EXPECT_EQ(include_read ? 5 : 4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(NewsgroupNum{rereading ? 1 : 4}, g_newsgroup_to_read);
    EXPECT_EQ(include_read ? "4 newsgroups matched, 2 selected." : "3 newsgroups matched, 1 selected.", g_msg);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, anEmptyPatternReusesThePreviousGroupPattern)
{
    const bool rereading = GetParam();
    seed_search(rereading, "comp*/");

    const DisplayState result = run_search("/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({0, 4, 6});
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 newsgroups selected.", g_msg);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, aRepeatedPatternRetainsItsReadModifier)
{
    const bool rereading = GetParam();
    seed_search(rereading, "comp*/r");

    const DisplayState result = run_search("/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({0, 2, 4, 6});
    EXPECT_EQ(4, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ("4 newsgroups selected.", g_msg);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, aNewPatternResetsThePreviousReadModifier)
{
    const bool rereading = GetParam();
    seed_search(rereading, "alt*/r");

    const DisplayState result = run_search("comp*/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({0, 4, 6});
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 newsgroups selected.", g_msg);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchDisplayTest, aSearchSelectsMatchingGroupsWhileRetainingTheDisplayFilter)
{
    const auto [rereading, restricted] = GetParam();
    prepare_search(rereading);
    if (restricted)
    {
        set_newsgroup_to_do("comp.*");
    }
    else
    {
        g_sel.exclusive = true;
    }
    initialize_page();
    remember_groups();

    const DisplayState result = run_search("comp*/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({0, 2, 4, 6});
    EXPECT_EQ(restricted ? 4 : 5, g_selected_count);
    EXPECT_EQ(restricted ? 4 : 5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(!restricted, g_sel.exclusive);
    const std::vector<std::string> included =
        restricted ? std::vector<std::string>{"comp.before-unselected", "comp.page-unselected", "comp.page-selected",
                                              "comp.after-selected"}
                   : std::vector<std::string>{"comp.before-unselected", "alt.before-selected", "comp.page-unselected",
                                              "comp.page-selected", "comp.after-selected"};
    EXPECT_EQ(included, groups_with_flag(NF_INCLUDED));
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, anExplicitPatternCanSelectGroupsOutsideADisplayRestriction)
{
    prepare_search(GetParam());
    set_newsgroup_to_do("comp.*");
    initialize_page();
    remember_groups();

    const DisplayState result = run_search("alt*/");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index, index == AFTER_UNSELECTED ? NF_DEL : NF_NONE,
                         index == AFTER_UNSELECTED ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE);
    }
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ("2 newsgroups matched, 1 selected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, anEmptyCatalogReportsNoAffectedGroups)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    initialize_page();

    const DisplayState result = run_search("missing/");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(nullptr, g_current_newsgroup);
    EXPECT_EQ(nullptr, g_recent_newsgroup);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, aSearchBeforeAnyGroupVisitKeepsTheCurrentGroupAbsent)
{
    const bool rereading = GetParam();
    prepare_search(rereading);
    g_current_newsgroup = nullptr;
    g_recent_newsgroup = nullptr;

    const DisplayState result = run_search("comp*/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({0, 2, 4, 6});
    EXPECT_EQ(5, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(nullptr, g_current_newsgroup);
    EXPECT_EQ(nullptr, g_recent_newsgroup);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorSearchReadScopeTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadScopeAndModifier, NewsgroupSelectorSearchModifierTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ReadScopeAndDisplay, NewsgroupSelectorSearchDisplayTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorSearchActionTest, explicitSelectionChangesOnlyMatchingGroups)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);

    const DisplayState result = run_search("comp*/:+");

    EXPECT_EQ(clean ? DS_UPDATE : DS_DISPLAY, result);
    expect_search_selection({0, 2, 4, 6});
    EXPECT_EQ(5, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(clean ? 3 : 2, g_term_line);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("4 newsgroups matched, 2 selected.", g_msg);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, explicitDeselectionRetainsNonmatchesAndSavedRanges)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);
    const NewsgroupFlags selection = static_cast<NewsgroupFlags>(g_sel.mask);

    const DisplayState result = run_search("comp*/:-");

    EXPECT_EQ(clean ? DS_UPDATE : DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool deselected = index == PAGE_SELECTED || index == AFTER_SELECTED;
        expect_preserved(index, deselected ? selection : NF_NONE, rereading && deselected ? NF_DEL : NF_NONE);
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ("4 newsgroups matched, 2 deselected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, catchupUpdatesOnlyMatchingReadRangesAndSelections)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);

    const DisplayState result = run_search("comp*/:c");

    EXPECT_EQ(clean && rereading ? DS_UPDATE : DS_DISPLAY, result);
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_read_target(index, rereading);
    }
    for (int index : {BEFORE_SELECTED, UNSUBSCRIBED, AFTER_UNSELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(rereading ? 6 : 2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? "4 newsgroups matched, 2 deselected." : "4 newsgroups less, 2 deselected.", g_msg);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, unsubscribeRemovesMatchingRowsAndPreservesTheirReadRanges)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);

    const DisplayState result = run_search("comp*/:u");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_unsubscribed(index);
    }
    for (int index : {BEFORE_SELECTED, UNSUBSCRIBED, AFTER_UNSELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, catchupThenUnsubscribeMarksReadBeforeRemovingMatchingRows)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);
    const NewsgroupFlags selection = static_cast<NewsgroupFlags>(g_sel.mask);

    const DisplayState result = run_search("comp*/:c : u");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        const bool selected = m_flags[index] & selection;
        EXPECT_EQ(TR_UNSUB, m_groups[index]->m_to_read);
        EXPECT_EQ(" 1-10", m_groups[index]->rc_numbers());
        EXPECT_EQ(((m_flags[index] | (rereading && selected ? NF_DEL : NF_NONE)) & ~(selection | NF_INCLUDED)),
                  m_groups[index]->m_flags);
        expect_identity(index, true);
    }
    for (int index : {BEFORE_SELECTED, UNSUBSCRIBED, AFTER_UNSELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, catchupThenSelectionKeepsTheOriginalMatchesAvailable)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);

    const DisplayState result = run_search("comp*/:c:+");

    EXPECT_EQ(clean && rereading ? DS_UPDATE : DS_DISPLAY, result);
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        EXPECT_EQ(TR_NONE, m_groups[index]->m_to_read);
        EXPECT_EQ(" 1-10", m_groups[index]->rc_numbers());
        EXPECT_EQ((m_flags[index] & ~NF_DEL) | static_cast<NewsgroupFlags>(g_sel.mask) | NF_INCLUDED,
                  m_groups[index]->m_flags);
        expect_identity(index);
    }
    for (int index : {BEFORE_SELECTED, UNSUBSCRIBED, AFTER_UNSELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(5, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, aMissDoesNotApplyItsUnsubscribeAction)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean);

    const DisplayState result = run_search("missing/:u");

    EXPECT_EQ(clean ? DS_UPDATE : DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index);
    }
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(6, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionTest, deselectionRebuildsAnExclusiveDisplayAfterMatchingRowsDisappear)
{
    const auto [rereading, clean] = GetParam();
    prepare_actions(rereading, clean, true);
    const NewsgroupFlags selection = static_cast<NewsgroupFlags>(g_sel.mask);

    const DisplayState result = run_search("comp*/:-");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool deselected = index == PAGE_SELECTED || index == AFTER_SELECTED;
        expect_preserved(index, deselected ? selection | NF_INCLUDED : NF_NONE,
                         rereading && deselected ? NF_DEL : NF_NONE);
    }
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionModifierTest, onlyEligibleMatchingGroupsReceiveTheUnsubscribeAction)
{
    const auto [rereading, include_read] = GetParam();
    prepare_mixed_search(rereading);
    prepare_screen(false);

    const DisplayState result = run_search(include_read ? "comp*/r:u" : "comp*/:u");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_UNSELECTED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_unsubscribed(index);
    }
    if (include_read)
    {
        expect_unsubscribed(PAGE_UNSELECTED);
    }
    else
    {
        expect_preserved(PAGE_UNSELECTED);
    }
    for (int index : {BEFORE_SELECTED, UNSUBSCRIBED, AFTER_UNSELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(include_read ? 4 : 3, g_perform_count);
    EXPECT_EQ(NewsgroupNum{rereading && include_read ? 0 : 1}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchReadScopeTest, aCatchupActionCanAddressGroupsHiddenByADisplayRestriction)
{
    const bool rereading = GetParam();
    prepare_search(rereading);
    set_newsgroup_to_do("comp.*");
    initialize_page();
    remember_groups();

    const DisplayState result = run_search("alt*/:c");

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index : {BEFORE_SELECTED, AFTER_UNSELECTED})
    {
        if (rereading)
        {
            expect_preserved(index, index == BEFORE_SELECTED ? NF_DEL_SEL : NF_NONE,
                             index == BEFORE_SELECTED ? NF_DEL : NF_NONE);
        }
        else
        {
            expect_caught_up(index, false);
        }
    }
    for (int index : {BEFORE_UNSELECTED, PAGE_UNSELECTED, UNSUBSCRIBED, PAGE_SELECTED, AFTER_SELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

TEST_P(NewsgroupSelectorSearchActionFallbackTest,
       replacingAnEmptyRestrictionRedrawsEvenWhenThePopulationSizeIsUnchanged)
{
    prepare_search(true);
    for (int index : {BEFORE_UNSELECTED, AFTER_SELECTED})
    {
        NewsgroupData &group = *m_groups[index];
        group.m_to_read = 7;
        group.m_rc_line.resize(group.m_num_offset);
        group.m_rc_line += " 1-3";
        group.m_flags &= ~NF_DEL_SEL;
    }
    g_newsgroup_to_read = NewsgroupNum{2};
    set_newsgroup_to_do("comp.page*");
    initialize_page();
    remember_groups();
    prepare_screen(GetParam());

    const DisplayState result = run_search("comp.page*/:u");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_unsubscribed(PAGE_UNSELECTED);
    expect_unsubscribed(PAGE_SELECTED);
    expect_preserved(BEFORE_SELECTED, NF_NONE, NF_INCLUDED);
    expect_preserved(AFTER_UNSELECTED, NF_NONE, NF_INCLUDED | NF_DEL);
    for (int index : {BEFORE_UNSELECTED, UNSUBSCRIBED, AFTER_SELECTED})
    {
        expect_preserved(index);
    }
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(m_groups[BEFORE_SELECTED], g_sel_page.page_ngd);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_search_context();
}

INSTANTIATE_TEST_SUITE_P(Screen, NewsgroupSelectorSearchActionFallbackTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadScopeAndScreen, NewsgroupSelectorSearchActionTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ReadScopeAndModifier, NewsgroupSelectorSearchActionModifierTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorPromptCancelTest, cancellingThePromptLeavesTheNextCommandQueued)
{
    const auto [command, editing] = GetParam();
    prepare_prompt(command, false, true);

    const DisplayState result = run_prompt(command, std::string{editing} + "q");

    EXPECT_EQ(DS_ASK, result);
    expect_catalog_preserved();
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(m_groups[PAGE_SELECTED], g_newsgroup_ptr);
    EXPECT_EQ('q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(CommandAndEditing, NewsgroupSelectorPromptCancelTest,
                         testing::Combine(testing::Values(':', '/'), testing::Values("\b", "partial\025")));

TEST_P(NewsgroupSelectorPromptCompletionTest, anEditedCommandRunsOnlyAfterCarriageReturn)
{
    const auto [command, rereading] = GetParam();
    prepare_prompt(command, rereading, true);
    const std::string input = command == ':' ? "+\b-\rQ" : "comp.page*/:+\b-\rQ";

    const DisplayState result = run_prompt(command, input);

    EXPECT_EQ(DS_UPDATE, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool deselected = command == ':'
                                    ? index == BEFORE_SELECTED || index == PAGE_SELECTED || index == AFTER_SELECTED
                                    : index == PAGE_SELECTED;
        expect_preserved(index, deselected ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE,
                         deselected && rereading ? NF_DEL : NF_NONE);
    }
    EXPECT_EQ(command == ':' ? 0 : 2, g_selected_count);
    EXPECT_EQ(command == ':' ? 3 : 2, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    if (command == '/')
    {
        expect_search_context();
    }
    else
    {
        expect_inline_context();
    }
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

TEST_P(NewsgroupSelectorPromptCompletionTest, aScreenRefreshDuringEntryPreservesThePartialCommand)
{
    const auto [command, rereading] = GetParam();
    prepare_prompt(command, rereading, false);
    const std::string input = command == ':' ? "-\f\n\nQ" : "comp.page*/:-\f\n\nQ";

    const DisplayState result = run_prompt(command, input);

    EXPECT_EQ(DS_DISPLAY, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        const bool deselected = command == ':'
                                    ? index == BEFORE_SELECTED || index == PAGE_SELECTED || index == AFTER_SELECTED
                                    : index == PAGE_SELECTED;
        expect_preserved(index, deselected ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE,
                         deselected && rereading ? NF_DEL : NF_NONE);
    }
    EXPECT_EQ(command == ':' ? 0 : 2, g_selected_count);
    EXPECT_EQ(command == ':' ? 3 : 2, g_perform_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(CommandAndReadScope, NewsgroupSelectorPromptCompletionTest,
                         testing::Combine(testing::Values(':', '/'), testing::Bool()));

TEST_P(NewsgroupSelectorSearchPromptErrorTest, anInvalidSearchReportsItsDiagnosticWithoutChangingGroups)
{
    const auto [clean, scenario] = GetParam();
    const auto [input, diagnostic] = scenario;
    prepare_actions(false, clean);

    const DisplayState result = run_prompt('/', input);

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find(diagnostic));
    EXPECT_EQ(std::string::npos, m_output.find("Interrupted"));
    expect_catalog_preserved();
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    EXPECT_FALSE(g_error_occurred);
    expect_search_context();
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(ScreenAndPattern, NewsgroupSelectorSearchPromptErrorTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(std::pair<std::string_view, std::string_view>{"[abc/\n\nQ",
                                                                                                        "Missing ]"},
                                                          std::pair<std::string_view, std::string_view>{
                                                              "\n\nQ", "No previous search pattern"})));

TEST_P(NewsgroupSelectorPromptErrorTest, anUnknownActionRetainsCompletedEffectsAndItsSpecificDiagnostic)
{
    const auto [command, clean] = GetParam();
    prepare_prompt(command, false, clean);
    const int         target = command == ':' ? BEFORE_SELECTED : PAGE_UNSELECTED;
    const std::string input = command == ':' ? "-:X:+\n\nQ" : "comp.page*/:+:X:-\n\nQ";

    const DisplayState result = run_prompt(command, input);

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find(command == ':' ? "Unknown command: X:+" : "Unknown command: X:-"));
    EXPECT_EQ(std::string::npos, m_output.find("Interrupted"));
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        expect_preserved(index, index == target && command == ':' ? NF_SEL : NF_NONE,
                         index == target && command == '/' ? NF_SEL : NF_NONE);
    }
    EXPECT_EQ(command == ':' ? 2 : 4, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    EXPECT_FALSE(g_error_occurred);
    EXPECT_EQ(0, g_sel_page.item_index);
    if (command == '/')
    {
        expect_search_context();
    }
    else
    {
        expect_inline_context();
    }
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(CommandAndScreen, NewsgroupSelectorPromptErrorTest,
                         testing::Combine(testing::Values(':', '/'), testing::Bool()));

TEST_F(NewsgroupSelectorRecoveryStateTest, aSearchCanBeRetriedAfterAnInvalidPattern)
{
    prepare_actions(false, true);
    ASSERT_EQ(DS_DISPLAY, run_prompt('/', "[abc/\n\n"));
    prepare_screen(true);

    const DisplayState result = run_prompt('/', "comp.page*/\nQ");

    EXPECT_EQ(DS_UPDATE, result);
    expect_search_selection({PAGE_UNSELECTED, PAGE_SELECTED});
    EXPECT_EQ(4, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(std::string::npos, m_output.find("Missing ]"));
    EXPECT_FALSE(g_error_occurred);
    expect_search_context();
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

TEST_P(NewsgroupSelectorInlineRecoveryTest, anEmptyInlineCommandDoesNotRepeatThePreviousStatus)
{
    prepare_prompt(':', GetParam(), true);
    ASSERT_EQ(DS_UPDATE, run_prompt(':', "-\n"));
    remember_groups();
    prepare_screen(true);

    const DisplayState result = run_prompt(':', "\nQ");

    EXPECT_EQ(DS_UPDATE, result);
    expect_catalog_preserved();
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(GetParam() ? NewsgroupNum{} : NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ("No newsgroups affected.", g_msg);
    expect_inline_context();
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

TEST_P(NewsgroupSelectorInlineRecoveryTest, cancellingAfterAnInterruptedListRetainsCompletedActions)
{
    const bool rereading = GetParam();
    prepare_prompt(':', rereading, true);
    g_int_count = 1;

    const int performed = newsgroup_sel_perform(":c");
    initialize_page();
    const DisplayState result = run_prompt(':', "\bQ");

    EXPECT_EQ(1, performed);
    EXPECT_EQ(DS_ASK, result);
    for (int index = 0; index < GROUP_COUNT; ++index)
    {
        if (index == BEFORE_SELECTED)
        {
            expect_read_target(index, rereading);
        }
        else
        {
            expect_preserved(index);
        }
    }
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ(0, g_int_count);
    EXPECT_EQ(rereading ? NewsgroupNum{} : NewsgroupNum{5}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags);
    expect_context();
    EXPECT_EQ(m_groups[PAGE_SELECTED], g_current_newsgroup);
    EXPECT_EQ(m_groups[BEFORE_SELECTED], g_newsgroup_ptr);
    EXPECT_EQ(m_groups[BEFORE_SELECTED], g_recent_newsgroup);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorInlineRecoveryTest, testing::Bool());

TEST_P(NewsgroupSelectorPromptFollowupTest, aScrolledCommandRetainsTheFollowingSelectorInput)
{
    const auto [command, followup] = GetParam();
    prepare_actions(false, false);
    const std::string input = std::string{command == ':' ? "+\n" : "comp.page*/\n"} + followup + "Q";

    const DisplayState result = run_prompt(command, input);

    EXPECT_EQ(followup == 'q' ? DS_DO_COMMAND : DS_DISPLAY, result);
    EXPECT_EQ(command == ':' ? 3 : 4, g_selected_count);
    EXPECT_EQ(command == ':' ? 3 : 2, g_perform_count);
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags);
    if (followup != 'q')
    {
        EXPECT_EQ(followup, read_tty_char() & 0177);
    }
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(CommandAndFollowup, NewsgroupSelectorPromptFollowupTest,
                         testing::Combine(testing::Values(':', '/'), testing::Values('q', 'a')));

TEST_F(NewsgroupSelectorRecoveryStateTest, cancellingAfterAScrolledCommandPreservesItsCompletedChanges)
{
    prepare_actions(false, false);
    ASSERT_EQ(DS_DISPLAY, run_prompt(':', "-\n\n"));
    remember_groups();

    const DisplayState result = run_prompt('/', "\b\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_catalog_preserved();
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{6}, g_newsgroup_to_read);
    EXPECT_EQ('Q', read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

TEST_P(NewsgroupPageDisplayTest, displaysEligibleGroupsWithTheirCurrentCountsAndMarks)
{
    const auto [rereading, mode] = GetParam();
    prepare_display(rereading, mode);
    ASSERT_FALSE(HasFatalFailure());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_rows(mode, {0, 2, 4, 6})));
    EXPECT_THAT(output, testing::HasSubstr("5 groups"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("alt.hidden")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.opposite")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.invalid")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Next page discussion")));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    const std::array<int, 4> indices{0, 2, 4, 6};
    const std::array<int, 4> marks{1, rereading ? 2 : 0, 2, 3};
    for (std::size_t index = 0; index < indices.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(m_entries[indices[index]], g_sel_page.items[index].u.np);
        EXPECT_EQ(static_cast<int>(index) + 2, g_sel_page.items[index].line);
        EXPECT_EQ(marks[index], g_sel_page.items[index].sel);
    }
    EXPECT_EQ(m_entries[0], g_sel_page.page_ngd);
    EXPECT_EQ(m_entries[7], g_sel_page.next_ngd);
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(7, g_sel_page.last_line);
    EXPECT_FALSE(g_sel_page.at_end);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(m_entries[4], g_newsgroup_ptr);
    expect_group_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndStyle, NewsgroupPageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values('s', 'm', 'l')));

TEST_P(ExclusiveNewsgroupPageDisplayTest, skipsHiddenGroupsWithoutConsumingTheirRowLabels)
{
    prepare_display(GetParam(), 'l', true);
    ASSERT_FALSE(HasFatalFailure());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_rows('l', {0, 6})));
    EXPECT_THAT(output, testing::HasSubstr("2 groups out of 5"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Beta discussion")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Gamma discussion")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("alt.hidden")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.opposite")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.invalid")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(m_entries[0], g_sel_page.items[0].u.np);
    EXPECT_EQ(m_entries[6], g_sel_page.items[1].u.np);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(3, g_sel_page.items[1].line);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(3, g_sel_page.items[1].sel);
    EXPECT_EQ(m_entries[0], g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(5, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_EQ(GetParam(), g_sel.rereading);
    EXPECT_EQ(m_entries[6], g_newsgroup_ptr);
    expect_group_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScope, ExclusiveNewsgroupPageDisplayTest, testing::Bool());

TEST_P(NewsgroupDescriptionFallbackTest, usesTheGroupNameWhenItsDescriptionIsUnavailable)
{
    const auto &[mode, fallback] = GetParam();
    prepare_display(false, mode);
    cache_descriptions(fallback.description);
    ASSERT_FALSE(HasFatalFailure());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a+    3 comp.alpha\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha discussion")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("?Unknown")));
    EXPECT_THAT(output, testing::HasSubstr("Beta discussion"));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(m_entries[0], g_sel_page.items[0].u.np);
    EXPECT_EQ(m_entries[2], g_sel_page.items[1].u.np);
    EXPECT_EQ(m_entries[4], g_sel_page.items[2].u.np);
    EXPECT_EQ(m_entries[6], g_sel_page.items[3].u.np);
    EXPECT_EQ(m_entries[7], g_sel_page.next_ngd);
    EXPECT_FALSE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(m_entries[4], g_newsgroup_ptr);
    expect_group_state();
}

INSTANTIATE_TEST_SUITE_P(StyleAndDescription, NewsgroupDescriptionFallbackTest,
                         testing::Combine(testing::Values('m', 'l'),
                                          testing::Values(GroupDescriptionFallback{"Missing", std::nullopt},
                                                          GroupDescriptionFallback{"Empty", "\n"},
                                                          GroupDescriptionFallback{"Unknown", "?Unknown\n"})));

TEST_P(EmptyNewsgroupPageDisplayTest, displaysNoRowsWhenThePopulationIsEmpty)
{
    const auto [rereading, populated] = GetParam();
    prepare_empty(rereading, populated);

    const std::string output = render_page();

    expect_empty(output, rereading);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPriorPopulation, EmptyNewsgroupPageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(FinalNewsgroupPageDisplayTest, endsAtTheLastGroupWithoutInventingAnotherPage)
{
    const auto [rereading, capacity] = GetParam();
    g_tc_LINES = capacity + 5;
    prepare_display(rereading, 's');
    ASSERT_FALSE(HasFatalFailure());

    const std::string output = page_to_end();

    const int                count = 5 % capacity ? 5 % capacity : capacity;
    const std::array<int, 5> indices{0, 2, 4, 6, 7};
    EXPECT_THAT(output, testing::HasSubstr("comp.next\n"));
    ASSERT_EQ(count, g_sel_page.page_item_cnt);
    EXPECT_EQ(count, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(5 - count, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    for (int index = 0; index < count; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(m_entries[indices[5 - count + index]], g_sel_page.items[index].u.np);
        EXPECT_EQ(index + 2, g_sel_page.items[index].line);
    }
    EXPECT_EQ(m_entries[indices[5 - count]], g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(count + 3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
    expect_group_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPageCapacity, FinalNewsgroupPageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values(2, 3, 5)));

TEST_P(RemovedNewsgroupPageDisplayTest, recoversAnEarlierPageAfterTheLastVisibleGroupLeavesTheReadScope)
{
    const bool rereading = GetParam();
    prepare_display(rereading, 's');
    ASSERT_FALSE(HasFatalFailure());
    (void) page_to_end();
    m_entries[7]->m_to_read = rereading ? 7 : 0;

    init_pages(PRESERVE_PAGE);
    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.next")));
    EXPECT_THAT(output, testing::HasSubstr("comp.alpha"));
    EXPECT_THAT(output, testing::HasSubstr("comp.delta"));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    const std::array<int, 4> indices{0, 2, 4, 6};
    for (int index = 0; index < 4; ++index)
    {
        EXPECT_EQ(m_entries[indices[index]], g_sel_page.items[index].u.np);
    }
    EXPECT_EQ(m_entries[0], g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(RemovedNewsgroupPageDisplayTest, displaysAnEmptyPageAfterAllVisibleGroupsLeaveTheReadScope)
{
    const bool rereading = GetParam();
    prepare_display(rereading, 's');
    ASSERT_FALSE(HasFatalFailure());
    (void) page_to_end();
    leave_read_scope();

    init_pages(PRESERVE_PAGE);
    const std::string output = render_page();

    expect_empty(output, rereading);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, RemovedNewsgroupPageDisplayTest, testing::Bool());

TEST_P(NewsgroupPageLayoutTest, truncatesLongDescriptionsWithinAlignedNameAndLabelColumns)
{
    const auto [rereading, numeric, mode] = GetParam();
    prepare_display(rereading, mode);
    cache_descriptions(std::string(100, 'A') + "\n");
    g_tc_COLS = 40;
    g_use_sel_num = numeric;
    const int         name_width = mode == 'l' ? 12 : 0;
    const std::string name = mode == 'l' ? "comp.alpha  " : "";
    const std::string row =
        row_prefix(0, numeric ? "01" : "a", '+') + name + std::string(31 - name_width - numeric, 'A');

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row + "\n"));
    EXPECT_EQ(39U, row.size());
    ASSERT_EQ(3, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_entries[0], g_sel_page.items[0].u.np);
    EXPECT_EQ(m_entries[2], g_sel_page.items[1].u.np);
    EXPECT_EQ(m_entries[4], g_sel_page.items[2].u.np);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(3, g_sel_page.items[1].line);
    EXPECT_EQ(4, g_sel_page.items[2].line);
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_entries[6], g_sel_page.next_ngd);
    EXPECT_FALSE(g_sel_page.at_end);
    expect_group_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeLabelsAndPresentation, NewsgroupPageLayoutTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Values('m', 'l')));

TEST_P(NewsgroupPageResizeTest, retainsTheNameWhenNoColumnsRemainForItsDescription)
{
    const auto [rereading, numeric] = GetParam();
    prepare_display(rereading, 'l');
    cache_descriptions(std::string(100, 'A') + "\n");
    g_use_sel_num = numeric;
    g_tc_COLS = 20 + numeric;
    const std::string row = std::string(numeric ? "01+" : "a+") + (rereading ? "    0 " : "    3 ") + "comp.alpha  ";

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row + "\n"));
    EXPECT_EQ(static_cast<std::size_t>(g_tc_COLS), row.size());
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("AAAA")));
    ASSERT_EQ(3, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_entries[0], g_sel_page.items[0].u.np);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    expect_group_state();
}

TEST_P(NewsgroupPageResizeTest, shrinkingTheTerminalRetainsVisibleGroupMappingsAndDefersLaterRows)
{
    const auto [rereading, numeric] = GetParam();
    prepare_display(rereading, 's');
    g_use_sel_num = numeric;
    g_newsgroup_ptr = m_entries[2];
    (void) render_page();
    g_tc_COLS = 39;
    g_tc_LINES = 8;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("comp.alpha\n"));
    EXPECT_THAT(output, testing::HasSubstr("comp.beta\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.gamma")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_entries[0], g_sel_page.items[0].u.np);
    EXPECT_EQ(m_entries[2], g_sel_page.items[1].u.np);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(3, g_sel_page.items[1].line);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(m_entries[4], g_sel_page.next_ngd);
    EXPECT_FALSE(g_sel_page.at_end);
    expect_group_state();
}

TEST_P(NewsgroupPageResizeTest, enlargingTheTerminalRestoresRowsWithoutChangingTheCurrentGroup)
{
    const auto [rereading, numeric] = GetParam();
    prepare_display(rereading, 's');
    g_use_sel_num = numeric;
    g_tc_COLS = 39;
    g_tc_LINES = 8;
    (void) render_page();
    g_tc_COLS = 80;
    g_tc_LINES = 11;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("comp.next\n"));
    ASSERT_EQ(5, g_sel_page.page_item_cnt);
    EXPECT_EQ(m_entries[7], g_sel_page.items[4].u.np);
    EXPECT_EQ(6, g_sel_page.items[4].line);
    EXPECT_EQ(m_entries[4], g_newsgroup_ptr);
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(5, g_sel_page.page_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_TRUE(g_sel_page.at_end);
    expect_group_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndLabels, NewsgroupPageResizeTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupPageEligibilityTest, cachedCountsAndCurrentSelectionDetermineEligibility)
{
    const NewsgroupEligibility &state = GetParam();
    set_read_scope(state.rereading);
    g_newsgroup_min_to_read = state.minimum;
    const NewsgroupFlags selection = static_cast<NewsgroupFlags>(g_sel.mask);
    const NewsgroupFlags inactive_selection = state.rereading ? NF_SEL : NF_DEL_SEL;
    NewsgroupData       &group =
        add_group("comp.test", state.unread, NF_INCLUDED | inactive_selection | (state.selected ? selection : NF_NONE));

    initialize_page();

    const std::vector<std::string> expected =
        state.included ? std::vector<std::string>{"comp.test"} : std::vector<std::string>{};
    EXPECT_EQ(expected, groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(state.included ? &group : nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(state.included ? 1 : 0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{state.included ? 1 : 0}, g_obj_count);
    EXPECT_EQ(state.selected ? 1 : 0, g_selected_count);
    EXPECT_EQ(state.included ? 1 : 0, g_sel_page.page_item_cnt);
    EXPECT_EQ(state.included ? 1 : 0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(state.unread, group.m_to_read);
    EXPECT_EQ(state.rereading && state.included && !state.selected, !!(group.m_flags & NF_DEL));
    EXPECT_EQ(inactive_selection, group.m_flags & inactive_selection);
}

TEST_P(NewsgroupPageInvalidTest, selectionsCannotIncludeUnsubscribedOrInvalidEntries)
{
    const auto &[unread, rereading] = GetParam();
    set_read_scope(rereading);
    NewsgroupData &group = add_group("comp.invalid", unread, NF_INCLUDED | static_cast<NewsgroupFlags>(g_sel.mask));
    group.m_abs_first = ArticleNum{};

    initialize_page();

    EXPECT_TRUE(groups_with_flag(NF_INCLUDED).empty());
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(unread, group.m_to_read);
    EXPECT_EQ(ArticleNum{}, group.m_abs_first);
}

TEST_P(NewsgroupPageReadScopeTest, selectionsCannotBypassNameRestrictions)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    const NewsgroupFlags selected = static_cast<NewsgroupFlags>(g_sel.mask);
    NewsgroupData       &included = add_group("comp.keep", rereading ? 0 : 3, selected);
    NewsgroupData       &excluded = add_group("alt.hidden", rereading ? 0 : 3, selected | NF_INCLUDED);
    excluded.m_abs_first = ArticleNum{};
    set_newsgroup_to_do("comp.*");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.keep"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&included, g_sel_page.page_ngd);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_only.max_to_do);
    EXPECT_EQ(ArticleNum{}, excluded.m_abs_first);
}

TEST_P(NewsgroupPageReadScopeTest, exclusiveDisplayCountsEligibleGroupsButIncludesOnlySelectedGroups)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    g_sel.exclusive = true;
    add_group("comp.hidden", rereading ? 0 : 3, NF_INCLUDED);
    NewsgroupData &selected = add_group("comp.selected", rereading ? 3 : 0, static_cast<NewsgroupFlags>(g_sel.mask));
    add_group("comp.invalid", TR_UNSUB, static_cast<NewsgroupFlags>(g_sel.mask));

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.selected"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&selected, g_sel_page.page_ngd);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    EXPECT_TRUE(groups_with_flag(NF_DEL).empty());
    EXPECT_TRUE(g_sel.exclusive);
}

TEST_P(NewsgroupPageReadScopeTest, emptyExclusiveSelectionRebuildsFromTheFirstEligibleGroup)
{
    const bool rereading = GetParam();
    set_read_scope(rereading);
    NewsgroupData &eligible = add_group("comp.eligible", rereading ? 0 : 3);
    NewsgroupData &ineligible = add_group("comp.ineligible", rereading ? 3 : 0, NF_INCLUDED);
    g_sel_page.page_ngd = &ineligible;
    g_sel.exclusive = true;

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&eligible, g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    EXPECT_FALSE(g_sel.exclusive);
}

TEST_P(NewsgroupPageReadScopeTest, aPopulationWithNoEligibleGroupsProducesAnEmptyPage)
{
    set_read_scope(GetParam());
    add_group("comp.other-scope", GetParam() ? 3 : 0, NF_INCLUDED);
    add_group("comp.unsubscribed", TR_UNSUB, NF_INCLUDED);

    initialize_page();

    EXPECT_TRUE(groups_with_flag(NF_INCLUDED).empty());
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
}

TEST_P(NewsgroupPageFallbackTest, anEmptyCatalogProducesAnEmptyPageAndClearsExclusiveDisplay)
{
    const auto &[rereading, exclusive] = GetParam();
    set_read_scope(rereading);
    g_sel.exclusive = exclusive;

    initialize_page();

    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    EXPECT_EQ(nullptr, g_sel_page.next_ngd);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_FALSE(g_sel.exclusive);
}

TEST_P(NewsgroupPageFallbackTest, restrictionsWithNoEligibleGroupsAreRemovedBeforeRebuilding)
{
    const auto &[rereading, exclusive] = GetParam();
    set_read_scope(rereading);
    g_sel.exclusive = exclusive;
    g_newsgroup_min_to_read = 2;
    add_group("comp.other-scope", rereading ? 3 : 0);
    NewsgroupData &eligible = add_group("alt.eligible", rereading ? 0 : 3);
    set_newsgroup_to_do("comp.*");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"alt.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&eligible, g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_only.max_to_do);
    EXPECT_EQ(TR_ONE, g_newsgroup_min_to_read);
    EXPECT_EQ("Exiting \"only\".", g_msg);
    EXPECT_FALSE(g_sel.exclusive);
}

TEST_P(NewsgroupPageLoadingTest, theFirstEligibleUninitializedGroupDefersRemainingCounts)
{
    const bool     rereading = GetParam();
    NewsgroupData &first = add_pending_group("comp.first", rereading ? "1-10" : "1-7");
    NewsgroupData &deferred = add_pending_group("comp.deferred", "1-10");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.first", "comp.deferred"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&first, g_sel_page.page_ngd);
    EXPECT_EQ(ArticleNum{1}, first.m_abs_first);
    EXPECT_EQ(rereading ? 0 : 3, first.m_to_read);
    EXPECT_EQ(ArticleNum{}, deferred.m_abs_first);
    EXPECT_EQ(rereading ? 0 : 1, deferred.m_to_read);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_FALSE(g_paranoid);
}

TEST_P(NewsgroupPageLoadingTest, anIneligibleUninitializedGroupDoesNotDeferLaterCounts)
{
    const bool     rereading = GetParam();
    NewsgroupData &ineligible = add_pending_group("comp.other-scope", rereading ? "1-7" : "1-10");
    NewsgroupData &eligible = add_pending_group("comp.eligible", rereading ? "1-10" : "1-7");

    initialize_page();

    EXPECT_EQ((std::vector<std::string>{"comp.eligible"}), groups_with_flag(NF_INCLUDED));
    EXPECT_EQ(&eligible, g_sel_page.page_ngd);
    EXPECT_EQ(ArticleNum{1}, ineligible.m_abs_first);
    EXPECT_EQ(ArticleNum{1}, eligible.m_abs_first);
    EXPECT_EQ(rereading ? 3 : 0, ineligible.m_to_read);
    EXPECT_EQ(rereading ? 0 : 3, eligible.m_to_read);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
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
