// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/addng-internal.h>

#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/ngdata.h>
#include <trn/only.h>
#include <trn/rcstuff.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/search.h>
#include <trn/smisc.h>
#include <trn/terminal.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <test_config.h>

#include "MockNNTPConnection.h"

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class ActiveListPatternTest : public testing::Test
{
protected:
    void SetUp() override
    {
        search_init();
        m_old_newsgroup_to_do = g_newsgroup_to_do;
        m_old_max_newsgroup_to_do = g_max_newsgroup_to_do;

        g_newsgroup_to_do.fill(std::string{});
        g_max_newsgroup_to_do = 0;
    }

    void TearDown() override
    {
        g_newsgroup_to_do = m_old_newsgroup_to_do;
        g_max_newsgroup_to_do = m_old_max_newsgroup_to_do;
    }

    void set_restrictions(std::initializer_list<std::string_view> restrictions)
    {
        g_max_newsgroup_to_do = 0;
        for (std::string_view restriction : restrictions)
        {
            g_newsgroup_to_do[g_max_newsgroup_to_do] = restriction;
            ++g_max_newsgroup_to_do;
        }
    }

    std::array<std::string, MAX_NG_TO_DO> m_old_newsgroup_to_do;
    int                                   m_old_max_newsgroup_to_do{};
};

class AddGroupStorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_groups = std::move(g_add_groups);
        g_add_groups = {};
        m_old_page = std::exchange(g_sel_page_gp, AddGroupIndex::None);
        m_old_next = std::exchange(g_sel_next_gp, AddGroupIndex::None);
        m_old_data_source = g_data_source;
        m_old_sel_sort = g_sel_sort;
        m_old_sel_direction = g_sel_direction;
        g_data_source = nullptr;
        g_sel_sort = SS_NATURAL;
        g_sel_direction = 1;
    }

    void TearDown() override
    {
        g_add_groups = std::move(m_old_groups);
        g_sel_page_gp = m_old_page;
        g_sel_next_gp = m_old_next;
        g_data_source = m_old_data_source;
        g_sel_sort = m_old_sel_sort;
        g_sel_direction = m_old_sel_direction;
    }

    AddGroupTable     m_old_groups;
    AddGroupIndex     m_old_page{};
    AddGroupIndex     m_old_next{};
    DataSource       *m_old_data_source{};
    SelectionSortMode m_old_sel_sort{};
    int               m_old_sel_direction{};
};

class StoredAddGroupsTest : public AddGroupStorageTest
{
protected:
    void SetUp() override;

    AddGroupIndex m_first{};
    AddGroupIndex m_last{};
};

void StoredAddGroupsTest::SetUp()
{
    AddGroupStorageTest::SetUp();
    m_first = add_to_list("comp.lang.c++", 42, ':');
    m_last = add_to_list("news.software.readers", -1, '!');
    ASSERT_NE(AddGroupIndex::None, m_first);
    ASSERT_NE(AddGroupIndex::None, m_last);
}

class AddGroupInitializationTest : public AddGroupStorageTest, public testing::WithParamInterface<bool>
{
};

class StoredAddGroupTraversalTest : public StoredAddGroupsTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void StoredAddGroupTraversalTest::SetUp()
{
    StoredAddGroupsTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    ASSERT_EQ(m_first, add_to_list("comp.lang.c++", 99, '!'));
}

struct EmptyGroupQueryCase
{
    const char *name;
    bool (*is_empty)(AddGroupTable &);
};

const EmptyGroupQueryCase EMPTY_GROUP_QUERY_CASES[]{
    {"Empty", [](AddGroupTable &groups) { return groups.empty(); }},
    {"Indices", [](AddGroupTable &groups) { return groups.indices().empty(); }},
    {"First", [](AddGroupTable &groups) { return groups.first() == AddGroupIndex::None; }},
    {"Last", [](AddGroupTable &groups) { return groups.last() == AddGroupIndex::None; }},
    {"Next", [](AddGroupTable &groups) { return groups.next(AddGroupIndex::None) == AddGroupIndex::None; }},
    {"Previous", [](AddGroupTable &groups) { return groups.prev(AddGroupIndex::None) == AddGroupIndex::None; }}};

void PrintTo(const EmptyGroupQueryCase &value, std::ostream *output)
{
    *output << value.name;
}

class EmptyAddGroupQueryTest : public AddGroupStorageTest, public testing::WithParamInterface<EmptyGroupQueryCase>
{
protected:
    void SetUp() override;
};

void EmptyAddGroupQueryTest::SetUp()
{
    AddGroupStorageTest::SetUp();
    sort_add_groups();
}

class GrownAddGroupIdentityTest : public AddGroupStorageTest
{
protected:
    void SetUp() override;

    AddGroupIndex m_beta{};
    AddGroupIndex m_alpha{};
    Selection     m_item{};
};

