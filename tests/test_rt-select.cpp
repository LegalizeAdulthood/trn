// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-select-internal.h>

#include <trn/terminal.h>

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

class SelectorEscapedCommandTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_errno = errno;
        errno = 0;
        drain_input();
    }

    void TearDown() override
    {
        drain_input();
        errno = m_old_errno;
    }

    int m_old_errno{};
};

} // namespace

TEST_F(SelectorEscapedCommandTest, readsCommandCharacter)
{
    push_char('x');

    EXPECT_EQ('x', read_selector_escaped_command_for_test());
}

TEST_F(SelectorEscapedCommandTest, readsDefaultCommandCharacter)
{
    push_char('\\');

    EXPECT_EQ('\\', read_selector_escaped_command_for_test());
}

TEST_F(SelectorEscapedCommandTest, readsBlankCommandCharacter)
{
    push_char(' ');

    EXPECT_EQ(' ', read_selector_escaped_command_for_test());
}
