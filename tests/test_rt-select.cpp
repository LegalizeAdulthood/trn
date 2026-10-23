// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-select-internal.h>

#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/rt-page.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/univ.h>

#include <gtest/gtest.h>

#include <cerrno>
#include <tuple>
#include <utility>

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
