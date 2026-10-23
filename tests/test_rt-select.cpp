// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-select-internal.h>

#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/rt-page.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/score.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <initializer_list>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

void drain_input()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
}

class SelectorCommandInputTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_errno = errno;
        m_old_s_default_cmd = g_s_default_cmd;
        m_old_univ_default_cmd = g_univ_default_cmd;
        m_old_erase_char = g_erase_char;
        m_old_kill_char = g_kill_char;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_verify = g_verify;
        errno = 0;
        g_s_default_cmd = false;
        g_univ_default_cmd = false;
        g_erase_char = '\b';
        g_kill_char = Ctl('u');
        g_general_mode = GM_READ;
        g_mode = MM_NONE;
        g_verify = false;
        drain_input();
    }

    void TearDown() override
    {
        drain_input();
        errno = m_old_errno;
        g_s_default_cmd = m_old_s_default_cmd;
        g_univ_default_cmd = m_old_univ_default_cmd;
        g_erase_char = m_old_erase_char;
        g_kill_char = m_old_kill_char;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_verify = m_old_verify;
    }

    int         m_old_errno{};
    bool        m_old_s_default_cmd{};
    bool        m_old_univ_default_cmd{};
    char        m_old_erase_char{};
    char        m_old_kill_char{};
    GeneralMode m_old_general_mode{};
    MinorMode   m_old_mode{};
    bool        m_old_verify{};
};

class ArticleSelectorCommandTest : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_page(SelectionMode mode, bool rereading);
    void rebuild_page();

    Subject       m_subject{};
    NewsgroupData m_newsgroup{};

private:
    DataSource               m_data_source{};
    decltype(g_article_list) m_old_articles{std::move(g_article_list)};
    decltype(g_art_ptr_list) m_old_article_ptrs{std::move(g_art_ptr_list)};
    Article                **m_old_art_ptr{g_art_ptr};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first_art{g_first_art};
    ArticleNum               m_old_last_art{g_last_art};
    ArticleNum               m_old_first_cached{g_first_cached};
    ArticleNum               m_old_last_cached{g_last_cached};
    ArticleNum               m_old_parsed_art{g_parsed_art};
    ArticleNum               m_old_obj_count{g_obj_count};
    Subject                 *m_old_first_subject{g_first_subject};
    Subject                 *m_old_last_subject{g_last_subject};
    NewsgroupData           *m_old_newsgroup{g_newsgroup_ptr};
    DataSource              *m_old_data_source{g_data_source};
    bool                     m_old_cached_all{g_cached_all_in_range};
    bool                     m_old_threaded{g_threaded_group};
    bool                     m_old_thread_always{g_thread_always};
    bool                     m_old_rereading{g_sel_rereading};
    bool                     m_old_exclusive{g_sel_exclusive};
    bool                     m_old_selected_only{g_selected_only};
    bool                     m_old_group_listing{g_sel_group_listing};
    SelectionMode            m_old_mode{g_sel_mode};
    SelectionSortMode        m_old_sort{g_sel_sort};
    int                      m_old_direction{g_sel_direction};
    AddGroupFlags            m_old_mask{g_sel_mask};
    ArticleUnread            m_old_selected_count{g_selected_count};
    int                      m_old_selected_subj_cnt{g_selected_subj_cnt};
    Article                **m_old_page_app{g_sel_page_app};
    Article                **m_old_next_app{g_sel_next_app};
    Article                 *m_old_last_ap{g_sel_last_ap};
    Subject                 *m_old_page_sp{g_sel_page_sp};
    Subject                 *m_old_next_sp{g_sel_next_sp};
    Subject                 *m_old_last_sp{g_sel_last_sp};
    int                      m_old_total{g_sel_total_obj_cnt};
    int                      m_old_prior{g_sel_prior_obj_cnt};
    int                      m_old_page_objects{g_sel_page_obj_cnt};
    int                      m_old_page_items{g_sel_page_item_cnt};
    std::string              m_old_sel_chars{g_sel_chars};
    int                      m_old_lines{g_tc_LINES};
    int                      m_old_columns{g_tc_COLS};
    int                      m_old_mouse_width{g_mouse_bar_width};
    char                     m_old_interrupts{g_int_count};
    long                     m_old_spin_estimate{g_spin_estimate};
    long                     m_old_spin_todo{g_spin_todo};
    int                      m_old_spin_count{g_spin_count};
    char                     m_old_spin_char{g_spin_char};
    int                      m_old_term_line{g_term_line};
};

void ArticleSelectorCommandTest::SetUp()
{
    g_article_list.clear();
    g_art_ptr_list.clear();
    g_art_ptr = nullptr;
    g_abs_first = ArticleNum{1};
    g_first_art = ArticleNum{3};
    g_last_art = ArticleNum{4};
    g_first_cached = g_abs_first;
    g_last_cached = g_last_art;
    g_cached_all_in_range = true;
    g_first_subject = &m_subject;
    g_last_subject = &m_subject;
    g_newsgroup_ptr = &m_newsgroup;
    g_data_source = &m_data_source;
    g_threaded_group = true;
    g_thread_always = false;
    g_sel_exclusive = false;
    g_selected_only = true;
    g_sel_group_listing = false;
    g_sel_sort = SS_NATURAL;
    g_sel_direction = 1;
    g_sel_page_app = nullptr;
    g_sel_next_app = nullptr;
    g_sel_page_sp = nullptr;
    g_sel_next_sp = nullptr;
    g_sel_last_ap = nullptr;
    g_sel_last_sp = nullptr;
    g_int_count = 0;
    g_tc_LINES = 24;
    g_tc_COLS = 80;
    g_mouse_bar_width = 0;
    m_newsgroup.m_to_read = 2;
    m_subject.m_str = "Re: Selector mode";
    m_subject.m_thread_link = &m_subject;
    Article *previous = nullptr;
    for (long number = 1; number <= 4; ++number)
    {
        Article *article = article_ptr(ArticleNum{number});
        article->m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
        if (number >= 3)
        {
            article->m_flags |= AF_UNREAD;
        }
        article->m_subj = &m_subject;
        if (previous)
        {
            previous->m_subj_next = article;
        }
        previous = article;
    }
    m_subject.m_articles = article_ptr(ArticleNum{1});
    m_subject.m_thread = m_subject.m_articles;
}

