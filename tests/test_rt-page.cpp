// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <trn/cache.h>
#include <trn/rthread.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

#include <array>
#include <initializer_list>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

class ArticlePageStateTest : public testing::Test
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    void              prepare_articles(bool rereading, std::initializer_list<ArticleFlags> flags);
    std::vector<long> article_numbers() const;
    std::vector<long> articles_with_flag(ArticleFlags flag) const;

    Subject m_subject{};

private:
    decltype(g_article_list) m_old_articles{std::move(g_article_list)};
    decltype(g_art_ptr_list) m_old_article_ptrs{std::move(g_art_ptr_list)};
    Article                **m_old_art_ptr{g_art_ptr};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first_art{g_first_art};
    ArticleNum               m_old_last_art{g_last_art};
    ArticleNum               m_old_last_cached{g_last_cached};
    ArticleNum               m_old_obj_count{g_obj_count};
    Subject                 *m_old_first_subject{g_first_subject};
    Subject                 *m_old_last_subject{g_last_subject};
    bool                     m_old_selected_only{g_selected_only};
    bool                     m_old_rereading{g_sel_rereading};
    bool                     m_old_exclusive{g_sel_exclusive};
    bool                     m_old_group_listing{g_sel_group_listing};
    SelectionMode            m_old_mode{g_sel_mode};
    SelectionSortMode        m_old_sort{g_sel_sort};
    int                      m_old_direction{g_sel_direction};
    AddGroupFlags            m_old_mask{g_sel_mask};
    ArticleUnread            m_old_selected_count{g_selected_count};
    int                      m_old_selected_subjects{g_selected_subj_cnt};
    Article                **m_old_page_app{g_sel_page_app};
    Article                **m_old_next_app{g_sel_next_app};
    Subject                 *m_old_page_sp{g_sel_page_sp};
    int                      m_old_total{g_sel_total_obj_cnt};
    int                      m_old_prior{g_sel_prior_obj_cnt};
    int                      m_old_page_objects{g_sel_page_obj_cnt};
    int                      m_old_page_items{g_sel_page_item_cnt};
    int                      m_old_lines{g_tc_LINES};
    int                      m_old_columns{g_tc_COLS};
    int                      m_old_mouse_width{g_mouse_bar_width};
    bool                     m_old_use_sel_num{g_use_sel_num};
    std::string              m_old_sel_chars{g_sel_chars};
};

void ArticlePageStateTest::SetUp()
{
    g_article_list.clear();
    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_abs_first = ArticleNum{1};
    g_first_art = g_abs_first;
    g_first_subject = &m_subject;
    g_last_subject = &m_subject;
    g_selected_only = false;
    g_sel_exclusive = false;
    g_sel_group_listing = false;
    g_sel_mode = SM_ARTICLE;
    g_sel_sort = SS_NATURAL;
    g_sel_direction = 1;
    g_sel_page_app = nullptr;
    g_sel_next_app = nullptr;
    g_sel_page_sp = nullptr;
    g_tc_LINES = 24;
    g_tc_COLS = 80;
    g_mouse_bar_width = 0;
    g_use_sel_num = false;
    m_subject.m_str = "Re: Article eligibility";
    m_subject.m_thread_link = &m_subject;
}

void ArticlePageStateTest::TearDown()
{
    g_article_list = std::move(m_old_articles);
    g_art_ptr_list = std::move(m_old_article_ptrs);
    g_art_ptr = m_old_art_ptr;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first_art;
    g_last_art = m_old_last_art;
    g_last_cached = m_old_last_cached;
    g_obj_count = m_old_obj_count;
    g_first_subject = m_old_first_subject;
    g_last_subject = m_old_last_subject;
    g_selected_only = m_old_selected_only;
    g_sel_rereading = m_old_rereading;
    g_sel_exclusive = m_old_exclusive;
    g_sel_group_listing = m_old_group_listing;
    g_sel_mode = m_old_mode;
    g_sel_sort = m_old_sort;
    g_sel_direction = m_old_direction;
    g_sel_mask = m_old_mask;
    g_selected_count = m_old_selected_count;
    g_selected_subj_cnt = m_old_selected_subjects;
    g_sel_page_app = m_old_page_app;
    g_sel_next_app = m_old_next_app;
    g_sel_page_sp = m_old_page_sp;
    g_sel_total_obj_cnt = m_old_total;
    g_sel_prior_obj_cnt = m_old_prior;
    g_sel_page_obj_cnt = m_old_page_objects;
    g_sel_page_item_cnt = m_old_page_items;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    g_mouse_bar_width = m_old_mouse_width;
    g_use_sel_num = m_old_use_sel_num;
    g_sel_chars = m_old_sel_chars;
}

