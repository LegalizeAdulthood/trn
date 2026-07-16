// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/addng-internal.h>

#include <trn/datasrc.h>
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

class AddGroupStorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_first = g_first_add_group;
        m_old_last = g_last_add_group;
        m_old_data_source = g_data_source;
        g_first_add_group = nullptr;
        g_last_add_group = nullptr;
        g_data_source = nullptr;
    }

    void TearDown() override
    {
        for (AddGroup *group = g_first_add_group; group != nullptr;)
        {
            AddGroup *next = group->m_next;
            delete group;
            group = next;
        }
        g_first_add_group = m_old_first;
        g_last_add_group = m_old_last;
        g_data_source = m_old_data_source;
    }

    AddGroup   *m_old_first{};
    AddGroup   *m_old_last{};
    DataSource *m_old_data_source{};
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

TEST_F(AddGroupStorageTest, storesNamesAndLinksWithoutAddingDuplicates)
{
    add_to_list("comp.lang.c++", 42, ':');
    add_to_list("news.software.readers", -1, '!');
    add_to_list("comp.lang.c++", 99, '!');

    ASSERT_NE(nullptr, g_first_add_group);
    ASSERT_NE(nullptr, g_last_add_group);
    EXPECT_EQ(g_last_add_group, g_first_add_group->m_next);
    EXPECT_EQ(g_first_add_group, g_last_add_group->m_prev);
    EXPECT_EQ(nullptr, g_first_add_group->m_prev);
    EXPECT_EQ(nullptr, g_last_add_group->m_next);
    EXPECT_EQ("comp.lang.c++", std::string_view{g_first_add_group->m_name});
    EXPECT_EQ("news.software.readers", std::string_view{g_last_add_group->m_name});
    EXPECT_EQ(42, g_first_add_group->m_to_read.value_of());
    EXPECT_EQ(0, g_last_add_group->m_to_read.value_of());
    EXPECT_EQ(AGF_SEL, g_first_add_group->m_flags);
    EXPECT_EQ(AGF_DEL, g_last_add_group->m_flags);
}