void ArticleSelectorCommandTest::TearDown()
{
    g_article_list = std::move(m_old_articles);
    g_art_ptr_list = std::move(m_old_article_ptrs);
    g_art_ptr = m_old_art_ptr;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first_art;
    g_last_art = m_old_last_art;
    g_first_cached = m_old_first_cached;
    g_last_cached = m_old_last_cached;
    g_parsed_art = m_old_parsed_art;
    g_obj_count = m_old_obj_count;
    g_first_subject = m_old_first_subject;
    g_last_subject = m_old_last_subject;
    g_newsgroup_ptr = m_old_newsgroup;
    g_data_source = m_old_data_source;
    g_cached_all_in_range = m_old_cached_all;
    g_threaded_group = m_old_threaded;
    g_thread_always = m_old_thread_always;
    g_sel_rereading = m_old_rereading;
    g_sel_exclusive = m_old_exclusive;
    g_selected_only = m_old_selected_only;
    g_sel_group_listing = m_old_group_listing;
    g_sel_mode = m_old_mode;
    g_sel_sort = m_old_sort;
    g_sel_direction = m_old_direction;
    g_sel_mask = m_old_mask;
    g_selected_count = m_old_selected_count;
    g_selected_subj_cnt = m_old_selected_subj_cnt;
    g_sel_page_app = m_old_page_app;
    g_sel_next_app = m_old_next_app;
    g_sel_last_ap = m_old_last_ap;
    g_sel_page_sp = m_old_page_sp;
    g_sel_next_sp = m_old_next_sp;
    g_sel_last_sp = m_old_last_sp;
    g_sel_total_obj_cnt = m_old_total;
    g_sel_prior_obj_cnt = m_old_prior;
    g_sel_page_obj_cnt = m_old_page_objects;
    g_sel_page_item_cnt = m_old_page_items;
    g_sel_chars = m_old_sel_chars;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    g_mouse_bar_width = m_old_mouse_width;
    g_int_count = m_old_interrupts;
    g_spin_estimate = m_old_spin_estimate;
    g_spin_todo = m_old_spin_todo;
    g_spin_count = m_old_spin_count;
    g_spin_char = m_old_spin_char;
    g_term_line = m_old_term_line;
}

void ArticleSelectorCommandTest::prepare_page(SelectionMode mode, bool rereading)
{
    g_sel_mode = mode;
    g_sel_rereading = rereading;
    rebuild_page();
    // Both cursor forms can survive a prior selector mode change.
    g_sel_page_app = article_ptr_list_begin();
    g_sel_page_sp = &m_subject;
}

void ArticleSelectorCommandTest::rebuild_page()
{
    g_sel_mask = g_sel_rereading ? AGF_DEL_SEL : AGF_SEL;
    count_subjects(CS_UNSEL_STORE);
    init_pages(FILL_LAST_PAGE);
}

using ArticleSelectorToggle = std::tuple<SelectionMode, bool>;

class ArticleSelectorToggleTest : public ArticleSelectorCommandTest,
                                  public testing::WithParamInterface<ArticleSelectorToggle>
{
};

class ArticleSelectorModeTest : public ArticleSelectorCommandTest, public testing::WithParamInterface<SelectionMode>
{
};

class ArticleSelectorSortTest : public ArticleSelectorCommandTest
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    void              prepare_sort(SelectionMode mode, bool rereading = false, int direction = 1);
    DisplayState      run_command(char command, std::string_view input = {});
    std::vector<long> displayed_order() const;
    void              expect_selection_preserved(bool rereading) const;

    Subject     m_bravo{};
    Subject     m_charlie{};
    std::string m_output;

private:
    int               m_old_errno{errno};
    int               m_old_subject_count{g_subject_count};
    SelectionMode     m_old_thread_mode{g_sel_thread_mode};
    SelectionSortMode m_old_art_sort{g_sel_art_sort};
    SelectionSortMode m_old_thread_sort{g_sel_thread_sort};
    std::string_view  m_old_mode_string{g_sel_mode_string};
    std::string_view  m_old_sort_string{g_sel_sort_string};
    bool              m_old_sc_initialized{g_sc_initialized};
    bool              m_old_default_cmd{g_s_default_cmd};
    bool              m_old_univ_default_cmd{g_univ_default_cmd};
    bool              m_old_verify{g_verify};
    bool              m_old_verbose{g_verbose};
    bool              m_old_can_home{g_can_home};
    int               m_old_mouse_bar_cnt{g_mouse_bar_cnt};
    int               m_old_term_col{g_term_col};
    GeneralMode       m_old_general_mode{g_general_mode};
    MinorMode         m_old_minor_mode{g_mode};
    std::string_view  m_old_tc_cr{g_tc_CR};
    std::string_view  m_old_tc_ce{g_tc_CE};
};

