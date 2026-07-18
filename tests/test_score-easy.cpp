// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/score-easy.h>

#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <string>

namespace
{

void drain_macro_buffer()
{
    while (macro_pending())
    {
        char discarded{};
        read_tty(&discarded, 1);
    }
}

class ScoreEasyTest : public testing::Test
{
protected:
    void SetUp() override
    {
        drain_macro_buffer();
    }

    void TearDown() override
    {
        drain_macro_buffer();
    }
};

} // namespace

TEST_F(ScoreEasyTest, appendExitReturnsEmptyString)
{
    push_char('0');

    testing::internal::CaptureStdout();
    const std::string line = sc_easy_append();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(line.empty());
    EXPECT_EQ("\n"
              "Scorefile easy append mode.\n"
              "0) Exit.\n"
              "1) List the current scorefile abbreviations.\n"
              "2) Add an entry to the global scorefile.\n"
              "3) Add an entry to this newsgroup's scorefile.\n"
              "4) Add an entry to another scorefile.\n"
              "5) Use a temporary scoring rule.\n"
              "Enter your choice: 0\n",
              output);
}

TEST_F(ScoreEasyTest, commandExitReturnsEmptyString)
{
    push_char('0');

    testing::internal::CaptureStdout();
    const std::string line = sc_easy_command();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(line.empty());
    EXPECT_EQ("\n"
              "Scoring easy command mode.\n"
              "0) Exit.\n"
              "1) Add something to a scorefile.\n"
              "2) Rescore the articles in the current newsgroup.\n"
              "3) Explain the current article's score.\n"
              "   (show the rules that matched this article)\n"
              "4) Edit this newsgroup's scoring rule file.\n"
              "5) Continue scoring unscored articles.\n"
              "Enter your choice: 0\n",
              output);
}

TEST_F(ScoreEasyTest, commandAppendReturnsAppendMarker)
{
    push_char('1');

    testing::internal::CaptureStdout();
    const std::string line = sc_easy_command();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("\"", line);
    EXPECT_EQ("\n"
              "Scoring easy command mode.\n"
              "0) Exit.\n"
              "1) Add something to a scorefile.\n"
              "2) Rescore the articles in the current newsgroup.\n"
              "3) Explain the current article's score.\n"
              "   (show the rules that matched this article)\n"
              "4) Edit this newsgroup's scoring rule file.\n"
              "5) Continue scoring unscored articles.\n"
              "Enter your choice: 1\n",
              output);
}