void GrownAddGroupIdentityTest::SetUp()
{
    AddGroupStorageTest::SetUp();
    m_beta = add_to_list("comp.lang.beta", 5, ':');
    m_alpha = add_to_list("comp.lang.alpha", 3, '!');
    ASSERT_NE(AddGroupIndex::None, m_beta);
    ASSERT_NE(AddGroupIndex::None, m_alpha);
    g_sel_page_gp = m_beta;
    g_sel_next_gp = m_alpha;
    m_item.gp = m_beta;
    for (int i = 0; i != 256; ++i)
    {
        add_to_list("news.test." + std::to_string(i), i, 0);
    }
    g_sel_sort = SS_STRING;
}

class NaturalAddGroupOrderTest : public GrownAddGroupIdentityTest
{
protected:
    void SetUp() override;
};

void NaturalAddGroupOrderTest::SetUp()
{
    GrownAddGroupIdentityTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    sort_add_groups();
    ASSERT_EQ(m_beta, add_to_list("comp.lang.beta", 99, '!'));
    g_sel_sort = SS_NATURAL;
}

class ScanActiveTest : public testing::Test
{
protected:
    void SetUp() override
    {
        search_init();
        m_saved_data_sources.swap(g_data_sources);
        m_old_data_source = g_data_source;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_multirc = g_multirc;
        m_old_nntp_link = g_nntp_link;
        m_old_quick_start = g_quick_start;
        m_old_use_add_selector = g_use_add_selector;
        m_old_groups = std::move(g_add_groups);
        g_add_groups = {};
        m_old_page = std::exchange(g_sel_page_gp, AddGroupIndex::None);
        m_old_next = std::exchange(g_sel_next_gp, AddGroupIndex::None);
        m_old_int_count = g_int_count;
        m_old_erase_screen = g_erase_screen;
        m_old_tc_am = g_tc_AM;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_page_line = g_page_line;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_cr = g_tc_CR;
        m_old_tc_ho = g_tc_HO;
        m_old_tc_ce = g_tc_CE;
        m_old_tc_up = g_tc_UP;
        m_old_tc_bc = g_tc_BC;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_data_sources.clear();
        g_data_sources.reserve(4);
        g_data_source = nullptr;
        g_newsrc_hash = hash_create(17, nullptr);
        nntp_gets_clear_buffer();
        m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
        g_quick_start = false;
        g_use_add_selector = false;
        g_int_count = 0;
        g_erase_screen = false;
        g_tc_AM = false;
        g_tc_LINES = 100;
        g_tc_COLS = 200;
        g_page_line = 1;
        g_term_line = 0;
        g_term_col = 0;
        g_tc_SO = "";
        g_tc_SE = "";
        g_tc_CR = "\r";
        g_tc_HO = "\033[H";
        g_tc_CE = "";
        g_tc_UP = "\033[A";
        g_tc_BC = "\b";

        m_newsrc.data_source = nullptr;
        m_newsrc.flags = RF_NONE;
        m_multirc.m_first = &m_newsrc;
        g_multirc = &m_multirc;
    }