void ArticleSelectorSortTest::SetUp()
{
    ArticleSelectorCommandTest::SetUp();
    drain_input();
    errno = 0;
    g_subject_count = 3;
    g_sc_initialized = true;
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    g_verify = false;
    g_verbose = false;
    g_can_home = false;
    g_mouse_bar_cnt = 0;
    g_general_mode = GM_SELECTOR;
    g_mode = MM_THREAD_SELECTOR;
    g_tc_CR = "\r";
    g_tc_CE = "";
    m_subject.m_str = "Re: Alpha";
    m_bravo.m_str = "Re: Bravo";
    m_charlie.m_str = "Re: Charlie";
    m_subject.m_next = &m_bravo;
    m_bravo.m_prev = &m_subject;
    m_bravo.m_next = &m_charlie;
    m_charlie.m_prev = &m_bravo;
    g_last_subject = &m_charlie;
    const std::array<Subject *, 4>    subjects{&m_subject, &m_bravo, &m_charlie, &m_subject};
    const std::array<std::time_t, 4>  dates{10, 30, 20, 40};
    const std::array<const char *, 4> authors{"Delta", "Charlie", "Bravo", "Alpha"};
    const std::array<long, 4>         lines{30, 10, 20, 40};
    const std::array<int, 4>          scores{20, 10, 50, 40};
    for (std::size_t i = 0; i < subjects.size(); ++i)
    {
        Article *article = article_ptr(ArticleNum{static_cast<long>(i + 1)});
        article->m_subj = subjects[i];
        article->m_subj_next = nullptr;
        article->m_date = dates[i];
        article->m_from = authors[i];
        article->m_lines = lines[i];
        article->m_score = scores[i];
        article->m_score_flags = SFLAG_SCORED;
        subjects[i]->m_thread_link = subjects[i];
        if (!subjects[i]->m_articles)
        {
            subjects[i]->m_articles = article;
            subjects[i]->m_thread = article;
        }
    }
    article_ptr(ArticleNum{1})->m_subj_next = article_ptr(ArticleNum{4});
}

void ArticleSelectorSortTest::TearDown()
{
    drain_input();
    errno = m_old_errno;
    g_subject_count = m_old_subject_count;
    g_sel_thread_mode = m_old_thread_mode;
    g_sel_art_sort = m_old_art_sort;
    g_sel_thread_sort = m_old_thread_sort;
    g_sel_mode_string = m_old_mode_string;
    g_sel_sort_string = m_old_sort_string;
    g_sc_initialized = m_old_sc_initialized;
    g_s_default_cmd = m_old_default_cmd;
    g_univ_default_cmd = m_old_univ_default_cmd;
    g_verify = m_old_verify;
    g_verbose = m_old_verbose;
    g_can_home = m_old_can_home;
    g_mouse_bar_cnt = m_old_mouse_bar_cnt;
    g_term_col = m_old_term_col;
    g_general_mode = m_old_general_mode;
    g_mode = m_old_minor_mode;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    ArticleSelectorCommandTest::TearDown();
}

void ArticleSelectorSortTest::prepare_sort(SelectionMode mode, bool rereading, int direction)
{
    const ArticleFlags selected = rereading ? AF_DEL_SEL : AF_SEL;
    for (auto &[number, article] : g_article_list)
    {
        article.m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
        if (!rereading)
        {
            article.m_flags |= AF_UNREAD;
        }
        if (number == ArticleNum{1} || number == ArticleNum{4})
        {
            article.m_flags |= selected;
        }
    }
    m_subject.m_flags = static_cast<SubjectFlags>(selected);
    m_newsgroup.m_to_read = rereading ? 0 : 4;
    set_selector(mode, static_cast<SelectionSortMode>(SS_DATE * direction));
    prepare_page(mode, rereading);
}

DisplayState ArticleSelectorSortTest::run_command(char command, std::string_view input)
{
    push_string(input, 0200);
    testing::internal::CaptureStdout();
    const DisplayState result = article_commands(command);
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

std::vector<long> ArticleSelectorSortTest::displayed_order() const
{
    std::vector<long> result;
    if (g_sel_mode == SM_ARTICLE)
    {
        for (const Article *article : g_art_ptr_list)
        {
            result.push_back(article->article_num().value_of());
        }
    }
    else
    {
        for (const Subject *subject = g_first_subject; subject; subject = subject->m_next)
        {
            result.push_back(subject->m_articles->article_num().value_of());
        }
    }
    return result;
}

void ArticleSelectorSortTest::expect_selection_preserved(bool rereading) const
{
    std::vector<long> selected;
    for (const auto &[number, article] : g_article_list)
    {
        if (article.m_flags & g_sel_mask)
        {
            selected.push_back(number.value_of());
        }
    }
    EXPECT_EQ((std::vector<long>{1, 4}), selected);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(rereading ? 0 : 4, m_newsgroup.m_to_read);
    EXPECT_EQ(rereading, g_sel_rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel_mask);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_THREAD_SELECTOR, g_mode);
}

using SelectorSortCase = std::tuple<char, SelectionSortMode, std::vector<long>>;
using SelectorSortParameters = std::tuple<SelectionMode, SelectorSortCase, bool>;

class ArticleSelectorSortCommandTest : public ArticleSelectorSortTest,
                                       public testing::WithParamInterface<SelectorSortParameters>
{
};

class ArticleSelectorReverseSortTest : public ArticleSelectorSortTest,
                                       public testing::WithParamInterface<std::tuple<SelectionMode, bool, int>>
{
};

class ArticleSelectorReadSortTest : public ArticleSelectorSortTest, public testing::WithParamInterface<SelectionMode>
{
};

class ArticleSelectorCancelSortTest : public ArticleSelectorSortTest,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class ArticleSelectorHelpSortTest : public ArticleSelectorSortTest,
                                    public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class ArticleSelectorExclusiveStateTest : public ArticleSelectorSortTest
{
protected:
    void              TearDown() override;
    void              prepare_exclusive(SelectionMode mode, bool rereading, bool exclusive);
    void              clear_selections();
    std::vector<long> included_items() const;
    void              expect_first_page(int total) const;

private:
    int m_old_item_index{g_sel_item_index};
};

void ArticleSelectorExclusiveStateTest::TearDown()
{
    g_sel_item_index = m_old_item_index;
    ArticleSelectorSortTest::TearDown();
}

void ArticleSelectorExclusiveStateTest::prepare_exclusive(SelectionMode mode, bool rereading, bool exclusive)
{
    prepare_sort(mode, rereading);
    g_sel_exclusive = exclusive;
    rebuild_page();
    if (mode == SM_ARTICLE)
    {
        g_sel_page_app = article_ptr_list_end() - 1;
    }
    else if (!exclusive)
    {
        g_sel_page_sp = &m_charlie;
    }
    g_sel_item_index = exclusive && mode != SM_ARTICLE ? 0 : 1;
}

