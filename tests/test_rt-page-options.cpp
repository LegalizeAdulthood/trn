// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/NewsgroupPageState.h"

#include <trn/OptionCatalog.h>
#include <trn/OptionDraft.h>
#include <trn/respond.h>
#include <trn/rt-page.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace
{

const std::array<std::string_view, 10> OPTION_HEADINGS{
    "Display Options", "Selector Options", "Newsreading Options", "Posting Options",           "Save Options",
    "Mouse Options",   "MIME Options",     "Misc Options",        "Article Scan Mode Options", "Scoring Options"};

class OptionPageDisplayState : public test_support::NewsgroupPageDisplayState
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    int         group_for(OptionIndex option) const;
    void        start_at_option(OptionIndex option);
    void        expect_row(int row, int catalog_row, int selected) const;
    std::string expected_heading(int row, std::string_view name, bool expanded = false) const;
    std::string expected_value(int row, std::string_view name, std::string_view value, int marker) const;

    const OptionCatalog m_catalog;
    OptionDraft         m_draft{static_cast<std::size_t>(m_catalog.option_limit())};

private:
    std::string              label(int row) const;
    OptionValueList          m_old_defaults{std::move(g_option_def_vals)};
    OptionValueList          m_old_saved{std::move(g_option_saved_vals)};
    OptionDraft             *m_old_draft{g_option_draft};
    OptionFlags             *m_old_flags{g_option_flags};
    int                      m_old_page{g_sel_page.page_op};
    std::string              m_old_indent{g_indent_string};
    std::string              m_old_group_name{g_newsgroup_name};
    std::vector<OptionFlags> m_flags;
};

void OptionPageDisplayState::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    m_flags.assign(static_cast<std::size_t>(m_catalog.row_count() + 1), OF_NONE);
    g_option_flags = m_flags.data();
    g_option_def_vals = OptionValueList(static_cast<std::size_t>(m_catalog.option_limit()));
    g_option_saved_vals = OptionValueList(static_cast<std::size_t>(m_catalog.option_limit()));
    g_option_draft = &m_draft;
    g_sel_page.page_op = -1;
    g_sel.mode = SM_OPTIONS;
    g_indent_string = ">";
}

void OptionPageDisplayState::TearDown()
{
    g_option_def_vals = std::move(m_old_defaults);
    g_option_saved_vals = std::move(m_old_saved);
    g_option_draft = m_old_draft;
    g_option_flags = m_old_flags;
    g_sel_page.page_op = m_old_page;
    g_indent_string = m_old_indent;
    g_newsgroup_name = m_old_group_name;
    NewsgroupPageDisplayState::TearDown();
}

int OptionPageDisplayState::group_for(OptionIndex option) const
{
    return m_catalog.previous_group_row(m_catalog.row_for(option));
}

void OptionPageDisplayState::start_at_option(OptionIndex option)
{
    g_option_flags[group_for(option)] |= OF_SEL;
    g_sel_page.page_op = m_catalog.row_for(option);
    g_tc_LINES = 6;
    initialize_page();
}

void OptionPageDisplayState::expect_row(int row, int catalog_row, int selected) const
{
    EXPECT_EQ(static_cast<OptionIndex>(catalog_row), g_sel_page.items[row].u.op);
    EXPECT_EQ(row + 2, g_sel_page.items[row].line);
    EXPECT_EQ(selected, g_sel_page.items[row].sel);
}

std::string OptionPageDisplayState::label(int row) const
{
    if (!g_use_sel_num)
    {
        return std::string(1, "abcdef"[row]);
    }
    std::ostringstream output;
    output << std::setw(2) << std::setfill('0') << row + 1;
    return output.str();
}

std::string OptionPageDisplayState::expected_heading(int row, std::string_view name, bool expanded) const
{
    std::ostringstream output;
    output << label(row) << (expanded ? '+' : ' ') << " ==" << std::left << std::setw(34) << std::setfill('=') << name
           << " \n";
    return output.str();
}

std::string OptionPageDisplayState::expected_value(int row, std::string_view name, std::string_view value,
                                                   int marker) const
{
    std::ostringstream output;
    output << label(row) << " +-*"[marker] << "   " << std::left << std::setw(34) << std::setfill('.') << name << " "
           << value << "\n";
    return output.str();
}

