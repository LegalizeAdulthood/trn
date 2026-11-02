// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-page.h>

#include <trn/cache.h>
#include <trn/ngdata.h>
#include <trn/rthread.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/utf.h>

#include <mock_env.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <ctime>
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
    decltype(g_cache.article_list) m_old_articles{std::move(g_cache.article_list)};
    decltype(g_art_ptr_list) m_old_article_ptrs{std::move(g_art_ptr_list)};
    Article                **m_old_art_ptr{g_art_ptr};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first_art{g_first_art};
    ArticleNum               m_old_last_art{g_last_art};
    ArticleNum                     m_old_last_cached{g_cache.last_cached};
    ArticleNum               m_old_obj_count{g_obj_count};
    Subject                       *m_old_first_subject{g_subj.first};
    Subject                       *m_old_last_subject{g_subj.last};
    bool                     m_old_selected_only{g_sel.selected_only};
    bool                     m_old_rereading{g_sel.rereading};
    bool                     m_old_exclusive{g_sel.exclusive};
    bool                     m_old_group_listing{g_sel.group_listing};
    SelectionMode            m_old_mode{g_sel.mode};
    SelectionSortMode        m_old_sort{g_sel.sort};
    int                      m_old_direction{g_sel.direction};
    AddGroupFlags            m_old_mask{g_sel.mask};
    ArticleUnread            m_old_selected_count{g_selected_count};
    int                      m_old_selected_subjects{g_selected_subj_cnt};
    Article                **m_old_page_app{g_sel_page.page_art};
    Article                **m_old_next_app{g_sel_page.next_art};
    Subject                 *m_old_page_sp{g_sel_page.page_subj};
    int                      m_old_total{g_sel_page.total_obj_cnt};
    int                      m_old_prior{g_sel_page.prior_obj_cnt};
    int                      m_old_page_objects{g_sel_page.page_obj_cnt};
    int                      m_old_page_items{g_sel_page.page_item_cnt};
    int                      m_old_lines{g_tc_LINES};
    int                      m_old_columns{g_tc_COLS};
    int                      m_old_mouse_width{g_mouse_bar_width};
    bool                     m_old_use_sel_num{g_use_sel_num};
    std::string              m_old_sel_chars{g_sel_page.chars};
};

void ArticlePageStateTest::SetUp()
{
    g_cache.article_list.clear();
    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_abs_first = ArticleNum{1};
    g_first_art = g_abs_first;
    g_subj.first = &m_subject;
    g_subj.last = &m_subject;
    g_sel.selected_only = false;
    g_sel.exclusive = false;
    g_sel.group_listing = false;
    g_sel.mode = SM_ARTICLE;
    g_sel.sort = SS_NATURAL;
    g_sel.direction = 1;
    g_sel_page.page_art = nullptr;
    g_sel_page.next_art = nullptr;
    g_sel_page.page_subj = nullptr;
    g_tc_LINES = 24;
    g_tc_COLS = 80;
    g_mouse_bar_width = 0;
    g_use_sel_num = false;
    m_subject.m_str = "Re: Article eligibility";
    m_subject.m_thread_link = &m_subject;
}

void ArticlePageStateTest::TearDown()
{
    g_cache.article_list = std::move(m_old_articles);
    g_art_ptr_list = std::move(m_old_article_ptrs);
    g_art_ptr = m_old_art_ptr;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first_art;
    g_last_art = m_old_last_art;
    g_cache.last_cached = m_old_last_cached;
    g_obj_count = m_old_obj_count;
    g_subj.first = m_old_first_subject;
    g_subj.last = m_old_last_subject;
    g_sel.selected_only = m_old_selected_only;
    g_sel.rereading = m_old_rereading;
    g_sel.exclusive = m_old_exclusive;
    g_sel.group_listing = m_old_group_listing;
    g_sel.mode = m_old_mode;
    g_sel.sort = m_old_sort;
    g_sel.direction = m_old_direction;
    g_sel.mask = m_old_mask;
    g_selected_count = m_old_selected_count;
    g_selected_subj_cnt = m_old_selected_subjects;
    g_sel_page.page_art = m_old_page_app;
    g_sel_page.next_art = m_old_next_app;
    g_sel_page.page_subj = m_old_page_sp;
    g_sel_page.total_obj_cnt = m_old_total;
    g_sel_page.prior_obj_cnt = m_old_prior;
    g_sel_page.page_obj_cnt = m_old_page_objects;
    g_sel_page.page_item_cnt = m_old_page_items;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    g_mouse_bar_width = m_old_mouse_width;
    g_use_sel_num = m_old_use_sel_num;
    g_sel_page.chars = m_old_sel_chars;
}

