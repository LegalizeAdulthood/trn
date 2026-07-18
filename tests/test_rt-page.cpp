// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <gtest/gtest.h>

#include <string>

TEST(SelectorPageTest, outputSelectorUsesConfiguredCharacterOrder)
{
    g_use_sel_num = false;
    g_sel_chars = "abc";

    testing::internal::CaptureStdout();
    output_sel(1, 0, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("b ", output);
}
