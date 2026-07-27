// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/search.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace
{

class CompiledRegexTest : public testing::Test
{
protected:
    void SetUp() override
    {
        search_init();
        m_regex.init_compex();
    }

    void TearDown() override
    {
        m_regex.free_compex();
    }

    CompiledRegex m_regex{};
};

TEST_F(CompiledRegexTest, literalPatternMatchesText)
{
    EXPECT_EQ(nullptr, m_regex.compile("needle", false, false));

    EXPECT_NE(nullptr, m_regex.execute("haystack needle"));
    EXPECT_EQ(nullptr, m_regex.execute("haystack"));
}

TEST_F(CompiledRegexTest, emptyPatternReusesPreviousExpression)
{
    ASSERT_EQ(nullptr, m_regex.compile("needle", false, false));
    EXPECT_EQ(nullptr, m_regex.compile("", false, false));

    EXPECT_NE(nullptr, m_regex.execute("needle"));
}

TEST_F(CompiledRegexTest, malformedCharacterClassReportsError)
{
    EXPECT_STREQ("Missing ]", m_regex.compile("[abc", true, false));
}

TEST_F(CompiledRegexTest, patternViewDoesNotReadTrailingText)
{
    const std::string      pattern{"[a]x"};
    const std::string_view prefix{pattern.data(), 3};

    ASSERT_EQ(nullptr, m_regex.compile(prefix, true, false));

    EXPECT_NE(nullptr, m_regex.execute("a"));
    EXPECT_EQ(nullptr, m_regex.execute("x"));
}

} // namespace
