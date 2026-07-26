// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-select-internal.h>

#include <trn/smisc.h>
#include <trn/terminal.h>
#include <trn/univ.h>

#include <gtest/gtest.h>

#include <cerrno>

namespace
{

void drain_input()
{
    while (macro_pending())
    {
        char discarded{};
        read_tty(&discarded, 1);
    }
}

class SelectorCommandInputTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_errno = errno;
        m_old_s_default_cmd = g_s_default_cmd;
        m_old_univ_default_cmd = g_univ_default_cmd;
        m_old_erase_char = g_erase_char;
        m_old_kill_char = g_kill_char;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_verify = g_verify;
        errno = 0;
        g_s_default_cmd = false;
        g_univ_default_cmd = false;
        g_erase_char = '\b';
        g_kill_char = Ctl('u');
        g_general_mode = GM_READ;
        g_mode = MM_NONE;
        g_verify = false;
        drain_input();
    }

    void TearDown() override
    {
        drain_input();
        errno = m_old_errno;
        g_s_default_cmd = m_old_s_default_cmd;
        g_univ_default_cmd = m_old_univ_default_cmd;
        g_erase_char = m_old_erase_char;
        g_kill_char = m_old_kill_char;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_verify = m_old_verify;
    }

    int         m_old_errno{};
    bool        m_old_s_default_cmd{};
    bool        m_old_univ_default_cmd{};
    char        m_old_erase_char{};
    char        m_old_kill_char{};
    GeneralMode m_old_general_mode{};
    MinorMode   m_old_mode{};
    bool        m_old_verify{};
};

} // namespace

TEST_F(SelectorCommandInputTest, readsCommandText)
{
    push_char('x');

    const std::string command = read_selector_command_for_test('>', 'Z', false);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('x', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsPageDefaultCommand)
{
    push_char(' ');

    const std::string command = read_selector_command_for_test('>', 'Z', false);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('>', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsEndDefaultCommand)
{
    push_char(' ');

    const std::string command = read_selector_command_for_test('>', 'Z', true);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('Z', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsEscapedCommandCharacter)
{
    push_char('x');

    EXPECT_EQ('x', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsEscapedDefaultCommandCharacter)
{
    push_char('\\');

    EXPECT_EQ('\\', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsEscapedBlankCommandCharacter)
{
    push_char(' ');

    EXPECT_EQ(' ', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationDigit)
{
    push_char('7');

    EXPECT_EQ('7', read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationEraseCharacter)
{
    push_char(g_erase_char);

    EXPECT_EQ(g_erase_char, read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationKillCharacter)
{
    push_char(g_kill_char);

    EXPECT_EQ(g_kill_char, read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsPromptCommandText)
{
    push_char('s');

    testing::internal::CaptureStdout();
    const std::string command =
        read_selector_prompt_command_for_test("Selector mode?", MM_SELECTOR_ORDER_PROMPT, "tsa");
    testing::internal::GetCapturedStdout();

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('s', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsPromptDefaultCommand)
{
    push_char(' ');

    testing::internal::CaptureStdout();
    const std::string command =
        read_selector_prompt_command_for_test("Selector mode?", MM_SELECTOR_ORDER_PROMPT, "tsa");
    testing::internal::GetCapturedStdout();

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('t', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsFollowupCommandText)
{
    push_char('x');

    EXPECT_EQ('x', read_selector_followup_command_for_test(2));
}

TEST_F(SelectorCommandInputTest, finishesSelectorCommandText)
{
    push_string("topic\n", 0200);

    const std::string command = finish_selector_command_for_test('/', false);

    EXPECT_EQ("/topic", command);
}
