// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/addng-internal.h>

#include <trn/only.h>

#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <string>
#include <string_view>

namespace
{

class ActiveListPatternTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsgroup_to_do = g_newsgroup_to_do;
        m_old_max_newsgroup_to_do = g_max_newsgroup_to_do;

        g_newsgroup_to_do.fill(std::string{});
        g_max_newsgroup_to_do = 0;
    }

    void TearDown() override
    {
        g_newsgroup_to_do = m_old_newsgroup_to_do;
        g_max_newsgroup_to_do = m_old_max_newsgroup_to_do;
    }

    void set_restrictions(std::initializer_list<std::string_view> restrictions)
    {
        g_max_newsgroup_to_do = 0;
        for (std::string_view restriction : restrictions)
        {
            g_newsgroup_to_do[g_max_newsgroup_to_do] = restriction;
            ++g_max_newsgroup_to_do;
        }
    }

    std::array<std::string, MAX_NG_TO_DO> m_old_newsgroup_to_do;
    int                                   m_old_max_newsgroup_to_do{};
};

} // namespace

TEST_F(ActiveListPatternTest, unrestrictedScanRequestsAllGroups)
{
    EXPECT_EQ("*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, multipleRestrictionsRequestAllGroups)
{
    set_restrictions({"comp", "news"});

    EXPECT_EQ("*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, singleRestrictionMatchesSubstring)
{
    set_restrictions({"comp.lang"});

    EXPECT_EQ("*comp.lang*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, leadingCaretMatchesPrefix)
{
    set_restrictions({"^comp.lang"});

    EXPECT_EQ("comp.lang*", active_list_pattern());
}

TEST_F(ActiveListPatternTest, trailingDollarRemovesGeneratedSuffix)
{
    set_restrictions({"comp.lang$"});

    EXPECT_EQ("*comp.lang", active_list_pattern());
}

TEST_F(ActiveListPatternTest, leadingCaretWithTrailingDollarMatchesExactPrefix)
{
    set_restrictions({"^comp.lang$"});

    EXPECT_EQ("comp.lang", active_list_pattern());
}
