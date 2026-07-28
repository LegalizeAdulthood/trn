// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/datasrc.h>

#include <config/common.h>
#include <trn/hash.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <util/env.h>

#include <test_config.h>

#include "mock_env.h"
#include "MockNNTPConnection.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class SourceFileOwner
{
public:
    SourceFileOwner()
    {
        m_source.m_hp = hash_create(17, nullptr);
    }

    SourceFileOwner(const SourceFileOwner &) = delete;
    SourceFileOwner &operator=(const SourceFileOwner &) = delete;

    ~SourceFileOwner()
    {
        m_source.close();
    }

    SourceFile &get()
    {
        return m_source;
    }

private:
    SourceFile m_source{};
};

class SourceFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    fs::path m_output_dir;
};

class DataSourceFindActiveGroupTest : public SourceFileTest
{
protected:
    void SetUp() override
    {
        SourceFileTest::SetUp();
        m_old_data_source = g_data_source;
        m_old_nntp_link = g_nntp_link;
        g_data_source = nullptr;
        nntp_gets_clear_buffer();
        m_source_path = m_output_dir / "active";
        std::ofstream{m_source_path} << "comp.lang.apl 0000000001 0000000001 y\n";
        ASSERT_EQ(1, m_data_source.m_act_sf.open(m_source_path, "", ""));
    }

    void TearDown() override
    {
        m_data_source.m_act_sf.close();
        g_data_source = m_old_data_source;
        g_nntp_link = m_old_nntp_link;
        nntp_gets_clear_buffer();
        SourceFileTest::TearDown();
    }

    fs::path                                                 m_source_path;
    DataSource                                               m_data_source{};
    DataSource                                              *m_old_data_source{};
    NNTPLink                                                 m_old_nntp_link{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
};

class SourceFileRemoteOpenTest : public SourceFileTest
{
protected:
    void SetUp() override
    {
        SourceFileTest::SetUp();
        m_old_nntp_link = g_nntp_link;
        m_old_net_speed = g_net_speed;
        nntp_gets_clear_buffer();

        m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
        g_nntp_link.connection = m_connection;
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
        g_net_speed = 1;
    }

    void TearDown() override
    {
        g_nntp_link.connection.reset();
        g_nntp_link = m_old_nntp_link;
        g_net_speed = m_old_net_speed;
        nntp_gets_clear_buffer();
        SourceFileTest::TearDown();
    }

    NNTPLink                                                 m_old_nntp_link{};
    int                                                      m_old_net_speed{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
};

class DataSourceFindGroupDescTest : public SourceFileTest
{
protected:
    void SetUp() override
    {
        SourceFileTest::SetUp();
        m_old_data_source = g_data_source;
        m_old_nntp_link = g_nntp_link;
        m_old_net_speed = g_net_speed;
        g_data_source = nullptr;
        nntp_gets_clear_buffer();

        m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
        m_data_source.m_flags = DF_REMOTE | DF_TMP_GROUP_DESC;
        m_data_source.m_group_desc = (m_output_dir / "newsgroups").generic_string();
        m_data_source.m_desc_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
        m_data_source.m_nntp_link.connection = m_connection;
        m_data_source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
        g_net_speed = 1;
    }

    void TearDown() override
    {
        m_data_source.m_desc_sf.close();
        g_data_source = m_old_data_source;
        g_nntp_link = m_old_nntp_link;
        g_net_speed = m_old_net_speed;
        nntp_gets_clear_buffer();
        SourceFileTest::TearDown();
    }

    DataSource                                               m_data_source{};
    DataSource                                              *m_old_data_source{};
    NNTPLink                                                 m_old_nntp_link{};
    int                                                      m_old_net_speed{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
};

class DataSourceOpenTest : public SourceFileTest
{
protected:
    void SetUp() override
    {
        SourceFileTest::SetUp();
        m_old_data_source = g_data_source;
        m_old_nntp_link = g_nntp_link;
        m_old_net_speed = g_net_speed;
        m_old_nntp_allow_timeout = g_nntp_allow_timeout;
        g_data_source = nullptr;
        nntp_gets_clear_buffer();

        m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
        g_net_speed = 1;

        m_data_source.m_flags = DF_REMOTE;
        m_data_source.m_news_id = "news.example";
        m_data_source.m_extra_name = (m_output_dir / "active").generic_string();
        m_data_source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
        m_data_source.m_nntp_link.connection = m_connection;
        m_data_source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
    }

