// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/artsrch.h>
#include <trn/artstate.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/head.h>
#include <trn/intrp.h>
#include <trn/ng-internal.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <initializer_list>
#include <string>
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

class SearchCommandArticleTest : public UnreadScopeArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        set_search_articles(std::initializer_list<ArticleFlags> flags, long current);
    std::string search_articles(char direction, std::string_view query, ArticleSwitchResult expected = AS_NORM);

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
                                                      ArticleSwitchResult expected)
{
    const auto                                       current = std::make_pair(g_curr_art, g_curr_artp);
    const auto                                       recent = std::make_pair(g_recent_art, g_recent_artp);
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
    push_char('\n');
    for (auto ch = query.rbegin(); ch != query.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{direction, FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_FALSE(macro_pending());
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
