// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngdata.h>
#include <trn/cache.h>
#include <trn/score.h>
#include <trn/scoresave.h>
#include <trn/scanart.h>
#include <trn/trn.h>

#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

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

class ScoreSaveTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_article_list = std::move(g_article_list);
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_sa_mode_read_elig = g_sa_mode_read_elig;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_score_file = m_output_dir / "saved-scores";
        m_temp_file = m_score_file;
        m_temp_file += ".tmp";
        m_score_file_name = m_score_file.generic_string();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        g_newsgroup_name = "comp.lang.apl";
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{};
        g_sa_mode_read_elig = false;
    }

    void TearDown() override
    {
        g_article_list.clear();
        g_article_list = std::move(m_old_article_list);
        g_newsgroup_name = m_old_newsgroup_name;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_sa_mode_read_elig = m_old_sa_mode_read_elig;

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    void add_scored_article(ArticleNum num, int score)
    {
        Article *article = article_ptr(num);
        article->m_flags = AF_EXISTS | AF_UNREAD;
        sc_set_score(num, score);
    }

    trn::testing::MockEnvironment m_env;
    std::string                   m_score_file_name;
    fs::path                      m_output_dir;
    fs::path                      m_score_file;
    fs::path                      m_temp_file;
    std::string                   m_old_newsgroup_name;
    std::map<ArticleNum, Article> m_old_article_list;
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    bool                          m_old_sa_mode_read_elig{};
};

} // namespace

TEST_F(ScoreSaveTest, saveFileRewritesConfiguredFile)
{
    sc_save_scores();

    m_env.expect_env("SAVESCOREFILE", m_score_file_name.c_str());
    sc_sv_save_file();

    ASSERT_TRUE(fs::exists(m_score_file));
    EXPECT_EQ((std::vector<std::string>{
                  "#STRN saved score file.",
                  "v1.0",
                  "!comp.lang.apl",
                  ":1",
              }),
              read_lines(m_score_file));
    EXPECT_FALSE(fs::exists(m_temp_file));
}

TEST_F(ScoreSaveTest, saveFileWritesEncodedScoreLines)
{
    g_last_art = ArticleNum{5};
    add_scored_article(ArticleNum{1}, 12);
    add_scored_article(ArticleNum{2}, 12);
    add_scored_article(ArticleNum{3}, 12);
    add_scored_article(ArticleNum{5}, -3);

    sc_save_scores();

    m_env.expect_env("SAVESCOREFILE", m_score_file_name.c_str());
    sc_sv_save_file();

    ASSERT_TRUE(fs::exists(m_score_file));
    EXPECT_EQ((std::vector<std::string>{
                  "#STRN saved score file.",
                  "v1.0",
                  "!comp.lang.apl",
                  ":1",
                  ".K2r2sG",
              }),
              read_lines(m_score_file));
}
