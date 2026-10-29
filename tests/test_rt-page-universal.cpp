// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include "support/NewsgroupPageState.h"

#include <trn/hash.h>
#include <trn/help.h>
#include <trn/univ.h>

#include <iomanip>
#include <sstream>
#include <tuple>

namespace
{

class UniversalPageDisplayState : public test_support::NewsgroupPageDisplayState
{
protected:
    void           SetUp() override;
    void           TearDown() override;
    UniversalItem &add_item(UniversalData data, std::string description = {}, UniversalItemFlags flags = UF_NONE);
    NewsgroupData &add_news(std::string_view name, ArticleUnread unread);
    void           add_article(std::string description, int score = 0, UniversalItemFlags flags = UF_NONE);
    void           expect_row(int row, int identity, int selected = 0) const;

private:
    UniversalItemList  m_old_items{std::move(g_univ_items)};
    HashTable         *m_old_hash{g_newsrc_hash};
    UniversalItemIndex m_old_page{g_sel_page_univ_index};
    UniversalItemIndex m_old_next{g_sel_next_univ_index};
    std::string        m_old_title{g_univ_title};
    int                m_old_level{g_univ_level};
};

void UniversalPageDisplayState::SetUp()
{
    NewsgroupPageDisplayState::SetUp();
    g_univ_items.clear();
    g_univ_items.reserve(16);
    g_sel.mode = SM_UNIVERSAL;
    g_sel_page_univ_index = {};
    g_sel_next_univ_index = {};
    g_univ_title = "Reading menu";
    g_univ_level = 2;
    g_newsrc_hash = hash_create(17, [](std::string_view key, HashDatum data)
                                { return key.compare(reinterpret_cast<NewsgroupData *>(data.dat_ptr)->rc_name()); });
}

void UniversalPageDisplayState::TearDown()
{
    hash_destroy(g_newsrc_hash);
    g_newsrc_hash = m_old_hash;
    g_univ_items = std::move(m_old_items);
    g_sel_page_univ_index = m_old_page;
    g_sel_next_univ_index = m_old_next;
    g_univ_title = m_old_title;
    g_univ_level = m_old_level;
    NewsgroupPageDisplayState::TearDown();
}

UniversalItem &UniversalPageDisplayState::add_item(UniversalData data, std::string description,
                                                   UniversalItemFlags flags)
{
    UniversalItem &item = g_univ_items.emplace_back();
    item.m_num = static_cast<int>(g_univ_items.size()) * 11;
    item.m_data = std::move(data);
    item.m_desc = std::move(description);
    item.m_flags = flags;
    return item;
}

NewsgroupData &UniversalPageDisplayState::add_news(std::string_view name, ArticleUnread unread)
{
    NewsgroupData &group = add_group(name, unread);
    group.m_rc = &m_newsrc;
    group.m_ng_max = ArticleNum{10};
    group.m_rc_line.append("1-5");
    hash_store(g_newsrc_hash, name, HashDatum{reinterpret_cast<char *>(&group), static_cast<unsigned>(name.size())});
    return group;
}

void UniversalPageDisplayState::add_article(std::string description, int score, UniversalItemFlags flags)
{
    add_item(UniversalVirtualArticle{"comp.reading", "<article@example.org>", "Ada", "Re: Reading", ArticleNum{8}},
             std::move(description), flags)
        .m_score = score;
}

void UniversalPageDisplayState::expect_row(int row, int identity, int selected) const
{
    EXPECT_EQ(univ_item(identity), g_sel_items[row].u.un);
    EXPECT_EQ(row + 2, g_sel_items[row].line);
    EXPECT_EQ(selected, g_sel_items[row].sel);
}

class UniversalMenuRowTest : public UniversalPageDisplayState, public testing::WithParamInterface<std::tuple<int, bool>>
{
};

class UniversalArticleRowTest : public UniversalPageDisplayState,
                                public testing::WithParamInterface<std::tuple<bool, int>>
{
protected:
    void SetUp() override;
};

void UniversalArticleRowTest::SetUp()
{
    UniversalPageDisplayState::SetUp();
    (void) add_news("comp.reading", 5);
}

class UniversalGroupRowTest : public UniversalPageDisplayState, public testing::WithParamInterface<ArticleUnread>
{
};

class UniversalHelpRowTest : public UniversalPageDisplayState,
                             public testing::WithParamInterface<std::pair<HelpLocation, std::string_view>>
{
};

class UniversalMarkerTest : public UniversalPageDisplayState,
                            public testing::WithParamInterface<std::tuple<bool, bool, int>>
{
};

class UniversalOrderTest : public UniversalPageDisplayState,
                           public testing::WithParamInterface<std::tuple<SelectionSortMode, int>>
{
};

class UniversalEligibilityTest : public UniversalPageDisplayState, public testing::WithParamInterface<int>
{
};

class UniversalPagingTest : public UniversalPageDisplayState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void UniversalPagingTest::SetUp()
{
    UniversalPageDisplayState::SetUp();
    g_use_sel_num = GetParam();
    g_tc_LINES = 7;
}

} // namespace

