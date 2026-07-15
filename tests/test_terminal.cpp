// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/terminal.h>

#include <gtest/gtest.h>

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

} // namespace

TEST_F(TerminalTest, getCommandExpandsMacroString)
{
    set_macro("~", "z");

    push_char('~');
    get_cmd(g_buf);

    EXPECT_EQ('z', g_buf[0]);
    EXPECT_EQ(FINISH_CMD, g_buf[1]);
}
