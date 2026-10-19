// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-wumpus.h>

#include <trn/Article.h>
#include <trn/artstate.h>
#include <trn/charsubst.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/utf.h>

#include "support/UnderlineFixture.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>

namespace
{

class TreeRenderingTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_curr_artp = g_curr_artp;
        m_old_recent_artp = g_recent_artp;
        m_old_selected_only = g_selected_only;
        m_old_do_hiding = g_do_hiding;
        m_old_char_subst = current_char_subst_mode();
        m_old_max_tree_lines = g_max_tree_lines;
        m_old_threaded_group = g_threaded_group;
        m_old_tc_cols = g_tc_COLS;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_us = g_tc_US;
        m_old_tc_ue = g_tc_UE;
        m_old_tc_uc = g_tc_UC;
        m_old_tc_bc = g_tc_BC;
        m_old_fire_is_out = g_fire_is_out;
        m_old_erase_screen = g_erase_screen;

        m_subject.m_thread = &m_current;
        m_subject.m_thread_link = &m_subject;
        m_current.m_subj = &m_subject;
        m_current.m_child1 = &m_recent;
        m_current.m_flags = AF_EXISTS | AF_CACHED | AF_UNREAD;
        m_recent.m_subj = &m_subject;
        m_recent.m_parent = &m_current;
        m_recent.m_flags = AF_EXISTS | AF_CACHED | AF_UNREAD;

        g_curr_artp = &m_current;
        g_recent_artp = &m_recent;
        g_selected_only = false;
        g_do_hiding = false;
        set_char_subst_mode('\0');
        g_max_tree_lines = 6;
        g_tc_COLS = 40;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
        g_tc_US = m_empty_capability;
        g_tc_UE = m_empty_capability;
        g_tc_UC = m_empty_capability;
        g_tc_BC = "\b";
        g_fire_is_out = 0;
        g_erase_screen = false;
    }

    void TearDown() override
    {
        g_curr_artp = nullptr;
        init_tree();

        g_curr_artp = m_old_curr_artp;
        g_recent_artp = m_old_recent_artp;
        g_selected_only = m_old_selected_only;
        g_do_hiding = m_old_do_hiding;
        set_char_subst_mode(m_old_char_subst);
        g_max_tree_lines = m_old_max_tree_lines;
        g_threaded_group = m_old_threaded_group;
        g_tc_COLS = m_old_tc_cols;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_US = m_old_tc_us;
        g_tc_UE = m_old_tc_ue;
        g_tc_UC = m_old_tc_uc;
        g_tc_BC = m_old_tc_bc;
        g_fire_is_out = m_old_fire_is_out;
        g_erase_screen = m_old_erase_screen;
    }

    char    m_empty_capability[1]{};
    char    m_standout_start[5]{"<so>"};
    char    m_standout_end[5]{"<se>"};
    Article m_current{};
    Article m_recent{};
    Subject m_subject{};

    Article    *m_old_curr_artp{};
    Article    *m_old_recent_artp{};
    bool        m_old_selected_only{};
    bool        m_old_do_hiding{};
    char        m_old_char_subst{};
    int         m_old_max_tree_lines{};
    bool        m_old_threaded_group{};
    int         m_old_tc_cols{};
    std::string_view m_old_tc_so;
    std::string_view m_old_tc_se;
    std::string_view m_old_tc_us;
    std::string_view m_old_tc_ue;
    std::string_view m_old_tc_uc;
    std::string_view m_old_tc_bc;
    int         m_old_fire_is_out{};
    bool        m_old_erase_screen{};
};

} // namespace

