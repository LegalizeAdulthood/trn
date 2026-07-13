// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngdata.h>
#include <trn/scoresave.h>
#include <trn/trn.h>

#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
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
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;

        std::error_code error;
        fs::remove(m_score_file, error);
        fs::remove(m_temp_file, error);
        g_newsgroup_name = "comp.lang.apl";
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{};
    }

    void TearDown() override
    {
        g_newsgroup_name = m_old_newsgroup_name;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;

        std::error_code error;
        fs::remove(m_score_file, error);
        fs::remove(m_temp_file, error);
    }

    trn::testing::MockEnvironment m_env;
    std::string                   m_score_file_name{TRN_TEST_TMP_DIR "/saved-scores"};
    fs::path                      m_score_file{m_score_file_name};
    fs::path                      m_temp_file{m_score_file_name + ".tmp"};
    std::string                   m_old_newsgroup_name;
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
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
