// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile-internal.h>

#include <trn/head.h>
#include <trn/init.h>
#include <trn/mempool.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <util/env.h>

#include <config/common.h>
#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

std::string g_fetched_url;

namespace fs = std::filesystem;

bool fetch_score_url(std::string_view url, const char *outfile)
{
    g_fetched_url = std::string{url};

    std::ofstream output{outfile, std::ios::binary};
    output << "10 subject: remote\n";
    return output.good();
}

class ScoreFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_tmp_dir = g_tmp_dir;
        m_old_pid = g_our_pid;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_term_scrolled = g_term_scrolled;

        mp_init();
        head_init();
        g_tmp_dir = TRN_TEST_TMP_DIR;
        g_our_pid = 1357;
        g_sf_num_entries = 0;
        g_sf_verbose = false;
        g_sf_score_verbose = 0;
    }

    void TearDown() override
    {
        sf_set_url_getter_for_test(nullptr);
        sf_clear_file_cache_for_test();
        sf_clean();
        head_final();
        g_sf_num_entries = 0;
        g_tmp_dir = m_old_tmp_dir;
        g_our_pid = m_old_pid;
        g_newsgroup_name = m_old_newsgroup_name;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_term_scrolled = m_old_term_scrolled;
    }

    std::string m_old_tmp_dir;
    std::string m_old_newsgroup_name;
    long        m_old_pid{};
    int         m_old_term_line{};
    int         m_old_term_col{};
    int         m_old_term_scrolled{};
};

} // namespace

TEST_F(ScoreFileTest, extraHeaderLookupIsCaseInsensitive)
{
    char header[]{"!header X-Custom-Score:"};
    sf_append(header);

    EXPECT_EQ(0, g_sf_num_entries);

    char rule[]{"!10 X-CUSTOM-SCORE: value"};
    sf_append(rule);

    EXPECT_EQ(1, g_sf_num_entries);
}

TEST_F(ScoreFileTest, includeUrlFetchesScoreFile)
{
    g_fetched_url.clear();
    sf_set_url_getter_for_test(fetch_score_url);

    char include[]{"!include URL:http://example.test/scores"};
    sf_append(include);

    EXPECT_EQ("http://example.test/scores", g_fetched_url);
    EXPECT_EQ(3, g_sf_num_entries);
}

TEST_F(ScoreFileTest, editLocalFileBuildsExpandedEditorCommand)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-edit"};
    const std::string score_file{score_dir + "/comp.lang.apl"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());
    env.expect_no_envar("EDITOR");
    env.expect_env("VISUAL", ":");

    sf_edit_file("\"");

    EXPECT_STREQ((": " + score_file).c_str(), g_cmd_buf);
    EXPECT_TRUE(fs::exists(score_dir));
}