void ArticlePageStateTest::prepare_articles(bool rereading, std::initializer_list<ArticleFlags> flags)
{
    g_sel_rereading = rereading;
    g_sel_mask = rereading ? AGF_DEL_SEL : AGF_SEL;
    long     number = 0;
    Article *previous = nullptr;
    for (const ArticleFlags state : flags)
    {
        Article *article = article_ptr(ArticleNum{++number});
        article->m_flags = state | AF_CACHED | AF_THREADED;
        article->m_subj = &m_subject;
        article->m_date = number;
        if (previous)
        {
            previous->m_subj_next = article;
        }
        else
        {
            m_subject.m_articles = article;
            m_subject.m_thread = article;
        }
        previous = article;
    }
    g_last_art = ArticleNum{number};
    g_last_cached = g_last_art;
    count_subjects(CS_UNSEL_STORE);
}

std::vector<long> ArticlePageStateTest::article_numbers() const
{
    std::vector<long> result;
    for (const Article *article : g_art_ptr_list)
    {
        result.push_back(article->article_num().value_of());
    }
    return result;
}

std::vector<long> ArticlePageStateTest::articles_with_flag(ArticleFlags flag) const
{
    std::vector<long> result;
    for (const auto &[number, article] : g_article_list)
    {
        if (article.m_flags & flag)
        {
            result.push_back(number.value_of());
        }
    }
    return result;
}

struct ArticleEligibility
{
    ArticleFlags flags;
    bool         rereading;
    bool         included;
};

class ArticlePageEligibilityTest : public ArticlePageStateTest,
                                   public testing::WithParamInterface<std::tuple<ArticleEligibility, bool>>
{
};

class ArticlePageReadScopeTest : public ArticlePageStateTest, public testing::WithParamInterface<bool>
{
};

class ArticlePageReadingSelectionTest : public ArticlePageStateTest,
                                        public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class SubjectPageStateTest : public ArticlePageStateTest
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    void              prepare_groups(SelectionMode mode, bool rereading);
    void              select_article(long number);
    void              exclude_subject(Subject &subject);
    void              initialize_page(bool fill_last_page = PRESERVE_PAGE);
    std::vector<long> subjects_with_flag(SubjectFlags flag) const;

    Subject m_bravo{};
    Subject m_charlie{};

private:
    int         m_old_subject_count{g_subject_count};
    bool        m_old_threaded{g_threaded_group};
    Subject    *m_old_next_sp{g_sel_next_sp};
    int         m_old_item_index{g_sel_item_index};
    int         m_old_term_line{g_term_line};
    std::string m_old_display_mode{g_sel_art_display_mode};
    std::size_t m_old_display_index{g_sel_art_display_mode_index};
};

void SubjectPageStateTest::SetUp()
{
    ArticlePageStateTest::SetUp();
    g_subject_count = 3;
    g_threaded_group = true;
    g_sel_next_sp = nullptr;
    g_sel_art_display_mode = "s";
    g_sel_art_display_mode_index = 0;
}

void SubjectPageStateTest::TearDown()
{
    g_subject_count = m_old_subject_count;
    g_threaded_group = m_old_threaded;
    g_sel_next_sp = m_old_next_sp;
    g_sel_item_index = m_old_item_index;
    g_term_line = m_old_term_line;
    g_sel_art_display_mode = m_old_display_mode;
    g_sel_art_display_mode_index = m_old_display_index;
    ArticlePageStateTest::TearDown();
}