void ArticlePageStateTest::prepare_articles(bool rereading, std::initializer_list<ArticleFlags> flags)
{
    g_sel.rereading = rereading;
    g_sel.mask = rereading ? AGF_DEL_SEL : AGF_SEL;
    long     number = 0;
    Article *previous = nullptr;
    for (const ArticleFlags state : flags)
    {
        Article *article = g_cache.ptr(ArticleNum{++number});
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
    g_cache.last_cached = g_last_art;
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
    for (const auto &[number, article] : g_cache.article_list)
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

class ArticlePageDisplayState : public ArticlePageStateTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        prepare_display(bool rereading, char mode, bool exclusive = false);
    std::string render_page();
    std::string expected_rows(char mode, std::initializer_list<long> numbers) const;
    void        expect_article_state() const;

private:
    trn::testing::MockEnvironment      m_env;
    std::array<ArticleFlags, 7>        m_flags{};
    std::array<SelectionItem, MAX_SEL> m_old_items{};
    bool                               m_old_in_group{g_in_ng};
    bool                               m_old_threaded{g_threaded_group};
    bool                               m_old_redirected{g_redirected};
    bool                               m_old_erase_screen{g_erase_screen};
    bool                               m_old_erase_each_line{g_erase_each_line};
    std::string                        m_old_name{g_newsgroup_name};
    std::string                        m_old_moderated{g_moderated};
    std::string                        m_old_display_mode{g_sel_art_display_mode};
    std::size_t                        m_old_display_index{g_sel_art_display_mode_index};
    int                                m_old_item_index{g_sel_page.item_index};
    int                                m_old_last_line{g_sel_page.last_line};
    bool                               m_old_at_end{g_sel_page.at_end};
    Article                           *m_old_last_article{g_sel_page.last_art};
    Subject                           *m_old_last_subject{g_sel_page.last_subj};
    int                                m_old_term_line{g_term_line};
    int                                m_old_term_column{g_term_col};
    int                                m_old_scrolled{g_term_scrolled};
    int                                m_old_fire{g_fire_is_out};
    std::string_view                   m_old_home{g_tc_HO};
    std::string_view                   m_old_clear{g_tc_CD};
    std::string_view                   m_old_return{g_tc_CR};
    std::string_view                   m_old_standout{g_tc_SO};
    std::string_view                   m_old_end_standout{g_tc_SE};
};

void ArticlePageDisplayState::SetUp()
{
    ArticlePageStateTest::SetUp();
    std::copy(std::begin(g_sel_page.items), std::end(g_sel_page.items), m_old_items.begin());
    m_env.expect_env_repeatedly("SELECTCHARS", "abcdef");
    g_in_ng = true;
    g_threaded_group = false;
    g_redirected = false;
    g_newsgroup_name = "comp.display";
    g_moderated.clear();
    g_erase_screen = false;
    g_erase_each_line = false;
    g_tc_HO = "<home>";
    g_tc_CD = "<clear>";
    g_tc_CR = "";
    g_tc_SO = "";
    g_tc_SE = "";
    g_tc_LINES = 9;
    g_sel_page.last_subj = nullptr;
}

void ArticlePageDisplayState::TearDown()
{
    std::copy(m_old_items.begin(), m_old_items.end(), std::begin(g_sel_page.items));
    g_in_ng = m_old_in_group;
    g_threaded_group = m_old_threaded;
    g_redirected = m_old_redirected;
    g_newsgroup_name = m_old_name;
    g_moderated = m_old_moderated;
    g_erase_screen = m_old_erase_screen;
    g_erase_each_line = m_old_erase_each_line;
    g_sel_art_display_mode = m_old_display_mode;
    g_sel_art_display_mode_index = m_old_display_index;
    g_sel_page.item_index = m_old_item_index;
    g_sel_page.last_line = m_old_last_line;
    g_sel_page.at_end = m_old_at_end;
    g_sel_page.last_art = m_old_last_article;
    g_sel_page.last_subj = m_old_last_subject;
    g_term_line = m_old_term_line;
    g_term_col = m_old_term_column;
    g_term_scrolled = m_old_scrolled;
    g_fire_is_out = m_old_fire;
    g_tc_HO = m_old_home;
    g_tc_CD = m_old_clear;
    g_tc_CR = m_old_return;
    g_tc_SO = m_old_standout;
    g_tc_SE = m_old_end_standout;
    ArticlePageStateTest::TearDown();
}

void ArticlePageDisplayState::prepare_display(bool rereading, char mode, bool exclusive)
{
    const ArticleFlags scope = rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
    const ArticleFlags opposite_scope = rereading ? AF_EXISTS | AF_UNREAD : AF_EXISTS;
    const ArticleFlags selected = rereading ? AF_DEL_SEL : AF_SEL;
    const ArticleFlags opposite_selection = rereading ? AF_SEL : AF_DEL_SEL;
    prepare_articles(rereading, {scope | selected, scope | opposite_selection, scope, scope | selected,
                                 opposite_scope | selected, selected, scope});
    g_sel.exclusive = exclusive;
    g_sel_art_display_mode.assign(1, mode);
    g_sel_art_display_mode_index = 0;
    const std::array<std::string, 7> authors{"Alice",           "Bob",      "Carol", "Dan", "Opposite scope",
                                             "Missing article", "Next page"};
    for (long number = 1; number <= 7; ++number)
    {
        Article *article = g_cache.ptr(ArticleNum{number});
        article->m_from = authors[static_cast<std::size_t>(number - 1)] + " <author@example.test>";
        article->m_date = static_cast<std::time_t>(number * 86400);
    }
    init_pages(PRESERVE_PAGE);
    for (long number = 1; number <= 7; ++number)
    {
        g_cache.ptr(ArticleNum{number})->m_flags &= ~AF_DEL;
    }
    g_cache.ptr(ArticleNum{3})->m_flags |= AF_DEL;
    g_cache.ptr(ArticleNum{4})->m_flags |= AF_DEL;
    for (long number = 1; number <= 7; ++number)
    {
        m_flags[static_cast<std::size_t>(number - 1)] = g_cache.ptr(ArticleNum{number})->m_flags;
    }
    g_sel_page.last_art = g_cache.ptr(ArticleNum{exclusive ? 4 : 3});
    g_sel_page.item_index = -1;
}

std::string ArticlePageDisplayState::render_page()
{
    testing::internal::CaptureStdout();
    display_page();
    return testing::internal::GetCapturedStdout();
}

std::string ArticlePageDisplayState::expected_rows(char mode, std::initializer_list<long> numbers) const
{
    const std::array<std::string, 4> authors{"Alice", "Bob", "Carol", "Dan"};
    const std::array<char, 4>        marks{'+', ' ', '-', '*'};
    std::string                      rows;
    char                             label = 'a';
    for (const long number : numbers)
    {
        const std::size_t index = static_cast<std::size_t>(number - 1);
        rows += label++;
        rows += marks[index];
        if (mode == 'd')
        {
            rows += std::string{std::ctime(&g_cache.ptr(ArticleNum{number})->m_date)}.substr(0, 16);
        }
        else if (mode != 's')
        {
            rows += authors[index] + std::string(16 - authors[index].size(), ' ');
        }
        rows += "  ";
        rows += mode != 's' && number != 1 ? ">Article eligibility\n" : "Article eligibility\n";
    }
    return rows;
}

void ArticlePageDisplayState::expect_article_state() const
{
    for (long number = 1; number <= 7; ++number)
    {
        SCOPED_TRACE(number);
        EXPECT_EQ(m_flags[static_cast<std::size_t>(number - 1)], g_cache.ptr(ArticleNum{number})->m_flags);
    }
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
}

class ArticlePageDisplayTest : public ArticlePageDisplayState,
                               public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class ExclusiveArticlePageDisplayTest : public ArticlePageDisplayState, public testing::WithParamInterface<bool>
{
};

class ArticlePageBoundaryState : public ArticlePageDisplayState
{
protected:
    void        prepare_empty(bool rereading, bool populated);
    void        leave_read_scope();
    std::string page_to_end();
    void        expect_empty(const std::string &output, bool rereading) const;
};

void ArticlePageBoundaryState::prepare_empty(bool rereading, bool populated)
{
    if (populated)
    {
        prepare_display(rereading, 'l');
        leave_read_scope();
    }
    else
    {
        prepare_articles(rereading, {});
    }
    init_pages(PRESERVE_PAGE);
}

void ArticlePageBoundaryState::leave_read_scope()
{
    for (Article *article : g_art_ptr_list)
    {
        article->m_flags ^= AF_UNREAD;
        article->m_flags &= ~(AF_SEL | AF_DEL_SEL);
    }
    count_subjects(CS_UNSEL_STORE);
}

std::string ArticlePageBoundaryState::page_to_end()
{
    std::string output = render_page();
    while (g_sel_page.next_page())
    {
        output = render_page();
    }
    return output;
}

void ArticlePageBoundaryState::expect_empty(const std::string &output, bool rereading) const
{
    EXPECT_THAT(output, testing::HasSubstr(rereading ? "0 read articles" : "0 articles"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Article eligibility")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Next page")));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_FALSE(g_sel_page.next_page());
}

class EmptyArticlePageDisplayTest : public ArticlePageBoundaryState,
                                    public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class FinalArticlePageDisplayTest : public ArticlePageBoundaryState,
                                    public testing::WithParamInterface<std::tuple<bool, int>>
{
};

class RemovedArticlePageDisplayTest : public ArticlePageBoundaryState, public testing::WithParamInterface<bool>
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
    int         m_old_subject_count{g_subj.count};
    bool        m_old_threaded{g_threaded_group};
    Subject    *m_old_next_sp{g_sel_page.next_subj};
    int         m_old_item_index{g_sel_page.item_index};
    int         m_old_term_line{g_term_line};
    std::string m_old_display_mode{g_sel_art_display_mode};
    std::size_t m_old_display_index{g_sel_art_display_mode_index};
};

void SubjectPageStateTest::SetUp()
{
    ArticlePageStateTest::SetUp();
    g_subj.count = 3;
    g_threaded_group = true;
    g_sel_page.next_subj = nullptr;
    g_sel_art_display_mode = "s";
    g_sel_art_display_mode_index = 0;
}

void SubjectPageStateTest::TearDown()
{
    g_subj.count = m_old_subject_count;
    g_threaded_group = m_old_threaded;
    g_sel_page.next_subj = m_old_next_sp;
    g_sel_page.item_index = m_old_item_index;
    g_term_line = m_old_term_line;
    g_sel_art_display_mode = m_old_display_mode;
    g_sel_art_display_mode_index = m_old_display_index;
    ArticlePageStateTest::TearDown();
}

