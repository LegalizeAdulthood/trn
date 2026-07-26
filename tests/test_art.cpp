// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/art-internal.h>

#include <gtest/gtest.h>

TEST(ArticlePagerCommandTest, quitCommandEndsPager)
{
    EXPECT_EQ(PS_TO_END, page_switch("q"));
}

TEST(ArticlePagerCommandTest, uppercaseQuitRaisesToArticleLevel)
{
    EXPECT_EQ(PS_RAISE, page_switch("Q"));
}
