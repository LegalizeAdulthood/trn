// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/NewsgroupPageState.h"

#include <trn/addng.h>
#include <trn/rt-page.h>

#include <iomanip>
#include <sstream>
#include <tuple>

namespace
{

std::string row_label(int row, bool numeric)
{
    if (!numeric)
    {
        return std::string(1, "abcdef"[row]);
    }
    std::ostringstream label;
    label << std::setfill('0') << std::setw(2) << row + 1;
    return label.str();
}

class SourcePageDisplayState : public test_support::NewsgroupPageDisplayState
{
protected:
    void     SetUp() override;
    void     TearDown() override;
    Multirc &add_sources(int number, const std::vector<std::string> &names, MultircFlags flags = MF_NONE);
    void     expect_row(int row, int number, int selected = 0) const;

private:
    std::vector<Multirc>    m_old_sources{std::move(g_multircs)};
    Multirc                *m_old_page{g_sel_page.page_mrc};
    Multirc                *m_old_next{g_sel_page.next_mrc};
    std::vector<DataSource> m_sources;
    std::vector<Newsrc>     m_newsrcs;
};

void SourcePageDisplayState::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    g_multircs.clear();
    g_multircs.reserve(8);
    m_sources.reserve(32);
    m_newsrcs.reserve(32);
    g_sel.mode = SM_MULTIRC;
    g_sel_page.page_mrc = nullptr;
    g_sel_page.next_mrc = nullptr;
}

void SourcePageDisplayState::TearDown()
{
    g_multircs = std::move(m_old_sources);
    g_sel_page.page_mrc = m_old_page;
    g_sel_page.next_mrc = m_old_next;
    NewsgroupPageDisplayState::TearDown();
}

Multirc &SourcePageDisplayState::add_sources(int number, const std::vector<std::string> &names, MultircFlags flags)
{
    Multirc &entry = g_multircs.emplace_back();
    entry.m_num = number;
    entry.m_flags = flags;
    Newsrc *previous = nullptr;
    for (const std::string &name : names)
    {
        DataSource &source = m_sources.emplace_back();
        source.m_name = name;
        Newsrc &newsrc = m_newsrcs.emplace_back();
        newsrc.data_source = &source;
        newsrc.flags = RF_ACTIVE;
        if (previous)
        {
            previous->next = &newsrc;
        }
        else
        {
            entry.m_first = &newsrc;
        }
        previous = &newsrc;
    }
    return entry;
}

void SourcePageDisplayState::expect_row(int row, int number, int selected) const
{
    ASSERT_NE(nullptr, g_sel_page.items[row].u.mp);
    EXPECT_EQ(multirc_ptr(number), g_sel_page.items[row].u.mp);
    EXPECT_EQ(number, g_sel_page.items[row].u.mp->m_num);
    EXPECT_EQ(row + 2, g_sel_page.items[row].line);
    EXPECT_EQ(selected, g_sel_page.items[row].sel);
}

class SourceRowTest : public SourcePageDisplayState, public testing::WithParamInterface<bool>
{
};

struct SourceListCase
{
    const char              *name;
    std::vector<std::string> sources;
    std::string              expected;
};

void PrintTo(const SourceListCase &value, std::ostream *output)
{
    *output << value.name;
}

const SourceListCase SOURCE_LIST_CASES[]{
    {"ShortList", {"alpha", "beta", "gamma"}, "alpha, beta, gamma"},
    {"SingleLongName", {std::string(45, 'a')}, std::string(45, 'a')},
    {"LongNameWithTail", {std::string(45, 'a'), "hidden"}, std::string(45, 'a') + ", ..."},
    {"BoundaryBeforeTail", {std::string(32, 'a'), "hidden"}, std::string(32, 'a') + ", ..."},
    {"CrossBoundaryWithSecondName", {std::string(31, 'a'), "beta", "hidden"}, std::string(31, 'a') + ", beta, ..."},
};

class SourceListTest : public SourcePageDisplayState, public testing::WithParamInterface<SourceListCase>
{
protected:
    void SetUp() override;
};

void SourceListTest::SetUp()
{
    SourcePageDisplayState::SetUp();
    (void) add_sources(17, GetParam().sources);
    initialize_page();
}

class SourcePageEndingTest : public SourcePageDisplayState, public testing::WithParamInterface<std::tuple<bool, int>>
{
protected:
    void SetUp() override;
};