void SubjectPageStateTest::prepare_groups(SelectionMode mode, bool rereading)
{
    prepare_articles(rereading, {AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_UNREAD, AF_EXISTS | AF_UNREAD,
                                 AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS | AF_UNREAD, AF_EXISTS, AF_EXISTS});
    g_sel.mode = mode;
    g_sel.sort = SS_DATE;
    m_subject.m_str = "Re: Alpha";
    m_bravo.m_str = "Re: Bravo";
    m_charlie.m_str = "Re: Charlie";
    m_subject.m_next = &m_bravo;
    m_bravo.m_prev = &m_subject;
    m_bravo.m_next = &m_charlie;
    m_charlie.m_prev = &m_bravo;
    g_subj.last = &m_charlie;
    m_bravo.m_articles = g_cache.ptr(ArticleNum{4});
    m_charlie.m_articles = g_cache.ptr(ArticleNum{7});
    m_bravo.m_thread = m_bravo.m_articles;
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_bravo.m_thread_link = &m_bravo;
    m_charlie.m_thread_link = &m_subject;
    const std::array<Subject *, 9> subjects{&m_subject, &m_subject, &m_subject, &m_bravo,  &m_bravo,
                                            &m_bravo,   &m_charlie, &m_charlie, &m_charlie};
    for (std::size_t i = 0; i < subjects.size(); ++i)
    {
        g_cache.ptr(ArticleNum{static_cast<long>(i + 1)})->m_subj = subjects[i];
    }
    g_cache.ptr(ArticleNum{3})->m_subj_next = nullptr;
    g_cache.ptr(ArticleNum{6})->m_subj_next = nullptr;
    g_cache.ptr(ArticleNum{1})->m_child1 = g_cache.ptr(ArticleNum{2});
    g_cache.ptr(ArticleNum{2})->m_parent = g_cache.ptr(ArticleNum{1});
    g_cache.ptr(ArticleNum{2})->m_sibling = g_cache.ptr(ArticleNum{3});
    g_cache.ptr(ArticleNum{3})->m_parent = g_cache.ptr(ArticleNum{1});
    g_cache.ptr(ArticleNum{3})->m_sibling = g_cache.ptr(ArticleNum{7});
    g_cache.ptr(ArticleNum{7})->m_parent = g_cache.ptr(ArticleNum{1});
    g_cache.ptr(ArticleNum{7})->m_child1 = g_cache.ptr(ArticleNum{8});
    g_cache.ptr(ArticleNum{8})->m_parent = g_cache.ptr(ArticleNum{7});
    g_cache.ptr(ArticleNum{8})->m_sibling = g_cache.ptr(ArticleNum{9});
    g_cache.ptr(ArticleNum{9})->m_parent = g_cache.ptr(ArticleNum{7});
    g_cache.ptr(ArticleNum{4})->m_child1 = g_cache.ptr(ArticleNum{5});
    g_cache.ptr(ArticleNum{5})->m_parent = g_cache.ptr(ArticleNum{4});
    g_cache.ptr(ArticleNum{5})->m_sibling = g_cache.ptr(ArticleNum{6});
    g_cache.ptr(ArticleNum{6})->m_parent = g_cache.ptr(ArticleNum{4});
    count_subjects(CS_UNSEL_STORE);
}

void SubjectPageStateTest::select_article(long number)
{
    g_cache.ptr(ArticleNum{number})->m_flags |= static_cast<ArticleFlags>(g_sel.mask);
    count_subjects(CS_UNSEL_STORE);
}

void SubjectPageStateTest::exclude_subject(Subject &subject)
{
    for (Article *article = subject.m_articles; article; article = article->m_subj_next)
    {
        if (g_sel.rereading)
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
    for (const Subject *subject = g_subj.first; subject; subject = subject->m_next)
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

struct SubjectDisplayRow
{
    int articles;
    int selected;
};

class SubjectPageDisplayState : public ArticlePageDisplayState
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        prepare_subjects(bool rereading, char mode, std::initializer_list<SubjectDisplayRow> rows);
    void        remove_subject(Subject &subject);
    void        rebuild_page();
    std::string heading(const Subject &subject, char label, char marker) const;

    Subject m_bravo{};
    Subject m_charlie{};

private:
    int         m_old_subject_count{g_subj.count};
    Subject    *m_old_next_subject{g_sel_page.next_subj};
    std::string m_old_input_charset{input_charset_name()};
    std::string m_old_output_charset{output_charset_name()};
};

void SubjectPageDisplayState::SetUp()
{
    ArticlePageDisplayState::SetUp();
    g_sel.mode = SM_SUBJECT;
    g_sel_page.next_subj = nullptr;
}

void SubjectPageDisplayState::TearDown()
{
    g_subj.count = m_old_subject_count;
    g_sel_page.next_subj = m_old_next_subject;
    utf_init(m_old_input_charset, m_old_output_charset);
    ArticlePageDisplayState::TearDown();
}

void SubjectPageDisplayState::prepare_subjects(bool rereading, char mode, std::initializer_list<SubjectDisplayRow> rows)
{
    g_sel.rereading = rereading;
    g_sel.mask = rereading ? AGF_DEL_SEL : AGF_SEL;
    g_sel_art_display_mode.assign(1, mode);
    g_sel_art_display_mode_index = 0;
    g_subj.count = static_cast<int>(rows.size());
    const std::array<Subject *, 3>   subjects{&m_subject, &m_bravo, &m_charlie};
    const std::array<std::string, 3> names{"Alpha", "Bravo", "Charlie"};
    std::size_t                      index = 0;
    long                             number = 0;
    Subject                         *previous = nullptr;
    for (const SubjectDisplayRow row : rows)
    {
        Subject &subject = *subjects[index];
        subject.m_str = "Re: " + names[index];
        subject.m_thread_link = &subject;
        subject.m_prev = previous;
        if (previous)
        {
            previous->m_next = &subject;
        }
        Article *last_article = nullptr;
        for (int i = 0; i <= row.articles; ++i)
        {
            Article   *article = g_cache.ptr(ArticleNum{++number});
            const bool in_scope = i < row.articles;
            article->m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
            if (in_scope != rereading)
            {
                article->m_flags |= AF_UNREAD;
            }
            if (i < row.selected)
            {
                article->m_flags |= static_cast<ArticleFlags>(g_sel.mask);
            }
            article->m_from = in_scope ? names[index] + " " + std::to_string(i + 1) : "Other scope";
            article->m_date = static_cast<std::time_t>(number * 86400);
            article->m_subj = &subject;
            if (last_article)
            {
                last_article->m_subj_next = article;
            }
            else
            {
                subject.m_articles = article;
                subject.m_thread = article;
            }
            last_article = article;
        }
        previous = &subject;
        ++index;
    }
    g_subj.first = rows.size() ? &m_subject : nullptr;
    g_subj.last = previous;
    g_last_art = ArticleNum{number};
    g_cache.last_cached = g_last_art;
    rebuild_page();
}

void SubjectPageDisplayState::remove_subject(Subject &subject)
{
    for (Article *article = subject.m_articles; article; article = article->m_subj_next)
    {
        if (g_sel.rereading)
        {
            article->m_flags |= AF_UNREAD;
        }
        else
        {
            article->m_flags &= ~AF_UNREAD;
        }
        article->m_flags &= ~(AF_SEL | AF_DEL_SEL);
    }
}

void SubjectPageDisplayState::rebuild_page()
{
    count_subjects(CS_UNSEL_STORE);
    init_pages(PRESERVE_PAGE);
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
}

std::string SubjectPageDisplayState::heading(const Subject &subject, char label, char marker) const
{
    std::string result{label, marker};
    if (current_article_display_mode() == 'd')
    {
        result += std::string{std::ctime(&subject.m_articles->m_date)}.substr(0, 16);
    }
    else if (current_article_display_mode() != 's')
    {
        const std::string author{subject.m_articles->from_view()};
        result += author + std::string(16 - author.size(), ' ');
    }
    const std::string count = std::to_string(subject.m_misc);
    return result + std::string(3 - count.size(), ' ') + count + "  " + subject.m_str.substr(4) + "\n";
}

class SubjectPageDisplayTest : public SubjectPageDisplayState,
                               public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class CompactSubjectPageDisplayTest : public SubjectPageDisplayState,
                                      public testing::WithParamInterface<std::tuple<bool, int, int>>
{
};

class SubjectPageBoundaryTest : public SubjectPageDisplayState, public testing::WithParamInterface<bool>
{
};

class ThreadPageDisplayState : public SubjectPageDisplayState
{
protected:
    void        prepare_threads(bool rereading, char mode, std::initializer_list<SubjectDisplayRow> rows,
                                bool first_thread_has_replies = true);
    void        link_thread(std::initializer_list<Subject *> subjects);
    std::string reply_heading(const Subject &subject) const;
};

void ThreadPageDisplayState::prepare_threads(bool rereading, char mode, std::initializer_list<SubjectDisplayRow> rows,
                                             bool first_thread_has_replies)
{
    prepare_subjects(rereading, mode, rows);
    g_sel.mode = SM_THREAD;
    g_threaded_group = true;
    if (rows.size() >= 2 && first_thread_has_replies)
    {
        link_thread({&m_subject, &m_bravo});
    }
    else if (rows.size() == 3)
    {
        link_thread({&m_bravo, &m_charlie});
    }
    g_sel_page.page_subj = nullptr;
    rebuild_page();
}

void ThreadPageDisplayState::link_thread(std::initializer_list<Subject *> subjects)
{
    Subject *first = *subjects.begin();
    Subject *previous = first;
    for (Subject *subject : subjects)
    {
        subject->m_thread = first->m_articles;
        previous->m_thread_link = subject;
        previous = subject;
    }
    previous->m_thread_link = first;
}

std::string ThreadPageDisplayState::reply_heading(const Subject &subject) const
{
    std::string result = heading(subject, ' ', ' ');
    result.insert(result.rfind(subject.m_str.substr(4)), ">");
    return result;
}

class ThreadPageDisplayTest : public ThreadPageDisplayState, public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class ThreadPageSelectionTest : public ThreadPageDisplayState, public testing::WithParamInterface<std::tuple<bool, int>>
{
};

struct TruncatedThread
{
    char mode;
    int  original_articles;
    int  reply_articles;
    int  lines;
    bool reply_visible;
};

class TruncatedThreadPageTest : public ThreadPageDisplayState,
                                public testing::WithParamInterface<std::tuple<bool, TruncatedThread>>
{
};

class ThreadPageBoundaryTest : public ThreadPageDisplayState, public testing::WithParamInterface<bool>
{
};

class ArticlePageLayoutTest : public ArticlePageDisplayState,
                              public testing::WithParamInterface<std::tuple<int, bool, char>>
{
};

class ArticlePageResizeTest : public ArticlePageDisplayState, public testing::WithParamInterface<bool>
{
};

class ArticlePageLabelCapacityTest : public SubjectPageDisplayState, public testing::WithParamInterface<bool>
{
};

class GroupedPageLayoutTest : public ThreadPageDisplayState,
                              public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
};

