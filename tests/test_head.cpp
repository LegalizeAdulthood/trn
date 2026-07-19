// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/head.h>

#include <gtest/gtest.h>

class HeaderLineTypeLookupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        head_init();
        g_head_buf[0] = '\0';
        g_header_type[CUSTOM_LINE].name.clear();
        g_header_type[CUSTOM_LINE].length = 0;
    }

    void TearDown() override
    {
        head_final();
    }
};

TEST_F(HeaderLineTypeLookupTest, findsKnownHeaderIgnoringCase)
{
    EXPECT_EQ(SUBJ_LINE, set_line_type("Subject"));
    EXPECT_EQ(SUBJ_LINE, set_line_type("SUBJECT"));
}

TEST_F(HeaderLineTypeLookupTest, rejectsHeaderWithWhitespaceBeforeColon)
{
    EXPECT_EQ(SOME_LINE, set_line_type("Subject "));
}

TEST_F(HeaderLineTypeLookupTest, recordsCustomHeaderNameInLowerCase)
{
    EXPECT_EQ(CUSTOM_LINE, get_header_num("X-Strange-Thing"));
    EXPECT_EQ("x-strange-thing", g_header_type[CUSTOM_LINE].name);
}