void SubjectPageStateTest::prepare_groups(SelectionMode mode, bool rereading)
{
    prepare_articles(rereading, {AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_UNREAD, AF_EXISTS | AF_UNREAD,
                                 AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS});
    g_sel_mode = mode;
    g_sel_sort = SS_DATE;
    m_subject.m_str = "Re: Alpha";
    m_bravo.m_str = "Re: Bravo";
    m_charlie.m_str = "Re: Charlie";
    m_subject.m_next = &m_bravo;
    m_bravo.m_prev = &m_subject;
    m_bravo.m_next = &m_charlie;
    m_charlie.m_prev = &m_bravo;
    g_last_subject = &m_charlie;
    m_bravo.m_articles = article_ptr(ArticleNum{4});
    m_charlie.m_articles = article_ptr(ArticleNum{7});
    m_bravo.m_thread = m_bravo.m_articles;
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_bravo.m_thread_link = &m_bravo;
    m_charlie.m_thread_link = &m_subject;
    const std::array<Subject *, 9> subjects{&m_subject, &m_subject, &m_subject, &m_bravo,  &m_bravo,
                                            &m_bravo,   &m_charlie, &m_charlie, &m_charlie};
    for (std::size_t i = 0; i < subjects.size(); ++i)
    {
        article_ptr(ArticleNum{static_cast<long>(i + 1)})->m_subj = subjects[i];
    }
    article_ptr(ArticleNum{3})->m_subj_next = nullptr;
    article_ptr(ArticleNum{6})->m_subj_next = nullptr;
    article_ptr(ArticleNum{1})->m_child1 = article_ptr(ArticleNum{2});
    article_ptr(ArticleNum{2})->m_parent = article_ptr(ArticleNum{1});
    article_ptr(ArticleNum{2})->m_sibling = article_ptr(ArticleNum{3});
    article_ptr(ArticleNum{3})->m_parent = article_ptr(ArticleNum{1});
    article_ptr(ArticleNum{3})->m_sibling = article_ptr(ArticleNum{7});
    article_ptr(ArticleNum{7})->m_parent = article_ptr(ArticleNum{1});
    article_ptr(ArticleNum{7})->m_child1 = article_ptr(ArticleNum{8});
    article_ptr(ArticleNum{8})->m_parent = article_ptr(ArticleNum{7});
    article_ptr(ArticleNum{8})->m_sibling = article_ptr(ArticleNum{9});
    article_ptr(ArticleNum{9})->m_parent = article_ptr(ArticleNum{7});
    article_ptr(ArticleNum{4})->m_child1 = article_ptr(ArticleNum{5});
    article_ptr(ArticleNum{5})->m_parent = article_ptr(ArticleNum{4});
    article_ptr(ArticleNum{5})->m_sibling = article_ptr(ArticleNum{6});
    article_ptr(ArticleNum{6})->m_parent = article_ptr(ArticleNum{4});
    count_subjects(CS_UNSEL_STORE);
}

void SubjectPageStateTest::select_article(long number)
{
    article_ptr(ArticleNum{number})->m_flags |= static_cast<ArticleFlags>(g_sel_mask);
    count_subjects(CS_UNSEL_STORE);
}

void SubjectPageStateTest::exclude_subject(Subject &subject)
{
    for (Article *article = subject.m_articles; article; article = article->m_subj_next)
    {
        if (g_sel_rereading)
        {
            article->m_flags |= AF_UNREAD;
        }
        else
        {
            article->m_flags &= ~AF_UNREAD;
        }
    }
    count_subjects(CS_UNSEL_STORE);
}

void SubjectPageStateTest::initialize_page(bool fill_last_page)
{
    init_pages(fill_last_page);
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
}

std::vector<long> SubjectPageStateTest::subjects_with_flag(SubjectFlags flag) const
{
    std::vector<long> result;
    for (const Subject *subject = g_first_subject; subject; subject = subject->m_next)
    {
        if (subject->m_flags & flag)
        {
            result.push_back(subject->m_articles->article_num().value_of());
        }
    }
    return result;
}

class SubjectPageAggregationTest : public SubjectPageStateTest,
                                   public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class SubjectPageSelectedThreadTest : public SubjectPageStateTest,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool, bool>>
{
};

class SelectorPageAnchorTest : public SubjectPageStateTest, public testing::WithParamInterface<bool>
{
};

class ArticlePageFillTest : public SubjectPageStateTest, public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class SubjectPageAnchorTest : public SubjectPageStateTest,
                              public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class SubjectPageFillTest : public SubjectPageStateTest,
                            public testing::WithParamInterface<std::tuple<SelectionMode, bool, bool>>
{
};

} // namespace