TEST_P(UniversalMenuRowTest, displaysTheDescriptionWithoutChangingTheNavigationPayload)
{
    const auto [kind, described] = GetParam();
    const std::string description = described ? "Menu description" : "";
    if (kind == 0)
    {
        (void) add_item(UniversalConfigFileData{"Child menu", "child.menu", "reading"}, description);
    }
    else if (kind == 1)
    {
        (void) add_item(UniversalGroupMaskData{"Languages", "comp.lang.*"}, description);
    }
    else
    {
        (void) add_item(UniversalTextFile{"reading.txt"}, description);
    }
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a        " + (described ? description : "[No Description]") + "\n"));
    EXPECT_THAT(output, testing::HasSubstr("[2] Reading menu"));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 11);
    EXPECT_EQ(description, g_univ_items[0].m_desc);
    EXPECT_EQ(0, g_univ_items[0].m_score);
    if (kind == 0)
    {
        EXPECT_EQ("Child menu", g_univ_items[0].config_file().title);
        EXPECT_EQ("child.menu", g_univ_items[0].config_file().fname);
        EXPECT_EQ("reading", g_univ_items[0].config_file().label);
    }
    else if (kind == 1)
    {
        EXPECT_EQ("Languages", g_univ_items[0].group_mask().title);
        EXPECT_EQ("comp.lang.*", g_univ_items[0].group_mask().mask_list);
    }
    else
    {
        EXPECT_EQ("reading.txt", g_univ_items[0].text_file().fname);
    }
    EXPECT_EQ(0, g_sel_next_univ_index);
    EXPECT_TRUE(g_sel_at_end);
}

INSTANTIATE_TEST_SUITE_P(PayloadAndDescription, UniversalMenuRowTest,
                         testing::Combine(testing::Values(0, 1, 2), testing::Bool()));

TEST_P(UniversalArticleRowTest, displaysCustomOrGeneratedDescriptionsWithoutChangingTheArticle)
{
    const auto [custom, score] = GetParam();
    add_article(custom ? "Featured article" : "", score);
    initialize_page();
    std::ostringstream generated;
    generated << "[" << std::setw(3) << score << "] " << std::left << std::setw(16) << "Ada" << " >Reading";

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a        " + (custom ? "Featured article" : generated.str()) + "\n"));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 11);
    const UniversalItem &item = g_univ_items[0];
    EXPECT_EQ(score, item.m_score);
    EXPECT_EQ(custom ? "Featured article" : "", item.m_desc);
    EXPECT_EQ("comp.reading", item.article().ng);
    EXPECT_EQ("<article@example.org>", item.article().id);
    EXPECT_EQ("Ada", item.article().from);
    EXPECT_EQ("Re: Reading", item.article().subj);
    EXPECT_EQ(ArticleNum{8}, item.article().num);
    EXPECT_EQ(" 1-5", find_newsgroup("comp.reading")->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(DescriptionAndScore, UniversalArticleRowTest,
                         testing::Combine(testing::Bool(), testing::Values(-12, 7)));

TEST_F(UniversalPageDisplayState, suppliesMissingArticleMetadataInTheDisplayedRow)
{
    (void) add_news("comp.reading", 5);
    (void) add_item(UniversalVirtualArticle{"comp.reading", "<empty@example.org>", "", "", ArticleNum{8}});
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("[  0]     <No Author>  <No Subject>\n"));
    EXPECT_TRUE(g_univ_items[0].article().from.empty());
    EXPECT_TRUE(g_univ_items[0].article().subj.empty());
}

TEST_P(UniversalGroupRowTest, displaysTheGroupReadStatusWithoutChangingItsNewsrc)
{
    const ArticleUnread count = GetParam();
    NewsgroupData      &group = add_news("comp.reading", count);
    (void) add_item(UniversalNewsgroup{"comp.reading"}, "Ignored group description");
    set_read_scope(count <= 0);
    initialize_page();
    std::ostringstream expected;
    if (count >= 0)
    {
        expected << std::setw(5) << count << " ";
    }
    else
    {
        expected << (count == TR_UNSUB ? "UNSUB " : "***** ");
    }

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected.str() + "comp.reading\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Ignored group description")));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 11, count <= 0 ? 2 : 0);
    EXPECT_EQ(count, group.m_to_read);
    EXPECT_EQ(" 1-5", group.rc_numbers());
    EXPECT_EQ("comp.reading", g_univ_items[0].group().ng);
    EXPECT_EQ(0, g_sel_next_univ_index);
}

