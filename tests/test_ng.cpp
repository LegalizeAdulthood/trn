// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/head.h>
#include <trn/ng-internal.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
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
    ArticleNum               m_old_curr_art{g_curr_art};
    Article                 *m_old_curr_artp{g_curr_artp};
    ArticleNum               m_old_recent_art{g_recent_art};
    Article                 *m_old_recent_artp{g_recent_artp};
    ArticleNum               m_old_parsed_art{g_parsed_art};
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
    ArticleUnread            m_old_selected_count{g_selected_count};
    decltype(g_sel_mask)     m_old_sel_mask{g_sel_mask};
    bool                     m_old_reread{g_reread};
    bool                     m_old_force_last{g_force_last};
    bool                     m_old_verbose{g_verbose};
    int                      m_old_term_line{g_term_line};
    int                      m_old_term_col{g_term_col};
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
    g_selected_count = ArticleUnread{};
    g_sel_mask = AGF_SEL;
    g_parsed_art = ArticleNum{};
    g_recent_art = ArticleNum{-1};
    g_recent_artp = nullptr;
    g_reread = false;
    g_force_last = false;
    g_verbose = false;
    g_term_line = 0;
    g_term_col = 0;
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
    g_curr_art = m_old_curr_art;
    g_curr_artp = m_old_curr_artp;
    g_recent_art = m_old_recent_art;
    g_recent_artp = m_old_recent_artp;
    g_parsed_art = m_old_parsed_art;
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
    g_selected_count = m_old_selected_count;
    g_sel_mask = m_old_sel_mask;
    g_reread = m_old_reread;
    g_force_last = m_old_force_last;
    g_verbose = m_old_verbose;
    g_term_line = m_old_term_line;
    g_term_col = m_old_term_col;
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
        if (flag & AF_SEL)
        {
            ++g_selected_count;
        }
        previous = article;
    }
    g_last_art = ArticleNum{number - 1};
    g_last_cached = g_last_art;
    g_art = ArticleNum{current};
    g_artp = g_art <= g_last_art ? article_ptr(g_art) : nullptr;
    g_curr_art = g_art;
    g_curr_artp = g_artp;
}

class NextUnreadArticleTest : public ArticleNavigationTest
{
};

class NextArticleTest : public ArticleNavigationTest
{
};

class PreviousUnreadArticleTest : public ArticleNavigationTest
{
};

class PreviousArticleTest : public ArticleNavigationTest
{
};

class LastVisitedArticleTest : public ArticleNavigationTest
{
};

class SubjectThreadArticleTest : public ArticleNavigationTest
{
protected:
    void set_subjects(std::initializer_list<ArticleFlags> flags, long current);

private:
    Subject m_subjects[4]{};
};

void SubjectThreadArticleTest::set_subjects(std::initializer_list<ArticleFlags> flags, long current)
{
    set_articles(flags, current);
    g_sel_mode = GetParam() ? SM_THREAD : SM_SUBJECT;
    g_first_subject = &m_subjects[0];
    g_last_subject = &m_subjects[3];
    for (long index = 0; index < 4; ++index)
    {
        Subject &subject = m_subjects[index];
        subject.m_prev = index ? &m_subjects[index - 1] : nullptr;
        subject.m_next = index < 3 ? &m_subjects[index + 1] : nullptr;
        subject.m_articles = article_ptr(ArticleNum{2 * index + 1});
        subject.m_thread = article_ptr(ArticleNum{4 * (index / 2) + 1});
        subject.m_thread_link = &m_subjects[index ^ 1];
        subject.m_flags = SF_VISIT;
        for (long offset = 0; offset < 2; ++offset)
        {
            const long number = 2 * index + offset + 1;
            Article   *article = article_ptr(ArticleNum{number});
            article->m_subj = &subject;
            article->m_subj_next = offset ? nullptr : article_ptr(ArticleNum{number + 1});
            article->m_parent = number % 4 == 1 ? nullptr : article_ptr(ArticleNum{number - 1});
            article->m_child1 = number % 4 ? article_ptr(ArticleNum{number + 1}) : nullptr;
        }
    }
}

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