TEST_P(ArticlePageEligibilityTest, selectionDoesNotOverrideExistenceOrReadScope)
{
    const auto &[test_case, selected] = GetParam();
    const ArticleFlags selected_flag = test_case.rereading ? AF_DEL_SEL : AF_SEL;
    const ArticleFlags anchor_flags = test_case.rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
    prepare_articles(test_case.rereading, {anchor_flags, test_case.flags | (selected ? selected_flag : AF_NONE)});

    init_pages(PRESERVE_PAGE);

    const std::vector<long> expected = test_case.included ? std::vector<long>{1, 2} : std::vector<long>{1};
    EXPECT_EQ(expected, article_numbers());
    EXPECT_EQ(expected, articles_with_flag(AF_INCLUDED));
    EXPECT_EQ(static_cast<int>(expected.size()), g_sel_total_obj_cnt);
    EXPECT_EQ(ArticleNum{static_cast<long>(expected.size())}, g_obj_count);
    EXPECT_EQ(selected && test_case.included ? 1 : 0, g_selected_count);
    EXPECT_EQ(article_ptr_list_begin(), g_sel_page_app);
}

TEST_P(ArticlePageReadScopeTest, mixedPopulationListsOnlyExistingArticlesInTheCurrentReadScope)
{
    const bool rereading = GetParam();
    prepare_articles(rereading, {AF_EXISTS | AF_UNREAD, AF_EXISTS | AF_UNREAD | AF_SEL, AF_EXISTS,
                                 AF_EXISTS | AF_DEL_SEL, AF_UNREAD, AF_NONE, AF_EXISTS | AF_UNREAD | AF_DEL});

    init_pages(PRESERVE_PAGE);

    const std::vector<long> expected = rereading ? std::vector<long>{3, 4} : std::vector<long>{1, 2, 7};
    EXPECT_EQ(expected, article_numbers());
    EXPECT_EQ(expected, articles_with_flag(AF_INCLUDED));
    EXPECT_EQ(static_cast<int>(expected.size()), g_sel_total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ((std::vector<long>{1, 2, 5, 7}), articles_with_flag(AF_UNREAD));
    EXPECT_EQ((std::vector<long>{2}), articles_with_flag(AF_SEL));
    EXPECT_EQ((std::vector<long>{4}), articles_with_flag(AF_DEL_SEL));
}

TEST_P(ArticlePageReadScopeTest, exclusiveDisplayUsesTheSelectionMaskForTheCurrentReadScope)
{
    const bool         rereading = GetParam();
    const ArticleFlags scope_flags = rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
    prepare_articles(rereading,
                     {scope_flags | AF_SEL, scope_flags | AF_DEL_SEL, scope_flags | AF_SEL | AF_DEL_SEL, scope_flags});
    g_sel_exclusive = true;

    init_pages(PRESERVE_PAGE);

    EXPECT_EQ((std::vector<long>{1, 2, 3, 4}), article_numbers());
    EXPECT_EQ((rereading ? std::vector<long>{2, 3} : std::vector<long>{1, 3}), articles_with_flag(AF_INCLUDED));
    EXPECT_EQ(2, g_sel_total_obj_cnt);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_TRUE(g_sel_exclusive);
}

TEST_P(ArticlePageReadScopeTest, readModeMarksOnlyUnselectedEligibleArticlesForLaterCleanup)
{
    const bool         rereading = GetParam();
    const ArticleFlags scope_flags = rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
    const ArticleFlags selected_flag = rereading ? AF_DEL_SEL : AF_SEL;
    prepare_articles(rereading, {scope_flags, scope_flags | selected_flag, AF_NONE});

    init_pages(PRESERVE_PAGE);

    EXPECT_EQ((rereading ? std::vector<long>{1} : std::vector<long>{}), articles_with_flag(AF_DEL));
    EXPECT_EQ((std::vector<long>{1, 2}), articles_with_flag(AF_INCLUDED));
    EXPECT_EQ((std::vector<long>{2}), articles_with_flag(selected_flag));
}

TEST_P(ArticlePageReadScopeTest, noEligibleArticlesProducesAnEmptyPage)
{
    const bool rereading = GetParam();
    prepare_articles(rereading, {rereading ? AF_EXISTS | AF_UNREAD : AF_EXISTS, AF_NONE, AF_UNREAD});

    init_pages(PRESERVE_PAGE);

    EXPECT_TRUE(g_art_ptr_list.empty());
    EXPECT_TRUE(articles_with_flag(AF_INCLUDED).empty());
    EXPECT_EQ(0, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(0, g_sel_page_obj_cnt);
    EXPECT_EQ(0, g_sel_page_item_cnt);
    EXPECT_EQ(nullptr, g_sel_page_app);
}

TEST_P(ArticlePageReadingSelectionTest, selectedOnlyReadingStillListsUnselectedArticlesForSelection)
{
    const auto &[rereading, selected_only] = GetParam();
    g_selected_only = selected_only;
    const ArticleFlags scope_flags = rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
    const ArticleFlags selected_flag = rereading ? AF_DEL_SEL : AF_SEL;
    prepare_articles(rereading, {scope_flags | selected_flag, scope_flags});

    init_pages(PRESERVE_PAGE);

    EXPECT_EQ((std::vector<long>{1, 2}), article_numbers());
    EXPECT_EQ((std::vector<long>{1, 2}), articles_with_flag(AF_INCLUDED));
    EXPECT_EQ(2, g_sel_total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_FALSE(g_sel_exclusive);
}

INSTANTIATE_TEST_SUITE_P(AvailabilityAndSelection, ArticlePageEligibilityTest,
                         testing::Combine(testing::Values(ArticleEligibility{AF_NONE, false, false},
                                                          ArticleEligibility{AF_UNREAD, false, false},
                                                          ArticleEligibility{AF_EXISTS, false, false},
                                                          ArticleEligibility{AF_EXISTS | AF_UNREAD, false, true},
                                                          ArticleEligibility{AF_NONE, true, false},
                                                          ArticleEligibility{AF_UNREAD, true, false},
                                                          ArticleEligibility{AF_EXISTS, true, true},
                                                          ArticleEligibility{AF_EXISTS | AF_UNREAD, true, false}),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, ArticlePageReadScopeTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadScopeAndReadingSelection, ArticlePageReadingSelectionTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(SubjectPageAggregationTest, groupsEligibleArticlesIntoSubjectOrThreadRows)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    m_charlie.m_flags |= SF_INCLUDED;

    initialize_page();

    EXPECT_EQ(1, m_subject.m_misc);
    EXPECT_EQ(rereading ? 1 : 2, m_bravo.m_misc);
    EXPECT_EQ(rereading ? 2 : 1, m_charlie.m_misc);
    EXPECT_EQ((mode == SM_THREAD ? std::vector<long>{1, 4} : std::vector<long>{1, 4, 7}),
              subjects_with_flag(SF_INCLUDED));
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(4, g_sel_page_obj_cnt);
    EXPECT_EQ(mode == SM_THREAD ? 2 : 3, g_sel_page_item_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_next_sp);
}

TEST_P(SubjectPageSelectedThreadTest, exclusiveDisplayIncludesTheWholeScopeContainingASelectedArticle)
{
    const auto &[mode, rereading, select_reply] = GetParam();
    prepare_groups(mode, rereading);
    const long selected = rereading ? (select_reply ? 8 : 2) : (select_reply ? 7 : 1);
    select_article(selected);
    g_sel_exclusive = true;

    initialize_page();

    const int expected_articles = mode == SM_THREAD ? (rereading ? 3 : 2) : (rereading && select_reply ? 2 : 1);
    EXPECT_EQ((mode == SM_THREAD || !select_reply ? std::vector<long>{1} : std::vector<long>{7}),
              subjects_with_flag(SF_INCLUDED));
    EXPECT_EQ(expected_articles, g_sel_total_obj_cnt);
    EXPECT_EQ(expected_articles, g_sel_page_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ((std::vector<long>{selected}), articles_with_flag(static_cast<ArticleFlags>(g_sel_mask)));
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_sel_exclusive);
}

TEST_P(SubjectPageAggregationTest, exclusiveDisplayExcludesUnselectedNeighboringThreads)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    select_article(rereading ? 6 : 4);
    g_sel_exclusive = true;

    initialize_page();

    EXPECT_EQ((std::vector<long>{4}), subjects_with_flag(SF_INCLUDED));
    EXPECT_EQ(rereading ? 1 : 2, g_sel_total_obj_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_sel_exclusive);
}

TEST_P(SubjectPageAggregationTest, unselectedReadSubjectsReceivePendingCleanupMarks)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    select_article(rereading ? 8 : 7);

    initialize_page();

    EXPECT_EQ((rereading ? std::vector<long>{1, 4} : std::vector<long>{}), subjects_with_flag(SF_DEL));
    EXPECT_EQ((std::vector<long>{7}), subjects_with_flag(static_cast<SubjectFlags>(g_sel_mask)));
    EXPECT_EQ((rereading ? std::vector<long>{8} : std::vector<long>{7}),
              articles_with_flag(static_cast<ArticleFlags>(g_sel_mask)));
    EXPECT_TRUE(articles_with_flag(AF_DEL).empty());
    EXPECT_EQ(1, g_selected_count);
}

TEST_P(SubjectPageAggregationTest, emptyOriginalSubjectDoesNotHideEligibleReplies)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    exclude_subject(m_subject);

    initialize_page();

    EXPECT_EQ(0, m_subject.m_misc);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
    EXPECT_EQ(3, g_sel_page_obj_cnt);
    EXPECT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(nullptr, g_sel_next_sp);
}

TEST_P(SubjectPageAggregationTest, groupsWithNoEligibleArticlesProduceNoRows)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    exclude_subject(m_subject);
    exclude_subject(m_bravo);
    exclude_subject(m_charlie);

    initialize_page();

    EXPECT_EQ(0, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_page_obj_cnt);
    EXPECT_EQ(0, g_sel_page_item_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_next_sp);
}