class NarrowGroupedPageTest : public ThreadPageDisplayState,
                              public testing::WithParamInterface<std::tuple<SelectionMode, char>>
{
};

class CompactThreadPageLayoutTest : public ThreadPageDisplayState, public testing::WithParamInterface<bool>
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
    EXPECT_EQ(static_cast<int>(expected.size()), g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{static_cast<long>(expected.size())}, g_obj_count);
    EXPECT_EQ(selected && test_case.included ? 1 : 0, g_selected_count);
    EXPECT_EQ(article_ptr_list_begin(), g_sel_page.page_art);
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
    EXPECT_EQ(static_cast<int>(expected.size()), g_sel_page.total_obj_cnt);
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
    g_sel.exclusive = true;

    init_pages(PRESERVE_PAGE);

    EXPECT_EQ((std::vector<long>{1, 2, 3, 4}), article_numbers());
    EXPECT_EQ((rereading ? std::vector<long>{2, 3} : std::vector<long>{1, 3}), articles_with_flag(AF_INCLUDED));
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_TRUE(g_sel.exclusive);
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
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(nullptr, g_sel_page.page_art);
}

TEST_P(ArticlePageReadingSelectionTest, selectedOnlyReadingStillListsUnselectedArticlesForSelection)
{
    const auto &[rereading, selected_only] = GetParam();
    g_sel.selected_only = selected_only;
    const ArticleFlags scope_flags = rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
    const ArticleFlags selected_flag = rereading ? AF_DEL_SEL : AF_SEL;
    prepare_articles(rereading, {scope_flags | selected_flag, scope_flags});

    init_pages(PRESERVE_PAGE);

    EXPECT_EQ((std::vector<long>{1, 2}), article_numbers());
    EXPECT_EQ((std::vector<long>{1, 2}), articles_with_flag(AF_INCLUDED));
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(selected_only, g_sel.selected_only);
    EXPECT_FALSE(g_sel.exclusive);
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
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(mode == SM_THREAD ? 2 : 3, g_sel_page.page_item_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
}

TEST_P(SubjectPageSelectedThreadTest, exclusiveDisplayIncludesTheWholeScopeContainingASelectedArticle)
{
    const auto &[mode, rereading, select_reply] = GetParam();
    prepare_groups(mode, rereading);
    const long selected = rereading ? (select_reply ? 8 : 2) : (select_reply ? 7 : 1);
    select_article(selected);
    g_sel.exclusive = true;

    initialize_page();

    const int expected_articles = mode == SM_THREAD ? (rereading ? 3 : 2) : (rereading && select_reply ? 2 : 1);
    EXPECT_EQ((mode == SM_THREAD || !select_reply ? std::vector<long>{1} : std::vector<long>{7}),
              subjects_with_flag(SF_INCLUDED));
    EXPECT_EQ(expected_articles, g_sel_page.total_obj_cnt);
    EXPECT_EQ(expected_articles, g_sel_page.page_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ((std::vector<long>{selected}), articles_with_flag(static_cast<ArticleFlags>(g_sel.mask)));
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_sel.exclusive);
}

TEST_P(SubjectPageAggregationTest, exclusiveDisplayExcludesUnselectedNeighboringThreads)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    select_article(rereading ? 6 : 4);
    g_sel.exclusive = true;

    initialize_page();

    EXPECT_EQ((std::vector<long>{4}), subjects_with_flag(SF_INCLUDED));
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_TRUE(g_sel.exclusive);
}

TEST_P(SubjectPageAggregationTest, unselectedReadSubjectsReceivePendingCleanupMarks)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    select_article(rereading ? 8 : 7);

    initialize_page();

    EXPECT_EQ((rereading ? std::vector<long>{1, 4} : std::vector<long>{}), subjects_with_flag(SF_DEL));
    EXPECT_EQ((std::vector<long>{7}), subjects_with_flag(static_cast<SubjectFlags>(g_sel.mask)));
    EXPECT_EQ((rereading ? std::vector<long>{8} : std::vector<long>{7}),
              articles_with_flag(static_cast<ArticleFlags>(g_sel.mask)));
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
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
}

TEST_P(SubjectPageAggregationTest, groupsWithNoEligibleArticlesProduceNoRows)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    exclude_subject(m_subject);
    exclude_subject(m_bravo);
    exclude_subject(m_charlie);

    initialize_page();

    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
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
    g_sel_page.page_art = article_ptr_list_begin() + 1;
    Article *anchor = *g_sel_page.page_art;
    g_sel.direction = -1;

    initialize_page();

    EXPECT_EQ((rereading ? std::vector<long>{9, 8, 6, 2} : std::vector<long>{7, 5, 4, 1}), article_numbers());
    ASSERT_EQ(article_ptr_list_begin() + 2, g_sel_page.page_art);
    EXPECT_EQ(anchor, *g_sel_page.page_art);
    EXPECT_EQ(2, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, removingTheAnchorAdvancesToTheNextEligibleArticle)
{
    const bool rereading = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    initialize_page();
    g_sel_page.page_art = article_ptr_list_begin() + 1;
    (*g_sel_page.page_art)->m_flags &= ~AF_EXISTS;
    count_subjects(CS_UNSEL_STORE);

    initialize_page();

    EXPECT_EQ((rereading ? std::vector<long>{2, 8, 9} : std::vector<long>{1, 5, 7}), article_numbers());
    ASSERT_EQ(article_ptr_list_begin() + 1, g_sel_page.page_art);
    EXPECT_EQ(ArticleNum{rereading ? 8 : 5}, (*g_sel_page.page_art)->article_num());
    EXPECT_EQ(1, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, removingEarlierArticlesKeepsTheAnchorAndRecountsItsPosition)
{
    const bool rereading = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    initialize_page();
    g_sel_page.page_art = article_ptr_list_begin() + 1;
    Article *anchor = *g_sel_page.page_art;
    exclude_subject(m_subject);

    initialize_page();

    ASSERT_EQ(article_ptr_list_begin(), g_sel_page.page_art);
    EXPECT_EQ(anchor, *g_sel_page.page_art);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, switchingFromSubjectsStartsWithTheAnchoredSubjectsFirstEligibleArticle)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    initialize_page();
    g_sel_page.page_subj = &m_bravo;
    g_sel.mode = SM_ARTICLE;

    initialize_page();

    ASSERT_EQ(article_ptr_list_begin() + 1, g_sel_page.page_art);
    EXPECT_EQ(ArticleNum{rereading ? 6 : 4}, (*g_sel_page.page_art)->article_num());
    EXPECT_EQ(1, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, anEmptySubjectAnchorAdvancesToTheNextEligibleSubject)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    g_tc_LINES = 6;
    initialize_page();
    exclude_subject(m_subject);

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page.page_subj);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
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

    EXPECT_EQ(&m_bravo, g_sel_page.page_subj);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, switchingToThreadsMapsAReplySubjectAnchorToTheThreadRow)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page.page_subj = &m_charlie;
    g_sel.mode = SM_THREAD;

    initialize_page();

    EXPECT_EQ(&m_subject, g_sel_page.page_subj);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 3 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
}