void ArticleSelectorExclusiveStateTest::clear_selections()
{
    for (auto &[number, article] : g_article_list)
    {
        article.m_flags &= ~(AF_SEL | AF_DEL_SEL);
    }
    for (Subject *subject = g_first_subject; subject; subject = subject->m_next)
    {
        subject->m_flags &= ~(SF_SEL | SF_DEL_SEL);
    }
}

std::vector<long> ArticleSelectorExclusiveStateTest::included_items() const
{
    std::vector<long> result;
    if (g_sel_mode == SM_ARTICLE)
    {
        for (const Article *article : g_art_ptr_list)
        {
            if (article->m_flags & AF_INCLUDED)
            {
                result.push_back(article->article_num().value_of());
            }
        }
    }
    else
    {
        for (const Subject *subject = g_first_subject; subject; subject = subject->m_next)
        {
            if (subject->m_flags & SF_INCLUDED)
            {
                result.push_back(subject->m_articles->article_num().value_of());
            }
        }
    }
    return result;
}

void ArticleSelectorExclusiveStateTest::expect_first_page(int total) const
{
    EXPECT_EQ(total, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(0, g_sel_item_index);
    EXPECT_EQ(SS_DATE, g_sel_sort);
    EXPECT_EQ(1, g_sel_direction);
    if (g_sel_mode == SM_ARTICLE)
    {
        EXPECT_EQ(article_ptr_list_begin(), g_sel_page_app);
    }
    else
    {
        EXPECT_EQ(g_first_subject, g_sel_page_sp);
    }
}

class ArticleSelectorExclusiveToggleTest : public ArticleSelectorExclusiveStateTest,
                                           public testing::WithParamInterface<std::tuple<SelectionMode, bool, bool>>
{
};

class ArticleSelectorExclusivePopulationTest : public ArticleSelectorExclusiveStateTest,
                                               public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class ArticleSelectorExclusiveCleanupTest : public ArticleSelectorExclusiveStateTest,
                                            public testing::WithParamInterface<SelectionMode>
{
};

class ArticleSelectorExclusiveThreadTest : public ArticleSelectorExclusiveStateTest,
                                           public testing::WithParamInterface<bool>
{
};

class ArticleSelectorBulkStateTest : public ArticleSelectorExclusiveStateTest
{
protected:
    void       SetUp() override;
    void       TearDown() override;
    void       prepare_bulk(SelectionMode mode, bool rereading = false, bool selected = true);
    void       calculate_page();
    void       select_all();
    void       join_reply_thread();
    ArticleNum page_article() const;
    void       expect_articles(std::initializer_list<long> unread, std::initializer_list<long> selected) const;

private:
    bool        m_old_use_sel_num{g_use_sel_num};
    std::string m_old_display_mode{g_sel_art_display_mode};
    std::size_t m_old_display_index{g_sel_art_display_mode_index};
};

void ArticleSelectorBulkStateTest::SetUp()
{
    ArticleSelectorExclusiveStateTest::SetUp();
    g_tc_LINES = 6;
    g_use_sel_num = false;
    g_sel_art_display_mode = "s";
    g_sel_art_display_mode_index = 0;
}

void ArticleSelectorBulkStateTest::TearDown()
{
    g_use_sel_num = m_old_use_sel_num;
    g_sel_art_display_mode = m_old_display_mode;
    g_sel_art_display_mode_index = m_old_display_index;
    ArticleSelectorExclusiveStateTest::TearDown();
}

void ArticleSelectorBulkStateTest::prepare_bulk(SelectionMode mode, bool rereading, bool selected)
{
    prepare_sort(mode, rereading);
    if (!selected)
    {
        clear_selections();
    }
    g_sel_page_app = nullptr;
    g_sel_page_sp = nullptr;
    rebuild_page();
    calculate_page();
}

void ArticleSelectorBulkStateTest::calculate_page()
{
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
    g_sel_item_index = 0;
}

void ArticleSelectorBulkStateTest::select_all()
{
    for (auto &[number, article] : g_article_list)
    {
        article.m_flags |= static_cast<ArticleFlags>(g_sel_mask);
    }
    for (Subject *subject = g_first_subject; subject; subject = subject->m_next)
    {
        subject->m_flags |= static_cast<SubjectFlags>(g_sel_mask);
    }
    rebuild_page();
    calculate_page();
}

void ArticleSelectorBulkStateTest::join_reply_thread()
{
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_charlie.m_thread_link = &m_subject;
    g_sel_exclusive = true;
    g_sel_page_sp = nullptr;
    rebuild_page();
    calculate_page();
}

ArticleNum ArticleSelectorBulkStateTest::page_article() const
{
    return g_sel_mode == SM_ARTICLE ? (*g_sel_page_app)->article_num() : g_sel_page_sp->m_articles->article_num();
}

void ArticleSelectorBulkStateTest::expect_articles(std::initializer_list<long> unread,
                                                   std::initializer_list<long> selected) const
{
    std::vector<long> actual_unread;
    std::vector<long> actual_selected;
    for (const auto &[number, article] : g_article_list)
    {
        if (article.m_flags & AF_UNREAD)
        {
            actual_unread.push_back(number.value_of());
        }
        if (article.m_flags & g_sel_mask)
        {
            actual_selected.push_back(number.value_of());
        }
    }
    EXPECT_EQ(std::vector<long>(unread), actual_unread);
    EXPECT_EQ(std::vector<long>(selected), actual_selected);
    EXPECT_EQ(static_cast<ArticleUnread>(unread.size()), m_newsgroup.m_to_read);
    EXPECT_EQ(static_cast<ArticleUnread>(selected.size()), g_selected_count);
}

class ArticleSelectorBulkModeTest : public ArticleSelectorBulkStateTest,
                                    public testing::WithParamInterface<SelectionMode>
{
};

class ArticleSelectorBulkCommandTest : public ArticleSelectorBulkStateTest,
                                       public testing::WithParamInterface<std::tuple<SelectionMode, char>>
{
};

class ArticleSelectorBulkExclusiveTest : public ArticleSelectorBulkStateTest,
                                         public testing::WithParamInterface<std::tuple<SelectionMode, char>>
{
};

class ArticleSelectorBulkThreadTest : public ArticleSelectorBulkStateTest, public testing::WithParamInterface<char>
{
};

} // namespace