INSTANTIATE_TEST_SUITE_P(ModeAndReadScope, SubjectPageAggregationTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeReadScopeAndSelectedSubject, SubjectPageSelectedThreadTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(), testing::Bool()));

TEST_P(SelectorPageAnchorTest, reversingSortKeepsTheSameArticleAtThePageAnchor)
{
    const bool rereading = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    initialize_page();
    g_sel_page_app = article_ptr_list_begin() + 1;
    Article *anchor = *g_sel_page_app;
    g_sel_direction = -1;

    initialize_page();

    EXPECT_EQ((rereading ? std::vector<long>{9, 8, 6, 2} : std::vector<long>{7, 5, 4, 1}), article_numbers());
    ASSERT_EQ(article_ptr_list_begin() + 2, g_sel_page_app);
    EXPECT_EQ(anchor, *g_sel_page_app);
    EXPECT_EQ(2, g_sel_prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, removingTheAnchorAdvancesToTheNextEligibleArticle)
{
    const bool rereading = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    initialize_page();
    g_sel_page_app = article_ptr_list_begin() + 1;
    (*g_sel_page_app)->m_flags &= ~AF_EXISTS;
    count_subjects(CS_UNSEL_STORE);

    initialize_page();

    EXPECT_EQ((rereading ? std::vector<long>{2, 8, 9} : std::vector<long>{1, 5, 7}), article_numbers());
    ASSERT_EQ(article_ptr_list_begin() + 1, g_sel_page_app);
    EXPECT_EQ(ArticleNum{rereading ? 8 : 5}, (*g_sel_page_app)->article_num());
    EXPECT_EQ(1, g_sel_prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, removingEarlierArticlesKeepsTheAnchorAndRecountsItsPosition)
{
    const bool rereading = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    initialize_page();
    g_sel_page_app = article_ptr_list_begin() + 1;
    Article *anchor = *g_sel_page_app;
    exclude_subject(m_subject);

    initialize_page();

    ASSERT_EQ(article_ptr_list_begin(), g_sel_page_app);
    EXPECT_EQ(anchor, *g_sel_page_app);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(3, g_sel_page_obj_cnt);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, switchingFromSubjectsStartsWithTheAnchoredSubjectsFirstEligibleArticle)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    initialize_page();
    g_sel_page_sp = &m_bravo;
    g_sel_mode = SM_ARTICLE;

    initialize_page();

    ASSERT_EQ(article_ptr_list_begin() + 1, g_sel_page_app);
    EXPECT_EQ(ArticleNum{rereading ? 6 : 4}, (*g_sel_page_app)->article_num());
    EXPECT_EQ(1, g_sel_prior_obj_cnt);
    EXPECT_EQ(3, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, anEmptySubjectAnchorAdvancesToTheNextEligibleSubject)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    g_tc_LINES = 6;
    initialize_page();
    exclude_subject(m_subject);

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page_sp);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, anEmptyThreadAnchorAdvancesPastAllItsSubjects)
{
    const bool rereading = GetParam();
    prepare_groups(SM_THREAD, rereading);
    g_tc_LINES = 6;
    initialize_page();
    exclude_subject(m_subject);
    exclude_subject(m_charlie);

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page_sp);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, switchingToThreadsMapsAReplySubjectAnchorToTheThreadRow)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page_sp = &m_charlie;
    g_sel_mode = SM_THREAD;

    initialize_page();

    EXPECT_EQ(&m_subject, g_sel_page_sp);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(rereading ? 3 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, fillingAnArticlePageBeforeTheEndKeepsItsAnchor)
{
    prepare_groups(SM_ARTICLE, GetParam());
    g_tc_LINES = 7;
    initialize_page();
    g_sel_page_app = article_ptr_list_begin() + 1;

    initialize_page(FILL_LAST_PAGE);

    EXPECT_EQ(article_ptr_list_begin() + 1, g_sel_page_app);
    EXPECT_EQ(1, g_sel_prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page_item_cnt);
    EXPECT_EQ(2, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(article_ptr_list_end() - 1, g_sel_next_app);
}

TEST_P(SelectorPageAnchorTest, fillingASubjectPageBeforeTheEndKeepsItsAnchor)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page_sp = &m_bravo;

    initialize_page(FILL_LAST_PAGE);

    EXPECT_EQ(&m_bravo, g_sel_page_sp);
    EXPECT_EQ(1, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(&m_charlie, g_sel_next_sp);
}

TEST_P(ArticlePageFillTest, theFillPolicyControlsWhetherTheFinalArticlePagePullsInEarlierRows)
{
    const auto &[rereading, fill_last_page] = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    g_tc_LINES = 7;
    initialize_page();
    g_sel_page_app = article_ptr_list_end() - 1;

    initialize_page(fill_last_page);

    EXPECT_EQ(article_ptr_list_end() - (fill_last_page ? 2 : 1), g_sel_page_app);
    EXPECT_EQ(fill_last_page ? 2 : 3, g_sel_prior_obj_cnt);
    EXPECT_EQ(fill_last_page ? 2 : 1, g_sel_page_item_cnt);
    EXPECT_EQ(fill_last_page ? 2 : 1, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(article_ptr_list_end(), g_sel_next_app);
}

TEST_P(SubjectPageAnchorTest, reversingSortKeepsTheSameGroupAtThePageAnchor)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page_sp = &m_bravo;
    g_sel_direction = -1;

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page_sp);
    EXPECT_EQ(mode == SM_THREAD ? 0 : (rereading ? 2 : 1), g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
}

TEST_P(SubjectPageAnchorTest, removingEarlierArticlesKeepsTheGroupAnchorAndRecountsItsPosition)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page_sp = &m_bravo;
    exclude_subject(m_subject);

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page_sp);
    EXPECT_EQ(mode == SM_THREAD ? &m_bravo : &m_subject, g_first_subject);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page_obj_cnt);
    EXPECT_EQ(3, g_sel_total_obj_cnt);
}