TEST_P(PreviousUnreadArticleTest, skipsReadAndMissingArticles)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_UNREAD, AF_EXISTS, AF_EXISTS}, 4);
    g_parsed_art = ArticleNum{1};

    EXPECT_EQ(AS_NORM, art_switch("p"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
}

TEST_P(PreviousUnreadArticleTest, skipsUnselectedUnreadArticlesInSelectedOnlyMode)
{
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS}, 3);
    g_selected_only = true;
    g_parsed_art = ArticleNum{1};

    EXPECT_EQ(AS_NORM, art_switch("p"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_FALSE(g_reread);
}

TEST_P(PreviousUnreadArticleTest, findsLastUnreadArticleFromEndOfGroup)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS}, 4);
    g_parsed_art = ArticleNum{2};

    EXPECT_EQ(AS_NORM, art_switch("p"));

    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
}

TEST_P(PreviousUnreadArticleTest, preservesCurrentArticleAtBeginning)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 1);
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("p");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find("No previous unread articles"));
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_P(PreviousUnreadArticleTest, preservesCurrentArticleWhenNoEarlierSelectedUnreadArticleExists)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL}, 3);
    g_selected_only = true;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("p");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find("No previous unread selected articles"));
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, PreviousUnreadArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Threaded" : "Numeric"; });

TEST_P(PreviousArticleTest, includesReadUnselectedArticlesBeforeFirstUnread)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL}, 3);
    g_first_art = ArticleNum{3};
    g_selected_only = true;

    EXPECT_EQ(AS_NORM, art_switch("P"));

    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
}

TEST_P(PreviousArticleTest, skipsMissingArticlesBeforeReadArticle)
{
    set_articles({AF_EXISTS, AF_UNREAD, AF_EXISTS}, 3);

    EXPECT_EQ(AS_NORM, art_switch("P"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
}

TEST_P(PreviousArticleTest, findsLastReadArticleFromEndOfGroup)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    g_first_art = ArticleNum{4};

    EXPECT_EQ(AS_NORM, art_switch("P"));

    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_artp);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
}

TEST_P(PreviousArticleTest, preservesFirstArticleWithVerboseWarning)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    g_verbose = true;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("P");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find("There are no articles prior to this one."));
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_P(PreviousArticleTest, preservesCurrentArticleWhenAllEarlierArticlesAreMissing)
{
    set_articles({AF_UNREAD, AF_EXISTS}, 2);
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("P");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find("No previous articles"));
    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, PreviousArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Threaded" : "Numeric"; });

TEST_P(LastVisitedArticleTest, returnsToReadUnselectedArticleAndPreservesHistory)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL}, 3);
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    g_selected_only = true;

    EXPECT_EQ(AS_NORM, art_switch("-"));

    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(ArticleNum{3}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_curr_artp);
    EXPECT_EQ(ArticleNum{1}, g_recent_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_recent_artp);
    EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{2})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_curr_artp->m_flags);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
}

TEST_P(LastVisitedArticleTest, returnsToEndOfGroupAndRetainsSearchAheadMode)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    g_recent_art = ArticleNum{3};
    g_search_ahead = ArticleNum{2};

    EXPECT_EQ(AS_NORM, art_switch("-"));

    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_curr_artp);
    EXPECT_EQ(ArticleNum{3}, g_recent_art);
    EXPECT_EQ(nullptr, g_recent_artp);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

TEST_P(LastVisitedArticleTest, acceptsBackspaceAndDeleteAliases)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 2);
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    for (const char *command : {"\b", "\177"})
    {
        SCOPED_TRACE(static_cast<int>(command[0]));
        g_art = g_curr_art;
        g_artp = g_curr_artp;
        g_reread = false;
        g_force_last = false;

        EXPECT_EQ(AS_NORM, art_switch(command));

        EXPECT_EQ(ArticleNum{1}, g_art);
        EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
        EXPECT_EQ(ArticleNum{2}, g_curr_art);
        EXPECT_EQ(article_ptr(ArticleNum{2}), g_curr_artp);
        EXPECT_EQ(ArticleNum{1}, g_recent_art);
        EXPECT_EQ(article_ptr(ArticleNum{1}), g_recent_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_TRUE(g_reread);
        EXPECT_TRUE(g_force_last);
    }
}

TEST_P(LastVisitedArticleTest, returnsToVirtualArticleUsingSavedPointer)
{
    set_articles({AF_EXISTS}, 1);
    Article virtual_article{};
    virtual_article.m_flags = AF_TMP_MEM;
    g_recent_art = ArticleNum{};
    g_recent_artp = &virtual_article;
    g_search_ahead = ArticleNum{-1};

    EXPECT_EQ(AS_NORM, art_switch("-"));

    EXPECT_EQ(ArticleNum{}, g_art);
    EXPECT_EQ(&virtual_article, g_artp);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_curr_artp);
    EXPECT_EQ(ArticleNum{}, g_recent_art);
    EXPECT_EQ(&virtual_article, g_recent_artp);
    EXPECT_EQ(AF_TMP_MEM, virtual_article.m_flags);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

