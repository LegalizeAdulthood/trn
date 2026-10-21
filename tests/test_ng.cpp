// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/ng-internal.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/univ.h>

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <utility>

namespace
{

class ArticleNavigationTest : public testing::TestWithParam<bool>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_articles(std::initializer_list<ArticleFlags> flags, long current);

    NewsgroupData m_newsgroup{};

private:
    Subject                  m_subject{};
    decltype(g_article_list) m_old_article_list{std::move(g_article_list)};
    decltype(g_art_ptr_list) m_old_art_ptr_list{std::move(g_art_ptr_list)};
    Article                **m_old_art_ptr{g_art_ptr};
    ArticleNum               m_old_art{g_art};
    Article                 *m_old_artp{g_artp};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first_art{g_first_art};
    ArticleNum               m_old_last_art{g_last_art};
    ArticleNum               m_old_first_cached{g_first_cached};
    ArticleNum               m_old_last_cached{g_last_cached};
    ArticleNum               m_old_search_ahead{g_search_ahead};
    Subject                 *m_old_first_subject{g_first_subject};
    Subject                 *m_old_last_subject{g_last_subject};
    NewsgroupData           *m_old_newsgroup_ptr{g_newsgroup_ptr};
    bool                     m_old_threaded_group{g_threaded_group};
    bool                     m_old_breadth_first{g_breadth_first};
    SelectionMode            m_old_sel_mode{g_sel_mode};
    bool                     m_old_selected_only{g_selected_only};
    bool                     m_old_reread{g_reread};
    bool                     m_old_force_last{g_force_last};
    int                      m_old_scan_on{g_scan_on};
    bool                     m_old_sa_in{g_sa_in};
    bool                     m_old_univ_read_virt_flag{g_univ_read_virt_flag};
    bool                     m_old_s_default_cmd{g_s_default_cmd};
    bool                     m_old_univ_default_cmd{g_univ_default_cmd};
    bool                     m_old_s_follow_temp{g_s_follow_temp};
    bool                     m_old_univ_follow_temp{g_univ_follow_temp};
};

void ArticleNavigationTest::SetUp()
{
    g_article_list.clear();
    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_abs_first = ArticleNum{1};
    g_first_art = ArticleNum{1};
    g_first_cached = ArticleNum{1};
    g_search_ahead = ArticleNum{};
    g_first_subject = &m_subject;
    g_last_subject = &m_subject;
    g_newsgroup_ptr = &m_newsgroup;
    g_threaded_group = GetParam();
    g_breadth_first = false;
    g_sel_mode = SM_ARTICLE;
    g_selected_only = false;
    g_reread = false;
    g_force_last = false;
    g_scan_on = 0;
    g_sa_in = false;
    g_univ_read_virt_flag = false;
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    g_s_follow_temp = false;
    g_univ_follow_temp = false;
}

void ArticleNavigationTest::TearDown()
{
    g_article_list = std::move(m_old_article_list);
    g_art_ptr_list = std::move(m_old_art_ptr_list);
    g_art_ptr = m_old_art_ptr;
    g_art = m_old_art;
    g_artp = m_old_artp;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first_art;
    g_last_art = m_old_last_art;
    g_first_cached = m_old_first_cached;
    g_last_cached = m_old_last_cached;
    g_search_ahead = m_old_search_ahead;
    g_first_subject = m_old_first_subject;
    g_last_subject = m_old_last_subject;
    g_newsgroup_ptr = m_old_newsgroup_ptr;
    g_threaded_group = m_old_threaded_group;
    g_breadth_first = m_old_breadth_first;
    g_sel_mode = m_old_sel_mode;
    g_selected_only = m_old_selected_only;
    g_reread = m_old_reread;
    g_force_last = m_old_force_last;
    g_scan_on = m_old_scan_on;
    g_sa_in = m_old_sa_in;
    g_univ_read_virt_flag = m_old_univ_read_virt_flag;
    g_s_default_cmd = m_old_s_default_cmd;
    g_univ_default_cmd = m_old_univ_default_cmd;
    g_s_follow_temp = m_old_s_follow_temp;
    g_univ_follow_temp = m_old_univ_follow_temp;
}

void ArticleNavigationTest::set_articles(std::initializer_list<ArticleFlags> flags, long current)
{
    Article *previous = nullptr;
    long     number = 1;
    for (const ArticleFlags flag : flags)
    {
        Article *article = article_ptr(ArticleNum{number++});
        article->m_flags = flag;
        article->m_subj = &m_subject;
        article->m_parent = previous;
        if (previous)
        {
            previous->m_child1 = article;
            previous->m_subj_next = article;
        }
        else
        {
            m_subject.m_articles = article;
            m_subject.m_thread = article;
        }
        if (flag & AF_UNREAD)
        {
            ++m_newsgroup.m_to_read;
            m_subject.m_flags = SF_VISIT;
        }
        previous = article;
    }
    g_last_art = ArticleNum{number - 1};
    g_last_cached = g_last_art;
    g_art = ArticleNum{current};
    g_artp = g_art <= g_last_art ? article_ptr(g_art) : nullptr;
}