void SourcePageEndingTest::SetUp()
{
    SourcePageDisplayState::SetUp();
    const auto [numeric, final_rows] = GetParam();
    g_use_sel_num = numeric;
    g_tc_LINES = 7;
    (void) add_sources(10, {"first"});
    (void) add_sources(20, {});
    (void) add_sources(30, {"second"});
    (void) add_sources(40, {"third"}, MF_SEL);
    if (final_rows == 2)
    {
        (void) add_sources(50, {"fourth"});
    }
    initialize_page();
}

class NewGroupPageDisplayState : public test_support::NewsgroupPageDisplayState
{
protected:
    void          SetUp() override;
    void          TearDown() override;
    AddGroupIndex add_new(std::string_view name, int count, AddGroupFlags flags = AGF_NONE,
                          std::optional<std::string_view> description = std::nullopt);
    void          prepare_scope(bool rereading, char mode = 's');
    void          expect_row(int row, AddGroupIndex identity, int selected = 0) const;

private:
    AddGroupTable m_old_groups{std::move(g_add_groups)};
    AddGroupIndex m_old_page{g_sel_page.page_grp};
    AddGroupIndex m_old_next{g_sel_page.next_grp};
};

void NewGroupPageDisplayState::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    g_add_groups = {};
    g_sel.mode = SM_ADD_GROUP;
    g_sel_page.page_grp = AddGroupIndex::None;
    g_sel_page.next_grp = AddGroupIndex::None;
    g_sel_grp_display_mode = "s";
    g_sel_grp_display_mode_index = 0;
    ASSERT_EQ(1, m_source.m_desc_sf.open({}, "", ""));
}

void NewGroupPageDisplayState::TearDown()
{
    g_add_groups = std::move(m_old_groups);
    g_sel_page.page_grp = m_old_page;
    g_sel_page.next_grp = m_old_next;
    NewsgroupPageDisplayState::TearDown();
}

AddGroupIndex NewGroupPageDisplayState::add_new(std::string_view name, int count, AddGroupFlags flags,
                                                std::optional<std::string_view> description)
{
    const AddGroupIndex identity = g_add_groups.add(name, count, 0, &m_source);
    g_add_groups[identity].m_flags = flags;
    if (description)
    {
        (void) m_source.m_desc_sf.append(std::string{name} + " " + std::string{*description} + "\n",
                                         static_cast<int>(name.size()));
    }
    return identity;
}

void NewGroupPageDisplayState::prepare_scope(bool rereading, char mode)
{
    set_read_scope(rereading);
    g_sel_grp_display_mode.assign(1, mode);
}

void NewGroupPageDisplayState::expect_row(int row, AddGroupIndex identity, int selected) const
{
    EXPECT_EQ(identity, g_sel_page.items[row].u.gp);
    EXPECT_EQ(row + 2, g_sel_page.items[row].line);
    EXPECT_EQ(selected, g_sel_page.items[row].sel);
}

class NewGroupRowTest : public NewGroupPageDisplayState,
                        public testing::WithParamInterface<std::tuple<bool, char, bool>>
{
};

class NewGroupFallbackTest : public NewGroupPageDisplayState, public testing::WithParamInterface<std::tuple<char, int>>
{
};

class NewGroupPageEndingTest : public NewGroupPageDisplayState,
                               public testing::WithParamInterface<std::tuple<bool, bool, int>>
{
protected:
    void                         SetUp() override;
    std::array<AddGroupIndex, 4> m_ids{};
};

void NewGroupPageEndingTest::SetUp()
{
    NewGroupPageDisplayState::SetUp();
    const auto [rereading, numeric, final_rows] = GetParam();
    prepare_scope(rereading);
    g_use_sel_num = numeric;
    g_tc_LINES = 7;
    const AddGroupFlags scope = rereading ? AGF_EXCLUDED : AGF_NONE;
    m_ids[0] = add_new("comp.first", 1, scope);
    (void) add_new("comp.hidden", 9, rereading ? AGF_NONE : AGF_EXCLUDED);
    m_ids[1] = add_new("comp.second", 2, scope);
    m_ids[2] = add_new("comp.third", 3, scope | static_cast<AddGroupFlags>(g_sel.mask));
    if (final_rows == 2)
    {
        m_ids[3] = add_new("comp.fourth", 4, scope);
    }
    initialize_page();
}

class NewGroupAlignedPageTest : public NewGroupPageDisplayState, public testing::WithParamInterface<bool>
{
};

