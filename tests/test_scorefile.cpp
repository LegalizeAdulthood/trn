// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile-internal.h>

#include <trn/head.h>
#include <trn/init.h>
#include <trn/mempool.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace
{

std::string g_fetched_url;

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
    }

    std::string m_old_tmp_dir;
    long        m_old_pid{};
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