    void TearDown() override
    {
        for (DataSource &source : g_data_sources)
        {
            source.m_act_sf.close();
        }
        g_data_sources.clear();
        m_saved_data_sources.swap(g_data_sources);

        if (g_newsrc_hash != nullptr && g_newsrc_hash != m_old_newsrc_hash)
        {
            hash_destroy(g_newsrc_hash);
        }
        g_newsrc_hash = m_old_newsrc_hash;
        nntp_gets_clear_buffer();
        g_nntp_link = m_old_nntp_link;
        g_data_source = m_old_data_source;
        g_multirc = m_old_multirc;
        g_add_groups = std::move(m_old_groups);
        g_sel_page_gp = m_old_page;
        g_sel_next_gp = m_old_next;
        g_quick_start = m_old_quick_start;
        g_use_add_selector = m_old_use_add_selector;
        g_int_count = m_old_int_count;
        g_erase_screen = m_old_erase_screen;
        g_tc_AM = m_old_tc_am;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_page_line = m_old_page_line;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_CR = m_old_tc_cr;
        g_tc_HO = m_old_tc_ho;
        g_tc_CE = m_old_tc_ce;
        g_tc_UP = m_old_tc_up;
        g_tc_BC = m_old_tc_bc;

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    void add_active_source(std::string_view active_line)
    {
        const fs::path source_path = m_output_dir / ("active-" + std::to_string(g_data_sources.size()));
        std::ofstream{source_path} << active_line;
        g_data_sources.emplace_back();
        DataSource &source = g_data_sources.back();
        source.m_flags = DF_OPEN;
        ASSERT_EQ(1, source.m_act_sf.open(source_path, "", ""));
    }

    void add_remote_active_source(std::string_view active_line)
    {
        const fs::path source_path = m_output_dir / ("active-" + std::to_string(g_data_sources.size()));
        std::ofstream{source_path} << active_line;
        g_data_sources.emplace_back();
        DataSource &source = g_data_sources.back();
        source.m_flags = DF_OPEN | DF_REMOTE;
        source.m_extra_name = source_path.generic_string();
        source.m_nntp_link.connection = m_connection;
        source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
        ASSERT_EQ(1, source.m_act_sf.open(source_path, "", ""));
        std::fclose(source.m_act_sf.m_fp);
        source.m_act_sf.m_fp = std::fopen(source_path.string().c_str(), "r+");
        ASSERT_NE(nullptr, source.m_act_sf.m_fp);
        source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
        m_newsrc.data_source = &source;
        m_newsrc.flags = RF_ADD_NEW_GROUPS | RF_ACTIVE;
    }

    void add_uncached_remote_source()
    {
        g_data_sources.emplace_back();
        DataSource &source = g_data_sources.back();
        source.m_flags = DF_OPEN | DF_REMOTE;
        source.m_nntp_link.connection = m_connection;
        source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
    }

    void add_local_active_source(std::string_view active_line, std::string_view active_times_line)
    {
        const fs::path source_path = m_output_dir / ("active-" + std::to_string(g_data_sources.size()));
        const fs::path active_times_path = m_output_dir / ("active-times-" + std::to_string(g_data_sources.size()));
        std::ofstream{source_path} << active_line;
        std::ofstream{active_times_path} << active_times_line;
        g_data_sources.emplace_back();
        DataSource &source = g_data_sources.back();
        source.m_flags = DF_OPEN;
        source.m_extra_name = active_times_path.generic_string();
        ASSERT_EQ(1, source.m_act_sf.open(source_path, "", ""));
        m_newsrc.data_source = &source;
        m_newsrc.flags = RF_ADD_NEW_GROUPS | RF_ACTIVE;
    }

    fs::path                m_output_dir;
    std::vector<DataSource> m_saved_data_sources;
    DataSource             *m_old_data_source{};
    HashTable              *m_old_newsrc_hash{};
    Multirc                *m_old_multirc{};
    NNTPLink                m_old_nntp_link{};
    bool                    m_old_quick_start{};
    bool                    m_old_use_add_selector{};
    AddGroupTable                                            m_old_groups;
    AddGroupIndex                                            m_old_page{};
    AddGroupIndex                                            m_old_next{};
    char                    m_old_int_count{};
    bool                    m_old_erase_screen{};
    bool                    m_old_tc_am{};
    int                     m_old_tc_lines{};
    int                     m_old_tc_cols{};
    int                     m_old_page_line{};
    int                     m_old_term_line{};
    int                     m_old_term_col{};
    std::string_view         m_old_tc_so;
    std::string_view         m_old_tc_se;
    std::string_view                                         m_old_tc_cr;
    std::string_view                                         m_old_tc_ho;
    std::string_view                                         m_old_tc_ce;
    std::string_view                                         m_old_tc_up;
    std::string_view                                         m_old_tc_bc;
    Newsrc                  m_newsrc{};
    Multirc                 m_multirc{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
};

class SelectorGroupListingTest : public ScanActiveTest
{
protected:
    void SetUp() override
    {
        ScanActiveTest::SetUp();
        m_old_errno = errno;
        m_old_sel_mode = g_sel_mode;
        m_old_sel_sort = g_sel_sort;
        m_old_sel_direction = g_sel_direction;
        m_old_sel_sort_string = g_sel_sort_string;
        m_old_sel_mode_string = g_sel_mode_string;
        m_old_sel_rereading = g_sel_rereading;
        m_old_sel_exclusive = g_sel_exclusive;
        m_old_selected_count = g_selected_count;
        m_old_sel_mask = g_sel_mask;
        m_old_display_mode = g_sel_grp_display_mode;
        m_old_display_index = g_sel_grp_display_mode_index;
        m_old_use_sel_num = g_use_sel_num;
        m_old_in_ng = g_in_ng;
        m_old_min_to_read = g_newsgroup_min_to_read;
        push_only();
        g_use_add_selector = true;
        g_sel_grp_display_mode = "s";
        g_sel_grp_display_mode_index = 0;
        g_use_sel_num = true;
        g_in_ng = false;
        g_tc_LINES = 8;
        set_selector(SM_ADD_GROUP, SS_NATURAL);
    }

    void TearDown() override
    {
        pop_only();
        drain_input();
        errno = m_old_errno;
        set_selector(SM_ADD_GROUP, SS_NATURAL);
        g_sel_mode = m_old_sel_mode;
        g_sel_sort = m_old_sel_sort;
        g_sel_direction = m_old_sel_direction;
        g_sel_sort_string = m_old_sel_sort_string;
        g_sel_mode_string = m_old_sel_mode_string;
        g_sel_rereading = m_old_sel_rereading;
        g_sel_exclusive = m_old_sel_exclusive;
        g_selected_count = m_old_selected_count;
        g_sel_mask = m_old_sel_mask;
        g_sel_grp_display_mode = m_old_display_mode;
        g_sel_grp_display_mode_index = m_old_display_index;
        g_use_sel_num = m_old_use_sel_num;
        g_in_ng = m_old_in_ng;
        g_newsgroup_min_to_read = m_old_min_to_read;
        ScanActiveTest::TearDown();
    }

    static void drain_input()
    {
        while (macro_pending())
        {
            (void) read_tty_char();
        }
    }

    std::string run_listing(std::string_view commands)
    {
        const NewsgroupNum old_count = g_newsgroup_count;
        drain_input();
        push_string(commands, 0200);
        errno = 0;
        testing::internal::CaptureStdout();
        const bool        changed = scan_active(false);
        const std::string output = testing::internal::GetCapturedStdout();
        drain_input();
        EXPECT_FALSE(changed);
        EXPECT_EQ(old_count, g_newsgroup_count);
        EXPECT_TRUE(g_add_groups.empty());
        EXPECT_EQ(AddGroupIndex::None, g_sel_page_gp);
        EXPECT_EQ(AddGroupIndex::None, g_sel_next_gp);
        return output;
    }

    static std::string last_display(const std::string &output)
    {
        const std::size_t start = output.rfind("Unsubscribed newsgroups");
        EXPECT_NE(std::string::npos, start);
        return start == std::string::npos ? std::string{} : output.substr(start);
    }

    void add_seven_groups()
    {
        add_active_source("test.g 1 1 y\ntest.c 5 1 y\ntest.f 2 1 y\ntest.a 7 1 y\n"
                          "test.e 3 1 y\ntest.b 6 1 y\ntest.d 4 1 y\n");
    }

    int               m_old_errno{};
    SelectionMode     m_old_sel_mode{};
    SelectionSortMode m_old_sel_sort{};
    int               m_old_sel_direction{};
    std::string_view  m_old_sel_sort_string;
    std::string_view  m_old_sel_mode_string;
    bool              m_old_sel_rereading{};
    bool              m_old_sel_exclusive{};
    ArticleUnread     m_old_selected_count{};
    AddGroupFlags     m_old_sel_mask{};
    std::string       m_old_display_mode;
    std::size_t       m_old_display_index{};
    bool              m_old_use_sel_num{};
    bool              m_old_in_ng{};
    ArticleUnread     m_old_min_to_read{};
};

} // namespace

TEST_F(ActiveListPatternTest, unrestrictedScanRequestsAllGroups)
{
    EXPECT_EQ("*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, multipleRestrictionsRequestAllGroups)
{
    set_restrictions({"comp", "news"});

    EXPECT_EQ("*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, singleRestrictionMatchesSubstring)
{
    set_restrictions({"comp.lang"});

    EXPECT_EQ("*comp.lang*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, leadingCaretMatchesPrefix)
{
    set_restrictions({"^comp.lang"});

    EXPECT_EQ("comp.lang*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, trailingDollarRemovesGeneratedSuffix)
{
    set_restrictions({"comp.lang$"});

    EXPECT_EQ("*comp.lang", active_list_pattern());
}

TEST_F(ActiveListPatternTest, leadingCaretWithTrailingDollarMatchesExactPrefix)
{
    set_restrictions({"^comp.lang$"});

    EXPECT_EQ("comp.lang", active_list_pattern());
}

TEST_F(ActiveListPatternTest, usesCurrentRestrictionInsideSavedOuterRestrictions)
{
    set_newsgroup_to_do("parent.*");
    push_only();
    set_newsgroup_to_do("news.*");

    EXPECT_EQ("*news.**", active_list_pattern());

    pop_only();
    EXPECT_TRUE(in_list("parent.group"));
    EXPECT_FALSE(in_list("news.test"));
    end_only();
}

TEST_F(StoredAddGroupsTest, duplicateInsertionPreservesIdentityAndStoredData)
{
    const AddGroupIndex duplicate = add_to_list("comp.lang.c++", 99, '!');

    ASSERT_EQ(2U, g_add_groups.size());
    EXPECT_EQ(m_first, duplicate);
    EXPECT_EQ("comp.lang.c++", g_add_groups[m_first].m_name);
    EXPECT_EQ("news.software.readers", g_add_groups[m_last].m_name);
    EXPECT_EQ(42, g_add_groups[m_first].m_to_read.value_of());
    EXPECT_EQ(0, g_add_groups[m_last].m_to_read.value_of());
    EXPECT_EQ(AGF_SEL, g_add_groups[m_first].m_flags);
    EXPECT_EQ(AGF_DEL, g_add_groups[m_last].m_flags);
}

TEST_P(AddGroupInitializationTest, initializesStoredNameCountAndSelection)
{
    const bool        unsubscribed = GetParam();
    const std::string name = unsubscribed ? "news.software.readers" : "comp.lang.c++";

    const AddGroupIndex index = add_to_list(name, unsubscribed ? -1 : 42, unsubscribed ? '!' : ':');

    ASSERT_NE(AddGroupIndex::None, index);
    EXPECT_EQ(1U, g_add_groups.size());
    EXPECT_EQ(name, g_add_groups[index].m_name);
    EXPECT_EQ(unsubscribed ? 0 : 42, g_add_groups[index].m_to_read.value_of());
    EXPECT_EQ(unsubscribed ? AGF_DEL : AGF_SEL, g_add_groups[index].m_flags);
}

INSTANTIATE_TEST_SUITE_P(SubscriptionState, AddGroupInitializationTest, testing::Bool());

TEST_P(StoredAddGroupTraversalTest, startsAtTheDirectionBoundary)
{
    const AddGroupIndex index = GetParam() ? g_add_groups.last() : g_add_groups.first();

    EXPECT_EQ(GetParam() ? m_last : m_first, index);
}

TEST_P(StoredAddGroupTraversalTest, visitsTheOtherStoredGroup)
{
    const AddGroupIndex index = GetParam() ? g_add_groups.prev(m_last) : g_add_groups.next(m_first);

    EXPECT_EQ(GetParam() ? m_first : m_last, index);
}

TEST_P(StoredAddGroupTraversalTest, stopsAtTheTraversalBoundary)
{
    const AddGroupIndex index = GetParam() ? g_add_groups.prev(m_first) : g_add_groups.next(m_last);

    EXPECT_EQ(AddGroupIndex::None, index);
}

INSTANTIATE_TEST_SUITE_P(ReverseOrForward, StoredAddGroupTraversalTest, testing::Bool());

TEST_F(AddGroupStorageTest, sortAddGroupsOrdersByUnreadCount)
{
    add_to_list("comp.lang.beta", 5, ':');
    add_to_list("comp.lang.alpha", 3, ':');
    add_to_list("comp.lang.gamma", 9, ':');
    g_sel_sort = SS_COUNT;

    sort_add_groups();

    std::vector<std::string> names;
    for (AddGroupIndex index : g_add_groups.indices())
    {
        names.push_back(g_add_groups[index].m_name);
    }
    EXPECT_THAT(names, testing::ElementsAre("comp.lang.alpha", "comp.lang.beta", "comp.lang.gamma"));
    const AddGroupIndex first = g_add_groups.first();
    const AddGroupIndex second = g_add_groups.next(first);
    const AddGroupIndex third = g_add_groups.next(second);
    EXPECT_EQ(first, g_add_groups.prev(second));
    EXPECT_EQ(second, g_add_groups.prev(third));
    EXPECT_EQ(third, g_add_groups.last());
    EXPECT_EQ(AddGroupIndex::None, g_add_groups.next(third));
}

TEST_P(EmptyAddGroupQueryTest, reportsNoStoredGroups)
{
    const bool empty = GetParam().is_empty(g_add_groups);

    EXPECT_TRUE(empty);
}

INSTANTIATE_TEST_SUITE_P(Query, EmptyAddGroupQueryTest, testing::ValuesIn(EMPTY_GROUP_QUERY_CASES),
                         [](const testing::TestParamInfo<EmptyGroupQueryCase> &info) { return info.param.name; });

TEST_F(GrownAddGroupIdentityTest, preservesPageSelectionAndDuplicateIdentityAcrossSorting)
{
    sort_add_groups();

    EXPECT_EQ(m_alpha, g_add_groups.first());
    EXPECT_EQ("comp.lang.beta", g_add_groups[g_sel_page_gp].m_name);
    EXPECT_EQ("comp.lang.alpha", g_add_groups[g_sel_next_gp].m_name);
    EXPECT_EQ(AGF_SEL, g_add_groups[m_item.gp].m_flags);
    EXPECT_EQ(m_beta, add_to_list("comp.lang.beta", 99, '!'));
}

TEST_F(NaturalAddGroupOrderTest, restoresInsertionOrderAfterNameSorting)
{
    sort_add_groups();

    EXPECT_EQ(m_beta, g_add_groups.first());
    EXPECT_EQ(m_alpha, g_add_groups.next(m_beta));
}

TEST_F(AddGroupStorageTest, nameAndCountSortingUseDirectionAndNameTies)
{
    const AddGroupIndex z = add_to_list("comp.Z", 3, ':');
    const AddGroupIndex a = add_to_list("comp.a", 3, ':');
    const AddGroupIndex b = add_to_list("comp.b", 9, ':');
    g_sel_sort = SS_COUNT;
    sort_add_groups();
    EXPECT_THAT(g_add_groups.indices(), testing::ElementsAre(a, z, b));
    g_sel_direction = -1;
    sort_add_groups();
    EXPECT_THAT(g_add_groups.indices(), testing::ElementsAre(b, z, a));
    EXPECT_EQ(z, g_add_groups.next(b));
    EXPECT_EQ(b, g_add_groups.prev(z));
    g_sel_sort = SS_STRING;
    sort_add_groups();
    EXPECT_THAT(g_add_groups.indices(), testing::ElementsAre(z, b, a));
    g_sel_sort = SS_NATURAL;
    sort_add_groups();
    EXPECT_THAT(g_add_groups.indices(), testing::ElementsAre(b, a, z));
}

TEST_F(AddGroupStorageTest, clearDisposesGroupsAndDuplicateLookupState)
{
    add_to_list(std::string(256, 'a'), 3, ':');
    add_to_list(std::string(256, 'a'), 9, '!');
    ASSERT_EQ(1U, g_add_groups.size());

    g_add_groups.clear();

    EXPECT_TRUE(g_add_groups.empty());
    EXPECT_EQ(AddGroupIndex::None, g_add_groups.first());
    const AddGroupIndex replacement = add_to_list(std::string(256, 'a'), 9, '!');
    ASSERT_EQ(1U, g_add_groups.size());
    EXPECT_EQ(9, g_add_groups[replacement].m_to_read.value_of());
    EXPECT_EQ(AGF_DEL, g_add_groups[replacement].m_flags);
}

TEST_F(ScanActiveTest, reportsCachedActiveFileGroups)
{
    add_active_source("comp.lang.apl 0000000042 0000000007 y\n");

    testing::internal::CaptureStdout();
    const bool changed = scan_active(false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(changed);
    EXPECT_NE(std::string::npos, output.find("Unsubscribed newsgroups:\n"));
    EXPECT_NE(std::string::npos, output.find("comp.lang.apl\n"));
}

TEST_F(ScanActiveTest, showsUnsubscribedResultsInSortableSelector)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");
    g_use_add_selector = true;
    push_string("q", 0200);
    errno = 0;

    testing::internal::CaptureStdout();
    const bool        changed = scan_active(false);
    const std::string output = testing::internal::GetCapturedStdout();
    while (macro_pending())
    {
        (void) read_tty_char();
    }

    EXPECT_FALSE(changed);
    EXPECT_NE(std::string::npos, output.find("Unsubscribed newsgroups"));
    EXPECT_NE(std::string::npos, output.find("natural order"));
    EXPECT_NE(std::string::npos, output.find("comp.lang.apl"));
    EXPECT_NE(std::string::npos, output.find("news.test"));
    EXPECT_TRUE(g_add_groups.empty());
}

TEST_F(ScanActiveTest, reportsRemoteActiveGroups)
{
    add_uncached_remote_source();

    testing::InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST active *"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list of newsgroups follows"))
        .WillOnce(testing::Return("comp.lang.apl 42 7 y"))
        .WillOnce(testing::Return("comp.lang.c++ 200 100 x"))
        .WillOnce(testing::Return("."));

    testing::internal::CaptureStdout();
    const bool        changed = scan_active(false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(changed);
    EXPECT_NE(std::string::npos, output.find("Unsubscribed newsgroups:\n"));
    EXPECT_NE(std::string::npos, output.find("comp.lang.apl\n"));
    EXPECT_EQ(std::string::npos, output.find("comp.lang.c++\n"));
}

TEST_F(ScanActiveTest, remoteNewGroupsAppendExcludedGroupToActiveFile)
{
    add_remote_active_source("");
    DataSource &source = g_data_sources.back();

    testing::InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("DATE"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_)).WillOnce(testing::Return("111 19700102000000"));
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("NEWGROUPS 700101 000000 GMT"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("231 list of new newsgroups follows"))
        .WillOnce(testing::Return("comp.lang.apl 42 7 x"))
        .WillOnce(testing::Return("."));

    const bool changed = find_new_groups();

    EXPECT_FALSE(changed);
    ASSERT_EQ(1U, source.m_act_sf.m_lines.size());
    EXPECT_EQ("comp.lang.apl 0000000042 00007 x\n", source.m_act_sf.m_lines[0]);
    EXPECT_EQ(86400L, source.m_last_new_group);
}

TEST_F(ScanActiveTest, localNewGroupsIgnoreExcludedActiveGroup)
{
    add_local_active_source("comp.lang.apl 42 7 x\n", "comp.lang.apl 200\n");
    DataSource &source = g_data_sources.back();
    const long  active_times_size = static_cast<long>(fs::file_size(source.m_extra_name));

    const bool changed = find_new_groups();

    EXPECT_FALSE(changed);
    EXPECT_TRUE(g_add_groups.empty());
    EXPECT_EQ(201L, source.m_last_new_group);
    EXPECT_EQ(active_times_size, source.m_act_sf.m_recent_cnt);
}

TEST_F(ScanActiveTest, remoteNewGroupsDiscardDuplicateCandidates)
{
    add_remote_active_source("");
    DataSource         &source = g_data_sources.back();
    testing::InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("DATE"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_)).WillOnce(testing::Return("111 19700102000000"));
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("NEWGROUPS 700101 000000 GMT"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("231 list of new newsgroups follows"))
        .WillOnce(testing::Return("comp.lang.apl 42 7 y"))
        .WillOnce(testing::Return("comp.lang.apl 99 7 y"))
        .WillOnce(testing::Return("news.test 3 1 y"))
        .WillOnce(testing::Return("."));

    new_nntp_groups(&source);

    ASSERT_EQ(2U, g_add_groups.size());
    const AddGroupIndex apl = g_add_groups.first();
    EXPECT_EQ("comp.lang.apl", g_add_groups[apl].m_name);
    EXPECT_EQ(&source, g_add_groups[apl].m_data_src);
    EXPECT_EQ(35, g_add_groups[apl].m_to_read.value_of());
    EXPECT_EQ(apl, add_to_list("comp.lang.apl", 0, '!'));
}

TEST_F(ScanActiveTest, localNewGroupsDiscardDuplicatesAcrossSources)
{
    add_local_active_source("comp.lang.apl 42 7 y\n", "comp.lang.apl 200\ncomp.lang.apl 201\n");
    new_local_groups(&g_data_sources.back());
    ASSERT_EQ(1U, g_add_groups.size());
    const AddGroupIndex apl = g_add_groups.first();

    add_local_active_source("comp.lang.apl 99 7 y\n", "comp.lang.apl 202\n");
    new_local_groups(&g_data_sources.back());

    ASSERT_EQ(1U, g_add_groups.size());
    EXPECT_EQ(apl, g_add_groups.first());
    EXPECT_EQ(35, g_add_groups[apl].m_to_read.value_of());
}

TEST_F(SelectorGroupListingTest, sortsResultsByGroupName)
{
    add_active_source("test.z 7 1 y\ntest.a 2 1 y\ntest.m 4 1 y\n");

    const std::string output = last_display(run_listing("Ogq"));

    EXPECT_LT(output.find("test.a"), output.find("test.m"));
    EXPECT_LT(output.find("test.m"), output.find("test.z"));
    EXPECT_NE(std::string::npos, output.find("group name order"));
}

TEST_F(SelectorGroupListingTest, sortsResultsByArticleCount)
{
    add_active_source("test.z 7 1 y\ntest.a 2 1 y\ntest.m 4 1 y\n");

    const std::string output = last_display(run_listing("Ocq"));

    EXPECT_LT(output.find("test.a"), output.find("test.m"));
    EXPECT_LT(output.find("test.m"), output.find("test.z"));
    EXPECT_NE(std::string::npos, output.find("count order"));
}

TEST_F(SelectorGroupListingTest, reversesCurrentOrder)
{
    add_active_source("test.z 7 1 y\ntest.a 2 1 y\ntest.m 4 1 y\n");

    const std::string output = last_display(run_listing("OgRq"));

    EXPECT_LT(output.find("test.z"), output.find("test.m"));
    EXPECT_LT(output.find("test.m"), output.find("test.a"));
    EXPECT_NE(std::string::npos, output.find("reverse group name order"));
}

TEST_F(SelectorGroupListingTest, nextPageFollowsSortedIndices)
{
    add_seven_groups();

    const std::string output = last_display(run_listing("Og>q"));

    EXPECT_LT(output.find("test.d"), output.find("test.e"));
    EXPECT_LT(output.find("test.e"), output.find("test.f"));
    EXPECT_EQ(std::string::npos, output.find("test.a"));
    EXPECT_EQ(std::string::npos, output.find("test.g"));
}

TEST_F(SelectorGroupListingTest, previousPageFollowsSortedIndices)
{
    add_seven_groups();

    const std::string output = last_display(run_listing("Og><q"));

    EXPECT_LT(output.find("test.a"), output.find("test.b"));
    EXPECT_LT(output.find("test.b"), output.find("test.c"));
    EXPECT_EQ(std::string::npos, output.find("test.d"));
}

TEST_F(SelectorGroupListingTest, lastPageFillsFromEndOfSortedIndices)
{
    add_seven_groups();

    const std::string output = last_display(run_listing("Og$q"));

    EXPECT_LT(output.find("test.e"), output.find("test.f"));
    EXPECT_LT(output.find("test.f"), output.find("test.g"));
    EXPECT_EQ(std::string::npos, output.find("test.d"));
}

TEST_F(SelectorGroupListingTest, changingOrderResetsLaterPageToFirstMatch)
{
    add_seven_groups();

    const std::string output = last_display(run_listing("Og>Ocq"));

    EXPECT_LT(output.find("test.g"), output.find("test.f"));
    EXPECT_LT(output.find("test.f"), output.find("test.e"));
    EXPECT_EQ(std::string::npos, output.find("test.d"));
}

TEST_F(SelectorGroupListingTest, narrowsSourceGlob)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");

    const std::string output = last_display(run_listing("lnews.*\nq"));

    EXPECT_NE(std::string::npos, output.find("news.test"));
    EXPECT_EQ(std::string::npos, output.find("comp.lang.apl")) << output;
}

TEST_F(SelectorGroupListingTest, broadensSourceGlobByRescanning)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");
    set_newsgroup_to_do("comp.*");

    const std::string output = last_display(run_listing("l*\nq"));

    EXPECT_NE(std::string::npos, output.find("comp.lang.apl"));
    EXPECT_NE(std::string::npos, output.find("news.test"));
}

TEST_F(SelectorGroupListingTest, emptyResultsAllowAnotherGlob)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");

    const std::string output = run_listing("lmissing.*\nlnews.*\nq");

    EXPECT_NE(std::string::npos, output.find("0 groups"));
    EXPECT_NE(std::string::npos, last_display(output).find("news.test"));
    EXPECT_EQ(std::string::npos, last_display(output).find("comp.lang.apl"));
}

TEST_F(SelectorGroupListingTest, invalidGlobKeepsCurrentResults)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");

    const std::string output = last_display(run_listing("l[\nq"));

    EXPECT_NE(std::string::npos, output.find("comp.lang.apl"));
    EXPECT_NE(std::string::npos, output.find("news.test"));
    EXPECT_EQ(0, g_max_newsgroup_to_do);
}

TEST_F(SelectorGroupListingTest, returnLeavesListingWithoutSubscribing)
{
    add_active_source("comp.lang.apl 42 7 y\n");

    const std::string output = run_listing("\n");

    EXPECT_NE(std::string::npos, output.find("Unsubscribed newsgroups"));
    EXPECT_NE(std::string::npos, output.find("comp.lang.apl"));
}

TEST_F(SelectorGroupListingTest, remoteGlobRefinementRescansServer)
{
    add_uncached_remote_source();
    testing::InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST active *"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list of newsgroups follows"))
        .WillOnce(testing::Return("comp.lang.apl 42 7 y"))
        .WillOnce(testing::Return("."));
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST active *news.**"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list of newsgroups follows"))
        .WillOnce(testing::Return("news.test 3 1 y"))
        .WillOnce(testing::Return("comp.lang.apl 42 7 y"))
        .WillOnce(testing::Return("."));

    const std::string output = last_display(run_listing("lnews.*\nq"));

    EXPECT_NE(std::string::npos, output.find("news.test"));
    EXPECT_EQ(std::string::npos, output.find("comp.lang.apl"));
}

