// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/terminal.h>

#include <trn/final.h>

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

class TerminalTest : public testing::Test
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

class MacroDisplayTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_int_count = g_int_count;
        m_old_erase_screen = g_erase_screen;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_am = g_tc_AM;
        m_old_fire_is_out = g_fire_is_out;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_page_line = g_page_line;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;

        g_int_count = 0;
        g_erase_screen = false;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
        g_tc_AM = false;
        g_tc_LINES = 100;
        g_tc_COLS = 80;
        g_page_line = 1;
        g_term_line = 0;
        g_term_col = 0;
    }

    void TearDown() override
    {
        g_int_count = m_old_int_count;
        g_erase_screen = m_old_erase_screen;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_AM = m_old_tc_am;
        g_fire_is_out = m_old_fire_is_out;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_page_line = m_old_page_line;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
    }

    char m_standout_start[5]{"<so>"};
    char m_standout_end[5]{"<se>"};

    char  m_old_int_count{};
    bool  m_old_erase_screen{};
    char *m_old_tc_so{};
    char *m_old_tc_se{};
    bool  m_old_tc_am{};
    int   m_old_fire_is_out{};
    int   m_old_tc_lines{};
    int   m_old_tc_cols{};
    int   m_old_page_line{};
    int   m_old_term_line{};
    int   m_old_term_col{};
};

class ChoiceInputTest : public MacroDisplayTest
{
protected:
    void SetUp() override
    {
        MacroDisplayTest::SetUp();
        drain_macro_buffer();

        m_old_tc_cr = g_tc_CR;
        m_old_tc_ce = g_tc_CE;
        g_tc_CR = m_carriage_return;
        g_tc_CE = m_erase_line;
    }

    void TearDown() override
    {
        drain_macro_buffer();
        g_tc_CR = m_old_tc_cr;
        g_tc_CE = m_old_tc_ce;
        MacroDisplayTest::TearDown();
    }

    char m_carriage_return[5]{"<cr>"};
    char m_erase_line[5]{"<ce>"};

    const char *m_old_tc_cr{};
    char       *m_old_tc_ce{};
};

} // namespace

TEST_F(TerminalTest, getCommandExpandsMacroString)
{
    set_macro("~", "z");

    push_char('~');
    get_cmd(g_buf);

    EXPECT_EQ('z', g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
}

TEST_F(MacroDisplayTest, showMacrosFormatsNestedControlKey)
{
    set_macro("\001A", "result");

    testing::internal::CaptureStdout();
    show_macros();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("<so>Macros:<se>\n^AA   result\n", output);
}

TEST_F(ChoiceInputTest, inChoiceCyclesToNextValue)
{
    push_char('\n');
    push_char(' ');

    testing::internal::CaptureStdout();
    const bool        clean_screen = in_choice("> ", "yes", "yes/no", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("no", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> yes<cr><cr><ce><cr>> no", output);
}

TEST_F(ChoiceInputTest, inChoiceCyclesValueWithinPrefix)
{
    push_char('\n');
    push_char(' ');

    testing::internal::CaptureStdout();
    const bool clean_screen =
        in_choice("> ", "reverse date", "[reverse] date/subject/author/groups/cnt/points", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("reverse subject", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> reverse date<cr><cr><ce><cr>> reverse subject", output);
}

TEST_F(ChoiceInputTest, inChoicePreservesNumericValue)
{
    push_char('\n');

    testing::internal::CaptureStdout();
    const bool        clean_screen = in_choice("> ", "12", "no/<# lines>", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("12", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> 12", output);
}

TEST_F(ChoiceInputTest, inChoiceDoesNotSplitSlashInsideFreeFormValue)
{
    push_char('\n');

    testing::internal::CaptureStdout();
    const bool        clean_screen = in_choice("> ", "a/b", "<e.g. a/b>", MM_OPTION_EDIT_PROMPT);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(clean_screen);
    EXPECT_STREQ("a/b", g_buf);
    EXPECT_EQ("<cr><cr><ce><cr>> a/b", output);
}