class OptionContextRowTest : public OptionPageDisplayState, public testing::WithParamInterface<bool>
{
};

class OptionHeadingRowTest : public OptionPageDisplayState, public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class OptionValueMarkerTest : public OptionPageDisplayState,
                              public testing::WithParamInterface<std::tuple<bool, bool, int>>
{
};

struct OptionTextCase
{
    const char *name;
    std::string text;
    std::string quoted;
};

void PrintTo(const OptionTextCase &value, std::ostream *output)
{
    *output << value.name;
}

const OptionTextCase OPTION_TEXT_CASES[]{
    {"Empty", "", ""},
    {"Plain", ">>", ">>"},
    {"LeadingSpace", " > ", "\" > \""},
    {"CommentCharacter", "#>", "\"#>\""},
    {"Quote", "say \"quoted\"", "'say \"quoted\"'"},
    {"Backslash", "path\\prefix", "\"path\\\\prefix\""},
    {"DisplayLimit", std::string(39, '>'), std::string(39, '>')},
    {"BeyondDisplayLimit", std::string(40, '>'), std::string(40, '>')},
};

class OptionTextRowTest : public OptionPageDisplayState,
                          public testing::WithParamInterface<std::tuple<bool, OptionTextCase>>
{
};

class OptionExpansionRowTest : public OptionPageDisplayState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void OptionExpansionRowTest::SetUp()
{
    OptionPageDisplayState::SetUp();
    g_use_sel_num = GetParam();
    g_sel_page.page_op = group_for(OI_CITED_TEXT_STRING);
}

class OptionPageEndingTest : public OptionPageDisplayState, public testing::WithParamInterface<std::tuple<bool, int>>
{
protected:
    void             SetUp() override;
    std::vector<int> m_headings;
};

void OptionPageEndingTest::SetUp()
{
    OptionPageDisplayState::SetUp();
    const auto [numeric, capacity] = GetParam();
    g_use_sel_num = numeric;
    g_tc_LINES = capacity + 5;
    for (int row = m_catalog.first_row(); row <= m_catalog.row_count(); ++row)
    {
        if (m_catalog.is_group(row))
        {
            m_headings.push_back(row);
        }
    }
    initialize_page();
}

class OptionRecoveryRowTest : public OptionPageDisplayState, public testing::WithParamInterface<bool>
{
};

} // namespace

TEST_P(OptionContextRowTest, showsOptionInstructionsWhenEnteredFromReadingOrASelector)
{
    g_in_ng = GetParam();
    g_newsgroup_name = "comp.reading";
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("Options"));
    EXPECT_THAT(output, testing::HasSubstr("(Press 'S' to save changes, 'q' to abandon, or TAB to use.)"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("comp.reading")));
    EXPECT_EQ(GetParam(), g_in_ng);
    EXPECT_EQ(SM_OPTIONS, g_sel.mode);
    EXPECT_EQ(10, g_sel_page.total_obj_cnt);
}

INSTANTIATE_TEST_SUITE_P(CallerContext, OptionContextRowTest, testing::Bool());

TEST_P(OptionHeadingRowTest, marksTheGroupHeadingWithoutChangingItsExpansion)
{
    const auto [numeric, expanded] = GetParam();
    g_use_sel_num = numeric;
    g_tc_LINES = 6;
    if (expanded)
    {
        g_option_flags[m_catalog.first_row()] |= OF_SEL;
    }
    initialize_page();
    const OptionFlags before = g_option_flags[m_catalog.first_row()];

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_heading(0, "Display Options", expanded)));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, m_catalog.first_row(), expanded ? 1 : 0);
    EXPECT_EQ(before, g_option_flags[m_catalog.first_row()]);
    EXPECT_EQ(expanded ? 18 : 10, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    EXPECT_FALSE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(LabelAndExpansion, OptionHeadingRowTest, testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(OptionValueMarkerTest, distinguishesDefaultsConfiguredValuesUnsavedChangesAndPendingEdits)
{
    const auto [numeric, edited, state] = GetParam();
    g_use_sel_num = numeric;
    if (state >= 1)
    {
        g_option_def_vals[OI_TERSE_OUTPUT] = "yes";
    }
    if (state == 2)
    {
        g_option_saved_vals[OI_TERSE_OUTPUT] = "no";
    }
    if (edited)
    {
        m_draft.set(OI_TERSE_OUTPUT, "no");
    }
    start_at_option(OI_TERSE_OUTPUT);
    const OptionValueList defaults = g_option_def_vals;
    const OptionValueList saved = g_option_saved_vals;
    const int             marker = edited ? 1 : state == 2 ? 3 : state == 1 ? 0 : 2;

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_value(0, "Terse Output", edited ? "no" : "yes", marker)));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, m_catalog.row_for(OI_TERSE_OUTPUT), marker);
    EXPECT_EQ(defaults, g_option_def_vals);
    EXPECT_EQ(saved, g_option_saved_vals);
    EXPECT_FALSE(g_verbose);
    EXPECT_EQ(edited, m_draft.contains(OI_TERSE_OUTPUT));
    if (edited)
    {
        EXPECT_EQ("no", m_draft.value(OI_TERSE_OUTPUT));
    }
}