class NextUnreadArticleTest : public ArticleNavigationTest
{
};

class NextArticleTest : public ArticleNavigationTest
{
};

} // namespace

TEST_P(NextUnreadArticleTest, skipsReadAndMissingArticles)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_UNREAD, AF_EXISTS | AF_UNREAD}, 1);

    EXPECT_EQ(AS_NORM, art_switch("n"));

    EXPECT_EQ(ArticleNum{4}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{4}), g_artp);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
}

TEST_P(NextUnreadArticleTest, wrapsToEarlierUnreadArticleAfterExhaustion)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS}, 2);

    EXPECT_EQ(AS_NORM, art_switch("n"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
}

TEST_P(NextUnreadArticleTest, remainsAfterLastArticleWhenNoUnreadRemain)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 3);

    EXPECT_EQ(AS_NORM, art_switch("n"));

    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
}

TEST_P(NextUnreadArticleTest, leavesExhaustedGroupWhenAlreadyAfterLastArticle)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);

    EXPECT_EQ(AS_CLEAN, art_switch("n"));

    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
}

TEST_P(NextUnreadArticleTest, restartsAtUnreadArticleWhenAlreadyAfterLastArticle)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS}, 4);

    EXPECT_EQ(AS_NORM, art_switch("n"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, NextUnreadArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Threaded" : "Numeric"; });

TEST_P(NextArticleTest, includesReadArticles)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);

    EXPECT_EQ(AS_NORM, art_switch("N"));

    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
}

TEST_P(NextArticleTest, skipsMissingArticlesBeforeReadArticle)
{
    set_articles({AF_EXISTS, AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);

    EXPECT_EQ(AS_NORM, art_switch("N"));

    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_artp);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
}

TEST_P(NextArticleTest, stopsAfterFinalArticleWithoutWrapping)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 2);

    EXPECT_EQ(AS_NORM, art_switch("N"));

    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_force_last);
}

TEST_P(NextArticleTest, restartsAtFirstArticleIncludingReadArticles)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS}, 4);
    g_first_art = ArticleNum{2};
    if (!GetParam())
    {
        g_first_subject = nullptr;
        g_last_subject = nullptr;
    }

    EXPECT_EQ(AS_NORM, art_switch("N"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
}

TEST_P(NextArticleTest, skipsMissingFirstCachedArticleWhenRestarting)
{
    set_articles({AF_NONE, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 4);

    EXPECT_EQ(AS_NORM, art_switch("N"));

    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, NextArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Threaded" : "Numeric"; });

TEST(NewsgroupUnreadPromptTest, usesGroupPromptWithoutCurrentArticle)
{
    EXPECT_EQ("\nUnkill: +select or all?", std::string{ng_unread_prompt_for_test(false, true)});
    EXPECT_EQ("\nUnkill?", std::string{ng_unread_prompt_for_test(false, false)});
}

TEST(NewsgroupUnreadPromptTest, omitsThreadHelpWithoutCurrentArticle)
{
    EXPECT_TRUE(ng_unread_thread_help_for_test(false, true).empty());
    EXPECT_TRUE(ng_unread_thread_help_for_test(false, false).empty());
}

TEST(NewsgroupUnreadPromptTest, usesArticlePromptWithCurrentArticle)
{
    EXPECT_EQ("\nUnkill: +select, thread, subthread, or all?", std::string{ng_unread_prompt_for_test(true, true)});
    EXPECT_EQ("\nUnkill?", std::string{ng_unread_prompt_for_test(true, false)});
}

TEST(NewsgroupUnreadPromptTest, usesThreadHelpWithCurrentArticle)
{
    EXPECT_EQ("Type t or SP to mark this thread's articles as unread.\n"
              "Type s to mark the current article and its descendants as unread.\n",
              std::string{ng_unread_thread_help_for_test(true, true)});
    EXPECT_EQ("t or SP to mark thread unread.\n"
              "s to mark subthread unread.\n",
              std::string{ng_unread_thread_help_for_test(true, false)});
}

TEST(CatchupLeaveUnreadCountTest, parsesNumericPrefix)
{
    EXPECT_EQ(3, ng_catchup_leave_unread_count_for_test("3abc"));
}

TEST(CatchupLeaveUnreadCountTest, returnsZeroWithoutNumericPrefix)
{
    EXPECT_EQ(0, ng_catchup_leave_unread_count_for_test("abc"));
}
