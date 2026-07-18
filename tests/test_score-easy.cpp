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
