// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/datasrc.h>
#include <trn/hash.h>
#include <trn/ngdata.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace
{

int compare_newsgroup_name(std::string_view key, HashDatum data)
{
    const NewsgroupData *group = reinterpret_cast<NewsgroupData *>(data.dat_ptr);

    return key.compare(group->rc_name());
}

class ExpiredArticleTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_newsrc.flags = RF_NONE;
        m_group.m_rc = &m_newsrc;
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.m_subscribe_char = ':';
    }

    void set_numbers(const std::string &numbers)
    {
        m_group.m_rc_line = "comp.lang.apl: " + numbers;
        m_group.hide_subscribe_char();
    }

    std::string visible_rc_line() const
    {
        std::string line = m_group.m_rc_line;
        line[static_cast<std::size_t>(m_group.m_num_offset - 1)] = m_group.m_subscribe_char;
        return line;
    }

    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
};

class AddArtNumTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsrc_hash = g_newsrc_hash;
        g_newsrc_hash = hash_create(17, compare_newsgroup_name);

        m_newsrc.flags = RF_NONE;
        m_newsrc.data_source = &m_data_source;
        m_group.m_rc = &m_newsrc;
        m_group.m_abs_first = ArticleNum{1};
        m_group.m_ng_max = ArticleNum{20};
        m_group.m_to_read = ArticleUnread{10};
        m_group.m_subscribe_char = ':';
    }

    void TearDown() override
    {
        hash_destroy(g_newsrc_hash);
        g_newsrc_hash = m_old_newsrc_hash;
    }

    void set_numbers(const std::string &numbers)
    {
        m_group.m_rc_line = "comp.lang.apl: " + numbers;
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.hide_subscribe_char();

        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&m_group);
        data.dat_len = static_cast<unsigned>(m_group.rc_name().size());
        hash_store(g_newsrc_hash, m_group.rc_name(), data);
    }

    void add_article(ArticleNum art_num)
    {
        add_art_num(&m_data_source, art_num, "comp.lang.apl");
    }

    std::string visible_rc_line() const
    {
        std::string line = m_group.m_rc_line;
        line[static_cast<std::size_t>(m_group.m_num_offset - 1)] = m_group.m_subscribe_char;
        return line;
    }

    HashTable    *m_old_newsrc_hash{};
    DataSource    m_data_source{};
    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
};

} // namespace

TEST_F(ExpiredArticleTest, extendsReadRangeToFirstAvailableArticle)
{
    set_numbers("2-20,25");

    m_group.check_expired(ArticleNum{10});

    EXPECT_EQ("comp.lang.apl: 1-20,25", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(AddArtNumTest, insertsStandaloneArticleBeforeLaterRange)
{
    set_numbers("1,5-7");

    add_article(ArticleNum{3});

    EXPECT_EQ("comp.lang.apl: 1,3,5-7", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, extendsPriorRange)
{
    set_numbers("1-3,6");

    add_article(ArticleNum{4});

    EXPECT_EQ("comp.lang.apl: 1-4,6", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, mergesAdjacentSingletons)
{
    set_numbers("1,3");

    add_article(ArticleNum{2});

    EXPECT_EQ("comp.lang.apl: 1-3", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, bridgesAdjacentRanges)
{
    set_numbers("1-3,5-7");

    add_article(ArticleNum{4});

    EXPECT_EQ("comp.lang.apl: 1-7", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(ExpiredArticleTest, removesExpiredRangesAndKeepsUnreadSuffix)
{
    set_numbers("1-5,10-20");

    m_group.check_expired(ArticleNum{8});

    EXPECT_EQ("comp.lang.apl: 1-7,10-20", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}
