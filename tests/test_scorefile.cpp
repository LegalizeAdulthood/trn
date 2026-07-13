// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile.h>

#include <trn/head.h>
#include <trn/mempool.h>

#include <gtest/gtest.h>

namespace
{

class ScoreFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        mp_init();
        head_init();
        g_sf_num_entries = 0;
        g_sf_verbose = false;
        g_sf_score_verbose = 0;
    }

    void TearDown() override
    {
        sf_clean();
        head_final();
        g_sf_num_entries = 0;
    }
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
