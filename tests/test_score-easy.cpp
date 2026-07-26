// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/score-easy.h>

#include <trn/score.h>
#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

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

class ScoreCommandTest : public testing::Test
{
protected:
    void SetUp() override
    {
        drain_macro_buffer();
        m_old_sc_initialized = g_sc_initialized;
        g_sc_initialized = true;
    }

    void TearDown() override
    {
        drain_macro_buffer();
        g_sc_initialized = m_old_sc_initialized;
    }

    bool m_old_sc_initialized{};
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

TEST_F(ScoreEasyTest, appendOtherScorefileReadsSingleCharacterAbbreviation)
{
    push_char('0');
    push_char('x');
    push_char('4');

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
              "Enter your choice: 4\n"
              "Type the (single character) abbreviation of the scorefile:x\n"
              "What type of line do you want to add?\n"
              "0) Exit.\n"
              "1) A scoring rule line.\n"
              "   (for the current article's author/subject)\n"
              "2) A command, comment, or other kind of line.\n"
              "   (use this for any other kind of line)\n"
              "\n"
              "[Other line formats will be supported later.]\n"
              "Enter your choice: 0\n",
              output);
}

TEST_F(ScoreEasyTest, appendOtherLineReturnsTypedLine)
{
    push_string("32!note\n", 0);

    testing::internal::CaptureStdout();
    const std::string line = sc_easy_append();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ("\" !note", line);
}

TEST_F(ScoreEasyTest, appendRuleRetriesInvalidScoreAmount)
{
    push_string("31bogus\n10\n1", 0);

    testing::internal::CaptureStdout();
    const std::string line = sc_easy_append();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ("\" 10 S", line);
}

TEST_F(ScoreEasyTest, appendRuleDestinationExitReturnsEmptyString)
{
    push_string("3110\n0", 0);

    testing::internal::CaptureStdout();
    const std::string line = sc_easy_append();
    testing::internal::GetCapturedStdout();

    EXPECT_TRUE(line.empty());
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

TEST_F(ScoreCommandTest, unknownCommandReportsCommandText)
{
    const std::string command{"xignored"};

    testing::internal::CaptureStdout();
    sc_score_cmd(std::string_view{command.data(), 1});
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Unknown scoring command |x|\n", output);
}

TEST_F(ScoreCommandTest, easyAppendCommandRunsAppendMenu)
{
    push_char('0');
    push_char('1');

    testing::internal::CaptureStdout();
    sc_score_cmd({});
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("\n"
              "Scoring easy command mode.\n"
              "0) Exit.\n"
              "1) Add something to a scorefile.\n"
              "2) Rescore the articles in the current newsgroup.\n"
              "3) Explain the current article's score.\n"
              "   (show the rules that matched this article)\n"
              "4) Edit this newsgroup's scoring rule file.\n"
              "5) Continue scoring unscored articles.\n"
              "Enter your choice: 1\n"
              "\n"
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