TEST_F(SelectorGroupListingTest, mergesDuplicateGroupsFromMultipleSources)
{
    add_active_source("comp.lang.apl 42 7 y\n");
    add_active_source("comp.lang.apl 99 7 y\nnews.test 3 1 y\n");

    const std::string output = last_display(run_listing("q"));
    const std::size_t apl = output.find("comp.lang.apl");

    ASSERT_NE(std::string::npos, apl);
    EXPECT_EQ(std::string::npos, output.find("comp.lang.apl", apl + 1));
    EXPECT_NE(std::string::npos, output.find("2 groups"));
}

TEST_F(SelectorGroupListingTest, includesUnsubscribedNewsrcEntriesAndOmitsSubscribedGroups)
{
    add_active_source("comp.subscribed 7 1 y\ncomp.unsubscribed 4 1 y\nnews.test 3 1 y\n");
    hash_destroy(g_newsrc_hash);
    g_newsrc_hash = hash_create(17, [](std::string_view key, HashDatum data)
                                { return key.compare(reinterpret_cast<NewsgroupData *>(data.dat_ptr)->m_rc_line); });
    NewsgroupData subscribed{};
    subscribed.m_rc_line = "comp.subscribed";
    subscribed.m_to_read = TR_ONE;
    NewsgroupData unsubscribed{};
    unsubscribed.m_rc_line = "comp.unsubscribed";
    unsubscribed.m_to_read = TR_UNSUB;
    hash_store(g_newsrc_hash, subscribed.m_rc_line, {reinterpret_cast<char *>(&subscribed), 0});
    hash_store(g_newsrc_hash, unsubscribed.m_rc_line, {reinterpret_cast<char *>(&unsubscribed), 0});

    const std::string output = last_display(run_listing("Ogq"));

    EXPECT_EQ(std::string::npos, output.find("comp.subscribed"));
    EXPECT_NE(std::string::npos, output.find("comp.unsubscribed"));
    EXPECT_NE(std::string::npos, output.find("news.test"));
    EXPECT_EQ(TR_ONE, subscribed.m_to_read);
    EXPECT_EQ(TR_UNSUB, unsubscribed.m_to_read);
}

