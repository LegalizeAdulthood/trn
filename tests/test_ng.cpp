// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artsrch.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/datasrc.h>
#include <trn/file-contents.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/mime.h>
#include <trn/ng-internal.h>
#include <trn/ngdata.h>
#include <trn/ngstuff.h>
#include <trn/nntp.h>
#include <trn/rcstuff.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <util/env.h>
#include <util/util2.h>

#include <test_config.h>

#include "mock_env.h"
#include "MockNNTPConnection.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

class ArticleStateTest : public testing::Test
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
    DoNewsgroupResult        m_old_newsgroup_exit_code{g_newsgroup_exit_code};
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

void ArticleStateTest::SetUp()
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
    g_newsgroup_exit_code = NG_NORM;
    g_threaded_group = true;
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

void ArticleStateTest::TearDown()
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
    g_newsgroup_exit_code = m_old_newsgroup_exit_code;
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

void ArticleStateTest::set_articles(std::initializer_list<ArticleFlags> flags, long current)
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

class ArticleNavigationTest : public ArticleStateTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void ArticleNavigationTest::SetUp()
{
    ArticleStateTest::SetUp();
    g_threaded_group = GetParam();
}

class UnreadScopeArticleTest : public ArticleStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_scope(std::initializer_list<ArticleFlags> flags, long current);
    std::string mark_unread(char scope, ArticleSwitchResult expected = AS_NORM);
    void expect_flags(std::initializer_list<ArticleFlags> flags);

    Subject m_subjects[3]{};

private:
    int         m_old_selected_subj_cnt{g_selected_subj_cnt};
    bool        m_old_sel_rereading{g_sel_rereading};
    std::string m_old_default_cmd{g_default_cmd};
    GeneralMode m_old_general_mode{g_general_mode};
    MinorMode   m_old_mode{g_mode};
    bool        m_old_allow_typeahead{g_allow_typeahead};
    bool        m_old_verify{g_verify};
    int         m_old_errno{errno};
    std::string m_old_typeahead;
};

void UnreadScopeArticleTest::SetUp()
{
    ArticleStateTest::SetUp();
    g_selected_subj_cnt = 0;
    g_sel_rereading = false;
    g_allow_typeahead = true;
    g_verify = false;
    errno = 0;
    while (macro_pending())
    {
        m_old_typeahead += read_tty_char();
    }
}

void UnreadScopeArticleTest::TearDown()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    for (auto ch = m_old_typeahead.rbegin(); ch != m_old_typeahead.rend(); ++ch)
    {
        push_char(*ch);
    }
    g_selected_subj_cnt = m_old_selected_subj_cnt;
    g_sel_rereading = m_old_sel_rereading;
    g_default_cmd = m_old_default_cmd;
    g_general_mode = m_old_general_mode;
    g_mode = m_old_mode;
    g_allow_typeahead = m_old_allow_typeahead;
    g_verify = m_old_verify;
    errno = m_old_errno;
    ArticleStateTest::TearDown();
}

void UnreadScopeArticleTest::set_scope(std::initializer_list<ArticleFlags> flags, long current)
{
    set_articles(flags, current);
    // First thread: 1 -> (2 -> 3 -> 4, 5). Second thread: 6 -> 7.
    struct ArticleLinks
    {
        long parent;
        long child;
        long sibling;
        long next_subject;
        int  subject;
    };
    const ArticleLinks links[]{{0, 2, 0, 2, 0}, {1, 3, 5, 0, 0}, {2, 4, 0, 4, 1}, {3, 0, 0, 5, 1},
                               {1, 0, 0, 0, 1}, {0, 7, 0, 7, 2}, {6, 0, 0, 0, 2}};
    const auto         article_at = [](long number) { return number ? article_ptr(ArticleNum{number}) : nullptr; };
    for (long number = 1; number <= 7; ++number)
    {
        Article            *article = article_at(number);
        const ArticleLinks &link = links[number - 1];
        article->m_parent = article_at(link.parent);
        article->m_child1 = article_at(link.child);
        article->m_sibling = article_at(link.sibling);
        article->m_subj_next = article_at(link.next_subject);
        article->m_subj = &m_subjects[link.subject];
        if (article->m_flags & AF_UNREAD)
        {
            article->m_subj->m_flags |= SF_VISIT;
        }
    }
    for (int index = 0; index < 3; ++index)
    {
        Subject &subject = m_subjects[index];
        subject.m_articles = article_at(index == 0 ? 1 : index == 1 ? 3 : 6);
        subject.m_thread = article_at(index < 2 ? 1 : 6);
        subject.m_thread_link = &m_subjects[index < 2 ? 1 - index : 2];
        subject.m_prev = index ? &m_subjects[index - 1] : nullptr;
        subject.m_next = index < 2 ? &m_subjects[index + 1] : nullptr;
    }
    g_first_subject = &m_subjects[0];
    g_last_subject = &m_subjects[2];
}

std::string UnreadScopeArticleTest::mark_unread(char scope, ArticleSwitchResult expected)
{
    push_char(scope);
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("U");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_NE(std::string::npos, output.find("Unkill"));
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(m_old_general_mode, g_general_mode);
    EXPECT_EQ(m_old_mode, g_mode);
    return output;
}

void UnreadScopeArticleTest::expect_flags(std::initializer_list<ArticleFlags> flags)
{
    long number = 1;
    for (const ArticleFlags expected : flags)
    {
        SCOPED_TRACE(number);
        EXPECT_EQ(expected, article_ptr(ArticleNum{number++})->m_flags);
    }
}

class ScopeSelectionArticleTest : public UnreadScopeArticleTest
{
protected:
    void        SetUp() override;
    void        set_selection_scope(std::initializer_list<ArticleFlags> flags, long current = 5);
    std::string change_scope(char action, long target, ArticleSwitchResult expected = AS_ASK);
};

void ScopeSelectionArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_sel_mode = SM_THREAD;
    set_selection_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
}

void ScopeSelectionArticleTest::set_selection_scope(std::initializer_list<ArticleFlags> flags, long current)
{
    g_article_list.clear();
    m_newsgroup.m_to_read = ArticleUnread{};
    g_selected_count = ArticleUnread{};
    g_selected_subj_cnt = 0;
    for (Subject &subject : m_subjects)
    {
        subject = Subject{};
    }
    set_scope(flags, current);
    for (Subject &subject : m_subjects)
    {
        for (Article *article = subject.m_articles; article; article = article->m_subj_next)
        {
            if (article->m_flags & AF_SEL)
            {
                subject.m_flags |= SF_SEL;
                ++g_selected_subj_cnt;
                break;
            }
        }
    }
    g_selected_only = g_selected_count != 0;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{3};
    g_search_ahead = ArticleNum{4};
    g_reread = true;
    g_force_last = true;
}

std::string ScopeSelectionArticleTest::change_scope(char action, long target, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleUnread unread = m_newsgroup.m_to_read;
    const ArticleNum    first = g_first_art;
    push_char('n');
    push_char(action);
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{'_', FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(ArticleNum{target}, g_art);
    EXPECT_EQ(target <= g_last_art.value_of() ? article_ptr(ArticleNum{target}) : nullptr, g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{3}, g_parsed_art);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('n', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

class SubjectThreadSelectionArticleTest : public ScopeSelectionArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SubjectThreadSelectionArticleTest::SetUp()
{
    ScopeSelectionArticleTest::SetUp();
    g_threaded_group = GetParam();
    g_sel_mode = GetParam() ? SM_THREAD : SM_SUBJECT;
}

class GroupEndScopeSelectionArticleTest : public ScopeSelectionArticleTest,
                                          public testing::WithParamInterface<std::tuple<bool, char>>
{
protected:
    void SetUp() override;
};

void GroupEndScopeSelectionArticleTest::SetUp()
{
    ScopeSelectionArticleTest::SetUp();
    g_threaded_group = std::get<0>(GetParam());
    g_sel_mode = g_threaded_group ? SM_THREAD : SM_SUBJECT;
    g_art = g_curr_art = ArticleNum{8};
    g_artp = g_curr_artp = nullptr;
}

class GroupUnreadArticleTest : public UnreadScopeArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    ArticleNum m_old_obj_count{g_obj_count};
    int        m_old_dm_count{g_dm_count};
};

void GroupUnreadArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_obj_count = ArticleNum{};
    g_dm_count = 0;
}

void GroupUnreadArticleTest::TearDown()
{
    g_obj_count = m_old_obj_count;
    g_dm_count = m_old_dm_count;
    UnreadScopeArticleTest::TearDown();
}

class ThreadKillArticleTest : public GroupUnreadArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_thread(long current);
    std::string kill_articles(std::string command, ArticleSwitchResult expected = AS_NORM);

private:
    bool m_old_sa_follow{g_sa_follow};
    int  m_old_check_count{g_check_count};
};

void ThreadKillArticleTest::SetUp()
{
    GroupUnreadArticleTest::SetUp();
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_sa_follow = false;
    g_check_count = 10;
}

void ThreadKillArticleTest::TearDown()
{
    g_sa_follow = m_old_sa_follow;
    g_check_count = m_old_check_count;
    GroupUnreadArticleTest::TearDown();
}

void ThreadKillArticleTest::set_thread(long current)
{
    set_scope({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
               AF_EXISTS | AF_YANK_BACK},
              current);
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_SEL;
    }
    g_selected_only = true;
    g_selected_subj_cnt = 3;
    g_sel_mode = SM_THREAD;
    g_dm_count = 1;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{3};
    g_search_ahead = ArticleNum{4};
    g_reread = true;
    g_force_last = true;
}

std::string ThreadKillArticleTest::kill_articles(std::string command, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    push_char('n');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::move(command));

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(ArticleNum{3}, g_parsed_art);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

class KillSubthreadArticleTest : public ThreadKillArticleTest
{
};

class KillThreadArticleTest : public ThreadKillArticleTest
{
};

class KillThreadScanArticleTest : public KillThreadArticleTest,
                                  public testing::WithParamInterface<std::pair<bool, bool>>
{
};

class KillSubthreadScanArticleTest : public KillSubthreadArticleTest,
                                     public testing::WithParamInterface<std::pair<bool, bool>>
{
};

class DelayScopeArticleTest : public ThreadKillArticleTest
{
protected:
    void        set_delay_articles(long current);
    std::string delay_scope(char action, ArticleSwitchResult expected = AS_NORM);
};

void DelayScopeArticleTest::set_delay_articles(long current)
{
    set_thread(current);
    article_ptr(ArticleNum{3})->m_flags = AF_EXISTS | AF_YANK_BACK;
    --m_newsgroup.m_to_read;
    ++g_dm_count;
    article_ptr(ArticleNum{4})->m_flags &= ~AF_SEL;
    --g_selected_count;
    article_ptr(ArticleNum{2})->m_auto_flags = AUTO_SEL_1;
    article_ptr(ArticleNum{4})->m_auto_flags = AUTO_KILL_SBJ;
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_WAS_SELECTED;
    }
}

std::string DelayScopeArticleTest::delay_scope(char action, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    Article **const                        cursor = g_art_ptr;
    const bool          threaded = g_threaded_group;
    const bool          breadth_first = g_breadth_first;
    const SelectionMode mode = g_sel_mode;
    const bool          scan = g_sa_in;
    const bool          follow = g_sa_follow;
    const bool          temporary_follow = g_s_follow_temp;
    const ArticleNum    first = g_first_art;
    push_char('n');
    push_char(action);
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{'_', FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(cursor, g_art_ptr);
    EXPECT_EQ(threaded, g_threaded_group);
    EXPECT_EQ(breadth_first, g_breadth_first);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(scan, g_sa_in);
    EXPECT_EQ(follow, g_sa_follow);
    EXPECT_EQ(temporary_follow, g_s_follow_temp);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(ArticleNum{3}, g_parsed_art);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_EQ(10, g_check_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
    EXPECT_EQ(AUTO_SEL_1, article_ptr(ArticleNum{2})->m_auto_flags);
    EXPECT_EQ(AUTO_KILL_SBJ, article_ptr(ArticleNum{4})->m_auto_flags);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('n', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

class DelaySubthreadOrderArticleTest : public DelayScopeArticleTest, public testing::WithParamInterface<bool>
{
};

class DelayScopeCommandArticleTest : public DelayScopeArticleTest, public testing::WithParamInterface<char>
{
};

class RestoreDelayedArticleTest : public ThreadKillArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string restore_delayed(long first);

private:
    bool m_old_panic{g_panic};
};

void RestoreDelayedArticleTest::SetUp()
{
    ThreadKillArticleTest::SetUp();
    g_panic = false;
}

void RestoreDelayedArticleTest::TearDown()
{
    g_panic = m_old_panic;
    ThreadKillArticleTest::TearDown();
}

std::string RestoreDelayedArticleTest::restore_delayed(long first)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    const ArticleNum    parsed = g_parsed_art;
    const ArticleNum    search_ahead = g_search_ahead;
    const bool          selected_only = g_selected_only;
    const bool          threaded = g_threaded_group;
    const bool          breadth_first = g_breadth_first;
    const SelectionMode mode = g_sel_mode;
    const bool          scan = g_sa_in;
    const bool          follow = g_sa_follow;
    const bool          temporary_follow = g_s_follow_temp;
    const bool          reread = g_reread;
    const bool          force_last = g_force_last;
    push_char('n');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("Y");

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(AS_NORM, result);
    EXPECT_EQ(ArticleNum{first}, g_art);
    EXPECT_EQ(first <= g_last_art.value_of() ? article_find(ArticleNum{first}) : nullptr, g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(search_ahead, g_search_ahead);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(threaded, g_threaded_group);
    EXPECT_EQ(breadth_first, g_breadth_first);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(scan, g_sa_in);
    EXPECT_EQ(follow, g_sa_follow);
    EXPECT_EQ(temporary_follow, g_s_follow_temp);
    EXPECT_EQ(reread, g_reread);
    EXPECT_EQ(force_last, g_force_last);
    EXPECT_EQ(10, g_check_count);
    EXPECT_EQ(0, g_dm_count);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('n', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

class RestoreSingleArticleTest : public RestoreDelayedArticleTest, public testing::WithParamInterface<bool>
{
};

class RestoreScopeArticleTest : public RestoreDelayedArticleTest, public testing::WithParamInterface<char>
{
};

enum class RestoreOrder
{
    Numeric,
    Subject,
    DepthFirst,
    BreadthFirst,
    Sorted
};

class RestoreOrderArticleTest : public RestoreDelayedArticleTest, public testing::WithParamInterface<RestoreOrder>
{
};

class CatchupArticleTest : public RestoreDelayedArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_catchup_articles(long current = 4);
    std::string catch_up(std::string_view input, ArticleSwitchResult expected);
    void        expect_catchup_unchanged();
    void        expect_subscription_unchanged();

    static const ArticleFlags CACHED;
    Newsrc                    m_newsrc{};

private:
    bool             m_old_in_ng{g_in_ng};
    std::string      m_old_newsgroup_name{g_newsgroup_name};
    NewsgroupNum     m_old_newsgroup_to_read{g_newsgroup_to_read};
    char             m_old_erase_char{g_erase_char};
    char             m_old_kill_char{g_kill_char};
    std::string_view m_old_tc_bc{g_tc_BC};
    bool             m_old_mouse_is_down{g_mouse_is_down};
    char             m_old_int_count{g_int_count};
    std::string      m_rc_line;
};

const ArticleFlags CatchupArticleTest::CACHED = AF_EXISTS | AF_CACHED;

void CatchupArticleTest::SetUp()
{
    RestoreDelayedArticleTest::SetUp();
    g_in_ng = true;
    g_newsgroup_name = "comp.test.catchup";
    g_newsgroup_to_read = NewsgroupNum{5};
    g_erase_char = '\b';
    g_kill_char = Ctl('U');
    g_tc_BC = "\b";
    g_mouse_is_down = false;
    g_int_count = 0;
    m_newsrc.flags = RF_ACTIVE;
    m_newsgroup.m_rc = &m_newsrc;
    m_newsgroup.m_rc_line = g_newsgroup_name + ": 3,7";
    m_newsgroup.m_num_offset = static_cast<int>(g_newsgroup_name.size()) + 1;
    m_newsgroup.m_subscribe_char = ':';
    m_newsgroup.hide_subscribe_char();
    m_rc_line = m_newsgroup.m_rc_line;
}

void CatchupArticleTest::TearDown()
{
    g_in_ng = m_old_in_ng;
    g_newsgroup_name = m_old_newsgroup_name;
    g_newsgroup_to_read = m_old_newsgroup_to_read;
    g_erase_char = m_old_erase_char;
    g_kill_char = m_old_kill_char;
    g_tc_BC = m_old_tc_bc;
    g_mouse_is_down = m_old_mouse_is_down;
    g_int_count = m_old_int_count;
    RestoreDelayedArticleTest::TearDown();
}

void CatchupArticleTest::set_catchup_articles(long current)
{
    set_scope({CACHED | AF_UNREAD | AF_SEL, CACHED | AF_UNREAD, CACHED | AF_DEL, CACHED | AF_UNREAD | AF_SEL,
               AF_CACHED | AF_THREADED, CACHED | AF_UNREAD | AF_SEL, CACHED | AF_YANK_BACK},
              current);
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_SEL;
    }
    g_selected_only = true;
    g_selected_subj_cnt = 3;
    g_dm_count = 1;
    g_sel_mode = SM_THREAD;
    g_recent_art = ArticleNum{2};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{3};
    g_search_ahead = ArticleNum{4};
    g_reread = true;
    g_force_last = true;
}

std::string CatchupArticleTest::catch_up(std::string_view input, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    Article **const                        cursor = g_art_ptr;
    const bool threaded = g_threaded_group;
    const bool breadth_first = g_breadth_first;
    const bool scan = g_sa_in;
    push_char('Q');
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("c");

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_NE(std::string::npos,
              output.find(g_verbose ? "Mark everything in comp.test.catchup as read?" : "Catchup comp.test.catchup?"));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(cursor, g_art_ptr);
    EXPECT_EQ(threaded, g_threaded_group);
    EXPECT_EQ(breadth_first, g_breadth_first);
    EXPECT_EQ(scan, g_sa_in);
    EXPECT_EQ(SM_THREAD, g_sel_mode);
    EXPECT_EQ(ArticleNum{3}, g_parsed_art);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(10, g_check_count);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(m_rc_line, m_newsgroup.m_rc_line);
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('Q', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

void CatchupArticleTest::expect_subscription_unchanged()
{
    EXPECT_EQ(':', m_newsgroup.m_subscribe_char);
    EXPECT_EQ(RF_ACTIVE, m_newsrc.flags);
    EXPECT_EQ(NewsgroupNum{5}, g_newsgroup_to_read);
}

void CatchupArticleTest::expect_catchup_unchanged()
{
    expect_flags({CACHED | AF_UNREAD | AF_SEL, CACHED | AF_UNREAD, CACHED | AF_DEL, CACHED | AF_UNREAD | AF_SEL,
                  AF_CACHED | AF_THREADED, CACHED | AF_UNREAD | AF_SEL, CACHED | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_force_last);
    expect_subscription_unchanged();
}

class CatchupConfirmArticleTest : public CatchupArticleTest, public testing::WithParamInterface<char>
{
};

class CatchupCancelArticleTest : public CatchupArticleTest, public testing::WithParamInterface<std::tuple<char, long>>
{
protected:
    void SetUp() override;
};

void CatchupCancelArticleTest::SetUp()
{
    CatchupArticleTest::SetUp();
    set_catchup_articles();
    g_art = g_curr_art = ArticleNum{std::get<1>(GetParam())};
    g_artp = g_curr_artp = g_art > g_last_art ? nullptr : article_ptr(g_art);
}

class CatchupHelpArticleTest : public CatchupArticleTest, public testing::WithParamInterface<bool>
{
};

class CatchupTailArticleTest : public CatchupArticleTest, public testing::WithParamInterface<std::string>
{
};

class CatchupZeroArticleTest : public CatchupArticleTest, public testing::WithParamInterface<std::string>
{
};

class CatchupNumericCancelArticleTest : public CatchupArticleTest, public testing::WithParamInterface<char>
{
};

class CatchupUnsubscribeArticleTest : public CatchupArticleTest, public testing::WithParamInterface<bool>
{
};

class CatchupUncachedTailArticleTest : public CatchupArticleTest, public testing::WithParamInterface<int>
{
};

class NumericJumpArticleTest : public UnreadScopeArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string enter_number(std::string_view input, ArticleSwitchResult expected = AS_NORM);
    void        expect_target(long number);

private:
    char                  m_old_erase_char{g_erase_char};
    char                  m_old_kill_char{g_kill_char};
    std::string_view      m_old_tc_bc{g_tc_BC};
    bool                  m_old_mouse_is_down{g_mouse_is_down};
    char                  m_old_int_count{g_int_count};
    std::string           m_old_msg{g_msg};
    int                   m_old_perform_count{g_perform_count};
    bool                  m_old_error_occurred{g_error_occurred};
    decltype(g_subj_line) m_old_subj_line{g_subj_line};
    int                   m_old_page_line{g_page_line};
    bool                  m_old_performed_article_loop{g_performed_article_loop};
    int                   m_old_spin_count{g_spin_count};
};

void NumericJumpArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_erase_char = '\b';
    g_kill_char = Ctl('U');
    g_tc_BC = "\b";
    g_mouse_is_down = false;
    g_int_count = 0;
    const ArticleFlags cached = AF_EXISTS | AF_CACHED;
    set_articles({cached | AF_UNREAD | AF_SEL, cached, cached | AF_UNREAD, cached | AF_UNREAD | AF_SEL, AF_CACHED,
                  cached | AF_UNREAD | AF_SEL, cached | AF_YANK_BACK, cached | AF_UNREAD, cached | AF_UNREAD, cached,
                  cached | AF_UNREAD | AF_SEL, cached | AF_UNREAD | AF_SEL},
                 4);
    g_selected_only = true;
    g_first_subject->m_flags |= SF_SEL;
    g_selected_subj_cnt = 1;
    g_recent_art = ArticleNum{7};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{4};
    g_search_ahead = ArticleNum{3};
}

void NumericJumpArticleTest::TearDown()
{
    g_erase_char = m_old_erase_char;
    g_kill_char = m_old_kill_char;
    g_tc_BC = m_old_tc_bc;
    g_mouse_is_down = m_old_mouse_is_down;
    g_int_count = m_old_int_count;
    g_msg = m_old_msg;
    g_perform_count = m_old_perform_count;
    g_error_occurred = m_old_error_occurred;
    g_subj_line = m_old_subj_line;
    g_page_line = m_old_page_line;
    g_performed_article_loop = m_old_performed_article_loop;
    g_spin_count = m_old_spin_count;
    UnreadScopeArticleTest::TearDown();
}

std::string NumericJumpArticleTest::enter_number(std::string_view input, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *>           current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>           recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>                     order = g_art_ptr_list;
    Article **const                                  cursor = g_art_ptr;
    const ArticleNum                                 first = g_first_art;
    const ArticleNum                                 parsed = g_parsed_art;
    const ArticleUnread                              unread = m_newsgroup.m_to_read;
    const ArticleUnread                              selected = g_selected_count;
    const SubjectFlags                               subject_flags = g_first_subject->m_flags;
    const int                                        selected_subjects = g_selected_subj_cnt;
    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[number, article] : g_article_list)
    {
        flags.emplace_back(number, article.m_flags);
    }
    push_char('Q');
    const std::string_view suffix = input.substr(1);
    for (auto ch = suffix.rbegin(); ch != suffix.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{input.front(), FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(cursor, g_art_ptr);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(subject_flags, g_first_subject->m_flags);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(g_curr_art > g_last_art ? MM_ARTICLE_END : MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(flags.size(), g_article_list.size());
    for (const auto &[number, expected_flags] : flags)
    {
        SCOPED_TRACE(number.value_of());
        EXPECT_EQ(expected_flags, article_find(number)->m_flags);
    }
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('Q', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

void NumericJumpArticleTest::expect_target(long number)
{
    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(article_find(ArticleNum{number}), g_artp);
    EXPECT_TRUE(g_reread);
}

class NumericRangeArticleTest : public NumericJumpArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string apply_range(std::string_view input, ArticleSwitchResult expected = AS_ASK, long search_ahead = -1);
    void        expect_changes(std::initializer_list<std::pair<long, ArticleFlags>> changes);

    static const ArticleFlags CACHED;

private:
    bool                                       m_old_one_command{g_one_command};
    bool                                       m_old_use_threads{g_use_threads};
    bool                                       m_old_in_ng{g_in_ng};
    int                                        m_old_check_count{g_check_count};
    int                                        m_old_dm_count{g_dm_count};
    decltype(g_missing_count)                  m_old_missing_count{g_missing_count};
    decltype(g_just_a_sec)                     m_old_just_a_sec{g_just_a_sec};
    std::string_view                           m_old_tc_cr{g_tc_CR};
    std::string_view                           m_old_tc_ce{g_tc_CE};
    std::vector<std::pair<long, ArticleFlags>> m_before_flags;
};

const ArticleFlags NumericRangeArticleTest::CACHED = AF_EXISTS | AF_CACHED;

void NumericRangeArticleTest::SetUp()
{
    NumericJumpArticleTest::SetUp();
    g_one_command = false;
    g_use_threads = true;
    g_in_ng = true;
    g_check_count = 10;
    g_dm_count = 1;
    g_missing_count = 0;
    g_just_a_sec = 0;
    g_tc_CR = "\r";
    g_tc_CE = "\033[K";
    g_perform_count = 77;
    g_msg = "prior status";
}

void NumericRangeArticleTest::TearDown()
{
    g_one_command = m_old_one_command;
    g_use_threads = m_old_use_threads;
    g_in_ng = m_old_in_ng;
    g_check_count = m_old_check_count;
    g_dm_count = m_old_dm_count;
    g_missing_count = m_old_missing_count;
    g_just_a_sec = m_old_just_a_sec;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    NumericJumpArticleTest::TearDown();
}

std::string NumericRangeArticleTest::apply_range(std::string_view input, ArticleSwitchResult expected,
                                                 long search_ahead)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    Article **const                        cursor = g_art_ptr;
    const ArticleNum                       parsed = g_parsed_art;
    const bool reread = g_reread;
    m_before_flags.clear();
    for (const auto &[number, entry] : g_article_list)
    {
        m_before_flags.emplace_back(number.value_of(), entry.m_flags);
    }
    push_char('Q');
    const std::string_view suffix = input.substr(1);
    for (auto ch = suffix.rbegin(); ch != suffix.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{input.front(), FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(cursor, g_art_ptr);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(reread, g_reread);
    EXPECT_EQ(ArticleNum{search_ahead}, g_search_ahead);
    EXPECT_TRUE(g_force_last);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(0, g_missing_count);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(current.first > g_last_art ? MM_ARTICLE_END : MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('Q', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

void NumericRangeArticleTest::expect_changes(std::initializer_list<std::pair<long, ArticleFlags>> changes)
{
    for (const auto &[number, flags] : changes)
    {
        const auto entry =
            std::find_if(m_before_flags.begin(), m_before_flags.end(),
                         [number](const std::pair<long, ArticleFlags> &item) { return item.first == number; });
        ASSERT_NE(m_before_flags.end(), entry);
        entry->second = flags;
    }
    ASSERT_EQ(m_before_flags.size(), g_article_list.size());
    for (const auto &[number, flags] : m_before_flags)
    {
        SCOPED_TRACE(number);
        const Article *entry = article_find(ArticleNum{number});
        ASSERT_NE(nullptr, entry);
        EXPECT_EQ(flags, entry->m_flags);
    }
}

class NumericRangeOutputArticleTest : public NumericRangeArticleTest, public testing::WithParamInterface<bool>
{
};

class NumericRangeInvalidArticleTest : public NumericRangeArticleTest, public testing::WithParamInterface<std::string>
{
};

class NumericRangeCancelArticleTest : public NumericRangeArticleTest, public testing::WithParamInterface<std::string>
{
};

class NumericRangeFailureArticleTest : public NumericRangeArticleTest, public testing::WithParamInterface<bool>
{
};

class NumericJumpTargetArticleTest : public NumericJumpArticleTest, public testing::WithParamInterface<long>
{
};

class NumericJumpInputArticleTest : public NumericJumpArticleTest, public testing::WithParamInterface<std::string>
{
};

class NumericJumpCancelArticleTest : public NumericJumpArticleTest, public testing::WithParamInterface<std::string>
{
};

class NumericJumpMissingArticleTest : public NumericJumpArticleTest, public testing::WithParamInterface<bool>
{
};

struct NumericJumpNavigation
{
    bool sorted;
    char command;
    long target;
};

class NumericJumpNavigationArticleTest : public NumericJumpArticleTest,
                                         public testing::WithParamInterface<NumericJumpNavigation>
{
};

class UnsubscribeArticleTest : public CatchupArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string unsubscribe();

private:
    ArticleUnread m_old_newsgroup_min_to_read{g_newsgroup_min_to_read};
    Subject      *m_old_sel_page_sp{g_sel_page_sp};
    Article     **m_old_sel_page_app{g_sel_page_app};
    Article      *m_old_sel_last_ap{g_sel_last_ap};
    Subject      *m_old_sel_last_sp{g_sel_last_sp};
};

void UnsubscribeArticleTest::SetUp()
{
    CatchupArticleTest::SetUp();
    g_newsgroup_min_to_read = TR_ONE;
}

void UnsubscribeArticleTest::TearDown()
{
    g_newsgroup_min_to_read = m_old_newsgroup_min_to_read;
    g_sel_page_sp = m_old_sel_page_sp;
    g_sel_page_app = m_old_sel_page_app;
    g_sel_last_ap = m_old_sel_last_ap;
    g_sel_last_sp = m_old_sel_last_sp;
    CatchupArticleTest::TearDown();
}

std::string UnsubscribeArticleTest::unsubscribe()
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    Article **const                        cursor = g_art_ptr;
    const ArticleNum    first = g_first_art;
    const ArticleUnread unread = m_newsgroup.m_to_read;
    const ArticleUnread selected = g_selected_count;
    const int           selected_subjects = g_selected_subj_cnt;
    const int           delayed = g_dm_count;
    const bool          selected_only = g_selected_only;
    const bool          threaded = g_threaded_group;
    const bool          scan = g_sa_in;
    const bool          follow = g_sa_follow;
    const bool          temporary_follow = g_s_follow_temp;
    const NewsgroupNum                     groups = g_newsgroup_to_read;
    const NewsrcFlags                      newsrc_flags = m_newsrc.flags;
    const std::string                      rc_line = m_newsgroup.m_rc_line;

    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[number, entry] : g_article_list)
    {
        flags.emplace_back(number, entry.m_flags);
    }
    push_char('n');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{'u', FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_CLEAN, result);
    EXPECT_EQ("\nUnsubscribed to newsgroup comp.test.catchup\n", output);
    EXPECT_EQ('!', m_newsgroup.m_subscribe_char);
    EXPECT_EQ(newsrc_flags | RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_EQ(groups - NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(rc_line, m_newsgroup.m_rc_line);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(cursor, g_art_ptr);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(delayed, g_dm_count);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(threaded, g_threaded_group);
    EXPECT_EQ(scan, g_sa_in);
    EXPECT_EQ(follow, g_sa_follow);
    EXPECT_EQ(temporary_follow, g_s_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_parsed_art);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_EQ(SM_THREAD, g_sel_mode);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_TRUE(g_in_ng);
    EXPECT_EQ(10, g_check_count);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(flags.size(), g_article_list.size());
    for (const auto &[number, expected] : flags)
    {
        const Article *entry = article_find(number);
        EXPECT_NE(nullptr, entry);
        if (entry)
        {
            EXPECT_EQ(expected, entry->m_flags) << number.value_of();
        }
    }
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('n', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

class UnsubscribePositionArticleTest : public UnsubscribeArticleTest, public testing::WithParamInterface<long>
{
};

class SelectorEntryArticleTest : public UnreadScopeArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string run_selector(const std::function<char(char_int)> &selector, ArticleSwitchResult expected,
                             DoNewsgroupResult exit_code = NG_NORM);
    std::string run_selector(std::string command, char_int request, const std::function<char(char_int)> &selector,
                             ArticleSwitchResult expected, DoNewsgroupResult exit_code = NG_NORM);

private:
    int m_old_lines{g_tc_LINES};
    int m_old_scrolled{g_term_scrolled};
    int m_old_check_count{g_check_count};
    int m_old_dm_count{g_dm_count};
};

void SelectorEntryArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_sa_in = true;
    g_tc_LINES = 24;
    g_term_line = 3;
    g_term_col = 4;
    g_term_scrolled = 2;
    g_check_count = 10;
    g_dm_count = 0;
}

void SelectorEntryArticleTest::TearDown()
{
    g_tc_LINES = m_old_lines;
    g_term_scrolled = m_old_scrolled;
    g_check_count = m_old_check_count;
    g_dm_count = m_old_dm_count;
    UnreadScopeArticleTest::TearDown();
}

std::string SelectorEntryArticleTest::run_selector(const std::function<char(char_int)> &selector,
                                                   ArticleSwitchResult expected, DoNewsgroupResult exit_code)
{
    return run_selector("+", '+', selector, expected, exit_code);
}

std::string SelectorEntryArticleTest::run_selector(std::string command, char_int request,
                                                   const std::function<char(char_int)> &selector,
                                                   ArticleSwitchResult expected, DoNewsgroupResult exit_code)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    int        calls = 0;
    const auto invoke = [&](char_int selector_command)
    {
        ++calls;
        EXPECT_EQ(request, selector_command);
        EXPECT_FALSE(g_sa_in);
        EXPECT_TRUE(g_s_follow_temp);
        EXPECT_TRUE(g_univ_follow_temp);
        return selector(selector_command);
    };
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::move(command), {}, invoke);

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_EQ(1, calls);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_FALSE(g_sa_in);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(exit_code, g_newsgroup_exit_code);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    return output;
}

class AlternateSelectorArticleTest : public SelectorEntryArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    char             m_old_erase_char{g_erase_char};
    char             m_old_kill_char{g_kill_char};
    std::string_view m_old_tc_bc{g_tc_BC};
    bool             m_old_mouse_is_down{g_mouse_is_down};
};

void AlternateSelectorArticleTest::SetUp()
{
    SelectorEntryArticleTest::SetUp();
    g_erase_char = '\b';
    g_kill_char = Ctl('U');
    g_tc_BC = "\b";
    g_mouse_is_down = false;
}

void AlternateSelectorArticleTest::TearDown()
{
    g_erase_char = m_old_erase_char;
    g_kill_char = m_old_kill_char;
    g_tc_BC = m_old_tc_bc;
    g_mouse_is_down = m_old_mouse_is_down;
    SelectorEntryArticleTest::TearDown();
}

class SelectorModeShortcutArticleTest : public AlternateSelectorArticleTest, public testing::WithParamInterface<char>
{
};

class ReadSelectorPromptArticleTest : public AlternateSelectorArticleTest, public testing::WithParamInterface<char>
{
};

class CancelSelectorPrefixArticleTest : public AlternateSelectorArticleTest, public testing::WithParamInterface<char>
{
};

class SelectorExitArticleTest : public SelectorEntryArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    bool m_old_sa_go{g_sa_go};
    bool m_old_sa_go_explicit{g_sa_go_explicit};
    bool m_old_sa_do_sel_threads{g_sa_do_sel_threads};
};

void SelectorExitArticleTest::SetUp()
{
    SelectorEntryArticleTest::SetUp();
    g_newsgroup_exit_code = NG_MINUS;
    g_sa_go = false;
    g_sa_go_explicit = false;
    g_sa_do_sel_threads = false;
}

void SelectorExitArticleTest::TearDown()
{
    g_sa_go = m_old_sa_go;
    g_sa_go_explicit = m_old_sa_go_explicit;
    g_sa_do_sel_threads = m_old_sa_do_sel_threads;
    SelectorEntryArticleTest::TearDown();
}

class SelectorGroupExitArticleTest : public SelectorExitArticleTest,
                                     public testing::WithParamInterface<std::pair<char, DoNewsgroupResult>>
{
};

class QuitCommandArticleTest : public UnreadScopeArticleTest
{
protected:
    void SetUp() override;
    void expect_quit(std::string_view command, DoNewsgroupResult expected);
};

void QuitCommandArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 2);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    g_first_subject->m_flags |= SF_SEL;
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{2};
    g_search_ahead = ArticleNum{2};
    g_reread = true;
    g_force_last = true;
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
}

void QuitCommandArticleTest::expect_quit(std::string_view command, DoNewsgroupResult expected)
{
    const std::pair<ArticleNum, Article *> position = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const MinorMode mode = g_mode;
    g_newsgroup_exit_code = NG_MINUS;
    push_char('n');

    EXPECT_EQ(AS_CLEAN, art_switch(std::string{command}));

    EXPECT_EQ(expected, g_newsgroup_exit_code);
    EXPECT_EQ(position, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(SF_SEL | SF_VISIT, g_first_subject->m_flags);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleNum{2}, g_parsed_art);
    EXPECT_EQ(ArticleNum{2}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(mode, g_mode);
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

class DefaultCommandArticleTest : public UnreadScopeArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_position(long number, DoNewsgroupResult exit_code = NG_NORM);
    void expect_pending(std::string_view input);

private:
    bool          m_old_sa_follow{g_sa_follow};
    bool          m_old_univ_follow{g_univ_follow};
    unsigned char m_old_last_char{g_last_char};
    int           m_old_int_count{g_int_count};
};

void DefaultCommandArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 1);
    g_sa_follow = false;
    g_univ_follow = false;
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    set_default_cmd();
}

void DefaultCommandArticleTest::TearDown()
{
    g_sa_follow = m_old_sa_follow;
    g_univ_follow = m_old_univ_follow;
    g_last_char = m_old_last_char;
    g_int_count = m_old_int_count;
    UnreadScopeArticleTest::TearDown();
}

void DefaultCommandArticleTest::expect_position(long number, DoNewsgroupResult exit_code)
{
    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(number <= g_last_art.value_of() ? article_ptr(ArticleNum{number}) : nullptr, g_artp);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_curr_artp);
    EXPECT_EQ(ArticleNum{-1}, g_recent_art);
    EXPECT_EQ(nullptr, g_recent_artp);
    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(exit_code, g_newsgroup_exit_code);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
}

void DefaultCommandArticleTest::expect_pending(std::string_view input)
{
    for (const char ch : input)
    {
        ASSERT_TRUE(macro_pending());
        EXPECT_EQ(ch, read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
}

class ScanContinuationArticleTest : public UnreadScopeArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_reading(long current, SelectionMode mode = SM_THREAD);
    void set_scan_articles(std::initializer_list<ArticleFlags> flags, long current, SelectionMode mode = SM_THREAD);
    void dispatch(std::string command, long number, ArticleSwitchResult expected = AS_NORM,
                  DoNewsgroupResult exit_code = NG_NORM);

private:
    bool          m_old_sa_follow{g_sa_follow};
    bool          m_old_univ_follow{g_univ_follow};
    bool          m_old_sa_go{g_sa_go};
    bool          m_old_sa_go_explicit{g_sa_go_explicit};
    bool          m_old_sa_do_sel_threads{g_sa_do_sel_threads};
    ArticleNum    m_old_sa_art{g_sa_art};
    unsigned char m_old_last_char{g_last_char};
    int           m_old_int_count{g_int_count};
};

void ScanContinuationArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_sa_in = true;
    g_sa_follow = false;
    g_univ_follow = false;
    g_sa_go = false;
    g_sa_go_explicit = false;
    g_sa_do_sel_threads = true;
    g_sa_art = ArticleNum{6};
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_int_count = 0;
    g_default_cmd = "npq";
    g_search_ahead = ArticleNum{3};
}

void ScanContinuationArticleTest::TearDown()
{
    g_sa_follow = m_old_sa_follow;
    g_univ_follow = m_old_univ_follow;
    g_sa_go = m_old_sa_go;
    g_sa_go_explicit = m_old_sa_go_explicit;
    g_sa_do_sel_threads = m_old_sa_do_sel_threads;
    g_sa_art = m_old_sa_art;
    g_last_char = m_old_last_char;
    g_int_count = m_old_int_count;
    UnreadScopeArticleTest::TearDown();
}

void ScanContinuationArticleTest::set_reading(long current, SelectionMode mode)
{
    set_scan_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                       AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                      current, mode);
}

void ScanContinuationArticleTest::set_scan_articles(std::initializer_list<ArticleFlags> flags, long current,
                                                    SelectionMode mode)
{
    set_scope(flags, current);
    g_sel_mode = mode;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_find(g_recent_art);
    g_parsed_art = ArticleNum{3};
    for (const auto &[number, article] : g_article_list)
    {
        if (article.m_flags & AF_SEL)
        {
            article.m_subj->m_flags |= SF_SEL;
        }
    }
    for (const Subject &subject : m_subjects)
    {
        g_selected_subj_cnt += (subject.m_flags & SF_SEL) != 0;
    }
}

void ScanContinuationArticleTest::dispatch(std::string command, long number, ArticleSwitchResult expected,
                                           DoNewsgroupResult exit_code)
{
    const std::pair<ArticleNum, Article *>           current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>           recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                                 first = g_first_art;
    const ArticleNum                                 parsed = g_parsed_art;
    const ArticleUnread                              unread = m_newsgroup.m_to_read;
    const ArticleUnread                              selected = g_selected_count;
    const int                                        selected_subjects = g_selected_subj_cnt;
    const SelectionMode                              mode = g_sel_mode;
    const ArticleNum                                 scan_article = g_sa_art;
    const bool                                       selected_only = g_selected_only;
    const bool                                       scan_in = g_sa_in;
    const bool                                       scan_go = g_sa_go;
    const bool                                       scan_explicit = g_sa_go_explicit || command == ";";
    const bool                                       scan_threads = g_sa_do_sel_threads;
    const bool                                       scan_follow = g_sa_follow;
    const bool                                       univ_follow = g_univ_follow;
    const bool                                       virtual_read = g_univ_read_virt_flag;
    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[article_number, article] : g_article_list)
    {
        flags.emplace_back(article_number, article.m_flags);
    }
    std::vector<SubjectFlags> subject_flags;
    for (const Subject &subject : m_subjects)
    {
        subject_flags.push_back(subject.m_flags);
    }
    push_char('q');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::move(command));

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_TRUE(output.empty()) << output;
    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(article_find(g_art), g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(scan_in, g_sa_in);
    EXPECT_EQ(scan_go, g_sa_go);
    EXPECT_EQ(scan_explicit, g_sa_go_explicit);
    EXPECT_EQ(scan_threads, g_sa_do_sel_threads);
    EXPECT_EQ(scan_article, g_sa_art);
    EXPECT_EQ(scan_follow, g_sa_follow);
    EXPECT_EQ(univ_follow, g_univ_follow);
    EXPECT_EQ(virtual_read, g_univ_read_virt_flag);
    EXPECT_EQ(exit_code, g_newsgroup_exit_code);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_TRUE(g_art_ptr_list.empty());
    EXPECT_EQ(nullptr, g_art_ptr);
    EXPECT_EQ(flags.size(), g_article_list.size());
    for (const auto &[article_number, expected_flags] : flags)
    {
        SCOPED_TRACE(article_number.value_of());
        EXPECT_EQ(expected_flags, article_find(article_number)->m_flags);
    }
    for (int index = 0; index < 3; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(subject_flags[index], m_subjects[index].m_flags);
    }
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

class ScanEntryArticleTest : public ScanContinuationArticleTest, public testing::WithParamInterface<bool>
{
};

class ScanNextCommandArticleTest : public ScanContinuationArticleTest, public testing::WithParamInterface<char>
{
};

class SearchCommandArticleTest : public UnreadScopeArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_search_articles(std::initializer_list<ArticleFlags> flags, long current);
    std::string search_articles(char direction, std::string_view query, ArticleSwitchResult expected = AS_NORM,
                                const std::function<void(ArticleNum)> &progress = {});
    std::string enter_search(char direction, std::string_view input, ArticleSwitchResult expected,
                             const std::function<void(ArticleNum)> &progress = {});
    void        expect_interrupted_search(long current);

private:
    DataSource            m_data_source{};
    DataSource           *m_old_data_source{g_data_source};
    HeaderType            m_old_subject_header{g_header_type[SUBJ_LINE]};
    std::string           m_old_last_pat{g_last_pat};
    ArtScope              m_old_art_how_much{g_art_how_much};
    HeaderLineType        m_old_art_srch_hdr{g_art_srch_hdr};
    bool                  m_old_art_do_read{g_art_do_read};
    bool                  m_old_use_threads{g_use_threads};
    int                   m_old_page_line{g_page_line};
    char                  m_old_int_count{g_int_count};
    int                   m_old_perform_count{g_perform_count};
    decltype(g_subj_line) m_old_subj_line{g_subj_line};
    bool                  m_old_error_occurred{g_error_occurred};
    bool                  m_old_performed_article_loop{g_performed_article_loop};
    int                   m_old_spin_count{g_spin_count};
    bool                  m_old_mouse_is_down{g_mouse_is_down};
    char                  m_old_erase_char{g_erase_char};
    char                  m_old_kill_char{g_kill_char};
    std::string_view      m_old_tc_bc{g_tc_BC};
};

void SearchCommandArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_data_source = &m_data_source;
    g_header_type[SUBJ_LINE].flags |= HT_CACHED;
    g_last_pat.clear();
    g_art_how_much = ARTSCOPE_SUBJECT;
    g_art_srch_hdr = SOME_LINE;
    g_art_do_read = false;
    g_use_threads = false;
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_int_count = 0;
    g_erase_char = '\b';
    g_kill_char = Ctl('U');
    g_tc_BC = "\b";
    search_init();
    art_search_init();
}

void SearchCommandArticleTest::TearDown()
{
    g_data_source = m_old_data_source;
    g_header_type[SUBJ_LINE] = m_old_subject_header;
    g_last_pat = m_old_last_pat;
    g_art_how_much = m_old_art_how_much;
    g_art_srch_hdr = m_old_art_srch_hdr;
    g_art_do_read = m_old_art_do_read;
    g_use_threads = m_old_use_threads;
    g_page_line = m_old_page_line;
    g_int_count = m_old_int_count;
    g_perform_count = m_old_perform_count;
    g_subj_line = m_old_subj_line;
    g_error_occurred = m_old_error_occurred;
    g_performed_article_loop = m_old_performed_article_loop;
    g_spin_count = m_old_spin_count;
    g_mouse_is_down = m_old_mouse_is_down;
    g_erase_char = m_old_erase_char;
    g_kill_char = m_old_kill_char;
    g_tc_BC = m_old_tc_bc;
    UnreadScopeArticleTest::TearDown();
}

void SearchCommandArticleTest::set_search_articles(std::initializer_list<ArticleFlags> flags, long current)
{
    set_scope(flags, current);
    m_subjects[0].m_str = "Re: Needle earlier";
    m_subjects[1].m_str = "Re: Plain middle";
    m_subjects[2].m_str = "Re: Needle later";
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
}

std::string SearchCommandArticleTest::search_articles(char direction, std::string_view query,
                                                      ArticleSwitchResult                    expected,
                                                      const std::function<void(ArticleNum)> &progress)
{
    const std::string output = enter_search(direction, std::string{query} + '\n', expected, progress);
    EXPECT_FALSE(macro_pending());
    return output;
}

std::string SearchCommandArticleTest::enter_search(char direction, std::string_view input, ArticleSwitchResult expected,
                                                   const std::function<void(ArticleNum)> &progress)
{
    const std::pair<ArticleNum, Article *>           current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>           recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                                 first = g_first_art;
    const ArticleNum                                 parsed = g_parsed_art;
    const ArticleUnread                              unread = m_newsgroup.m_to_read;
    const ArticleUnread                              selected = g_selected_count;
    const int                                        selected_subjects = g_selected_subj_cnt;
    const bool                                       selected_only = g_selected_only;
    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[number, article] : g_article_list)
    {
        flags.emplace_back(number, article.m_flags);
    }
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{direction, FINISH_CMD}, progress);

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(selected_only, g_selected_only);
    for (const auto &[number, expected_flags] : flags)
    {
        SCOPED_TRACE(number.value_of());
        EXPECT_EQ(expected_flags, article_ptr(number)->m_flags);
    }
    return output;
}

class SearchActionArticleTest : public SearchCommandArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_action_articles(long current);
    std::string perform_search(std::string_view command, ArticleSwitchResult expected = AS_ASK,
                               const std::function<void(ArticleNum)> &progress = {});

private:
    bool                      m_old_in_ng{g_in_ng};
    bool                      m_old_one_command{g_one_command};
    int                       m_old_check_count{g_check_count};
    decltype(g_just_a_sec)    m_old_just_a_sec{g_just_a_sec};
    decltype(g_missing_count) m_old_missing_count{g_missing_count};
    std::string               m_old_msg{g_msg};
    std::string_view          m_old_tc_cr{g_tc_CR};
    std::string_view          m_old_tc_ce{g_tc_CE};
};

void SearchActionArticleTest::SetUp()
{
    SearchCommandArticleTest::SetUp();
    g_in_ng = true;
    g_one_command = false;
    g_just_a_sec = 0;
    g_missing_count = 0;
    g_tc_CR = "\r";
    g_tc_CE = "\033[K";
}

void SearchActionArticleTest::TearDown()
{
    g_in_ng = m_old_in_ng;
    g_one_command = m_old_one_command;
    g_check_count = m_old_check_count;
    g_just_a_sec = m_old_just_a_sec;
    g_missing_count = m_old_missing_count;
    g_msg = m_old_msg;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    SearchCommandArticleTest::TearDown();
}

void SearchActionArticleTest::set_action_articles(long current)
{
    set_search_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL},
                        current);
    m_subjects[0].m_flags |= SF_SEL;
    m_subjects[1].m_flags |= SF_SEL;
    m_subjects[2].m_flags |= SF_SEL;
    g_selected_subj_cnt = 3;
    g_selected_only = true;
    g_parsed_art = ArticleNum{3};
}

std::string SearchActionArticleTest::perform_search(std::string_view command, ArticleSwitchResult expected,
                                                    const std::function<void(ArticleNum)> &progress)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                       parsed = g_parsed_art;
    push_char('q');
    push_char('\n');
    const std::string_view input = command.substr(1);
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{command.front(), FINISH_CMD}, progress);

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

class DirectionalSearchActionArticleTest : public SearchActionArticleTest, public testing::WithParamInterface<char>
{
};

class SearchListingArticleTest : public SearchActionArticleTest,
                                 public testing::WithParamInterface<std::pair<char, long>>
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    trn::testing::MockEnvironment m_env;
    int                           m_old_tc_lines{g_tc_LINES};
    int                           m_old_tc_cols{g_tc_COLS};
    int                           m_old_term_scrolled{g_term_scrolled};
};

void SearchListingArticleTest::SetUp()
{
    SearchActionArticleTest::SetUp();
    m_env.expect_no_envar("SUBJLINE");
    g_tc_LINES = 24;
    g_tc_COLS = 80;
}

void SearchListingArticleTest::TearDown()
{
    g_tc_LINES = m_old_tc_lines;
    g_tc_COLS = m_old_tc_cols;
    g_term_scrolled = m_old_term_scrolled;
    SearchActionArticleTest::TearDown();
}

class SearchAheadArticleTest : public SearchCommandArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_subject_articles(std::initializer_list<ArticleFlags> flags, long current);
    std::string next(long number, ArticleSwitchResult expected = AS_NORM,
                     const std::function<void(ArticleNum)> &progress = {});
    std::string dispatch(char command, long number, ArticleSwitchResult expected = AS_NORM,
                         const std::function<void(ArticleNum)> &progress = {}, DoNewsgroupResult exit_code = NG_NORM);

private:
    bool m_old_in_ng{g_in_ng};
    bool m_old_old_subject{g_old_subject};
};

void SearchAheadArticleTest::SetUp()
{
    SearchCommandArticleTest::SetUp();
    g_threaded_group = false;
    g_scan_on = 1;
    g_search_ahead = ArticleNum{-1};
    g_in_ng = true;
    g_old_subject = false;
    g_page_line = 9;
    g_last_pat = "retained search";
    g_force_last = true;
}

void SearchAheadArticleTest::TearDown()
{
    g_in_ng = m_old_in_ng;
    g_old_subject = m_old_old_subject;
    SearchCommandArticleTest::TearDown();
}

void SearchAheadArticleTest::set_subject_articles(std::initializer_list<ArticleFlags> flags, long current)
{
    set_search_articles(flags, current);
    m_subjects[0].m_str = "Re: Target [subject].";
    m_subjects[1].m_str = "Re: Other subject";
    m_subjects[2].m_str = m_subjects[0].m_str;
}

std::string SearchAheadArticleTest::next(long number, ArticleSwitchResult expected,
                                         const std::function<void(ArticleNum)> &progress)
{
    const std::string output = dispatch('n', number, expected, progress);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    return output;
}

std::string SearchAheadArticleTest::dispatch(char command, long number, ArticleSwitchResult expected,
                                             const std::function<void(ArticleNum)> &progress,
                                             DoNewsgroupResult                      exit_code)
{
    const std::pair<ArticleNum, Article *>           current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>           recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                                 first = g_first_art;
    const ArticleNum                                 parsed = g_parsed_art;
    const ArticleUnread                              unread = m_newsgroup.m_to_read;
    const ArticleUnread                              selected = g_selected_count;
    const int                                        selected_subjects = g_selected_subj_cnt;
    const bool                                       selected_only = g_selected_only;
    const bool                                       force_last = g_force_last;
    const std::size_t                                article_count = g_article_list.size();
    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[article_number, article] : g_article_list)
    {
        flags.emplace_back(article_number, article.m_flags);
    }
    std::vector<SubjectFlags> subject_flags;
    for (const Subject &subject : m_subjects)
    {
        subject_flags.push_back(subject.m_flags);
    }
    push_char('q');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{command}, progress);

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(article_find(g_art), g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(force_last, g_force_last);
    EXPECT_EQ(article_count, g_article_list.size());
    EXPECT_EQ("retained search", g_last_pat);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(exit_code, g_newsgroup_exit_code);
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
    for (const auto &[article_number, expected_flags] : flags)
    {
        SCOPED_TRACE(article_number.value_of());
        EXPECT_EQ(expected_flags, article_find(article_number)->m_flags);
    }
    for (int index = 0; index < 3; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(subject_flags[index], m_subjects[index].m_flags);
    }
    return output;
}

constexpr char NEXT_SUBJECT = Ctl('n');
constexpr char PREVIOUS_SUBJECT = Ctl('p');

class SameSubjectArticleTest : public SearchAheadArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    bool          m_old_sa_follow{g_sa_follow};
    bool          m_old_univ_follow{g_univ_follow};
    unsigned char m_old_last_char{g_last_char};
};

void SameSubjectArticleTest::SetUp()
{
    SearchAheadArticleTest::SetUp();
    g_search_ahead = ArticleNum{};
    g_sa_follow = false;
    g_univ_follow = false;
}

void SameSubjectArticleTest::TearDown()
{
    g_sa_follow = m_old_sa_follow;
    g_univ_follow = m_old_univ_follow;
    g_last_char = m_old_last_char;
    SearchAheadArticleTest::TearDown();
}

class CachedNextSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<ArticleFlags>
{
};

class CachedPreviousSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<ArticleFlags>
{
};

class ScanFollowSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<bool>
{
};

class UniversalFollowSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<bool>
{
};

class SelectedFallbackSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<char>
{
};

class ExplicitSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<char>
{
};

class ExhaustedScanSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<char>
{
};

class InterruptedSubjectArticleTest : public SameSubjectArticleTest, public testing::WithParamInterface<char>
{
};

class KillSubjectArticleTest : public SearchCommandArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_kill_articles(long current);
    std::string kill_subject(ArticleSwitchResult expected, long target,
                             const std::function<void(ArticleNum)> &progress = {}, std::string command = "k");

private:
    bool                   m_old_in_ng{g_in_ng};
    bool                   m_old_sa_follow{g_sa_follow};
    int                    m_old_check_count{g_check_count};
    int                    m_old_dm_count{g_dm_count};
    decltype(g_just_a_sec) m_old_just_a_sec{g_just_a_sec};
    std::string            m_old_msg{g_msg};
};

void KillSubjectArticleTest::SetUp()
{
    SearchCommandArticleTest::SetUp();
    g_in_ng = true;
    g_sa_follow = false;
    g_check_count = 10;
    g_dm_count = 1;
    g_just_a_sec = 0;
}

void KillSubjectArticleTest::TearDown()
{
    g_in_ng = m_old_in_ng;
    g_sa_follow = m_old_sa_follow;
    g_check_count = m_old_check_count;
    g_dm_count = m_old_dm_count;
    g_just_a_sec = m_old_just_a_sec;
    g_msg = m_old_msg;
    SearchCommandArticleTest::TearDown();
}

void KillSubjectArticleTest::set_kill_articles(long current)
{
    set_search_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK,
                         AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                         AF_EXISTS},
                        current);
    m_subjects[0].m_str = "Re: Target subject";
    m_subjects[1].m_str = "Re: Other subject";
    m_subjects[2].m_str = "Re: Target subject";
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_SEL | SF_WAS_SELECTED;
    }
    g_selected_only = true;
    g_selected_subj_cnt = 3;
    g_force_last = true;
}

std::string KillSubjectArticleTest::kill_subject(ArticleSwitchResult expected, long target,
                                                 const std::function<void(ArticleNum)> &progress, std::string command)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    push_char('n');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::move(command), progress);

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(ArticleNum{target}, g_art);
    EXPECT_EQ(target <= g_last_art.value_of() ? article_ptr(ArticleNum{target}) : nullptr, g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

class BulkSelectionArticleTest : public KillSubjectArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string apply_bulk(std::string_view input, ArticleSwitchResult expected = AS_ASK);
    void        expect_bulk_unchanged();

private:
    bool                      m_old_one_command{g_one_command};
    decltype(g_missing_count) m_old_missing_count{g_missing_count};
    std::string_view          m_old_tc_cr{g_tc_CR};
    std::string_view          m_old_tc_ce{g_tc_CE};
};

void BulkSelectionArticleTest::SetUp()
{
    KillSubjectArticleTest::SetUp();
    g_one_command = false;
    g_missing_count = 0;
    g_tc_CR = "\r";
    g_tc_CE = "\033[K";
    set_scope({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
              4);
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_SEL;
        subject.m_misc = 1;
    }
    g_selected_subj_cnt = 3;
    g_selected_only = true;
    g_sel_mode = SM_THREAD;
    g_recent_art = ArticleNum{2};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{3};
    g_search_ahead = ArticleNum{4};
    g_reread = true;
    g_force_last = true;
}

void BulkSelectionArticleTest::TearDown()
{
    g_one_command = m_old_one_command;
    g_missing_count = m_old_missing_count;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    KillSubjectArticleTest::TearDown();
}

std::string BulkSelectionArticleTest::apply_bulk(std::string_view input, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    Article **const                        cursor = g_art_ptr;
    push_char('n');
    for (auto it = input.rbegin(); it != input.rend(); ++it)
    {
        push_char(*it);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{':', FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(cursor, g_art_ptr);
    EXPECT_EQ(ArticleNum{3}, g_parsed_art);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('n', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

void BulkSelectionArticleTest::expect_bulk_unchanged()
{
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
}

class BulkArticleOrderTest : public BulkSelectionArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void BulkArticleOrderTest::SetUp()
{
    BulkSelectionArticleTest::SetUp();
    if (GetParam())
    {
        for (const long number : {6, 3, 5, 2, 4, 1, 7})
        {
            g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
        }
        g_art_ptr = g_art_ptr_list.data() + 4;
    }
}

struct EmptyBulkCommandCase
{
    std::string_view name;
    std::string_view input;
};

void PrintTo(const EmptyBulkCommandCase &value, std::ostream *output)
{
    *output << value.name;
}

class EmptyBulkCommandArticleTest : public BulkSelectionArticleTest,
                                    public testing::WithParamInterface<EmptyBulkCommandCase>
{
protected:
    void SetUp() override;
};

void EmptyBulkCommandArticleTest::SetUp()
{
    BulkSelectionArticleTest::SetUp();
    g_msg = "prior status";
    g_perform_count = 19;
}

class PersistentKillArticleTest : public KillSubjectArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;

    std::filesystem::path m_kill_file;
    trn::testing::MockEnvironment m_env;

private:
    std::filesystem::path         m_output_dir;
    KillFileStateFlags            m_old_kf_state{g_kf_state};
    std::FILE                    *m_old_local_kfp{g_local_kfp};
    bool                          m_old_novice_delays{g_novice_delays};
};

void PersistentKillArticleTest::SetUp()
{
    KillSubjectArticleTest::SetUp();
    g_local_kfp = nullptr;
    g_kf_state = KFS_NONE;
    g_novice_delays = false;
    const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
    m_output_dir = std::filesystem::path{TRN_TEST_TMP_DIR} / "PersistentKillArticleTest" / info->name();
    m_kill_file = m_output_dir / "KILL";
    std::error_code error;
    std::filesystem::create_directories(m_output_dir, error);
    ASSERT_FALSE(error) << error.message();
    std::filesystem::remove(m_kill_file, error);
    ASSERT_FALSE(error) << error.message();
    m_env.expect_env_repeatedly("KILLLOCAL", m_kill_file.generic_string().c_str());
}

void PersistentKillArticleTest::TearDown()
{
    if (g_local_kfp != nullptr && g_local_kfp != m_old_local_kfp)
    {
        std::fclose(g_local_kfp);
    }
    g_local_kfp = m_old_local_kfp;
    g_kf_state = m_old_kf_state;
    g_novice_delays = m_old_novice_delays;
    std::error_code error;
    std::filesystem::remove(m_kill_file, error);
    std::filesystem::remove(m_output_dir, error);
    KillSubjectArticleTest::TearDown();
}

class PersistentKillScanArticleTest : public PersistentKillArticleTest,
                                      public testing::WithParamInterface<std::pair<bool, bool>>
{
};

class MemorizeArticleTest : public PersistentKillArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string memorize(char command, std::string_view replies, ArticleSwitchResult expected = AS_ASK);
    void        expect_auto_flags(std::initializer_list<int> flags);
    void        seed_auto_flags();

    std::filesystem::path m_global_file;

private:
    KillFileStateFlags m_old_thread_change_set{g_kfs_thread_change_set};
    int                m_old_change_thread_count{g_kf_change_thread_cnt};
    bool               m_old_kill_thru_kludge{g_kill_thru_kludge};
    HeaderType         m_old_from_header{g_header_type[FROM_LINE]};
    HeaderType         m_old_reply_header{g_header_type[REPLY_LINE]};
    std::string        m_old_head_buf{g_head_buf};
    std::string_view   m_old_tc_ce{g_tc_CE};
    std::string_view   m_old_tc_cr{g_tc_CR};
};

void MemorizeArticleTest::SetUp()
{
    PersistentKillArticleTest::SetUp();
    g_kfs_thread_change_set = KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES;
    g_kf_change_thread_cnt = 0;
    g_kill_thru_kludge = false;
    g_header_type[FROM_LINE].flags |= HT_CACHED;
    g_tc_CE = "\033[K";
    g_tc_CR = "\r";
    m_global_file = m_kill_file.parent_path() / "GLOBAL";
    std::error_code error;
    std::filesystem::remove(m_global_file, error);
    ASSERT_FALSE(error) << error.message();
    m_env.expect_env_repeatedly("KILLGLOBAL", m_global_file.generic_string().c_str());
    set_kill_articles(2);
    for (long number = 1; number <= 7; ++number)
    {
        article_ptr(ArticleNum{number})->m_from =
            number == 2 || number == 4 ? "reader@example.org" : "other@example.org";
    }
}

void MemorizeArticleTest::TearDown()
{
    g_kfs_thread_change_set = m_old_thread_change_set;
    g_kf_change_thread_cnt = m_old_change_thread_count;
    g_kill_thru_kludge = m_old_kill_thru_kludge;
    g_header_type[FROM_LINE] = m_old_from_header;
    g_header_type[REPLY_LINE] = m_old_reply_header;
    g_head_buf = m_old_head_buf;
    g_tc_CE = m_old_tc_ce;
    g_tc_CR = m_old_tc_cr;
    std::error_code error;
    std::filesystem::remove(m_global_file, error);
    PersistentKillArticleTest::TearDown();
}

std::string MemorizeArticleTest::memorize(char command, std::string_view replies, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    push_char('n');
    for (auto it = replies.rbegin(); it != replies.rend(); ++it)
    {
        push_char(*it);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{command});

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(expected, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

void MemorizeArticleTest::expect_auto_flags(std::initializer_list<int> flags)
{
    long number = 0;
    for (const int expected : flags)
    {
        SCOPED_TRACE(++number);
        EXPECT_EQ(expected, article_ptr(ArticleNum{number})->m_auto_flags);
    }
    EXPECT_EQ(7, number);
}

void MemorizeArticleTest::seed_auto_flags()
{
    for (long number = 1; number <= 7; ++number)
    {
        article_ptr(ArticleNum{number})->m_auto_flags = AUTO_SEL_1 | AUTO_OLD;
    }
}

class MemorizeScopeArticleTest : public MemorizeArticleTest, public testing::WithParamInterface<char>
{
};

class KillThreadFallbackArticleTest : public KillSubjectArticleTest
{
};

class KillSubjectScanArticleTest : public KillSubjectArticleTest,
                                   public testing::WithParamInterface<std::pair<bool, bool>>
{
};

class KillSubjectSearchArticleTest : public KillSubjectArticleTest, public testing::WithParamInterface<bool>
{
};

void SearchCommandArticleTest::expect_interrupted_search(long current)
{
    set_search_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                         AF_EXISTS | AF_UNREAD},
                        current);
    g_selected_only = true;
    g_selected_subj_cnt = 2;
    m_subjects[0].m_flags |= SF_SEL;
    m_subjects[2].m_flags |= SF_SEL;
    g_parsed_art = ArticleNum{3};
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        for (const char direction : {'/', '?'})
        {
            SCOPED_TRACE(direction);
            g_art = g_curr_art = ArticleNum{current};
            g_artp = g_curr_artp = g_art <= g_last_art ? article_ptr(g_art) : nullptr;
            g_search_ahead = ArticleNum{4};
            std::vector<ArticleNum> visited;
            const auto              progress = [&](ArticleNum article)
            {
                EXPECT_EQ(0, g_int_count);
                visited.push_back(article);
                if (visited.size() == 2)
                {
                    EXPECT_EQ(article_ptr(visited.front()), g_artp);
                    g_int_count = 1;
                }
            };

            const std::string output = search_articles(direction, "absent", AS_ASK, progress);

            const long first = direction == '/' ? (current > g_last_art.value_of() ? 1 : current + 1) : current - 1;
            const long interrupted = first + (direction == '/' ? 1 : -1);
            EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{first}, ArticleNum{interrupted}}), visited);
            EXPECT_NE(std::string::npos, output.find(std::string{verbose ? "(Interrupted at article " : "(Intr at "} +
                                                     std::to_string(interrupted) + ")"));
            EXPECT_EQ(std::string::npos, output.find("Not found."));
            EXPECT_EQ(ArticleNum{current}, g_art);
            EXPECT_EQ(g_curr_artp, g_artp);
            EXPECT_EQ(0, g_int_count);
            EXPECT_EQ("absent", g_last_pat);
            EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
            EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
            EXPECT_EQ(SF_VISIT, m_subjects[1].m_flags);
            EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
            EXPECT_FALSE(g_force_last);
        }
    }
}

class NextUnreadArticleTest : public ArticleNavigationTest
{
};

class SortedArticleNavigationTest : public ArticleStateTest
{
protected:
    void set_order(std::initializer_list<long> order);
    void        set_position_after_navigation(long number, long index);
    void expect_next_unread(long number, long index, ArticleSwitchResult result = AS_NORM);
    void expect_next_article(long number, long index);
    std::string expect_previous(char command, long number, long index, ArticleSwitchResult result = AS_NORM);

private:
    std::string expect_navigation(char command, long number, long index, ArticleSwitchResult result);
};

void SortedArticleNavigationTest::set_order(std::initializer_list<long> order)
{
    for (const long number : order)
    {
        g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
    }
    g_art_ptr = nullptr;
    g_recent_art = g_last_art;
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{1};
    g_reread = true;
    g_force_last = true;
}

void SortedArticleNavigationTest::set_position_after_navigation(long number, long index)
{
    g_art = ArticleNum{number};
    g_artp = g_art <= g_last_art ? article_ptr(g_art) : nullptr;
    g_art_ptr = article_ptr_list_begin() + index;
    g_s_follow_temp = true;
    g_univ_follow_temp = true;
}

void SortedArticleNavigationTest::expect_next_unread(long number, long index, ArticleSwitchResult result)
{
    expect_navigation('n', number, index, result);
}

void SortedArticleNavigationTest::expect_next_article(long number, long index)
{
    g_reread = false;
    g_force_last = false;
    expect_navigation('N', number, index, AS_NORM);
}

std::string SortedArticleNavigationTest::expect_previous(char command, long number, long index,
                                                         ArticleSwitchResult result)
{
    g_reread = false;
    g_force_last = false;
    if (command == 'p' && result == AS_NORM)
    {
        g_parsed_art = ArticleNum{number};
    }
    return expect_navigation(command, number, index, result);
}

std::string SortedArticleNavigationTest::expect_navigation(char command, long number, long index,
                                                           ArticleSwitchResult result)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>           order = g_art_ptr_list;
    const ArticleUnread                    unread = m_newsgroup.m_to_read;
    const ArticleUnread                    selected = g_selected_count;
    const ArticleNum                       first = g_first_art;
    const ArticleNum                       parsed = g_parsed_art;
    const ArticleNum                       search_ahead = g_search_ahead;
    const SubjectFlags                     subject_flags = g_first_subject->m_flags;
    const bool selected_only = g_selected_only;
    const bool reread = g_reread;
    const bool force_last = g_force_last;

    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[article_number, article] : g_article_list)
    {
        flags.emplace_back(article_number, article.m_flags);
    }
    testing::internal::CaptureStdout();
    const ArticleSwitchResult actual = art_switch(std::string{command});
    const std::string         output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(result, actual);

    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(number <= g_last_art.value_of() ? article_ptr(ArticleNum{number}) : nullptr, g_artp);
    EXPECT_EQ(article_ptr_list_begin() + index, g_art_ptr);
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(subject_flags, g_first_subject->m_flags);
    EXPECT_EQ(selected_only, g_selected_only);
    const bool explicit_reread =
        (command == 'N' || command == 'P') && result == AS_NORM && number <= g_last_art.value_of();
    EXPECT_EQ(explicit_reread || reread, g_reread);
    EXPECT_EQ(command == 'N' && number > g_last_art.value_of() ? true : force_last, g_force_last);
    EXPECT_EQ(result == AS_CLEAN || (command == 'P' && result == AS_ASK) ? search_ahead : ArticleNum{}, g_search_ahead);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    for (const auto &[article_number, expected_flags] : flags)
    {
        SCOPED_TRACE(article_number.value_of());
        EXPECT_EQ(expected_flags, article_ptr(article_number)->m_flags);
    }
    return output;
}

class SortedPreviousArticleNavigationTest : public SortedArticleNavigationTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SortedPreviousArticleNavigationTest::SetUp()
{
    SortedArticleNavigationTest::SetUp();
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 3);
    set_order({2, 4, 1, 3});
    g_threaded_group = GetParam();
    g_search_ahead = GetParam() ? ArticleNum{2} : ArticleNum{};
}

class SortedPreviousCursorRepairArticleNavigationTest : public SortedPreviousArticleNavigationTest
{
protected:
    void SetUp() override;
};

void SortedPreviousCursorRepairArticleNavigationTest::SetUp()
{
    SortedPreviousArticleNavigationTest::SetUp();
    g_art_ptr = article_ptr_list_begin() + 2;
    g_search_ahead = ArticleNum{};
    g_s_follow_temp = true;
    g_univ_follow_temp = true;
}

class SortedPreviousBoundaryArticleNavigationTest : public SortedArticleNavigationTest,
                                                    public testing::WithParamInterface<char>
{
protected:
    void SetUp() override;

    int m_line{};
};

void SortedPreviousBoundaryArticleNavigationTest::SetUp()
{
    SortedArticleNavigationTest::SetUp();
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 2);
    set_order({2, 4, 1, 3});
    g_selected_only = true;
    g_parsed_art = ArticleNum{2};
    g_search_ahead = ArticleNum{2};
    m_line = g_term_line;
}

class SortedForwardArticleNavigationTest : public SortedArticleNavigationTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
    void set_forward_articles(std::initializer_list<ArticleFlags> flags);
};

void SortedForwardArticleNavigationTest::SetUp()
{
    SortedArticleNavigationTest::SetUp();
    g_threaded_group = GetParam();
}

void SortedForwardArticleNavigationTest::set_forward_articles(std::initializer_list<ArticleFlags> flags)
{
    set_articles(flags, 4);
    set_order({2, 4, 1, 3});
}

class SortedExplicitForwardArticleNavigationTest : public SortedForwardArticleNavigationTest
{
protected:
    void SetUp() override;
};

void SortedExplicitForwardArticleNavigationTest::SetUp()
{
    SortedForwardArticleNavigationTest::SetUp();
    set_forward_articles(
        {AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL});
    g_selected_only = true;
    g_search_ahead = ArticleNum{2};
}

class SortedExhaustedArticleNavigationTest : public SortedArticleNavigationTest,
                                             public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SortedExhaustedArticleNavigationTest::SetUp()
{
    SortedArticleNavigationTest::SetUp();
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 2);
    set_order({3, 1, 2});
    if (GetParam())
    {
        set_position_after_navigation(4, 0);
    }
}

class SortedRestartArticleNavigationTest : public SortedArticleNavigationTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SortedRestartArticleNavigationTest::SetUp()
{
    SortedArticleNavigationTest::SetUp();
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS}, 4);
    set_order({3, 1, 2});
    g_selected_only = true;
    g_art_ptr = article_ptr_list_begin() + 1;
    if (GetParam())
    {
        set_position_after_navigation(3, 0);
    }
}

class SortedSingleEntryArticleNavigationTest : public SortedArticleNavigationTest,
                                               public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SortedSingleEntryArticleNavigationTest::SetUp()
{
    SortedArticleNavigationTest::SetUp();
    set_articles({AF_EXISTS}, 1);
    set_order({1});
    if (GetParam())
    {
        set_position_after_navigation(2, 0);
    }
}

class ArticleTraversalTest : public UnreadScopeArticleTest
{
protected:
    void        expect_next(char command, long number, std::initializer_list<long> missing = {},
                            ArticleSwitchResult result = AS_NORM);
    std::string expect_previous(char command, long number, std::initializer_list<long> missing = {},
                                ArticleSwitchResult result = AS_NORM);

private:
    std::string expect_navigation(char command, long number, std::initializer_list<long> missing,
                                  ArticleSwitchResult result);
};

class BranchingArticleTest : public ArticleTraversalTest
{
protected:
    void set_tree(std::initializer_list<ArticleFlags> flags, long current, bool breadth_first = true);
};

void BranchingArticleTest::set_tree(std::initializer_list<ArticleFlags> flags, long current, bool breadth_first)
{
    ASSERT_EQ(12U, flags.size());
    set_articles(flags, current);
    g_breadth_first = breadth_first;
    g_sel_mode = SM_THREAD;
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{1};
    g_first_subject = &m_subjects[0];
    g_last_subject = &m_subjects[1];
    struct ArticleLinks
    {
        long parent;
        long child;
        long sibling;
    };
    // Children: 1:{2,6,7}, 2:{3,5}, 3:{4}, 7:{8,10}, 8:{9}; 11:{12}.
    // Levels: 1 / 2,6,7 / 3,5,8,10 / 4,9; then thread 11,12.
    const ArticleLinks links[]{{0, 2, 0}, {1, 3, 6},  {2, 4, 5}, {3, 0, 0}, {2, 0, 0},  {1, 0, 7},
                               {1, 8, 0}, {7, 9, 10}, {8, 0, 0}, {7, 0, 0}, {0, 12, 0}, {11, 0, 0}};
    for (int index = 0; index < 2; ++index)
    {
        Subject &subject = m_subjects[index];
        subject.m_articles = article_ptr(ArticleNum{index ? 11 : 1});
        subject.m_thread = subject.m_articles;
        subject.m_thread_link = &subject;
        subject.m_prev = index ? &m_subjects[0] : nullptr;
        subject.m_next = index ? nullptr : &m_subjects[1];
    }
    for (long number = 1; number <= 12; ++number)
    {
        Article            *article = article_ptr(ArticleNum{number});
        const ArticleLinks &link = links[number - 1];
        article->m_parent = link.parent ? article_ptr(ArticleNum{link.parent}) : nullptr;
        article->m_child1 = link.child ? article_ptr(ArticleNum{link.child}) : nullptr;
        article->m_sibling = link.sibling ? article_ptr(ArticleNum{link.sibling}) : nullptr;
        article->m_subj_next = number == 10 || number == 12 ? nullptr : article_ptr(ArticleNum{number + 1});
        article->m_subj = &m_subjects[number > 10];
        if (article->m_flags & AF_UNREAD)
        {
            article->m_subj->m_flags |= SF_VISIT;
        }
        if (article->m_flags & AF_SEL)
        {
            article->m_subj->m_flags |= SF_SEL;
        }
    }
    for (int index = 0; index < 2; ++index)
    {
        g_selected_subj_cnt += (m_subjects[index].m_flags & SF_SEL) != 0;
    }
}

void ArticleTraversalTest::expect_next(char command, long number, std::initializer_list<long> missing,
                                       ArticleSwitchResult result)
{
    EXPECT_TRUE(expect_navigation(command, number, missing, result).empty());
}

std::string ArticleTraversalTest::expect_previous(char command, long number, std::initializer_list<long> missing,
                                                  ArticleSwitchResult result)
{
    if (command == 'p' && result == AS_NORM)
    {
        g_parsed_art = ArticleNum{number};
    }
    return expect_navigation(command, number, missing, result);
}

std::string ArticleTraversalTest::expect_navigation(char command, long number, std::initializer_list<long> missing,
                                                    ArticleSwitchResult result)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                       first = g_first_art;
    const ArticleNum                       parsed = g_parsed_art;
    const ArticleNum                       search_ahead = g_search_ahead;
    const SelectionMode                    selection_mode = g_sel_mode;
    const int     selected_subjects = g_selected_subj_cnt;
    const bool    selected_only = g_selected_only;
    const bool    threaded_group = g_threaded_group;
    const bool    breadth_first = g_breadth_first;
    ArticleUnread unread = m_newsgroup.m_to_read;
    ArticleUnread selected = g_selected_count;

    std::vector<SubjectFlags> subject_flags;
    for (const Subject &subject : m_subjects)
    {
        subject_flags.push_back(subject.m_flags);
    }
    std::vector<ArticleFlags> flags;
    for (long article = 1; article <= 12; ++article)
    {
        flags.push_back(article_ptr(ArticleNum{article})->m_flags);
    }
    for (const long article : missing)
    {
        ArticleFlags &expected = flags[article - 1];
        EXPECT_FALSE(expected & AF_EXISTS);
        if (expected & AF_UNREAD)
        {
            --unread;
            selected -= (expected & AF_SEL) != 0;
            expected &= ~(AF_UNREAD | AF_SEL);
        }
    }
    g_reread = false;
    g_force_last = false;

    testing::internal::CaptureStdout();
    const ArticleSwitchResult actual = art_switch(std::string{command});
    const std::string         output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(result, actual);

    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(number <= 12 ? article_ptr(ArticleNum{number}) : nullptr, g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(threaded_group, g_threaded_group);
    EXPECT_EQ(breadth_first, g_breadth_first);
    EXPECT_EQ(selection_mode, g_sel_mode);
    for (int index = 0; index < 3; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(subject_flags[index], m_subjects[index].m_flags);
    }
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ((command == 'N' || command == 'P') && number <= 12 && result == AS_NORM, g_reread);
    EXPECT_EQ(command == 'N' && number > 12, g_force_last);
    EXPECT_EQ(result == AS_CLEAN || (command == 'P' && result == AS_ASK) ? search_ahead : ArticleNum{}, g_search_ahead);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_TRUE(g_art_ptr_list.empty());
    EXPECT_EQ(nullptr, g_art_ptr);
    for (long article = 1; article <= 12; ++article)
    {
        SCOPED_TRACE(article);
        EXPECT_EQ(flags[article - 1], article_ptr(ArticleNum{article})->m_flags);
    }
    return output;
}

class BreadthFirstArticleTest : public BranchingArticleTest
{
};

class BreadthFirstPreviousBoundaryArticleTest : public BreadthFirstArticleTest, public testing::WithParamInterface<char>
{
protected:
    void SetUp() override;

    int m_line{};
};

void BreadthFirstPreviousBoundaryArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS},
             1);
    g_selected_only = true;
    g_search_ahead = ArticleNum{2};
    m_line = g_term_line;
}

struct BreadthFirstSelectedNextCase
{
    std::string_view name;
    long             current;
    long             target;
};

const BreadthFirstSelectedNextCase BREADTH_FIRST_SELECTED_NEXT_CASES[]{{"RestartFromGroupEnd", 13, 8},
                                                                       {"NextSelectedLevel", 8, 4},
                                                                       {"NextSelectedThread", 4, 12},
                                                                       {"WrapToFirstSelectedArticle", 12, 8}};

void PrintTo(const BreadthFirstSelectedNextCase &value, std::ostream *output)
{
    *output << value.name;
}

class BreadthFirstSelectedNextArticleTest : public BreadthFirstArticleTest,
                                            public testing::WithParamInterface<BreadthFirstSelectedNextCase>
{
protected:
    void SetUp() override;
};

void BreadthFirstSelectedNextArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             GetParam().current);
    g_selected_only = true;
    g_s_follow_temp = GetParam().current <= 12;
    g_univ_follow_temp = GetParam().current <= 12;
}

struct BreadthFirstAllReadBoundaryCase
{
    std::string_view    name;
    long                current;
    char                command;
    long                target;
    ArticleSwitchResult result;
};

const BreadthFirstAllReadBoundaryCase BREADTH_FIRST_ALL_READ_BOUNDARY_CASES[]{{"ReachGroupEnd", 12, 'n', 13, AS_NORM},
                                                                              {"RestartReading", 13, 'N', 1, AS_NORM},
                                                                              {"LeaveGroup", 13, 'n', 13, AS_CLEAN}};

void PrintTo(const BreadthFirstAllReadBoundaryCase &value, std::ostream *output)
{
    *output << value.name;
}

class BreadthFirstAllReadBoundaryArticleTest : public BreadthFirstArticleTest,
                                               public testing::WithParamInterface<BreadthFirstAllReadBoundaryCase>
{
protected:
    void SetUp() override;
};

void BreadthFirstAllReadBoundaryArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS},
             GetParam().current <= 12 ? GetParam().current : 1);
    if (GetParam().current > 12)
    {
        g_s_follow_temp = true;
        g_univ_follow_temp = true;
        ASSERT_EQ(AS_NORM, art_switch("$"));
    }
}

struct BreadthFirstUnreadCase
{
    std::string_view                    name;
    std::initializer_list<ArticleFlags> flags;
    std::initializer_list<long>         order;
    long                                wrap_target;
    bool                                selected_only;
    long                                search_ahead;
};

const BreadthFirstUnreadCase BREADTH_FIRST_UNREAD_CASES[]{
    {"AllUnread",
     {AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
      AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
      AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
     {2, 6, 7, 3, 5, 8, 10, 4, 9, 11, 12},
     1,
     false,
     2},
    {"ReadParents",
     {AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
      AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD},
     {6, 3, 8, 4, 9, 12},
     6,
     false,
     0},
    {"SelectedOnly",
     {AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
      AF_EXISTS | AF_UNREAD | AF_SEL},
     {7, 3, 8, 10, 4, 12},
     1,
     true,
     0}};

void PrintTo(const BreadthFirstUnreadCase &value, std::ostream *output)
{
    *output << value.name;
}

class BreadthFirstUnreadArticleTest : public BreadthFirstArticleTest,
                                      public testing::WithParamInterface<BreadthFirstUnreadCase>
{
protected:
    void SetUp() override;
};

void BreadthFirstUnreadArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree(GetParam().flags, 1);
    g_selected_only = GetParam().selected_only;
    g_search_ahead = ArticleNum{GetParam().search_ahead};
}

class BreadthFirstUnreadWrapArticleTest : public BreadthFirstUnreadArticleTest
{
protected:
    void SetUp() override;
};

void BreadthFirstUnreadWrapArticleTest::SetUp()
{
    BreadthFirstUnreadArticleTest::SetUp();
    g_art = ArticleNum{*(GetParam().order.end() - 1)};
    g_artp = article_ptr(g_art);
    g_search_ahead = ArticleNum{};
    g_s_follow_temp = true;
    g_univ_follow_temp = true;
}

class BreadthFirstExplicitNextArticleTest : public BreadthFirstArticleTest
{
protected:
    void SetUp() override;
};

void BreadthFirstExplicitNextArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_SEL,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
              AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             1);
    g_selected_only = true;
}

class BreadthFirstExplicitNextBoundaryArticleTest : public BreadthFirstExplicitNextArticleTest,
                                                    public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void BreadthFirstExplicitNextBoundaryArticleTest::SetUp()
{
    BreadthFirstExplicitNextArticleTest::SetUp();
    g_art = ArticleNum{GetParam() ? 13 : 12};
    g_artp = g_art <= g_last_art ? article_ptr(g_art) : nullptr;
    g_s_follow_temp = true;
    g_univ_follow_temp = true;
}

struct DepthFirstSkipCase
{
    std::string_view name;
    long             current;
    int              depth;
    long             target;
    int              target_depth;
};

const DepthFirstSkipCase DEPTH_FIRST_SKIP_CASES[]{
    {"ChildBranch", 2, 1, 6, 1}, {"Sibling", 3, 2, 5, 2}, {"AncestorSibling", 5, 2, 6, 1}};

void PrintTo(const DepthFirstSkipCase &value, std::ostream *output)
{
    *output << value.name;
}

class DepthFirstSkipArticleTest : public BreadthFirstArticleTest, public testing::WithParamInterface<DepthFirstSkipCase>
{
protected:
    void SetUp() override;

    Article *m_cursor{};
    int      m_depth{};
};

void DepthFirstSkipArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS},
             1);
    m_cursor = article_ptr(ArticleNum{GetParam().current});
    m_depth = GetParam().depth;
}

class BreadthFirstLevelArticleTest : public BreadthFirstArticleTest
{
protected:
    void SetUp() override;

    Article *m_root{};
};

void BreadthFirstLevelArticleTest::SetUp()
{
    BreadthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS},
             1);
    m_root = article_ptr(ArticleNum{1});
    m_root->m_sibling = article_ptr(ArticleNum{11});
}

struct NextOnLevelCase
{
    std::string_view name;
    long             current;
    int              level;
    long             target;
};

const NextOnLevelCase NEXT_ON_LEVEL_CASES[]{{"RootSibling", 1, 0, 11},
                                            {"ExhaustedRootLevel", 11, 0, 0},
                                            {"FirstChild", 1, 1, 2},
                                            {"FirstGrandchild", 1, 2, 3},
                                            {"GrandchildSibling", 3, 0, 5},
                                            {"GrandchildAcrossBranches", 5, 0, 8},
                                            {"LastGrandchild", 8, 0, 10},
                                            {"ExhaustedGrandchildLevel", 10, 0, 0},
                                            {"FirstDeepestChild", 1, 3, 4},
                                            {"DeepestChildAcrossBranches", 4, 0, 9},
                                            {"ExhaustedDeepestLevel", 9, 0, 0},
                                            {"NonexistentLevel", 1, 4, 0}};

void PrintTo(const NextOnLevelCase &value, std::ostream *output)
{
    *output << value.name;
}

class BreadthFirstNextOnLevelArticleTest : public BreadthFirstLevelArticleTest,
                                           public testing::WithParamInterface<NextOnLevelCase>
{
protected:
    void SetUp() override;

    Article *m_cursor{};
};

void BreadthFirstNextOnLevelArticleTest::SetUp()
{
    BreadthFirstLevelArticleTest::SetUp();
    m_cursor = article_ptr(ArticleNum{GetParam().current});
}

struct LastOnLevelCase
{
    std::string_view name;
    int              level;
    long             before;
    long             target;
};

const LastOnLevelCase LAST_ON_LEVEL_CASES[]{{"RootTail", 0, 0, 11},
                                            {"ChildTail", 1, 0, 12},
                                            {"GrandchildTail", 2, 0, 10},
                                            {"DeepestTail", 3, 0, 9},
                                            {"NonexistentLevel", 4, 0, 0},
                                            {"BeforeFirstRoot", 0, 1, 0},
                                            {"BeforeFirstGrandchild", 2, 3, 0},
                                            {"RootPredecessor", 0, 11, 1},
                                            {"ChildPredecessor", 1, 12, 7},
                                            {"GrandchildPredecessor", 2, 8, 5},
                                            {"DeepestPredecessor", 3, 9, 4}};

void PrintTo(const LastOnLevelCase &value, std::ostream *output)
{
    *output << value.name;
}

class BreadthFirstLastOnLevelArticleTest : public BreadthFirstLevelArticleTest,
                                           public testing::WithParamInterface<LastOnLevelCase>
{
protected:
    void SetUp() override;

    const Article *m_before{};
};

void BreadthFirstLastOnLevelArticleTest::SetUp()
{
    BreadthFirstLevelArticleTest::SetUp();
    m_before = GetParam().before ? article_ptr(ArticleNum{GetParam().before}) : nullptr;
}

class DepthFirstArticleTest : public BranchingArticleTest
{
protected:
    void set_tree(std::initializer_list<ArticleFlags> flags, long current);
};

void DepthFirstArticleTest::set_tree(std::initializer_list<ArticleFlags> flags, long current)
{
    BranchingArticleTest::set_tree(flags, current, false);
    // Depth-first order: 1,7,8,9,10,2,3,5,4,6; then thread 11,12.
    article_ptr(ArticleNum{1})->m_child1 = article_ptr(ArticleNum{7});
    article_ptr(ArticleNum{7})->m_sibling = article_ptr(ArticleNum{2});
    article_ptr(ArticleNum{6})->m_sibling = nullptr;
    article_ptr(ArticleNum{3})->m_child1 = nullptr;
    article_ptr(ArticleNum{5})->m_child1 = article_ptr(ArticleNum{4});
    article_ptr(ArticleNum{4})->m_parent = article_ptr(ArticleNum{5});
}

class DepthFirstPreviousBoundaryArticleTest : public DepthFirstArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;

    int m_line{};
};

void DepthFirstPreviousBoundaryArticleTest::SetUp()
{
    DepthFirstArticleTest::SetUp();
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD,
              AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             3);
    g_first_art = ArticleNum{3};
    g_selected_only = true;
    g_verbose = GetParam();
    g_search_ahead = ArticleNum{2};
    m_line = g_term_line;
}

class DepthFirstMissingArticleTest : public DepthFirstArticleTest, public testing::WithParamInterface<char>
{
};

class DirectNavigationArticleTest : public BranchingArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        position(long number);
    void        set_flags(long number, ArticleFlags flags);
    void        set_order(std::initializer_list<long> order);
    std::string navigate(std::string command, long target, bool reread, ArticleSwitchResult result = AS_NORM,
                         long search_ahead = 0, long order_index = -1, char typed_second = '\0');

    static const ArticleFlags CACHED;

private:
    char             m_old_erase_char{g_erase_char};
    char             m_old_kill_char{g_kill_char};
    std::string_view m_old_tc_bc{g_tc_BC};
    bool             m_old_mouse_is_down{g_mouse_is_down};
    char             m_old_int_count{g_int_count};
};

const ArticleFlags DirectNavigationArticleTest::CACHED = AF_EXISTS | AF_CACHED;

void DirectNavigationArticleTest::SetUp()
{
    BranchingArticleTest::SetUp();
    set_tree({CACHED | AF_UNREAD | AF_SEL, CACHED, CACHED | AF_UNREAD, CACHED | AF_UNREAD | AF_SEL, AF_CACHED,
              CACHED | AF_UNREAD | AF_SEL, CACHED | AF_YANK_BACK, CACHED | AF_UNREAD, CACHED | AF_UNREAD, CACHED,
              CACHED | AF_UNREAD | AF_SEL, CACHED | AF_UNREAD | AF_SEL},
             4, false);
    g_selected_only = true;
    g_recent_art = ArticleNum{7};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{4};
    g_search_ahead = ArticleNum{3};
    g_erase_char = '\b';
    g_kill_char = Ctl('U');
    g_tc_BC = "\b";
    g_mouse_is_down = false;
    g_int_count = 0;
}

void DirectNavigationArticleTest::TearDown()
{
    g_erase_char = m_old_erase_char;
    g_kill_char = m_old_kill_char;
    g_tc_BC = m_old_tc_bc;
    g_mouse_is_down = m_old_mouse_is_down;
    g_int_count = m_old_int_count;
    BranchingArticleTest::TearDown();
}

void DirectNavigationArticleTest::position(long number)
{
    g_art = g_curr_art = ArticleNum{number};
    g_artp = g_curr_artp = article_find(g_art);
    g_mode = g_art > g_last_art ? MM_ARTICLE_END : MM_ARTICLE;
}

void DirectNavigationArticleTest::set_flags(long number, ArticleFlags flags)
{
    Article *article = article_ptr(ArticleNum{number});
    m_newsgroup.m_to_read += ((flags & AF_UNREAD) != 0) - ((article->m_flags & AF_UNREAD) != 0);
    g_selected_count += ((flags & AF_SEL) != 0) - ((article->m_flags & AF_SEL) != 0);
    article->m_flags = flags;
}

void DirectNavigationArticleTest::set_order(std::initializer_list<long> order)
{
    for (const long number : order)
    {
        g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
    }
    g_art_ptr = std::find(g_art_ptr_list.data(), g_art_ptr_list.data() + g_art_ptr_list.size(), g_artp);
}

std::string DirectNavigationArticleTest::navigate(std::string command, long target, bool reread,
                                                  ArticleSwitchResult result, long search_ahead, long order_index,
                                                  char typed_second)
{
    const std::pair<ArticleNum, Article *>           current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>           recent = std::make_pair(g_recent_art, g_recent_artp);
    const std::vector<Article *>                     order = g_art_ptr_list;
    Article **const                                  cursor = g_art_ptr;
    const ArticleNum                                 first = g_first_art;
    const ArticleNum                                 parsed = g_parsed_art;
    const ArticleUnread                              unread = m_newsgroup.m_to_read;
    const ArticleUnread                              selected = g_selected_count;
    const int                                        selected_subjects = g_selected_subj_cnt;
    const bool                                       selected_only = g_selected_only;
    const bool                                       force_last = g_force_last;
    const bool                                       threaded = g_threaded_group;
    const bool                                       breadth_first = g_breadth_first;
    const MinorMode                                  mode = g_mode;
    const std::pair<bool, bool>                      follow = std::make_pair(g_s_follow_temp, g_univ_follow_temp);
    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[number, article] : g_article_list)
    {
        flags.emplace_back(number, article.m_flags);
    }
    std::vector<SubjectFlags> subjects;
    for (const Subject &subject : m_subjects)
    {
        subjects.push_back(subject.m_flags);
    }
    push_char('Q');
    if (typed_second)
    {
        push_char(typed_second);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult actual = art_switch(std::move(command));

    const std::string output = testing::internal::GetCapturedStdout();
    SCOPED_TRACE(output);
    EXPECT_EQ(result, actual);
    EXPECT_EQ(ArticleNum{target}, g_art);
    EXPECT_EQ(article_find(ArticleNum{target}), g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(order, g_art_ptr_list);
    EXPECT_EQ(order_index < 0 ? cursor : article_ptr_list_begin() + order_index, g_art_ptr);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(force_last, g_force_last);
    EXPECT_EQ(threaded, g_threaded_group);
    EXPECT_EQ(breadth_first, g_breadth_first);
    EXPECT_EQ(reread, g_reread);
    EXPECT_EQ(ArticleNum{search_ahead}, g_search_ahead);
    EXPECT_EQ(follow, std::make_pair(g_s_follow_temp, g_univ_follow_temp));
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(mode, g_mode);
    EXPECT_EQ(flags.size(), g_article_list.size());
    for (const auto &[number, expected_flags] : flags)
    {
        SCOPED_TRACE(number.value_of());
        const Article *article = article_find(number);
        EXPECT_NE(nullptr, article);
        if (article)
        {
            EXPECT_EQ(expected_flags, article->m_flags);
        }
    }
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(subjects[index], m_subjects[index].m_flags);
    }
    EXPECT_TRUE(macro_pending());
    if (macro_pending())
    {
        EXPECT_EQ('Q', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

class DirectNumericStepArticleTest : public DirectNavigationArticleTest, public testing::WithParamInterface<char>
{
};

class DirectNumericInputArticleTest : public DirectNavigationArticleTest, public testing::WithParamInterface<char>
{
};

class DirectNumericMissingArticleTest : public DirectNavigationArticleTest, public testing::WithParamInterface<char>
{
};

class DirectNumericBoundaryArticleTest : public DirectNavigationArticleTest, public testing::WithParamInterface<bool>
{
};

enum class DirectReadingOrder
{
    Numeric,
    DepthFirst,
    BreadthFirst,
    Subject,
    Sorted
};

class DirectFirstUnreadArticleTest : public DirectNavigationArticleTest,
                                     public testing::WithParamInterface<DirectReadingOrder>
{
};

class DirectFirstExhaustionArticleTest : public DirectNavigationArticleTest, public testing::WithParamInterface<bool>
{
};

class SubjectOrderPreviousArticleTest : public ArticleTraversalTest
{
protected:
    void        set_subjects(std::initializer_list<ArticleFlags> flags, long current);
    std::string expect_subject_previous(char command, long number, std::initializer_list<long> missing = {},
                                        ArticleSwitchResult result = AS_NORM);
};

void SubjectOrderPreviousArticleTest::set_subjects(std::initializer_list<ArticleFlags> flags, long current)
{
    ASSERT_EQ(12U, flags.size());
    set_articles(flags, current);
    g_threaded_group = false;
    g_sel_mode = SM_SUBJECT;
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{1};
    g_first_subject = &m_subjects[0];
    g_last_subject = &m_subjects[2];
    g_selected_count = ArticleUnread{};
    // Each subject's articles are numeric; the subject order is not.
    const long order[][4]{{5, 7, 10, 12}, {1, 3, 8, 11}, {2, 4, 6, 9}};
    for (int index = 0; index < 3; ++index)
    {
        Subject &subject = m_subjects[index];
        subject.m_articles = article_ptr(ArticleNum{order[index][0]});
        subject.m_prev = index ? &m_subjects[index - 1] : nullptr;
        subject.m_next = index < 2 ? &m_subjects[index + 1] : nullptr;
        for (int offset = 0; offset < 4; ++offset)
        {
            Article *article = article_ptr(ArticleNum{order[index][offset]});
            article->m_subj = &subject;
            article->m_subj_next = offset < 3 ? article_ptr(ArticleNum{order[index][offset + 1]}) : nullptr;
            if (article->m_flags & AF_UNREAD)
            {
                subject.m_flags |= SF_VISIT;
                if (article->m_flags & AF_SEL)
                {
                    ++g_selected_count;
                    subject.m_flags |= SF_SEL;
                }
            }
        }
        g_selected_subj_cnt += (subject.m_flags & SF_SEL) != 0;
    }
}

std::string SubjectOrderPreviousArticleTest::expect_subject_previous(char command, long number,
                                                                     std::initializer_list<long> missing,
                                                                     ArticleSwitchResult         result)
{
    // Each command starts in search-ahead mode; successful p/P cancels it.
    g_search_ahead = ArticleNum{-1};
    return expect_previous(command, number, missing, result);
}

class SubjectOrderPreviousCommandArticleTest : public SubjectOrderPreviousArticleTest,
                                               public testing::WithParamInterface<char>
{
};

class SubjectOrderPreviousBoundaryArticleTest : public SubjectOrderPreviousArticleTest,
                                                public testing::WithParamInterface<std::tuple<char, bool>>
{
protected:
    void SetUp() override;

    int m_line{};
};

void SubjectOrderPreviousBoundaryArticleTest::SetUp()
{
    SubjectOrderPreviousArticleTest::SetUp();
    set_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS},
                 5);
    g_selected_only = true;
    g_verbose = std::get<1>(GetParam());
    g_search_ahead = ArticleNum{-1};
    m_line = g_term_line;
}

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

class ParentRootArticleTest : public ArticleNavigationTest
{
};

class ChildLeafArticleTest : public ArticleNavigationTest
{
};

class ReadStateArticleTest : public ArticleNavigationTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    int m_old_check_count{g_check_count};
    int m_old_dm_count{g_dm_count};
};

void ReadStateArticleTest::SetUp()
{
    ArticleNavigationTest::SetUp();
    g_check_count = 10;
    g_dm_count = 0;
}

void ReadStateArticleTest::TearDown()
{
    g_check_count = m_old_check_count;
    g_dm_count = m_old_dm_count;
    ArticleNavigationTest::TearDown();
}

class SiblingArticleTest : public ArticleNavigationTest
{
protected:
    void set_siblings(long current);
};

void SiblingArticleTest::set_siblings(long current)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL},
                 current);
    g_threaded_group = true;
    struct ArticleLinks
    {
        long parent;
        long child;
        long sibling;
    };
    // Children: 1:{2,6,7}, 2:{3,5}, 3:{4}, 7:{8,10}, 8:{9}.
    const ArticleLinks links[]{{0, 2, 0}, {1, 3, 6}, {2, 4, 5},  {3, 0, 0}, {2, 0, 0},
                               {1, 0, 7}, {1, 8, 0}, {7, 9, 10}, {8, 0, 0}, {7, 0, 0}};
    long               number = 1;
    for (const ArticleLinks &link : links)
    {
        Article *article = article_ptr(ArticleNum{number++});
        article->m_parent = link.parent ? article_ptr(ArticleNum{link.parent}) : nullptr;
        article->m_child1 = link.child ? article_ptr(ArticleNum{link.child}) : nullptr;
        article->m_sibling = link.sibling ? article_ptr(ArticleNum{link.sibling}) : nullptr;
    }
}

class SubjectThreadArticleTest : public ArticleNavigationTest
{
protected:
    void set_subjects(std::initializer_list<ArticleFlags> flags, long current);

    Subject m_subjects[6]{};
};

void SubjectThreadArticleTest::set_subjects(std::initializer_list<ArticleFlags> flags, long current)
{
    ASSERT_NE(0U, flags.size());
    ASSERT_EQ(0U, flags.size() % 4);
    ASSERT_LE(flags.size(), 12U);
    set_articles(flags, current);
    const long subject_count = static_cast<long>(flags.size() / 2);
    g_sel_mode = GetParam() ? SM_THREAD : SM_SUBJECT;
    g_first_subject = &m_subjects[0];
    g_last_subject = &m_subjects[subject_count - 1];
    for (long index = 0; index < subject_count; ++index)
    {
        Subject &subject = m_subjects[index];
        subject.m_prev = index ? &m_subjects[index - 1] : nullptr;
        subject.m_next = index + 1 < subject_count ? &m_subjects[index + 1] : nullptr;
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

class SelectedPreviousSubjectThreadArticleTest : public SubjectThreadArticleTest
{
protected:
    void TearDown() override;
    void set_selected_subjects(std::initializer_list<ArticleFlags> flags, long current);
    void expect_previous(long number, std::initializer_list<long> missing = {}, bool force_last = false);
    void expect_navigation(char command, long number, bool reread, std::initializer_list<long> missing = {},
                           bool force_last = false);

private:
    int  m_old_selected_subjects{g_selected_subj_cnt};
    bool m_old_verify{g_verify};
};

void SelectedPreviousSubjectThreadArticleTest::TearDown()
{
    g_selected_subj_cnt = m_old_selected_subjects;
    g_verify = m_old_verify;
    SubjectThreadArticleTest::TearDown();
}

void SelectedPreviousSubjectThreadArticleTest::set_selected_subjects(std::initializer_list<ArticleFlags> flags,
                                                                     long                                current)
{
    set_subjects(flags, current);
    g_selected_only = true;
    g_verify = false;
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{2};
    g_search_ahead = ArticleNum{3};
    for (long index = 0; index < static_cast<long>(flags.size() / 2); ++index)
    {
        Subject &subject = m_subjects[index];
        subject.m_flags = SF_NONE;
        for (Article *ap = subject.m_articles; ap; ap = ap->m_subj_next)
        {
            if ((ap->m_flags & (AF_UNREAD | AF_SEL)) == (AF_UNREAD | AF_SEL))
            {
                subject.m_flags = SF_SEL | SF_VISIT;
                ++g_selected_count;
            }
        }
        g_selected_subj_cnt += (subject.m_flags & SF_SEL) != 0;
    }
}

void SelectedPreviousSubjectThreadArticleTest::expect_previous(long number, std::initializer_list<long> missing,
                                                               bool force_last)
{
    expect_navigation('<', number, false, missing, force_last);
}

void SelectedPreviousSubjectThreadArticleTest::expect_navigation(char command, long number, bool reread,
                                                                 std::initializer_list<long> missing, bool force_last)
{
    const std::pair<ArticleNum, Article *>  current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>  recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                        first = g_first_art;
    const ArticleNum                        parsed = g_parsed_art;
    const ArticleNum                        search_ahead = g_search_ahead;
    const int                               selected_subjects = g_selected_subj_cnt;
    const std::pair<GeneralMode, MinorMode> mode = std::make_pair(g_general_mode, g_mode);
    Subject *const                          first_subject = g_first_subject;
    Subject *const                          last_subject = g_last_subject;
    const std::pair<int, int>               terminal = std::make_pair(g_term_line, g_term_col);
    const std::pair<bool, bool>             follow = std::make_pair(g_s_follow_temp, g_univ_follow_temp);
    const bool                breadth_first = g_breadth_first;
    const bool                selected_only = g_selected_only;
    const bool                threaded_group = g_threaded_group;
    ArticleUnread                           unread = m_newsgroup.m_to_read;
    ArticleUnread                           selected = g_selected_count;
    std::vector<ArticleFlags> flags;
    for (long article = 1; article <= g_last_art.value_of(); ++article)
    {
        flags.push_back(article_ptr(ArticleNum{article})->m_flags);
    }
    std::vector<SubjectFlags> subject_flags;
    for (const Subject &subject : m_subjects)
    {
        subject_flags.push_back(subject.m_flags);
    }
    for (const long article : missing)
    {
        ArticleFlags &expected = flags[article - 1];
        ASSERT_FALSE(expected & AF_EXISTS);
        if (expected & AF_UNREAD)
        {
            --unread;
            selected -= (expected & AF_SEL) != 0;
            expected &= ~(AF_UNREAD | AF_SEL);
        }
    }
    g_reread = false;
    g_force_last = false;
    testing::internal::CaptureStdout();
    const ArticleSwitchResult result = art_switch(std::string{command});
    const std::string         output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(AS_NORM, result);
    EXPECT_TRUE(output.empty());
    EXPECT_EQ(ArticleNum{number}, g_art);
    EXPECT_EQ(number <= g_last_art.value_of() ? article_ptr(ArticleNum{number}) : nullptr, g_artp);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(search_ahead, g_search_ahead);
    EXPECT_EQ(mode, std::make_pair(g_general_mode, g_mode));
    EXPECT_EQ(terminal, std::make_pair(g_term_line, g_term_col));
    EXPECT_EQ(follow, std::make_pair(g_s_follow_temp, g_univ_follow_temp));
    EXPECT_EQ(first_subject, g_first_subject);
    EXPECT_EQ(last_subject, g_last_subject);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(reread, g_reread);
    EXPECT_EQ(force_last, g_force_last);
    EXPECT_EQ(threaded_group, g_threaded_group);
    EXPECT_EQ(GetParam() ? SM_THREAD : SM_SUBJECT, g_sel_mode);
    EXPECT_EQ(breadth_first, g_breadth_first);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    for (long article = 1; article <= g_last_art.value_of(); ++article)
    {
        SCOPED_TRACE(article);
        EXPECT_EQ(flags[article - 1], article_ptr(ArticleNum{article})->m_flags);
    }
    for (long index = 0; index < 6; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(subject_flags[index], m_subjects[index].m_flags);
    }
}

class SubjectFallbackArticleTest : public SelectedPreviousSubjectThreadArticleTest
{
protected:
    void set_fallback_subjects(std::initializer_list<ArticleFlags> flags, bool selected_only = true);
    void expect_from(char command, long current, long target, bool reread, std::initializer_list<long> missing = {},
                     bool force_last = false);
};

void SubjectFallbackArticleTest::set_fallback_subjects(std::initializer_list<ArticleFlags> flags, bool selected_only)
{
    set_selected_subjects(flags, 1);
    g_selected_only = selected_only;
    g_selected_subj_cnt = 0;
    for (Subject &subject : m_subjects)
    {
        // Visit flags and selections can remain after a scope is read.
        subject.m_flags = SF_VISIT;
        for (Article *ap = subject.m_articles; ap; ap = ap->m_subj_next)
        {
            if (ap->m_flags & AF_SEL)
            {
                subject.m_flags |= SF_SEL;
            }
        }
        g_selected_subj_cnt += (subject.m_flags & SF_SEL) != 0;
    }
}

void SubjectFallbackArticleTest::expect_from(char command, long current, long target, bool reread,
                                             std::initializer_list<long> missing, bool force_last)
{
    g_art = ArticleNum{current};
    g_artp = article_ptr(g_art);
    g_curr_art = g_art;
    g_curr_artp = g_artp;
    expect_navigation(command, target, reread, missing, force_last);
}

class RereadArticleTest : public UnreadScopeArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_reading_position(long current);
    std::string display(char command);
    std::string display(std::string command, std::string_view input = {}, ArticleSwitchResult expected = AS_NORM);
    void        expect_unmoved_view();

private:
    bool             m_old_do_hiding{g_do_hiding};
    bool             m_old_rotate{g_rotate};
    bool             m_old_do_fseek{g_do_fseek};
    ArticleLine      m_old_art_line_num{g_art_line_num};
    ArticleLine      m_old_top_line{g_top_line};
    int              m_old_fire_is_out{g_fire_is_out};
    int              m_old_tc_lines{g_tc_LINES};
    std::string_view m_old_tc_cd{g_tc_CD};
    std::string_view m_old_tc_ho{g_tc_HO};
    std::string_view m_old_tc_cr{g_tc_CR};
};

void RereadArticleTest::SetUp()
{
    UnreadScopeArticleTest::SetUp();
    g_do_hiding = true;
    g_rotate = false;
    g_do_fseek = false;
    g_art_line_num = ArticleLine{41};
    g_top_line = ArticleLine{15};
    g_term_line = 12;
    g_term_col = 9;
    g_fire_is_out = STANDOUT | UNDERLINE;
    g_tc_LINES = 24;
    g_tc_CD = "\033[J";
    g_tc_HO = "\033[H";
    g_tc_CR = "\r";
}

void RereadArticleTest::TearDown()
{
    g_do_hiding = m_old_do_hiding;
    g_rotate = m_old_rotate;
    g_do_fseek = m_old_do_fseek;
    g_art_line_num = m_old_art_line_num;
    g_top_line = m_old_top_line;
    g_fire_is_out = m_old_fire_is_out;
    g_tc_LINES = m_old_tc_lines;
    g_tc_CD = m_old_tc_cd;
    g_tc_HO = m_old_tc_ho;
    g_tc_CR = m_old_tc_cr;
    UnreadScopeArticleTest::TearDown();
}

void RereadArticleTest::set_reading_position(long current)
{
    set_scope({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS | AF_UNREAD,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL},
              current);
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_SEL;
    }
    g_selected_subj_cnt = 3;
    g_selected_only = true;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{1};
    g_search_ahead = ArticleNum{6};
    g_general_mode = GM_READ;
    g_mode = current <= g_last_art.value_of() ? MM_ARTICLE : MM_ARTICLE_END;
}

std::string RereadArticleTest::display(char command)
{
    return display(std::string{command});
}

std::string RereadArticleTest::display(std::string command, std::string_view input, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                       first = g_first_art;
    const ArticleNum                       parsed = g_parsed_art;
    const ArticleNum                       search_ahead = g_search_ahead;
    const MinorMode                        mode = g_mode;
    std::vector<SubjectFlags>              subject_flags;
    for (const Subject &subject : m_subjects)
    {
        subject_flags.push_back(subject.m_flags);
    }
    push_char('q');
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::move(command));

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(search_ahead, g_search_ahead);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(mode, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL});
    for (std::size_t index = 0; index < subject_flags.size(); ++index)
    {
        EXPECT_EQ(subject_flags[index], m_subjects[index].m_flags);
    }
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

void RereadArticleTest::expect_unmoved_view()
{
    EXPECT_FALSE(g_do_fseek);
    EXPECT_EQ(ArticleLine{41}, g_art_line_num);
    EXPECT_EQ(ArticleLine{15}, g_top_line);
    EXPECT_EQ(12, g_term_line);
    EXPECT_EQ(9, g_term_col);
    EXPECT_EQ(STANDOUT | UNDERLINE, g_fire_is_out);
}

class InlineViewingArticleTest : public RereadArticleTest, public testing::WithParamInterface<long>
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string toggle_viewing();

private:
    bool            m_old_auto_view_inline{g_auto_view_inline};
    ArticleLine     m_old_first_view{g_first_view};
    ArticlePosition m_old_art_pos{g_art_pos};
};

void InlineViewingArticleTest::SetUp()
{
    RereadArticleTest::SetUp();
    set_reading_position(GetParam());
    g_art_pos = ArticlePosition{123};
    g_first_view = ArticleLine{37};
}

void InlineViewingArticleTest::TearDown()
{
    g_auto_view_inline = m_old_auto_view_inline;
    g_first_view = m_old_first_view;
    g_art_pos = m_old_art_pos;
    RereadArticleTest::TearDown();
}

std::string InlineViewingArticleTest::toggle_viewing()
{
    const std::string output = display("i", {}, AS_ASK);

    EXPECT_EQ(ArticlePosition{123}, g_art_pos);
    EXPECT_EQ(ArticleLine{41}, g_art_line_num);
    EXPECT_EQ(ArticleLine{15}, g_top_line);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_EQ(STANDOUT | UNDERLINE, g_fire_is_out);
    EXPECT_EQ(14, g_term_line);
    EXPECT_EQ(0, g_term_col);
    return output;
}

class RereadCurrentArticleTest : public RereadArticleTest, public testing::WithParamInterface<long>
{
};

class RotateArticleTest : public RereadArticleTest, public testing::WithParamInterface<char>
{
};

class ToggleRotationArticleTest : public RereadArticleTest, public testing::WithParamInterface<bool>
{
};

class CachedDisplayArticleTest : public RereadArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void                         set_article_text(std::string_view text);
    void cache_article(std::string_view expected = "\xC3\x84\n");
    void expect_redraw(ArticleLine line);
    void expect_cleared_cache();
    const std::filesystem::path &output_dir() const
    {
        return m_output_dir;
    }

private:
    DataSource            m_data_source{};
    MimeSection           m_section{};
    std::filesystem::path m_output_dir;
    std::FILE            *m_old_art_fp{g_art_fp};
    DataSource           *m_old_data_source{g_data_source};
    MimeSection          *m_old_mime_section{g_mime_section};
    MimeState             m_old_mime_state{g_mime_state};
    bool                  m_old_is_mime{g_is_mime};
    ArticlePosition       m_old_art_pos{g_art_pos};
    ArticlePosition       m_old_raw_art_size{g_raw_art_size};
    ArticlePosition       m_old_art_size{g_art_size};
    HeaderType            m_old_past_header{g_header_type[PAST_HEADER]};
    int                   m_old_tc_cols{g_tc_COLS};
    int                   m_old_word_wrap_offset{g_word_wrap_offset};
    char                  m_old_subst{current_char_subst_mode()};
    std::string           m_old_charsets{g_charsets};
};

void CachedDisplayArticleTest::SetUp()
{
    RereadArticleTest::SetUp();
    g_art_fp = nullptr;
    art_io_init();
    g_data_source = &m_data_source;
    m_section.mime_parse_type("text/plain; charset=Latin1");
    g_mime_section = &m_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    g_header_type[PAST_HEADER].min_pos = ArticlePosition{};
    g_tc_COLS = 80;
    g_word_wrap_offset = 8;
    g_charsets = "pa";
    set_char_subst_mode('p');
    const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
    m_output_dir =
        std::filesystem::path{TRN_TEST_TMP_DIR} / "CachedDisplayArticleTest" / info->test_suite_name() / info->name();
    std::error_code error;
    std::filesystem::create_directories(m_output_dir, error);
    ASSERT_FALSE(error) << error.message();
    set_article_text("\xC4\n");
}

void CachedDisplayArticleTest::set_article_text(std::string_view text)
{
    if (g_art_fp)
    {
        std::fclose(g_art_fp);
        g_art_fp = nullptr;
    }
    const std::filesystem::path article_file = m_output_dir / "article";
    {
        std::ofstream output{article_file, std::ios::binary};
        output << text;
        ASSERT_TRUE(output.good());
    }
    g_art_fp = std::fopen(article_file.string().c_str(), "rb");
    ASSERT_NE(nullptr, g_art_fp);
    g_raw_art_size = ArticlePosition{static_cast<long>(text.size())};
    g_art_size = g_raw_art_size;
    g_art_pos = ArticlePosition{};
    clear_art_buf();
}

void CachedDisplayArticleTest::TearDown()
{
    if (g_art_fp)
    {
        std::fclose(g_art_fp);
    }
    art_io_final();
    g_art_fp = m_old_art_fp;
    g_data_source = m_old_data_source;
    g_mime_section = m_old_mime_section;
    g_mime_state = m_old_mime_state;
    g_is_mime = m_old_is_mime;
    g_art_pos = m_old_art_pos;
    g_raw_art_size = m_old_raw_art_size;
    g_art_size = m_old_art_size;
    g_header_type[PAST_HEADER] = m_old_past_header;
    g_tc_COLS = m_old_tc_cols;
    g_word_wrap_offset = m_old_word_wrap_offset;
    g_charsets = m_old_charsets;
    set_char_subst_mode(m_old_subst);
    std::error_code error;
    std::filesystem::remove(m_output_dir / "article", error);
    std::filesystem::remove(m_output_dir, error);
    RereadArticleTest::TearDown();
}

void CachedDisplayArticleTest::cache_article(std::string_view expected)
{
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ(expected, line);
    ASSERT_FALSE(art_buf_empty());
    ASSERT_EQ(expected, art_buf_view(ArticlePosition{}));
}

void CachedDisplayArticleTest::expect_redraw(ArticleLine line)
{
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_do_fseek);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(line, g_art_line_num);
    EXPECT_EQ(0, g_term_line);
    EXPECT_EQ(0, g_term_col);
    EXPECT_EQ(0, g_fire_is_out);
}

void CachedDisplayArticleTest::expect_cleared_cache()
{
    EXPECT_TRUE(art_buf_empty());
    EXPECT_EQ(ArticlePosition{}, art_buf_len());
    EXPECT_EQ(ArticlePosition{}, art_buf_pos());
    EXPECT_EQ(ArticlePosition{}, art_buf_seek());
}

class RefreshArticleTest : public CachedDisplayArticleTest, public testing::WithParamInterface<std::pair<char, long>>
{
};

class RefreshBoundaryArticleTest : public CachedDisplayArticleTest, public testing::WithParamInterface<char>
{
};

class SubstitutionArticleTest : public CachedDisplayArticleTest, public testing::WithParamInterface<long>
{
};

class BackwardPagingArticleTest : public CachedDisplayArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void load_article(long line_count = 40, long page_break_after = -1);
    void expect_target(long line, std::string_view expected);

private:
    std::string           m_old_tmp_dir{g_tmp_dir};
    std::filesystem::path m_vary_file;
    MarkingMode           m_old_marking{g_marking};
    MarkingAreas          m_old_marking_areas{g_marking_areas};
    ArticleLine           m_old_highlight{g_highlight};
};

void BackwardPagingArticleTest::SetUp()
{
    CachedDisplayArticleTest::SetUp();
    g_tmp_dir = output_dir().generic_string();
    m_vary_file = file_exp(VARYNAME);
    back_page_init();
    g_tc_LINES = 10;
    g_marking = NO_MARKING;
    g_marking_areas = BACK_PAGE_MARKING;
    g_highlight = ArticleLine{7};
}

void BackwardPagingArticleTest::TearDown()
{
    back_page_final();
    std::error_code error;
    std::filesystem::remove(m_vary_file, error);
    g_tmp_dir = m_old_tmp_dir;
    g_marking = m_old_marking;
    g_marking_areas = m_old_marking_areas;
    g_highlight = m_old_highlight;
    CachedDisplayArticleTest::TearDown();
}

void BackwardPagingArticleTest::load_article(long line_count, long page_break_after)
{
    std::string text;
    for (long line = 0; line < line_count; ++line)
    {
        text += "Line " + std::to_string(line);
        if (line == page_break_after)
        {
            text += '\f';
        }
        text += '\n';
    }
    set_article_text(text);
    for (long line = 0; line < line_count; ++line)
    {
        const ArticlePosition position = art_buf_pos();
        std::string           content;
        ASSERT_TRUE(read_art_buf(content, false));
        ASSERT_EQ("Line " + std::to_string(line) + (line == page_break_after ? "\f\n" : "\n"), content);
        virtual_write(ArticleLine{line}, line == page_break_after ? -position : position);
    }
    virtual_write(ArticleLine{line_count}, art_buf_pos());
}

void BackwardPagingArticleTest::expect_target(long line, std::string_view expected)
{
    EXPECT_EQ(ArticleLine{line}, g_art_line_num);
    const ArticlePosition saved_position = virtual_read(g_art_line_num);
    const ArticlePosition position = saved_position < 0 ? -saved_position : saved_position;
    ASSERT_EQ(0, seek_art(art_buf_seek()));
    ASSERT_EQ(0, seek_art_buf(position));
    std::string content;
    ASSERT_TRUE(read_art_buf(content, false));
    EXPECT_EQ(expected, content);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_EQ('p', current_char_subst_mode());
}

class BackwardPageArticleTest : public BackwardPagingArticleTest,
                                public testing::WithParamInterface<std::tuple<char, int, long>>
{
};

class BackwardPageBoundaryArticleTest : public BackwardPagingArticleTest, public testing::WithParamInterface<char>
{
};

class BackwardCommandArticleTest : public BackwardPagingArticleTest, public testing::WithParamInterface<char>
{
};

class BackwardPageMarkingArticleTest : public BackwardPagingArticleTest,
                                       public testing::WithParamInterface<std::tuple<MarkingMode, MarkingAreas, long>>
{
};

class JumpToEndArticleTest : public CachedDisplayArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void read_prefix(std::string_view expected);
    void expect_end(ArticlePosition end, ArticleLine line = ArticleLine{41}, ArticleLine light = ArticleLine{40},
                    int column = 9);

private:
    ArticlePosition m_old_inner_search{g_inner_search};
    ArticleLine     m_old_inner_light{g_inner_light};
    int             m_old_g_line{g_g_line};
    char            m_old_hide_everything{g_hide_everything};
};

void JumpToEndArticleTest::SetUp()
{
    CachedDisplayArticleTest::SetUp();
    g_inner_search = ArticlePosition{29};
    g_inner_light = ArticleLine{13};
    g_g_line = 6;
    g_hide_everything = '\f';
}

void JumpToEndArticleTest::TearDown()
{
    g_inner_search = m_old_inner_search;
    g_inner_light = m_old_inner_light;
    g_g_line = m_old_g_line;
    g_hide_everything = m_old_hide_everything;
    CachedDisplayArticleTest::TearDown();
}

void JumpToEndArticleTest::read_prefix(std::string_view expected)
{
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ(expected, line);
    g_art_pos = art_buf_article_pos();
}

void JumpToEndArticleTest::expect_end(ArticlePosition end, ArticleLine line, ArticleLine light, int column)
{
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_do_fseek);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(line, g_art_line_num);
    EXPECT_EQ(line, g_top_line);
    EXPECT_EQ(light, g_inner_light);
    EXPECT_EQ(end, g_inner_search);
    EXPECT_EQ(0, g_g_line);
    EXPECT_EQ('b', g_hide_everything);
    EXPECT_EQ(12, g_term_line);
    EXPECT_EQ(column, g_term_col);
    EXPECT_EQ(STANDOUT | UNDERLINE, g_fire_is_out);
    EXPECT_FALSE(g_rotate);
    EXPECT_EQ('p', current_char_subst_mode());
}

class KnownSizeEndArticleTest : public JumpToEndArticleTest, public testing::WithParamInterface<std::pair<long, bool>>
{
};

class DownloadEndArticleTest : public JumpToEndArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void start_download();

private:
    DataSource                                               m_remote_source{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
    NNTPLink                                                 m_old_nntp_link{g_nntp_link};
    std::string                                              m_old_head_buf{g_head_buf};
    std::string                                              m_old_ser_line{g_ser_line};
    std::string                                              m_old_last_command{g_last_command};
    std::filesystem::path                                    m_old_current_path{std::filesystem::current_path()};
    std::filesystem::path                                    m_download_file;
    std::string_view                                         m_old_tc_ce{g_tc_CE};
};

void DownloadEndArticleTest::SetUp()
{
    JumpToEndArticleTest::SetUp();
    m_remote_source.m_flags = DF_REMOTE;
    g_data_source = &m_remote_source;
    m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    g_nntp_link = {};
    g_nntp_link.connection = m_connection;
    g_nntp_link.flags = NNTP_NEW_CMD_OK;
    g_tc_CE = "\033[K";
    nntp_gets_clear_buffer();
    (void) nntp_art_name(ArticleNum{}, false);
    std::error_code error;
    std::filesystem::current_path(output_dir(), error);
    ASSERT_FALSE(error) << error.message();
}

void DownloadEndArticleTest::TearDown()
{
    nntp_finish_body(FB_DISCARD);
    if (g_art_fp)
    {
        std::fclose(g_art_fp);
        g_art_fp = nullptr;
    }
    (void) nntp_art_name(ArticleNum{}, false);
    nntp_gets_clear_buffer();
    g_nntp_link = std::move(m_old_nntp_link);
    g_head_buf = m_old_head_buf;
    g_ser_line = m_old_ser_line;
    g_last_command = m_old_last_command;
    g_tc_CE = m_old_tc_ce;
    std::error_code error;
    std::filesystem::current_path(m_old_current_path, error);
    std::filesystem::remove(m_download_file, error);
    JumpToEndArticleTest::TearDown();
}

void DownloadEndArticleTest::start_download()
{
    set_reading_position(4);
    g_do_hiding = GetParam();
    std::fclose(g_art_fp);
    g_art_fp = nullptr;
    g_parsed_art = g_art;
    g_head_buf = "Subject: end\n\n";
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("BODY 4"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("222 4 <end@example.test> body follows"))
        .WillOnce(testing::Return("\xC4"));
    nntp_body(g_art);
    ASSERT_NE(nullptr, g_art_fp);
    m_download_file = output_dir() / nntp_art_name(g_art, false);
    ASSERT_EQ(0, seek_art(g_header_type[PAST_HEADER].min_pos));
    set_art_buf_seek(g_header_type[PAST_HEADER].min_pos);
    g_raw_art_size = g_art_size = ArticlePosition{-1};
    read_prefix(g_do_hiding ? "\xC3\x84\n" : "\xC4\n");
    ASSERT_EQ(ArticlePosition{-1}, nntp_art_size());
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(m_connection.get()));
    EXPECT_CALL(*m_connection, read_line(testing::_)).WillOnce(testing::Return("Last")).WillOnce(testing::Return("."));
}

class SubjectListingArticleTest : public RereadArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_listing_articles(long current = 4);
    std::string list_subjects(std::string_view input = {});

    trn::testing::MockEnvironment m_env;

private:
    DataSource                 m_data_source{};
    DataSource                *m_old_data_source{g_data_source};
    HeaderType                 m_old_subject_header{g_header_type[SUBJ_LINE]};
    std::optional<std::string> m_old_subj_line{g_subj_line};
    char                       m_old_int_count{g_int_count};
    int                        m_old_page_line{g_page_line};
    int                        m_old_tc_cols{g_tc_COLS};
    int                        m_old_term_scrolled{g_term_scrolled};
    bool                       m_old_erase_screen{g_erase_screen};
    bool                       m_old_in_ng{g_in_ng};
    std::string_view           m_old_tc_ce{g_tc_CE};
    std::string_view           m_old_tc_so{g_tc_SO};
    std::string_view           m_old_tc_se{g_tc_SE};
};

void SubjectListingArticleTest::SetUp()
{
    RereadArticleTest::SetUp();
    g_data_source = &m_data_source;
    g_header_type[SUBJ_LINE].flags |= HT_CACHED;
    g_subj_line = std::nullopt;
    g_int_count = 0;
    g_page_line = 9;
    g_tc_COLS = 80;
    g_term_scrolled = 0;
    g_erase_screen = false;
    g_in_ng = true;
    g_tc_CE = "\033[K";
    g_tc_SO = "";
    g_tc_SE = "";
    m_env.expect_no_envar("SUBJLINE");
}

void SubjectListingArticleTest::TearDown()
{
    g_data_source = m_old_data_source;
    g_header_type[SUBJ_LINE] = m_old_subject_header;
    g_subj_line = m_old_subj_line;
    g_int_count = m_old_int_count;
    g_page_line = m_old_page_line;
    g_tc_COLS = m_old_tc_cols;
    g_term_scrolled = m_old_term_scrolled;
    g_erase_screen = m_old_erase_screen;
    g_in_ng = m_old_in_ng;
    g_tc_CE = m_old_tc_ce;
    g_tc_SO = m_old_tc_so;
    g_tc_SE = m_old_tc_se;
    RereadArticleTest::TearDown();
}

void SubjectListingArticleTest::set_listing_articles(long current)
{
    set_reading_position(current);
    g_parsed_art = ArticleNum{};
    m_subjects[0].m_str = "Re: First topic";
    m_subjects[1].m_str = "Re: Middle topic";
    m_subjects[2].m_str = "Re: Last topic";
}

std::string SubjectListingArticleTest::list_subjects(std::string_view input)
{
    const std::pair<ArticleNum, Article *>           article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *>           current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *>           recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleNum                                 first = g_first_art;
    const ArticleNum                                 parsed = g_parsed_art;
    const ArticleNum                                 search_ahead = g_search_ahead;
    const ArticlePosition                            position = g_art_pos;
    const MinorMode                                  mode = g_mode;
    const ArticleUnread                              unread = m_newsgroup.m_to_read;
    const ArticleUnread                              selected = g_selected_count;
    const int                                        selected_subjects = g_selected_subj_cnt;
    const bool                                       selected_only = g_selected_only;
    const bool                                       reread = g_reread;
    const bool                                       force_last = g_force_last;
    std::vector<std::pair<ArticleNum, ArticleFlags>> flags;
    for (const auto &[number, entry] : g_article_list)
    {
        flags.emplace_back(number, entry.m_flags);
    }
    std::vector<SubjectFlags> subject_flags;
    for (const Subject &subject : m_subjects)
    {
        subject_flags.push_back(subject.m_flags);
    }
    push_char('q');
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("=");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(parsed, g_parsed_art);
    EXPECT_EQ(search_ahead, g_search_ahead);
    EXPECT_EQ(position, g_art_pos);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(mode, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(unread, m_newsgroup.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(reread, g_reread);
    EXPECT_EQ(force_last, g_force_last);
    EXPECT_EQ(ArticleLine{41}, g_art_line_num);
    EXPECT_EQ(ArticleLine{15}, g_top_line);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_EQ(0, g_int_count);
    EXPECT_FALSE(g_subj_line.has_value());
    EXPECT_EQ(flags.size(), g_article_list.size());
    for (const auto &[number, expected] : flags)
    {
        SCOPED_TRACE(number.value_of());
        EXPECT_EQ(expected, article_find(number)->m_flags);
    }
    for (std::size_t index = 0; index < subject_flags.size(); ++index)
    {
        EXPECT_EQ(subject_flags[index], m_subjects[index].m_flags);
    }
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

class SubjectListingPositionArticleTest : public SubjectListingArticleTest, public testing::WithParamInterface<long>
{
};

} // namespace

TEST_F(SelectorEntryArticleTest, retainsTheChosenArticleAndSelectionWhenReturningToReading)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    g_recent_art = ArticleNum{2};
    g_recent_artp = article_ptr(g_recent_art);
    g_reread = true;
    g_force_last = true;
    push_char('n');

    const std::string output = run_selector(
        [&](char_int)
        {
            EXPECT_FALSE(g_selected_only);
            article_ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
            g_art = ArticleNum{3};
            g_artp = article_ptr(g_art);
            return 'Z';
        },
        AS_NORM);

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_artp);
    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_F(SelectorEntryArticleTest, returnsToReadingWhenUnreadArticlesRemainWithoutSelections)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 2);

    const std::string output = run_selector(
        [&](char_int)
        {
            g_art = ArticleNum{1};
            g_artp = article_ptr(g_art);
            return 'Z';
        },
        AS_NORM);

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD});
}

TEST_F(SelectorEntryArticleTest, cancellationPreservesSelectionAndAllowsRepeatedEntryAtTheArticlePrompt)
{
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    g_first_subject->m_flags |= SF_SEL;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        g_sa_in = true;
        g_s_follow_temp = false;
        g_univ_follow_temp = false;

        const std::string output = run_selector([](char_int) { return '+'; }, AS_ASK);

        EXPECT_NE(std::string::npos, output.find('\n'));
        EXPECT_EQ(std::string::npos, output.find("Already inside article selector"));
        EXPECT_EQ(g_tc_LINES, g_term_scrolled);
        EXPECT_EQ(g_tc_LINES - 1, g_term_line);
        EXPECT_EQ(0, g_term_col);
        EXPECT_EQ(ArticleNum{1}, g_art);
        EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
        EXPECT_TRUE(g_selected_only);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
        EXPECT_EQ(1, g_selected_subj_cnt);
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD});
    }
}

TEST_F(SelectorEntryArticleTest, emptyGroupRequestsCleanupAndAllowsRepeatedSelectorEntry)
{
    set_articles({}, 1);
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_TRUE(run_selector([](char_int) { return 'Z'; }, AS_CLEAN).empty());

        EXPECT_EQ(ArticleNum{1}, g_art);
        EXPECT_EQ(nullptr, g_artp);
        EXPECT_TRUE(g_article_list.empty());
        EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_FALSE(g_selected_only);
    }
}

TEST_F(SelectorEntryArticleTest, exhaustedGroupPreservesTheEndPositionWhenRequestingCleanup)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);

    EXPECT_TRUE(run_selector([](char_int) { return 'Z'; }, AS_CLEAN).empty());

    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_FALSE(g_selected_only);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS});
}

TEST_F(SelectorEntryArticleTest, selectorReadingTheLastUnreadArticleRequestsCleanup)
{
    set_articles({AF_EXISTS | AF_UNREAD}, 1);

    const std::string output = run_selector(
        [&](char_int)
        {
            article_ptr(ArticleNum{1})->mark_as_read();
            g_art = article_after(g_last_art);
            g_artp = nullptr;
            return 'Z';
        },
        AS_CLEAN);

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_FALSE(g_selected_only);
    expect_flags({AF_EXISTS});
}

TEST_F(SelectorEntryArticleTest, cancellationKeepsAnEmptyGroupAtTheArticlePrompt)
{
    set_articles({}, 1);

    const std::string output = run_selector([](char_int) { return '+'; }, AS_ASK);

    EXPECT_NE(std::string::npos, output.find('\n'));
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(g_tc_LINES, g_term_scrolled);
    EXPECT_EQ(g_tc_LINES - 1, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_F(SelectorEntryArticleTest, recursiveEntryIsRejectedUntilTheActiveSelectorReturns)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 1);
    int               recursive_calls = 0;
    const std::string output = run_selector(
        [&](char_int)
        {
            const ArticleSwitchResult nested = art_switch("+", {},
                                                          [&](char_int)
                                                          {
                                                              ++recursive_calls;
                                                              return 'Z';
                                                          });
            EXPECT_EQ(AS_ASK, nested);
            EXPECT_EQ(ArticleNum{1}, g_art);
            EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
            return 'Z';
        },
        AS_NORM);

    EXPECT_EQ(0, recursive_calls);
    EXPECT_NE(std::string::npos, output.find("Already inside article selector!"));
    EXPECT_EQ(5, g_term_line);
    EXPECT_EQ(0, g_term_col);
    EXPECT_TRUE(run_selector([](char_int) { return 'Z'; }, AS_NORM).empty());
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_FALSE(g_selected_only);
    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD});
}

TEST_P(SelectorGroupExitArticleTest, leavesForTheRequestedGroupDestinationWithoutChangingReadingContext)
{
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    g_first_subject->m_flags |= SF_SEL;
    g_recent_art = ArticleNum{2};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{2};
    g_search_ahead = ArticleNum{2};
    g_reread = true;
    g_force_last = true;
    push_char('n');

    const std::string output = run_selector([&](char_int) { return GetParam().first; }, AS_CLEAN, GetParam().second);

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(SF_SEL | SF_VISIT, g_first_subject->m_flags);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleNum{2}, g_parsed_art);
    EXPECT_EQ(ArticleNum{2}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_FALSE(g_sa_go);
    EXPECT_FALSE(g_sa_go_explicit);
    EXPECT_FALSE(g_sa_do_sel_threads);
    EXPECT_EQ(3, g_term_line);
    EXPECT_EQ(4, g_term_col);
    EXPECT_EQ(2, g_term_scrolled);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD});
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(SelectorCommand, SelectorGroupExitArticleTest,
                         testing::Values(std::make_pair('q', NG_NEXT), std::make_pair('Q', NG_ASK),
                                         std::make_pair('N', NG_SEL_NEXT), std::make_pair('P', NG_SEL_PRIOR)),
                         [](const testing::TestParamInfo<std::pair<char, DoNewsgroupResult>> &info)
                         { return std::string{info.param.first}; });

TEST_F(SelectorExitArticleTest, groupExitAtExhaustionRetainsTheEndPositionAndExistingScanRequests)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    g_sa_go_explicit = true;
    g_sa_do_sel_threads = true;

    EXPECT_TRUE(run_selector([](char_int) { return 'q'; }, AS_CLEAN, NG_NEXT).empty());

    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_sa_go);
    EXPECT_TRUE(g_sa_go_explicit);
    EXPECT_TRUE(g_sa_do_sel_threads);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS});
}

TEST_F(SelectorExitArticleTest, scanReturnRequestsExplicitSelectedThreadScanningForTheChosenArticle)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    push_char('n');

    const std::string output = run_selector(
        [&](char_int)
        {
            article_ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
            g_art = ArticleNum{3};
            g_artp = article_ptr(g_art);
            return ';';
        },
        AS_SA, NG_MINUS);

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_artp);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_sa_go);
    EXPECT_TRUE(g_sa_go_explicit);
    EXPECT_TRUE(g_sa_do_sel_threads);
    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL});
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_F(SelectorExitArticleTest, scanReturnAtExhaustionRetainsTheEndPositionAndGroupExitCode)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    g_sa_go_explicit = true;
    g_sa_do_sel_threads = true;

    EXPECT_TRUE(run_selector([](char_int) { return ';'; }, AS_SA, NG_MINUS).empty());

    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_sa_go);
    EXPECT_TRUE(g_sa_go_explicit);
    EXPECT_TRUE(g_sa_do_sel_threads);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS});
}

TEST_P(SelectorModeShortcutArticleTest, typedShortcutForwardsTheRequestedUnreadViewAndRetainsTheChosenArticle)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 2);
    g_first_art = ArticleNum{2};
    g_sel_mode = SM_THREAD;
    push_char('n');
    push_char(GetParam());

    const std::string output = run_selector(
        std::string{'_', FINISH_CMD}, GetParam(),
        [&](char_int)
        {
            // The selector interprets a/s/t/T; dispatch must preserve the request.
            EXPECT_FALSE(g_sel_rereading);
            EXPECT_EQ(AGF_SEL, g_sel_mask);
            EXPECT_EQ(SM_THREAD, g_sel_mode);
            EXPECT_EQ(ArticleNum{2}, g_first_art);
            expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD});
            g_art = ArticleNum{3};
            g_artp = article_ptr(g_art);
            g_artp->select_article(AUTO_KILL_NONE);
            return 'Z';
        },
        AS_NORM);

    EXPECT_NE(std::string::npos, output.find('_'));
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_artp);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL});
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThreadViews, SelectorModeShortcutArticleTest,
                         testing::Values('a', 's', 't', 'T'));

TEST_F(AlternateSelectorArticleTest, completedShortcutCancellationPreservesReadingContextAndQueuedInput)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD}, 2);
    g_first_art = ArticleNum{2};
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    g_first_subject->m_flags |= SF_SEL;
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{2};
    g_search_ahead = ArticleNum{3};
    g_reread = true;
    g_force_last = true;
    push_char('n');

    run_selector("_s", 's', [](char_int) { return '+'; }, AS_ASK);

    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(ArticleNum{2}, g_parsed_art);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD});
    EXPECT_EQ(g_tc_LINES, g_term_scrolled);
    EXPECT_EQ(g_tc_LINES - 1, g_term_line);
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_F(AlternateSelectorArticleTest, readSelectorCanReturnARevivedArticleFromAnExhaustedGroup)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    g_first_art = ArticleNum{4};
    push_char('n');
    push_char('+');

    const std::string output = run_selector(
        "U", 'U',
        [&](char_int)
        {
            // U requests already-read articles without unreading the group itself.
            EXPECT_EQ(ArticleNum{4}, g_art);
            EXPECT_EQ(nullptr, g_artp);
            EXPECT_EQ(ArticleNum{4}, g_first_art);
            EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
            expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS});
            g_art = ArticleNum{2};
            g_artp = article_ptr(g_art);
            g_artp->one_more();
            g_artp->select_article(AUTO_KILL_NONE);
            return 'Z';
        },
        AS_NORM);

    EXPECT_NE(std::string::npos, output.find("Unkill"));
    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_P(ReadSelectorPromptArticleTest, explicitOrDefaultSelectionCancellationPreservesReadAndUnreadArticles)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD}, 1);
    g_first_art = ArticleNum{2};
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    g_first_subject->m_flags |= SF_SEL;
    push_char('n');
    push_char(GetParam());

    const std::string output = run_selector("U", 'U', [](char_int) { return '+'; }, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("Unkill"));
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_FALSE(g_sel_rereading);
    EXPECT_EQ(AGF_SEL, g_sel_mask);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD});
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(PlusAndReturn, ReadSelectorPromptArticleTest, testing::Values('+', '\n'));

TEST_P(CancelSelectorPrefixArticleTest, erasingOrKillingThePrefixNeverEntersTheSelector)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD}, 2);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    g_first_subject->m_flags |= SF_SEL;
    push_char('n');
    push_char(GetParam());
    int calls = 0;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{'_', FINISH_CMD}, {},
                                                  [&](char_int)
                                                  {
                                                      ++calls;
                                                      return 'Z';
                                                  });

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_INP, result);
    EXPECT_EQ(0, calls);
    EXPECT_EQ(std::string::npos, output.find("Type h for help"));
    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_sa_in);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD});
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(EraseAndKill, CancelSelectorPrefixArticleTest, testing::Values('\b', Ctl('U')));

TEST_F(KillSubjectArticleTest, cachedKillIncludesSelectedUnreadArticlesAndPreservesOtherSubjectsOnRepeat)
{
    set_kill_articles(4);
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_TRUE(kill_subject(AS_NORM, 4).empty());

        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK, AF_EXISTS,
                      AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{2}, g_selected_count);
        EXPECT_FALSE(m_subjects[1].m_flags & (SF_VISIT | SF_WAS_SELECTED));
        EXPECT_TRUE(m_subjects[0].m_flags & SF_VISIT);
        EXPECT_TRUE(m_subjects[2].m_flags & SF_VISIT);
        EXPECT_FALSE(g_reread);
        EXPECT_EQ(ArticleNum{}, g_search_ahead);
    }
}

TEST_P(KillSubjectScanArticleTest, cachedKillReturnsToScanUnlessFollowingArticles)
{
    set_kill_articles(4);
    g_sa_in = true;
    g_sa_follow = GetParam().first;
    g_s_follow_temp = GetParam().second;

    EXPECT_TRUE(kill_subject(g_sa_follow || g_s_follow_temp ? AS_NORM : AS_SA, 4).empty());

    EXPECT_TRUE(g_sa_in);
    EXPECT_EQ(GetParam().first, g_sa_follow);
    EXPECT_EQ(GetParam().second, g_s_follow_temp);
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
}

INSTANTIATE_TEST_SUITE_P(FollowPolicy, KillSubjectScanArticleTest,
                         testing::Values(std::make_pair(false, false), std::make_pair(true, false),
                                         std::make_pair(false, true)));

TEST_P(KillSubjectSearchArticleTest, fallbackKillsMatchingSubjectsAndRestoresTheCurrentArticlePair)
{
    set_kill_articles(2);
    g_threaded_group = GetParam();
    g_last_cached = GetParam() ? ArticleNum{5} : g_last_art;
    std::vector<ArticleNum> visited;

    const std::string output = kill_subject(AS_ASK, 2, [&](ArticleNum number) { visited.push_back(number); });

    EXPECT_NE(std::string::npos, output.find("done"));
    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{2}, ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6},
                                       ArticleNum{7}}),
              visited);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
}

INSTANTIATE_TEST_SUITE_P(UnthreadedOrPartlyCached, KillSubjectSearchArticleTest, testing::Bool());

TEST_F(KillSubjectArticleTest, fallbackWithSubjectSearchAheadResumesAtTheFirstRemainingSelectedArticle)
{
    set_kill_articles(2);
    g_threaded_group = false;
    g_search_ahead = ArticleNum{-1};

    const std::string output = kill_subject(AS_NORM, 4);

    EXPECT_NE(std::string::npos, output.find("done"));
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

TEST_F(KillSubjectArticleTest, interruptingFallbackKeepsCompletedKillsAndRestoresTheCurrentArticlePair)
{
    set_kill_articles(2);
    g_last_cached = ArticleNum{5};

    const std::string output = kill_subject(AS_ASK, 2,
                                            [](ArticleNum number)
                                            {
                                                if (number == ArticleNum{6})
                                                {
                                                    g_int_count = 1;
                                                }
                                            });

    EXPECT_NE(std::string::npos, output.find("Intr at 6"));
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    EXPECT_EQ(0, g_int_count);
    EXPECT_TRUE(g_reread);
}

TEST_F(KillSubjectArticleTest, groupEndReportsTheMissingCurrentArticleWithoutChangingReadState)
{
    set_kill_articles(8);
    g_sa_in = true;

    const std::string output = kill_subject(AS_ASK, 8);

    EXPECT_NE(std::string::npos, output.find("End of group"));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_TRUE(g_sa_in);
    EXPECT_TRUE(m_subjects[1].m_flags & SF_VISIT);
    EXPECT_FALSE(g_reread);
}

TEST_F(KillSubthreadArticleTest, killsDescendantsAcrossSubjectsWithoutTouchingAncestorsOrSiblingBranchesOnRepeat)
{
    set_thread(2);
    g_breadth_first = true;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_TRUE(kill_articles(",").empty());

        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{3}, g_selected_count);
        EXPECT_TRUE(g_breadth_first);
    }
}

TEST_F(KillSubthreadArticleTest, leafKillStopsAtTheSiblingOfAnAncestor)
{
    set_thread(4);

    EXPECT_TRUE(kill_articles(",").empty());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
}

TEST_F(KillSubthreadArticleTest, readAndMissingNodesStillLeadToUnreadDescendants)
{
    set_thread(2);
    article_ptr(ArticleNum{2})->m_flags = AF_EXISTS;
    article_ptr(ArticleNum{3})->m_flags = AF_NONE;
    m_newsgroup.m_to_read = ArticleUnread{4};
    g_selected_count = ArticleUnread{4};

    EXPECT_TRUE(kill_articles(",").empty());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
}

TEST_F(KillSubthreadArticleTest, finalThreadEndsAtNullAndPreservesAlreadyReadDescendants)
{
    set_thread(6);

    EXPECT_TRUE(kill_articles(",").empty());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
}

TEST_F(KillSubthreadArticleTest, unthreadedFallbackKillsOnlyTheCurrentArticleOnRepeat)
{
    set_thread(2);
    g_threaded_group = false;
    g_sel_mode = SM_SUBJECT;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_TRUE(kill_articles(",").empty());

        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{4}, g_selected_count);
        EXPECT_FALSE(g_threaded_group);
    }
}

TEST_P(KillSubthreadScanArticleTest, scanContinuationHonorsPersistentAndTemporaryFollowSettings)
{
    set_thread(2);
    g_sa_in = true;
    g_sa_follow = GetParam().first;
    g_s_follow_temp = GetParam().second;

    EXPECT_TRUE(kill_articles(",", g_sa_follow || g_s_follow_temp ? AS_NORM : AS_SA).empty());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_TRUE(g_sa_in);
    EXPECT_EQ(GetParam().first, g_sa_follow);
    EXPECT_EQ(GetParam().second, g_s_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(FollowPolicy, KillSubthreadScanArticleTest,
                         testing::Values(std::make_pair(false, false), std::make_pair(true, false),
                                         std::make_pair(false, true)));

TEST_F(KillSubthreadArticleTest, groupEndRejectsTheCommandWithoutChangingAnyArticle)
{
    set_thread(8);
    g_sa_in = true;

    const std::string output = kill_articles(",", AS_ASK);

    EXPECT_NE(std::string::npos, output.find("End of group"));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_TRUE(g_sa_in);
}

TEST_F(KillThreadArticleTest, descendantEntryKillsAllSubjectsInItsThreadAndPreservesOtherThreadsOnRepeat)
{
    set_thread(4);
    for (Subject &subject : m_subjects)
    {
        subject.m_flags |= SF_WAS_SELECTED;
    }
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_TRUE(kill_articles("J").empty());

        expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
        EXPECT_FALSE(m_subjects[0].m_flags & (SF_VISIT | SF_WAS_SELECTED));
        EXPECT_FALSE(m_subjects[1].m_flags & (SF_VISIT | SF_WAS_SELECTED));
        EXPECT_EQ(SF_SEL | SF_VISIT | SF_WAS_SELECTED, m_subjects[2].m_flags);
    }
}

TEST_F(KillThreadArticleTest, readArticleInASingleSubjectThreadStillKillsItsEarlierUnreadArticle)
{
    set_thread(7);

    EXPECT_TRUE(kill_articles("J").empty());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_TRUE(m_subjects[0].m_flags & SF_VISIT);
    EXPECT_TRUE(m_subjects[1].m_flags & SF_VISIT);
    EXPECT_FALSE(m_subjects[2].m_flags & SF_VISIT);
}

TEST_F(KillThreadArticleTest, killingTheFinalUnreadThreadLeavesZeroCountsAndPreservesTheArticlePosition)
{
    set_thread(4);
    article_ptr(ArticleNum{6})->m_flags = AF_EXISTS;
    m_subjects[2].m_flags = SF_NONE;
    m_newsgroup.m_to_read = ArticleUnread{5};
    g_selected_count = ArticleUnread{4};
    g_selected_subj_cnt = 2;

    EXPECT_TRUE(kill_articles("J").empty());

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
}

TEST_P(KillThreadScanArticleTest, threadedKillAlwaysReturnsToScanRegardlessOfFollowSettings)
{
    set_thread(4);
    g_sa_in = true;
    g_sa_follow = GetParam().first;
    g_s_follow_temp = GetParam().second;

    EXPECT_TRUE(kill_articles("J", AS_SA).empty());

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_sa_in);
    EXPECT_EQ(GetParam().first, g_sa_follow);
    EXPECT_EQ(GetParam().second, g_s_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(FollowPolicy, KillThreadScanArticleTest,
                         testing::Values(std::make_pair(false, false), std::make_pair(true, false),
                                         std::make_pair(false, true)));

TEST_F(KillThreadFallbackArticleTest, unthreadedFallbackKillsMatchingSubjectsAndRestoresTheCurrentArticlePair)
{
    set_kill_articles(2);
    g_threaded_group = false;
    std::vector<ArticleNum> visited;

    const std::string output = kill_subject(AS_ASK, 2, [&](ArticleNum number) { visited.push_back(number); }, "J");

    EXPECT_NE(std::string::npos, output.find("done"));
    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{2}, ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6},
                                       ArticleNum{7}}),
              visited);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_reread);
}

TEST_F(KillThreadArticleTest, groupEndRejectsTheCommandWithoutChangingAnyThread)
{
    set_thread(8);
    g_sa_in = true;

    const std::string output = kill_articles("J", AS_ASK);

    EXPECT_NE(std::string::npos, output.find("End of group"));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_TRUE(g_sa_in);
}

TEST_F(PersistentKillArticleTest, savesALocalRuleAndKillsEarlierSubjectArticlesAndLaterMatches)
{
    set_kill_articles(2);

    const std::string output = kill_subject(AS_NORM, 2, {}, "K");

    EXPECT_NE(std::string::npos, output.find("KILL"));
    EXPECT_NE(std::string::npos, output.find("done"));
    EXPECT_EQ("/: *Target subject/:j\n", file_contents(m_kill_file));
    EXPECT_NE(nullptr, g_local_kfp);
    EXPECT_TRUE(g_kf_state & KFS_NORMAL_LINES);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_FALSE(g_reread);
}

TEST_F(PersistentKillArticleTest, repeatedKillsAppendToExistingRulesWithoutDoubleCountingArticles)
{
    set_kill_articles(2);
    std::ofstream{m_kill_file} << "/keep/:j";
    std::string expected{"/keep/:j\n"};
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        kill_subject(AS_NORM, 2, {}, "K");

        expected += "/: *Target subject/:j\n";
        EXPECT_EQ(expected, file_contents(m_kill_file));
        expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    }
}

TEST_F(PersistentKillArticleTest, savedRuleEscapesRegexCharactersAndSearchDelimiters)
{
    set_kill_articles(2);
    m_subjects[0].m_str = "Re: Target v1.0/notes";
    m_subjects[2].m_str = "Re: Target v1X0/notes";

    kill_subject(AS_NORM, 2, {}, "K");

    EXPECT_EQ("/: *Target v1\\.0\\\\/notes/:j\n", file_contents(m_kill_file));
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
}

TEST_P(PersistentKillScanArticleTest, scanReturnHonorsPersistentAndTemporaryFollowSettings)
{
    set_kill_articles(2);
    g_sa_in = true;
    g_sa_follow = GetParam().first;
    g_s_follow_temp = GetParam().second;

    kill_subject(g_sa_follow || g_s_follow_temp ? AS_NORM : AS_SA, 2, {}, "K");

    EXPECT_EQ("/: *Target subject/:j\n", file_contents(m_kill_file));
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_TRUE(g_sa_in);
    EXPECT_EQ(GetParam().first, g_sa_follow);
    EXPECT_EQ(GetParam().second, g_s_follow_temp);
    EXPECT_FALSE(g_reread);
}

INSTANTIATE_TEST_SUITE_P(FollowPolicy, PersistentKillScanArticleTest,
                         testing::Values(std::make_pair(false, false), std::make_pair(true, false),
                                         std::make_pair(false, true)));

TEST_F(PersistentKillArticleTest, groupEndDoesNotCreateARuleOrChangeArticles)
{
    set_kill_articles(8);
    g_sa_in = true;

    const std::string output = kill_subject(AS_ASK, 8, {}, "K");

    EXPECT_NE(std::string::npos, output.find("End of group"));
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
    EXPECT_EQ(nullptr, g_local_kfp);
    EXPECT_EQ(KFS_NONE, g_kf_state);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_TRUE(g_sa_in);
}

TEST_F(PersistentKillArticleTest, failedRuleSaveReportsTheErrorAndStillKillsMatchingArticles)
{
    set_kill_articles(2);
    ASSERT_TRUE(std::filesystem::create_directory(m_kill_file));

    const std::string output = kill_subject(AS_NORM, 2, {}, "K");

    EXPECT_NE(std::string::npos, output.find("Can't open"));
    EXPECT_EQ(nullptr, g_local_kfp);
    EXPECT_TRUE(std::filesystem::is_empty(m_kill_file));
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
}

TEST_P(MemorizeScopeArticleTest, selectsAndRemembersOnlyTheCurrentSubject)
{
    const std::string output = memorize(GetParam(), "S");

    EXPECT_NE(std::string::npos, output.find("Selection memorized"));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({AUTO_SEL_SBJ, AUTO_SEL_SBJ, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

TEST_P(MemorizeScopeArticleTest, remembersSingleArticleSelectionWithoutSelectingItImmediately)
{
    memorize(GetParam(), "m");

    expect_auto_flags({0, AUTO_SEL_1, 0, 0, 0, 0, 0});
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

TEST_P(MemorizeScopeArticleTest, killsAndRemembersOnlyTheCurrentArticle)
{
    const std::string output = memorize(GetParam(), "j", AS_NORM);

    EXPECT_NE(std::string::npos, output.find("Kill memorized"));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({0, AUTO_KILL_1, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

TEST_P(MemorizeScopeArticleTest, killsAndRemembersOnlyTheCurrentSubject)
{
    memorize(GetParam(), "K", AS_NORM);

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({AUTO_KILL_SBJ, AUTO_KILL_SBJ, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

TEST_P(MemorizeScopeArticleTest, clearsRememberedRepliesAcrossSubjectsWithoutChangingReadOrSelectedState)
{
    seed_auto_flags();

    memorize(GetParam(), "c");

    expect_auto_flags(
        {AUTO_SEL_1 | AUTO_OLD, 0, 0, 0, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD});
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(3, g_kf_change_thread_cnt);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
}

TEST_P(MemorizeScopeArticleTest, helpThenCancellationLeavesNoSavedOrImmediateAction)
{
    g_verbose = GetParam() == 'T';
    seed_auto_flags();

    const std::string output = memorize(GetParam(), "hq");

    EXPECT_NE(std::string::npos, output.find("abort"));
    expect_auto_flags({AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD,
                       AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD});
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_NONE, g_kf_state);
    EXPECT_EQ(0, g_kf_change_thread_cnt);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
    EXPECT_FALSE(std::filesystem::exists(m_global_file));
}

TEST_P(MemorizeScopeArticleTest, groupEndRejectsMemorizationWithoutPrompting)
{
    g_art = g_curr_art = ArticleNum{8};
    g_artp = g_curr_artp = nullptr;

    const std::string output = memorize(GetParam(), "");

    EXPECT_NE(std::string::npos, output.find("End of group"));
    EXPECT_EQ(std::string::npos, output.find("Memorize"));
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_NONE, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

INSTANTIATE_TEST_SUITE_P(Command, MemorizeScopeArticleTest, testing::Values('A', 'T'));

TEST_F(MemorizeArticleTest, threadDefaultSelectsAndRemembersTheWholeThreadFromADescendant)
{
    memorize('T', " ");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS});
    expect_auto_flags({AUTO_SEL_THD, AUTO_SEL_THD, AUTO_SEL_THD, AUTO_SEL_THD, AUTO_SEL_THD, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

TEST_F(MemorizeArticleTest, threadDotSelectsAndRemembersRepliesAcrossSubjectsIncludingReadArticles)
{
    article_ptr(ArticleNum{4})->m_flags &= ~AF_SEL;
    --g_selected_count;

    memorize('T', ".");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({0, AUTO_SEL_FOL, AUTO_SEL_FOL, AUTO_SEL_FOL, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
}

TEST_F(MemorizeArticleTest, threadJunkKillsAndRemembersTheWholeThreadIncludingSelectedArticles)
{
    memorize('T', "J", AS_NORM);

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS});
    expect_auto_flags({AUTO_KILL_THD, AUTO_KILL_THD, AUTO_KILL_THD, AUTO_KILL_THD, AUTO_KILL_THD, 0, 0});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
}

TEST_F(MemorizeArticleTest, threadCommaKillsAndRemembersRepliesWithoutKillingSiblingBranches)
{
    memorize('T', ",", AS_NORM);

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({0, AUTO_KILL_FOL, AUTO_KILL_FOL, AUTO_KILL_FOL, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
}

TEST_F(MemorizeArticleTest, threadClearRemovesOnlyThisThreadsRememberedActions)
{
    seed_auto_flags();

    memorize('T', "C");

    expect_auto_flags({0, 0, 0, 0, 0, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD});
    EXPECT_EQ(5, g_kf_change_thread_cnt);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
}

TEST_F(MemorizeArticleTest, articleClearRemovesOnlyThisSubjectsRememberedActions)
{
    seed_auto_flags();

    memorize('A', "C");

    expect_auto_flags({0, 0, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD, AUTO_SEL_1 | AUTO_OLD,
                       AUTO_SEL_1 | AUTO_OLD});
    EXPECT_EQ(2, g_kf_change_thread_cnt);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_LOCAL_CHANGES | KFS_THREAD_CHANGES, g_kf_state);
}

TEST_F(MemorizeArticleTest, articleDefaultSavesALocalSubjectSelectionRuleAndSelectsLaterMatches)
{
    memorize('A', " ");

    EXPECT_EQ("/: *Target subject/:+\n", file_contents(m_kill_file));
    EXPECT_FALSE(std::filesystem::exists(m_global_file));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_TRUE(g_kf_state & KFS_NORMAL_LINES);
}

TEST_F(MemorizeArticleTest, articleDotSavesAReplySelectionRuleAndSelectsDescendantsOfMatches)
{
    article_ptr(ArticleNum{4})->m_flags &= ~AF_SEL;
    --g_selected_count;

    memorize('A', ".");

    EXPECT_EQ("/: *Target subject/:.\n", file_contents(m_kill_file));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
}

TEST_F(MemorizeArticleTest, articleJunkSavesASubjectKillRuleAndKillsMatchesFromTheCurrentArticle)
{
    memorize('A', "J", AS_NORM);

    EXPECT_EQ("/: *Target subject/:j\n", file_contents(m_kill_file));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS});
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
}

TEST_F(MemorizeArticleTest, articleCommaSavesAReplyKillRuleAndKillsDescendantsOfMatches)
{
    memorize('A', ",", AS_NORM);

    EXPECT_EQ("/: *Target subject/:,\n", file_contents(m_kill_file));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
}

TEST_F(MemorizeArticleTest, articleCommaAlsoKillsUnreadRepliesWhenTheCurrentArticleWasAlreadyRead)
{
    g_artp->m_flags &= ~AF_UNREAD;
    --m_newsgroup.m_to_read;

    memorize('A', ",", AS_NORM);

    EXPECT_EQ("/: *Target subject/:,\n", file_contents(m_kill_file));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
}

TEST_F(MemorizeArticleTest, authorAndGlobalTogglesSaveAnAuthorRuleAndApplyItAcrossSubjects)
{
    g_parsed_art = g_art;
    g_head_buf = "From: reader@example.org\n";
    g_header_type[FROM_LINE].min_pos = ArticlePosition{0};
    g_header_type[FROM_LINE].max_pos = ArticlePosition{static_cast<long>(g_head_buf.size())};
    g_header_type[REPLY_LINE].min_pos = ArticlePosition{-1};

    const std::string output = memorize('A', "fgJ", AS_NORM);
    SCOPED_TRACE(output);

    EXPECT_NE(std::string::npos, output.find("Global-Memorize author command"));
    EXPECT_EQ("/reader@example\\.org/f:j\n", file_contents(m_global_file));
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
}

TEST_F(MemorizeArticleTest, togglingAuthorAndGlobalBackRestoresLocalSubjectRulesInAnUnthreadedGroup)
{
    g_threaded_group = false;

    memorize('A', "fgfg+");

    EXPECT_EQ("/: *Target subject/:+\n", file_contents(m_kill_file));
    EXPECT_FALSE(std::filesystem::exists(m_global_file));
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
}

TEST_F(MemorizeArticleTest, unthreadedGroupRejectsThreadMemorizationWithoutPrompting)
{
    g_threaded_group = false;

    const std::string output = memorize('T', "");

    EXPECT_NE(std::string::npos, output.find("thread"));
    EXPECT_EQ(std::string::npos, output.find("Memorize"));
    expect_auto_flags({0, 0, 0, 0, 0, 0, 0});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(KFS_NONE, g_kf_state);
    EXPECT_FALSE(std::filesystem::exists(m_kill_file));
}

TEST_P(BulkArticleOrderTest, junkAffectsOnlySelectedUnreadArticlesAndReportsExactCounts)
{
    const std::string output = apply_bulk("j\n");
    SCOPED_TRACE(output);

    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 articles killed, 3 deselected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_P(BulkArticleOrderTest, deselectLeavesReadSelectionsAndUnselectedArticlesAlone)
{
    const std::string output = apply_bulk("-\n");
    SCOPED_TRACE(output);

    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ("3 articles deselected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_P(BulkArticleOrderTest, inverseSelectionSelectsUnreadArticlesAcrossThreads)
{
    article_ptr(ArticleNum{6})->m_flags &= ~AF_SEL;
    --g_selected_count;

    apply_bulk(":+\n");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ("2 articles selected.", g_msg);
}

TEST_P(BulkArticleOrderTest, markUnreadTargetsOnlyReadSelections)
{
    apply_bulk("m\n");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ("1 article unkilled.", g_msg);
}

TEST_P(BulkArticleOrderTest, currentThreadScopeExcludesSelectedArticlesInOtherThreads)
{
    apply_bulk(".j\n");

    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ("2 articles killed, 2 deselected.", g_msg);
}

TEST_P(BulkArticleOrderTest, inverseCurrentThreadScopeExcludesOtherThreadsAndReadArticles)
{
    article_ptr(ArticleNum{6})->m_flags &= ~AF_SEL;
    --g_selected_count;

    apply_bulk(":.+\n");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ("1 article selected.", g_msg);
}

TEST_P(BulkArticleOrderTest, rereadingModeDeselectsOnlyReadSelections)
{
    g_sel_rereading = true;

    apply_bulk("-\n");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ("1 article deselected.", g_msg);
}

TEST_P(BulkArticleOrderTest, noUnreadSelectionsReportsNoArticlesAffected)
{
    for (const long number : {1, 4, 6})
    {
        article_ptr(ArticleNum{number})->m_flags &= ~AF_SEL;
    }
    g_selected_count = ArticleUnread{1};

    const std::string output = apply_bulk("j\n");

    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ("No articles affected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_P(BulkArticleOrderTest, bulkActionFromGroupEndRestoresTheNullReadingAnchor)
{
    g_art = g_curr_art = ArticleNum{8};
    g_artp = g_curr_artp = nullptr;

    apply_bulk("j\n");

    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
}

TEST_P(BulkArticleOrderTest, invalidActionStopsAtTheFirstMatchAndPreservesCompletedEffects)
{
    const std::string output = apply_bulk("j?\n");

    EXPECT_NE(std::string::npos, output.find("Unknown command: ?"));
    const long killed = GetParam() ? 6 : 1;
    for (long number = 1; number <= 7; ++number)
    {
        SCOPED_TRACE(number);
        const ArticleFlags flags[] = {AF_EXISTS | AF_UNREAD | AF_SEL,
                                      AF_EXISTS | AF_UNREAD,
                                      AF_EXISTS | AF_SEL,
                                      AF_EXISTS | AF_UNREAD | AF_SEL,
                                      AF_EXISTS,
                                      AF_EXISTS | AF_UNREAD | AF_SEL,
                                      AF_EXISTS};
        EXPECT_EQ(number == killed ? AF_EXISTS : flags[number - 1], article_ptr(ArticleNum{number})->m_flags);
    }
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ("1 article killed, 1 deselected.", g_msg);
}

INSTANTIATE_TEST_SUITE_P(ReadingOrder, BulkArticleOrderTest, testing::Bool());

TEST_F(BulkSelectionArticleTest, erasedCommandRequestsInputWithoutAffectingArticles)
{
    g_msg = "prior status";

    const std::string output = apply_bulk("\b", AS_INP);

    expect_bulk_unchanged();
    EXPECT_EQ("prior status", g_msg);
    EXPECT_EQ(std::string::npos, output.find("prior status"));
}

TEST_P(EmptyBulkCommandArticleTest, requestsInputWithoutReportingAStaleStatus)
{
    const std::string output = apply_bulk(GetParam().input, AS_INP);

    expect_bulk_unchanged();
    EXPECT_EQ("prior status", g_msg);
    EXPECT_EQ(std::string::npos, output.find("prior status"));
}

INSTANTIATE_TEST_SUITE_P(CommandPrefix, EmptyBulkCommandArticleTest,
                         testing::Values(EmptyBulkCommandCase{"Plain", "\n"}, EmptyBulkCommandCase{"Inverse", ":\n"},
                                         EmptyBulkCommandCase{"CurrentThread", ".\n"},
                                         EmptyBulkCommandCase{"InverseCurrentThread", ":.\n"}),
                         [](const testing::TestParamInfo<EmptyBulkCommandCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(BulkSelectionArticleTest, currentThreadScopeAtGroupEndRequestsInputWithoutAffectingArticles)
{
    g_art = g_curr_art = ArticleNum{8};
    g_artp = g_curr_artp = nullptr;
    g_msg = "prior status";

    apply_bulk(".j\n", AS_INP);

    expect_bulk_unchanged();
    EXPECT_EQ("prior status", g_msg);
}

TEST_F(BulkSelectionArticleTest, threadShortcutKillsWholeSelectedThreadsIncludingUnselectedUnreadArticles)
{
    const std::string output = apply_bulk("J\n");

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{0}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_FALSE(g_performed_article_loop);
    EXPECT_EQ("2 threads matched, 4 articles killed, 3 deselected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_F(BulkSelectionArticleTest, inverseThreadShortcutLeavesSelectedThreadsAlone)
{
    m_subjects[2].m_flags &= ~SF_SEL;
    --g_selected_subj_cnt;
    article_ptr(ArticleNum{6})->m_flags &= ~AF_SEL;
    --g_selected_count;

    apply_bulk(":J\n");

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_FALSE(g_performed_article_loop);
    EXPECT_EQ("1 thread matched, 1 article killed.", g_msg);
}

TEST_P(SubjectThreadSelectionArticleTest, selectsTheScopeAndStartsAtItsFirstUnreadArticleWithoutDoubleCounting)
{
    const long target = GetParam() ? 2 : 4;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        const std::string output = change_scope('+', target, attempt ? AS_ASK : AS_NORM);

        EXPECT_NE(std::string::npos, output.find(GetParam() ? "Selected all articles in this thread."
                                                            : "Selected all articles in this subject."));
        expect_flags({AF_EXISTS, GetParam() ? AF_EXISTS | AF_UNREAD | AF_SEL : AF_EXISTS | AF_UNREAD, AF_EXISTS,
                      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{GetParam() ? 4 : 3}, g_selected_count);
        EXPECT_EQ(GetParam() ? 3 : 2, g_selected_subj_cnt);
        EXPECT_EQ(SF_VISIT | SF_SEL | SF_WAS_SELECTED, m_subjects[1].m_flags);
        EXPECT_EQ(SF_VISIT | SF_SEL, m_subjects[2].m_flags);
        EXPECT_TRUE(g_selected_only);
    }
}

TEST_P(SubjectThreadSelectionArticleTest, selectionEnablesSelectedOnlyModeWithoutSelectingOtherScopes)
{
    set_selection_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS});
    ASSERT_FALSE(g_selected_only);

    change_scope('+', GetParam() ? 2 : 4, AS_NORM);

    expect_flags({AF_EXISTS, GetParam() ? AF_EXISTS | AF_UNREAD | AF_SEL : AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{GetParam() ? 3 : 2}, g_selected_count);
    EXPECT_EQ(GetParam() ? 2 : 1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
}

TEST_P(SubjectThreadSelectionArticleTest, selectionSkipsMissingEntriesBeforeTheFirstEligibleArticle)
{
    set_selection_scope({AF_NONE, AF_NONE, AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                         AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});

    change_scope('+', 4, AS_NORM);

    expect_flags({AF_NONE, AF_NONE, AF_NONE, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
}

TEST_P(SubjectThreadSelectionArticleTest, selectionOfAnAllReadScopePreservesTheReadingAnchor)
{
    set_selection_scope(
        {AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});

    change_scope('+', 5);

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
}

TEST_P(SubjectThreadSelectionArticleTest, uncachedCurrentArticleIsSelectedWithoutLosingItsReadingPointer)
{
    m_subjects[1].m_articles = nullptr;
    m_subjects[1].m_thread = nullptr;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        change_scope('+', 5);

        expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{3}, g_selected_count);
        EXPECT_EQ(2, g_selected_subj_cnt);
    }
}

TEST_P(SubjectThreadSelectionArticleTest, rereadingSelectionStartsAtTheFirstReadArticle)
{
    set_selection_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS});
    g_sel_rereading = true;

    change_scope('+', GetParam() ? 1 : 3, AS_NORM);

    expect_flags({GetParam() ? AF_EXISTS | AF_SEL : AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{GetParam() ? 2 : 1}, g_selected_count);
    EXPECT_EQ(GetParam() ? 2 : 1, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
}

TEST_P(SubjectThreadSelectionArticleTest, deselectsOnlyTheScopeIncludingReadSelectionsWithoutDoubleCounting)
{
    set_selection_scope({AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL,
                         AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                         AF_EXISTS | AF_YANK_BACK});
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        const std::string output = change_scope('-', 5);

        EXPECT_NE(std::string::npos, output.find(GetParam() ? "Deselected all articles in this thread."
                                                            : "Deselected all articles in this subject."));
        expect_flags({GetParam() ? AF_EXISTS : AF_EXISTS | AF_SEL,
                      GetParam() ? AF_EXISTS | AF_UNREAD : AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                      AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{GetParam() ? 1 : 3}, g_selected_count);
        EXPECT_EQ(GetParam() ? 1 : 2, g_selected_subj_cnt);
        EXPECT_EQ(SF_NONE, m_subjects[1].m_flags);
        EXPECT_EQ(SF_VISIT | SF_SEL, m_subjects[2].m_flags);
        EXPECT_TRUE(g_selected_only);
    }
}

TEST_P(SubjectThreadSelectionArticleTest, deselectionScopeDoesNotDependOnThePreviouslyUsedSelectorMode)
{
    set_selection_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    g_sel_mode = GetParam() ? SM_ARTICLE : SM_THREAD;

    change_scope('-', 5);

    expect_flags({AF_EXISTS, GetParam() ? AF_EXISTS | AF_UNREAD : AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{GetParam() ? 1 : 2}, g_selected_count);
    EXPECT_EQ(GetParam() ? 1 : 2, g_selected_subj_cnt);
}

TEST_P(SubjectThreadSelectionArticleTest, deselectingTheLastSelectionKeepsCountsAtZeroOnRepeat)
{
    set_selection_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                         AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS});
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        change_scope('-', 5);

        expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                      AF_EXISTS | AF_UNREAD, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(0, g_selected_subj_cnt);
    }
}

TEST_P(GroupEndScopeSelectionArticleTest, rejectsCommandWithoutChangingSelections)
{
    const std::string output = change_scope(std::get<1>(GetParam()), 8);

    EXPECT_NE(std::string::npos, output.find("End of group"));
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
}

INSTANTIATE_TEST_SUITE_P(ReadingScope, SubjectThreadSelectionArticleTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(ReadingScopeAndCommand, GroupEndScopeSelectionArticleTest,
                         testing::Combine(testing::Bool(), testing::Values('+', '-')),
                         [](const testing::TestParamInfo<std::tuple<bool, char>> &info)
                         {
                             return std::string{std::get<0>(info.param) ? "Thread" : "Subject"} +
                                    (std::get<1>(info.param) == '+' ? "Select" : "Deselect");
                         });

TEST_F(ScopeSelectionArticleTest, breadthFirstSelectionSkipsReadAncestorsBeforeChoosingAnUnreadSibling)
{
    g_breadth_first = true;
    article_ptr(ArticleNum{2})->m_flags &= ~AF_UNREAD;
    --m_newsgroup.m_to_read;

    change_scope('+', 5);

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
}

TEST_P(DelaySubthreadOrderArticleTest, delaysCrossSubjectDescendantsWithoutAncestorsOrSiblingBranchesOnRepeat)
{
    set_delay_articles(2);
    g_breadth_first = GetParam();
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        delay_scope('m');

        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
                      AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{3}, g_selected_count);
        EXPECT_EQ(4, g_dm_count);
        for (const Subject &subject : m_subjects)
        {
            EXPECT_EQ(SF_SEL | SF_VISIT | SF_WAS_SELECTED, subject.m_flags);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(ReadingOrder, DelaySubthreadOrderArticleTest, testing::Bool());

TEST_F(DelayScopeArticleTest, readCurrentAndIntermediateArticlesAreMarkedForReturnWithoutChangingTheirReadCounts)
{
    set_delay_articles(2);
    g_artp->m_flags = AF_EXISTS | AF_DEL;
    --m_newsgroup.m_to_read;
    --g_selected_count;
    article_ptr(ArticleNum{3})->m_flags = AF_EXISTS;
    --g_dm_count;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        delay_scope('m');

        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_DEL | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
                      AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{3}, g_selected_count);
        EXPECT_EQ(4, g_dm_count);
    }
}

TEST_F(DelayScopeArticleTest, leafSubthreadStopsAtTheSiblingOfAnAncestor)
{
    set_delay_articles(4);

    delay_scope('m');

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(3, g_dm_count);
}

TEST_F(DelayScopeArticleTest, expiredDelayedIntermediateStillLeadsToUnreadDescendants)
{
    set_delay_articles(2);
    article_ptr(ArticleNum{3})->m_flags = AF_YANK_BACK;

    delay_scope('m');

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK, AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(4, g_dm_count);
}

TEST_F(DelayScopeArticleTest, delaysTheWholeThreadFromAnUnselectedDescendantWithoutDoubleCounting)
{
    set_delay_articles(4);
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        delay_scope('M');

        expect_flags({AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
                      AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
        EXPECT_EQ(6, g_dm_count);
        EXPECT_EQ(SF_SEL, m_subjects[0].m_flags);
        EXPECT_EQ(SF_SEL, m_subjects[1].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT | SF_WAS_SELECTED, m_subjects[2].m_flags);
    }
}

TEST_F(DelayScopeArticleTest, delayingAnAllReadThreadOnlyAddsReturnFlags)
{
    set_delay_articles(4);
    g_breadth_first = true;
    for (const long number : {1, 2, 4, 5})
    {
        article_ptr(ArticleNum{number})->one_less();
    }
    g_artp->m_flags |= AF_DEL;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        delay_scope('M');

        expect_flags({AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
                      AF_EXISTS | AF_DEL | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_YANK_BACK});
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
        EXPECT_EQ(6, g_dm_count);
    }
}

TEST_F(DelayScopeArticleTest, missingThreadDataFallsBackToOnlyTheCurrentSubject)
{
    set_delay_articles(4);
    g_threaded_group = false;
    g_sel_mode = SM_SUBJECT;
    m_subjects[1].m_thread = nullptr;

    delay_scope('M');

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(4, g_dm_count);
    EXPECT_EQ(SF_SEL | SF_VISIT | SF_WAS_SELECTED, m_subjects[0].m_flags);
    EXPECT_EQ(SF_SEL, m_subjects[1].m_flags);
}

TEST_P(DelayScopeCommandArticleTest, uncachedUnthreadedArticleFallsBackToOnlyTheCurrentArticle)
{
    set_delay_articles(2);
    g_threaded_group = false;
    g_sel_mode = SM_ARTICLE;
    g_artp->m_subj = nullptr;
    g_artp->m_parent = nullptr;
    g_artp->m_child1 = nullptr;
    g_artp->m_sibling = nullptr;

    delay_scope(GetParam());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(3, g_dm_count);
}

TEST_P(DelayScopeCommandArticleTest, groupEndRejectsDelayWithoutChangingArticlesOrPendingReturns)
{
    set_delay_articles(8);

    const std::string output = delay_scope(GetParam(), AS_ASK);

    EXPECT_NE(std::string::npos, output.find("End of group"));
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(2, g_dm_count);
}

TEST_P(DelayScopeCommandArticleTest, lastThreadStopsAtNullAndContinuesReadingFromScanWithoutFollow)
{
    set_delay_articles(6);
    g_sa_in = true;
    g_sa_follow = false;
    g_s_follow_temp = false;
    for (const long number : {5, 1, 6, 2, 4, 3, 7})
    {
        g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
    }
    g_art_ptr = g_art_ptr_list.data() + 2;

    delay_scope(GetParam());

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_YANK_BACK,
                  AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(3, g_dm_count);
}

INSTANTIATE_TEST_SUITE_P(Command, DelayScopeCommandArticleTest, testing::Values('m', 'M'));

TEST_P(RestoreSingleArticleTest, restoresAnUnreadSelectedArticleAfterMWithoutChangingOtherReadState)
{
    set_scope({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
               AF_EXISTS | AF_UNREAD, AF_EXISTS},
              2);
    g_threaded_group = GetParam();
    g_selected_only = true;
    m_subjects[0].m_flags |= SF_SEL;
    g_selected_subj_cnt = 1;
    testing::internal::CaptureStdout();
    EXPECT_EQ(AS_ASK, art_switch("M"));
    EXPECT_NE(std::string::npos, testing::internal::GetCapturedStdout().find("Article 2 will return."));
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(1, g_dm_count);
    g_first_art = ArticleNum{4};

    EXPECT_EQ("\nReturning 1 Marked article...\n", restore_delayed(2));

    expect_flags({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
}

TEST_P(RestoreSingleArticleTest, restoresAPreviouslyReadArticleAndRepeatedYDoesNotCountItAgain)
{
    set_scope(
        {AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS},
        1);
    g_threaded_group = GetParam();
    g_first_art = ArticleNum{2};
    testing::internal::CaptureStdout();
    EXPECT_EQ(AS_ASK, art_switch("M"));
    EXPECT_NE(std::string::npos, testing::internal::GetCapturedStdout().find("Article 1 will return."));
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(AF_EXISTS | AF_DEL | AF_YANK_BACK, g_artp->m_flags);

    EXPECT_EQ("\nReturning 1 Marked article...\n", restore_delayed(1));

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                      AF_EXISTS, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(0, g_selected_subj_cnt);
        EXPECT_EQ(ArticleNum{1}, g_first_art);
        EXPECT_TRUE(restore_delayed(1).empty());
    }
}

INSTANTIATE_TEST_SUITE_P(Threading, RestoreSingleArticleTest, testing::Bool());

TEST_P(RestoreScopeArticleTest, restoresTheDelayedScopeAndEarlierPendingArticlesWithoutChangingUnmarkedReadArticles)
{
    set_thread(2);
    article_ptr(ArticleNum{1})->one_less();
    push_char(GetParam());
    testing::internal::CaptureStdout();
    EXPECT_EQ(AS_NORM, art_switch(std::string{'_', FINISH_CMD}));
    (void) testing::internal::GetCapturedStdout();
    EXPECT_EQ(GetParam() == 'm' ? 4 : 6, g_dm_count);
    g_first_art = ArticleNum{6};

    EXPECT_EQ(GetParam() == 'm' ? "\nReturning 4 Marked articles...\n" : "\nReturning 6 Marked articles...\n",
              restore_delayed(GetParam() == 'm' ? 2 : 1));

    expect_flags({GetParam() == 'm' ? AF_EXISTS : AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL});
    EXPECT_EQ(ArticleUnread{GetParam() == 'm' ? 6 : 7}, m_newsgroup.m_to_read);
    EXPECT_EQ(m_newsgroup.m_to_read, g_selected_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{GetParam() == 'm' ? 2 : 1}, g_first_art);
    for (const Subject &subject : m_subjects)
    {
        EXPECT_EQ(SF_SEL | SF_VISIT, subject.m_flags);
    }
}

INSTANTIATE_TEST_SUITE_P(Command, RestoreScopeArticleTest, testing::Values('m', 'M'));

TEST_F(RestoreDelayedArticleTest, anArticleMadeUnreadBeforeYIsNotCountedOrSelectedTwice)
{
    set_thread(2);
    testing::internal::CaptureStdout();
    EXPECT_EQ(AS_ASK, art_switch("M"));
    EXPECT_EQ(AS_ASK, art_switch("m"));
    (void) testing::internal::GetCapturedStdout();
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(2, g_dm_count);

    EXPECT_EQ("\nReturning 2 Marked articles...\n", restore_delayed(1));

    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL});
    EXPECT_EQ(ArticleUnread{7}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{6}, g_selected_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
}

TEST_P(RestoreOrderArticleTest, restartsAtTheFirstEligibleArticleInReadingOrderIncludingExistingUnreadArticles)
{
    set_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
              7);
    g_dm_count = 2;
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    m_subjects[2].m_flags |= SF_SEL;
    g_first_art = ArticleNum{6};
    g_threaded_group = GetParam() == RestoreOrder::DepthFirst || GetParam() == RestoreOrder::BreadthFirst;
    g_breadth_first = GetParam() == RestoreOrder::BreadthFirst;
    g_search_ahead = GetParam() == RestoreOrder::Subject ? ArticleNum{4} : ArticleNum{};
    g_sa_in = true;
    g_sa_follow = false;
    g_s_follow_temp = false;
    if (GetParam() == RestoreOrder::Sorted)
    {
        for (const long number : {3, 2, 6, 5, 1, 4, 7})
        {
            g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
        }
        g_art_ptr = g_art_ptr_list.data() + 6;
    }
    const long first = GetParam() == RestoreOrder::Sorted ? 6 : GetParam() == RestoreOrder::BreadthFirst ? 5 : 4;

    EXPECT_EQ("\nReturning 2 Marked articles...\n", restore_delayed(first));

    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{4}, g_first_art);
    EXPECT_EQ(SF_VISIT, m_subjects[0].m_flags);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[1].m_flags);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
    EXPECT_EQ(GetParam() == RestoreOrder::Sorted ? g_art_ptr_list.data() + 2 : nullptr, g_art_ptr);
}

INSTANTIATE_TEST_SUITE_P(ReadingOrder, RestoreOrderArticleTest,
                         testing::Values(RestoreOrder::Numeric, RestoreOrder::Subject, RestoreOrder::DepthFirst,
                                         RestoreOrder::BreadthFirst, RestoreOrder::Sorted));

TEST_F(RestoreDelayedArticleTest, noPendingArticlesStillRestartsAtExistingUnreadArticlesFromGroupEnd)
{
    set_scope(
        {AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS},
        8);
    g_first_art = ArticleNum{2};

    EXPECT_TRUE(restore_delayed(2).empty());

    expect_flags(
        {AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
}

TEST_F(RestoreDelayedArticleTest, emptyRestorationInAnAllReadGroupKeepsTheEndNumberAndNullPointerTogether)
{
    set_scope({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 8);
    g_first_art = ArticleNum{8};
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_TRUE(restore_delayed(8).empty());

        expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(ArticleNum{8}, g_first_art);
    }
}

TEST_F(RestoreDelayedArticleTest, missingMarkedArticlesAreDiscardedWithoutAddingUnreadOrSelectionCounts)
{
    set_scope(
        {AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 8);
    g_dm_count = 2;
    g_selected_only = true;
    g_first_art = ArticleNum{3};

    EXPECT_EQ("\nReturning 2 Marked articles...\n", restore_delayed(2));

    expect_flags(
        {AF_NONE, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
    EXPECT_EQ(SF_VISIT, m_subjects[1].m_flags);
    EXPECT_TRUE(restore_delayed(2).empty());
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
}

TEST_F(RestoreDelayedArticleTest, missingMarkedArticlesInExplicitOrderNeverBecomeReadingTargets)
{
    set_scope(
        {AF_YANK_BACK, AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 8);
    g_dm_count = 2;
    g_first_art = ArticleNum{3};
    for (const long number : {1, 2, 3, 4, 5, 6, 7})
    {
        g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
    }
    g_art_ptr = g_art_ptr_list.data() + 6;

    EXPECT_EQ("\nReturning 2 Marked articles...\n", restore_delayed(2));

    expect_flags({AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(g_art_ptr_list.data() + 1, g_art_ptr);
}

TEST_F(RestoreDelayedArticleTest, expirationAfterMLeavesNoArticleToRestoreAndClearsThePendingCount)
{
    set_scope({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 1);
    testing::internal::CaptureStdout();
    EXPECT_EQ(AS_ASK, art_switch("M"));
    (void) testing::internal::GetCapturedStdout();
    g_artp->one_missing();
    g_first_art = ArticleNum{2};
    EXPECT_EQ(1, g_dm_count);

    EXPECT_EQ("\nReturning 1 Marked article...\n", restore_delayed(2));

    expect_flags(
        {AF_CACHED | AF_THREADED, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
}

TEST_P(CatchupConfirmArticleTest, confirmationMarksAllReadAndKeepsTheEndNumberAndPointerTogetherOnRepeat)
{
    set_catchup_articles();
    article_ptr(ArticleNum{7})->m_flags &= ~AF_YANK_BACK;
    g_dm_count = 0;
    g_sa_in = true;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        catch_up(std::string{GetParam()}, AS_NORM);

        expect_flags({CACHED, CACHED, CACHED | AF_DEL, CACHED, AF_CACHED | AF_THREADED, CACHED, CACHED});
        EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(0, g_selected_subj_cnt);
        EXPECT_EQ(0, g_dm_count);
        EXPECT_FALSE(g_selected_only);
        EXPECT_FALSE(g_force_last);
        EXPECT_EQ(ArticleNum{8}, g_art);
        EXPECT_EQ(nullptr, g_artp);
        expect_subscription_unchanged();
    }
}

INSTANTIATE_TEST_SUITE_P(Answer, CatchupConfirmArticleTest, testing::Values('y', ' '));

TEST_P(CatchupCancelArticleTest, cancellationPreservesArticlesCountsAndReadingPosition)
{
    catch_up(std::string{std::get<0>(GetParam())}, AS_ASK);

    EXPECT_EQ(ArticleNum{std::get<1>(GetParam())}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    expect_catchup_unchanged();
}

INSTANTIATE_TEST_SUITE_P(AnswerAndPosition, CatchupCancelArticleTest,
                         testing::Combine(testing::Values('n', 'q'), testing::Values(4L, 8L)),
                         [](const testing::TestParamInfo<std::tuple<char, long>> &info)
                         {
                             return std::string{std::get<0>(info.param) == 'n' ? "Decline" : "Quit"} +
                                    (std::get<1>(info.param) == 8 ? "AtGroupEnd" : "AtArticle");
                         });

TEST_P(CatchupHelpArticleTest, helpExplainsTheChoicesAndReturnsToThePromptBeforeCancellation)
{
    set_catchup_articles();
    g_verbose = GetParam();

    const std::string output = catch_up("hHn", AS_ASK);

    EXPECT_NE(std::string::npos,
              output.find(g_verbose ? "Type y or SP to mark all articles as read." : "y or SP to mark all read."));
    EXPECT_NE(std::string::npos, output.find(g_verbose ? "Type u to mark everything read and unsubscribe."
                                                       : "u to mark all and unsubscribe."));
    EXPECT_NE(std::string::npos,
              output.find(g_verbose ? "The # means enter a number" : "# means enter a number to leave unread."));
    EXPECT_NE(output.find("comp.test.catchup"), output.rfind("comp.test.catchup"));
    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    expect_catchup_unchanged();
}

INSTANTIATE_TEST_SUITE_P(Verbosity, CatchupHelpArticleTest, testing::Bool());

TEST_P(CatchupTailArticleTest, numericInputLeavesAnApproximateTailAndPreservesReadAndDelayedArticlesInIt)
{
    set_catchup_articles();

    catch_up(GetParam(), AS_NORM);

    expect_flags({CACHED, CACHED, CACHED | AF_DEL, CACHED, AF_CACHED | AF_THREADED, CACHED | AF_UNREAD | AF_SEL,
                  CACHED | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
    EXPECT_EQ(SF_SEL, m_subjects[0].m_flags);
    EXPECT_EQ(SF_SEL, m_subjects[1].m_flags);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
    EXPECT_EQ(ArticleNum{6}, g_first_art);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    expect_subscription_unchanged();
}

INSTANTIATE_TEST_SUITE_P(Input, CatchupTailArticleTest,
                         testing::Values(std::string{"3\n"}, std::string{"#3\n"}, std::string{"04\b3\n"}));

TEST_P(CatchupZeroArticleTest, zeroMarksAllReadButReturnsArticlesThatWereExplicitlyDelayed)
{
    set_catchup_articles();

    const std::string output = catch_up(GetParam(), AS_NORM);

    EXPECT_NE(std::string::npos, output.find("Returning 1 Marked article"));
    expect_flags({CACHED, CACHED, CACHED | AF_DEL, CACHED, AF_CACHED | AF_THREADED, CACHED, CACHED | AF_UNREAD});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(0, g_dm_count);
    EXPECT_FALSE(g_selected_only);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    expect_subscription_unchanged();
}

INSTANTIATE_TEST_SUITE_P(Input, CatchupZeroArticleTest, testing::Values(std::string{"0\n"}, std::string{"#0"}));

TEST_P(CatchupNumericCancelArticleTest, deletingTheNumericAnswerReturnsToConfirmationWithoutCatchingUp)
{
    set_catchup_articles();

    const std::string output = catch_up(std::string{"4"} + GetParam() + 'n', AS_ASK);

    EXPECT_NE(output.find("Catchup comp.test.catchup?"), output.rfind("Catchup comp.test.catchup?"));
    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    expect_catchup_unchanged();
}

INSTANTIATE_TEST_SUITE_P(Editing, CatchupNumericCancelArticleTest, testing::Values('\b', static_cast<char>(Ctl('U'))));

TEST_F(CatchupArticleTest, aTailLargerThanTheGroupDoesNotMarkAnyArticlesRead)
{
    set_catchup_articles();

    catch_up("99\n", AS_NORM);

    expect_flags({CACHED | AF_UNREAD | AF_SEL, CACHED | AF_UNREAD, CACHED | AF_DEL, CACHED | AF_UNREAD | AF_SEL,
                  AF_CACHED | AF_THREADED, CACHED | AF_UNREAD | AF_SEL, CACHED | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    expect_subscription_unchanged();
}

TEST_P(CatchupUnsubscribeArticleTest, unsubscribeCatchesUpAndRequestsCleanupWhilePreservingTheReadingAnchor)
{
    set_catchup_articles(GetParam() ? 8 : 4);
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);

    const std::string output = catch_up("u", AS_CLEAN);

    EXPECT_NE(std::string::npos, output.find("Unsubscribed to newsgroup comp.test.catchup"));
    EXPECT_NE(std::string::npos, output.find("If you meant to hit 'y' instead of 'u'"));
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    expect_flags({CACHED, CACHED, CACHED | AF_DEL, CACHED, AF_CACHED | AF_THREADED, CACHED, CACHED | AF_UNREAD});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(0, g_dm_count);
    EXPECT_FALSE(g_selected_only);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ('!', m_newsgroup.m_subscribe_char);
    EXPECT_EQ(RF_ACTIVE | RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_EQ(NewsgroupNum{4}, g_newsgroup_to_read);
}

INSTANTIATE_TEST_SUITE_P(GroupEnd, CatchupUnsubscribeArticleTest, testing::Bool());

TEST_P(CatchupUncachedTailArticleTest, partialCatchupCountsCachedAndUncachedUnreadArticlesExactlyOnce)
{
    set_catchup_articles();
    g_last_cached = ArticleNum{5};
    article_ptr(ArticleNum{6})->m_flags = AF_EXISTS | AF_UNREAD;
    article_ptr(ArticleNum{6})->m_subj = nullptr;
    article_ptr(ArticleNum{7})->m_flags = AF_EXISTS | AF_YANK_BACK;
    article_ptr(ArticleNum{7})->m_subj = nullptr;
    m_subjects[2].m_articles = nullptr;
    m_subjects[2].m_flags = SF_NONE;
    --g_selected_count;
    --g_selected_subj_cnt;
    g_selected_only = false;

    catch_up(std::to_string(GetParam()) + '\n', AS_NORM);

    expect_flags({CACHED, CACHED, CACHED | AF_DEL, GetParam() == 4 ? CACHED | AF_UNREAD | AF_SEL : CACHED,
                  AF_CACHED | AF_THREADED, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_YANK_BACK});
    EXPECT_EQ(ArticleUnread{GetParam() == 4 ? 2 : 1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{GetParam() == 4 ? 2 : 1}, g_obj_count);
    EXPECT_EQ(ArticleUnread{GetParam() == 4 ? 1 : 0}, g_selected_count);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_FALSE(g_selected_only);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    expect_subscription_unchanged();
}

INSTANTIATE_TEST_SUITE_P(Tail, CatchupUncachedTailArticleTest, testing::Values(3, 4));

TEST_P(UnsubscribePositionArticleTest, requestsCleanupWithoutChangingArticlesOrTheReadingAnchor)
{
    set_catchup_articles(GetParam());

    unsubscribe();

    EXPECT_EQ(ArticleNum{GetParam()}, g_art);
    EXPECT_EQ(GetParam() == 8 ? nullptr : article_find(g_art), g_artp);
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(1, g_dm_count);
    EXPECT_EQ(NewsgroupNum{4}, g_newsgroup_to_read);
}

INSTANTIATE_TEST_SUITE_P(Position, UnsubscribePositionArticleTest, testing::Values(3L, 4L, 8L));

TEST_F(UnsubscribeArticleTest, sortedScanViewStillRequestsGroupCleanupAndRetainsItsCursor)
{
    set_catchup_articles();
    g_threaded_group = false;
    g_sa_in = true;
    g_sa_follow = false;
    g_s_follow_temp = true;
    for (const long number : {6, 3, 4, 1, 7, 2, 5})
    {
        g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
    }
    g_art_ptr = g_art_ptr_list.data() + 2;

    unsubscribe();

    EXPECT_EQ(g_art_ptr_list.data() + 2, g_art_ptr);
    EXPECT_EQ(article_find(ArticleNum{4}), *g_art_ptr);
}

TEST_F(UnsubscribeArticleTest, anAlreadyDirtyNewsrcRetainsItsOtherFlags)
{
    set_catchup_articles();
    m_newsrc.flags = RF_ACTIVE | RF_OPEN | RF_RC_CHANGED;

    unsubscribe();

    EXPECT_EQ(RF_ACTIVE | RF_OPEN | RF_RC_CHANGED, m_newsrc.flags);
}

TEST_F(UnsubscribeArticleTest, unsubscribingTheLastGroupInTheAllGroupsViewLeavesNoGroupsToRead)
{
    set_catchup_articles(8);
    for (auto &[number, entry] : g_article_list)
    {
        entry.one_less();
        entry.m_flags &= ~AF_YANK_BACK;
    }
    for (Subject &subject : m_subjects)
    {
        subject.m_flags = SF_NONE;
    }
    g_selected_only = false;
    g_selected_subj_cnt = 0;
    g_dm_count = 0;
    g_first_art = ArticleNum{8};
    g_newsgroup_min_to_read = TR_NONE;
    g_newsgroup_to_read = NewsgroupNum{1};

    unsubscribe();

    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
}

TEST_F(UnsubscribeArticleTest, cleanupSerializesTheUnsubscribeWhileKeepingUnreadAndDelayedArticles)
{
    set_catchup_articles();
    unsubscribe();
    testing::internal::CaptureStdout();

    deselect_all();
    yank_back();
    bits_to_rc();

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(std::string::npos, output.find("Returning 1 Marked article"));
    expect_flags({CACHED | AF_UNREAD, CACHED | AF_UNREAD, CACHED | AF_DEL, CACHED | AF_UNREAD, AF_CACHED | AF_THREADED,
                  CACHED | AF_UNREAD, CACHED | AF_UNREAD});
    EXPECT_EQ(TR_UNSUB, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(0, g_dm_count);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(NewsgroupNum{4}, g_newsgroup_to_read);
    EXPECT_EQ(RF_ACTIVE | RF_RC_CHANGED, m_newsrc.flags);
    EXPECT_EQ('!', m_newsgroup.m_subscribe_char);
    EXPECT_EQ(" 1-0,3,5", m_newsgroup.rc_numbers());
    m_newsgroup.show_subscribe_char();
    EXPECT_EQ("comp.test.catchup! 1-0,3,5", m_newsgroup.m_rc_line);
}

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

TEST_P(SortedForwardArticleNavigationTest, nextUnreadFollowsDisplayedOrderInsteadOfNumericOrThreadOrder)
{
    set_forward_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD});

    expect_next_unread(1, 2);
    expect_next_unread(3, 3);
    expect_next_unread(2, 0);
}

INSTANTIATE_TEST_SUITE_P(ReadingMode, SortedForwardArticleNavigationTest, testing::Bool());

TEST_F(SortedArticleNavigationTest, nextUnreadSkipsReadEntriesButIncludesUnselectedUnreadArticles)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD}, 4);
    set_order({4, 2, 1, 3});
    g_search_ahead = ArticleNum{2};

    expect_next_unread(1, 2);
    expect_next_unread(3, 3);
}

TEST_F(SortedArticleNavigationTest, selectedNextUnreadSkipsUnselectedUnreadAndSelectedReadEntries)
{
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD},
                 4);
    set_order({4, 2, 3, 1, 5});
    g_selected_only = true;

    expect_next_unread(1, 3);
    expect_next_unread(4, 0);
}

TEST_F(SortedArticleNavigationTest, nextUnreadWrapsToTheFirstEligibleSortedEntry)
{
    set_articles(
        {AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS},
        3);
    set_order({5, 4, 2, 1, 3});
    g_selected_only = true;

    expect_next_unread(2, 2);
}

TEST_F(SortedArticleNavigationTest, nextUnreadFromGroupEndRestartsAtTheFirstEligibleSortedEntry)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL}, 5);
    set_order({2, 4, 1, 3});
    g_art_ptr = article_ptr_list_begin() + 3;

    expect_next_unread(4, 1);
}

TEST_F(SortedArticleNavigationTest, selectedNextUnreadWrapsBackToTheOnlyEligibleArticle)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL}, 3);
    set_order({2, 3, 1});
    g_selected_only = true;

    expect_next_unread(3, 1);
}

TEST_P(SortedExhaustedArticleNavigationTest, nextUnreadHandlesAnExhaustedSortedGroup)
{
    expect_next_unread(4, 0, GetParam() ? AS_CLEAN : AS_NORM);
}

INSTANTIATE_TEST_SUITE_P(StartPosition, SortedExhaustedArticleNavigationTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info)
                         { return info.param ? "AtGroupEnd" : "AtLastArticle"; });

TEST_F(SortedArticleNavigationTest, selectedNextUnreadRemainsAtGroupEndWhenOnlyUnselectedUnreadArticlesRemain)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS}, 3);
    set_order({1, 2, 3});
    g_selected_only = true;

    expect_next_unread(4, 0);
    expect_next_unread(4, 0);
}

TEST_F(SortedArticleNavigationTest, nextUnreadRepairsTheSortedCursorAfterAnArticleJump)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 4);
    set_order({2, 4, 1, 3});
    expect_next_unread(1, 2);
    g_art = ArticleNum{2};
    g_artp = article_ptr(g_art);

    expect_next_unread(4, 1);
}

TEST_P(SortedExplicitForwardArticleNavigationTest, nextArticleIncludesReadAndUnselectedEntriesInDisplayedOrder)
{
    expect_next_article(1, 2);
    expect_next_article(3, 3);
}

TEST_P(SortedExplicitForwardArticleNavigationTest, nextArticleStopsAfterTheSortedEnd)
{
    g_art = ArticleNum{3};
    g_artp = article_ptr(g_art);
    g_art_ptr = article_ptr_list_begin() + 3;

    expect_next_article(5, 0);
}

INSTANTIATE_TEST_SUITE_P(ReadingMode, SortedExplicitForwardArticleNavigationTest, testing::Bool());

TEST_P(SortedRestartArticleNavigationTest, nextArticleFollowsSortedOrderAtRestart)
{
    expect_next_article(GetParam() ? 1 : 3, GetParam() ? 1 : 0);
}

INSTANTIATE_TEST_SUITE_P(StartPosition, SortedRestartArticleNavigationTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info)
                         { return info.param ? "AfterRestart" : "AtGroupEnd"; });

TEST_F(SortedArticleNavigationTest, nextArticleRestartsTheSortedListEvenWhenAllArticlesAreRead)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    set_order({2, 3, 1});

    expect_next_article(2, 0);
}

TEST_F(SortedArticleNavigationTest, nextArticleRestartsInSortedOrderWhenTheFirstCachedArticleIsMissing)
{
    set_articles({AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 5);
    set_order({3, 2, 4});

    expect_next_article(3, 0);
}

TEST_P(SortedSingleEntryArticleNavigationTest, nextArticleHandlesTheSortedBoundary)
{
    expect_next_article(GetParam() ? 1 : 2, 0);
}

INSTANTIATE_TEST_SUITE_P(StartPosition, SortedSingleEntryArticleNavigationTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info)
                         { return info.param ? "AtGroupEnd" : "AtOnlyArticle"; });

TEST_P(BreadthFirstUnreadArticleTest, nextUnreadVisitsEligibleArticlesInLevelOrderAcrossThreads)
{
    for (const long number : GetParam().order)
    {
        SCOPED_TRACE(number);
        expect_next('n', number);
    }
}

INSTANTIATE_TEST_SUITE_P(Tree, BreadthFirstUnreadArticleTest, testing::ValuesIn(BREADTH_FIRST_UNREAD_CASES),
                         [](const testing::TestParamInfo<BreadthFirstUnreadCase> &info)
                         { return std::string{info.param.name}; });

TEST_P(BreadthFirstUnreadWrapArticleTest, nextUnreadWrapsFromTheLastThreadToTheFirstEligibleArticle)
{
    expect_next('n', GetParam().wrap_target);
}

INSTANTIATE_TEST_SUITE_P(Tree, BreadthFirstUnreadWrapArticleTest, testing::ValuesIn(BREADTH_FIRST_UNREAD_CASES),
                         [](const testing::TestParamInfo<BreadthFirstUnreadCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(BreadthFirstExplicitNextArticleTest, explicitNextIncludesReadAndUnselectedEntriesInLevelOrder)
{
    for (const long number : {2, 6, 7, 3, 5, 8, 10, 4, 9, 11, 12})
    {
        SCOPED_TRACE(number);
        expect_next('N', number);
    }
}

TEST_P(BreadthFirstExplicitNextBoundaryArticleTest, explicitNextHandlesTheGroupBoundary)
{
    expect_next('N', GetParam() ? 1 : 13);
}

INSTANTIATE_TEST_SUITE_P(StartPosition, BreadthFirstExplicitNextBoundaryArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info)
                         { return info.param ? "AtGroupEnd" : "AtLastArticle"; });

TEST_F(BreadthFirstArticleTest, missingParentLosesUnreadAndSelectionCountsOnceWhileItsChildrenRemainReachable)
{
    set_tree({AF_EXISTS, AF_EXISTS, AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS,
              AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS},
             7);

    expect_next('n', 5, {3});
    for (const long number : {8, 10, 4, 9})
    {
        SCOPED_TRACE(number);
        expect_next('n', number);
    }
    expect_next('n', 5, {3});
}

TEST_F(BreadthFirstArticleTest, missingRootAndChildAreSkippedWhenRestartingFromGroupEnd)
{
    set_tree({AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS},
             13);

    expect_next('n', 6, {1, 2});
    expect_next('n', 7);
    expect_next('n', 3);
}

TEST_F(BreadthFirstArticleTest, explicitNextSkipsMissingRootsAndParentsWithoutLosingCountsOrChildren)
{
    set_tree({AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS},
             13);

    expect_next('N', 6, {1, 2});
    expect_next('N', 7);
    expect_next('N', 5, {3});
    for (const long number : {8, 10, 4, 9, 11, 12, 13})
    {
        SCOPED_TRACE(number);
        expect_next('N', number);
    }
}

TEST_P(BreadthFirstSelectedNextArticleTest, nextUnreadFindsTheSelectedArticleInLevelOrder)
{
    expect_next('n', GetParam().target);
}

INSTANTIATE_TEST_SUITE_P(Position, BreadthFirstSelectedNextArticleTest,
                         testing::ValuesIn(BREADTH_FIRST_SELECTED_NEXT_CASES),
                         [](const testing::TestParamInfo<BreadthFirstSelectedNextCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(BreadthFirstArticleTest, selectedNextUnreadStaysAtGroupEndWhileUnselectedUnreadArticlesRemain)
{
    set_tree({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD},
             7);
    g_selected_only = true;

    expect_next('n', 13);
    expect_next('n', 13);
}

TEST_F(BreadthFirstArticleTest, depthFirstStepTracksDepthAcrossChildrenSiblingsAndRoots)
{
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS},
             1);
    article_ptr(ArticleNum{1})->m_sibling = article_ptr(ArticleNum{11});
    Article *cursor = article_ptr(ArticleNum{1});
    int      depth = 0;

    for (const auto &[number, expected_depth] : {std::pair{2L, 1},
                                                 {3L, 2},
                                                 {4L, 3},
                                                 {5L, 2},
                                                 {6L, 1},
                                                 {7L, 1},
                                                 {8L, 2},
                                                 {9L, 3},
                                                 {10L, 2},
                                                 {11L, 0},
                                                 {12L, 1}})
    {
        SCOPED_TRACE(number);
        ASSERT_NE(nullptr, cursor);
        cursor = cursor->bump_article(depth);
        EXPECT_EQ(article_ptr(ArticleNum{number}), cursor);
        EXPECT_EQ(expected_depth, depth);
    }
    ASSERT_NE(nullptr, cursor);
    EXPECT_EQ(nullptr, cursor->bump_article(depth));
    EXPECT_EQ(-1, depth);
}

TEST_P(DepthFirstSkipArticleTest, skipsDescendantsAndTracksTheTargetDepth)
{
    m_cursor = m_cursor->bump_article(m_depth, false);

    EXPECT_EQ(article_ptr(ArticleNum{GetParam().target}), m_cursor);
    EXPECT_EQ(GetParam().target_depth, m_depth);
}

INSTANTIATE_TEST_SUITE_P(Position, DepthFirstSkipArticleTest, testing::ValuesIn(DEPTH_FIRST_SKIP_CASES),
                         [](const testing::TestParamInfo<DepthFirstSkipCase> &info)
                         { return std::string{info.param.name}; });

TEST_P(BreadthFirstNextOnLevelArticleTest, returnsTheNextArticleAtTheRequestedLevel)
{
    Article *const next = m_cursor->next_on_level(GetParam().level);

    Article *const expected = GetParam().target ? article_ptr(ArticleNum{GetParam().target}) : nullptr;
    EXPECT_EQ(expected, next);
}

INSTANTIATE_TEST_SUITE_P(Query, BreadthFirstNextOnLevelArticleTest, testing::ValuesIn(NEXT_ON_LEVEL_CASES),
                         [](const testing::TestParamInfo<NextOnLevelCase> &info)
                         { return std::string{info.param.name}; });

TEST_P(BreadthFirstLastOnLevelArticleTest, returnsTheLastArticleBeforeTheRequestedBoundary)
{
    Article *const last = m_root->last_on_level(GetParam().level, m_before);

    Article *const expected = GetParam().target ? article_ptr(ArticleNum{GetParam().target}) : nullptr;
    EXPECT_EQ(expected, last);
}

INSTANTIATE_TEST_SUITE_P(Query, BreadthFirstLastOnLevelArticleTest, testing::ValuesIn(LAST_ON_LEVEL_CASES),
                         [](const testing::TestParamInfo<LastOnLevelCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(BreadthFirstArticleTest, explicitPreviousIncludesReadAndUnselectedEntriesInReverseLevelOrder)
{
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_SEL,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
              AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             9);
    g_selected_only = true;
    g_search_ahead = ArticleNum{2};

    for (const long number : {4, 10, 8, 5, 3, 7, 6, 2, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('P', number).empty());
    }
}

TEST_F(BreadthFirstArticleTest, explicitPreviousFromGroupEndEntersTheDeepestLastLevelOfEachThread)
{
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS},
             13);

    for (const long number : {12, 11, 9})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('P', number).empty());
    }
}

TEST_F(BreadthFirstArticleTest, previousUnreadSkipsReadParentsAndCrossesThreadsInReverseLevelOrder)
{
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             13);

    for (const long number : {12, 9, 4, 10, 8, 3, 7, 6, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('p', number).empty());
    }
}

TEST_F(BreadthFirstArticleTest, selectedPreviousUnreadSkipsUnselectedAndSelectedReadArticles)
{
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             13);
    g_selected_only = true;

    for (const long number : {12, 4, 10, 8, 3, 7, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('p', number).empty());
    }
}

TEST_P(BreadthFirstPreviousBoundaryArticleTest, previousAtTheFirstArticleWarnsAndPreservesPosition)
{
    const std::string output = expect_previous(GetParam(), 1, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find(GetParam() == 'p' ? "No previous unread selected articles"
                                                               : "No previous selected articles"));
    EXPECT_EQ(m_line + 2, g_term_line);
}

INSTANTIATE_TEST_SUITE_P(Command, BreadthFirstPreviousBoundaryArticleTest, testing::Values('p', 'P'),
                         [](const testing::TestParamInfo<char> &info) { return info.param == 'p' ? "Unread" : "All"; });

TEST_F(BreadthFirstArticleTest, previousUnreadPreservesTheFirstEligiblePositionDespiteLaterUnreadArticles)
{
    set_tree({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
              AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
              AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL},
             8);
    g_selected_only = true;
    g_search_ahead = ArticleNum{2};

    const std::string output = expect_previous('p', 8, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread selected articles"));
}

TEST_F(BreadthFirstArticleTest, selectedPreviousUnreadPreservesGroupEndWhenOnlyUnselectedUnreadArticlesRemain)
{
    set_tree({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD},
             13);
    g_selected_only = true;

    const std::string output = expect_previous('p', 13, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread selected articles"));
}

TEST_F(BreadthFirstArticleTest, previousSkipsMissingTailParentAndRootAndAdjustsCountsOnlyOnce)
{
    set_tree({AF_UNREAD | AF_SEL, AF_EXISTS, AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS},
             11);

    EXPECT_TRUE(expect_previous('P', 4, {9}).empty());
    for (const long number : {10, 8, 5})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('P', number).empty());
    }
    EXPECT_TRUE(expect_previous('P', 7, {3}).empty());
    EXPECT_TRUE(expect_previous('P', 6).empty());
    EXPECT_TRUE(expect_previous('P', 2).empty());

    const std::string output = expect_previous('P', 11, {1}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous articles"));
    EXPECT_TRUE(expect_previous('P', 4, {9}).empty());
}

TEST_F(BreadthFirstArticleTest, previousTraversesMultipleRootsAtEachLevelBeforeReturningToThePreviousLevel)
{
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS},
             13);
    article_ptr(ArticleNum{1})->m_sibling = article_ptr(ArticleNum{11});
    m_subjects[1].m_thread = m_subjects[0].m_thread;
    m_subjects[0].m_thread_link = &m_subjects[1];
    m_subjects[1].m_thread_link = &m_subjects[0];

    for (const long number : {9, 4, 10, 8, 5, 3, 12, 7, 6, 2, 11, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('P', number).empty());
    }
}

TEST_P(BreadthFirstAllReadBoundaryArticleTest, nextCommandHandlesTheAllReadGroupBoundary)
{
    expect_next(GetParam().command, GetParam().target, {}, GetParam().result);
}

INSTANTIATE_TEST_SUITE_P(Position, BreadthFirstAllReadBoundaryArticleTest,
                         testing::ValuesIn(BREADTH_FIRST_ALL_READ_BOUNDARY_CASES),
                         [](const testing::TestParamInfo<BreadthFirstAllReadBoundaryCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(DepthFirstArticleTest, previousAllCrossesBranchesAndReadThreadsBeforeTheFirstUnreadArticle)
{
    set_tree({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
              AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             13);
    g_first_art = ArticleNum{12};
    g_selected_only = true;
    g_search_ahead = ArticleNum{2};

    for (const long number : {12, 11, 6, 4, 5, 3, 2, 10, 9, 8, 7, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('P', number).empty());
    }
}

TEST_F(DepthFirstArticleTest, previousUnreadSkipsReadParentsWhileIncludingUnselectedUnreadArticles)
{
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
              AF_EXISTS | AF_UNREAD | AF_SEL},
             13);

    for (const long number : {12, 4, 3, 9, 8, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('p', number).empty());
    }
}

TEST_F(DepthFirstArticleTest, selectedPreviousUnreadSkipsUnselectedUnreadAndSelectedReadArticles)
{
    set_tree({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             13);
    g_selected_only = true;
    g_selected_count = ArticleUnread{5};

    for (const long number : {12, 4, 8, 7, 1})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_previous('p', number).empty());
    }
}

TEST_P(DepthFirstMissingArticleTest, previousSkipsMissingDescendantsParentsAndRootAndAdjustsCountsOnlyOnce)
{
    set_tree({AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
              AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_UNREAD | AF_SEL,
              AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
             6);
    g_selected_only = true;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        g_art = g_curr_art = ArticleNum{6};
        g_artp = g_curr_artp = article_ptr(g_art);
        EXPECT_TRUE(expect_previous(GetParam(), 3, {4, 5}).empty());
        EXPECT_TRUE(expect_previous(GetParam(), 9, {2, 10}).empty());
        if (GetParam() == 'P')
        {
            EXPECT_TRUE(expect_previous('P', 7, {8}).empty());
        }
        g_curr_art = g_art;
        g_curr_artp = g_artp;
        g_search_ahead = ArticleNum{2};

        const std::string output = expect_previous(
            GetParam(), GetParam() == 'P' ? 7 : 9,
            GetParam() == 'P' ? std::initializer_list<long>{1} : std::initializer_list<long>{8, 1}, AS_ASK);

        EXPECT_NE(std::string::npos, output.find(GetParam() == 'P' ? "No previous selected articles"
                                                                   : "No previous unread selected articles"));
    }
}

INSTANTIATE_TEST_SUITE_P(PreviousCommand, DepthFirstMissingArticleTest, testing::Values('p', 'P'),
                         [](const testing::TestParamInfo<char> &info) { return std::string{info.param}; });

TEST_P(DepthFirstPreviousBoundaryArticleTest, firstEligibleBranchPreservesPositionWithoutWrappingToLaterSelections)
{
    const std::string output = expect_previous('p', 3, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find(GetParam() ? "There are no unread selected articles prior to this one."
                                                        : "No previous unread selected articles"));
    EXPECT_EQ(m_line + 2, g_term_line);
}

INSTANTIATE_TEST_SUITE_P(Verbosity, DepthFirstPreviousBoundaryArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Verbose" : "Terse"; });

TEST_F(DepthFirstArticleTest, previousUnreadPreservesGroupEndWhenNoUnreadArticlesAreSelected)
{
    set_tree({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD,
              AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD},
             13);
    g_selected_only = true;

    const std::string output = expect_previous('p', 13, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread selected articles"));
}

TEST_F(SubjectOrderPreviousArticleTest, previousAllFollowsSubjectOrderIncludingReadUnselectedSubjects)
{
    set_subjects({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL},
                 13);
    g_first_art = ArticleNum{12};
    g_selected_only = true;

    for (const long number : {9, 6, 4, 2, 11, 8, 3, 1, 12, 10, 7, 5})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_subject_previous('P', number).empty());
    }
}

TEST_F(SubjectOrderPreviousArticleTest, previousUnreadSkipsReadEntriesAndIncludesUnselectedUnreadArticles)
{
    set_subjects({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS},
                 13);

    for (const long number : {9, 6, 2, 8, 1, 10, 5})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_subject_previous('p', number).empty());
    }
}

TEST_F(SubjectOrderPreviousArticleTest, selectedPreviousUnreadSkipsReadAndUnselectedEntries)
{
    set_subjects({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                  AF_EXISTS},
                 13);
    g_selected_only = true;

    for (const long number : {6, 8, 10, 5})
    {
        SCOPED_TRACE(number);
        EXPECT_TRUE(expect_subject_previous('p', number).empty());
    }
}

TEST_F(SubjectOrderPreviousArticleTest, previousUnreadSkipsAnEntireReadSubjectWhenCrossingSubjects)
{
    set_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS},
                 2);

    EXPECT_TRUE(expect_subject_previous('p', 10).empty());
}

TEST_P(SubjectOrderPreviousCommandArticleTest, previousSkipsMissingSubjectTailsAndEntriesWithoutDoubleCounting)
{
    set_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                  AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL},
                 13);
    g_selected_only = true;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        g_art = g_curr_art = ArticleNum{13};
        g_artp = g_curr_artp = nullptr;
        EXPECT_TRUE(expect_subject_previous(GetParam(), 6, {9}).empty());
        EXPECT_TRUE(expect_subject_previous(GetParam(), 2, {4}).empty());
        EXPECT_TRUE(expect_subject_previous(GetParam(), 8, {11}).empty());
        EXPECT_TRUE(expect_subject_previous(GetParam(), 1, {3}).empty());
        EXPECT_TRUE(expect_subject_previous(GetParam(), 10, {12}).empty());
        EXPECT_TRUE(expect_subject_previous(GetParam(), 5, {7}).empty());
    }
}

TEST_P(SubjectOrderPreviousCommandArticleTest, missingEarlierSubjectsPreserveTheFirstBoundaryWithoutDoubleCounting)
{
    set_subjects({AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                  AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL},
                 2);
    g_selected_only = true;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        const std::string output = expect_subject_previous(GetParam(), 2, {11, 8, 3, 1, 12, 10, 7, 5}, AS_ASK);

        EXPECT_NE(std::string::npos, output.find(GetParam() == 'p' ? "No previous unread selected articles"
                                                                   : "No previous selected articles"));
    }
}

TEST_P(SubjectOrderPreviousBoundaryArticleTest, firstSubjectBoundaryPreservesPositionWithoutWrapping)
{
    const std::string output = expect_previous(std::get<0>(GetParam()), 5, {}, AS_ASK);

    const std::string qualifier = std::get<0>(GetParam()) == 'p' ? " unread" : "";
    EXPECT_NE(std::string::npos,
              output.find(std::get<1>(GetParam()) ? "There are no" + qualifier + " selected articles prior to this one."
                                                  : "No previous" + qualifier + " selected articles"));
    EXPECT_EQ(m_line + 2, g_term_line);
}

INSTANTIATE_TEST_SUITE_P(CommandAndVerbosity, SubjectOrderPreviousBoundaryArticleTest,
                         testing::Combine(testing::Values('p', 'P'), testing::Bool()),
                         [](const testing::TestParamInfo<std::tuple<char, bool>> &info)
                         {
                             return std::string{std::get<0>(info.param)} +
                                    (std::get<1>(info.param) ? "Verbose" : "Terse");
                         });

TEST_P(SubjectOrderPreviousCommandArticleTest, successfulPreviousCancelsSearchAheadBeforeTheNextNumericStep)
{
    set_subjects({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                 1);

    EXPECT_TRUE(expect_subject_previous(GetParam(), 12).empty());
    EXPECT_TRUE(expect_previous(GetParam(), 11).empty());
}

INSTANTIATE_TEST_SUITE_P(PreviousCommand, SubjectOrderPreviousCommandArticleTest, testing::Values('p', 'P'),
                         [](const testing::TestParamInfo<char> &info) { return std::string{info.param}; });

TEST_F(SubjectOrderPreviousArticleTest, selectedPreviousUnreadPreservesGroupEndWhenNoUnreadArticlesAreSelected)
{
    set_subjects({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS},
                 13);
    g_selected_only = true;

    const std::string output = expect_subject_previous('p', 13, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread selected articles"));
}

TEST_F(SubjectOrderPreviousArticleTest, previousUnreadPreservesGroupEndWhenAllSubjectsAreRead)
{
    set_subjects({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                  AF_EXISTS, AF_EXISTS, AF_EXISTS},
                 13);

    const std::string output = expect_subject_previous('p', 13, {}, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread articles"));
}

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

TEST_P(SortedPreviousArticleNavigationTest, previousUnreadFollowsDisplayedOrder)
{
    EXPECT_TRUE(expect_previous('p', 1, 2).empty());
    EXPECT_TRUE(expect_previous('p', 4, 1).empty());
    EXPECT_TRUE(expect_previous('p', 2, 0).empty());
}

INSTANTIATE_TEST_SUITE_P(ReadingMode, SortedPreviousArticleNavigationTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info)
                         { return info.param ? "Threaded" : "Unthreaded"; });

TEST_P(SortedPreviousCursorRepairArticleNavigationTest, previousUnreadRepairsTheSortedCursorAfterAnArticleJump)
{
    EXPECT_TRUE(expect_previous('p', 1, 2).empty());
}

INSTANTIATE_TEST_SUITE_P(ReadingMode, SortedPreviousCursorRepairArticleNavigationTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info)
                         { return info.param ? "Threaded" : "Unthreaded"; });

TEST_F(SortedArticleNavigationTest, previousUnreadSkipsReadAndUnselectedEntries)
{
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL},
                 4);
    set_order({5, 1, 2, 3, 4});
    g_selected_only = true;

    EXPECT_TRUE(expect_previous('p', 1, 1).empty());
}

TEST_F(SortedArticleNavigationTest, previousUnreadFromGroupEndFindsTheLastEligibleSortedEntry)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 5);
    set_order({2, 1, 4, 3});
    g_selected_only = true;
    g_art_ptr = article_ptr_list_begin() + 2;

    EXPECT_TRUE(expect_previous('p', 2, 0).empty());
}

TEST_F(SortedArticleNavigationTest, previousAllIncludesReadAndUnselectedEntriesBeforeFirstUnread)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL}, 3);
    set_order({2, 4, 1, 3});
    g_selected_only = true;
    g_first_art = ArticleNum{2};
    g_search_ahead = ArticleNum{2};

    EXPECT_TRUE(expect_previous('P', 1, 2).empty());
    EXPECT_TRUE(expect_previous('P', 4, 1).empty());
    EXPECT_TRUE(expect_previous('P', 2, 0).empty());
}

TEST_F(SortedArticleNavigationTest, previousAllFromGroupEndUsesTheLastSortedReadEntry)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 5);
    set_order({3, 4, 1, 2});
    g_first_art = ArticleNum{5};

    EXPECT_TRUE(expect_previous('P', 2, 3).empty());
}

TEST_P(SortedPreviousBoundaryArticleNavigationTest, previousAtTheFirstSortedEntryPreservesPositionAndWarns)
{
    const std::string output = expect_previous(GetParam(), 2, 0, AS_ASK);

    EXPECT_NE(std::string::npos, output.find(GetParam() == 'p' ? "No previous unread selected articles"
                                                               : "No previous selected articles"));
    EXPECT_EQ(m_line + 2, g_term_line);
}

INSTANTIATE_TEST_SUITE_P(Command, SortedPreviousBoundaryArticleNavigationTest, testing::Values('p', 'P'),
                         [](const testing::TestParamInfo<char> &info) { return info.param == 'p' ? "Unread" : "All"; });

TEST_F(SortedArticleNavigationTest, previousUnreadPreservesTheFirstEligiblePositionWhenThePrefixIsRead)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD}, 2);
    set_order({1, 2, 3});

    const std::string output = expect_previous('p', 2, 1, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread articles"));
}

TEST_F(SortedArticleNavigationTest, previousUnreadPreservesTheSelectionBoundaryWhenThePrefixIsUnselected)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL}, 2);
    set_order({1, 2, 3});
    g_selected_only = true;

    const std::string output = expect_previous('p', 2, 1, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread selected articles"));
}

TEST_F(SortedArticleNavigationTest, previousUnreadFromAnAllReadGroupPreservesTheEndPosition)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    set_order({2, 1, 3});

    const std::string output = expect_previous('p', 4, 3, AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No previous unread articles"));
}

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

    EXPECT_EQ(NG_MINUS, g_newsgroup_exit_code);
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

TEST_P(SelectedPreviousSubjectThreadArticleTest, findsSelectedUnreadTargetPastReadAndUnselectedArticles)
{
    set_selected_subjects({AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                          12);

    expect_previous(GetParam() ? 6 : 10);
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, threadTargetCanBelongToAnotherSubjectThanItsRoot)
{
    set_selected_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                          12);

    expect_previous(GetParam() ? 8 : 9);
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, skipsUnvisitedSubjectsAndThreadsWithUnselectedUnreadArticles)
{
    set_selected_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                          9);

    expect_previous(GetParam() ? 1 : 4);
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, skipsMissingScopesAndClearsTheirCountsOnlyOnceWithoutRereading)
{
    set_selected_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS},
                          9);

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        g_art = g_curr_art;
        g_artp = g_curr_artp;
        expect_previous(GetParam() ? 2 : 4, {5, 6, 7, 8});
    }
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, skipsMissingParentsAndSelectedReadArticlesBeforeTheUnreadTarget)
{
    set_selected_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_SEL, AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS},
                          9);

    expect_previous(8, GetParam() ? std::initializer_list<long>{5, 7} : std::initializer_list<long>{7});
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, choosesTheFirstSelectedUnreadArticleInTheConfiguredReadingOrder)
{
    set_selected_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                          9);
    // Thread 5 has children 6 and 7; article 8 is a child of 6.
    article_ptr(ArticleNum{6})->m_child1 = article_ptr(ArticleNum{8});
    article_ptr(ArticleNum{6})->m_sibling = article_ptr(ArticleNum{7});
    article_ptr(ArticleNum{7})->m_parent = article_ptr(ArticleNum{5});
    article_ptr(ArticleNum{7})->m_child1 = nullptr;
    article_ptr(ArticleNum{8})->m_parent = article_ptr(ArticleNum{6});

    for (const bool breadth_first : {false, true})
    {
        SCOPED_TRACE(breadth_first);
        g_breadth_first = breadth_first;
        g_art = g_curr_art;
        g_artp = g_curr_artp;

        expect_previous(GetParam() && !breadth_first ? 8 : 7);
    }
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, entersFromGroupEndAtTheLastEligibleSubjectOrThread)
{
    set_selected_subjects({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                           AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                          13);

    expect_previous(6);
}

TEST_P(SelectedPreviousSubjectThreadArticleTest, stopsAtGroupEndWhenEarlierScopesHaveNoSelectedUnreadArticles)
{
    set_selected_subjects({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                           AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                          9);

    expect_previous(13, {}, true);
}

TEST_P(SubjectFallbackArticleTest, prefersSelectedReadArticlesInTheConfiguredReadingOrder)
{
    set_fallback_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                           AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    // Thread 5 has children 6 and 7; article 8 is a child of 6.
    article_ptr(ArticleNum{6})->m_child1 = article_ptr(ArticleNum{8});
    article_ptr(ArticleNum{6})->m_sibling = article_ptr(ArticleNum{7});
    article_ptr(ArticleNum{7})->m_parent = article_ptr(ArticleNum{5});
    article_ptr(ArticleNum{7})->m_child1 = nullptr;
    article_ptr(ArticleNum{8})->m_parent = article_ptr(ArticleNum{6});

    for (const bool breadth_first : {false, true})
    {
        SCOPED_TRACE(breadth_first);
        g_breadth_first = breadth_first;
        for (const char command : {'>', '<'})
        {
            SCOPED_TRACE(command);
            const long target = GetParam() ? (breadth_first ? 7 : 8) : (command == '>' ? 4 : 7);

            expect_from(command, command == '>' ? 1 : 9, target, true);
        }
    }
}

TEST_P(SubjectFallbackArticleTest, usesSelectorScopeAndRestoresTheDifferentIncomingThreadingMode)
{
    set_fallback_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS,
                           AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
    g_threaded_group = !GetParam();

    for (const char command : {'>', '<'})
    {
        SCOPED_TRACE(command);
        expect_from(command, command == '>' ? 1 : 9, GetParam() ? 8 : (command == '>' ? 4 : 8), true);
    }
}

TEST_P(SubjectFallbackArticleTest, fallsBackToUnselectedUnreadArticleWhenTheScopeHasNoSelections)
{
    set_fallback_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});

    for (const char command : {'>', '<'})
    {
        SCOPED_TRACE(command);
        expect_from(command, command == '>' ? 1 : 9, GetParam() ? 5 : (command == '>' ? 3 : 7), true);
    }
}

TEST_P(SubjectFallbackArticleTest, allReadScopeStartsAtTheFirstArticleWhenSelectedOnlyModeIsOff)
{
    set_fallback_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS,
                           AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                          false);

    for (const char command : {'>', '<'})
    {
        SCOPED_TRACE(command);
        expect_from(command, command == '>' ? 1 : 9, GetParam() ? 5 : (command == '>' ? 3 : 7), true);
    }
}

TEST_P(SubjectFallbackArticleTest, skipsMissingParentsBeforeFallingBackToASelectedReadArticle)
{
    set_fallback_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL,
                           AF_UNREAD | AF_SEL, AF_EXISTS, AF_UNREAD | AF_SEL, AF_EXISTS | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});

    for (const char command : {'>', '<'})
    {
        SCOPED_TRACE(command);
        const auto missing = GetParam()
                                 ? std::initializer_list<long>{5, 7}
                                 : (command == '>' ? std::initializer_list<long>{3} : std::initializer_list<long>{7});

        expect_from(command, command == '>' ? 1 : 9, GetParam() ? 8 : (command == '>' ? 4 : 8), true, missing);
    }
}

TEST_P(SubjectFallbackArticleTest, nextClearsRereadAfterSkippingMissingScopesBeforeAnUnreadTarget)
{
    set_fallback_subjects({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                           AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});

    expect_from('>', 1, 9, false,
                GetParam() ? std::initializer_list<long>{5, 6, 7, 8} : std::initializer_list<long>{3, 4, 5, 6, 7, 8});
}

TEST_P(SubjectFallbackArticleTest, exhaustingMissingScopesClearsRereadAndRequestsTheEndOfGroup)
{
    set_fallback_subjects({AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL,
                           AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL,
                           AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL, AF_UNREAD | AF_SEL});

    for (const char command : {'>', '<'})
    {
        SCOPED_TRACE(command);
        expect_from(command, command == '>' ? 8 : 5, 13, false,
                    command == '>' ? std::initializer_list<long>{9, 10, 11, 12}
                                   : std::initializer_list<long>{1, 2, 3, 4},
                    true);
    }
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, SubjectFallbackArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Thread" : "Subject"; });

INSTANTIATE_TEST_SUITE_P(ArticleOrder, SelectedPreviousSubjectThreadArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Thread" : "Subject"; });

INSTANTIATE_TEST_SUITE_P(ArticleOrder, SubjectThreadArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Thread" : "Subject"; });

TEST_P(ParentRootArticleTest, returnsToReadUnselectedAncestorAndPreservesHistory)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL}, 4);
    g_threaded_group = true;
    g_selected_only = true;
    g_recent_art = ArticleNum{2};
    g_recent_artp = article_ptr(g_recent_art);
    g_search_ahead = ArticleNum{2};
    const ArticleNum expected{GetParam() ? 1L : 3L};

    EXPECT_EQ(AS_NORM, art_switch(GetParam() ? "{" : "["));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
    EXPECT_EQ(ArticleNum{4}, g_curr_art);
    ASSERT_EQ(article_ptr(ArticleNum{4}), g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_curr_artp->m_flags);
    EXPECT_EQ(ArticleNum{2}, g_recent_art);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_recent_artp);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(ArticleNum{2}, g_search_ahead);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_force_last);
}

TEST_P(ParentRootArticleTest, preservesPositionWithoutAncestorWithTerseWarning)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 1);
    g_threaded_group = true;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(GetParam() ? "{" : "[");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find(GetParam() ? "No prior root." : "No prior parent."));
    EXPECT_EQ(ArticleNum{1}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_P(ParentRootArticleTest, preservesPositionWithoutAncestorWithVerboseWarning)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 1);
    g_threaded_group = true;
    g_verbose = true;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(GetParam() ? "{" : "[");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find(GetParam() ? "There is no root article prior to this one."
                                                        : "There is no parent article prior to this one."));
    EXPECT_EQ(ArticleNum{1}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{1}), g_artp);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_P(ParentRootArticleTest, preservesEndOfGroupWithoutCurrentArticle)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 3);
    g_threaded_group = true;
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? "{" : "[");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(verbose ? "You're at the end of the group." : "End of group."));
        EXPECT_EQ(ArticleNum{3}, g_art);
        EXPECT_EQ(nullptr, g_artp);
        EXPECT_EQ(ArticleNum{3}, g_curr_art);
        EXPECT_EQ(nullptr, g_curr_artp);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

TEST_P(ParentRootArticleTest, rejectsUnthreadedGroupWithoutMoving)
{
    set_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 3);
    g_threaded_group = false;
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? "{" : "[");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(verbose ? "This group is not threaded." : "Unthreaded group."));
        EXPECT_EQ(ArticleNum{3}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{3}), g_artp);
        EXPECT_EQ(ArticleNum{3}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, ParentRootArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Root" : "Parent"; });

TEST_P(ChildLeafArticleTest, followsFirstChildBranchAndPreservesHistoryAndSelection)
{
    set_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD}, 1);
    article_ptr(ArticleNum{2})->m_sibling = article_ptr(ArticleNum{5});
    article_ptr(ArticleNum{4})->m_child1 = nullptr;
    article_ptr(ArticleNum{5})->m_parent = article_ptr(ArticleNum{1});
    g_threaded_group = true;
    g_selected_only = true;
    g_recent_art = ArticleNum{3};
    g_recent_artp = article_ptr(g_recent_art);
    g_search_ahead = ArticleNum{3};
    const ArticleNum expected{GetParam() ? 4L : 2L};

    EXPECT_EQ(AS_NORM, art_switch(GetParam() ? "}" : "]"));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
    EXPECT_EQ(ArticleNum{1}, g_curr_art);
    ASSERT_EQ(article_ptr(ArticleNum{1}), g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_curr_artp->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{5})->m_flags);
    EXPECT_EQ(ArticleNum{3}, g_recent_art);
    EXPECT_EQ(article_ptr(ArticleNum{3}), g_recent_artp);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_force_last);
}

TEST_P(ChildLeafArticleTest, preservesPositionWithoutDescendantsWithTerseWarning)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD}, 2);
    g_threaded_group = true;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(GetParam() ? "}" : "]");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find("Last leaf."));
    EXPECT_EQ(ArticleNum{2}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(ArticleNum{2}, g_curr_art);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_P(ChildLeafArticleTest, preservesPositionWithoutDescendantsWithVerboseWarning)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD}, 2);
    g_threaded_group = true;
    g_verbose = true;
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(GetParam() ? "}" : "]");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_NE(std::string::npos, output.find("This is the last leaf in this tree."));
    EXPECT_EQ(ArticleNum{2}, g_art);
    ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
    EXPECT_EQ(ArticleNum{2}, g_curr_art);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(2, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

TEST_P(ChildLeafArticleTest, preservesEndOfGroupWithoutCurrentArticle)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 3);
    g_threaded_group = true;
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? "}" : "]");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(verbose ? "You're at the end of the group." : "End of group."));
        EXPECT_EQ(ArticleNum{3}, g_art);
        EXPECT_EQ(nullptr, g_artp);
        EXPECT_EQ(ArticleNum{3}, g_curr_art);
        EXPECT_EQ(nullptr, g_curr_artp);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

TEST_P(ChildLeafArticleTest, rejectsUnthreadedGroupWithoutMoving)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS}, 1);
    g_threaded_group = false;
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? "}" : "]");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(verbose ? "This group is not threaded." : "Unthreaded group."));
        EXPECT_EQ(ArticleNum{1}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{1}), g_artp);
        EXPECT_EQ(ArticleNum{1}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, ChildLeafArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Leaf" : "Child"; });

TEST_P(SiblingArticleTest, visitsReadUnselectedSiblingAndPreservesHistoryAndSelection)
{
    set_siblings(6);
    g_selected_only = true;
    g_recent_art = ArticleNum{4};
    g_recent_artp = article_ptr(g_recent_art);
    g_search_ahead = ArticleNum{4};
    const ArticleNum expected{GetParam() ? 7L : 2L};

    EXPECT_EQ(AS_NORM, art_switch(GetParam() ? ")" : "("));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
    EXPECT_EQ(ArticleNum{6}, g_curr_art);
    ASSERT_EQ(article_ptr(ArticleNum{6}), g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_curr_artp->m_flags);
    EXPECT_EQ(ArticleNum{4}, g_recent_art);
    EXPECT_EQ(article_ptr(ArticleNum{4}), g_recent_artp);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_EQ(ArticleNum{4}, g_search_ahead);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_force_last);
}

TEST_P(SiblingArticleTest, visitsCousinAtSameDepthAcrossShallowBranch)
{
    const ArticleNum current{GetParam() ? 5L : 8L};
    set_siblings(current.value_of());
    g_selected_only = true;
    const ArticleNum expected{GetParam() ? 8L : 5L};

    EXPECT_EQ(AS_NORM, art_switch(GetParam() ? ")" : "("));

    EXPECT_EQ(expected, g_art);
    ASSERT_EQ(article_ptr(expected), g_artp);
    EXPECT_EQ(GetParam() ? AF_EXISTS : AF_EXISTS | AF_UNREAD | AF_SEL, g_artp->m_flags);
    EXPECT_EQ(current, g_curr_art);
    EXPECT_EQ(article_ptr(current), g_curr_artp);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{9})->m_flags);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    EXPECT_TRUE(g_selected_only);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_force_last);
}

TEST_P(SiblingArticleTest, preservesPositionAtFirstOrLastSiblingWithWarning)
{
    const ArticleNum current{GetParam() ? 10L : 3L};
    set_siblings(current.value_of());
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? ")" : "(");

        const std::string output = testing::internal::GetCapturedStdout();
        const std::string expected =
            verbose ? (GetParam() ? "This article has no next sibling." : "This article has no previous sibling.")
                    : (GetParam() ? "No next sibling." : "No previous sibling.");
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(expected));
        EXPECT_EQ(current, g_art);
        EXPECT_EQ(article_ptr(current), g_artp);
        EXPECT_EQ(current, g_curr_art);
        EXPECT_EQ(article_ptr(current), g_curr_artp);
        EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{3}, g_selected_count);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

TEST_P(SiblingArticleTest, preservesEndOfGroupWithoutCurrentArticle)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS}, 3);
    g_threaded_group = true;
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? ")" : "(");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(verbose ? "You're at the end of the group." : "End of group."));
        EXPECT_EQ(ArticleNum{3}, g_art);
        EXPECT_EQ(nullptr, g_artp);
        EXPECT_EQ(ArticleNum{3}, g_curr_art);
        EXPECT_EQ(nullptr, g_curr_artp);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

TEST_P(SiblingArticleTest, rejectsUnthreadedGroupWithoutMoving)
{
    set_siblings(6);
    g_threaded_group = false;
    for (const bool verbose : {false, true})
    {
        SCOPED_TRACE(verbose);
        g_verbose = verbose;
        g_term_line = 0;
        g_term_col = 0;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(GetParam() ? ")" : "(");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find(verbose ? "This group is not threaded." : "Unthreaded group."));
        EXPECT_EQ(ArticleNum{6}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{6}), g_artp);
        EXPECT_EQ(ArticleNum{6}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_artp->m_flags);
        EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{3}, g_selected_count);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        EXPECT_EQ(2, g_term_line);
        EXPECT_EQ(0, g_term_col);
    }
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, SiblingArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Next" : "Previous"; });

TEST_P(ReadStateArticleTest, marksCurrentArticleReadAndRemovesSelectionOnlyOnce)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS}, 2);
    g_selected_only = true;
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_EQ(AS_ASK, art_switch("j"));

        EXPECT_EQ(ArticleNum{2}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
        EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(ArticleNum{1}, g_recent_art);
        EXPECT_EQ(article_ptr(ArticleNum{1}), g_recent_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
        EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{3})->m_flags);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(10 + attempt, g_check_count);
        EXPECT_EQ(0, g_dm_count);
        EXPECT_TRUE(g_selected_only);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
    }
}

TEST_P(ReadStateArticleTest, marksCurrentArticleUnreadAndRepairsFirstUnreadOnlyOnce)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD}, 2);
    g_first_art = ArticleNum{3};
    g_artp->m_subj->m_flags = SF_NONE;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch("m");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find("Article 2 marked as still unread."));
        EXPECT_EQ(ArticleNum{2}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_artp->m_flags);
        EXPECT_EQ(SF_VISIT, g_artp->m_subj->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{3})->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_first_art);
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(10, g_check_count);
        EXPECT_EQ(0, g_dm_count);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
    }
}

TEST_P(ReadStateArticleTest, selectsNewlyUnreadArticleInSelectedSubjectOnlyOnce)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL}, 2);
    g_selected_only = true;
    g_first_art = ArticleNum{3};
    g_artp->m_subj->m_flags = SF_SEL | SF_VISIT;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);

        EXPECT_EQ(AS_ASK, art_switch("m"));

        EXPECT_EQ(ArticleNum{2}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, g_artp->m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, g_artp->m_subj->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{3})->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_first_art);
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{2}, g_selected_count);
        EXPECT_EQ(10, g_check_count);
        EXPECT_TRUE(g_selected_only);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
    }
}

TEST_P(ReadStateArticleTest, markingUnreadInAnUnselectedSubjectPreservesSelectionsAndTheEarliestUnreadPosition)
{
    set_articles({AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL}, 2);
    Subject &unselected_subject = *g_artp->m_subj;
    Subject  selected_subject{};
    selected_subject.m_articles = article_ptr(ArticleNum{4});
    selected_subject.m_thread = selected_subject.m_articles;
    selected_subject.m_flags = SF_SEL | SF_VISIT;
    selected_subject.m_prev = &unselected_subject;
    unselected_subject.m_flags = SF_NONE;
    unselected_subject.m_next = &selected_subject;
    article_ptr(ArticleNum{3})->m_subj_next = nullptr;
    article_ptr(ArticleNum{3})->m_child1 = nullptr;
    selected_subject.m_articles->m_subj = &selected_subject;
    selected_subject.m_articles->m_parent = nullptr;
    g_last_subject = &selected_subject;
    g_selected_only = true;
    g_first_art = ArticleNum{4};
    g_recent_art = ArticleNum{1};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{4};
    g_search_ahead = ArticleNum{3};

    for (const long current : {2, 3})
    {
        SCOPED_TRACE(current);
        g_art = ArticleNum{current};
        g_artp = article_ptr(g_art);
        g_curr_art = g_art;
        g_curr_artp = g_artp;
        for (int attempt = 1; attempt <= 2; ++attempt)
        {
            SCOPED_TRACE(attempt);
            testing::internal::CaptureStdout();

            const ArticleSwitchResult result = art_switch("m");

            const std::string output = testing::internal::GetCapturedStdout();
            EXPECT_EQ(AS_ASK, result);
            EXPECT_NE(std::string::npos,
                      output.find("Article " + std::to_string(current) + " marked as still unread."));
            EXPECT_EQ(ArticleNum{current}, g_art);
            EXPECT_EQ(article_ptr(ArticleNum{current}), g_artp);
            EXPECT_EQ(g_art, g_curr_art);
            EXPECT_EQ(g_artp, g_curr_artp);
            EXPECT_EQ(ArticleNum{1}, g_recent_art);
            EXPECT_EQ(article_ptr(ArticleNum{1}), g_recent_artp);
            EXPECT_EQ(ArticleNum{4}, g_parsed_art);
            EXPECT_EQ(ArticleNum{3}, g_search_ahead);
            EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{1})->m_flags);
            EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{2})->m_flags);
            EXPECT_EQ(AF_EXISTS | (current == 3 ? AF_UNREAD : AF_DEL), article_ptr(ArticleNum{3})->m_flags);
            EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, selected_subject.m_articles->m_flags);
            EXPECT_EQ(SF_NONE, unselected_subject.m_flags);
            EXPECT_EQ(SF_SEL | SF_VISIT, selected_subject.m_flags);
            EXPECT_EQ(&unselected_subject, g_first_subject);
            EXPECT_EQ(&selected_subject, g_last_subject);
            EXPECT_EQ(ArticleNum{2}, g_first_art);
            EXPECT_EQ(ArticleUnread{current}, m_newsgroup.m_to_read);
            EXPECT_EQ(ArticleUnread{1}, g_selected_count);
            EXPECT_EQ(10, g_check_count);
            EXPECT_EQ(0, g_dm_count);
            EXPECT_TRUE(g_selected_only);
            EXPECT_EQ(GetParam(), g_threaded_group);
            EXPECT_EQ(SM_ARTICLE, g_sel_mode);
            EXPECT_FALSE(g_reread);
            EXPECT_FALSE(g_force_last);
        }
    }
}

TEST_P(ReadStateArticleTest, marksCurrentArticleForDelayedReturnOnlyOnce)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS}, 2);
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch("M");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find("Article 2 will return."));
        EXPECT_EQ(ArticleNum{2}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
        EXPECT_EQ(AF_EXISTS | AF_YANK_BACK, g_artp->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
        EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{3})->m_flags);
        EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(1, g_dm_count);
        EXPECT_EQ(10, g_check_count);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
    }
}

TEST_P(ReadStateArticleTest, delaysAnAlreadyReadArticleWithoutChangingUnreadOrSelectedCounts)
{
    set_articles({AF_EXISTS | AF_YANK_BACK, AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD},
                 2);
    g_selected_only = true;
    g_artp->m_subj->m_flags = SF_SEL | SF_VISIT;
    g_first_art = ArticleNum{3};
    g_recent_art = ArticleNum{4};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{3};
    g_search_ahead = ArticleNum{4};
    g_dm_count = 1;
    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch("M");

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_NE(std::string::npos, output.find("Article 2 will return."));
        EXPECT_EQ(ArticleNum{2}, g_art);
        ASSERT_EQ(article_ptr(ArticleNum{2}), g_artp);
        EXPECT_EQ(AF_EXISTS | AF_DEL | AF_YANK_BACK, g_artp->m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, g_artp->m_subj->m_flags);
        EXPECT_EQ(ArticleNum{2}, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(ArticleNum{4}, g_recent_art);
        EXPECT_EQ(article_ptr(ArticleNum{4}), g_recent_artp);
        EXPECT_EQ(ArticleNum{3}, g_parsed_art);
        EXPECT_EQ(ArticleNum{4}, g_search_ahead);
        EXPECT_EQ(AF_EXISTS | AF_YANK_BACK, article_ptr(ArticleNum{1})->m_flags);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{3})->m_flags);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{4})->m_flags);
        EXPECT_EQ(ArticleNum{3}, g_first_art);
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
        EXPECT_EQ(2, g_dm_count);
        EXPECT_EQ(10, g_check_count);
        EXPECT_TRUE(g_selected_only);
        EXPECT_EQ(GetParam(), g_threaded_group);
        EXPECT_EQ(SM_ARTICLE, g_sel_mode);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
    }
}

TEST_P(ReadStateArticleTest, ignoresEndOfGroupAndExpiredArticlePositions)
{
    set_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS}, 4);
    struct CommandCase
    {
        const char *command;
        long        current;
    };
    for (const CommandCase test_case :
         {CommandCase{"j", 4}, CommandCase{"m", 4}, CommandCase{"M", 4}, CommandCase{"j", 0}, CommandCase{"m", 0}})
    {
        SCOPED_TRACE(testing::Message() << test_case.command << " at " << test_case.current);
        g_art = ArticleNum{test_case.current};
        g_curr_art = g_art;
        testing::internal::CaptureStdout();

        const ArticleSwitchResult result = art_switch(test_case.command);

        const std::string output = testing::internal::GetCapturedStdout();
        EXPECT_EQ(AS_ASK, result);
        EXPECT_EQ(std::string::npos, output.find("Article"));
        EXPECT_EQ(ArticleNum{test_case.current}, g_art);
        EXPECT_EQ(nullptr, g_artp);
        EXPECT_EQ(g_art, g_curr_art);
        EXPECT_EQ(nullptr, g_curr_artp);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, article_ptr(ArticleNum{1})->m_flags);
        EXPECT_EQ(AF_EXISTS | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{2})->m_flags);
        EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{3})->m_flags);
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{1}, g_selected_count);
        EXPECT_EQ(10, g_check_count);
        EXPECT_EQ(0, g_dm_count);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
    }
}

INSTANTIATE_TEST_SUITE_P(ArticleOrder, ReadStateArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Threaded" : "Numeric"; });

TEST_F(UnreadScopeArticleTest, restoresWholeThreadAcrossSubjectsAndLeavesOtherThreadAlone)
{
    set_scope({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE, AF_EXISTS | AF_DEL,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_DEL},
              3);
    g_first_art = ArticleNum{2};

    mark_unread('t');

    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_DEL});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(SF_VISIT, m_subjects[0].m_flags);
    EXPECT_EQ(SF_VISIT, m_subjects[1].m_flags);
    EXPECT_EQ(SF_VISIT, m_subjects[2].m_flags);
}

TEST_F(UnreadScopeArticleTest, restoresAndSelectsWholeThreadWithoutDoubleCounting)
{
    set_scope({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS | AF_DEL,
               AF_EXISTS | AF_UNREAD, AF_EXISTS},
              3);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    m_subjects[0].m_flags |= SF_SEL | SF_DEL;
    m_subjects[1].m_flags |= SF_DEL;

    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        mark_unread('t');

        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_NONE, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{4}, g_selected_count);
        EXPECT_EQ(2, g_selected_subj_cnt);
        EXPECT_TRUE(g_selected_only);
        EXPECT_EQ(ArticleNum{1}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT | SF_WAS_SELECTED, m_subjects[1].m_flags);
        EXPECT_EQ(SF_VISIT, m_subjects[2].m_flags);
    }
}

TEST_F(UnreadScopeArticleTest, startsAtFirstExistingArticleWhenThreadRootIsMissing)
{
    set_scope({AF_NONE, AF_EXISTS, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 3);
    g_first_art = ArticleNum{8};

    mark_unread('t');

    expect_flags(
        {AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
}

TEST_F(UnreadScopeArticleTest, fallsBackToCurrentSubjectWithoutThreadInformation)
{
    set_scope({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 4);
    g_threaded_group = false;
    m_subjects[0].m_thread = nullptr;
    m_subjects[1].m_thread = nullptr;
    g_first_art = ArticleNum{8};

    mark_unread('t');

    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                  AF_EXISTS});
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{3}, g_first_art);
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(SF_NONE, m_subjects[0].m_flags);
    EXPECT_EQ(SF_VISIT, m_subjects[1].m_flags);
    EXPECT_EQ(SF_NONE, m_subjects[2].m_flags);
}

TEST_F(UnreadScopeArticleTest, restoresSubthreadAcrossSubjectsWithoutAncestorsOrSiblingBranch)
{
    set_scope({AF_EXISTS | AF_DEL, AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_DEL, AF_EXISTS | AF_DEL,
               AF_EXISTS | AF_UNREAD, AF_EXISTS},
              2);
    g_first_art = ArticleNum{3};

    mark_unread('s');

    expect_flags({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_FALSE(g_selected_only);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(ArticleNum{2}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
    EXPECT_EQ(SF_VISIT, m_subjects[1].m_flags);
    EXPECT_EQ(SF_VISIT, m_subjects[2].m_flags);
}

TEST_F(UnreadScopeArticleTest, selectsOnlyExistingUnreadArticlesWithinSubthread)
{
    set_scope({AF_EXISTS, AF_EXISTS | AF_DEL, AF_NONE, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD,
               AF_EXISTS},
              2);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    m_subjects[0].m_flags = SF_DEL;
    m_subjects[1].m_flags |= SF_SEL;

    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        mark_unread('s');

        expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                      AF_EXISTS | AF_UNREAD, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{2}, g_selected_count);
        EXPECT_EQ(2, g_selected_subj_cnt);
        EXPECT_TRUE(g_selected_only);
        EXPECT_EQ(ArticleNum{2}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[1].m_flags);
        EXPECT_EQ(SF_VISIT, m_subjects[2].m_flags);
    }
}

TEST_F(UnreadScopeArticleTest, makesEachRestoredSubjectVisibleInSelectedOnlyMode)
{
    set_scope(
        {AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS}, 2);
    g_selected_only = true;
    g_selected_subj_cnt = 1;
    m_subjects[1].m_flags = SF_DEL;
    m_subjects[2].m_flags |= SF_SEL;

    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        mark_unread('s');

        expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{4}, g_selected_count);
        EXPECT_EQ(3, g_selected_subj_cnt);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[1].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
        EXPECT_EQ(ArticleNum{2}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
    }
}

TEST_F(UnreadScopeArticleTest, leafSubthreadChangesOnlyTheLeafAndDoesNotDoubleCount)
{
    set_scope(
        {AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_DEL, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS},
        4);

    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        mark_unread('s');

        expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                      AF_EXISTS | AF_UNREAD, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(1, g_selected_subj_cnt);
        EXPECT_FALSE(g_selected_only);
        EXPECT_EQ(ArticleNum{4}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
    }
}

TEST_F(UnreadScopeArticleTest, rootSubthreadIncludesAllBranchesOfOnlyItsOwnThread)
{
    set_scope({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 1);

    mark_unread('s');

    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS | AF_UNREAD,
                  AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(SF_NONE, m_subjects[2].m_flags);
}

TEST_F(GroupUnreadArticleTest, restoresAllExistingArticlesAcrossThreadsWithoutDoubleCounting)
{
    set_scope({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE, AF_EXISTS | AF_YANK_BACK,
               AF_EXISTS | AF_UNREAD, AF_EXISTS},
              3);
    g_first_art = ArticleNum{2};
    g_dm_count = 1;
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);

    for (int attempt = 1; attempt <= 2; ++attempt)
    {
        SCOPED_TRACE(attempt);
        mark_unread('a');

        expect_flags({AF_EXISTS | AF_UNREAD | AF_DEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE,
                      AF_EXISTS | AF_UNREAD | AF_YANK_BACK, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD});
        EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleNum{6}, g_obj_count);
        EXPECT_EQ(1, g_dm_count);
        EXPECT_EQ(ArticleUnread{}, g_selected_count);
        EXPECT_EQ(0, g_selected_subj_cnt);
        EXPECT_FALSE(g_selected_only);
        EXPECT_EQ(ArticleNum{1}, g_first_art);
        EXPECT_EQ(ArticleNum{3}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
        EXPECT_EQ(g_art, g_curr_art);
        EXPECT_EQ(g_artp, g_curr_artp);
        EXPECT_EQ(ArticleNum{6}, g_recent_art);
        EXPECT_EQ(article_ptr(g_recent_art), g_recent_artp);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_force_last);
        for (const Subject &subject : m_subjects)
        {
            EXPECT_EQ(2, subject.m_misc);
            EXPECT_EQ(SF_VISIT, subject.m_flags);
        }
    }
}

TEST_F(GroupUnreadArticleTest, restoresWholeGroupInSelectedOnlyModeWithoutSelectingMoreArticles)
{
    set_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_DEL, AF_NONE, AF_EXISTS,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
              3);
    g_selected_only = true;
    g_selected_subj_cnt = 2;
    m_subjects[0].m_flags |= SF_SEL;
    m_subjects[2].m_flags |= SF_SEL;

    mark_unread('a');

    expect_flags({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_DEL, AF_NONE,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{6}, g_obj_count);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
    EXPECT_EQ(SF_NONE, m_subjects[1].m_flags);
    EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
}

TEST_F(GroupUnreadArticleTest, restoresGroupFromEndAfterShowingGroupHelp)
{
    set_scope({AF_NONE, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS}, 8);
    g_first_art = ArticleNum{3};
    g_verbose = true;
    push_char('a');

    const std::string output = mark_unread('h');

    EXPECT_NE(std::string::npos, output.find("Unkill: +select or all?"));
    EXPECT_NE(std::string::npos, output.find("Type a to mark all articles in this group as unread."));
    EXPECT_NE(std::string::npos, output.find("Type n or q to change nothing."));
    EXPECT_EQ(std::string::npos, output.find("Type t or SP"));
    EXPECT_EQ(std::string::npos, output.find("Type s to mark"));
    expect_flags({AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD});
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(nullptr, g_curr_artp);
    EXPECT_EQ("+anq", g_default_cmd);
}

TEST_F(GroupUnreadArticleTest, leavesEmptyGroupEmpty)
{
    set_articles({}, 1);

    mark_unread('a');

    EXPECT_TRUE(g_article_list.empty());
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(nullptr, g_curr_artp);
}

TEST_F(GroupUnreadArticleTest, cancellationPreservesArticleStateAndCountsWithOrWithoutCurrentArticle)
{
    set_scope({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS | AF_YANK_BACK,
               AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
              3);
    g_selected_only = true;
    g_selected_subj_cnt = 2;
    g_obj_count = ArticleNum{2};
    g_dm_count = 1;
    g_first_art = ArticleNum{2};
    g_recent_art = ArticleNum{6};
    g_recent_artp = article_ptr(g_recent_art);
    g_parsed_art = ArticleNum{3};
    g_reread = true;
    g_force_last = true;
    m_subjects[0].m_flags |= SF_SEL;
    m_subjects[2].m_flags |= SF_SEL;
    m_subjects[0].m_misc = 1;
    m_subjects[2].m_misc = 1;
    struct PromptCase
    {
        long current;
        char answer;
    };
    for (const PromptCase test_case : {PromptCase{3, 'n'}, PromptCase{3, 'q'}, PromptCase{8, 'n'}, PromptCase{8, 'q'}})
    {
        SCOPED_TRACE(testing::Message() << test_case.answer << " at " << test_case.current);
        g_art = ArticleNum{test_case.current};
        g_artp = test_case.current == 3 ? article_ptr(g_art) : nullptr;

        mark_unread(test_case.answer, AS_ASK);

        expect_flags({AF_EXISTS | AF_DEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS | AF_YANK_BACK,
                      AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS});
        EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
        EXPECT_EQ(ArticleNum{2}, g_obj_count);
        EXPECT_EQ(1, g_dm_count);
        EXPECT_EQ(ArticleUnread{2}, g_selected_count);
        EXPECT_EQ(2, g_selected_subj_cnt);
        EXPECT_TRUE(g_selected_only);
        EXPECT_EQ(ArticleNum{2}, g_first_art);
        EXPECT_EQ(ArticleNum{test_case.current}, g_art);
        EXPECT_EQ(test_case.current == 3 ? article_ptr(g_art) : nullptr, g_artp);
        EXPECT_EQ(ArticleNum{3}, g_curr_art);
        EXPECT_EQ(article_ptr(g_curr_art), g_curr_artp);
        EXPECT_EQ(ArticleNum{6}, g_recent_art);
        EXPECT_EQ(article_ptr(g_recent_art), g_recent_artp);
        EXPECT_EQ(ArticleNum{3}, g_parsed_art);
        EXPECT_TRUE(g_reread);
        EXPECT_TRUE(g_force_last);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
        EXPECT_EQ(SF_NONE, m_subjects[1].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
        EXPECT_EQ(1, m_subjects[0].m_misc);
        EXPECT_EQ(0, m_subjects[1].m_misc);
        EXPECT_EQ(1, m_subjects[2].m_misc);
    }
}

TEST_F(GroupUnreadArticleTest, helpThenCancelPreservesReadState)
{
    set_scope({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 3);
    g_first_art = ArticleNum{2};
    g_obj_count = ArticleNum{1};
    push_char('n');

    const std::string output = mark_unread('h', AS_ASK);

    EXPECT_NE(std::string::npos, output.find("t or SP to mark thread unread."));
    EXPECT_NE(std::string::npos, output.find("s to mark subthread unread."));
    EXPECT_NE(std::string::npos, output.find("a to mark all articles unread."));
    EXPECT_NE(std::string::npos, output.find("n or q to change nothing."));
    expect_flags({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(g_artp, g_curr_artp);
}

TEST_P(CachedNextSubjectArticleTest, skipsAnIneligibleCachedReplyAndChoosesTheNextSelectedUnreadReply)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, GetParam(),
                          AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         3);
    g_selected_only = true;
    g_search_ahead = ArticleNum{6};
    g_reread = true;
    std::vector<ArticleNum> visited;

    EXPECT_TRUE(dispatch(NEXT_SUBJECT, 5, AS_NORM, [&](ArticleNum article) { visited.push_back(article); }).empty());

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(9, g_page_line);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(MissingReadOrUnselected, CachedNextSubjectArticleTest,
                         testing::Values(AF_NONE, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD));

TEST_P(CachedPreviousSubjectArticleTest, skipsAnIneligibleCachedReplyAndIncludesAnEarlierReadSelectedReply)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_SEL, GetParam(),
                          AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         5);
    g_selected_only = true;
    g_search_ahead = ArticleNum{6};
    g_reread = true;
    std::vector<ArticleNum> visited;

    EXPECT_TRUE(
        dispatch(PREVIOUS_SUBJECT, 3, AS_NORM, [&](ArticleNum article) { visited.push_back(article); }).empty());

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{6}, g_search_ahead);
    EXPECT_EQ(9, g_page_line);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(MissingOrUnselected, CachedPreviousSubjectArticleTest,
                         testing::Values(AF_NONE, AF_EXISTS | AF_UNREAD));

TEST_F(SameSubjectArticleTest, nextSearchSkipsReadAndMissingMatchesAfterTheCachedSubjectEnds)
{
    set_subject_articles(
        {AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
        2);
    m_subjects[1].m_str = m_subjects[0].m_str;
    std::vector<ArticleNum> visited;

    const std::string output =
        dispatch(NEXT_SUBJECT, 5, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}}), visited);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(1, g_page_line);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_old_subject);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

TEST_F(SameSubjectArticleTest, previousSearchIncludesReadRepliesBeforeTheFirstUnreadArticle)
{
    set_subject_articles(
        {AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
        6);
    g_first_art = ArticleNum{6};
    g_search_ahead = ArticleNum{7};
    std::vector<ArticleNum> visited;

    const std::string output =
        dispatch(PREVIOUS_SUBJECT, 2, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{5}, ArticleNum{4}, ArticleNum{3}, ArticleNum{2}}), visited);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(1, g_page_line);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_old_subject);
    EXPECT_EQ(AF_EXISTS, article_ptr(ArticleNum{2})->m_flags);
}

TEST_F(SameSubjectArticleTest, nextSearchTreatsSubjectPunctuationAsLiteralText)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    m_subjects[1].m_str = "Re: Target sX";
    std::vector<ArticleNum> visited;

    dispatch(NEXT_SUBJECT, 6, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}}), visited);
    EXPECT_TRUE(g_old_subject);
    EXPECT_FALSE(g_reread);
}

TEST_F(SameSubjectArticleTest, nextSearchResumesAtTheRememberedSearchAheadNumber)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    m_subjects[1].m_str = m_subjects[0].m_str;
    g_search_ahead = ArticleNum{6};
    std::vector<ArticleNum> visited;

    dispatch(NEXT_SUBJECT, 6, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{6}}), visited);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_TRUE(g_old_subject);
    EXPECT_FALSE(g_reread);
}

TEST_F(SameSubjectArticleTest, previousExhaustionRepromptsAtTheOriginalArticleWithoutWrapping)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    std::vector<ArticleNum> visited;

    const std::string output =
        dispatch(PREVIOUS_SUBJECT, 1, AS_ASK, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_TRUE(visited.empty());
    EXPECT_NE(std::string::npos, output.find("Not found."));
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SameSubjectArticleTest, nextExhaustionContinuesWithTheFirstUnreadArticleInAnotherSubject)
{
    set_subject_articles(
        {AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS}, 2);
    m_subjects[2].m_str = m_subjects[1].m_str;
    std::vector<ArticleNum> visited;

    const std::string output =
        dispatch(NEXT_SUBJECT, 3, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}, ArticleNum{7}}),
              visited);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SameSubjectArticleTest, nextExhaustionKeepsAConsistentGroupEndWhenNoUnreadArticleRemains)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 2);

    dispatch(NEXT_SUBJECT, 8);

    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_old_subject);
}

TEST_F(SameSubjectArticleTest, nextReturnsToScanWhenItsDefaultCommandDoesNotFollowSubjects)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_sa_in = true;
    g_default_cmd = "^Nq";
    std::vector<ArticleNum> visited;

    EXPECT_TRUE(dispatch(' ', 1, AS_SA, [&](ArticleNum article) { visited.push_back(article); }).empty());

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_EQ(9, g_page_line);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
}

TEST_F(SameSubjectArticleTest, scanDefaultTakesPriorityOverTheUniversalDefault)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_sa_in = true;
    g_default_cmd = "^Nq";
    g_univ_read_virt_flag = true;

    dispatch(' ', 1, AS_SA);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(9, g_page_line);
}

TEST_F(SameSubjectArticleTest, universalDefaultMovesToTheNextGroupInsteadOfFollowingTheSubject)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_univ_read_virt_flag = true;
    g_default_cmd = "^Nq";
    std::vector<ArticleNum> visited;

    dispatch(' ', 1, AS_CLEAN, [&](ArticleNum article) { visited.push_back(article); }, NG_NEXT);

    EXPECT_TRUE(visited.empty());
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_EQ(9, g_page_line);
}

TEST_F(SameSubjectArticleTest, previousCommandReadsTheEarlierReplyDespiteScanAndUniversalDefaults)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    g_sa_in = true;
    g_default_cmd = "^Pq";
    g_univ_read_virt_flag = true;

    dispatch(' ', 1);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(9, g_page_line);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
}

TEST_P(ScanFollowSubjectArticleTest, scanFollowAllowsTheNextReplyWithoutExitingTheUniversalGroup)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_sa_in = true;
    g_default_cmd = "^Nq";
    g_sa_follow = GetParam();
    g_s_follow_temp = !GetParam();
    g_univ_read_virt_flag = true;

    dispatch(' ', 2);

    EXPECT_EQ(GetParam(), g_sa_follow);
    EXPECT_EQ(!GetParam(), g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(9, g_page_line);
}

INSTANTIATE_TEST_SUITE_P(PersistentOrTemporary, ScanFollowSubjectArticleTest, testing::Bool());

TEST_P(UniversalFollowSubjectArticleTest, universalFollowAllowsTheNextReplyWithoutExitingTheGroup)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_univ_read_virt_flag = true;
    g_default_cmd = "^Nq";
    g_univ_follow = GetParam();
    g_univ_follow_temp = !GetParam();

    dispatch(' ', 2);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_EQ(GetParam(), g_univ_follow);
    EXPECT_EQ(!GetParam(), g_univ_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(PersistentOrTemporary, UniversalFollowSubjectArticleTest, testing::Bool());

TEST_P(ExplicitSubjectArticleTest, explicitCommandOverridesScanAndUniversalDefaultsAndEnablesTemporaryFollow)
{
    const char direction = GetParam();
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         direction == NEXT_SUBJECT ? 1 : 2);
    g_sa_in = true;
    g_univ_read_virt_flag = true;
    g_default_cmd = "^Nq";
    g_s_default_cmd = true;
    g_univ_default_cmd = true;

    dispatch(direction, direction == NEXT_SUBJECT ? 2 : 1);

    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(Direction, ExplicitSubjectArticleTest, testing::Values(NEXT_SUBJECT, PREVIOUS_SUBJECT));

TEST_F(SameSubjectArticleTest, inactiveScanAndUniversalDefaultsStillFollowTheNextReply)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_default_cmd = "^Nq";

    dispatch(' ', 2);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(9, g_page_line);
}

TEST_P(SelectedFallbackSubjectArticleTest, searchFallbackSkipsUnselectedMatchesJustLikeCachedNavigation)
{
    const char direction = GetParam();
    set_subject_articles({AF_EXISTS, AF_EXISTS | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                          AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                         direction == NEXT_SUBJECT ? 2 : 6);
    m_subjects[1].m_str = m_subjects[0].m_str;
    g_selected_only = true;
    std::vector<ArticleNum> visited;

    dispatch(direction, direction == NEXT_SUBJECT ? 6 : 2, AS_NORM,
             [&](ArticleNum article) { visited.push_back(article); });

    const std::vector<ArticleNum> expected =
        direction == NEXT_SUBJECT ? std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}}
                                  : std::vector<ArticleNum>{ArticleNum{5}, ArticleNum{4}, ArticleNum{3}, ArticleNum{2}};
    EXPECT_EQ(expected, visited);
    EXPECT_TRUE(g_old_subject);
    EXPECT_FALSE(g_reread);
}

INSTANTIATE_TEST_SUITE_P(Direction, SelectedFallbackSubjectArticleTest,
                         testing::Values(NEXT_SUBJECT, PREVIOUS_SUBJECT));

TEST_P(ExhaustedScanSubjectArticleTest, exhaustedSearchRestoresTheReadingAnchorBeforeReturningToScan)
{
    const char direction = GetParam();
    const long current = direction == NEXT_SUBJECT ? 6 : 1;
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         current);
    g_sa_in = true;

    const std::string output = dispatch(direction, current, AS_SA);

    EXPECT_EQ(direction == PREVIOUS_SUBJECT, output.find("Not found.") != std::string::npos);
    EXPECT_EQ(1, g_page_line);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

INSTANTIATE_TEST_SUITE_P(Direction, ExhaustedScanSubjectArticleTest, testing::Values(NEXT_SUBJECT, PREVIOUS_SUBJECT));

TEST_P(InterruptedSubjectArticleTest, interruptedSearchRestoresTheReadingAnchorAndReportsItsStoppingPoint)
{
    const char direction = GetParam();
    const long current = direction == NEXT_SUBJECT ? 2 : 6;
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         current);
    std::vector<ArticleNum> visited;

    const std::string output = dispatch(direction, current, AS_ASK,
                                        [&](ArticleNum article)
                                        {
                                            visited.push_back(article);
                                            if (article == ArticleNum{4})
                                            {
                                                g_int_count = 1;
                                            }
                                        });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{direction == NEXT_SUBJECT ? 3 : 5}, ArticleNum{4}}), visited);
    EXPECT_NE(std::string::npos, output.find("(Intr at 4)"));
    EXPECT_EQ(0, g_int_count);
    EXPECT_EQ(1, g_page_line);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

INSTANTIATE_TEST_SUITE_P(Direction, InterruptedSubjectArticleTest, testing::Values(NEXT_SUBJECT, PREVIOUS_SUBJECT));

TEST_F(SearchAheadArticleTest, cachedSuccessorKeepsSearchAheadActiveWithoutStartingASearch)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    g_search_ahead = ArticleNum{6};
    g_reread = true;
    std::vector<ArticleNum> visited;

    const std::string output = next(2, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(9, g_page_line);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, cachedSuccessorSkipsMissingAndReadEntriesWithoutSearchingOtherSubjects)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    article_ptr(ArticleNum{2})->m_subj_next = article_ptr(ArticleNum{3});
    article_ptr(ArticleNum{3})->m_subj = &m_subjects[0];
    article_ptr(ArticleNum{4})->m_subj = &m_subjects[0];
    article_ptr(ArticleNum{4})->m_subj_next = nullptr;
    m_subjects[1].m_articles = article_ptr(ArticleNum{5});
    std::vector<ArticleNum> visited;

    next(4, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(9, g_page_line);
}

TEST_F(SearchAheadArticleTest, cachedSuccessorHonorsSelectedOnlyEligibility)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                          AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS},
                         1);
    article_ptr(ArticleNum{2})->m_subj_next = article_ptr(ArticleNum{3});
    article_ptr(ArticleNum{3})->m_subj = &m_subjects[0];
    article_ptr(ArticleNum{4})->m_subj = &m_subjects[0];
    article_ptr(ArticleNum{4})->m_subj_next = nullptr;
    m_subjects[1].m_articles = article_ptr(ArticleNum{5});
    g_selected_only = true;
    std::vector<ArticleNum> visited;

    next(4, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(SearchAheadArticleTest, exhaustedCacheSearchesLaterNumbersForTheSameSubject)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    g_reread = true;
    std::vector<ArticleNum> visited;

    const std::string output = next(6, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}}), visited);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(1, g_page_line);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, fallbackSkipsReadAndMissingMatchesBeforeChoosingAnUnreadReply)
{
    set_subject_articles(
        {AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
        2);
    m_subjects[1].m_str = m_subjects[0].m_str;
    std::vector<ArticleNum> visited;

    next(5, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}}), visited);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, positiveSearchAheadStartsFallbackAtTheRememberedNumber)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    m_subjects[1].m_str = m_subjects[0].m_str;
    g_search_ahead = ArticleNum{6};
    std::vector<ArticleNum> visited;

    next(6, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{6}}), visited);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, fallbackTreatsSubjectPunctuationAsLiteralText)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    m_subjects[1].m_str = "Re: Target sX";
    std::vector<ArticleNum> visited;

    next(6, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}}), visited);
    EXPECT_TRUE(g_old_subject);
    EXPECT_FALSE(g_reread);
}

TEST_F(SearchAheadArticleTest, repeatedAdvancementUsesTheCachedReplyThenFallsBackFromTheNewReadingPosition)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         1);
    std::vector<ArticleNum> visited;
    const auto              progress = [&](ArticleNum article) { visited.push_back(article); };

    next(2, AS_NORM, progress);
    EXPECT_TRUE(visited.empty());
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    next(6, AS_NORM, progress);

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}}), visited);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_TRUE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, subjectExhaustionRestartsAtTheFirstUnreadArticleInAnotherSubject)
{
    set_subject_articles(
        {AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS}, 2);
    std::vector<ArticleNum> visited;

    const std::string output = next(4, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}, ArticleNum{5}, ArticleNum{6}, ArticleNum{7}}),
              visited);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SearchAheadArticleTest, subjectExhaustionRestartsAtTheFirstSelectedUnreadArticle)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_SEL,
                          AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS},
                         2);
    g_selected_only = true;

    next(5);

    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, subjectExhaustionKeepsTheGroupEndPairWhenOnlyUnselectedUnreadArticlesRemain)
{
    set_subject_articles(
        {AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS}, 2);
    g_selected_only = true;

    next(8);

    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(SearchAheadArticleTest, subjectExhaustionEndsAnAllReadGroupWithoutRereadingTheCurrentArticle)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 7);

    next(8);

    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_old_subject);
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SearchAheadArticleTest, interruptedFallbackRestoresTheCurrentPairAndKeepsSearchAheadEnabled)
{
    set_subject_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    g_search_ahead = ArticleNum{3};
    std::vector<ArticleNum> visited;

    const std::string output = next(2, AS_ASK,
                                    [&](ArticleNum article)
                                    {
                                        visited.push_back(article);
                                        if (article == ArticleNum{4})
                                        {
                                            g_int_count = 1;
                                        }
                                    });

    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}, ArticleNum{4}}), visited);
    EXPECT_NE(std::string::npos, output.find("(Intr at 4)"));
    EXPECT_EQ(0, g_int_count);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_old_subject);
}

TEST_F(SearchAheadArticleTest, disabledScanThreadedGroupOrDisabledSearchAheadUsesOrdinaryAdvancement)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         2);
    for (int gate = 0; gate < 3; ++gate)
    {
        SCOPED_TRACE(gate);
        g_art = g_curr_art;
        g_artp = g_curr_artp;
        g_scan_on = gate == 0 ? 0 : 1;
        g_threaded_group = gate == 1;
        g_search_ahead = gate == 2 ? ArticleNum{} : ArticleNum{-1};
        std::vector<ArticleNum> visited;

        next(3, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

        EXPECT_TRUE(visited.empty());
        EXPECT_EQ(ArticleNum{}, g_search_ahead);
        EXPECT_EQ(9, g_page_line);
        EXPECT_FALSE(g_reread);
        EXPECT_FALSE(g_old_subject);
    }
}

TEST_F(SearchAheadArticleTest, nextFromGroupEndRestartsReadingAndClearsSearchAheadWithoutSearching)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                          AF_EXISTS | AF_UNREAD, AF_EXISTS},
                         8);
    std::vector<ArticleNum> visited;

    next(3, AS_NORM, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_EQ(9, g_page_line);
    EXPECT_FALSE(g_reread);
}

TEST_F(SearchAheadArticleTest, nextAtTheEndOfAnAllReadGroupRequestsCleanupWithoutSearching)
{
    set_subject_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 8);
    g_search_ahead = ArticleNum{5};
    g_reread = true;
    std::vector<ArticleNum> visited;

    next(8, AS_CLEAN, [&](ArticleNum article) { visited.push_back(article); });

    EXPECT_TRUE(visited.empty());
    EXPECT_EQ(ArticleNum{5}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(9, g_page_line);
}

TEST_P(SearchListingArticleTest, listingMatchesRestoresTheReadingAnchorWithoutChangingReadStateOrSelection)
{
    const char direction = GetParam().first;
    const long current = GetParam().second;
    set_action_articles(current);
    const std::string command = std::string{direction} + "needle" + direction + "r:=";

    const std::string output = perform_search(command);

    EXPECT_EQ(ArticleNum{current}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_NE(std::string::npos, output.find("Needle later"));
    EXPECT_EQ(std::string::npos, output.find("Plain middle"));
    EXPECT_EQ(std::string::npos, output.find("done"));
    EXPECT_EQ(current == 8 ? 4 : direction == '/' ? 2 : 3, g_perform_count);
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL});
}

INSTANTIATE_TEST_SUITE_P(DirectionAndReadingPosition, SearchListingArticleTest,
                         testing::Values(std::make_pair('/', 6L), std::make_pair('?', 6L), std::make_pair('/', 8L),
                                         std::make_pair('?', 8L)));

TEST_P(DirectionalSearchActionArticleTest, junkIncludesTheCurrentMatchAndStopsAtTheDirectionalBoundary)
{
    const char direction = GetParam();
    set_action_articles(6);
    g_verbose = true;
    std::vector<ArticleNum> visited;
    const std::string       command = std::string{direction} + "needle" + direction + ":j";

    const std::string output = perform_search(command, AS_ASK, [&](ArticleNum number) { visited.push_back(number); });

    EXPECT_EQ(ArticleNum{6}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_NE(std::string::npos, output.find("Junked"));
    EXPECT_NE(std::string::npos, output.find("done\n"));
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{2}, g_selected_count);
    if (direction == '/')
    {
        EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{6}, ArticleNum{7}}), visited);
        expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS,
                      AF_EXISTS, AF_EXISTS});
    }
    else
    {
        EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{6}, ArticleNum{5}, ArticleNum{4}, ArticleNum{3}, ArticleNum{2},
                                           ArticleNum{1}}),
                  visited);
        expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS, AF_EXISTS,
                      AF_EXISTS | AF_UNREAD | AF_SEL});
    }
}

INSTANTIATE_TEST_SUITE_P(Direction, DirectionalSearchActionArticleTest, testing::Values('/', '?'));

TEST_F(SearchActionArticleTest, threadedJunkFromTheTopReportsKillsAndDeselectionsThenRestoresTheCurrentArticle)
{
    set_action_articles(3);
    g_use_threads = true;

    const std::string output = perform_search("/needle/t:j");

    EXPECT_EQ("3 articles killed, 2 deselected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS});
}

TEST_F(SearchActionArticleTest, unreadActionAlsoSelectsReadMatchesAndRestoresTheGroupEnd)
{
    set_action_articles(8);
    g_use_threads = true;

    const std::string output = perform_search("/needle/:m");

    EXPECT_TRUE(g_art_do_read);
    EXPECT_EQ("4 articles matched, 1 unkilled, 1 selected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(ArticleUnread{5}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL,
                  AF_NONE, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL});
}

TEST_F(SearchActionArticleTest, selectionIncludesUnselectedUnreadMatchesAndReportsOnlyNewSelections)
{
    set_action_articles(3);
    g_use_threads = true;

    const std::string output = perform_search("/needle/:+");

    EXPECT_EQ("2 articles matched, 1 selected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL});
}

TEST_F(SearchActionArticleTest, unmatchedActionReportsNoAffectedArticlesAndKeepsTheReadingAnchor)
{
    set_action_articles(6);
    g_use_threads = true;

    const std::string output = perform_search("/absent/:j");

    EXPECT_EQ("No articles affected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ(ArticleNum{6}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ(ArticleUnread{4}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS,
                  AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL});
}

TEST_F(SearchActionArticleTest, subjectSearchAheadResumesAtTheFirstRemainingSelectedUnreadArticleAfterJunk)
{
    set_action_articles(6);
    g_search_ahead = ArticleNum{7};

    const std::string output = perform_search("/needle/t:j", AS_NORM);

    EXPECT_NE(std::string::npos, output.find("done\n"));
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(ArticleUnread{1}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS});
}

TEST_F(SearchActionArticleTest, subjectSearchAheadKeepsTheEndNumberAndNullPointerTogetherAfterJunkingAllUnreadArticles)
{
    set_action_articles(8);
    g_search_ahead = ArticleNum{-1};

    const std::string output = perform_search("/.*/:j", AS_NORM);

    EXPECT_NE(std::string::npos, output.find("done\n"));
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS, AF_EXISTS});
}

TEST_F(SearchActionArticleTest, interruptionPreservesCompletedActionsAndRestoresTheOriginalReadingAnchor)
{
    set_action_articles(6);

    const std::string output = perform_search("/needle/:j", AS_ASK,
                                              [](ArticleNum number)
                                              {
                                                  if (number == ArticleNum{7})
                                                  {
                                                      g_int_count = 1;
                                                  }
                                              });

    EXPECT_NE(std::string::npos, output.find("Intr at 7"));
    EXPECT_EQ(std::string::npos, output.find("done"));
    EXPECT_EQ(ArticleNum{6}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(0, g_int_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ(ArticleUnread{3}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_selected_count);
    expect_flags({AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_NONE, AF_EXISTS,
                  AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL});
}

TEST_F(SearchCommandArticleTest, forwardSearchSkipsCurrentReadAndMissingArticlesEvenInSelectedOnlyMode)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD, AF_EXISTS,
                         AF_NONE, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    m_subjects[1].m_str = "Re: Needle middle";
    m_subjects[0].m_flags |= SF_SEL;
    g_selected_only = true;
    g_selected_subj_cnt = 1;

    const std::string output = search_articles('/', "needle");

    EXPECT_EQ(ArticleNum{6}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ("needle", g_last_pat);
    EXPECT_FALSE(g_art_do_read);
}

TEST_F(SearchCommandArticleTest, reverseSearchSkipsCurrentReadAndMissingArticles)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        6);
    m_subjects[1].m_str = "Re: Needle middle";

    const std::string output = search_articles('?', "needle");

    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_FALSE(g_art_do_read);
}

TEST_F(SearchCommandArticleTest, readModifierFindsReadArticlesInEitherDirectionWithoutChangingReadState)
{
    set_search_articles(
        {AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE, AF_EXISTS, AF_EXISTS | AF_UNREAD},
        3);
    struct SearchCase
    {
        char        direction;
        const char *query;
        long        target;
    };
    for (const SearchCase test_case : {SearchCase{'/', "needle/r", 6}, SearchCase{'?', "needle?r", 2}})
    {
        SCOPED_TRACE(test_case.direction);
        g_art = g_curr_art;
        g_artp = g_curr_artp;

        search_articles(test_case.direction, test_case.query);

        EXPECT_EQ(ArticleNum{test_case.target}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
        EXPECT_EQ(AF_EXISTS, g_artp->m_flags);
        EXPECT_TRUE(g_art_do_read);
    }
}

TEST_F(SearchCommandArticleTest, unsuccessfulSearchRestoresCurrentArticleNumberAndPointerInEitherDirection)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_NONE, AF_EXISTS,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    for (const char direction : {'/', '?'})
    {
        SCOPED_TRACE(direction);
        g_art = g_curr_art;
        g_artp = g_curr_artp;

        const std::string output = search_articles(direction, "absent", AS_ASK);

        EXPECT_NE(std::string::npos, output.find("Not found."));
        EXPECT_EQ(ArticleNum{3}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
    }
}

TEST_F(SearchCommandArticleTest, directionalExhaustionDoesNotWrapToMatchesOnTheOtherSide)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        7);

    const std::string forward_output = search_articles('/', "earlier", AS_ASK);

    EXPECT_NE(std::string::npos, forward_output.find("Not found."));
    EXPECT_EQ(ArticleNum{7}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    g_art = g_curr_art = ArticleNum{1};
    g_artp = g_curr_artp = article_ptr(g_art);

    const std::string reverse_output = search_articles('?', "later", AS_ASK);

    EXPECT_NE(std::string::npos, reverse_output.find("Not found."));
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
}

TEST_F(SearchCommandArticleTest, searchesFromGroupEndStartAtTheAppropriateBoundary)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        8);
    for (const char direction : {'/', '?'})
    {
        SCOPED_TRACE(direction);
        g_art = g_curr_art;
        g_artp = g_curr_artp;

        search_articles(direction, "needle");

        EXPECT_EQ(ArticleNum{direction == '/' ? 1 : 7}, g_art);
        EXPECT_EQ(article_ptr(g_art), g_artp);
    }
}

TEST_F(SearchCommandArticleTest, emptyPatternRepeatsThePreviousSearchInEitherDirection)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);

    search_articles('/', "needle");
    ASSERT_EQ(ArticleNum{6}, g_art);
    ASSERT_EQ(article_ptr(g_art), g_artp);
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    search_articles('/', "");
    EXPECT_EQ(ArticleNum{7}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    search_articles('?', "");
    EXPECT_EQ(ArticleNum{6}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ("needle", g_last_pat);
}

TEST_F(SearchCommandArticleTest, unsuccessfulSearchFromGroupEndRestoresTheNullCurrentPointer)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        8);
    for (const char direction : {'/', '?'})
    {
        SCOPED_TRACE(direction);
        g_art = g_curr_art;
        g_artp = g_curr_artp;

        search_articles(direction, "absent", AS_ASK);

        EXPECT_EQ(ArticleNum{8}, g_art);
        EXPECT_EQ(nullptr, g_artp);
    }
}

TEST_F(SearchCommandArticleTest, interruptionRestoresCurrentArticleNumberAndPointer)
{
    expect_interrupted_search(3);
}

TEST_F(SearchCommandArticleTest, interruptionFromGroupEndRestoresTheNullCurrentPointer)
{
    expect_interrupted_search(8);
}

TEST_F(SearchCommandArticleTest, invalidPatternPreservesPositionAndSelectionFromAnArticleOrGroupEnd)
{
    set_search_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_NONE, AF_EXISTS,
                         AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS},
                        3);
    g_first_art = ArticleNum{2};
    g_selected_only = true;
    g_selected_subj_cnt = 2;
    m_subjects[0].m_flags |= SF_SEL;
    m_subjects[2].m_flags |= SF_SEL;
    struct SearchCase
    {
        char direction;
        long current;
    };
    for (const SearchCase test_case : {SearchCase{'/', 3}, SearchCase{'?', 3}, SearchCase{'/', 8}, SearchCase{'?', 8}})
    {
        SCOPED_TRACE(testing::Message() << test_case.direction << " at " << test_case.current);
        g_art = g_curr_art = ArticleNum{test_case.current};
        g_artp = g_curr_artp = test_case.current == 3 ? article_ptr(g_art) : nullptr;

        const std::string output = search_articles(test_case.direction, "[", AS_INP);

        EXPECT_NE(std::string::npos, output.find("Missing ]"));
        EXPECT_EQ(std::string::npos, output.find("Not found."));
        EXPECT_EQ(ArticleNum{test_case.current}, g_art);
        EXPECT_EQ(g_curr_artp, g_artp);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[0].m_flags);
        EXPECT_EQ(SF_NONE, m_subjects[1].m_flags);
        EXPECT_EQ(SF_SEL | SF_VISIT, m_subjects[2].m_flags);
    }
}

TEST_F(SearchCommandArticleTest, repeatWithoutPreviousPatternReportsAnErrorWithoutMoving)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    for (const char direction : {'/', '?'})
    {
        SCOPED_TRACE(direction);

        const std::string output = search_articles(direction, "", AS_INP);

        EXPECT_NE(std::string::npos, output.find("Null search string"));
        EXPECT_EQ(std::string::npos, output.find("Not found."));
        EXPECT_EQ(ArticleNum{3}, g_art);
        EXPECT_EQ(g_curr_artp, g_artp);
        EXPECT_TRUE(g_last_pat.empty());
    }
}

TEST_F(SearchCommandArticleTest, erasingInitialDelimiterCancelsAndPreservesNextCommand)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    push_char('n');

    const std::string output = enter_search('/', "\b", AS_INP);

    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_TRUE(g_last_pat.empty());
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_F(SearchCommandArticleTest, lineKillCancelsPartialSearchAtGroupEndAndRetainsPreviousPattern)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    search_articles('/', "needle");
    ASSERT_EQ(ArticleNum{6}, g_art);
    g_art = g_curr_art = ArticleNum{8};
    g_artp = g_curr_artp = nullptr;
    push_char('p');

    const std::string output = enter_search('?', std::string{"unfinished"} + Ctl('U'), AS_INP);

    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_EQ("needle", g_last_pat);
    EXPECT_EQ(ARTSCOPE_SUBJECT, g_art_how_much);
    EXPECT_FALSE(g_art_do_read);
    EXPECT_EQ(ArticleNum{8}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('p', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_F(SearchCommandArticleTest, erasingWholePatternCancelsWithoutCompilingOrConsumingNextCommand)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    push_char('n');

    const std::string output = enter_search('?', "needle\b\b\b\b\b\b\b", AS_INP);

    EXPECT_EQ(std::string::npos, output.find("Not found."));
    EXPECT_TRUE(g_last_pat.empty());
    EXPECT_EQ(ArticleNum{3}, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

TEST_F(SearchCommandArticleTest, correctedPatternFindsAnArticleAfterACompileError)
{
    set_search_articles({AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_NONE,
                         AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD},
                        3);
    const std::string invalid_output = search_articles('/', "[", AS_INP);
    ASSERT_NE(std::string::npos, invalid_output.find("Missing ]"));
    ASSERT_EQ(ArticleNum{3}, g_art);
    ASSERT_EQ(g_curr_artp, g_artp);

    const std::string corrected_output = search_articles('/', "needle");

    EXPECT_EQ(std::string::npos, corrected_output.find("Missing ]"));
    EXPECT_EQ(std::string::npos, corrected_output.find("Not found."));
    EXPECT_EQ(ArticleNum{6}, g_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ("needle", g_last_pat);
}

TEST_P(InlineViewingArticleTest, enablingInlineViewingResetsTheFirstViewWithoutMovingArticles)
{
    g_auto_view_inline = false;

    const std::string output = toggle_viewing();

    EXPECT_EQ("\nAuto-View inlined mime is on\n", output);
    EXPECT_TRUE(g_auto_view_inline);
    EXPECT_EQ(ArticleLine{}, g_first_view);
}

TEST_P(InlineViewingArticleTest, disablingInlineViewingPreservesTheFirstViewWithoutMovingArticles)
{
    g_auto_view_inline = true;

    const std::string output = toggle_viewing();

    EXPECT_EQ("\nAuto-View inlined mime is off\n", output);
    EXPECT_FALSE(g_auto_view_inline);
    EXPECT_EQ(ArticleLine{37}, g_first_view);
}

INSTANTIATE_TEST_SUITE_P(ReadingOrGroupEnd, InlineViewingArticleTest, testing::Values(4L, 8L));

TEST_P(RereadCurrentArticleTest, viewingUnhiddenRereadsTheCurrentArticleWithoutDisablingRotation)
{
    set_reading_position(GetParam());
    g_rotate = true;

    const std::string output = display('v');

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_do_hiding);
    EXPECT_TRUE(g_rotate);
    EXPECT_FALSE(g_force_last);
    expect_unmoved_view();
}

TEST_P(RereadCurrentArticleTest, controlRRereadsTheCurrentArticleWithHidingAndWithoutRotation)
{
    set_reading_position(GetParam());
    g_do_hiding = false;
    g_rotate = true;

    const std::string output = display(Ctl('r'));

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_FALSE(g_force_last);
    expect_unmoved_view();
}

INSTANTIATE_TEST_SUITE_P(ReadOrUnread, RereadCurrentArticleTest, testing::Values(2L, 4L));

TEST_P(RotateArticleTest, rotationRereadsTheCurrentReadArticleWithoutChangingHiding)
{
    set_reading_position(2);

    const std::string output = display(GetParam());

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_rotate);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_force_last);
    expect_unmoved_view();
}

TEST_P(RotateArticleTest, requestingRotationWhenAlreadyEnabledKeepsItEnabled)
{
    set_reading_position(4);
    g_rotate = true;
    g_do_hiding = false;

    const std::string output = display(GetParam());

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_rotate);
    EXPECT_FALSE(g_do_hiding);
    EXPECT_FALSE(g_force_last);
    expect_unmoved_view();
}

TEST_P(RotateArticleTest, rotationAtGroupEndKeepsTheEndPromptWithoutRereadingTheEndPosition)
{
    set_reading_position(8);

    const std::string output = display(GetParam());

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_rotate);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_TRUE(g_force_last);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(nullptr, g_artp);
    expect_unmoved_view();
}

INSTANTIATE_TEST_SUITE_P(RotationKeys, RotateArticleTest, testing::Values('x', Ctl('x')));

TEST_F(RereadArticleTest, viewingUnhiddenAtGroupEndLeavesTheDisplayOptionsAndRereadStateAlone)
{
    set_reading_position(8);
    g_rotate = true;

    const std::string output = display('v');

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_do_hiding);
    EXPECT_TRUE(g_rotate);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(nullptr, g_artp);
    expect_unmoved_view();
}

TEST_F(RereadArticleTest, controlRAtGroupEndResetsDisplayOptionsAndKeepsTheEndPrompt)
{
    set_reading_position(8);
    g_do_hiding = false;
    g_rotate = true;

    const std::string output = display(Ctl('r'));

    EXPECT_TRUE(output.empty());
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_FALSE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(nullptr, g_artp);
    expect_unmoved_view();
}

TEST_P(ToggleRotationArticleTest, uppercaseRotationTogglesTheModeAndRedrawsFromTheCurrentTopLine)
{
    set_reading_position(4);
    g_rotate = GetParam();
    g_do_hiding = false;

    const std::string output = display('X');

    EXPECT_FALSE(output.empty());
    EXPECT_EQ(!GetParam(), g_rotate);
    EXPECT_FALSE(g_do_hiding);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_TRUE(g_do_fseek);
    EXPECT_EQ(ArticleLine{15}, g_art_line_num);
    EXPECT_EQ(ArticleLine{15}, g_top_line);
    EXPECT_EQ(0, g_term_line);
    EXPECT_EQ(0, g_term_col);
    EXPECT_EQ(0, g_fire_is_out);
}

TEST_P(ToggleRotationArticleTest, uppercaseRotationAtGroupEndTogglesWithoutForcingTheEndPromptOrRedraw)
{
    set_reading_position(8);
    g_rotate = GetParam();
    g_do_hiding = false;

    const std::string output = display('X');

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(!GetParam(), g_rotate);
    EXPECT_FALSE(g_do_hiding);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(nullptr, g_artp);
    expect_unmoved_view();
}

INSTANTIATE_TEST_SUITE_P(RotationInitiallyOffOrOn, ToggleRotationArticleTest, testing::Bool());

TEST_F(RereadArticleTest, uppercaseRotationClampsAnUnsetTopLineAtTheArticleStart)
{
    set_reading_position(4);
    g_top_line = ArticleLine{-1};

    const std::string output = display('X');

    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(g_rotate);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_TRUE(g_do_fseek);
    EXPECT_EQ(ArticleLine{}, g_art_line_num);
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_EQ(0, g_term_line);
    EXPECT_EQ(0, g_term_col);
    EXPECT_EQ(0, g_fire_is_out);
}

TEST_P(RefreshArticleTest, refreshRedrawsTheCurrentScreenWithoutChangingDisplayOptionsOrCachedText)
{
    const char command = GetParam().first;
    const long current = GetParam().second;
    set_reading_position(current);
    cache_article();
    g_rotate = true;
    const bool hiding = current != 2;
    g_do_hiding = hiding;

    const std::string output = display(command);

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{15});
    EXPECT_EQ(ArticleLine{15}, g_top_line);
    EXPECT_EQ(hiding, g_do_hiding);
    EXPECT_TRUE(g_rotate);
    EXPECT_EQ('p', current_char_subst_mode());
    EXPECT_EQ("\xC3\x84\n", art_buf_view(ArticlePosition{}));
    EXPECT_EQ(ArticlePosition{3}, art_buf_pos());
    EXPECT_EQ(ArticlePosition{3}, art_buf_len());
    EXPECT_EQ(ArticlePosition{2}, art_buf_seek());
}

INSTANTIATE_TEST_SUITE_P(CommandAndReadState, RefreshArticleTest,
                         testing::Values(std::make_pair('l', 2L), std::make_pair('l', 4L), std::make_pair(Ctl('l'), 2L),
                                         std::make_pair(Ctl('l'), 4L)));

TEST_P(RefreshBoundaryArticleTest, refreshClampsAnUnsetTopLineWithoutInvalidatingCachedText)
{
    set_reading_position(4);
    cache_article();
    g_top_line = ArticleLine{-1};

    const std::string output = display(GetParam());

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_EQ('p', current_char_subst_mode());
    EXPECT_EQ("\xC3\x84\n", art_buf_view(ArticlePosition{}));
    EXPECT_EQ(ArticlePosition{3}, art_buf_pos());
}

TEST_P(RefreshBoundaryArticleTest, refreshAtGroupEndPreservesTheEndPositionAndCachedPresentation)
{
    set_reading_position(8);
    cache_article();
    g_rotate = true;

    const std::string output = display(GetParam());

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_TRUE(g_rotate);
    EXPECT_EQ('p', current_char_subst_mode());
    EXPECT_EQ("\xC3\x84\n", art_buf_view(ArticlePosition{}));
    EXPECT_EQ(ArticlePosition{3}, art_buf_pos());
    expect_unmoved_view();
}

INSTANTIATE_TEST_SUITE_P(RefreshKeys, RefreshBoundaryArticleTest, testing::Values('l', Ctl('l')));

TEST_P(SubstitutionArticleTest, changingSubstitutionDiscardsCachedTextAndRestartsTheCurrentArticle)
{
    set_reading_position(GetParam());
    cache_article();
    g_rotate = true;

    const std::string output = display("_C");

    EXPECT_FALSE(output.empty());
    EXPECT_EQ('a', current_char_subst_mode());
    expect_cleared_cache();
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_TRUE(g_rotate);
    ASSERT_EQ(0, seek_art(ArticlePosition{}));
    cache_article("Ae\n");
}

INSTANTIATE_TEST_SUITE_P(ReadOrUnread, SubstitutionArticleTest, testing::Values(2L, 4L));

TEST_F(CachedDisplayArticleTest, substitutionWrapsToTheFirstModeAndRegeneratesThePresentation)
{
    set_reading_position(4);
    set_char_subst_mode('a');
    cache_article("Ae\n");

    const std::string output = display("_C");

    EXPECT_FALSE(output.empty());
    EXPECT_EQ('p', current_char_subst_mode());
    expect_cleared_cache();
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    ASSERT_EQ(0, seek_art(ArticlePosition{}));
    cache_article();
}

TEST_F(CachedDisplayArticleTest, substitutionEnablesTheFirstConfiguredModeWhenInitiallyDisabled)
{
    set_reading_position(4);
    set_char_subst_mode('\0');
    cache_article();

    const std::string output = display("_C");

    EXPECT_FALSE(output.empty());
    EXPECT_EQ('p', current_char_subst_mode());
    expect_cleared_cache();
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
}

TEST_F(CachedDisplayArticleTest, substitutionAtGroupEndChangesModeAndDiscardsCacheWithoutRequestingRedraw)
{
    set_reading_position(8);
    cache_article();
    g_rotate = true;

    const std::string output = display("_C");

    EXPECT_TRUE(output.empty());
    EXPECT_EQ('a', current_char_subst_mode());
    expect_cleared_cache();
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_EQ(ArticleLine{41}, g_art_line_num);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_TRUE(g_do_hiding);
    EXPECT_TRUE(g_rotate);
    EXPECT_EQ(12, g_term_line);
    EXPECT_EQ(9, g_term_col);
    EXPECT_EQ(STANDOUT | UNDERLINE, g_fire_is_out);
}

TEST_F(CachedDisplayArticleTest, completingTheSubstitutionPrefixConsumesOnlyItsSecondCharacter)
{
    set_reading_position(4);
    cache_article();

    const std::string output = display(std::string{'_', FINISH_CMD}, "C");

    EXPECT_NE(std::string::npos, output.find('C'));
    EXPECT_EQ('a', current_char_subst_mode());
    expect_cleared_cache();
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
}

TEST_F(BackwardPagingArticleTest, uppercaseBMovesBackOneLineWithoutAddingPageHighlighting)
{
    set_reading_position(4);
    load_article();
    g_top_line = ArticleLine{28};
    g_marking = STANDOUT;

    const std::string output = display('B');

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{27});
    EXPECT_EQ(ArticleLine{27}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(27, "Line 27\n");
}

TEST_P(BackwardPageArticleTest, pageBackUsesTheScreenHeightAndRecordedArticleOffsets)
{
    const char command = std::get<0>(GetParam());
    const int  height = std::get<1>(GetParam());
    const long target = std::get<2>(GetParam());
    set_reading_position(2);
    load_article();
    g_top_line = ArticleLine{28};
    g_tc_LINES = height;

    const std::string output = display(command);

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{target});
    EXPECT_EQ(ArticleLine{target}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(target, "Line " + std::to_string(target) + '\n');
}

INSTANTIATE_TEST_SUITE_P(PageKeysAndScreenHeights, BackwardPageArticleTest,
                         testing::Values(std::make_tuple('b', 10, 20L), std::make_tuple(Ctl('b'), 10, 20L),
                                         std::make_tuple('b', 24, 6L), std::make_tuple(Ctl('b'), 24, 6L)));

TEST_P(BackwardPageBoundaryArticleTest, pageBackClampsAtTheArticleStartWhenLessThanAPageRemains)
{
    set_reading_position(4);
    load_article();
    g_top_line = ArticleLine{3};

    const std::string output = display(GetParam());

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(0, "Line 0\n");
}

TEST_F(BackwardPagingArticleTest, lineBackAtTheArticleStartKeepsTheStartSentinelAndClampsTheSeekLine)
{
    set_reading_position(4);
    load_article();
    g_top_line = ArticleLine{};

    const std::string output = display('B');

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(0, "Line 0\n");
}

TEST_P(BackwardPageMarkingArticleTest, pageBackHighlightsTheOldTopLineOnlyWhenBackPageMarkingIsEnabled)
{
    const MarkingMode  marking = std::get<0>(GetParam());
    const MarkingAreas areas = std::get<1>(GetParam());
    const long         highlight = std::get<2>(GetParam());
    set_reading_position(4);
    load_article();
    g_top_line = ArticleLine{28};
    g_marking = marking;
    g_marking_areas = areas;

    const std::string output = display('b');

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{20});
    EXPECT_EQ(ArticleLine{20}, g_top_line);
    EXPECT_EQ(ArticleLine{highlight}, g_highlight);
    EXPECT_EQ(marking, g_marking);
    EXPECT_EQ(areas, g_marking_areas);
    expect_target(20, "Line 20\n");
}

INSTANTIATE_TEST_SUITE_P(MarkingPolicy, BackwardPageMarkingArticleTest,
                         testing::Values(std::make_tuple(STANDOUT, BACK_PAGE_MARKING, 28L),
                                         std::make_tuple(UNDERLINE, BACK_PAGE_MARKING, 28L),
                                         std::make_tuple(NO_MARKING, BACK_PAGE_MARKING, 7L),
                                         std::make_tuple(STANDOUT, HALF_PAGE_MARKING, 7L)));

TEST_P(BackwardPageBoundaryArticleTest, pageBackStopsAfterThePreviousFormFeedBoundary)
{
    set_reading_position(4);
    load_article(40, 12);
    g_top_line = ArticleLine{20};
    ASSERT_LT(virtual_read(ArticleLine{12}), ArticlePosition{});

    const std::string output = display(GetParam());

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{13});
    EXPECT_EQ(ArticleLine{13}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(13, "Line 13\n");
}

INSTANTIATE_TEST_SUITE_P(PageKeys, BackwardPageBoundaryArticleTest, testing::Values('b', Ctl('b')));

TEST_F(BackwardPagingArticleTest, singleLineBackCanReachTheLineBeforeAFormFeedBoundary)
{
    set_reading_position(4);
    load_article(40, 12);
    g_top_line = ArticleLine{13};
    ASSERT_LT(virtual_read(ArticleLine{12}), ArticlePosition{});

    const std::string output = display('B');

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{12});
    EXPECT_EQ(ArticleLine{12}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(12, "Line 12\f\n");
}

TEST_P(BackwardCommandArticleTest, unsetTopLineRequestsRereadingFromTheArticleStart)
{
    set_reading_position(4);
    load_article();
    g_top_line = ArticleLine{-1};

    const std::string output = display(GetParam());

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{});
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(0, "Line 0\n");
}

TEST_P(BackwardCommandArticleTest, groupEndDoesNotSeekRedrawOrChangeHighlighting)
{
    set_reading_position(8);
    load_article();
    g_marking = STANDOUT;
    const ArticlePosition cached_position = art_buf_pos();
    const ArticlePosition cached_length = art_buf_len();

    const std::string output = display(GetParam());

    EXPECT_TRUE(output.empty());
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    EXPECT_EQ(STANDOUT, g_marking);
    EXPECT_EQ(cached_position, art_buf_pos());
    EXPECT_EQ(cached_length, art_buf_len());
    expect_unmoved_view();
}

INSTANTIATE_TEST_SUITE_P(BackwardKeys, BackwardCommandArticleTest, testing::Values('B', 'b', Ctl('b')));

TEST_F(BackwardPagingArticleTest, repeatedPageBackUsesTheUpdatedTopLineAndHighlight)
{
    set_reading_position(4);
    load_article();
    g_top_line = ArticleLine{28};
    g_marking = STANDOUT;

    const std::string first_output = display('b');
    const ArticleLine first_top = g_top_line;
    const std::string second_output = display('b');

    EXPECT_FALSE(first_output.empty());
    EXPECT_FALSE(second_output.empty());
    EXPECT_EQ(ArticleLine{20}, first_top);
    expect_redraw(ArticleLine{12});
    EXPECT_EQ(ArticleLine{12}, g_top_line);
    EXPECT_EQ(ArticleLine{20}, g_highlight);
    expect_target(12, "Line 12\n");
}

TEST_F(BackwardPagingArticleTest, longArticlePagingCrossesTheVirtualArrayWindowUsingSavedOffsets)
{
    set_reading_position(4);
    load_article(300);
    g_top_line = ArticleLine{260};

    const std::string output = display('b');

    EXPECT_FALSE(output.empty());
    expect_redraw(ArticleLine{252});
    EXPECT_EQ(ArticleLine{252}, g_top_line);
    EXPECT_EQ(ArticleLine{7}, g_highlight);
    expect_target(252, "Line 252\n");
}

TEST_P(KnownSizeEndArticleTest, jumpsToTheKnownEndWithoutChangingArticleReadOrSelectionState)
{
    set_reading_position(GetParam().first);
    set_article_text("First\nLast\n");
    g_do_hiding = GetParam().second;
    ASSERT_NO_FATAL_FAILURE(read_prefix("First\n"));

    const std::string output = display(Ctl('e'));

    EXPECT_EQ(std::string::npos, output.find("Receiving"));
    expect_end(ArticlePosition{11});
    EXPECT_EQ(ArticlePosition{11}, g_raw_art_size);
    EXPECT_EQ(ArticlePosition{11}, g_art_size);
    EXPECT_EQ(ArticlePosition{6}, g_art_pos);
    EXPECT_EQ(ArticlePosition{6}, art_buf_pos());
    EXPECT_EQ(ArticlePosition{g_do_hiding ? 11 : 0}, art_buf_len());
    EXPECT_EQ(ArticlePosition{g_do_hiding ? 11 : 6}, tell_art());
    EXPECT_EQ(GetParam().second, g_do_hiding);
    std::string next_line;
    ASSERT_TRUE(read_art_buf(next_line, false));
    EXPECT_EQ("Last\n", next_line);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnreadWithAndWithoutHiding, KnownSizeEndArticleTest,
                         testing::Values(std::make_pair(2L, true), std::make_pair(4L, true), std::make_pair(2L, false),
                                         std::make_pair(4L, false)));

TEST_F(JumpToEndArticleTest, decodesTheRemainingMimeTextAndRestoresTheHeaderRelativeReadingPosition)
{
    set_reading_position(4);
    const std::string     header{"Subject: end\n\n"};
    const ArticlePosition header_end{static_cast<long>(header.size())};
    set_article_text(header + "First\n=4C=61=73=74\n");
    g_mime_section->m_encoding = MENCODE_QPRINT;
    g_header_type[PAST_HEADER].min_pos = header_end;
    ASSERT_EQ(0, seek_art(header_end));
    set_art_buf_seek(header_end);
    ASSERT_NO_FATAL_FAILURE(read_prefix("First\n"));

    const std::string output = display(Ctl('e'));

    EXPECT_EQ(std::string::npos, output.find("Receiving"));
    expect_end(header_end + ArticlePosition{11});
    EXPECT_EQ(header_end + ArticlePosition{19}, g_raw_art_size);
    EXPECT_EQ(header_end + ArticlePosition{11}, g_art_size);
    EXPECT_EQ(header_end + ArticlePosition{6}, g_art_pos);
    EXPECT_EQ(ArticlePosition{6}, art_buf_pos());
    EXPECT_EQ(ArticlePosition{11}, art_buf_len());
    EXPECT_EQ(g_raw_art_size, art_buf_seek());
    EXPECT_EQ("First\nLast\n", art_buf_view(header_end));
    std::string next_line;
    ASSERT_TRUE(read_art_buf(next_line, false));
    EXPECT_EQ("Last\n", next_line);
}

TEST_F(JumpToEndArticleTest, jumpingFromTheFirstLineSetsTheHighlightBeforeTheArticle)
{
    set_reading_position(4);
    set_article_text("First\nLast\n");
    g_art_line_num = ArticleLine{};

    const std::string output = display(Ctl('e'));

    EXPECT_EQ(std::string::npos, output.find("Receiving"));
    expect_end(ArticlePosition{11}, ArticleLine{}, ArticleLine{-1});
    EXPECT_EQ(ArticlePosition{}, g_art_pos);
    EXPECT_EQ(ArticlePosition{}, art_buf_pos());
    EXPECT_EQ("First\nLast\n", art_buf_view(ArticlePosition{}));
}

TEST_F(JumpToEndArticleTest, anEmptyArticleStillRequestsTheLastPageWithoutReadingPastTheBuffer)
{
    set_reading_position(4);
    set_article_text("");
    g_art_line_num = ArticleLine{};

    const std::string output = display(Ctl('e'));

    EXPECT_EQ(std::string::npos, output.find("Receiving"));
    expect_end(ArticlePosition{}, ArticleLine{}, ArticleLine{-1});
    EXPECT_EQ(ArticlePosition{}, g_art_pos);
    EXPECT_EQ(ArticlePosition{}, g_raw_art_size);
    EXPECT_EQ(ArticlePosition{}, g_art_size);
    expect_cleared_cache();
}

TEST_F(JumpToEndArticleTest, jumpingWhenAlreadyAtArticleEndRetainsTheEndPosition)
{
    set_reading_position(4);
    set_article_text("Last\n");
    ASSERT_NO_FATAL_FAILURE(read_prefix("Last\n"));
    g_art_line_num = ArticleLine{1};

    const std::string output = display(Ctl('e'));

    EXPECT_EQ(std::string::npos, output.find("Receiving"));
    expect_end(ArticlePosition{5}, ArticleLine{1}, ArticleLine{});
    EXPECT_EQ(ArticlePosition{5}, g_art_pos);
    EXPECT_EQ(ArticlePosition{5}, art_buf_pos());
    EXPECT_EQ(ArticlePosition{5}, art_buf_len());
    EXPECT_EQ("Last\n", art_buf_view(ArticlePosition{}));
}

TEST_F(JumpToEndArticleTest, groupEndLeavesPagerStateUntouchedWithoutAnOpenArticleOrKnownSize)
{
    set_reading_position(8);
    std::fclose(g_art_fp);
    g_art_fp = nullptr;
    g_raw_art_size = g_art_size = ArticlePosition{-1};
    g_art_pos = ArticlePosition{17};

    const std::string output = display(Ctl('e'));

    EXPECT_EQ(std::string::npos, output.find("Receiving"));
    expect_unmoved_view();
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticlePosition{-1}, g_raw_art_size);
    EXPECT_EQ(ArticlePosition{-1}, g_art_size);
    EXPECT_EQ(ArticlePosition{17}, g_art_pos);
    EXPECT_EQ(ArticlePosition{29}, g_inner_search);
    EXPECT_EQ(ArticleLine{13}, g_inner_light);
    EXPECT_EQ(6, g_g_line);
    EXPECT_EQ('\f', g_hide_everything);
    expect_cleared_cache();
}

TEST_P(DownloadEndArticleTest, finishesThePendingBodyAndTargetsTheEndInTheCurrentDisplayMode)
{
    ASSERT_NO_FATAL_FAILURE(start_download());
    const ArticlePosition header_end = g_header_type[PAST_HEADER].min_pos;
    const ArticlePosition position = header_end + ArticlePosition{g_do_hiding ? 3 : 2};
    const ArticlePosition end = header_end + ArticlePosition{g_do_hiding ? 8 : 7};

    const std::string output = display(Ctl('e'));

    EXPECT_NE(std::string::npos, output.find("Receiving..."));
    expect_end(end, ArticleLine{41}, ArticleLine{40}, 0);
    EXPECT_EQ(header_end + ArticlePosition{7}, nntp_art_size());
    EXPECT_EQ(nntp_art_size(), g_raw_art_size);
    EXPECT_EQ(end, g_art_size);
    EXPECT_EQ(position, g_art_pos);
    EXPECT_EQ(ArticlePosition{g_do_hiding ? 3 : 2}, art_buf_pos());
    EXPECT_EQ(GetParam(), g_do_hiding);
    EXPECT_TRUE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
    std::string next_line;
    ASSERT_TRUE(read_art_buf(next_line, false));
    EXPECT_EQ("Last\n", next_line);
    EXPECT_EQ(0, seek_art(ArticlePosition{}));
    EXPECT_EQ("Subject: end\n", nntp_read_art(NNTP_STRLEN));
    EXPECT_EQ("\n", nntp_read_art(NNTP_STRLEN));
    EXPECT_EQ("\xC4\n", nntp_read_art(NNTP_STRLEN));
    EXPECT_EQ("Last\n", nntp_read_art(NNTP_STRLEN));
    EXPECT_TRUE(nntp_read_art(NNTP_STRLEN).empty());
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutHiding, DownloadEndArticleTest, testing::Bool());

TEST_P(SubjectListingPositionArticleTest, listsAllUnreadSubjectsInArticleOrderEvenInSelectedOnlyMode)
{
    set_listing_articles(GetParam());

    const std::string output = list_subjects();

    EXPECT_EQ("\n1     First topic\n4     Middle topic\n5     Middle topic\n7     Last topic\n", output);
    EXPECT_EQ(5, g_page_line);
}

TEST_P(SubjectListingPositionArticleTest, customFormattingUsesEachListedArticleAndRestoresTheReadingAnchor)
{
    set_listing_articles(GetParam());
    m_env.expect_env("SUBJLINE", "%a: %s");

    const std::string output = list_subjects();

    EXPECT_EQ("\n1     1: First topic\n4     4: Middle topic\n5     5: Middle topic\n7     7: Last topic\n", output);
    EXPECT_EQ(5, g_page_line);
}

INSTANTIATE_TEST_SUITE_P(ReadUnreadAndGroupEnd, SubjectListingPositionArticleTest, testing::Values(2L, 4L, 8L));

TEST_F(SubjectListingArticleTest, noUnreadArticlesProduceNoSubjectRows)
{
    set_listing_articles();
    for (auto &[number, article] : g_article_list)
    {
        article.m_flags &= ~(AF_UNREAD | AF_SEL);
    }
    m_newsgroup.m_to_read = 0;
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    g_selected_only = false;

    const std::string output = list_subjects();

    EXPECT_EQ("\n", output);
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SubjectListingArticleTest, anEmptyGroupReturnsToItsPromptWithoutCreatingArticles)
{
    set_articles({}, 1);
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE_END;

    const std::string output = list_subjects();

    EXPECT_EQ("\n", output);
    EXPECT_TRUE(g_article_list.empty());
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SubjectListingArticleTest, aMissingUnreadArticleDoesNotProduceAStaleCachedSubject)
{
    set_listing_articles();
    article_ptr(ArticleNum{3})->m_flags = AF_UNREAD;
    ++m_newsgroup.m_to_read;

    const std::string output = list_subjects();

    EXPECT_EQ("\n1     First topic\n4     Middle topic\n5     Middle topic\n7     Last topic\n", output);
    EXPECT_EQ(5, g_page_line);
}

TEST_F(SubjectListingArticleTest, anArticleWithoutASubjectIsSkippedWhileLaterSubjectsAreListed)
{
    set_listing_articles();
    g_parsed_art = ArticleNum{4};
    g_header_type[SUBJ_LINE].min_pos = ArticlePosition{-1};

    const std::string output = list_subjects();

    EXPECT_EQ("\n1     First topic\n5     Middle topic\n7     Last topic\n", output);
    EXPECT_EQ(4, g_page_line);
}

TEST_F(SubjectListingArticleTest, aPendingInterruptStopsListingAndClearsTheInterruptAndCachedFormat)
{
    set_listing_articles();
    g_int_count = 1;
    g_subj_line = "cached: %s";

    const std::string output = list_subjects();

    EXPECT_EQ("\n", output);
    EXPECT_EQ(1, g_page_line);
}

TEST_F(SubjectListingArticleTest, quittingTheMorePromptStopsCustomListingAndRetainsTheGroupEndAnchor)
{
    set_listing_articles(8);
    m_env.expect_env("SUBJLINE", "%s");
    g_tc_LINES = 3;
    g_force_last = true;

    const std::string output = list_subjects("q");

    EXPECT_NE(std::string::npos, output.find("\n1     First topic\n4     Middle topic\n[MORE] "));
    EXPECT_EQ(std::string::npos, output.find("5     "));
    EXPECT_EQ(std::string::npos, output.find("Last topic"));
    EXPECT_EQ(-1, g_page_line);
}

TEST_F(SubjectListingArticleTest, spaceAtTheMorePromptContinuesWithTheRemainingUnreadSubjects)
{
    set_listing_articles();
    g_tc_LINES = 3;

    const std::string output = list_subjects(" ");

    EXPECT_NE(std::string::npos, output.find("\n1     First topic\n4     Middle topic\n[MORE] "));
    EXPECT_NE(std::string::npos, output.find("5     Middle topic\n7     Last topic\n"));
    EXPECT_EQ(3, g_page_line);
}

TEST_F(SubjectListingArticleTest, aLaterListingReloadsTheSubjectFormatFromTheEnvironment)
{
    set_listing_articles();
    g_subj_line = "cached: %s";
    m_env.expect_env("SUBJLINE", "fresh: %s");

    const std::string first_output = list_subjects();
    const std::string second_output = list_subjects();

    EXPECT_EQ("\n1     cached: First topic\n4     cached: Middle topic\n5     cached: Middle topic\n7     cached: Last "
              "topic\n",
              first_output);
    EXPECT_EQ(
        "\n1     fresh: First topic\n4     fresh: Middle topic\n5     fresh: Middle topic\n7     fresh: Last topic\n",
        second_output);
    EXPECT_EQ(5, g_page_line);
}

TEST_F(QuitCommandArticleTest, lowerCaseQuitRequestsTheNextNewsgroup)
{
    expect_quit("q", NG_NEXT);
}

TEST_F(QuitCommandArticleTest, upperCaseQuitRequestsTheNewsgroupPrompt)
{
    expect_quit("Q", NG_ASK);
}

TEST_F(QuitCommandArticleTest, backtickQuitRequestsTheNewsgroupPrompt)
{
    expect_quit("`", NG_ASK);
}

TEST_F(QuitCommandArticleTest, quitVariantsPreserveTheEndOfGroupPosition)
{
    g_art = g_curr_art = ArticleNum{4};
    g_artp = g_curr_artp = nullptr;
    g_mode = MM_ARTICLE_END;
    struct QuitCase
    {
        const char       *command;
        DoNewsgroupResult result;
    };
    for (const QuitCase test_case : {QuitCase{"q", NG_NEXT}, QuitCase{"Q", NG_ASK}, QuitCase{"`", NG_ASK}})
    {
        SCOPED_TRACE(test_case.command);
        expect_quit(test_case.command, test_case.result);
    }
}

TEST_P(ScanEntryArticleTest, semicolonRequestsExplicitScanWithoutChangingTheReadingAnchor)
{
    const long current = GetParam() ? 8 : 3;
    set_reading(current);
    g_sa_in = false;
    g_s_follow_temp = true;
    g_univ_follow_temp = true;
    g_reread = true;
    g_force_last = true;

    dispatch(";", current, AS_SA);
    g_sa_in = true;
    dispatch(";", current, AS_SA);

    EXPECT_TRUE(g_sa_go_explicit);
    EXPECT_FALSE(g_sa_go);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

INSTANTIATE_TEST_SUITE_P(ArticleOrGroupEnd, ScanEntryArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "GroupEnd" : "Article"; });

TEST_P(ScanNextCommandArticleTest, scanDefaultTakesPriorityOverUniversalReturnAtGroupEnd)
{
    set_reading(8);
    g_default_cmd = std::string{GetParam()};
    g_univ_read_virt_flag = true;
    g_reread = true;
    g_force_last = true;

    dispatch(" ", 8, AS_SA);

    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_TRUE(g_reread);
    EXPECT_TRUE(g_force_last);
}

TEST_P(ScanNextCommandArticleTest, universalFollowDoesNotBypassTheScanDefaultReturn)
{
    set_reading(2);
    g_default_cmd = std::string{GetParam()};
    g_univ_read_virt_flag = true;
    g_univ_follow = true;

    dispatch(" ", 2, AS_SA);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_P(ScanNextCommandArticleTest, persistentScanFollowTakesPriorityOverUniversalDefaultReturn)
{
    set_reading(2);
    g_default_cmd = std::string{GetParam()};
    g_univ_read_virt_flag = true;
    g_sa_follow = true;

    dispatch(" ", 3);

    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_EQ(GetParam() == 'N', g_reread);
    EXPECT_FALSE(g_force_last);
}

TEST_P(ScanNextCommandArticleTest, universalDefaultReturnsWhenScanIsInactive)
{
    set_reading(2);
    g_default_cmd = std::string{GetParam()};
    g_sa_in = false;
    g_sa_follow = true;
    g_univ_read_virt_flag = true;
    g_reread = true;

    dispatch(" ", 2, AS_CLEAN, NG_NEXT);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_TRUE(g_reread);
}

TEST_P(ScanNextCommandArticleTest, persistentUniversalFollowContinuesAcrossThreadsWhenScanIsInactive)
{
    set_reading(5);
    g_default_cmd = std::string{GetParam()};
    g_sa_in = false;
    g_univ_read_virt_flag = true;
    g_univ_follow = true;

    dispatch(" ", 6);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_EQ(GetParam() == 'N', g_reread);
}

TEST_P(ScanNextCommandArticleTest, explicitNextEstablishesTemporaryUniversalFollowForLaterDefaults)
{
    set_reading(2);
    g_default_cmd = std::string{GetParam()};
    g_sa_in = false;
    g_univ_read_virt_flag = true;

    dispatch(std::string{GetParam()}, 3);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    dispatch(" ", 4);

    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_EQ(GetParam() == 'N', g_reread);
}

TEST_P(ScanNextCommandArticleTest, allReadGroupCleansUpOnUnreadNextAndAllowsExplicitRereading)
{
    set_scan_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 8);
    const bool rereading = GetParam() == 'N';

    dispatch(std::string{GetParam()}, rereading ? 1 : 8, rereading ? AS_NORM : AS_CLEAN);

    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(rereading ? ArticleNum{} : ArticleNum{3}, g_search_ahead);
    EXPECT_EQ(rereading, g_reread);
    EXPECT_FALSE(g_force_last);
}

INSTANTIATE_TEST_SUITE_P(NextCommand, ScanNextCommandArticleTest, testing::Values('n', 'N'),
                         [](const testing::TestParamInfo<char> &info)
                         { return info.param == 'n' ? "NextUnread" : "NextIncludingRead"; });

TEST_F(ScanContinuationArticleTest, articleModeExplicitNextReadsOneArticleThenTheDefaultReturnsToScan)
{
    set_reading(1, SM_ARTICLE);
    g_univ_read_virt_flag = true;

    dispatch("n", 2);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    g_curr_art = g_art;
    g_curr_artp = g_artp;
    g_search_ahead = ArticleNum{5};

    dispatch(" ", 3, AS_SA);

    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{5}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(ScanContinuationArticleTest, articleModeDefaultWithPersistentFollowAdvancesBeforeReturningToScan)
{
    set_reading(1, SM_ARTICLE);
    g_sa_follow = true;

    dispatch(" ", 2, AS_SA);

    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(ScanContinuationArticleTest, subjectModeTemporaryFollowStopsAtTheFirstArticleOfTheNextSubject)
{
    set_reading(1, SM_SUBJECT);
    g_univ_read_virt_flag = true;

    dispatch("n", 2);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    g_curr_art = g_art;
    g_curr_artp = g_artp;
    g_search_ahead = ArticleNum{5};

    dispatch(" ", 3, AS_SA);

    EXPECT_NE(g_curr_artp->m_subj, g_artp->m_subj);
    EXPECT_EQ(g_curr_artp->m_subj->m_thread, g_artp->m_subj->m_thread);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{5}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(ScanContinuationArticleTest, threadModeTemporaryFollowCrossesSubjectsAndReturnsAtTheNextThread)
{
    set_reading(2);
    g_univ_read_virt_flag = true;

    dispatch("n", 3);
    EXPECT_NE(g_curr_artp->m_subj, g_artp->m_subj);
    EXPECT_EQ(g_curr_artp->m_subj->m_thread, g_artp->m_subj->m_thread);
    for (const long number : {4, 5})
    {
        SCOPED_TRACE(number);
        g_curr_art = g_art;
        g_curr_artp = g_artp;
        dispatch(" ", number);
        EXPECT_EQ(ArticleNum{}, g_search_ahead);
    }
    g_curr_art = g_art;
    g_curr_artp = g_artp;
    g_search_ahead = ArticleNum{5};

    dispatch(" ", 6, AS_SA);

    EXPECT_NE(g_curr_artp->m_subj->m_thread, g_artp->m_subj->m_thread);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{5}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(ScanContinuationArticleTest, nextUnreadSkipsReadMissingAndUnselectedArticlesBeforeTheThreadBoundary)
{
    set_scan_articles({AF_EXISTS, AF_EXISTS, AF_EXISTS | AF_SEL, AF_NONE, AF_EXISTS | AF_UNREAD,
                       AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS | AF_UNREAD | AF_SEL},
                      2);
    g_selected_only = true;

    dispatch("n", 6, AS_SA);

    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

TEST_F(ScanContinuationArticleTest, explicitUppercaseNextIncludesReadUnselectedArticlesAndCrossesScanBoundaries)
{
    set_scan_articles({AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS, AF_EXISTS,
                       AF_EXISTS | AF_UNREAD | AF_SEL},
                      2, SM_SUBJECT);
    g_selected_only = true;

    dispatch("N", 3);

    EXPECT_NE(g_curr_artp->m_subj, g_artp->m_subj);
    EXPECT_TRUE(g_reread);
    g_art = g_curr_art = ArticleNum{5};
    g_artp = g_curr_artp = article_find(g_art);
    g_sel_mode = SM_THREAD;
    g_reread = false;
    g_search_ahead = ArticleNum{5};

    dispatch("N", 6);

    EXPECT_NE(g_curr_artp->m_subj->m_thread, g_artp->m_subj->m_thread);
    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

TEST_F(ScanContinuationArticleTest, unreadExhaustionReturnsToScanWithoutWrappingAndGroupEndRestartHonorsSelection)
{
    set_scan_articles(
        {AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS, AF_EXISTS, AF_EXISTS},
        7);
    g_selected_only = true;

    dispatch("n", 8, AS_SA);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    dispatch("n", 4, AS_SA);

    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
}

TEST_F(ScanContinuationArticleTest, groupEndWithoutSelectedUnreadArticlesReturnsToScanWithoutChoosingUnselectedArticles)
{
    set_scan_articles(
        {AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS, AF_EXISTS}, 8);
    g_selected_only = true;

    dispatch("n", 8, AS_SA);

    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
}

TEST_F(ScanContinuationArticleTest, uppercaseNextReachesGroupEndAndRestartsWithoutReturningToScan)
{
    set_reading(7);

    dispatch("N", 8);
    EXPECT_TRUE(g_force_last);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    g_curr_art = g_art;
    g_curr_artp = g_artp;
    g_force_last = false;

    dispatch("N", 1);

    EXPECT_TRUE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
}

TEST_F(ScanContinuationArticleTest, nonThreadedReadingDoesNotApplyTheThreadedScanSubjectBoundary)
{
    set_reading(2, SM_SUBJECT);
    g_threaded_group = false;

    dispatch("n", 3);

    EXPECT_NE(g_curr_artp->m_subj, g_artp->m_subj);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    EXPECT_FALSE(g_reread);
}

TEST_F(DefaultCommandArticleTest, spaceExecutesOneDefaultAndLeavesLaterCommandsForTheirOwnDispatch)
{
    ASSERT_EQ("npq", g_default_cmd);
    push_char('Q');
    push_char('n');

    EXPECT_EQ(AS_NORM, art_switch(" "));

    expect_position(2);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    EXPECT_EQ('n', g_last_char);
    ASSERT_TRUE(macro_pending());
    const std::string next_command = get_cmd();
    ASSERT_EQ((std::string{'n', FINISH_CMD}), next_command);

    EXPECT_EQ(AS_NORM, art_switch(next_command));

    expect_position(3);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    expect_pending("Q");
}

#ifndef STRICT_CR
TEST_F(DefaultCommandArticleTest, returnAndCarriageReturnAcceptTheDisplayedDefault)
{
    for (const char command : {'\n', '\r'})
    {
        SCOPED_TRACE(static_cast<int>(command));
        g_art = ArticleNum{1};
        g_artp = article_ptr(g_art);
        push_char('q');

        EXPECT_EQ(AS_NORM, art_switch(std::string{command, FINISH_CMD}));

        expect_position(2);
        EXPECT_TRUE(g_s_default_cmd);
        EXPECT_TRUE(g_univ_default_cmd);
        EXPECT_EQ('n', g_last_char);
        expect_pending("q");
    }
}
#endif

TEST_F(DefaultCommandArticleTest, caretDefaultDispatchesTheControlCommandWithoutConsumingQueuedInput)
{
    g_search_ahead = ArticleNum{1};
    set_default_cmd();
    ASSERT_EQ("^Nnpq", g_default_cmd);
    push_char('q');

    EXPECT_EQ(AS_NORM, art_switch(" "));

    expect_position(2);
    EXPECT_EQ(Ctl('n'), g_last_char);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_FALSE(g_s_follow_temp);
    EXPECT_FALSE(g_univ_follow_temp);
    expect_pending("q");
}

TEST_F(DefaultCommandArticleTest, acceptingEitherReadingDefaultReturnsToTheScanSelector)
{
    g_sa_in = true;
    for (const long search_ahead : {0, 1})
    {
        SCOPED_TRACE(search_ahead);
        g_search_ahead = ArticleNum{search_ahead};
        set_default_cmd();
        push_char('n');

        EXPECT_EQ(AS_SA, art_switch(" "));

        expect_position(1);
        EXPECT_EQ(ArticleNum{search_ahead}, g_search_ahead);
        EXPECT_TRUE(g_s_default_cmd);
        EXPECT_TRUE(g_univ_default_cmd);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        expect_pending("n");
    }
}

TEST_F(DefaultCommandArticleTest, acceptingEitherReadingDefaultReturnsToTheUniversalSelector)
{
    g_univ_read_virt_flag = true;
    for (const long search_ahead : {0, 1})
    {
        SCOPED_TRACE(search_ahead);
        g_search_ahead = ArticleNum{search_ahead};
        set_default_cmd();
        push_char('n');

        EXPECT_EQ(AS_CLEAN, art_switch(" "));

        expect_position(1, NG_NEXT);
        EXPECT_EQ(ArticleNum{search_ahead}, g_search_ahead);
        EXPECT_TRUE(g_s_default_cmd);
        EXPECT_TRUE(g_univ_default_cmd);
        EXPECT_FALSE(g_s_follow_temp);
        EXPECT_FALSE(g_univ_follow_temp);
        expect_pending("n");
    }
}

TEST_F(DefaultCommandArticleTest, explicitNextOverridesSelectorReturnAndLaterDefaultsRetainFollowMode)
{
    g_sa_in = true;
    g_univ_read_virt_flag = true;
    g_sel_mode = SM_SUBJECT;
    g_s_default_cmd = true;
    g_univ_default_cmd = true;
    push_char('q');

    EXPECT_EQ(AS_NORM, art_switch("n"));

    expect_position(2);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);

    EXPECT_EQ(AS_NORM, art_switch(" "));

    expect_position(3);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_TRUE(g_s_follow_temp);
    EXPECT_TRUE(g_univ_follow_temp);
    expect_pending("q");
}

TEST_F(DefaultCommandArticleTest, repeatedDefaultInputUsesTheCurrentConfigurationOnceEachTime)
{
    push_char('n');
    EXPECT_EQ(AS_NORM, art_switch(" "));
    expect_position(2);
    g_default_cmd = "Qnpq";

    EXPECT_EQ(AS_CLEAN, art_switch(" "));

    expect_position(2, NG_ASK);
    EXPECT_EQ('Q', g_last_char);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    expect_pending("n");
}

TEST_F(DefaultCommandArticleTest, defaultAtAnExhaustedGroupPreservesTheEndPositionAndQueuedInput)
{
    g_art = g_curr_art = ArticleNum{4};
    g_artp = g_curr_artp = nullptr;
    g_mode = MM_ARTICLE_END;
    m_newsgroup.m_to_read = ArticleUnread{};
    for (long number = 1; number <= 3; ++number)
    {
        article_ptr(ArticleNum{number})->m_flags = AF_EXISTS;
    }
    set_default_cmd();
    ASSERT_EQ("npq", g_default_cmd);
    push_char('Q');

    EXPECT_EQ(AS_CLEAN, art_switch(" "));

    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_EQ(ArticleNum{4}, g_curr_art);
    EXPECT_EQ(nullptr, g_curr_artp);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    expect_flags({AF_EXISTS, AF_EXISTS, AF_EXISTS});
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE_END, g_mode);
    expect_pending("Q");
}

TEST_P(NumericJumpTargetArticleTest, explicitlyJumpsToReadUnselectedOrSelectedArticleWithoutChangingItsState)
{
    enter_number(std::to_string(GetParam()) + '\n');

    expect_target(GetParam());
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

INSTANTIATE_TEST_SUITE_P(ArticleKinds, NumericJumpTargetArticleTest, testing::Values(2L, 3L, 6L));

TEST_P(NumericJumpInputArticleTest, completesMultipleDigitsAndHonorsEditingAndTrailingSpaces)
{
    const std::string output = enter_number(GetParam());

    expect_target(12);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
    EXPECT_EQ(std::string::npos, output.find("article is"));
}

INSTANTIATE_TEST_SUITE_P(NumberEntry, NumericJumpInputArticleTest, testing::Values("12\n", "19\b2\r", "0012   \n"));

TEST_P(NumericJumpCancelArticleTest, cancellingTheNumberPreservesTheReadingPositionAndSearchAhead)
{
    enter_number(GetParam(), AS_INP);

    EXPECT_EQ(g_curr_art, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
    EXPECT_FALSE(g_reread);
}

INSTANTIATE_TEST_SUITE_P(Cancellation, NumericJumpCancelArticleTest, testing::Values("1\b", "12\025"));

TEST_F(NumericJumpArticleTest, clampsZeroToTheFirstAvailableNumberAndWarns)
{
    g_abs_first = ArticleNum{2};

    const std::string output = enter_number("0\n");

    expect_target(2);
    EXPECT_NE(std::string::npos, output.find("(First article is 2)"));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

TEST_F(NumericJumpArticleTest, clampsNumbersBeyondTheGroupToTheLastArticleAndWarns)
{
    const std::string output = enter_number("99\n");

    expect_target(12);
    EXPECT_NE(std::string::npos, output.find("(Last article is 12)"));
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

TEST_F(NumericJumpArticleTest, jumpingToTheCurrentArticleRetainsItsPointerAndRequestsAReread)
{
    for (const long search_ahead : {0, 3, -1})
    {
        SCOPED_TRACE(search_ahead);
        g_search_ahead = ArticleNum{search_ahead};
        g_reread = false;

        enter_number("4\n");

        expect_target(4);
        EXPECT_EQ(ArticleNum{search_ahead == 0 ? 0 : -1}, g_search_ahead);
    }
}

TEST_F(NumericJumpArticleTest, jumpsBackIntoAnExhaustedGroup)
{
    g_art = g_curr_art = article_after(g_last_art);
    g_artp = g_curr_artp = nullptr;
    g_mode = MM_ARTICLE_END;

    enter_number("2\n");

    expect_target(2);
    EXPECT_EQ(ArticleNum{-1}, g_search_ahead);
}

TEST_F(NumericJumpArticleTest, rejectsANumberInAnEmptyGroupWithoutRequestingAReread)
{
    g_article_list.clear();
    g_first_subject->m_articles = nullptr;
    g_first_subject->m_thread = nullptr;
    g_last_art = ArticleNum{};
    g_art = g_curr_art = ArticleNum{1};
    g_artp = g_curr_artp = nullptr;
    g_recent_art = ArticleNum{-1};
    g_recent_artp = nullptr;
    g_mode = MM_ARTICLE_END;
    m_newsgroup.m_to_read = ArticleUnread{};
    g_selected_count = ArticleUnread{};

    const std::string output = enter_number("1\n", AS_ASK);

    EXPECT_NE(std::string::npos, output.find("No articles"));
    EXPECT_EQ(ArticleNum{1}, g_art);
    EXPECT_EQ(nullptr, g_artp);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(ArticleNum{3}, g_search_ahead);
}

TEST_P(NumericJumpMissingArticleTest, retainsTheMissingNumberWithoutCreatingAnArticle)
{
    if (!GetParam())
    {
        g_article_list.erase(ArticleNum{5});
    }
    g_threaded_group = false;
    g_search_ahead = ArticleNum{};

    enter_number("5\n");

    expect_target(5);
    EXPECT_EQ(GetParam(), article_hasdata(ArticleNum{5}));
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
}

INSTANTIATE_TEST_SUITE_P(CachedAndAbsent, NumericJumpMissingArticleTest, testing::Bool());

TEST_P(NumericJumpNavigationArticleTest, continuesFromTheJumpTargetInTheConfiguredReadingOrder)
{
    const NumericJumpNavigation test_case = GetParam();
    g_threaded_group = false;
    g_search_ahead = ArticleNum{};
    if (test_case.sorted)
    {
        for (const long number : {4, 11, 2, 3, 6, 1, 12, 7, 8, 9, 10})
        {
            g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
        }
        g_art_ptr = article_ptr_list_begin();
    }
    enter_number("2\n");
    expect_target(2);
    // The reading loop accepts the jump before the user's next command.
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    EXPECT_EQ(AS_NORM, art_switch(std::string{test_case.command}));

    expect_target(test_case.target);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
    EXPECT_TRUE(g_selected_only);
    if (test_case.sorted)
    {
        ASSERT_NE(nullptr, g_art_ptr);
        EXPECT_EQ(g_artp, *g_art_ptr);
    }
}

INSTANTIATE_TEST_SUITE_P(NumericAndSortedOrder, NumericJumpNavigationArticleTest,
                         testing::Values(NumericJumpNavigation{false, 'n', 4}, NumericJumpNavigation{false, 'N', 3},
                                         NumericJumpNavigation{false, 'P', 1}, NumericJumpNavigation{true, 'n', 6},
                                         NumericJumpNavigation{true, 'N', 3}, NumericJumpNavigation{true, 'P', 11}),
                         [](const testing::TestParamInfo<NumericJumpNavigation> &info)
                         { return std::string{info.param.sorted ? "Sorted" : "Numeric"} + info.param.command; });

TEST_F(NumericJumpArticleTest, nextUnreadInAThreadContinuesAfterTheReadJumpTarget)
{
    enter_number("2\n");
    expect_target(2);

    EXPECT_EQ(AS_NORM, art_switch("n"));

    expect_target(4);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
}

TEST_P(NumericRangeOutputArticleTest, editedRangeJunksReadSelectedAndUnselectedTargetsWithTheConfiguredStatusOutput)
{
    g_use_threads = GetParam();
    g_verbose = true;

    const std::string output = apply_range("1-9\b3:j\n");

    expect_changes({{1, CACHED}, {3, CACHED}});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
    EXPECT_EQ(12, g_check_count);
    if (GetParam())
    {
        EXPECT_EQ("3 articles matched, 2 killed, 1 deselected.", g_msg);
        EXPECT_NE(std::string::npos, output.find(g_msg));
        EXPECT_NE(std::string::npos, output.find("Processing..."));
    }
    else
    {
        EXPECT_NE(std::string::npos, output.find("1      \tJunked"));
        EXPECT_NE(std::string::npos, output.find("2      \n"));
        EXPECT_NE(std::string::npos, output.find("3      \tJunked"));
        EXPECT_EQ(std::string::npos, output.find("Processing..."));
        EXPECT_EQ(std::string::npos, output.find("articles matched"));
    }
}

INSTANTIATE_TEST_SUITE_P(StatusStyle, NumericRangeOutputArticleTest, testing::Bool());

TEST_F(NumericRangeArticleTest, commaSeparatedOverlappingRangesRetainNumericScopeInASortedSelectedOnlyView)
{
    for (const long number : {12, 6, 1, 4, 3, 2, 11})
    {
        g_art_ptr_list.push_back(article_ptr(ArticleNum{number}));
    }
    g_art_ptr = article_ptr_list_begin() + 3;

    const std::string output = apply_range("3,1-3:j\n");

    expect_changes({{1, CACHED}, {3, CACHED}});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(4, g_perform_count);
    EXPECT_EQ("4 articles matched, 2 killed, 1 deselected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_F(NumericRangeArticleTest, dotAloneMarksTheCurrentReadArticleUnreadAndSelectedWithoutRequestingAReread)
{
    g_art = g_curr_art = ArticleNum{2};
    g_artp = g_curr_artp = article_ptr(g_art);
    g_first_art = ArticleNum{4};

    apply_range(".\n");

    expect_changes({{2, CACHED | AF_UNREAD | AF_SEL}});
    EXPECT_EQ(ArticleUnread{9}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{6}, g_selected_count);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ("1 article unkilled, 1 selected.", g_msg);
}

TEST_F(NumericRangeArticleTest, aRangeWithoutAnActionDefaultsToMarkingReadArticlesUnread)
{
    g_first_art = ArticleNum{4};

    apply_range("2-3\n");

    expect_changes({{2, CACHED | AF_UNREAD | AF_SEL}});
    EXPECT_EQ(ArticleUnread{9}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{6}, g_selected_count);
    EXPECT_EQ(ArticleNum{2}, g_first_art);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_EQ("2 articles matched, 1 unkilled, 1 selected.", g_msg);
}

TEST_F(NumericRangeArticleTest, currentArticleCanBeTheUpperRangeEndpoint)
{
    apply_range("2-.:j\n");

    expect_changes({{3, CACHED}, {4, CACHED}});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(3, g_perform_count);
}

TEST_F(NumericRangeArticleTest, dotThroughDollarIncludesTheLastArticleAndSkipsAbsentNumbers)
{
    g_article_list.erase(ArticleNum{8});
    --m_newsgroup.m_to_read;

    apply_range(".-$:j\n");

    expect_changes({{4, CACHED}, {6, CACHED}, {9, CACHED}, {11, CACHED}, {12, CACHED}});
    EXPECT_EQ(ArticleUnread{2}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(8, g_perform_count);
    EXPECT_FALSE(article_hasdata(ArticleNum{8}));
    EXPECT_EQ(AF_CACHED, article_ptr(ArticleNum{5})->m_flags);
    EXPECT_EQ("8 articles matched, 5 killed, 4 deselected.", g_msg);
}

TEST_F(NumericRangeArticleTest, rangeSelectionAndDeselectionChangeOnlyEligibleTargetsAndExactCounts)
{
    apply_range("1-4:+\n");

    expect_changes({{3, CACHED | AF_UNREAD | AF_SEL}});
    EXPECT_EQ(ArticleUnread{8}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{6}, g_selected_count);
    EXPECT_EQ("4 articles matched, 1 selected.", g_msg);

    apply_range("3-4:-\n");

    expect_changes({{3, CACHED | AF_UNREAD}, {4, CACHED | AF_UNREAD}});
    EXPECT_EQ(ArticleUnread{8}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ("2 articles deselected.", g_msg);
}

TEST_F(NumericRangeArticleTest, rangeWithoutAnyStoredTargetsReportsNoArticlesAffected)
{
    g_article_list.erase(ArticleNum{8});
    --m_newsgroup.m_to_read;

    const std::string output = apply_range("8-8:j\n");

    expect_changes({});
    EXPECT_EQ(ArticleUnread{7}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_EQ("No articles affected.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_F(NumericRangeArticleTest, clampsBothRangeEndpointsAndWarnsBeforeApplyingTheAction)
{
    const std::string output = apply_range("0-99:j\n");

    expect_changes(
        {{1, CACHED}, {3, CACHED}, {4, CACHED}, {6, CACHED}, {8, CACHED}, {9, CACHED}, {11, CACHED}, {12, CACHED}});
    EXPECT_EQ(ArticleUnread{}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(12, g_perform_count);
    EXPECT_NE(std::string::npos, output.find("(First article is 1)"));
    EXPECT_NE(std::string::npos, output.find("(Last article is 12)"));
}

TEST_P(NumericRangeInvalidArticleTest, rejectsReversedOrUnfinishedRangesBeforeChangingArticles)
{
    const std::string output = apply_range(GetParam());

    expect_changes({});
    EXPECT_EQ(ArticleUnread{8}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(0, g_perform_count);
    EXPECT_NE(std::string::npos, output.find("Bad range"));
    EXPECT_EQ(std::string::npos, output.find("prior status"));
}

INSTANTIATE_TEST_SUITE_P(Malformed, NumericRangeInvalidArticleTest, testing::Values("4-2:j\n", "4-\n"));

TEST_P(NumericRangeCancelArticleTest, cancellingBeforeCompletionChangesNothingAndReturnsToSingleCharacterInput)
{
    const std::string output = apply_range(GetParam(), AS_INP, 3);

    expect_changes({});
    EXPECT_EQ(ArticleUnread{8}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(77, g_perform_count);
    EXPECT_EQ("prior status", g_msg);
    EXPECT_EQ(std::string::npos, output.find("Processing..."));
}

INSTANTIATE_TEST_SUITE_P(Editing, NumericRangeCancelArticleTest, testing::Values(".\b", "1-3:j\025"));

TEST_F(NumericRangeArticleTest, malformedLaterRangePreservesCompletedActionsAndRestoresTheReadingAnchor)
{
    const std::string output = apply_range("1-2,7-:j\n");

    expect_changes({{1, CACHED}});
    EXPECT_EQ(ArticleUnread{7}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(2, g_perform_count);
    EXPECT_NE(std::string::npos, output.find("Bad range"));
}

TEST_P(NumericRangeFailureArticleTest, invalidActionStopsAtTheFirstTargetAndRetainsCompletedEffects)
{
    g_verbose = GetParam();

    const std::string output = apply_range("1-3:j?\n");

    expect_changes({{1, CACHED}});
    EXPECT_EQ(ArticleUnread{7}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_NE(std::string::npos, output.find("Unknown command: ?"));
    EXPECT_NE(std::string::npos, output.find(GetParam() ? "(Interrupted at article 1)" : "(Intr at 1)"));
}

INSTANTIATE_TEST_SUITE_P(Verbosity, NumericRangeFailureArticleTest, testing::Bool());

TEST_F(NumericRangeArticleTest, rangeExecutorStopsAtAnInterruptionAndRestoresTheReadingAnchor)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    g_int_count = 1;
    testing::internal::CaptureStdout();

    // Command input clears prior interrupts; inject one at the executor.
    const NumNumResult result = num_num("1-3:j");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(NN_ASK, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(CACHED, article_ptr(ArticleNum{1})->m_flags);
    EXPECT_EQ(CACHED | AF_UNREAD, article_ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(CACHED | AF_UNREAD | AF_SEL, article_ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(ArticleUnread{7}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ(0, g_int_count);
    EXPECT_NE(std::string::npos, output.find("(Intr at 1)"));
    EXPECT_EQ(std::string::npos, output.find("Unknown command"));
}

TEST_F(NumericRangeArticleTest, delayedRangeMarksReadAndUnreadTargetsForReturnWithoutChangingOtherArticles)
{
    apply_range("2-4:M\n");

    expect_changes({{2, CACHED | AF_YANK_BACK}, {3, CACHED | AF_YANK_BACK}, {4, CACHED | AF_YANK_BACK}});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
    EXPECT_EQ(4, g_dm_count);
    EXPECT_EQ(3, g_perform_count);
}

TEST_F(NumericRangeArticleTest, applyingARangeFromGroupEndRestoresTheNullReadingAnchor)
{
    g_art = g_curr_art = article_after(g_last_art);
    g_artp = g_curr_artp = nullptr;
    g_mode = MM_ARTICLE_END;

    apply_range("1-3:j\n");

    expect_changes({{1, CACHED}, {3, CACHED}});
    EXPECT_EQ(ArticleUnread{6}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{4}, g_selected_count);
}

TEST_F(NumericRangeArticleTest, anExplicitActionOnOneNumberRepromptsInsteadOfJumpingToThatArticle)
{
    apply_range("6:m\n");

    expect_changes({});
    EXPECT_EQ(ArticleNum{4}, g_art);
    EXPECT_FALSE(g_reread);
    EXPECT_EQ(ArticleUnread{8}, m_newsgroup.m_to_read);
    EXPECT_EQ(ArticleUnread{5}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
    EXPECT_EQ("1 article matched.", g_msg);
}

TEST_P(DirectNumericStepArticleTest, numericStepBypassesConfiguredOrderAndReadAndSelectionFilters)
{
    const char direction = GetParam();
    position(direction == 'N' ? 1 : 4);
    g_breadth_first = true;
    set_order({11, 4, 6, 1, 12, 3, 2, 7, 8, 9, 10});

    EXPECT_TRUE(navigate(std::string{'_', direction}, direction == 'N' ? 2 : 3, true).empty());
}

INSTANTIATE_TEST_SUITE_P(Direction, DirectNumericStepArticleTest, testing::Values('N', 'P'));

TEST_P(DirectNumericInputArticleTest, typedNumericStepIncludesCachedMissingTargetsAndRetainsQueuedInput)
{
    const char direction = GetParam();
    position(direction == 'N' ? 4 : 6);

    navigate(std::string{'_', FINISH_CMD}, 5, true, AS_NORM, 0, -1, direction);

    EXPECT_EQ(AF_CACHED, article_ptr(ArticleNum{5})->m_flags);
}

INSTANTIATE_TEST_SUITE_P(TypedDirection, DirectNumericInputArticleTest, testing::Values('N', 'P'));

TEST_P(DirectNumericMissingArticleTest, numericStepRetainsAnAbsentNumberWithoutAllocatingAnArticle)
{
    const char direction = GetParam();
    position(direction == 'N' ? 4 : 6);
    g_article_list.erase(ArticleNum{5});

    navigate(std::string{'_', direction}, 5, true);

    EXPECT_FALSE(article_hasdata(ArticleNum{5}));
}

INSTANTIATE_TEST_SUITE_P(AbsentDirection, DirectNumericMissingArticleTest, testing::Values('N', 'P'));

TEST_F(DirectNavigationArticleTest, numericNextReachesGroupEndAndExplicitlyRestartsAtTheFirstNumber)
{
    set_flags(1, CACHED);
    position(12);

    EXPECT_TRUE(navigate("_N", 13, false).empty());

    position(13);
    g_search_ahead = ArticleNum{3};

    EXPECT_TRUE(navigate("_N", 1, true).empty());
    EXPECT_EQ(CACHED, article_ptr(ArticleNum{1})->m_flags);
}

TEST_F(DirectNavigationArticleTest, numericPreviousFromGroupEndIncludesTheLastReadUnselectedArticle)
{
    set_flags(12, CACHED);
    position(13);

    EXPECT_TRUE(navigate("_P", 12, true).empty());
}

TEST_P(DirectNumericBoundaryArticleTest, numericPreviousAtTheFirstNumberRepromptsWithoutUnreadOrSelectionQualifiers)
{
    position(1);
    g_verbose = GetParam();

    const std::string output = navigate("_P", 1, false, AS_ASK, 3);

    EXPECT_NE(std::string::npos,
              output.find(GetParam() ? "There are no articles prior to this one." : "No previous articles"));
    EXPECT_EQ(std::string::npos, output.find("unread"));
    EXPECT_EQ(std::string::npos, output.find("selected"));
}

INSTANTIATE_TEST_SUITE_P(WarningStyle, DirectNumericBoundaryArticleTest, testing::Bool());

TEST_P(DirectFirstUnreadArticleTest, caretUsesConfiguredOrderAndSkipsReadAndUnselectedArticles)
{
    set_flags(1, CACHED);
    set_flags(2, CACHED | AF_UNREAD);
    set_flags(3, CACHED | AF_UNREAD | AF_SEL);
    g_first_art = ArticleNum{2};
    long target = 3;
    long order_index = -1;
    switch (GetParam())
    {
    case DirectReadingOrder::Numeric:
        g_threaded_group = false;
        g_search_ahead = ArticleNum{};
        break;
    case DirectReadingOrder::DepthFirst:
        break;
    case DirectReadingOrder::BreadthFirst:
        g_breadth_first = true;
        target = 6;
        break;
    case DirectReadingOrder::Subject:
        g_threaded_group = false;
        m_subjects[1].m_prev = nullptr;
        m_subjects[1].m_next = &m_subjects[0];
        m_subjects[0].m_prev = &m_subjects[1];
        m_subjects[0].m_next = nullptr;
        g_first_subject = &m_subjects[1];
        g_last_subject = &m_subjects[0];
        target = 11;
        break;
    case DirectReadingOrder::Sorted:
        set_order({2, 8, 11, 3, 6, 1, 4, 12, 7, 9, 10});
        target = 11;
        order_index = 2;
        break;
    }

    EXPECT_TRUE(navigate("^", target, false, AS_NORM, 0, order_index).empty());
}

INSTANTIATE_TEST_SUITE_P(ReadingOrder, DirectFirstUnreadArticleTest,
                         testing::Values(DirectReadingOrder::Numeric, DirectReadingOrder::DepthFirst,
                                         DirectReadingOrder::BreadthFirst, DirectReadingOrder::Subject,
                                         DirectReadingOrder::Sorted));

TEST_F(DirectNavigationArticleTest, caretIncludesUnselectedUnreadArticlesWhenSelectionFilteringIsOff)
{
    set_flags(1, CACHED);
    set_flags(2, CACHED | AF_UNREAD);
    g_first_art = ArticleNum{2};
    g_selected_only = false;
    g_threaded_group = false;
    g_search_ahead = ArticleNum{};

    EXPECT_TRUE(navigate("^", 2, false).empty());
}

TEST_F(DirectNavigationArticleTest, caretSkipsAMissingRootAndContinuesThroughItsThread)
{
    set_flags(1, AF_CACHED);
    g_selected_only = false;
    g_first_art = ArticleNum{3};

    EXPECT_TRUE(navigate("^", 3, false).empty());
}

TEST_P(DirectFirstExhaustionArticleTest, caretRetainsAConsistentEndPositionWhenNoEligibleUnreadArticleExists)
{
    for (long number = 1; number <= 12; ++number)
    {
        const ArticleFlags flags = article_ptr(ArticleNum{number})->m_flags;
        set_flags(number, flags & ~(GetParam() ? AF_SEL : AF_UNREAD | AF_SEL));
    }
    g_selected_only = GetParam();

    EXPECT_TRUE(navigate("^", 13, false).empty());
}

INSTANTIATE_TEST_SUITE_P(AllReadOrUnselected, DirectFirstExhaustionArticleTest, testing::Bool());

TEST_F(DirectNavigationArticleTest, caretRestartsFromGroupEndAtTheFirstEligibleUnreadArticle)
{
    position(13);
    set_order({2, 8, 11, 3, 6, 1, 4, 12, 7, 9, 10});

    EXPECT_TRUE(navigate("^", 11, false, AS_NORM, 0, 2).empty());
}

TEST_F(DirectNavigationArticleTest, configuredNavigationAfterANumericStepRepairsTheOrderCursorFromTheNewPointer)
{
    position(1);
    set_order({11, 4, 6, 1, 12, 3, 2, 7, 8, 9, 10});
    navigate("_N", 2, true);
    g_curr_art = g_art;
    g_curr_artp = g_artp;

    EXPECT_EQ(AS_NORM, art_switch("N"));

    EXPECT_EQ(ArticleNum{7}, g_art);
    EXPECT_EQ(article_find(g_art), g_artp);
    EXPECT_EQ(article_ptr_list_begin() + 7, g_art_ptr);
    EXPECT_TRUE(g_reread);
    EXPECT_EQ(ArticleNum{}, g_search_ahead);
}

TEST_F(DirectNavigationArticleTest, directCommandsKeepNumberAndPointerConsistentInAnEmptyGroup)
{
    g_article_list.clear();
    g_first_subject = g_last_subject = nullptr;
    g_last_art = g_last_cached = ArticleNum{};
    m_newsgroup.m_to_read = ArticleUnread{};
    g_selected_count = ArticleUnread{};
    g_selected_subj_cnt = 0;
    g_recent_art = ArticleNum{-1};
    g_recent_artp = nullptr;
    position(1);
    for (const std::string command : {"^", "_N", "_P"})
    {
        SCOPED_TRACE(command);
        g_search_ahead = ArticleNum{3};
        const bool previous = command == "_P";

        navigate(command, 1, false, previous ? AS_ASK : AS_NORM, previous ? 3 : 0);
    }
}

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
