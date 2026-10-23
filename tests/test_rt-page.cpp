// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <trn/cache.h>
#include <trn/rthread.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

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

private:
    Subject                  m_subject{};
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

TEST(SelectorPageTest, outputSelectorUsesConfiguredCharacterOrder)
{
    g_use_sel_num = false;
    g_sel_chars = "abc";

    testing::internal::CaptureStdout();
    output_sel(1, 0, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("b ", output);
}