TEST_F(SelectorGroupListingTest, helpExplainsSortingAndGlobRefinement)
{
    add_active_source("news.test 3 1 y\n");
    g_tc_LINES = 100;

    const std::string output = run_listing("Hqq");

    EXPECT_NE(std::string::npos, output.find("l glob"));
    EXPECT_NE(std::string::npos, output.find("Reverse the current sort order"));
    EXPECT_EQ(std::string::npos, output.find("NOT WORKING YET"));
}

TEST_F(SelectorGroupListingTest, globCommandWorksWithAlphabeticSelectionKeys)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");
    g_use_sel_num = false;

    const std::string output = last_display(run_listing("lnews.*\nq"));

    EXPECT_NE(std::string::npos, output.find("news.test"));
    EXPECT_EQ(std::string::npos, output.find("comp.lang.apl"));
}

TEST_F(SelectorGroupListingTest, initiallyEmptyResultsAllowAnotherGlob)
{
    add_active_source("news.test 3 1 y\n");
    set_newsgroup_to_do("missing.*");

    const std::string output = run_listing("lnews.*\nq");

    EXPECT_NE(std::string::npos, output.find("0 groups"));
    EXPECT_NE(std::string::npos, last_display(output).find("news.test"));
}

TEST_F(SelectorGroupListingTest, globRefinementPreservesUnreadThreshold)
{
    add_active_source("comp.lang.apl 42 7 y\nnews.test 3 1 y\n");
    set_newsgroup_to_do("comp.*");
    g_newsgroup_min_to_read = TR_NONE;

    (void) run_listing("lnews.*\nq");

    EXPECT_EQ(TR_NONE, g_newsgroup_min_to_read);
}
