// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/search.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

namespace
{

class CompiledRegexTest : public testing::Test
{
protected:
    void SetUp() override;

    void TearDown() override
    {
        m_regex.free_compex();
    }

    CompiledRegex m_regex{};
};

void CompiledRegexTest::SetUp()
{
    search_init();
    m_regex.init_compex();
}

struct RegexMatchCase
{
    const char      *name;
    std::string_view pattern;
    std::size_t      pattern_length;
    bool             metacharacters;
    std::string_view text;
    bool             expected;
};

void PrintTo(const RegexMatchCase &value, std::ostream *out)
{
    *out << value.name;
}

class RegexMatchingTest : public CompiledRegexTest, public testing::WithParamInterface<RegexMatchCase>
{
protected:
    void SetUp() override;

    std::string m_pattern;
    std::string m_text;
};

void RegexMatchingTest::SetUp()
{
    CompiledRegexTest::SetUp();
    const RegexMatchCase &input = GetParam();
    m_pattern = input.pattern;
    m_text = input.text;
    const std::string_view pattern = std::string_view{m_pattern}.substr(0, input.pattern_length);
    ASSERT_TRUE(m_regex.compile(pattern, input.metacharacters, false).empty());
}

TEST_P(RegexMatchingTest, matchesOnlyTheConfiguredPatternAndInput)
{
    const bool matched = m_regex.execute(std::string_view{m_text});

    EXPECT_EQ(GetParam().expected, matched);
}

INSTANTIATE_TEST_SUITE_P(LiteralPatterns, RegexMatchingTest,
                         testing::Values(RegexMatchCase{"LiteralMatch", "needle", 6, false, "haystack needle", true},
                                         RegexMatchCase{"LiteralMiss", "needle", 6, false, "haystack", false}));

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

INSTANTIATE_TEST_SUITE_P(BoundedPatterns, RegexMatchingTest,
                         testing::Values(RegexMatchCase{"BoundedMatch", "[a]x", 3, true, "a", true},
                                         RegexMatchCase{"BoundedMiss", "[a]x", 3, true, "x", false}));

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
