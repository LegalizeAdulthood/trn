// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-ov.h>

#include <nntp/nntpclient.h>

#include <config/common.h>
#include <test_config.h>
#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/trn.h>

#include <parsedate/parsedate.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "MockNNTPConnection.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace
{

namespace fs = std::filesystem;

constexpr ArticleNum TEST_ARTICLE_NUM{1};

class OverviewTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_current_path = fs::current_path();
        m_old_data_source = g_data_source;
        m_old_article_list = std::move(g_article_list);
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_first_cached = g_first_cached;
        m_old_last_cached = g_last_cached;
        m_old_cached_all_in_range = g_cached_all_in_range;
        m_old_verbose = g_verbose;
        m_old_threaded_group = g_threaded_group;
        m_old_thread_always = g_thread_always;
        m_old_spin_todo = g_spin_todo;
        m_old_spin_estimate = g_spin_estimate;
        m_old_int_count = g_int_count;
        m_old_curr_artp = g_curr_artp;
        m_old_sentinel_art_ptr = g_sentinel_art_ptr;
        m_old_nntp_connection = g_nntp_link.connection;
        m_old_nntp_flags = g_nntp_link.flags;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_overview_dir = m_output_dir / "overview";

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        std::ofstream{m_output_dir / "1"} << "article 1\n";
        fs::current_path(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        m_overview_dir_text = m_overview_dir.generic_string();

        g_data_source = &m_data_source;
        g_newsgroup_ptr = &m_group;
        m_data_source.m_over_dir = m_overview_dir_text;
        for (int i = 0; i < OV_MAX_FIELDS; ++i)
        {
            m_data_source.m_field_num[i] = static_cast<OverviewFieldNum>(i);
            m_data_source.m_field_flags[i] = FF_HAS_FIELD;
        }

        g_newsgroup_name = "comp.lang.apl";
        g_abs_first = TEST_ARTICLE_NUM;
        g_first_art = TEST_ARTICLE_NUM;
        g_last_art = TEST_ARTICLE_NUM;
        g_first_cached = TEST_ARTICLE_NUM;
        g_last_cached = ArticleNum{};
        g_cached_all_in_range = false;
        g_verbose = false;
        g_threaded_group = false;
        g_thread_always = false;
        g_spin_todo = 1;
        g_spin_estimate = 1;
        g_int_count = 0;
        g_curr_artp = nullptr;
        g_sentinel_art_ptr = nullptr;
        g_nntp_link.connection.reset();
        g_nntp_link.flags = NNTP_NEW_CMD_OK;

        head_init();

        m_group.m_rc_line = "comp.lang.apl: ";
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.m_abs_first = g_abs_first;
        m_group.m_ng_max = g_last_art;
        m_group.m_to_read = ArticleUnread{1};

        build_cache();
    }

    void TearDown() override
    {
        ov_close();
        close_cache();

        std::error_code error;
        fs::current_path(m_old_current_path, error);

        g_data_source = m_old_data_source;
        g_article_list = std::move(m_old_article_list);
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_newsgroup_name = m_old_newsgroup_name;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_first_cached = m_old_first_cached;
        g_last_cached = m_old_last_cached;
        g_cached_all_in_range = m_old_cached_all_in_range;
        g_verbose = m_old_verbose;
        g_threaded_group = m_old_threaded_group;
        g_thread_always = m_old_thread_always;
        g_spin_todo = m_old_spin_todo;
        g_spin_estimate = m_old_spin_estimate;
        g_int_count = m_old_int_count;
        g_curr_artp = m_old_curr_artp;
        g_sentinel_art_ptr = m_old_sentinel_art_ptr;
        g_nntp_link.connection = std::move(m_old_nntp_connection);
        g_nntp_link.flags = m_old_nntp_flags;

        fs::remove_all(m_output_dir, error);

        head_final();
    }

    fs::path overview_file() const
    {
        return fs::path{m_overview_dir_text + "/comp/lang/apl" OV_FILE_NAME};
    }

    DataSource    m_data_source{};
    NewsgroupData m_group{};
    fs::path      m_old_current_path;
    fs::path      m_output_dir;
    fs::path      m_overview_dir;
    std::string   m_overview_dir_text;

    DataSource                   *m_old_data_source{};
    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    std::string                   m_old_newsgroup_name;
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    ArticleNum  m_old_first_cached{};
    ArticleNum  m_old_last_cached{};
    bool        m_old_cached_all_in_range{};
    bool        m_old_verbose{};
    bool        m_old_threaded_group{};
    bool        m_old_thread_always{};
    long        m_old_spin_todo{};
    long        m_old_spin_estimate{};
    char        m_old_int_count{};
    Article    *m_old_curr_artp{};
    Article    *m_old_sentinel_art_ptr{};
    ConnectionPtr m_old_nntp_connection;
    NNTPFlags     m_old_nntp_flags{};
};

} // namespace