TEST_P(LastVisitedArticleTest, requestsGroupCleanupForNegativeHistoryWithoutMoving)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD}, 2);
    g_search_ahead = ArticleNum{2};

    EXPECT_EQ(AS_CLEAN, art_switch("-"));

    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(ArticleNum{2}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_curr_artp);
    EXPECT_EQ(ArticleNum{-1}, g_recent_art);
    EXPECT_EQ(nullptr, g_recent_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleNum{2}, g_search_ahead);
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, LastVisitedArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Threaded" : "Numeric"; });

TEST_P(SubjectThreadArticleTest, nextSkipsTheCurrentSubjectOrThread)
{
    set_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD},
                 1);
    const ArticleNum expected{GetParam() ? 6L : 3L};

    EXPECT_EQ(AS_NORM, art_switch(">"));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_curr_artp);
    EXPECT_EQ(ArticleNum{-1}, g_recent_art);
    EXPECT_EQ(nullptr, g_recent_artp);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(GetParam(), g_threaded_group);
}

TEST_P(SubjectThreadArticleTest, previousSkipsTheCurrentSubjectOrThread)
{
    set_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD},
                 8);
    const ArticleNum expected{GetParam() ? 2L : 6L};

    EXPECT_EQ(AS_NORM, art_switch("<"));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleNum{8}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{8}), g_curr_artp);
    EXPECT_EQ(ArticleNum{-1}, g_recent_art);
    EXPECT_EQ(nullptr, g_recent_artp);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(GetParam(), g_threaded_group);
}

TEST_P(SubjectThreadArticleTest, nextFindsSelectedUnreadArticleWithoutChangingSelection)
{
    set_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                 1);
    g_selected_only = true;
    const ArticleNum expected{GetParam() ? 7L : 4L};

    EXPECT_EQ(AS_NORM, art_switch(">"));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_artp->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{6})->m_flags);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(GetParam(), g_threaded_group);
}

TEST_P(SubjectThreadArticleTest, nextSkipsMissingArticlesAndFallsBackToReadArticle)
{
    set_subjects({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_UNREAD, AF_UNREAD, AF_UNREAD, AF_UNREAD, AF_EXISTS,
                  AF_EXISTS},
                 1);

    EXPECT_EQ(AS_NORM, art_switch(">"));

    EXPECT_EQ(ArticleNum{7}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{7}), g_artp);
    EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{5})->m_flags);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{6})->m_flags);
    EXPECT_EQ(GetParam() ? AF_UNREAD : AF_NONE, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(GetParam() ? AF_UNREAD : AF_NONE, article_ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(ArticleUnread{GetParam() ? 4L : 2L}, m_newsgroup.m_to_read);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(GetParam(), g_threaded_group);
}

TEST_P(SubjectThreadArticleTest, entersFromEndAndStopsAtSubjectOrThreadBoundaries)
{
    set_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD},
                 9);
    struct NavigationCase
    {
        const char *command;
        long        current;
        long        expected;
        bool        force_last;
    };
    for (const NavigationCase test_case :
         {NavigationCase{">", 9, 2, false}, NavigationCase{"<", 9, GetParam() ? 6L : 8L, false},
          NavigationCase{">", 8, 9, true}, NavigationCase{"<", 1, 9, true}})
    {
        SCOPED_TRACE(testing::Message() << test_case.command << " from " << test_case.current);
        g_art = ArticleNum{test_case.current};
        g_artp = test_case.current <= 8 ? article_ptr(g_art) : nullptr;
        g_curr_art = g_art;
        g_curr_artp = g_artp;
        g_reread = false;
        g_force_last = false;

        EXPECT_EQ(AS_NORM, art_switch(test_case.command));

        EXPECT_EQ(ArticleNum{test_case.expected}, g_art);
        EXPECT_EQ(test_case.expected <= 8 ? article_ptr(ArticleNum{test_case.expected}) : nullptr, g_artp);
        EXPECT_EQ(ArticleNum{test_case.current}, g_curr_art);
        EXPECT_EQ(test_case.current <= 8 ? article_ptr(ArticleNum{test_case.current}) : nullptr, g_curr_artp);
        EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
        EXPECT_FALSE(g_reread);
        EXPECT_EQ(test_case.force_last, g_force_last);
        EXPECT_EQ(GetParam(), g_threaded_group);
    }
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, SubjectThreadArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Thread" : "Subject"; });

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