INSTANTIATE_TEST_SUITE_P(LabelDraftAndSavedState, OptionValueMarkerTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Values(0, 1, 2)));

TEST_P(OptionTextRowTest, showsTheCurrentOrEditedTextWithinTheValueColumn)
{
    const auto [edited, text] = GetParam();
    if (edited)
    {
        g_indent_string = "Original";
        m_draft.set(OI_CITED_TEXT_STRING, text.text);
    }
    else
    {
        g_indent_string = text.text;
    }
    start_at_option(OI_CITED_TEXT_STRING);
    const std::string original = g_indent_string;
    const std::string expected = (edited ? text.text : text.quoted).substr(0, 39);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_value(0, "Cited Text String", expected, edited ? 1 : 2)));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    expect_row(0, m_catalog.row_for(OI_CITED_TEXT_STRING), edited ? 1 : 2);
    EXPECT_EQ(original, g_indent_string);
    EXPECT_EQ(edited ? 1U : 0U, m_draft.size());
    if (edited)
    {
        EXPECT_EQ(text.text, m_draft.value(OI_CITED_TEXT_STRING));
    }
}

INSTANTIATE_TEST_SUITE_P(DraftAndText, OptionTextRowTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(OPTION_TEXT_CASES)));

TEST_P(OptionExpansionRowTest, insertsAnExpandedGroupsOptionsBeforeTheNextHeading)
{
    g_option_flags[group_for(OI_CITED_TEXT_STRING)] |= OF_SEL;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_heading(0, "Posting Options", true)));
    EXPECT_THAT(output, testing::HasSubstr(expected_value(1, "Cited Text String", ">", 2)));
    EXPECT_THAT(output, testing::HasSubstr(expected_heading(2, "Save Options")));
    EXPECT_THAT(output, testing::HasSubstr(expected_heading(3, "Mouse Options")));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    expect_row(0, group_for(OI_CITED_TEXT_STRING), 1);
    expect_row(1, m_catalog.row_for(OI_CITED_TEXT_STRING), 2);
    expect_row(2, group_for(OI_SAVE_DIR), 0);
    expect_row(3, group_for(OI_USE_MOUSE), 0);
    EXPECT_EQ(11, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(7, g_sel_page.last_line);
}

TEST_P(OptionExpansionRowTest, collapsingAGroupHidesItsPendingEditWithoutDiscardingIt)
{
    g_option_flags[group_for(OI_CITED_TEXT_STRING)] |= OF_SEL;
    m_draft.set(OI_CITED_TEXT_STRING, "Pending citation");
    initialize_page();
    (void) render_page();
    g_option_flags[group_for(OI_CITED_TEXT_STRING)] &= ~OF_SEL;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Pending citation")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Cited Text String")));
    EXPECT_THAT(output, testing::HasSubstr(expected_heading(0, "Posting Options")));
    EXPECT_THAT(output, testing::HasSubstr(expected_heading(1, "Save Options")));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    expect_row(1, group_for(OI_SAVE_DIR), 0);
    EXPECT_EQ(10, g_sel_page.total_obj_cnt);
    EXPECT_EQ("Pending citation", m_draft.value(OI_CITED_TEXT_STRING));
    EXPECT_EQ(">", g_indent_string);
}

