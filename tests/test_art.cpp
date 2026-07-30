// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/art-internal.h>

#include <trn/search.h>
#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <string>

namespace
{

void drain_macro_buffer()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
}

std::string unfinished_pager_command(char command)
{
    std::string result;
    result += command;
    result += static_cast<char>(FINISH_CMD);
    return result;
}

class ArticlePagerCommandTest : public testing::Test
{
protected:
    void SetUp() override
    {
        search_init();
        art_init();
        drain_macro_buffer();
    }

    void TearDown() override
    {
        drain_macro_buffer();
    }
};

} // namespace

TEST_F(ArticlePagerCommandTest, quitCommandEndsPager)
{
    EXPECT_EQ(PS_TO_END, page_switch("q"));
}

TEST_F(ArticlePagerCommandTest, uppercaseQuitRaisesToArticleLevel)
{
    EXPECT_EQ(PS_RAISE, page_switch("Q"));
}

TEST_F(ArticlePagerCommandTest, completedSearchCommandUsesCommandText)
{
    testing::internal::CaptureStdout();
    EXPECT_EQ(PS_ASK, page_switch("g["));
    testing::internal::GetCapturedStdout();
}

TEST_F(ArticlePagerCommandTest, searchCommandCompletesFromInput)
{
    push_string("[\n", 0);
    const std::string command = unfinished_pager_command('g');

    testing::internal::CaptureStdout();
    EXPECT_EQ(PS_ASK, page_switch(command));
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("g["));
}

TEST_F(ArticlePagerCommandTest, doubleCommandCompletesFromInput)
{
    push_char('x');
    const std::string command = unfinished_pager_command('_');

    testing::internal::CaptureStdout();
    EXPECT_EQ(PS_RAISE, page_switch(command));
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_NE(std::string::npos, output.find("_x"));
}