TEST_F(OverviewTest, localOverviewPathUsesGroupDirectory)
{
    const fs::path file{overview_file()};
    fs::create_directories(file.parent_path());
    std::ofstream{file};

    EXPECT_TRUE(ov_data(TEST_ARTICLE_NUM, TEST_ARTICLE_NUM, false));
    EXPECT_NE(nullptr, m_data_source.m_ov_in);
}

TEST_F(OverviewTest, localOverviewLinePopulatesArticleFields)
{
    const fs::path file{overview_file()};
    fs::create_directories(file.parent_path());
    const std::string date{"Tue, 01 Jan 2019 00:00:00 GMT"};
    std::ofstream{file} << "1\tRe: Overview Subject\tAlice <alice@example.com>\t" << date
                        << "\t<child@example.com>\t<parent@example.com>\t"
                           "1234\t56\tcomp.lang.apl:1 comp.lang.cpp:5\n";

    EXPECT_TRUE(ov_data(TEST_ARTICLE_NUM, TEST_ARTICLE_NUM, false));

    Article *article = article_ptr(TEST_ARTICLE_NUM);
    EXPECT_TRUE(article->m_flags & AF_CACHED);
    EXPECT_EQ("Re: Overview Subject", article->get_cached_line_text(SUBJ_LINE, false));
    ASSERT_TRUE(article->m_from);
    EXPECT_EQ("Alice <alice@example.com>", *article->m_from);
    ASSERT_TRUE(article->m_msg_id);
    EXPECT_EQ("<child@example.com>", *article->m_msg_id);
    EXPECT_EQ(parsedate(date.c_str()), article->m_date);
    EXPECT_EQ(1234, article->m_bytes);
    EXPECT_EQ(56, article->m_lines);
    ASSERT_TRUE(article->m_xrefs);
    EXPECT_EQ("comp.lang.apl:1 comp.lang.cpp:5", *article->m_xrefs);
}

TEST_F(OverviewTest, remoteOverviewRequestsXoverRange)
{
    const std::shared_ptr<testing::StrictMock<MockNNTPConnection>> connection =
        std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    const std::string date{"Tue, 01 Jan 2019 00:00:00 GMT"};
    g_nntp_link.connection = connection;
    m_data_source.m_over_dir.clear();
    m_data_source.m_flags |= DF_REMOTE;

    EXPECT_CALL(*connection, write_line(testing::StrEq("XOVER 1-1"), testing::_));
    EXPECT_CALL(*connection, read_line(testing::_))
        .WillOnce(testing::Return("224 Overview follows"))
        .WillOnce(testing::Return("1\tRe: Overview Subject\tAlice <alice@example.com>\t" + date +
                                  "\t<child@example.com>\t<parent@example.com>\t"
                                  "1234\t56\tcomp.lang.apl:1 comp.lang.cpp:5"))
        .WillOnce(testing::Return("."));

    EXPECT_TRUE(ov_data(TEST_ARTICLE_NUM, TEST_ARTICLE_NUM, false));

    Article *article = article_ptr(TEST_ARTICLE_NUM);
    EXPECT_TRUE(article->m_flags & AF_CACHED);
    EXPECT_EQ("Re: Overview Subject", article->get_cached_line_text(SUBJ_LINE, false));
}

TEST_F(OverviewTest, initMapsOverviewFormatFields)
{
    const fs::path overview_format = m_output_dir / "overview.fmt";
    std::ofstream{overview_format} << "Subject:\n"
                                      "From:\n"
                                      "Date:\n"
                                      "Message-ID:\n"
                                      "References:\n"
                                      "Bytes:full\n"
                                      "Lines:\n"
                                      "Xref:full\n";
    m_data_source.m_over_fmt = overview_format.generic_string();
    for (int i = 0; i < OV_MAX_FIELDS; ++i)
    {
        m_data_source.m_field_num[i] = OV_NUM;
        m_data_source.m_field_flags[i] = FF_NONE;
    }

    EXPECT_TRUE(ov_init());

    EXPECT_EQ(OV_NUM, m_data_source.m_field_num[0]);
    EXPECT_EQ(OV_SUBJ, m_data_source.m_field_num[1]);
    EXPECT_EQ(OV_FROM, m_data_source.m_field_num[2]);
    EXPECT_EQ(OV_DATE, m_data_source.m_field_num[3]);
    EXPECT_EQ(OV_MSG_ID, m_data_source.m_field_num[4]);
    EXPECT_EQ(OV_REFS, m_data_source.m_field_num[5]);
    EXPECT_EQ(OV_BYTES, m_data_source.m_field_num[6]);
    EXPECT_EQ(OV_LINES, m_data_source.m_field_num[7]);
    EXPECT_EQ(OV_XREF, m_data_source.m_field_num[8]);
    EXPECT_EQ(FF_HAS_FIELD | FF_HAS_HDR, m_data_source.m_field_flags[OV_BYTES]);
    EXPECT_EQ(FF_HAS_FIELD | FF_HAS_HDR, m_data_source.m_field_flags[OV_XREF]);
}