    void TearDown() override
    {
        g_nntp_link.connection.reset();
        m_data_source.m_nntp_link.connection.reset();
        m_data_source.close();
        g_data_source = m_old_data_source;
        g_nntp_link = m_old_nntp_link;
        g_net_speed = m_old_net_speed;
        g_nntp_allow_timeout = m_old_nntp_allow_timeout;
        nntp_gets_clear_buffer();
        SourceFileTest::TearDown();
    }

    DataSource                                               m_data_source{};
    DataSource                                              *m_old_data_source{};
    NNTPLink                                                 m_old_nntp_link{};
    int                                                      m_old_net_speed{};
    bool                                                     m_old_nntp_allow_timeout{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
};

class FindCloseMatchTest : public SourceFileTest
{
protected:
    void SetUp() override
    {
        SourceFileTest::SetUp();
        m_saved_data_sources.swap(g_data_sources);
        m_old_data_source = g_data_source;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_newsgroup_dir = g_newsgroup_dir;
        m_old_verbose = g_verbose;
        g_data_source = nullptr;
        g_verbose = false;
        g_data_sources.reserve(2);
    }

    void TearDown() override
    {
        for (DataSource &source : g_data_sources)
        {
            source.m_act_sf.close();
        }
        g_data_sources.clear();
        m_saved_data_sources.swap(g_data_sources);
        g_data_source = m_old_data_source;
        g_newsgroup_name = m_old_newsgroup_name;
        g_newsgroup_dir = m_old_newsgroup_dir;
        g_verbose = m_old_verbose;
        SourceFileTest::TearDown();
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

    std::vector<DataSource> m_saved_data_sources;
    DataSource             *m_old_data_source{};
    std::string             m_old_newsgroup_name;
    std::string             m_old_newsgroup_dir;
    bool                    m_old_verbose{};
};

class DataSourceInitTest : public SourceFileTest
{
protected:
    void SetUp() override
    {
        SourceFileTest::SetUp();
        m_saved_data_sources.swap(g_data_sources);
        m_old_data_source = g_data_source;
        m_old_dot_dir = g_dot_dir;
        m_old_trn_dir = g_trn_dir;
        m_old_rn_lib = g_rn_lib;
        m_old_tmp_dir = g_tmp_dir;
        m_old_trn_access_text = g_trn_access_text;
        m_old_nntp_auth_file = g_nntp_auth_file;
        m_old_def_refetch_secs = g_def_refetch_secs;
        g_data_source = nullptr;
        g_dot_dir = m_output_dir.generic_string();
        g_trn_dir = m_output_dir.generic_string();
        g_rn_lib = m_output_dir.generic_string();
        g_tmp_dir = m_output_dir.generic_string();
        g_trn_access_text.clear();
        g_nntp_auth_file.clear();
        g_def_refetch_secs = DEFAULT_REFETCH_SECS;
    }

    void TearDown() override
    {
        data_source_finalize();
        g_data_sources.clear();
        m_saved_data_sources.swap(g_data_sources);
        g_data_source = m_old_data_source;
        g_dot_dir = m_old_dot_dir;
        g_trn_dir = m_old_trn_dir;
        g_rn_lib = m_old_rn_lib;
        g_tmp_dir = m_old_tmp_dir;
        g_trn_access_text = m_old_trn_access_text;
        g_nntp_auth_file = m_old_nntp_auth_file;
        g_def_refetch_secs = m_old_def_refetch_secs;
        SourceFileTest::TearDown();
    }