TEST_F(SelectorCommandInputTest, readsCommandText)
{
    push_char('x');

    const std::string command = read_selector_command_for_test('>', 'Z', false);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('x', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsPageDefaultCommand)
{
    push_char(' ');

    const std::string command = read_selector_command_for_test('>', 'Z', false);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('>', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsEndDefaultCommand)
{
    push_char(' ');

    const std::string command = read_selector_command_for_test('>', 'Z', true);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('Z', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsEscapedCommandCharacter)
{
    push_char('x');

    EXPECT_EQ('x', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsEscapedDefaultCommandCharacter)
{
    push_char('\\');

    EXPECT_EQ('\\', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsEscapedBlankCommandCharacter)
{
    push_char(' ');

    EXPECT_EQ(' ', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationDigit)
{
    push_char('7');

    EXPECT_EQ('7', read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationEraseCharacter)
{
    push_char(g_erase_char);

    EXPECT_EQ(g_erase_char, read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationKillCharacter)
{
    push_char(g_kill_char);

    EXPECT_EQ(g_kill_char, read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsPromptCommandText)
{
    push_char('s');

    testing::internal::CaptureStdout();
    const std::string command =
        read_selector_prompt_command_for_test("Selector mode?", MM_SELECTOR_ORDER_PROMPT, "tsa");
    testing::internal::GetCapturedStdout();

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('s', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsPromptDefaultCommand)
{
    push_char(' ');

    testing::internal::CaptureStdout();
    const std::string command =
        read_selector_prompt_command_for_test("Selector mode?", MM_SELECTOR_ORDER_PROMPT, "tsa");
    testing::internal::GetCapturedStdout();

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('t', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsFollowupCommandText)
{
    push_char('x');

    EXPECT_EQ('x', read_selector_followup_command_for_test(2));
}

TEST_F(SelectorCommandInputTest, finishesSelectorCommandText)
{
    push_string("topic\n", 0200);

    const std::string command = finish_selector_command_for_test('/', false);

    EXPECT_EQ("/topic", command);
}

TEST_P(ArticleSelectorToggleTest, togglesThePopulationAndRequestsARebuild)
{
    const auto &[mode, rereading] = GetParam();
    prepare_page(mode, rereading);

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(!rereading, g_sel_rereading);
    EXPECT_EQ(nullptr, g_sel_page_app);
    EXPECT_EQ(nullptr, g_sel_page_sp);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(2, m_newsgroup.m_to_read);
}

TEST_P(ArticleSelectorToggleTest, rebuildsThePageWithTheOtherPopulation)
{
    const auto &[mode, rereading] = GetParam();
    prepare_page(mode, rereading);

    ASSERT_EQ(DS_RESTART, article_commands('U'));
    rebuild_page();

    EXPECT_EQ(2, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    EXPECT_EQ(2, m_subject.m_misc);
    EXPECT_EQ(!rereading, g_sel_rereading);
    EXPECT_EQ(ArticleNum{rereading ? 3 : 1}, g_first_art);
    if (mode == SM_ARTICLE)
    {
        ASSERT_EQ(2U, g_art_ptr_list.size());
        EXPECT_EQ(ArticleNum{rereading ? 3 : 1}, g_art_ptr_list[0]->article_num());
        EXPECT_EQ(ArticleNum{rereading ? 4 : 2}, g_art_ptr_list[1]->article_num());
        EXPECT_EQ(article_ptr_list_begin(), g_sel_page_app);
    }
    else
    {
        EXPECT_EQ(&m_subject, g_sel_page_sp);
        EXPECT_TRUE(m_subject.m_flags & SF_INCLUDED);
    }
}

TEST_P(ArticleSelectorModeTest, commitsPendingReadMarksBeforeShowingReadArticles)
{
    prepare_page(GetParam(), false);
    if (GetParam() == SM_ARTICLE)
    {
        article_ptr(ArticleNum{3})->m_flags |= AF_DEL;
    }
    else
    {
        m_subject.m_flags |= SF_DEL;
    }
    article_ptr(ArticleNum{4})->m_flags |= AF_SEL;
    m_subject.m_flags |= SF_SEL;
    g_selected_count = 1;
    g_selected_subj_cnt = 1;

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_TRUE(g_sel_rereading);
    EXPECT_FALSE(article_ptr(ArticleNum{3})->m_flags & (AF_UNREAD | AF_DEL));
    EXPECT_TRUE(article_ptr(ArticleNum{4})->m_flags & AF_UNREAD);
    EXPECT_TRUE(article_ptr(ArticleNum{4})->m_flags & AF_SEL);
    EXPECT_EQ(1, m_newsgroup.m_to_read);
}

TEST_P(ArticleSelectorModeTest, restoresMarkedReadArticlesBeforeShowingUnreadArticles)
{
    prepare_page(GetParam(), true);
    article_ptr(ArticleNum{1})->m_flags |= AF_DEL_SEL;
    m_subject.m_flags |= SF_DEL_SEL;
    g_sel_last_ap = article_ptr(ArticleNum{1});
    g_sel_last_sp = &m_subject;

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_sel_rereading);
    EXPECT_TRUE(article_ptr(ArticleNum{1})->m_flags & AF_UNREAD);
    EXPECT_TRUE(article_ptr(ArticleNum{1})->m_flags & AF_SEL);
    EXPECT_FALSE(article_ptr(ArticleNum{1})->m_flags & AF_DEL_SEL);
    EXPECT_FALSE(article_ptr(ArticleNum{2})->m_flags & (AF_UNREAD | AF_DEL | AF_DEL_SEL));
    EXPECT_EQ(3, m_newsgroup.m_to_read);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_last_ap);
    EXPECT_EQ(nullptr, g_sel_last_sp);
}

TEST_P(ArticleSelectorModeTest, retainsUnreadModeWhenReadArticleCachingIsInterrupted)
{
    prepare_page(GetParam(), false);
    g_cached_all_in_range = false;
    // The current header is already parsed; interrupt the remaining cache work.
    g_parsed_art = ArticleNum{1};
    article_ptr(ArticleNum{1})->m_flags &= ~(AF_CACHED | AF_THREADED);
    g_int_count = 1;

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_sel_rereading);
    EXPECT_EQ(AGF_SEL, g_sel_mask);
    EXPECT_EQ(nullptr, g_sel_page_app);
    EXPECT_EQ(nullptr, g_sel_page_sp);
    EXPECT_FALSE(g_cached_all_in_range);
    EXPECT_LT(g_last_cached, g_last_art);
    EXPECT_EQ(0, g_int_count);
    EXPECT_EQ(2, m_newsgroup.m_to_read);
}

INSTANTIATE_TEST_SUITE_P(ModeAndPopulation, ArticleSelectorToggleTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, ArticleSelectorModeTest,
                         testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD));

TEST_P(ArticleSelectorSortCommandTest, ordersThePopulationWithoutChangingSelections)
{
    const auto &[mode, sort_case, reverse] = GetParam();
    const auto &[key, sort, ascending] = sort_case;
    prepare_sort(mode);
    const char        input = reverse ? static_cast<char>(std::toupper(static_cast<unsigned char>(key))) : key;
    std::vector<long> expected = ascending;
    if (reverse)
    {
        std::reverse(expected.begin(), expected.end());
    }

    const DisplayState result = run_command('O', std::string(1, input));

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(sort, g_sel_sort);
    EXPECT_EQ(reverse ? -1 : 1, g_sel_direction);
    EXPECT_EQ(expected, displayed_order());
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    if (mode == SM_ARTICLE)
    {
        EXPECT_EQ(article_ptr_list_begin(), g_sel_page_app);
    }
    else
    {
        EXPECT_EQ(g_first_subject, g_sel_page_sp);
    }
    expect_selection_preserved(false);
}

TEST_P(ArticleSelectorReverseSortTest, reversesTheCurrentOrderWithoutChangingSelections)
{
    const auto &[mode, rereading, direction] = GetParam();
    prepare_sort(mode, rereading, direction);
    std::vector<long> expected = displayed_order();
    std::reverse(expected.begin(), expected.end());

    const DisplayState result = run_command('R');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(SS_DATE, g_sel_sort);
    EXPECT_EQ(-direction, g_sel_direction);
    EXPECT_EQ(expected, displayed_order());
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    EXPECT_EQ(0, g_sel_prior_obj_cnt);
    expect_selection_preserved(rereading);
}

TEST_P(ArticleSelectorReadSortTest, reordersReadArticlesWithoutRestoringPendingSelections)
{
    prepare_sort(GetParam(), true);

    const DisplayState result = run_command('O', "S");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(GetParam(), g_sel_mode);
    EXPECT_EQ(SS_STRING, g_sel_sort);
    EXPECT_EQ(-1, g_sel_direction);
    EXPECT_EQ(GetParam() == SM_ARTICLE ? (std::vector<long>{3, 2, 4, 1}) : (std::vector<long>{3, 2, 1}),
              displayed_order());
    expect_selection_preserved(true);
}

TEST_F(ArticleSelectorSortTest, blankSortChoiceUsesForwardDateOrder)
{
    prepare_sort(SM_ARTICLE);
    set_selector(SM_ARTICLE, static_cast<SelectionSortMode>(-SS_STRING));
    rebuild_page();

    const DisplayState result = run_command('O', " ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(SS_DATE, g_sel_sort);
    EXPECT_EQ(1, g_sel_direction);
    EXPECT_EQ((std::vector<long>{1, 3, 2, 4}), displayed_order());
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    expect_selection_preserved(false);
}

TEST_P(ArticleSelectorCancelSortTest, cancellationRetainsTheOrderAndPageAnchor)
{
    const auto &[mode, can_home] = GetParam();
    prepare_sort(mode);
    g_can_home = can_home;
    const std::vector<long> before = displayed_order();
    Article **const         page_app = g_sel_page_app;
    Subject *const          page_sp = g_sel_page_sp;

    const DisplayState result = run_command('O', "q");

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(SS_DATE, g_sel_sort);
    EXPECT_EQ(1, g_sel_direction);
    EXPECT_EQ(before, displayed_order());
    EXPECT_EQ(page_app, g_sel_page_app);
    EXPECT_EQ(page_sp, g_sel_page_sp);
    expect_selection_preserved(false);
}

TEST_P(ArticleSelectorHelpSortTest, helpListsTheAvailableSortKeysAndReturnsToThePrompt)
{
    const auto &[mode, verbose] = GetParam();
    prepare_sort(mode);
    g_verbose = verbose;
    const std::vector<long> before = displayed_order();

    const DisplayState result = run_command('O', verbose ? "Hq" : "hq");

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(before, displayed_order());
    EXPECT_EQ(SS_DATE, g_sel_sort);
    EXPECT_EQ(1, g_sel_direction);
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type d or SP" : "d or SP sorts"));
    if (mode == SM_ARTICLE)
    {
        EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type a" : "a sorts"));
        EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type n" : "n sorts"));
        EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type g" : "g sorts"));
    }
    else
    {
        EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type c" : "c sorts"));
        EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type l" : "l sorts"));
    }
    EXPECT_NE(std::string::npos, m_output.find(verbose ? "Type q" : "q does nothing"));
    const std::size_t first_prompt = m_output.find("Order by");
    ASSERT_NE(std::string::npos, first_prompt);
    EXPECT_NE(std::string::npos, m_output.find("Order by", first_prompt + 1));
    expect_selection_preserved(false);
}

TEST_F(ArticleSelectorSortTest, sortingThreadsKeepsRelatedSubjectsTogether)
{
    prepare_sort(SM_THREAD);
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_charlie.m_thread_link = &m_subject;
    rebuild_page();

    const DisplayState result = run_command('O', "S");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ((std::vector<long>{2, 3, 1}), displayed_order());
    EXPECT_EQ(&m_charlie, m_subject.m_thread_link);
    EXPECT_EQ(&m_subject, m_charlie.m_thread_link);
    EXPECT_EQ(&m_bravo, m_bravo.m_thread_link);
    EXPECT_EQ(4, g_sel_total_obj_cnt);
    expect_selection_preserved(false);
}

INSTANTIATE_TEST_SUITE_P(ArticleKeys, ArticleSelectorSortCommandTest,
                         testing::Combine(testing::Values(SM_ARTICLE),
                                          testing::Values(SelectorSortCase{'d', SS_DATE, {1, 3, 2, 4}},
                                                          SelectorSortCase{'s', SS_STRING, {1, 4, 2, 3}},
                                                          SelectorSortCase{'a', SS_AUTHOR, {4, 3, 2, 1}},
                                                          SelectorSortCase{'n', SS_NATURAL, {1, 2, 3, 4}},
                                                          SelectorSortCase{'g', SS_GROUPS, {1, 4, 3, 2}},
                                                          SelectorSortCase{'p', SS_SCORE, {3, 4, 1, 2}}),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(SubjectAndThreadKeys, ArticleSelectorSortCommandTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD),
                                          testing::Values(SelectorSortCase{'d', SS_DATE, {1, 3, 2}},
                                                          SelectorSortCase{'s', SS_STRING, {1, 2, 3}},
                                                          SelectorSortCase{'c', SS_COUNT, {3, 2, 1}},
                                                          SelectorSortCase{'l', SS_LINES, {2, 3, 1}},
                                                          SelectorSortCase{'p', SS_SCORE, {3, 1, 2}}),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModePopulationAndDirection, ArticleSelectorReverseSortTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values(1, -1)));

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, ArticleSelectorReadSortTest,
                         testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD));

INSTANTIATE_TEST_SUITE_P(ModeAndTerminal, ArticleSelectorCancelSortTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeAndVerbosity, ArticleSelectorHelpSortTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

TEST_P(ArticleSelectorExclusiveToggleTest, togglesTheFilterAndRebuildsFromTheFirstItem)
{
    const auto &[mode, rereading, exclusive] = GetParam();
    prepare_exclusive(mode, rereading, exclusive);
    const std::vector<long> all_items = displayed_order();
    const std::vector<long> selected_items = mode == SM_ARTICLE ? std::vector<long>{1, 4} : std::vector<long>{1};

    const DisplayState result = run_command('E');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(!exclusive, g_sel_exclusive);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(exclusive ? all_items : selected_items, included_items());
    expect_first_page(exclusive ? 4 : 2);
    expect_selection_preserved(rereading);
}

TEST_P(ArticleSelectorExclusivePopulationTest, noSelectionsFallsBackToTheWholePopulation)
{
    const auto &[mode, rereading] = GetParam();
    prepare_exclusive(mode, rereading, false);
    clear_selections();
    rebuild_page();
    const std::vector<long> all_items = displayed_order();

    const DisplayState result = run_command('E');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_sel_exclusive);
    EXPECT_EQ(mode, g_sel_mode);
    EXPECT_EQ(all_items, included_items());
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(rereading, g_sel_rereading);
    EXPECT_EQ(rereading ? 0 : 4, m_newsgroup.m_to_read);
    expect_first_page(4);
}

TEST_P(ArticleSelectorExclusiveCleanupTest, commitsPendingReadMarksBeforeFiltering)
{
    prepare_exclusive(GetParam(), false, false);
    if (GetParam() == SM_ARTICLE)
    {
        article_ptr(ArticleNum{2})->m_flags |= AF_DEL;
    }
    else
    {
        m_bravo.m_flags |= SF_DEL;
    }

    const DisplayState result = run_command('E');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel_exclusive);
    EXPECT_FALSE(article_ptr(ArticleNum{2})->m_flags & (AF_UNREAD | AF_DEL));
    EXPECT_TRUE(article_ptr(ArticleNum{3})->m_flags & AF_UNREAD);
    EXPECT_TRUE(article_ptr(ArticleNum{1})->m_flags & AF_SEL);
    EXPECT_TRUE(article_ptr(ArticleNum{4})->m_flags & AF_SEL);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(3, m_newsgroup.m_to_read);
    EXPECT_EQ(GetParam() == SM_ARTICLE ? (std::vector<long>{1, 4}) : (std::vector<long>{1}), included_items());
    expect_first_page(2);
}

TEST_P(ArticleSelectorExclusiveThreadTest, selectedReplyIncludesTheWholeThread)
{
    const bool rereading = GetParam();
    prepare_exclusive(SM_THREAD, rereading, false);
    clear_selections();
    article_ptr(ArticleNum{3})->m_flags |= rereading ? AF_DEL_SEL : AF_SEL;
    m_charlie.m_flags |= rereading ? SF_DEL_SEL : SF_SEL;
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_charlie.m_thread_link = &m_subject;
    rebuild_page();

    const DisplayState result = run_command('E');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel_exclusive);
    EXPECT_EQ((std::vector<long>{1}), included_items());
    EXPECT_TRUE(m_subject.m_flags & SF_INCLUDED);
    EXPECT_FALSE(m_charlie.m_flags & SF_INCLUDED);
    EXPECT_FALSE(m_bravo.m_flags & SF_INCLUDED);
    EXPECT_TRUE(article_ptr(ArticleNum{3})->m_flags & g_sel_mask);
    EXPECT_FALSE(article_ptr(ArticleNum{1})->m_flags & g_sel_mask);
    EXPECT_FALSE(article_ptr(ArticleNum{4})->m_flags & g_sel_mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(rereading, g_sel_rereading);
    EXPECT_EQ(rereading ? 0 : 4, m_newsgroup.m_to_read);
    expect_first_page(3);
}

INSTANTIATE_TEST_SUITE_P(ModePopulationAndFilter, ArticleSelectorExclusiveToggleTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeAndPopulation, ArticleSelectorExclusivePopulationTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, ArticleSelectorExclusiveCleanupTest,
                         testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD));

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, ArticleSelectorExclusiveThreadTest, testing::Bool());

TEST_P(ArticleSelectorBulkModeTest, groupCommandMarksUnselectedItemsReadAcrossPages)
{
    prepare_bulk(GetParam());
    ASSERT_TRUE(next_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{3}, page_article());

    const DisplayState result = run_command('X');

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(GetParam(), g_sel_mode);
}

TEST_P(ArticleSelectorBulkModeTest, junkCommandMarksSelectedItemsReadAndRedisplaysSurvivors)
{
    prepare_bulk(GetParam());

    const DisplayState result = run_command('J');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2, 3}, {});
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(0, g_sel_item_index);
    EXPECT_EQ(ArticleNum{3}, page_article());
}

TEST_P(ArticleSelectorBulkModeTest, pageCommandPreservesUnselectedItemsOnOtherPages)
{
    prepare_bulk(GetParam());
    ASSERT_TRUE(next_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{3}, page_article());
    ASSERT_EQ(1, g_sel_page_item_cnt);

    const DisplayState result = run_command('D');

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 2, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorBulkModeTest, pageCommandWithoutSelectionsContinuesAtTheNextPage)
{
    prepare_bulk(GetParam(), false, false);
    ASSERT_TRUE(next_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{3}, page_article());

    const DisplayState result = run_command('D');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 4}, {});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(ArticleNum{2}, page_article());
    EXPECT_EQ(0, g_sel_item_index);
}