INSTANTIATE_TEST_SUITE_P(ReadStatus, UniversalGroupRowTest,
                         testing::Values(ArticleUnread{3}, ArticleUnread{0}, ArticleUnread{TR_UNSUB},
                                         ArticleUnread{TR_BOGUS}));

TEST_P(UniversalHelpRowTest, displaysTheHelpDestinationInsteadOfItsDescription)
{
    const auto [location, name] = GetParam();
    (void) add_item(location, "Ignored help description");
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a        Help on the " + std::string{name} + "\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Ignored help description")));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 11);
    EXPECT_EQ(location, g_univ_items[0].help_location());
}

INSTANTIATE_TEST_SUITE_P(Destination, UniversalHelpRowTest,
                         testing::Values(std::pair{UHELP_PAGE, std::string_view{"Article Pager Mode"}},
                                         std::pair{UHELP_ART, std::string_view{"Article Display/Selection Mode"}},
                                         std::pair{UHELP_NG, std::string_view{"Newsgroup Browse Mode"}},
                                         std::pair{UHELP_NGSEL, std::string_view{"Newsgroup Selector"}},
                                         std::pair{UHELP_ADDSEL, std::string_view{"Add-Newsgroup Selector"}},
                                         std::pair{UHELP_SUBS, std::string_view{"Escape Substitutions"}},
                                         std::pair{UHELP_ARTSEL, std::string_view{"Thread/Subject/Article Selector"}},
                                         std::pair{UHELP_MULTIRC, std::string_view{"Newsrc Selector"}},
                                         std::pair{UHELP_OPTIONS, std::string_view{"Option Selector"}},
                                         std::pair{UHELP_SCANART, std::string_view{"Article Scan Mode"}},
                                         std::pair{UHELP_UNIV, std::string_view{"Universal Selector"}}));

TEST_P(UniversalMarkerTest, rendersTheActiveScopeSelectionAndDeletionMarker)
{
    const auto [rereading, numeric, state] = GetParam();
    (void) add_news("comp.reading", 5);
    set_read_scope(rereading);
    g_use_sel_num = numeric;
    UniversalItemFlags flags = state & 1 ? (rereading ? UF_DEL_SEL : UF_SEL) : UF_NONE;
    if (state & 2)
    {
        flags |= UF_DEL;
    }
    add_article("Reading", 0, flags);
    g_univ_items[0].article().num = ArticleNum{rereading ? 3 : 8};
    initialize_page();
    // Read-mode initialization marks unselected read articles for deletion.
    const int marker = rereading && !(state & 1) ? 2 : state;

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(std::string{numeric ? "01" : "a"} + " +-*"[marker] + "       Reading\n"));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 11, marker);
    EXPECT_EQ(flags | UF_INCLUDED | (rereading && !(state & 1) ? UF_DEL : UF_NONE), g_univ_items[0].m_flags);
}

INSTANTIATE_TEST_SUITE_P(ScopeLabelAndState, UniversalMarkerTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Values(0, 1, 2, 3)));

TEST_P(UniversalOrderTest, keepsLabelsAndPayloadsTogetherAfterSorting)
{
    const auto [sort, direction] = GetParam();
    (void) add_news("comp.reading", 5);
    add_article("First", -5);
    add_article("Second", 20, UF_SEL);
    add_article("Third", 20);
    g_sel.sort = sort;
    g_sel.direction = direction;
    initialize_page();
    const std::array<int, 3> identities =
        sort == SS_SCORE ? (direction > 0 ? std::array<int, 3>{22, 33, 11} : std::array<int, 3>{11, 33, 22})
                         : (direction > 0 ? std::array<int, 3>{11, 22, 33} : std::array<int, 3>{33, 22, 11});

    const std::string output = render_page();

    ASSERT_EQ(3, g_sel_page_item_cnt);
    for (int row = 0; row < 3; ++row)
    {
        const int            identity = identities[row];
        const UniversalItem *item = univ_item(identity);
        ASSERT_NE(nullptr, item);
        expect_row(row, identity, identity == 22 ? 1 : 0);
        EXPECT_EQ(identity == 11 ? -5 : 20, item->m_score);
        EXPECT_EQ(ArticleNum{8}, item->article().num);
        const std::string description = identity == 11 ? "First" : identity == 22 ? "Second" : "Third";
        EXPECT_EQ(description, item->m_desc);
        EXPECT_THAT(output, testing::HasSubstr(std::string(1, "abc"[row]) + (identity == 22 ? "+       " : "        ") +
                                               description + "\n"));
    }
    EXPECT_EQ(0, g_sel_next_univ_index);
    EXPECT_TRUE(g_sel_at_end);
}