    trn::testing::MockEnvironment m_env;
    std::vector<DataSource>       m_saved_data_sources;
    DataSource                   *m_old_data_source{};
    std::string                   m_old_dot_dir;
    std::string                   m_old_trn_dir;
    std::string                   m_old_rn_lib;
    std::string                   m_old_tmp_dir;
    std::string                   m_old_trn_access_text;
    std::string                   m_old_nntp_auth_file;
    std::time_t                   m_old_def_refetch_secs{};
};

} // namespace

TEST(SourceFileAppendTest, storesNormalizedLineAndReturnsStoredStorage)
{
    SourceFileOwner            source_file_owner;
    SourceFile                &source_file = source_file_owner.get();
    constexpr std::string_view line{"comp.lang.c++     C++ language discussion"};
    const int                  key_len = static_cast<int>(line.find(' '));

    const std::string_view stored_line = source_file.append(line, key_len);

    ASSERT_EQ(1U, source_file.m_lines.size());
    EXPECT_EQ(source_file.m_lines.back().data(), stored_line.data());
    EXPECT_EQ("comp.lang.c++ C++ language discussion\n", stored_line);
    EXPECT_EQ("comp.lang.c++ C++ language discussion\n", source_file.m_lines.back());
    EXPECT_EQ("C++ language discussion\n", std::string_view{stored_line.data() + key_len + 1});
}

TEST(SourceFileAppendTest, normalizesGreySpaceInDescription)
{
    SourceFileOwner            source_file_owner;
    SourceFile                &source_file = source_file_owner.get();
    constexpr std::string_view line{"comp.lang.apl APL\tdiscussion"};
    const int                  key_len = static_cast<int>(line.find(' '));

    const std::string_view stored_line = source_file.append(line, key_len);

    ASSERT_EQ(1U, source_file.m_lines.size());
    EXPECT_EQ("comp.lang.apl APL discussion\n", stored_line);
    EXPECT_EQ("comp.lang.apl APL discussion\n", source_file.m_lines.back());
}

TEST_F(SourceFileTest, openReadsLinesFromLocalFile)
{
    const fs::path source_path = m_output_dir / "active";
    std::ofstream{source_path} << "comp.lang.apl 0000000001 0000000001 y\n"
                               << "comp.lang.c++ 0000000002 0000000001 y\n";
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();

    const int result = source_file.open(source_path, "", "");

    ASSERT_EQ(1, result);
    ASSERT_EQ(2U, source_file.m_lines.size());
    EXPECT_EQ("comp.lang.apl 0000000001 0000000001 y\n", source_file.m_lines[0]);
    EXPECT_EQ("comp.lang.c++ 0000000002 0000000001 y\n", source_file.m_lines[1]);
    EXPECT_EQ(0L, source_file.m_line_positions[0]);
}

TEST_F(SourceFileTest, openNormalizesUnterminatedLocalLine)
{
    const fs::path source_path = m_output_dir / "newsgroups";
    std::ofstream{source_path} << "comp.lang.apl     APL discussion";
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();

    const int result = source_file.open(source_path, "", "");

    ASSERT_EQ(1, result);
    ASSERT_EQ(1U, source_file.m_lines.size());
    EXPECT_EQ("comp.lang.apl APL discussion\n", source_file.m_lines[0]);
    EXPECT_EQ(0L, source_file.m_line_positions[0]);
}

TEST_F(SourceFileTest, openPreservesLongLocalLine)
{
    const fs::path    source_path = m_output_dir / "newsgroups";
    const std::string description(LINE_BUF_LEN, 'x');
    std::ofstream{source_path} << "comp.lang.apl " << description << '\n';
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();

    const int result = source_file.open(source_path, "", "");

    ASSERT_EQ(1, result);
    ASSERT_EQ(1U, source_file.m_lines.size());
    EXPECT_EQ("comp.lang.apl " + description + '\n', source_file.m_lines[0]);
    EXPECT_EQ(0L, source_file.m_line_positions[0]);
}

TEST_F(SourceFileRemoteOpenTest, openFetchesRemoteLinesFromServer)
{
    const fs::path  source_path = m_output_dir / "active";
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();
    source_file.m_refetch_secs = DEFAULT_REFETCH_SECS;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return("comp.lang.apl 10 1 y"))
        .WillOnce(testing::Return("comp.lang.c++ 20 1 y"))
        .WillOnce(testing::Return("."));

