// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/rt-process.h>

#include <trn/Article.h>
#include <trn/hash.h>
#include <trn/kfile.h>
#include <trn/ngdata.h>
#include <trn/rthread.h>

#include <gtest/gtest.h>

#include <string>

namespace
{

class MessageIdHashTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_hash = g_msg_id_hash;
        m_old_abs_first = g_abs_first;
        m_old_change_thread_count = g_kf_change_thread_cnt;

        g_msg_id_hash = hash_create(17, msg_id_cmp);
        g_abs_first = ArticleNum{1};
        g_kf_change_thread_cnt = 0;
    }

    void TearDown() override
    {
        hash_destroy(g_msg_id_hash);
        g_msg_id_hash = m_old_hash;
        g_abs_first = m_old_abs_first;
        g_kf_change_thread_cnt = m_old_change_thread_count;
    }

private:
    HashTable *m_old_hash{};
    ArticleNum m_old_abs_first{};
    int        m_old_change_thread_count{};
};

} // namespace

TEST_F(MessageIdHashTest, getArticlePromotesPendingMessageIdToFakeArticle)
{
    HashDatum data = make_pending_msg_id("<case@example.com>", AUTO_SEL_THD | 1);
    hash_store(g_msg_id_hash, "<case@example.com>", data);

    char lookup[] = "<case@EXAMPLE.COM>";

    Article *article = get_article(lookup);

    ASSERT_NE(nullptr, article);
    EXPECT_STREQ("<case@EXAMPLE.COM>", lookup);
    ASSERT_TRUE(article->m_msg_id);
    EXPECT_EQ("<case@example.com>", *article->m_msg_id);
    EXPECT_TRUE((article->m_flags & AF_FAKE) != 0);
    EXPECT_TRUE((article->m_flags & AF_TMP_MEM) != 0);
    EXPECT_TRUE((article->m_auto_flags & AUTO_SEL_THD) != 0);
    EXPECT_EQ(1, g_kf_change_thread_cnt);

    const HashDatum stored = hash_fetch(g_msg_id_hash, "<case@example.com>");
    EXPECT_EQ(0U, stored.dat_len);
    EXPECT_EQ(article, reinterpret_cast<Article *>(stored.dat_ptr));

    article->clear_article();
    delete article;
}

TEST(MessageIdTest, fixMsgIdLowercasesOnlyDomain)
{
    const char  lookup[] = "<Case@EXAMPLE.COM>";
    std::string normalized = fix_msg_id(lookup);

    EXPECT_EQ("<Case@example.com>", normalized);
    EXPECT_STREQ("<Case@EXAMPLE.COM>", lookup);
}