TEST_P(SubjectPageFillTest, theFillPolicyControlsWhetherTheFinalGroupPagePullsInEarlierRows)
{
    const auto &[mode, rereading, fill_last_page] = GetParam();
    prepare_groups(mode, rereading);
    g_tc_LINES = 8;
    g_sel_direction = mode == SM_THREAD ? -1 : 1;
    initialize_page();
    g_sel_page_sp = mode == SM_THREAD ? &m_charlie : &m_bravo;

    initialize_page(fill_last_page);

    Subject *expected_anchor =
        fill_last_page ? (mode == SM_THREAD ? &m_bravo : &m_subject) : (mode == SM_THREAD ? &m_charlie : &m_bravo);
    const int earlier_articles = mode == SM_THREAD ? (rereading ? 1 : 2) : 1;
    EXPECT_EQ(expected_anchor, g_sel_page_sp);
    EXPECT_EQ(fill_last_page ? 0 : earlier_articles, g_sel_prior_obj_cnt);
    EXPECT_EQ(fill_last_page ? 4 : 4 - earlier_articles, g_sel_page_obj_cnt);
    EXPECT_EQ((mode == SM_THREAD ? 1 : 2) + (fill_last_page ? 1 : 0), g_sel_page_item_cnt);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_next_sp);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, SelectorPageAnchorTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadScopeAndFillPolicy, ArticlePageFillTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeAndReadScope, SubjectPageAnchorTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeReadScopeAndFillPolicy, SubjectPageFillTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(), testing::Bool()));

TEST(SelectorPageTest, outputSelectorUsesConfiguredCharacterOrder)
{
    g_use_sel_num = false;
    g_sel_chars = "abc";

    testing::internal::CaptureStdout();
    output_sel(1, 0, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("b ", output);
}