    testing::internal::CaptureStdout();
    const int result = source_file.open(source_path, "active", "news.example");
    (void) testing::internal::GetCapturedStdout();

    EXPECT_EQ(2, result);
    ASSERT_EQ(2U, source_file.m_lines.size());
    EXPECT_EQ("comp.lang.apl 10 1 y\n", source_file.m_lines[0]);
    EXPECT_EQ("comp.lang.c++ 20 1 y\n", source_file.m_lines[1]);
}

TEST_F(SourceFileTest, endAppendUpdatesCachedFileTimestamp)
{
    const fs::path source_path = m_output_dir / "active";
    std::ofstream{source_path} << "comp.lang.apl 0000000001 0000000001 y\n";
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();
    source_file.m_fp = std::fopen(source_path.string().c_str(), "r+");
    ASSERT_NE(nullptr, source_file.m_fp);
    source_file.m_refetch_secs = DEFAULT_REFETCH_SECS;
    source_file.m_last_fetch = 946684800;

    source_file.end_append(source_path);

    struct stat file_stat;
    ASSERT_EQ(0, stat(source_path.string().c_str(), &file_stat));
    EXPECT_EQ(source_file.m_last_fetch, file_stat.st_mtime);
}

TEST_F(SourceFileTest, closeRemovesTemporaryRemoteFiles)
{
    const fs::path active_path = m_output_dir / "active";
    const fs::path group_desc_path = m_output_dir / "newsgroups";
    std::ofstream{active_path} << "comp.lang.apl 0000000001 0000000001 y\n";
    std::ofstream{group_desc_path} << "comp.lang.apl APL discussion\n";
    DataSource data_source{};
    data_source.m_flags = DF_REMOTE | DF_TMP_ACTIVE_FILE | DF_TMP_GROUP_DESC;
    data_source.m_extra_name = active_path.generic_string();
    data_source.m_group_desc = group_desc_path.generic_string();

    data_source.close();

    EXPECT_FALSE(fs::exists(active_path));
    EXPECT_FALSE(fs::exists(group_desc_path));
}

TEST_F(SourceFileTest, findGroupDescClearsMissingTemporaryGroupDescription)
{
    const fs::path group_desc_path = m_output_dir / "missing-newsgroups";
    DataSource     data_source{};
    data_source.m_flags = DF_TMP_GROUP_DESC;
    data_source.m_group_desc = group_desc_path.generic_string();

    EXPECT_TRUE(data_source.find_group_desc("comp.lang.apl").empty());

    EXPECT_TRUE(data_source.m_group_desc.empty());
    EXPECT_FALSE(data_source.m_flags & DF_TMP_GROUP_DESC);
}

TEST_F(DataSourceFindActiveGroupTest, returnsCachedActiveLine)
{
    m_data_source.m_flags = DF_REMOTE;
    m_data_source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;

    const std::string active_line = m_data_source.find_active_group("comp.lang.apl", ArticleNum{});

    EXPECT_EQ("comp.lang.apl 0000000001 0000000001 y\n", active_line);
}

TEST_F(DataSourceFindActiveGroupTest, returnsEmptyForMissingGroup)
{
    const std::string active_line = m_data_source.find_active_group("comp.lang.c++", ArticleNum{});

    EXPECT_TRUE(active_line.empty());
}

TEST_F(DataSourceFindActiveGroupTest, updatesCachedHighWaterMark)
{
    std::fclose(m_data_source.m_act_sf.m_fp);
    m_data_source.m_act_sf.m_fp = std::fopen(m_source_path.string().c_str(), "r+");
    ASSERT_NE(nullptr, m_data_source.m_act_sf.m_fp);
    m_data_source.m_flags = DF_REMOTE;
    m_data_source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;

    const std::string active_line = m_data_source.find_active_group("comp.lang.apl", ArticleNum{42});

    EXPECT_EQ("comp.lang.apl 0000000042 0000000001 y\n", active_line);
    EXPECT_EQ("comp.lang.apl 0000000042 0000000001 y\n", m_data_source.m_act_sf.m_lines[0]);
}

TEST_F(DataSourceFindActiveGroupTest, fetchesActiveLineFromServerList)
{
    std::fclose(m_data_source.m_act_sf.m_fp);
    m_data_source.m_act_sf.m_fp = std::fopen(m_source_path.string().c_str(), "r+");
    ASSERT_NE(nullptr, m_data_source.m_act_sf.m_fp);
    m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    m_data_source.m_flags = DF_REMOTE | DF_USE_LIST_ACTIVE;
    m_data_source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
    m_data_source.m_nntp_link.connection = m_connection;
    m_data_source.m_nntp_link.flags = NNTP_NEW_CMD_OK;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST active comp.lang.apl"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return("comp.lang.apl 0000000042 0000000007 y"))
        .WillOnce(testing::Return("."));

    const std::string active_line = m_data_source.find_active_group("comp.lang.apl", ArticleNum{});

    EXPECT_EQ("comp.lang.apl 0000000042 0000000007 y\n", active_line);
    EXPECT_EQ("comp.lang.apl 0000000042 0000000007 y\n", m_data_source.m_act_sf.m_lines[0]);
}

TEST_F(DataSourceFindGroupDescTest, fetchesDescriptionFromServer)
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("XGTITLE comp.lang.apl"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("282 list follows"))
        .WillOnce(testing::Return("comp.lang.apl APL discussion"))
        .WillOnce(testing::Return("."));

