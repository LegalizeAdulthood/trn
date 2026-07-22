// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/addng-internal.h>

#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/only.h>
#include <trn/rcstuff.h>
#include <trn/terminal.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <test_config.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class MockNNTPConnection : public INNTPConnection
{
public:
    ~MockNNTPConnection() override = default;

    MOCK_METHOD(std::string, read_line, (error_code_t &), (override));
    MOCK_METHOD(void, write_line, (const std::string &, error_code_t &), (override));
    MOCK_METHOD(void, write, (const char *, std::size_t, error_code_t &), (override));
    MOCK_METHOD(std::size_t, read, (char *, std::size_t, error_code_t &), (override));
};

class ActiveListPatternTest : public testing::Test
{
protected:
    void SetUp() override
    {
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
        m_old_first = g_first_add_group;
        m_old_last = g_last_add_group;
        m_old_data_source = g_data_source;
        g_first_add_group = nullptr;
        g_last_add_group = nullptr;
        g_data_source = nullptr;
    }

    void TearDown() override
    {
        for (AddGroup *group = g_first_add_group; group != nullptr;)
        {
            AddGroup *next = group->m_next;
            delete group;
            group = next;
        }
        g_first_add_group = m_old_first;
        g_last_add_group = m_old_last;
        g_data_source = m_old_data_source;
    }

    AddGroup   *m_old_first{};
    AddGroup   *m_old_last{};
    DataSource *m_old_data_source{};
};

class ScanActiveTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_saved_data_sources.swap(g_data_sources);
        m_old_data_source = g_data_source;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_multirc = g_multirc;
        m_old_nntp_link = g_nntp_link;
        m_old_quick_start = g_quick_start;
        m_old_use_add_selector = g_use_add_selector;
        m_old_first = g_first_add_group;
        m_old_last = g_last_add_group;
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

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_data_sources.clear();
        g_data_source = nullptr;
        g_newsrc_hash = hash_create(17, nullptr);
        nntp_gets_clear_buffer();
        m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
        g_first_add_group = nullptr;
        g_last_add_group = nullptr;
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
        g_first_add_group = m_old_first;
        g_last_add_group = m_old_last;
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

    fs::path                m_output_dir;
    std::vector<DataSource> m_saved_data_sources;
    DataSource             *m_old_data_source{};
    HashTable              *m_old_newsrc_hash{};
    Multirc                *m_old_multirc{};
    NNTPLink                m_old_nntp_link{};
    bool                    m_old_quick_start{};
    bool                    m_old_use_add_selector{};
    AddGroup               *m_old_first{};
    AddGroup               *m_old_last{};
    char                    m_old_int_count{};
    bool                    m_old_erase_screen{};
    bool                    m_old_tc_am{};
    int                     m_old_tc_lines{};
    int                     m_old_tc_cols{};
    int                     m_old_page_line{};
    int                     m_old_term_line{};
    int                     m_old_term_col{};
    const char             *m_old_tc_so{};
    const char             *m_old_tc_se{};
    Newsrc                  m_newsrc{};
    Multirc                 m_multirc{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
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

TEST_F(AddGroupStorageTest, storesNamesAndLinksWithoutAddingDuplicates)
{
    add_to_list("comp.lang.c++", 42, ':');
    add_to_list("news.software.readers", -1, '!');
    add_to_list("comp.lang.c++", 99, '!');

    ASSERT_NE(nullptr, g_first_add_group);
    ASSERT_NE(nullptr, g_last_add_group);
    EXPECT_EQ(g_last_add_group, g_first_add_group->m_next);
    EXPECT_EQ(g_first_add_group, g_last_add_group->m_prev);
    EXPECT_EQ(nullptr, g_first_add_group->m_prev);
    EXPECT_EQ(nullptr, g_last_add_group->m_next);
    EXPECT_EQ("comp.lang.c++", std::string_view{g_first_add_group->m_name});
    EXPECT_EQ("news.software.readers", std::string_view{g_last_add_group->m_name});
    EXPECT_EQ(42, g_first_add_group->m_to_read.value_of());
    EXPECT_EQ(0, g_last_add_group->m_to_read.value_of());
    EXPECT_EQ(AGF_SEL, g_first_add_group->m_flags);
    EXPECT_EQ(AGF_DEL, g_last_add_group->m_flags);
}

TEST_F(ScanActiveTest, reportsCachedActiveFileGroups)
{
    add_active_source("comp.lang.apl 0000000042 0000000007 y\n");

    testing::internal::CaptureStdout();
    const bool changed = scan_active(false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(changed);
    EXPECT_NE(std::string::npos, output.find("Completely unsubscribed newsgroups:\n"));
    EXPECT_NE(std::string::npos, output.find("comp.lang.apl\n"));
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