TEST_P(ArticleSelectorBulkModeTest, deletingTheLastPageRepairsThePageAnchor)
{
    prepare_bulk(GetParam(), false, false);
    ASSERT_TRUE(last_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{GetParam() == SM_ARTICLE ? 4 : 2}, page_article());

    const DisplayState result = run_command('D');

    EXPECT_EQ(DS_DISPLAY, result);
    if (GetParam() == SM_ARTICLE)
    {
        expect_articles({1, 2, 3}, {});
        EXPECT_EQ(ArticleNum{2}, page_article());
    }
    else
    {
        expect_articles({1, 3, 4}, {});
        EXPECT_EQ(ArticleNum{3}, page_article());
    }
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(0, g_sel_item_index);
}

TEST_P(ArticleSelectorBulkModeTest, junkCommandWithNoSelectionsLeavesUnreadArticlesAlone)
{
    prepare_bulk(GetParam(), false, false);

    const DisplayState result = run_command('J');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 3, 4}, {});
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorBulkModeTest, junkCommandAlsoCommitsPendingReadMarks)
{
    prepare_bulk(GetParam());
    if (GetParam() == SM_ARTICLE)
    {
        article_ptr(ArticleNum{3})->m_flags |= AF_DEL;
    }
    else
    {
        m_charlie.m_flags |= SF_DEL;
    }

    const DisplayState result = run_command('J');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2}, {});
    EXPECT_EQ(ArticleNum{1}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorBulkCommandTest, readingAllTargetedArticlesExitsTheSelector)
{
    const auto &[mode, command] = GetParam();
    g_tc_LINES = 24;
    prepare_bulk(mode, false, command == 'J');
    if (command == 'J')
    {
        select_all();
    }

    const DisplayState result = run_command(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({}, {});
    EXPECT_EQ(ArticleNum{}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorBulkCommandTest, readModeKeepsArticlesReadAndOnlyJunkClearsSelections)
{
    const auto &[mode, command] = GetParam();
    prepare_bulk(mode, true);

    const DisplayState result = run_command(command);

    EXPECT_EQ(command == 'J' ? DS_DISPLAY : DS_QUIT, result);
    if (command == 'J')
    {
        expect_articles({}, {});
        EXPECT_EQ(0, g_selected_subj_cnt);
    }
    else
    {
        expect_articles({}, {1, 4});
        EXPECT_EQ(1, g_selected_subj_cnt);
    }
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_TRUE(g_sel_rereading);
    EXPECT_EQ(AGF_DEL_SEL, g_sel_mask);
    EXPECT_EQ(mode, g_sel_mode);
}

TEST_P(ArticleSelectorBulkExclusiveTest, exclusiveCommandsPreserveHiddenUnselectedItems)
{
    const auto &[mode, command] = GetParam();
    prepare_bulk(mode);
    g_sel_exclusive = true;
    rebuild_page();
    calculate_page();

    const DisplayState result = run_command(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 2, 3, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_TRUE(g_sel_exclusive);
}

TEST_P(ArticleSelectorBulkThreadTest, exclusiveCommandsIncludeUnselectedRepliesInTheDisplayedThread)
{
    prepare_bulk(SM_THREAD);
    join_reply_thread();
    ASSERT_EQ(3, g_sel_page_obj_cnt);
    ASSERT_EQ(&m_bravo, g_sel_next_sp);

    const DisplayState result = run_command(GetParam());

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 2, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
}

TEST_F(ArticleSelectorBulkStateTest, junkCommandIncludesSelectedRepliesBelowAnUnselectedThreadRoot)
{
    prepare_bulk(SM_THREAD);
    clear_selections();
    article_ptr(ArticleNum{3})->m_flags |= AF_SEL;
    m_charlie.m_flags |= SF_SEL;
    join_reply_thread();

    const DisplayState result = run_command('J');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 4}, {});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_FALSE(g_sel_exclusive);
}

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, ArticleSelectorBulkModeTest,
                         testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD));

INSTANTIATE_TEST_SUITE_P(ModeAndCommand, ArticleSelectorBulkCommandTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD),
                                          testing::Values('X', 'D', 'J')));

INSTANTIATE_TEST_SUITE_P(ModeAndCommand, ArticleSelectorBulkExclusiveTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD),
                                          testing::Values('X', 'D')));

INSTANTIATE_TEST_SUITE_P(GroupAndPage, ArticleSelectorBulkThreadTest, testing::Values('X', 'D'));