    const std::string_view description = m_data_source.find_group_desc("comp.lang.apl");

    EXPECT_EQ("APL discussion\n", description);
}

TEST_F(DataSourceFindGroupDescTest, returnsCachedDescription)
{
    const std::string_view group_name{"comp.lang.apl"};
    ASSERT_EQ(1, m_data_source.m_desc_sf.open({}, "", ""));
    (void) m_data_source.m_desc_sf.append("comp.lang.apl APL discussion\n", static_cast<int>(group_name.size()));

    const std::string_view description = m_data_source.find_group_desc(group_name);

    EXPECT_EQ("APL discussion\n", description);
}

TEST_F(DataSourceFindGroupDescTest, storesEmptyDescriptionForEmptyServerList)
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("XGTITLE comp.lang.apl"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("282 list follows"))
        .WillOnce(testing::Return("."));

    const std::string_view description = m_data_source.find_group_desc("comp.lang.apl");

    EXPECT_EQ("\n\n", description);
}

TEST_F(DataSourceOpenTest, importsActiveListWhenServerIgnoresControlPattern)
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST active control"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return("comp.lang.apl 10 1 y"))
        .WillOnce(testing::Return("comp.lang.c++ 20 1 y"))
        .WillOnce(testing::Return("."));

    testing::internal::CaptureStdout();
    const bool result = m_data_source.open();
    (void) testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_TRUE(m_data_source.m_flags & DF_OPEN);
    ASSERT_EQ(2U, m_data_source.m_act_sf.m_lines.size());
    EXPECT_EQ("comp.lang.apl 10 1 y\n", m_data_source.m_act_sf.m_lines[0]);
    EXPECT_EQ("comp.lang.c++ 20 1 y\n", m_data_source.m_act_sf.m_lines[1]);
}

TEST_F(DataSourceInitTest, createsDefaultRemoteSourceFromNntpServer)
{
    m_env.expect_env("NNTPSERVER", "news.example");
    m_env.expect_no_envar("NNTP_FORCE_AUTH");

    data_source_init();

    ASSERT_EQ(1U, g_data_sources.size());
    const DataSource &source = g_data_sources.front();
    EXPECT_EQ("default", source.m_name);
    EXPECT_EQ("news.example", source.m_news_id);
    EXPECT_TRUE(source.m_flags & DF_DEFAULT);
    EXPECT_TRUE(source.m_flags & DF_REMOTE);
}