class NewGroupLongNameTest : public NewGroupPageDisplayState, public testing::WithParamInterface<char>
{
};

class NewGroupScopeTest : public NewGroupPageDisplayState, public testing::WithParamInterface<bool>
{
};

} // namespace

TEST_P(SourceRowTest, labelsOnlyConfiguredSourcesAndPreservesTheirSelections)
{
    g_use_sel_num = GetParam();
    (void) add_sources(10, {"alpha"}, MF_SEL);
    (void) add_sources(20, {});
    (void) add_sources(30, {"beta", "gamma"});
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("Newsrcs"));
    EXPECT_THAT(output, testing::HasSubstr(row_label(0, GetParam()) + "+   10 alpha\n"));
    EXPECT_THAT(output, testing::HasSubstr(row_label(1, GetParam()) + "    30 beta, gamma\n"));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, 10, 1);
    expect_row(1, 30);
    EXPECT_EQ(MF_SEL | MF_INCLUDED, g_multircs[0].m_flags);
    EXPECT_EQ(MF_NONE, g_multircs[1].m_flags);
    EXPECT_EQ(MF_INCLUDED, g_multircs[2].m_flags);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_page.next_mrc);
    EXPECT_TRUE(g_sel_page.at_end);
}

TEST_P(SourceRowTest, displaysAnEmptyCatalogWithoutSelectableRows)
{
    g_use_sel_num = GetParam();
    (void) add_sources(10, {});
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("Newsrcs"));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(SourceRowTest, recoversEarlierRowsWhenTheFinalSourceDisappears)
{
    g_use_sel_num = GetParam();
    g_tc_LINES = 7;
    (void) add_sources(10, {"first"});
    (void) add_sources(20, {"second"});
    (void) add_sources(30, {"removed"});
    initialize_page();
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());
    g_multircs[2].m_first = nullptr;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("removed")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, 10);
    expect_row(1, 20);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    ASSERT_EQ(multirc_ptr(30), g_sel_page.next_mrc);
    EXPECT_FALSE(g_sel_page.next_mrc->m_flags & MF_INCLUDED);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, SourceRowTest, testing::Bool());

TEST_P(SourceListTest, abbreviatesOnlyTheUndisplayedTailOfTheSourceList)
{
    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a    17 " + GetParam().expected + "\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("hidden")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, 17);
    const Newsrc *newsrc = g_multircs[0].m_first;
    for (const std::string &name : GetParam().sources)
    {
        ASSERT_NE(nullptr, newsrc);
        EXPECT_EQ(name, newsrc->data_source->m_name);
        EXPECT_EQ(RF_ACTIVE, newsrc->flags);
        newsrc = newsrc->next;
    }
    EXPECT_EQ(nullptr, newsrc);
}

INSTANTIATE_TEST_SUITE_P(NameList, SourceListTest, testing::ValuesIn(SOURCE_LIST_CASES),
                         [](const testing::TestParamInfo<SourceListCase> &info) { return info.param.name; });

