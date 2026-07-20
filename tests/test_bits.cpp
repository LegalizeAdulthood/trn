// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/bits.h>

#include <config/common.h>
#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <map>
#include <string>
#include <utility>

namespace
{

class BitsToRcTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_article_list = std::move(g_article_list);
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;

        g_article_list.clear();
        g_newsgroup_ptr = &m_group;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{7};

        m_newsrc.flags = RF_NONE;
        m_group.m_rc = &m_newsrc;
        set_rc_line("comp.lang.apl: old");
        m_group.m_to_read = ArticleUnread{};
    }

    void TearDown() override
    {
        g_article_list = std::move(m_old_article_list);
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
    }

    void add_article(long num, bool unread)
    {
        Article *article = article_ptr(ArticleNum{num});
        article->m_flags = AF_EXISTS;
        if (unread)
        {
            article->m_flags |= AF_UNREAD;
        }
    }

    void add_existing_articles(long first, long last)
    {
        for (long num = first; num <= last; ++num)
        {
            add_article(num, false);
        }
    }

    void expect_unread(long num, bool unread)
    {
        SCOPED_TRACE(num);
        EXPECT_EQ(unread, article_unread(ArticleNum{num}));
    }

    void set_rc_line(std::string line)
    {
        m_group.m_rc_line = std::move(line);
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.m_subscribe_char = m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)];
        m_group.hide_subscribe_char();
    }

    std::string visible_rc_line() const
    {
        std::string line = m_group.m_rc_line;
        line[static_cast<std::size_t>(m_group.m_num_offset - 1)] = m_group.m_subscribe_char;
        return line;
    }

    Newsrc                        m_newsrc{};
    NewsgroupData                 m_group{};
    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
};

} // namespace

TEST_F(BitsToRcTest, decodesReadRangesIntoUnreadArticleFlags)
{
    set_rc_line("comp.lang.apl: 2,4-5");
    add_existing_articles(1, 7);

    rc_to_bits();

    expect_unread(1, true);
    expect_unread(2, false);
    expect_unread(3, true);
    expect_unread(4, false);
    expect_unread(5, false);
    expect_unread(6, true);
    expect_unread(7, true);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleUnread{4}, m_group.m_to_read);
}

TEST_F(BitsToRcTest, decodesLeadingReadRangeIntoFirstUnreadArticle)
{
    set_rc_line("comp.lang.apl: 1-2,4");
    add_existing_articles(1, 7);

    rc_to_bits();

    expect_unread(1, false);
    expect_unread(2, false);
    expect_unread(3, true);
    expect_unread(4, false);
    expect_unread(5, true);
    expect_unread(6, true);
    expect_unread(7, true);
    EXPECT_EQ(ArticleNum{3}, g_first_art);
    EXPECT_EQ(ArticleUnread{4}, m_group.m_to_read);
}

TEST_F(BitsToRcTest, setFirstArtSkipsLeadingReadRange)
{
    EXPECT_TRUE(set_first_art(" 1-2,4"));
    EXPECT_EQ(ArticleNum{3}, g_first_art);
}

TEST_F(BitsToRcTest, setFirstArtUsesAbsoluteFirstWithoutLeadingReadRange)
{
    g_abs_first = ArticleNum{4};

    EXPECT_FALSE(set_first_art("2,6"));
    EXPECT_EQ(ArticleNum{4}, g_first_art);
}

TEST_F(BitsToRcTest, reconstructsSubscribedLineFromReadRanges)
{
    add_article(1, false);
    add_article(2, false);
    add_article(3, true);
    add_article(4, false);
    add_article(5, false);
    add_article(6, true);
    add_article(7, true);

    bits_to_rc();

    EXPECT_EQ("comp.lang.apl: 1-2,4-5", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(ArticleUnread{3}, m_group.m_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(BitsToRcTest, reconstructsUnsubscribedLineAndKeepsItInvisible)
{
    m_group.m_subscribe_char = UNSUBSCRIBED_CHAR;

    add_article(1, false);
    add_article(2, false);
    add_article(3, true);
    add_article(4, false);
    add_article(5, false);
    add_article(6, true);
    add_article(7, true);

    bits_to_rc();

    EXPECT_EQ("comp.lang.apl! 1-2,4-5", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(TR_UNSUB, m_group.m_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}