TEST_F(DataSourceInitTest, readsForceAuthFromEnvironment)
{
    m_env.expect_env("NNTPSERVER", "news.example");
    m_env.expect_env("NNTP_FORCE_AUTH", "yes");

    data_source_init();

    ASSERT_EQ(1U, g_data_sources.size());
    const DataSource &source = g_data_sources.front();
    EXPECT_TRUE(source.m_nntp_link.flags & NNTP_FORCE_AUTH_NEEDED);
}

TEST_F(DataSourceInitTest, readsNntpAuthFileForRemoteSource)
{
    std::ofstream{m_output_dir / ".nntpauth"} << "reader\n"
                                              << "secret\n";
    m_env.expect_env("NNTPSERVER", "news.example");
    m_env.expect_no_envar("NNTP_FORCE_AUTH");

    data_source_init();

    ASSERT_EQ(1U, g_data_sources.size());
    const DataSource &source = g_data_sources.front();
    EXPECT_EQ("reader", source.m_auth_user);
    EXPECT_EQ("secret", source.m_auth_pass);
}

TEST_F(DataSourceInitTest, parsesNntpServerPort)
{
    m_env.expect_env("NNTPSERVER", "news.example;119");
    m_env.expect_no_envar("NNTP_FORCE_AUTH");

    data_source_init();

    ASSERT_EQ(1U, g_data_sources.size());
    const DataSource &source = g_data_sources.front();
    EXPECT_EQ("news.example", source.m_news_id);
    EXPECT_EQ(119, source.m_nntp_link.port_number);
}

TEST_F(FindCloseMatchTest, usesSingleCloseMatch)
{
    add_active_source("comp.lang.apl 10 1 y\n");
    set_newsgroup_name("comp.lang.apx");
    testing::internal::CaptureStdout();

    const int         result = find_close_match();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(1, result);
    EXPECT_EQ("comp.lang.apl", g_newsgroup_name);
    EXPECT_EQ("comp/lang/apl", g_newsgroup_dir);
    EXPECT_EQ("(Using comp.lang.apl)\n", output);
    EXPECT_EQ("comp.lang.apl 10 1 y\n", g_data_sources.front().m_act_sf.m_lines.front());
}

TEST_F(FindCloseMatchTest, deduplicatesMatchesFromMultipleSources)
{
    add_active_source("comp.lang.apl 10 1 y\n");
    add_active_source("comp.lang.apl 10 1 y\n");
    set_newsgroup_name("comp.lang.apx");
    testing::internal::CaptureStdout();

    const int         result = find_close_match();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(1, result);
    EXPECT_EQ("comp.lang.apl", g_newsgroup_name);
    EXPECT_EQ("(Using comp.lang.apl)\n", output);
}

TEST_F(FindCloseMatchTest, leavesNameUnchangedWhenNoMatchExists)
{
    add_active_source("comp.lang.apl 10 1 y\n");
    set_newsgroup_name("alt.binaries.example");
    testing::internal::CaptureStdout();

    const int         result = find_close_match();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(0, result);
    EXPECT_EQ("alt.binaries.example", g_newsgroup_name);
    EXPECT_TRUE(output.empty());
}

TEST_F(FindCloseMatchTest, promptsForMultipleCloseMatches)
{
    add_active_source("comp.lang.apl 10 1 y\n"
                      "comp.lang.apm 10 1 y\n");
    set_newsgroup_name("comp.lang.apx");
    push_char('2');
    testing::internal::CaptureStdout();

    const int         result = find_close_match();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(1, result);
    EXPECT_EQ("comp.lang.apm", g_newsgroup_name);
    EXPECT_NE(std::string::npos, output.find("  1.  comp.lang.apl\n"));
    EXPECT_NE(std::string::npos, output.find("  2.  comp.lang.apm\n"));
}