TEST_P(SelectorPageAnchorTest, fillingAnArticlePageBeforeTheEndKeepsItsAnchor)
{
    prepare_groups(SM_ARTICLE, GetParam());
    g_tc_LINES = 7;
    initialize_page();
    g_sel_page.page_art = article_ptr_list_begin() + 1;

    initialize_page(FILL_LAST_PAGE);

    EXPECT_EQ(article_ptr_list_begin() + 1, g_sel_page.page_art);
    EXPECT_EQ(1, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(article_ptr_list_end() - 1, g_sel_page.next_art);
}

TEST_P(SelectorPageAnchorTest, fillingASubjectPageBeforeTheEndKeepsItsAnchor)
{
    const bool rereading = GetParam();
    prepare_groups(SM_SUBJECT, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page.page_subj = &m_bravo;

    initialize_page(FILL_LAST_PAGE);

    EXPECT_EQ(&m_bravo, g_sel_page.page_subj);
    EXPECT_EQ(1, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(&m_charlie, g_sel_page.next_subj);
}

TEST_P(ArticlePageFillTest, theFillPolicyControlsWhetherTheFinalArticlePagePullsInEarlierRows)
{
    const auto &[rereading, fill_last_page] = GetParam();
    prepare_groups(SM_ARTICLE, rereading);
    g_tc_LINES = 7;
    initialize_page();
    g_sel_page.page_art = article_ptr_list_end() - 1;

    initialize_page(fill_last_page);

    EXPECT_EQ(article_ptr_list_end() - (fill_last_page ? 2 : 1), g_sel_page.page_art);
    EXPECT_EQ(fill_last_page ? 2 : 3, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(fill_last_page ? 2 : 1, g_sel_page.page_item_cnt);
    EXPECT_EQ(fill_last_page ? 2 : 1, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(article_ptr_list_end(), g_sel_page.next_art);
}

TEST_P(SubjectPageAnchorTest, reversingSortKeepsTheSameGroupAtThePageAnchor)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page.page_subj = &m_bravo;
    g_sel.direction = -1;

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page.page_subj);
    EXPECT_EQ(mode == SM_THREAD ? 0 : (rereading ? 2 : 1), g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
}

TEST_P(SubjectPageAnchorTest, removingEarlierArticlesKeepsTheGroupAnchorAndRecountsItsPosition)
{
    const auto &[mode, rereading] = GetParam();
    prepare_groups(mode, rereading);
    g_tc_LINES = 6;
    initialize_page();
    g_sel_page.page_subj = &m_bravo;
    exclude_subject(m_subject);

    initialize_page();

    EXPECT_EQ(&m_bravo, g_sel_page.page_subj);
    EXPECT_EQ(mode == SM_THREAD ? &m_bravo : &m_subject, g_subj.first);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(rereading ? 1 : 2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
}

TEST_P(SubjectPageFillTest, theFillPolicyControlsWhetherTheFinalGroupPagePullsInEarlierRows)
{
    const auto &[mode, rereading, fill_last_page] = GetParam();
    prepare_groups(mode, rereading);
    g_tc_LINES = 8;
    g_sel.direction = mode == SM_THREAD ? -1 : 1;
    initialize_page();
    g_sel_page.page_subj = mode == SM_THREAD ? &m_charlie : &m_bravo;

    initialize_page(fill_last_page);

    Subject *expected_anchor =
        fill_last_page ? (mode == SM_THREAD ? &m_bravo : &m_subject) : (mode == SM_THREAD ? &m_charlie : &m_bravo);
    const int earlier_articles = mode == SM_THREAD ? (rereading ? 1 : 2) : 1;
    EXPECT_EQ(expected_anchor, g_sel_page.page_subj);
    EXPECT_EQ(fill_last_page ? 0 : earlier_articles, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(fill_last_page ? 4 : 4 - earlier_articles, g_sel_page.page_obj_cnt);
    EXPECT_EQ((mode == SM_THREAD ? 1 : 2) + (fill_last_page ? 1 : 0), g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, SelectorPageAnchorTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(ReadScopeAndFillPolicy, ArticlePageFillTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeAndReadScope, SubjectPageAnchorTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeReadScopeAndFillPolicy, SubjectPageFillTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(), testing::Bool()));

TEST_P(ArticlePageDisplayTest, displaysArticleRowsWithoutChangingTheirReadingState)
{
    const auto [rereading, mode] = GetParam();
    prepare_display(rereading, mode);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_rows(mode, {1, 2, 3, 4})));
    EXPECT_THAT(output, testing::HasSubstr(rereading ? "5 read articles" : "5 articles"));
    EXPECT_THAT(output, testing::HasSubstr("comp.display"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Next page")));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    for (int index = 0; index < 4; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(g_cache.ptr(ArticleNum{index + 1}), g_sel_page.items[index].u.ap);
        EXPECT_EQ(index + 2, g_sel_page.items[index].line);
        EXPECT_EQ(index == 0 ? 1 : index == 1 ? 0 : index, g_sel_page.items[index].sel);
    }
    EXPECT_EQ(article_ptr_list_begin(), g_sel_page.page_art);
    ASSERT_EQ(article_ptr_list_begin() + 4, g_sel_page.next_art);
    EXPECT_EQ(g_cache.ptr(ArticleNum{7}), *g_sel_page.next_art);
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(7, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.last_art);
    EXPECT_EQ(nullptr, g_sel_page.last_subj);
    EXPECT_FALSE(g_sel_page.at_end);
    expect_article_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPresentation, ArticlePageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values('l', 'm', 's', 'd')));

TEST_P(ExclusiveArticlePageDisplayTest, skipsUnselectedArticlesWithoutAssigningThemRowLabels)
{
    const bool rereading = GetParam();
    prepare_display(rereading, 'l', true);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(expected_rows('l', {1, 4})));
    EXPECT_THAT(output, testing::HasSubstr(rereading ? "2 read articles out of 5" : "2 articles out of 5"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bob")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Carol")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Next page")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(g_cache.ptr(ArticleNum{1}), g_sel_page.items[0].u.ap);
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_sel_page.items[1].u.ap);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(3, g_sel_page.items[1].line);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(3, g_sel_page.items[1].sel);
    EXPECT_EQ(article_ptr_list_begin(), g_sel_page.page_art);
    EXPECT_EQ(article_ptr_list_end(), g_sel_page.next_art);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(5, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.last_art);
    EXPECT_TRUE(g_sel_page.at_end);
    expect_article_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScope, ExclusiveArticlePageDisplayTest, testing::Bool());

TEST_P(EmptyArticlePageDisplayTest, displaysNoRowsWhenThePopulationIsEmpty)
{
    const auto [rereading, populated] = GetParam();
    prepare_empty(rereading, populated);

    const std::string output = render_page();

    expect_empty(output, rereading);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPriorPopulation, EmptyArticlePageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(FinalArticlePageDisplayTest, endsAtTheLastArticleWithoutInventingAnotherPage)
{
    const auto [rereading, capacity] = GetParam();
    g_tc_LINES = capacity + 5;
    prepare_display(rereading, 'l');

    const std::string output = page_to_end();

    const int                 count = 5 % capacity ? 5 % capacity : capacity;
    const std::array<long, 5> numbers{1, 2, 3, 4, 7};
    EXPECT_THAT(output, testing::HasSubstr("Next page"));
    ASSERT_EQ(count, g_sel_page.page_item_cnt);
    EXPECT_EQ(count, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(5 - count, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(ArticleNum{5}, g_obj_count);
    for (int index = 0; index < count; ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(g_cache.ptr(ArticleNum{numbers[5 - count + index]}), g_sel_page.items[index].u.ap);
        EXPECT_EQ(index + 2, g_sel_page.items[index].line);
    }
    EXPECT_EQ(article_ptr_list_end() - count, g_sel_page.page_art);
    EXPECT_EQ(article_ptr_list_end(), g_sel_page.next_art);
    EXPECT_EQ(count + 3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
    expect_article_state();
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPageCapacity, FinalArticlePageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values(2, 3, 5)));

TEST_P(RemovedArticlePageDisplayTest, recoversAnEarlierPageAfterTheLastVisibleArticleLeavesTheReadScope)
{
    const bool rereading = GetParam();
    prepare_display(rereading, 'l');
    (void) page_to_end();
    g_cache.ptr(ArticleNum{7})->m_flags ^= AF_UNREAD;
    count_subjects(CS_UNSEL_STORE);

    init_pages(PRESERVE_PAGE);
    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Next page")));
    EXPECT_THAT(output, testing::HasSubstr("Alice"));
    EXPECT_THAT(output, testing::HasSubstr("Dan"));
    ASSERT_EQ(4, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    for (int index = 0; index < 4; ++index)
    {
        EXPECT_EQ(g_cache.ptr(ArticleNum{index + 1}), g_sel_page.items[index].u.ap);
    }
    EXPECT_EQ(article_ptr_list_begin(), g_sel_page.page_art);
    EXPECT_EQ(article_ptr_list_end(), g_sel_page.next_art);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(RemovedArticlePageDisplayTest, displaysAnEmptyPageAfterAllVisibleArticlesLeaveTheReadScope)
{
    const bool rereading = GetParam();
    prepare_display(rereading, 'l');
    (void) page_to_end();
    leave_read_scope();

    init_pages(PRESERVE_PAGE);
    const std::string output = render_page();

    expect_empty(output, rereading);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, RemovedArticlePageDisplayTest, testing::Bool());

TEST_P(SubjectPageDisplayTest, mapsMultiArticleSubjectsAndTheirSelectionMarkersToRows)
{
    const auto [rereading, mode] = GetParam();
    g_tc_LINES = 17;
    prepare_subjects(rereading, mode, {{3, 3}, {3, 1}, {2, 0}});
    g_sel_page.last_subj = &m_bravo;
    g_sel_page.item_index = -1;

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '+')));
    EXPECT_THAT(output, testing::HasSubstr(heading(m_bravo, 'b', '*')));
    EXPECT_THAT(output, testing::HasSubstr(heading(m_charlie, 'c', rereading ? '-' : ' ')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Other scope")));
    ASSERT_EQ(3, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(&m_bravo, g_sel_page.items[1].u.sp);
    EXPECT_EQ(&m_charlie, g_sel_page.items[2].u.sp);
    const int lines = mode == 'l' || mode == 'm' ? 3 : 1;
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(2 + lines, g_sel_page.items[1].line);
    EXPECT_EQ(2 + 2 * lines, g_sel_page.items[2].line);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(3, g_sel_page.items[1].sel);
    EXPECT_EQ(rereading ? 2 : 0, g_sel_page.items[2].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(8, g_sel_page.page_obj_cnt);
    EXPECT_EQ(8, g_sel_page.total_obj_cnt);
    EXPECT_EQ(4, g_selected_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
    EXPECT_EQ(mode == 'l' || mode == 'm' ? 11 : 6, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
}

TEST_P(SubjectPageDisplayTest, usesTheFirstEligibleArticleWhenTheOriginalLeavesTheReadScope)
{
    const auto [rereading, mode] = GetParam();
    prepare_subjects(rereading, mode, {{3, 1}});
    m_subject.m_articles->m_flags ^= AF_UNREAD;
    m_subject.m_articles->m_flags &= ~(AF_SEL | AF_DEL_SEL);
    rebuild_page();

    const std::string output = render_page();

    const char  marker = rereading ? '-' : ' ';
    std::string row{'a', marker};
    if (mode == 'd')
    {
        row += std::string{std::ctime(&m_subject.m_articles->m_subj_next->m_date)}.substr(0, 16);
    }
    else if (mode != 's')
    {
        row += "Alpha 2         ";
    }
    EXPECT_THAT(output, testing::HasSubstr(row + "  2  >Alpha\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha 1")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(mode == 'l' || mode == 'm' ? 5 : 4, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPresentation, SubjectPageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values('s', 'd', 'l', 'm')));

TEST_P(CompactSubjectPageDisplayTest, packsMiddleAuthorsWhileKeepingTheFinalAuthorOnItsOwnLine)
{
    const auto [rereading, articles, lines] = GetParam();
    g_tc_LINES = 17;
    prepare_subjects(rereading, 'm', {{articles, 1}, {1, 1}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    for (int i = 2; i <= articles; ++i)
    {
        const std::string author = "Alpha " + std::to_string(i);
        const std::size_t position = output.find(author);
        ASSERT_NE(std::string::npos, position) << output;
        EXPECT_EQ(std::string::npos, output.find(author, position + author.size()));
    }
    const std::string final_author = "Alpha " + std::to_string(articles);
    EXPECT_THAT(output, testing::HasSubstr("\n  " + final_author + std::string(16 - final_author.size(), ' ') + "\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Other scope")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_bravo, g_sel_page.items[1].u.sp);
    EXPECT_EQ(2 + lines, g_sel_page.items[1].line);
    EXPECT_EQ(articles + 1, g_sel_page.page_obj_cnt);
    EXPECT_EQ(lines + 4, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndAuthorPacking, CompactSubjectPageDisplayTest,
                         testing::Values(std::tuple{false, 4, 4}, std::tuple{true, 4, 4}, std::tuple{false, 5, 4},
                                         std::tuple{true, 5, 4}, std::tuple{false, 7, 5}, std::tuple{true, 7, 5},
                                         std::tuple{false, 8, 5}, std::tuple{true, 8, 5}));

TEST_P(SubjectPageBoundaryTest, defersAWholeSubjectWhenItsAuthorsDoNotFitTheRemainingLines)
{
    prepare_subjects(GetParam(), 'l', {{3, 1}, {2, 2}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr("  Alpha 2         \n  Alpha 3         \n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(&m_bravo, g_sel_page.next_subj);
    EXPECT_FALSE(g_sel_page.at_end);
}

TEST_P(SubjectPageBoundaryTest, displaysTheFinalSubjectAfterAdvancingPastAnEarlierPage)
{
    prepare_subjects(GetParam(), 'l', {{3, 1}, {2, 2}});
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_bravo, 'a', '+')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_bravo, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(5, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(SubjectPageBoundaryTest, truncatesAnOversizedSubjectWithoutLosingItsArticleCount)
{
    prepare_subjects(GetParam(), 'l', {{6, 1}, {1, 1}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr("Alpha 4"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha 5")));
    EXPECT_THAT(output, testing::HasSubstr("... etc. [6 lines total]"));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(6, g_sel_page.page_obj_cnt);
    EXPECT_EQ(7, g_sel_page.total_obj_cnt);
    EXPECT_EQ(&m_bravo, g_sel_page.next_subj);
    EXPECT_FALSE(g_sel_page.at_end);
}

TEST_P(SubjectPageBoundaryTest, endsWithoutAnotherPageWhenTheFinalSubjectExactlyFillsTheScreen)
{
    prepare_subjects(GetParam(), 'l', {{4, 1}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr("Alpha 4"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("etc.")));
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(7, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(SubjectPageBoundaryTest, exclusiveDisplayKeepsAllArticlesOfAPartiallySelectedSubject)
{
    prepare_subjects(GetParam(), 'l', {{3, 1}, {1, 0}, {1, 1}});
    g_sel.exclusive = true;
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr(heading(m_charlie, 'b', '+')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(&m_charlie, g_sel_page.items[1].u.sp);
    EXPECT_EQ(5, g_sel_page.items[1].line);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_TRUE(g_sel_page.at_end);
}

TEST_P(SubjectPageBoundaryTest, displaysNoRowsForAnEmptySubjectCatalog)
{
    prepare_subjects(GetParam(), 'l', {});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(GetParam() ? "0 read articles" : "0 articles"));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(SubjectPageBoundaryTest, recoversAnEarlierPageAfterTheFinalSubjectLeavesTheReadScope)
{
    prepare_subjects(GetParam(), 'l', {{3, 1}, {2, 2}});
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());
    (void) render_page();
    remove_subject(m_bravo);
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(SubjectPageBoundaryTest, displaysNoRowsAfterTheLastSubjectLeavesTheReadScope)
{
    prepare_subjects(GetParam(), 'l', {{3, 1}});
    (void) render_page();
    remove_subject(m_subject);
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

INSTANTIATE_TEST_SUITE_P(ReadScope, SubjectPageBoundaryTest, testing::Bool());

TEST_P(ThreadPageDisplayTest, groupsSubjectsUnderOneLabelAndAggregatesTheirSelection)
{
    const auto [rereading, mode] = GetParam();
    g_tc_LINES = 15;
    prepare_threads(rereading, mode, {{2, 2}, {2, 0}, {1, 1}});
    g_sel_page.last_subj = &m_charlie;
    g_sel_page.item_index = -1;

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr(reply_heading(m_bravo)));
    EXPECT_THAT(output, testing::HasSubstr(heading(m_charlie, 'b', '+')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Other scope")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(&m_charlie, g_sel_page.items[1].u.sp);
    EXPECT_EQ(3, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(mode == 'l' || mode == 'm' ? 6 : 4, g_sel_page.items[1].line);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(5, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_EQ(mode == 'l' || mode == 'm' ? 8 : 6, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
    EXPECT_TRUE(g_sel_page.at_end);
}

TEST_P(ThreadPageDisplayTest, labelsEligibleRepliesWhenTheOriginalSubjectHasNoEligibleArticles)
{
    const auto [rereading, mode] = GetParam();
    prepare_threads(rereading, mode, {{0, 0}, {2, 2}, {1, 1}});
    std::string reply = heading(m_bravo, 'a', '+');
    reply.insert(reply.rfind("Bravo"), ">");

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(reply));
    EXPECT_THAT(output, testing::HasSubstr(heading(m_charlie, 'b', '+')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(&m_charlie, g_sel_page.items[1].u.sp);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(mode == 'l' || mode == 'm' ? 4 : 3, g_sel_page.items[1].line);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_selected_count);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndPresentation, ThreadPageDisplayTest,
                         testing::Combine(testing::Bool(), testing::Values('s', 'd', 'l', 'm')));

TEST_P(ThreadPageSelectionTest, displaysAThreadWideMarkerForMatchingSubjectSelections)
{
    const auto [rereading, selected] = GetParam();
    prepare_threads(rereading, 's', {{2, selected}, {2, selected}});
    const char marker = selected == 2 ? '+' : selected ? '*' : rereading ? '-' : ' ';

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', marker)));
    EXPECT_THAT(output, testing::HasSubstr(reply_heading(m_bravo)));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(selected == 2 ? 1 : selected ? 3 : rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2 * selected, g_selected_count);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndSelectedCount, ThreadPageSelectionTest,
                         testing::Combine(testing::Bool(), testing::Values(0, 1, 2)));

TEST_P(TruncatedThreadPageTest, clipsVisibleRowsWhileCountingEveryArticleInTheThread)
{
    const auto [rereading, thread] = GetParam();
    prepare_threads(rereading, thread.mode, {{thread.original_articles, 1}, {thread.reply_articles, 1}, {1, 1}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_EQ(thread.reply_visible, output.find("Bravo 1") != std::string::npos);
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo 2")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Charlie")));
    EXPECT_THAT(output, testing::HasSubstr("... etc. [" + std::to_string(thread.lines) + " lines total]"));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(thread.original_articles + thread.reply_articles, g_sel_page.page_obj_cnt);
    EXPECT_EQ(thread.original_articles + thread.reply_articles + 1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(&m_charlie, g_sel_page.next_subj);
    EXPECT_FALSE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndTruncationPoint, TruncatedThreadPageTest,
                         testing::Combine(testing::Bool(), testing::Values(TruncatedThread{'l', 3, 3, 6, true},
                                                                           TruncatedThread{'l', 4, 2, 6, false},
                                                                           TruncatedThread{'m', 7, 2, 7, false})));

TEST_P(ThreadPageBoundaryTest, skipsAnEmptyReplySubjectWithoutConsumingARowOrLabel)
{
    prepare_threads(GetParam(), 'l', {{1, 1}, {0, 0}, {1, 1}});
    link_thread({&m_subject, &m_bravo, &m_charlie});
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '+')));
    EXPECT_THAT(output, testing::HasSubstr(reply_heading(m_charlie)));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
}

TEST_P(ThreadPageBoundaryTest, defersAllSubjectsOfAThreadWhenTheRemainingLinesAreInsufficient)
{
    prepare_threads(GetParam(), 'l', {{2, 1}, {2, 1}, {1, 1}}, false);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Charlie")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(&m_bravo, g_sel_page.next_subj);
    EXPECT_FALSE(g_sel_page.at_end);
}

TEST_P(ThreadPageBoundaryTest, displaysTheFinalThreadWithoutRepeatingSubjectsFromThePreviousPage)
{
    prepare_threads(GetParam(), 'l', {{2, 1}, {2, 1}, {1, 1}}, false);
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_bravo, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr(reply_heading(m_charlie)));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_bravo, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(6, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, advancesPastEverySubjectOfATruncatedThread)
{
    prepare_threads(GetParam(), 'l', {{3, 1}, {3, 1}, {1, 1}});
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_charlie, 'a', '+')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_charlie, g_sel_page.items[0].u.sp);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    EXPECT_EQ(6, g_sel_page.prior_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, endsWhenTheFinalThreadExactlyFillsTheScreen)
{
    prepare_threads(GetParam(), 'l', {{2, 1}, {2, 1}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(reply_heading(m_bravo)));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("etc.")));
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(7, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, displaysNoRowsForAnEmptyThreadCatalog)
{
    prepare_threads(GetParam(), 'l', {});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(GetParam() ? "0 read articles" : "0 articles"));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, displaysNoRowsWhenAllThreadArticlesAreOutsideTheReadScope)
{
    prepare_threads(GetParam(), 'l', {{0, 0}, {0, 0}});

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Other scope")));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, recoversAnEarlierPageAfterTheFinalThreadLeavesTheReadScope)
{
    prepare_threads(GetParam(), 'l', {{2, 1}, {2, 1}, {1, 1}}, false);
    (void) render_page();
    ASSERT_TRUE(g_sel_page.next_page());
    (void) render_page();
    remove_subject(m_bravo);
    remove_subject(m_charlie);
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Charlie")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, displaysNoRowsAfterTheLastThreadLeavesTheReadScope)
{
    prepare_threads(GetParam(), 'l', {{2, 1}, {2, 1}});
    (void) render_page();
    remove_subject(m_subject);
    remove_subject(m_bravo);
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.page_obj_cnt);
    EXPECT_EQ(0, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(3, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_FALSE(g_sel_page.next_page());
}

TEST_P(ThreadPageBoundaryTest, exclusiveDisplayOmitsEverySubjectOfAnUnselectedThread)
{
    prepare_threads(GetParam(), 'l', {{2, 0}, {2, 0}, {1, 1}});
    g_sel.exclusive = true;
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_charlie, 'a', '+')));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alpha")));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Bravo")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_charlie, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
}

TEST_P(ThreadPageBoundaryTest, exclusiveDisplayKeepsUnselectedSubjectsOfAPartiallySelectedThread)
{
    prepare_threads(GetParam(), 'l', {{2, 0}, {2, 1}, {1, 0}});
    g_sel.exclusive = true;
    rebuild_page();

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(heading(m_subject, 'a', '*')));
    EXPECT_THAT(output, testing::HasSubstr(reply_heading(m_bravo)));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Charlie")));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(4, g_sel_page.page_obj_cnt);
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(ReadScope, ThreadPageBoundaryTest, testing::Bool());

TEST_P(ArticlePageLayoutTest, fitsLongSubjectRowsToTheAvailableColumnsAndLabelWidth)
{
    const auto [columns, numeric, mode] = GetParam();
    prepare_display(false, mode);
    g_tc_COLS = columns;
    g_use_sel_num = numeric;
    m_subject.m_str = "Re: " + std::string(100, 'A');
    std::string row = numeric ? "01+" : "a+";
    const int   details = mode != 's' && columns >= 40 ? columns / 5 : 0;
    if (details && mode == 'd')
    {
        row += std::string{std::ctime(&m_subject.m_articles->m_date)}.substr(0, details);
    }
    else if (details)
    {
        row += "Alice" + std::string(details - 5, ' ');
    }
    const int subject_width = std::max(columns - 5 - numeric, 32) - details;
    row += "  " + std::string(subject_width, 'A');

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row + "\n"));
    EXPECT_LT(row.size(), static_cast<std::size_t>(columns));
    const int items = columns < 50 ? 3 : 4;
    ASSERT_EQ(items, g_sel_page.page_item_cnt);
    for (int i = 0; i < items; ++i)
    {
        EXPECT_EQ(g_cache.ptr(ArticleNum{i + 1}), g_sel_page.items[i].u.ap);
        EXPECT_EQ(i + 2, g_sel_page.items[i].line);
    }
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(items, g_sel_page.page_obj_cnt);
    EXPECT_EQ(items + 3, g_sel_page.last_line);
    EXPECT_FALSE(g_sel_page.at_end);
    expect_article_state();
}

INSTANTIATE_TEST_SUITE_P(ColumnsLabelsAndPresentation, ArticlePageLayoutTest,
                         testing::Combine(testing::Values(39, 40, 80), testing::Bool(),
                                          testing::Values('s', 'd', 'l')));

TEST_P(ArticlePageResizeTest, shrinkingTheTerminalRebuildsThePageAndRetainsVisibleArticleMappings)
{
    prepare_display(false, 'l');
    g_use_sel_num = GetParam();
    (void) render_page();
    g_tc_COLS = 39;
    g_tc_LINES = 8;
    init_pages(PRESERVE_PAGE);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(GetParam() ? "01+  Article eligibility\n" : "a+  Article eligibility\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Alice")));
    ASSERT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(g_cache.ptr(ArticleNum{1}), g_sel_page.items[0].u.ap);
    EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_sel_page.items[1].u.ap);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    EXPECT_EQ(3, g_sel_page.items[1].line);
    EXPECT_EQ(2, g_sel_page.page_obj_cnt);
    EXPECT_EQ(5, g_sel_page.total_obj_cnt);
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), *g_sel_page.next_art);
    EXPECT_FALSE(g_sel_page.at_end);
    expect_article_state();
}

TEST_P(ArticlePageResizeTest, enlargingTheTerminalShowsTheRemainingArticlesOnTheSamePage)
{
    prepare_display(false, 'l');
    g_use_sel_num = GetParam();
    g_tc_COLS = 39;
    g_tc_LINES = 8;
    g_sel_page.last_art = g_cache.ptr(ArticleNum{2});
    (void) render_page();
    g_tc_COLS = 80;
    g_tc_LINES = 11;
    init_pages(PRESERVE_PAGE);

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(GetParam() ? "05 Next page" : "e Next page"));
    ASSERT_EQ(5, g_sel_page.page_item_cnt);
    EXPECT_EQ(g_cache.ptr(ArticleNum{7}), g_sel_page.items[4].u.ap);
    EXPECT_EQ(6, g_sel_page.items[4].line);
    EXPECT_EQ(5, g_sel_page.page_obj_cnt);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(article_ptr_list_end(), g_sel_page.next_art);
    EXPECT_TRUE(g_sel_page.at_end);
    expect_article_state();
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, ArticlePageResizeTest, testing::Bool());

TEST_P(ArticlePageLabelCapacityTest, numericLabelsAllowMoreRowsThanTheConfiguredLetterAlphabet)
{
    prepare_subjects(false, 's', {{8, 1}});
    g_sel.mode = SM_ARTICLE;
    g_sel_page.page_art = nullptr;
    g_tc_LINES = 20;
    g_use_sel_num = GetParam();
    init_pages(PRESERVE_PAGE);

    const std::string output = render_page();

    const int items = GetParam() ? 8 : 6;
    ASSERT_EQ(items, g_sel_page.page_item_cnt);
    EXPECT_THAT(output, testing::HasSubstr(GetParam() ? "08   Alpha\n" : "f   Alpha\n"));
    EXPECT_EQ(items, g_sel_page.page_obj_cnt);
    EXPECT_EQ(8, g_sel_page.total_obj_cnt);
    EXPECT_EQ(items + 3, g_sel_page.last_line);
    EXPECT_EQ(GetParam(), g_sel_page.at_end);
    EXPECT_EQ(g_cache.ptr(ArticleNum{items}), g_sel_page.items[items - 1].u.ap);
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, ArticlePageLabelCapacityTest, testing::Bool());

TEST_P(GroupedPageLayoutTest, alignsAndTruncatesSubjectHeadingsWithLetterOrNumericThreadLabels)
{
    const auto [selection_mode, numeric, mode] = GetParam();
    g_tc_LINES = 12;
    prepare_threads(false, mode, {{2, 1}, {1, 1}});
    g_sel.mode = selection_mode;
    g_use_sel_num = numeric;
    m_subject.m_str = "Re: " + std::string(100, 'A');
    m_bravo.m_str = "Re: " + std::string(100, 'B');
    rebuild_page();
    const int   details = mode == 's' ? 0 : 16;
    std::string row = numeric ? "01*" : "a*";
    if (mode == 'd')
    {
        row += std::string{std::ctime(&m_subject.m_articles->m_date)}.substr(0, 16);
    }
    else if (details)
    {
        row += "Alpha 1         ";
    }
    const int subject_width = 72 - numeric - details;
    row += "  2  " + std::string(subject_width, 'A');

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(row + "\n"));
    EXPECT_EQ(79U, row.size());
    EXPECT_THAT(output, testing::HasSubstr(">" + std::string(subject_width - 1, 'B') + "\n"));
    if (mode == 'l' || mode == 'm')
    {
        EXPECT_THAT(output, testing::HasSubstr(numeric ? "   Alpha 2         \n" : "  Alpha 2         \n"));
    }
    ASSERT_EQ(selection_mode == SM_THREAD ? 1 : 2, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(2, g_sel_page.items[0].line);
    if (selection_mode == SM_SUBJECT)
    {
        EXPECT_THAT(output, testing::HasSubstr(numeric ? "02+" : "b+"));
        EXPECT_EQ(&m_bravo, g_sel_page.items[1].u.sp);
        EXPECT_EQ(mode == 'l' || mode == 'm' ? 4 : 3, g_sel_page.items[1].line);
    }
    EXPECT_EQ(3, g_sel_page.page_obj_cnt);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(SelectorLabelsAndPresentation, GroupedPageLayoutTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('s', 'd', 'l', 'm')));

TEST_P(NarrowGroupedPageTest, allocatesOneLinePerSubjectWhenTheScreenCannotShowAuthors)
{
    const auto [selection_mode, mode] = GetParam();
    prepare_threads(false, mode, {{4, 1}, {2, 1}, {1, 1}});
    g_sel.mode = selection_mode;
    g_tc_COLS = 39;
    rebuild_page();

    const std::string output = render_page();

    const std::string rows = selection_mode == SM_THREAD ? "a*  4  Alpha\n    2  >Bravo\nb+  1  Charlie\n"
                                                         : "a*  4  Alpha\nb*  2  >Bravo\nc+  1  Charlie\n";
    EXPECT_THAT(output, testing::HasSubstr(rows));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("etc.")));
    ASSERT_EQ(selection_mode == SM_THREAD ? 2 : 3, g_sel_page.page_item_cnt);
    EXPECT_EQ(&m_subject, g_sel_page.items[0].u.sp);
    EXPECT_EQ(&m_charlie, g_sel_page.items[g_sel_page.page_item_cnt - 1].u.sp);
    EXPECT_EQ(4, g_sel_page.items[g_sel_page.page_item_cnt - 1].line);
    EXPECT_EQ(7, g_sel_page.page_obj_cnt);
    EXPECT_EQ(7, g_sel_page.total_obj_cnt);
    EXPECT_EQ(6, g_sel_page.last_line);
    EXPECT_EQ(nullptr, g_sel_page.next_subj);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(SelectorAndAuthorPresentation, NarrowGroupedPageTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Values('l', 'm')));

TEST_P(CompactThreadPageLayoutTest, alignsPackedAuthorsAndUnlabelledReplySubjectsWithNumericLabels)
{
    g_tc_LINES = 15;
    prepare_threads(false, 'm', {{7, 1}, {1, 1}});
    g_use_sel_num = GetParam();
    rebuild_page();
    const std::string indent(GetParam() ? 3 : 2, ' ');
    const std::string packed = indent + "Alpha 3" + std::string(15, ' ') + indent + "Alpha 4" + std::string(15, ' ') +
                               indent + "Alpha 5" + std::string(15, ' ') + "\n";
    std::string       reply = reply_heading(m_bravo);
    if (GetParam())
    {
        reply.insert(0, " ");
    }

    const std::string output = render_page();

    EXPECT_THAT(output, testing::HasSubstr(packed));
    EXPECT_THAT(output, testing::HasSubstr(reply));
    EXPECT_THAT(output, testing::HasSubstr(indent + "Alpha 7         \n"));
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(8, g_sel_page.page_obj_cnt);
    EXPECT_EQ(9, g_sel_page.last_line);
    EXPECT_TRUE(g_sel_page.at_end);
}

INSTANTIATE_TEST_SUITE_P(LabelStyle, CompactThreadPageLayoutTest, testing::Bool());

TEST(SelectorPageTest, outputSelectorUsesConfiguredCharacterOrder)
{
    g_use_sel_num = false;
    g_sel_page.chars = "abc";

    testing::internal::CaptureStdout();
    output_sel(1, 0, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("b ", output);
}