TEST_P(OptionExpansionRowTest, reopeningAGroupRestoresItsPendingEditAndMarker)
{
    g_option_flags[group_for(OI_CITED_TEXT_STRING)] |= OF_SEL;
    m_draft.set(OI_CITED_TEXT_STRING, "Pending citation");
    initialize_page();
    (void) render_page();
    g_option_flags[group_for(OI_CITED_TEXT_STRING)] &= ~OF_SEL;
    initialize_page();
    (void) render_page();
    g_option_flags[group_for(OI_CITED_TEXT_STRING)] |= OF_SEL;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_value(1, "Cited Text String", "Pending citation", 1)));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    expect_row(1, m_catalog.row_for(OI_CITED_TEXT_STRING), 1);
    EXPECT_EQ("Pending citation", m_draft.value(OI_CITED_TEXT_STRING));
    EXPECT_EQ(">", g_indent_string);
    EXPECT_EQ(11, g_sel_page.total_obj_cnt);
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, OptionExpansionRowTest, testing::Bool());

TEST_P(OptionPageEndingTest, skipsCollapsedOptionsWhenAdvancingToTheNextPage)
{
    const int capacity = std::get<1>(GetParam());
    (void) render_page();
    ASSERT_EQ(capacity, g_sel_page.page_item_cnt);
    ASSERT_TRUE(next_page());

    const std::string output = render_page();

    ASSERT_EQ(capacity, g_sel_page.page_item_cnt);
    for (int row = 0; row < capacity; ++row)
    {
        expect_row(row, m_headings[capacity + row], 0);
        EXPECT_THAT(output, testing::HasSubstr(expected_heading(row, OPTION_HEADINGS[capacity + row])));
    }
    EXPECT_EQ(capacity, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(10, g_sel_page.total_obj_cnt);
    EXPECT_FALSE(g_sel_page.at_end);
}

TEST_P(OptionPageEndingTest, displaysThePartialOrFullFinalPageAndRejectsFurtherAdvancement)
{
    const int capacity = std::get<1>(GetParam());
    (void) render_page();
    for (int page = 0; page < 10 && !g_sel_page.at_end; ++page)
    {
        ASSERT_TRUE(next_page());
        (void) render_page();
    }
    ASSERT_TRUE(g_sel_page.at_end);
    const int final_rows = capacity == 2 ? 2 : 1;
    const int prior = 10 - final_rows;

    const std::string output = render_page();

    ASSERT_EQ(final_rows, g_sel_page.page_item_cnt);
    for (int row = 0; row < final_rows; ++row)
    {
        expect_row(row, m_headings[prior + row], 0);
        EXPECT_THAT(output, testing::HasSubstr(expected_heading(row, OPTION_HEADINGS[prior + row])));
    }
    EXPECT_EQ(prior, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(final_rows, g_sel_page.page_obj_cnt);
    EXPECT_EQ(final_rows + 3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(next_page());
}

INSTANTIATE_TEST_SUITE_P(LabelAndPageCapacity, OptionPageEndingTest,
                         testing::Combine(testing::Bool(), testing::Values(2, 3)));

TEST_P(OptionRecoveryRowTest, returnsToVisibleHeadingsAfterTheFinalOptionsGroupCollapses)
{
    g_use_sel_num = GetParam();
    g_tc_LINES = 7;
    g_option_flags[group_for(OI_SC_VERBOSE)] |= OF_SEL;
    g_sel_page.page_op = m_catalog.row_for(OI_SC_VERBOSE);
    m_draft.set(OI_SC_VERBOSE, "yes");
    initialize_page();
    (void) render_page();
    g_option_flags[group_for(OI_SC_VERBOSE)] &= ~OF_SEL;
    init_pages(FILL_LAST_PAGE);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Verbose scoring")));
    EXPECT_THAT(output, testing::HasSubstr(expected_heading(0, "Article Scan Mode Options")));
    EXPECT_THAT(output, testing::HasSubstr(expected_heading(1, "Scoring Options")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    expect_row(0, group_for(OI_SCAN_ART_FOLLOW), 0);
    expect_row(1, group_for(OI_SC_VERBOSE), 0);
    EXPECT_EQ(8, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(10, g_sel_page.total_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_EQ("yes", m_draft.value(OI_SC_VERBOSE));
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, OptionRecoveryRowTest, testing::Bool());