INSTANTIATE_TEST_SUITE_P(SortAndDirection, UniversalOrderTest,
                         testing::Combine(testing::Values(SS_NATURAL, SS_SCORE), testing::Values(1, -1)));

TEST_P(UniversalEligibilityTest, omitsAnIneligibleEntryWithoutConsumingItsRowLabel)
{
    (void) add_news("comp.reading", 5);
    switch (GetParam())
    {
    case 0:
        (void) add_item(UniversalNewsgroup{"comp.missing"}, "Hidden");
        break;

    case 1:
        (void) add_item(UniversalVirtualGroup{"comp.*", 0, 100, UF_VG_NONE}, "Hidden");
        break;

    case 2:
        add_article("Hidden");
        g_univ_items.back().article().num = ArticleNum{3};
        break;

    default:
        add_article("Hidden");
        g_univ_items.back().m_state = UIS_DESELECTED;
        break;
    }
    (void) add_item(UniversalTextFile{"reading.txt"}, "Visible text");
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a        Visible text\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Hidden")));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 22);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_next_univ_index);
    EXPECT_TRUE(g_sel_at_end);
}

INSTANTIATE_TEST_SUITE_P(IneligibleEntry, UniversalEligibilityTest, testing::Values(0, 1, 2, 3));

TEST_F(UniversalPageDisplayState, displaysOnlySelectedEntriesInExclusiveMode)
{
    (void) add_item(UniversalTextFile{"first.txt"}, "Unselected");
    (void) add_item(UniversalConfigFileData{"Menu", "child.menu", ""}, "Selected", UF_SEL);
    (void) add_item(UHELP_UNIV, "Unselected help");
    g_sel.exclusive = true;
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("a+       Selected\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Unselected")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Help on")));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 22, 1);
    EXPECT_EQ(1, g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
}

TEST_P(UniversalPagingTest, resumesFromTheNextIdentityAcrossHiddenEntries)
{
    (void) add_item(UniversalTextFile{"first.txt"}, "First");
    add_item(UniversalTextFile{"hidden.txt"}, "Hidden").m_state = UIS_DESELECTED;
    (void) add_item(UHELP_UNIV);
    (void) add_item(UniversalConfigFileData{"Menu", "child.menu", ""}, "Last");
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(std::string{GetParam() ? "01" : "a"} + "        First\n"));
    EXPECT_THAT(output,
                testing::HasSubstr(std::string{GetParam() ? "02" : "b"} + "        Help on the Universal Selector\n"));
    ASSERT_EQ(2, g_sel_page_item_cnt);
    expect_row(0, 11);
    expect_row(1, 33);
    EXPECT_EQ(44, g_sel_next_univ_index);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
    EXPECT_FALSE(g_sel_at_end);
}

TEST_P(UniversalPagingTest, rendersTheFinalPartialPageAndStops)
{
    (void) add_item(UniversalTextFile{"first.txt"}, "First");
    (void) add_item(UHELP_UNIV);
    (void) add_item(UniversalConfigFileData{"Menu", "child.menu", "last"}, "Last");
    add_item(UniversalTextFile{"hidden.txt"}, "Hidden").m_state = UIS_DESELECTED;
    initialize_page();
    (void) render_page();
    ASSERT_TRUE(next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(std::string{GetParam() ? "01" : "a"} + "        Last\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("First")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Hidden")));
    ASSERT_EQ(1, g_sel_page_item_cnt);
    expect_row(0, 33);
    EXPECT_EQ("last", g_sel_items[0].u.un->config_file().label);
    EXPECT_EQ(2, g_sel_prior_obj_cnt);
    EXPECT_EQ(0, g_sel_next_univ_index);
    EXPECT_TRUE(g_sel_at_end);
    EXPECT_FALSE(next_page());
}

TEST_P(UniversalPagingTest, endsAnExactlyFullFinalPage)
{
    (void) add_item(UniversalTextFile{"first.txt"}, "First");
    (void) add_item(UHELP_UNIV);
    initialize_page();

    (void) render_page();

    EXPECT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(0, g_sel_next_univ_index);
    EXPECT_TRUE(g_sel_at_end);
    EXPECT_FALSE(next_page());
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, UniversalPagingTest, testing::Bool());

TEST_F(UniversalPageDisplayState, rendersAnEmptyMenuWithoutStalePageCounts)
{
    initialize_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr("[2] Reading menu"));
    EXPECT_EQ(0, g_sel_page_item_cnt);
    EXPECT_EQ(0, g_sel_page_obj_cnt);
    EXPECT_EQ(0, g_sel_next_univ_index);
    EXPECT_TRUE(g_sel_at_end);
}
