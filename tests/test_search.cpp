// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/search.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

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

struct RegexFindCase
{
    const char      *name;
    std::string_view pattern;
    std::string_view text;
    std::size_t      offset;
    std::size_t      length{std::string_view::npos};
};

void PrintTo(const RegexFindCase &value, std::ostream *out)
{
    *out << value.name;
}

class RegexFindTest : public CompiledRegexTest, public testing::WithParamInterface<RegexFindCase>
{
protected:
    void SetUp() override;
};

void RegexFindTest::SetUp()
{
    CompiledRegexTest::SetUp();
    ASSERT_TRUE(m_regex.compile(GetParam().pattern, true, false).empty());
}

TEST_P(RegexFindTest, returnsTheByteOffsetOfTheFirstMatchWithinTheView)
{
    const RegexFindCase &input = GetParam();

    const std::size_t offset = m_regex.find(input.text.substr(0, input.length));

    EXPECT_EQ(input.offset, offset);
}

INSTANTIATE_TEST_SUITE_P(MatchPositions, RegexFindTest,
                         testing::Values(RegexFindCase{"LiteralAtStart", "needle", "needle suffix", 0},
                                         RegexFindCase{"LiteralAfterPrefix", "needle", "prefix needle", 7},
                                         RegexFindCase{"FirstRepeatedMatch", "needle", "a needle and needle", 2},
                                         RegexFindCase{"ClassAtStart", "[n]eedle", "needle suffix", 0},
                                         RegexFindCase{"ClassAfterPrefix", "[n]eedle", "prefix needle", 7},
                                         RegexFindCase{"SecondAlternative", "missing\\|needle", "prefix needle", 7},
                                         RegexFindCase{"ZeroWidthStart", "^", "needle", 0},
                                         RegexFindCase{"MissingLiteral", "needle", "haystack", std::string_view::npos},
                                         RegexFindCase{"MissingClass", "[n]eedle", "haystack", std::string_view::npos},
                                         RegexFindCase{"BoundedInput", "needle", "a needle", std::string_view::npos, 3},
                                         RegexFindCase{"Utf8ByteOffset", "needle", "\345\244\251e\314\201 needle", 7}),
                         [](const testing::TestParamInfo<RegexFindCase> &info) { return info.param.name; });

TEST_F(CompiledRegexTest, findingWithoutAnExpressionReturnsNoMatch)
{
    const std::size_t offset = m_regex.find("needle");

    EXPECT_EQ(std::string_view::npos, offset);
}

struct RegexMatchCase
{
    const char      *name;
    std::string_view pattern;
    std::size_t      pattern_length;
    bool             metacharacters;
    std::string_view text;
    bool             expected;
    std::size_t      text_length{std::string_view::npos};
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
    const std::string_view text = std::string_view{m_text}.substr(0, GetParam().text_length);

    const bool matched = m_regex.execute(text);

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

class MatchedBracketRegexTest : public CompiledRegexTest
{
protected:
    void SetUp() override;

    const std::string m_pattern{R"(prefix \(needle\) suffix)"};
    const std::string m_text{"prefix needle suffix"};
};

void MatchedBracketRegexTest::SetUp()
{
    CompiledRegexTest::SetUp();
    ASSERT_TRUE(m_regex.compile(m_pattern, true, false).empty());
    ASSERT_TRUE(m_regex.execute(std::string_view{m_text}));
}

TEST_F(MatchedBracketRegexTest, reportsThePresenceOfCaptureBrackets)
{
    EXPECT_TRUE(m_regex.has_brackets());
}

using BracketLookupCase = std::pair<int, std::string_view>;

class BracketLookupTest : public MatchedBracketRegexTest, public testing::WithParamInterface<BracketLookupCase>
{
};

TEST_P(BracketLookupTest, returnsTheRequestedCaptureOrAnEmptyView)
{
    const std::string_view capture = m_regex.get_bracket(GetParam().first);

    EXPECT_EQ(GetParam().second, capture);
}

INSTANTIATE_TEST_SUITE_P(ValidAndMissingCaptures, BracketLookupTest,
                         testing::Values(BracketLookupCase{1, "needle"}, BracketLookupCase{2, ""}));

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

INSTANTIATE_TEST_SUITE_P(
    BoundedAndFullText, RegexMatchingTest,
    testing::Values(RegexMatchCase{"BoundedTextMiss", "needle", 6, false, "haystack needle", false, 8},
                    RegexMatchCase{"FullTextMatch", "needle", 6, false, "haystack needle", true, 15}));

} // namespace
