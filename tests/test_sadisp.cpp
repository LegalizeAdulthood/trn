// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/sadisp-internal.h>

#include <trn/scanart.h>
#include <trn/score.h>

#include <gtest/gtest.h>

#include <string>

namespace
{

class ScanArticleDisplayTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_sa_mode_order = g_sa_mode_order;
        m_old_score_new_first = g_score_new_first;
    }

    void TearDown() override
    {
        g_sa_mode_order = m_old_sa_mode_order;
        g_score_new_first = m_old_score_new_first;
    }

    SaDisplayOrder m_old_sa_mode_order{};
    bool           m_old_score_new_first{};
};

} // namespace

TEST_F(ScanArticleDisplayTest, orderTextUsesArrivalOrder)
{
    g_sa_mode_order = SA_ORDER_ARRIVAL;

    EXPECT_EQ("arrival", std::string{sa_order_text_for_test()});
}

TEST_F(ScanArticleDisplayTest, orderTextUsesOldFirstScoreOrder)
{
    g_sa_mode_order = SA_ORDER_DESCENDING;
    g_score_new_first = false;

    EXPECT_EQ("score (old>new)", std::string{sa_order_text_for_test()});
}

TEST_F(ScanArticleDisplayTest, orderTextUsesNewFirstScoreOrder)
{
    g_sa_mode_order = SA_ORDER_DESCENDING;
    g_score_new_first = true;

    EXPECT_EQ("score (new>old)", std::string{sa_order_text_for_test()});
}

TEST_F(ScanArticleDisplayTest, orderTextUsesUnknownFallback)
{
    g_sa_mode_order = SA_ORDER_NONE;

    EXPECT_EQ("unknown", std::string{sa_order_text_for_test()});
}
