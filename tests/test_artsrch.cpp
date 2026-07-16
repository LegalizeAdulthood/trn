// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artsrch.h>

#include <trn/cache.h>
#include <trn/head.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/search.h>
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

        m_group.m_to_read = ArticleUnread{};

        search_init();
        head_init();
        art_search_init();
    }

    void TearDown() override
    {
        if (g_local_kfp != nullptr)
        {
            std::fclose(g_local_kfp);
        }
        g_local_kfp = m_old_local_kfp;

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

    trn::testing::MockEnvironment m_env;
    NewsgroupData                 m_group{};
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
};

} // namespace

TEST_F(ArticleSearchTest, serializesLocalSubjectKillCommand)
{
    const fs::path    kill_file{m_output_dir / "local-kill"};
    const std::string kill_file_name = kill_file.generic_string();
    EXPECT_CALL(m_env.getter, Call(::testing::StrEq("KILLLOCAL")))
        .WillRepeatedly(::testing::Return(const_cast<char *>(kill_file_name.c_str())));

    char command[]{"/one\\/two/K:j"};

    EXPECT_EQ(SRCH_DONE, art_search(command, sizeof command, false));
    EXPECT_EQ((std::vector<std::string>{"/one\\/two/:j"}), read_lines(kill_file));
}

TEST_F(ArticleSearchTest, serializesHeaderKillCommand)
{
    const fs::path    kill_file{m_output_dir / "header-kill"};
    const std::string kill_file_name = kill_file.generic_string();
    EXPECT_CALL(m_env.getter, Call(::testing::StrEq("KILLLOCAL")))
        .WillRepeatedly(::testing::Return(const_cast<char *>(kill_file_name.c_str())));

    char command[]{"/Casey/KHFrom:+"};

    EXPECT_EQ(SRCH_DONE, art_search(command, sizeof command, false));
    EXPECT_EQ((std::vector<std::string>{"/Casey/Hfrom:+"}), read_lines(kill_file));
}
