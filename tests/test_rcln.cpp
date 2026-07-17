// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngdata.h>
#include <trn/rcstuff.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

namespace
{

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

} // namespace

TEST_F(ExpiredArticleTest, extendsReadRangeToFirstAvailableArticle)
{
    set_numbers("2-20,25");

    m_group.check_expired(ArticleNum{10});

    EXPECT_EQ("comp.lang.apl: 1-20,25", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(ExpiredArticleTest, removesExpiredRangesAndKeepsUnreadSuffix)
{
    set_numbers("1-5,10-20");

    m_group.check_expired(ArticleNum{8});

    EXPECT_EQ("comp.lang.apl: 1-7,10-20", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}