TEST_P(SourcePageEndingTest, retainsTheContinuationAfterAFullPage)
{
    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("first\n"));
    EXPECT_THAT(output, testing::HasSubstr("second\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("third")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, 10);
    expect_row(1, 30);
    EXPECT_EQ(multirc_ptr(40), g_sel_page.next_mrc);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_FALSE(g_sel_page.at_end);
}

TEST_P(SourcePageEndingTest, restartsLabelsOnTheFinalPageAndStopsAtItsEnd)
{
    const auto [numeric, final_rows] = GetParam();
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row_label(0, numeric) + "+   40 third\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("first\n")));
    ASSERT_EQ(final_rows, g_sel_page.page_item_cnt);
    expect_row(0, 40, 1);
    if (final_rows == 2)
    {
        expect_row(1, 50);
        EXPECT_THAT(output, testing::HasSubstr(row_label(1, numeric) + "    50 fourth\n"));
    }
    EXPECT_EQ(2, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(final_rows, g_sel_page.page_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_page.next_mrc);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

INSTANTIATE_TEST_SUITE_P(LabelAndFinalPageSize, SourcePageEndingTest,
                         testing::Combine(testing::Bool(), testing::Values(1, 2)));

TEST_P(NewGroupRowTest, displaysEligibleGroupCountsDescriptionsAndPendingMarkers)
{
    const auto [rereading, mode, numeric] = GetParam();
    prepare_scope(rereading, mode);
    g_use_sel_num = numeric;
    const AddGroupFlags                   scope = rereading ? AGF_EXCLUDED : AGF_NONE;
    const AddGroupFlags                   selected = static_cast<AddGroupFlags>(g_sel.mask);
    const std::array<std::string_view, 4> names{"comp.a", "comp.longer", "comp.c", "comp.d"};
    const std::array<std::string_view, 4> descriptions{"Alpha discussion", "Beta discussion", "Gamma discussion",
                                                       "Delta discussion"};
    const std::array<int, 4>              counts{0, 12, 123456, 7};
    const std::array<AddGroupFlags, 4>    marks{selected, AGF_NONE, AGF_DEL, selected | AGF_DEL};
    std::array<AddGroupIndex, 4>          identities{};
    identities[0] = add_new(names[0], counts[0], scope | marks[0], descriptions[0]);
    (void) add_new("comp.hidden.with.a.very.long.name", 99, rereading ? AGF_NONE : AGF_EXCLUDED);
    for (std::size_t i = 1; i < identities.size(); ++i)
    {
        identities[i] = add_new(names[i], counts[i], scope | marks[i], descriptions[i]);
    }
    initialize_page();
    const AddGroupTable before = g_add_groups;

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("ADD newsgroups"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.hidden")));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    for (int row = 0; row < 4; ++row)
    {
        const int          marker = row == 1 ? (rereading ? 2 : 0) : row == 0 ? 1 : row;
        std::ostringstream expected;
        expected << row_label(row, numeric) << " +-*"[marker] << std::setw(5) << counts[row] << " ";
        if (mode == 's')
        {
            expected << names[row];
        }
        else if (mode == 'm')
        {
            expected << descriptions[row];
        }
        else
        {
            expected << std::left << std::setw(13) << names[row] << descriptions[row];
        }
        expected << "\n";
        EXPECT_THAT(output, testing::HasSubstr(expected.str()));
        expect_row(row, identities[row], marker);
        const AddGroup &group = g_add_groups[identities[row]];
        EXPECT_EQ(before[identities[row]].m_flags, group.m_flags);
        EXPECT_EQ(names[row], group.m_name);
        EXPECT_EQ(ArticleNum{counts[row]}, group.m_to_read);
        EXPECT_EQ(&m_source, group.m_data_src);
        EXPECT_EQ(before[identities[row]].m_num, group.m_num);
    }
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ(AddGroupIndex::None, g_sel_page.next_grp);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ScopePresentationAndLabel, NewGroupRowTest,
                         testing::Combine(testing::Bool(), testing::Values('s', 'm', 'l'), testing::Bool()));

TEST_P(NewGroupFallbackTest, displaysTheNameWhenNoUsableDescriptionIsCached)
{
    const auto [mode, kind] = GetParam();
    prepare_scope(false, mode);
    const std::array<std::optional<std::string_view>, 3> descriptions{std::nullopt, "", "? unknown"};
    const AddGroupIndex identity = add_new("comp.reading", 3, AGF_NONE, descriptions[kind]);
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a     3 comp.reading\n"));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, identity);
    EXPECT_EQ(ArticleNum{3}, g_add_groups[identity].m_to_read);
}

INSTANTIATE_TEST_SUITE_P(PresentationAndMissingDescription, NewGroupFallbackTest,
                         testing::Combine(testing::Values('m', 'l'), testing::Values(0, 1, 2)));

TEST_P(NewGroupPageEndingTest, retainsTheNextGroupIdentityAfterAFullPage)
{
    const auto [rereading, numeric, final_rows] = GetParam();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("comp.first\n"));
    EXPECT_THAT(output, testing::HasSubstr("comp.second\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.hidden")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.third")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, m_ids[0], rereading ? 2 : 0);
    expect_row(1, m_ids[1], rereading ? 2 : 0);
    EXPECT_EQ(m_ids[2], g_sel_page.next_grp);
    EXPECT_EQ(2 + final_rows, g_sel_page.total_obj_cnt);
    EXPECT_FALSE(g_sel_page.at_end);
}

TEST_P(NewGroupPageEndingTest, displaysTheFinalPageWithItsSelectionAndStops)
{
    const auto [rereading, numeric, final_rows] = GetParam();
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row_label(0, numeric) + "+    3 comp.third\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.first")));
    ASSERT_EQ(final_rows, g_sel_page.page_item_cnt);
    expect_row(0, m_ids[2], 1);
    if (final_rows == 2)
    {
        expect_row(1, m_ids[3], rereading ? 2 : 0);
    }
    EXPECT_EQ(2, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(final_rows, g_sel_page.page_obj_cnt);
    EXPECT_EQ(AddGroupIndex::None, g_sel_page.next_grp);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

INSTANTIATE_TEST_SUITE_P(ScopeLabelAndFinalPageSize, NewGroupPageEndingTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Values(1, 2)));

TEST_P(NewGroupAlignedPageTest, alignsDescriptionsUsingOnlyTheVisiblePageNames)
{
    prepare_scope(false, 'l');
    g_use_sel_num = GetParam();
    g_tc_LINES = 7;
    const AddGroupIndex first = add_new("comp.a", 1, AGF_NONE, "Alpha");
    (void) add_new("comp.hidden.with.a.long.name", 9, AGF_EXCLUDED, "Hidden");
    const AddGroupIndex second = add_new("comp.bb", 2, AGF_NONE, "Beta");
    const AddGroupIndex later = add_new("comp." + std::string(60, 'x'), 3, AGF_NONE, "Later");
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row_label(0, GetParam()) + "     1 comp.a   Alpha\n"));
    EXPECT_THAT(output, testing::HasSubstr(row_label(1, GetParam()) + "     2 comp.bb  Beta\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Hidden")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Later")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, first);
    expect_row(1, second);
    EXPECT_EQ(later, g_sel_page.next_grp);
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, NewGroupAlignedPageTest, testing::Bool());

TEST_P(NewGroupLongNameTest, retainsTheFullGroupNameWhenItExceedsTheTerminalWidth)
{
    const char mode = GetParam();
    prepare_scope(false, mode);
    const std::string   name = "comp." + std::string(90, 'x');
    const AddGroupIndex identity = add_new(name, 42, AGF_NONE, "Description");
    initialize_page();

    const std::string output = render_page();

    const std::string visible = mode == 'm' ? "Description" : name + (mode == 'l' ? "  " : "");
    EXPECT_THAT(output, testing::HasSubstr("a    42 " + visible + "\n"));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, identity);
    EXPECT_EQ(name, g_add_groups[identity].m_name);
    EXPECT_EQ(4, g_sel_page.last_line);
}

