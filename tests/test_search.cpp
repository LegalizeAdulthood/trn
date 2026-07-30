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
    EXPECT_TRUE(m_regex.compile("needle", false, false).empty());

    EXPECT_TRUE(m_regex.execute(std::string_view{"haystack needle"}));
    EXPECT_FALSE(m_regex.execute(std::string_view{"haystack"}));
}

TEST_F(CompiledRegexTest, emptyPatternReusesPreviousExpression)
{
    ASSERT_TRUE(m_regex.compile("needle", false, false).empty());
    EXPECT_TRUE(m_regex.compile("", false, false).empty());

    EXPECT_TRUE(m_regex.execute(std::string_view{"needle"}));
}

TEST_F(CompiledRegexTest, malformedCharacterClassReportsError)
{
    EXPECT_EQ(std::string_view{"Missing ]"}, m_regex.compile("[abc", true, false));
}

TEST_F(CompiledRegexTest, patternViewDoesNotReadTrailingText)
{
    const std::string      pattern{"[a]x"};
    const std::string_view prefix{pattern.data(), 3};

    ASSERT_TRUE(m_regex.compile(prefix, true, false).empty());

    EXPECT_TRUE(m_regex.execute(std::string_view{"a"}));
    EXPECT_FALSE(m_regex.execute(std::string_view{"x"}));
}

TEST_F(CompiledRegexTest, bracketTextIsAvailableAfterMatch)
{
    ASSERT_TRUE(m_regex.compile(R"(prefix \(needle\) suffix)", true, false).empty());
    ASSERT_TRUE(m_regex.execute(std::string_view{"prefix needle suffix"}));

    EXPECT_TRUE(m_regex.has_brackets());
    EXPECT_EQ(std::string_view{"needle"}, m_regex.get_bracket(1));
    EXPECT_TRUE(m_regex.get_bracket(2).empty());
}

TEST_F(CompiledRegexTest, bracketTextDoesNotOutliveCallerView)
{
    ASSERT_TRUE(m_regex.compile(R"(\(needle\))", true, false).empty());
    {
        const std::string      text{"needle trailing"};
        const std::string_view prefix{text.data(), 6};

        ASSERT_TRUE(m_regex.execute(prefix));
    }

    EXPECT_EQ(std::string_view{"needle"}, m_regex.get_bracket(1));
}

TEST_F(CompiledRegexTest, matchViewDoesNotReadTrailingText)
{
    const std::string      text{"haystack needle"};
    const std::string_view prefix{text.data(), 8};

    ASSERT_TRUE(m_regex.compile("needle", false, false).empty());

    EXPECT_FALSE(m_regex.execute(prefix));
    EXPECT_TRUE(m_regex.execute(text));
}

} // namespace
