// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artsrch.h>

#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/head.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/search.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class ArticleSearchTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_article_list = std::move(g_article_list);
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_art = g_art;
        m_old_mode = g_mode;
        m_old_general_mode = g_general_mode;
        m_old_use_threads = g_use_threads;
        m_old_verbose = g_verbose;
        m_old_novice_delays = g_novice_delays;
        m_old_local_kfp = g_local_kfp;
        m_old_art_fp = g_art_fp;
        m_old_open_art = g_open_art;
        m_old_art_buf = g_art_buf;
        m_old_art_buf_pos = g_art_buf_pos;
        m_old_art_buf_seek = g_art_buf_seek;
        m_old_art_buf_len = g_art_buf_len;
        m_old_raw_art_size = g_raw_art_size;
        m_old_art_size = g_art_size;
        m_old_do_hiding = g_do_hiding;
        m_old_data_source = g_data_source;
        m_old_parsed_art = g_parsed_art;
        m_old_past_header = g_header_type[PAST_HEADER];
        m_old_threaded_group = g_threaded_group;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        fs::create_directories(m_output_dir, error);

        g_article_list.clear();
        g_newsgroup_ptr = &m_group;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{};
        g_art = ArticleNum{};
        g_mode = MM_ARTICLE;
        g_general_mode = GM_READ;
        g_use_threads = false;
        g_verbose = false;
        g_novice_delays = false;
        g_local_kfp = nullptr;
        g_art_fp = nullptr;
        g_art_buf = nullptr;
        g_open_art = ArticleNum{};
        g_data_source = &m_data_source;
        g_do_hiding = false;
        g_threaded_group = false;

        m_group.m_to_read = ArticleUnread{};

        search_init();
        head_init();
        art_io_init();
        art_search_init();
    }

    void TearDown() override
    {
        if (g_local_kfp != nullptr)
        {
            std::fclose(g_local_kfp);
        }
        g_local_kfp = m_old_local_kfp;

        art_close();
        art_io_final();
        head_final();

        g_article_list.clear();
        g_article_list = std::move(m_old_article_list);
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_art = m_old_art;
        g_mode = m_old_mode;
        g_general_mode = m_old_general_mode;
        g_use_threads = m_old_use_threads;
        g_verbose = m_old_verbose;
        g_novice_delays = m_old_novice_delays;
        g_art_fp = m_old_art_fp;
        g_open_art = m_old_open_art;
        g_art_buf = m_old_art_buf;
        g_art_buf_pos = m_old_art_buf_pos;
        g_art_buf_seek = m_old_art_buf_seek;
        g_art_buf_len = m_old_art_buf_len;
        g_raw_art_size = m_old_raw_art_size;
        g_art_size = m_old_art_size;
        g_do_hiding = m_old_do_hiding;
        g_data_source = m_old_data_source;
        g_parsed_art = m_old_parsed_art;
        g_header_type[PAST_HEADER] = m_old_past_header;
        g_threaded_group = m_old_threaded_group;

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    std::vector<std::string> read_lines(const fs::path &path)
    {
        std::ifstream            input{path};
        std::vector<std::string> lines;
        std::string              line;
        while (std::getline(input, line))
        {
            lines.push_back(line);
        }
        return lines;
    }

    void stage_article(std::string_view text)
    {
        const fs::path article_file = m_output_dir / "article";
        {
            std::ofstream output{article_file};
            output << text;
        }

        const std::string article_file_name = article_file.generic_string();
        g_art_fp = std::fopen(article_file_name.c_str(), "r");
        ASSERT_NE(nullptr, g_art_fp);
        g_open_art = ArticleNum{1};
        g_raw_art_size = ArticlePosition{static_cast<long>(text.size())};
        g_art_size = g_raw_art_size;
        clear_art_buf();

        Article *article = article_ptr(ArticleNum{1});
        m_subject.m_str = "Body search";
        article->m_flags = AF_EXISTS | AF_UNREAD | AF_CACHED;
        article->m_subj = &m_subject;
        g_last_art = ArticleNum{1};
        m_group.m_to_read = ArticleUnread{1};
    }

    trn::testing::MockEnvironment m_env;
    NewsgroupData                 m_group{};
    DataSource                    m_data_source{};
    Subject                       m_subject{};
    fs::path                      m_output_dir;
    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    ArticleNum                    m_old_art{};
    MinorMode                     m_old_mode{};
    GeneralMode                   m_old_general_mode{};
    bool                          m_old_use_threads{};
    bool                          m_old_verbose{};
    bool                          m_old_novice_delays{};
    std::FILE                    *m_old_local_kfp{};
    std::FILE                    *m_old_art_fp{};
    ArticleNum                    m_old_open_art{};
    char                         *m_old_art_buf{};
    ArticlePosition               m_old_art_buf_pos{};
    ArticlePosition               m_old_art_buf_seek{};
    ArticlePosition               m_old_art_buf_len{};
    ArticlePosition               m_old_raw_art_size{};
    ArticlePosition               m_old_art_size{};
    bool                          m_old_do_hiding{};
    DataSource                   *m_old_data_source{};
    ArticleNum                    m_old_parsed_art{};
    HeaderType                    m_old_past_header{};
    bool                          m_old_threaded_group{};
};

} // namespace

TEST_F(ArticleSearchTest, serializesLocalSubjectKillCommand)
{
    const fs::path    kill_file{m_output_dir / "local-kill"};
    const std::string kill_file_name = kill_file.generic_string();
    m_env.expect_env("KILLLOCAL", kill_file_name.c_str());

    EXPECT_EQ(SRCH_DONE, art_search("/one\\/two/K:j", false));
    EXPECT_EQ((std::vector<std::string>{"/one\\/two/:j"}), read_lines(kill_file));
}

TEST_F(ArticleSearchTest, serializesHeaderKillCommand)
{
    const fs::path    kill_file{m_output_dir / "header-kill"};
    const std::string kill_file_name = kill_file.generic_string();
    m_env.expect_env("KILLLOCAL", kill_file_name.c_str());

    EXPECT_EQ(SRCH_DONE, art_search("/Casey/KHFrom:+", false));
    EXPECT_EQ((std::vector<std::string>{"/Casey/Hfrom:+"}), read_lines(kill_file));
}

TEST_F(ArticleSearchTest, bodySearchFindsBodyText)
{
    stage_article("Subject: Body search\n"
                  "From: casey@example.test\n"
                  "\n"
                  "body-needle lives here\n");

    EXPECT_EQ(SRCH_FOUND, art_search("/body-needle/B", false));
}

TEST_F(ArticleSearchTest, bodyNoSigIgnoresSignatureText)
{
    stage_article("Subject: Body search\n"
                  "From: casey@example.test\n"
                  "\n"
                  "ordinary body line\n"
                  "-- \n"
                  "signature-needle lives here\n");

    EXPECT_EQ(SRCH_NOT_FOUND, art_search("/signature-needle/b", false));
}