INSTANTIATE_TEST_SUITE_P(Presentation, NewGroupLongNameTest, testing::Values('s', 'm', 'l'));

TEST_P(NewGroupScopeTest, displaysOnlySelectedGroupsOnAnExclusivePage)
{
    const bool rereading = GetParam();
    prepare_scope(rereading);
    const AddGroupFlags scope = rereading ? AGF_EXCLUDED : AGF_NONE;
    (void) add_new("comp.unselected", 1, scope);
    const AddGroupIndex identity = add_new("comp.selected", 9, scope | static_cast<AddGroupFlags>(g_sel.mask));
    g_sel.exclusive = true;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a+    9 comp.selected\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.unselected")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, identity, 1);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_TRUE(g_sel_page.at_end);
}

TEST_P(NewGroupScopeTest, rendersAnEmptyScopeWithoutStaleCounts)
{
    prepare_scope(GetParam());
    (void) add_new("comp.opposite", 3, GetParam() ? AGF_NONE : AGF_EXCLUDED);
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("ADD newsgroups"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.opposite")));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(NewGroupScopeTest, recoversEarlierGroupsAfterTheFinalPageLeavesTheScope)
{
    const bool rereading = GetParam();
    prepare_scope(rereading);
    g_tc_LINES = 7;
    const AddGroupFlags scope = rereading ? AGF_EXCLUDED : AGF_NONE;
    const AddGroupIndex first = add_new("comp.first", 1, scope);
    const AddGroupIndex second = add_new("comp.second", 2, scope);
    const AddGroupIndex removed = add_new("comp.removed", 3, scope);
    initialize_page();
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());
    g_add_groups[removed].m_flags ^= AGF_EXCLUDED;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.removed")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, first, rereading ? 2 : 0);
    expect_row(1, second, rereading ? 2 : 0);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    ASSERT_EQ(removed, g_sel_page.next_grp);
    EXPECT_FALSE(g_add_groups[g_sel_page.next_grp].m_flags & AGF_INCLUDED);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewGroupScopeTest, testing::Bool());