TEST_F(TreeRenderingTest, currentAndRecentArticlesAreMarked)
{
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("+", ArticleLine{}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    std::string expected{"+"};
    expected.append(27, ' ');
    expected += "  <so>[1]<se>--[<so>1<se>]\n";
    EXPECT_EQ(expected, output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, normalHeaderKeywordIsSplitFromValue)
{
    g_curr_artp = nullptr;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: Tester\nignored", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Tester\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, hiddenHeaderDecodesEncodedWords)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: =?US-ASCII?B?Qm96byB0aGUgQ2xvd24=?=\nignored", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Bozo the Clown\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, hiddenHeaderSubstitutesDecodedLatinScalars)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    set_char_subst_mode('a');
    init_tree();
    const std::string raw{"From: =?ISO-8859-1?Q?=C4?=\xE2\x80\x99"};

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts(raw, ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Ae\xE2\x80\x99\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
    EXPECT_EQ("From: =?ISO-8859-1?Q?=C4?=\xE2\x80\x99", raw);
}

TEST_F(TreeRenderingTest, unicodeModeSubstitutesDecodedHeaders)
{
    const std::string previous_charsets = g_charsets;
    g_charsets = "up";
    reset_char_subst_mode();
    g_curr_artp = nullptr;
    g_do_hiding = true;
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts("From: =?UTF-8?Q?=E2=80=99?=", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();
    g_charsets = previous_charsets;
    EXPECT_EQ("From: '\n", output);
}

TEST_F(TreeRenderingTest, texConversionDoesNotLeakBetweenHeaders)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    set_char_subst_mode('t');
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts("From: =?US-ASCII?Q?=22A?=", ArticleLine{1}, 0);
    tree_puts("Other: \"", ArticleLine{2}, 0);
    tree_puts("From: A", ArticleLine{3}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: \xC3\x84\nOther: \"\nFrom: A\n", output);
}

TEST_F(TreeRenderingTest, headerWrapMeasuresUtf8Columns)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    g_tc_COLS = 10;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: =?Latin1?Q?=C4=C4=C4?=", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: \xC3\x84\xC3\x84\xC3\x84\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, headerHardWrapRetainsWholeUnicodeScalars)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    g_tc_COLS = 8;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: =?Latin1?Q?=C4=C4?=", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: \xC3\x84\n+     \xC3\x84\n", output);
    EXPECT_EQ(ArticleLine{2}, lines);
}

TEST_F(TreeRenderingTest, continuationLineUsesPreviousHeaderIndent)
{
    g_curr_artp = nullptr;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine first_lines = tree_puts("From: Tester", ArticleLine{1}, 0);
    const ArticleLine second_lines = tree_puts(" continuation", ArticleLine{2}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Tester\n+     continuation\n", output);
    EXPECT_EQ(ArticleLine{1}, first_lines);
    EXPECT_EQ(ArticleLine{1}, second_lines);
}

TEST_F(TreeRenderingTest, longHeaderValueWrapsAtWordBoundary)
{
    g_curr_artp = nullptr;
    g_tc_COLS = 20;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: alpha beta gamma delta", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: alpha beta\n+     gamma delta\n", output);
    EXPECT_EQ(ArticleLine{2}, lines);
}

TEST_F(TreeRenderingTest, subjectLineGetsSubjectPrefixWhenUnthreaded)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("Thread title", ArticleLine{1}, 1);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Subject: Thread title\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, issue30SubjectRestoresUnderlineBeforeFollowingText)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    g_do_hiding = true;
    g_tc_COLS = 120;
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";
    g_fire_is_out = UNDERLINE | STANDOUT;
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts(UNDERLINE_SUBJECT, ArticleLine{1}, 1);
    std::fputs("After\n", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Subject: \x1B[4m" + std::string{UNDERLINE_DECODED_SUBJECT} + "\x1B[24m\nAfter\n", output);
}

TEST_F(TreeRenderingTest, issue30SubjectUnderlinesWholeScalarsOnUndercharTerminal)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    g_do_hiding = true;
    g_tc_COLS = 120;
    g_tc_UC = "<uc>";
    init_tree();
    std::string expected{"Subject: "};
    expected += "\xE2\x80\x9C\b<uc>";
    for (char ch : std::string_view{"Microsoft CEO says up to 30% of the company"})
    {
        expected += std::string{ch} + "\b<uc>";
    }
    expected += "\xE2\x80\x99\b<uc>";
    for (char ch : std::string_view{"s code was written by AI"})
    {
        expected += std::string{ch} + "\b<uc>";
    }
    expected += "\xE2\x80\x9D\b<uc>\nAfter\n";

    testing::internal::CaptureStdout();
    tree_puts(UNDERLINE_SUBJECT, ArticleLine{1}, 1);
    std::fputs("After\n", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(expected, output);
}

TEST_F(TreeRenderingTest, issue30WrappedSubjectRestoresUnderlineOnEveryLine)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    g_do_hiding = true;
    g_tc_COLS = 40;
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";
    g_fire_is_out = UNDERLINE | STANDOUT;
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts(UNDERLINE_SUBJECT, ArticleLine{1}, 1);
    std::fputs("After\n", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Subject: \x1B[4m\xE2\x80\x9CMicrosoft CEO says up to 30%\x1B[24m\n"
              "+        \x1B[4mof the company\xE2\x80\x99s code was\x1B[24m\n"
              "+        \x1B[4mwritten by AI\xE2\x80\x9D\x1B[24m\nAfter\n",
              output);
}

TEST_F(TreeRenderingTest, underPrintPreservesIssue30SubjectAndRestoresAttributes)
{
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";

    testing::internal::CaptureStdout();
    under_print(UNDERLINE_DECODED_SUBJECT);
    std::fputs("After", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("\x1B[4m" + std::string{UNDERLINE_DECODED_SUBJECT} + "\x1B[24mAfter", output);
}

TEST_F(TreeRenderingTest, underPrintHandlesWideCombiningAndMalformedScalars)
{
    g_tc_UC = "<uc>";
    const std::string old_input{input_charset_name()};
    const std::string old_output{output_charset_name()};
    utf_init("Latin1", "Latin1");

    testing::internal::CaptureStdout();
    under_print("\xE7\x95\x8C"
                "e\xCC\x81\xFFZ");
    const std::string output = testing::internal::GetCapturedStdout();
    utf_init(old_input, old_output);

    EXPECT_EQ("\xE7\x95\x8C\b\b<uc><uc>e\b<uc>\xCC\x81\xEF\xBF\xBD\b<uc>Z\b<uc>", output);
}

TEST_F(TreeRenderingTest, underPrintDisplaysControlsWithoutExecutingThem)
{
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";
    std::string text{"\x1B[4m"};
    text += '\0';
    text += "\x7F\xC2\x9B";
    for (const std::string_view capability : {std::string_view{}, std::string_view{"<uc>"}})
    {
        g_tc_UC = capability;
        std::string expected;
        for (char ch : std::string_view{"^[[4m^@^?^["})
        {
            expected += ch;
            if (!capability.empty())
            {
                expected += "\b<uc>";
            }
        }
        if (capability.empty())
        {
            expected = "\x1B[4m" + expected + "\x1B[24m";
        }

        testing::internal::CaptureStdout();
        under_print(text);
        const std::string output = testing::internal::GetCapturedStdout();

        EXPECT_EQ(expected, output);
    }
}

TEST_F(TreeRenderingTest, siblingBranchesAreRenderedOnSeparateLines)
{
    Article sibling{};
    sibling.m_subj = &m_subject;
    sibling.m_parent = &m_current;
    sibling.m_flags = AF_EXISTS | AF_CACHED | AF_UNREAD;
    m_recent.m_sibling = &sibling;

    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("+", ArticleLine{}, 0);
    const ArticleLine second_lines = tree_puts("+", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    std::string expected{"+"};
    expected.append(27, ' ');
    expected += "  <so>[1]<se>+-[<so>1<se>]\n";
    expected += "+";
    expected.append(27, ' ');
    expected += "     \\-[1]\n";
    EXPECT_EQ(expected, output);
    EXPECT_EQ(ArticleLine{1}, lines);
    EXPECT_EQ(ArticleLine{1}, second_lines);
}
