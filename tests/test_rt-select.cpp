// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-select-internal.h>

#include <trn/artsrch.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngstuff.h>
#include <trn/opt.h>
#include <trn/OptionCatalog.h>
#include <trn/rcstuff.h>
#include <trn/respond.h>
#include <trn/rt-page.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/score.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/util.h>

#include <config/fdio.h>

#ifdef I_UNISTD
#include <unistd.h>
#else
#include <fcntl.h>
#endif

#include <test_config.h>
#include <test_process_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <thread>
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
    decltype(g_cache.article_list) m_old_articles{std::move(g_cache.article_list)};
    decltype(g_art_order.ptr_list) m_old_article_ptrs{std::move(g_art_order.ptr_list)};
    Article                      **m_old_art_ptr{g_art_order.ptr};
    ArticleNum               m_old_abs_first{g_abs_first};
    ArticleNum               m_old_first_art{g_first_art};
    ArticleNum               m_old_last_art{g_last_art};
    ArticleNum                     m_old_first_cached{g_cache.first_cached};
    ArticleNum                     m_old_last_cached{g_cache.last_cached};
    ArticleNum               m_old_parsed_art{g_parsed_header.parsed_art};
    ArticleNum               m_old_obj_count{g_obj_count};
    Subject                       *m_old_first_subject{g_subj.first};
    Subject                       *m_old_last_subject{g_subj.last};
    NewsgroupData           *m_old_newsgroup{g_newsgroup_ptr};
    DataSource              *m_old_data_source{g_data_source};
    bool                           m_old_cached_all{g_cache.cached_all_in_range};
    bool                     m_old_threaded{g_threaded_group};
    bool                     m_old_thread_always{g_thread_always};
    bool                     m_old_rereading{g_sel.rereading};
    bool                     m_old_exclusive{g_sel.exclusive};
    bool                     m_old_selected_only{g_sel.selected_only};
    bool                     m_old_group_listing{g_sel.group_listing};
    SelectionMode            m_old_mode{g_sel.mode};
    SelectionSortMode        m_old_sort{g_sel.sort};
    int                      m_old_direction{g_sel.direction};
    AddGroupFlags            m_old_mask{g_sel.mask};
    ArticleUnread            m_old_selected_count{g_selected_count};
    int                      m_old_selected_subj_cnt{g_selected_subj_cnt};
    Article                **m_old_page_app{g_sel_page.page_art};
    Article                **m_old_next_app{g_sel_page.next_art};
    Article                 *m_old_last_ap{g_sel_page.last_art};
    Subject                 *m_old_page_sp{g_sel_page.page_subj};
    Subject                 *m_old_next_sp{g_sel_page.next_subj};
    Subject                 *m_old_last_sp{g_sel_page.last_subj};
    int                      m_old_total{g_sel_page.total_obj_cnt};
    int                      m_old_prior{g_sel_page.prior_obj_cnt};
    int                      m_old_page_objects{g_sel_page.page_obj_cnt};
    int                      m_old_page_items{g_sel_page.page_item_cnt};
    std::string              m_old_sel_chars{g_sel_page.chars};
    int                      m_old_lines{g_tc_LINES};
    int                      m_old_columns{g_tc_COLS};
    int                      m_old_mouse_width{g_mouse_bar_width};
    char                     m_old_interrupts{g_int_count};
    long                           m_old_spin_estimate{g_progress.estimate};
    long                           m_old_spin_todo{g_progress.todo};
    int                            m_old_spin_count{g_progress.count};
    char                           m_old_spin_char{g_progress.mark};
    int                      m_old_term_line{g_term_line};
};

void ArticleSelectorCommandTest::SetUp()
{
    g_cache.article_list.clear();
    g_art_order.ptr_list.clear();
    g_art_order.ptr = nullptr;
    g_abs_first = ArticleNum{1};
    g_first_art = ArticleNum{3};
    g_last_art = ArticleNum{4};
    g_cache.first_cached = g_abs_first;
    g_cache.last_cached = g_last_art;
    g_cache.cached_all_in_range = true;
    g_subj.first = &m_subject;
    g_subj.last = &m_subject;
    g_newsgroup_ptr = &m_newsgroup;
    g_data_source = &m_data_source;
    g_threaded_group = true;
    g_thread_always = false;
    g_sel.exclusive = false;
    g_sel.selected_only = true;
    g_sel.group_listing = false;
    g_sel.sort = SS_NATURAL;
    g_sel.direction = 1;
    g_sel_page.page_art = nullptr;
    g_sel_page.next_art = nullptr;
    g_sel_page.page_subj = nullptr;
    g_sel_page.next_subj = nullptr;
    g_sel_page.last_art = nullptr;
    g_sel_page.last_subj = nullptr;
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
        Article *article = g_cache.ptr(ArticleNum{number});
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
    m_subject.m_articles = g_cache.ptr(ArticleNum{1});
    m_subject.m_thread = m_subject.m_articles;
}

void ArticleSelectorCommandTest::TearDown()
{
    g_cache.article_list = std::move(m_old_articles);
    g_art_order.ptr_list = std::move(m_old_article_ptrs);
    g_art_order.ptr = m_old_art_ptr;
    g_abs_first = m_old_abs_first;
    g_first_art = m_old_first_art;
    g_last_art = m_old_last_art;
    g_cache.first_cached = m_old_first_cached;
    g_cache.last_cached = m_old_last_cached;
    g_parsed_header.parsed_art = m_old_parsed_art;
    g_obj_count = m_old_obj_count;
    g_subj.first = m_old_first_subject;
    g_subj.last = m_old_last_subject;
    g_newsgroup_ptr = m_old_newsgroup;
    g_data_source = m_old_data_source;
    g_cache.cached_all_in_range = m_old_cached_all;
    g_threaded_group = m_old_threaded;
    g_thread_always = m_old_thread_always;
    g_sel.rereading = m_old_rereading;
    g_sel.exclusive = m_old_exclusive;
    g_sel.selected_only = m_old_selected_only;
    g_sel.group_listing = m_old_group_listing;
    g_sel.mode = m_old_mode;
    g_sel.sort = m_old_sort;
    g_sel.direction = m_old_direction;
    g_sel.mask = m_old_mask;
    g_selected_count = m_old_selected_count;
    g_selected_subj_cnt = m_old_selected_subj_cnt;
    g_sel_page.page_art = m_old_page_app;
    g_sel_page.next_art = m_old_next_app;
    g_sel_page.last_art = m_old_last_ap;
    g_sel_page.page_subj = m_old_page_sp;
    g_sel_page.next_subj = m_old_next_sp;
    g_sel_page.last_subj = m_old_last_sp;
    g_sel_page.total_obj_cnt = m_old_total;
    g_sel_page.prior_obj_cnt = m_old_prior;
    g_sel_page.page_obj_cnt = m_old_page_objects;
    g_sel_page.page_item_cnt = m_old_page_items;
    g_sel_page.chars = m_old_sel_chars;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    g_mouse_bar_width = m_old_mouse_width;
    g_int_count = m_old_interrupts;
    g_progress.estimate = m_old_spin_estimate;
    g_progress.todo = m_old_spin_todo;
    g_progress.count = m_old_spin_count;
    g_progress.mark = m_old_spin_char;
    g_term_line = m_old_term_line;
}

void ArticleSelectorCommandTest::prepare_page(SelectionMode mode, bool rereading)
{
    g_sel.mode = mode;
    g_sel.rereading = rereading;
    rebuild_page();
    // Both cursor forms can survive a prior selector mode change.
    g_sel_page.page_art = g_art_order.begin();
    g_sel_page.page_subj = &m_subject;
}

void ArticleSelectorCommandTest::rebuild_page()
{
    g_sel.mask = g_sel.rereading ? AGF_DEL_SEL : AGF_SEL;
    count_subjects(CS_UNSEL_STORE);
    init_pages(FILL_LAST_PAGE);
}

class SelectorInputTest : public ArticleSelectorCommandTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_input(SelectionMode mode, bool rereading);
    char run_input(std::string_view commands);

    std::string m_output;

private:
    Subject                            m_second{};
    std::array<SelectionItem, MAX_SEL> m_old_items{};
    int                                m_old_term_scrolled{g_term_scrolled};
    int                                m_old_page_line{g_page_line};
    bool                               m_old_chase_phrase{g_output_chase_phrase};
    std::string_view                   m_old_tc_vb{g_tc_VB};
    int                                m_old_subject_count{g_subj.count};
    std::string_view                   m_old_tc_so{g_tc_SO};
    std::string_view                   m_old_tc_se{g_tc_SE};
    std::string_view                   m_old_tc_cr{g_tc_CR};
    std::string_view                   m_old_tc_ce{g_tc_CE};
    int                                m_old_errno{errno};
    int                                m_old_index{g_sel_page.item_index};
    int                                m_old_last_line{g_sel_page.last_line};
    int                                m_old_term_col{g_term_col};
    int                                m_old_mouse_count{g_mouse_bar_cnt};
    bool                               m_old_at_end{g_sel_page.at_end};
    bool                               m_old_numeric{g_use_sel_num};
    bool                               m_old_can_home{g_can_home};
    bool                               m_old_default{g_s_default_cmd};
    bool                               m_old_univ_default{g_univ_default_cmd};
    bool                               m_old_verify{g_verify};
    GeneralMode                        m_old_general_mode{g_general_mode};
    MinorMode                          m_old_minor_mode{g_mode};
    std::string_view                   m_old_mode_string{g_sel.mode_string};
    std::string_view                   m_old_sort_string{g_sel.sort_string};
};

void SelectorInputTest::SetUp()
{
    ArticleSelectorCommandTest::SetUp();
    g_tc_VB = "";
    g_subj.count = 2;
    g_tc_SO = "";
    g_tc_SE = "";
    g_tc_CR = "\r";
    g_tc_CE = "";
    std::copy(std::begin(g_sel_page.items), std::end(g_sel_page.items), m_old_items.begin());
    drain_input();
    errno = 0;
    g_sel_page.chars = "abc";
    g_use_sel_num = false;
    g_can_home = false;
    g_mouse_bar_cnt = 0;
    g_s_default_cmd = false;
    g_univ_default_cmd = false;
    g_verify = false;
    g_general_mode = GM_SELECTOR;
    g_mode = MM_THREAD_SELECTOR;
    g_sel.mode_string = "articles";
    g_sel.sort_string = "natural";
}

void SelectorInputTest::prepare_input(SelectionMode mode, bool rereading)
{
    g_cache.ptr(ArticleNum{1})->m_flags = ArticleFlags{};
    g_cache.ptr(ArticleNum{2})->m_flags = ArticleFlags{};
    Article *first = g_cache.ptr(ArticleNum{3});
    Article *second = g_cache.ptr(ArticleNum{4});
    m_subject.m_articles = first;
    m_subject.m_thread = first;
    m_subject.m_next = &m_second;
    first->m_subj_next = nullptr;
    second->m_subj = &m_second;
    m_second.m_str = "Re: Second topic";
    m_second.m_articles = second;
    m_second.m_thread = second;
    m_second.m_thread_link = &m_second;
    m_second.m_prev = &m_subject;
    g_subj.last = &m_second;
    if (rereading)
    {
        first->m_flags &= ~AF_UNREAD;
        second->m_flags &= ~AF_UNREAD;
        m_newsgroup.m_to_read = 0;
    }
    prepare_page(mode, rereading);
    g_sel_page.chars = "abc";
    g_sel_page.page_item_cnt = 2;
    g_sel_page.item_index = 0;
    g_sel_page.last_line = 10;
    g_sel_page.at_end = true;
    g_term_line = 0;
    for (int index = 0; index < 2; ++index)
    {
        g_sel_page.items[index] = {};
        g_sel_page.items[index].line = index + 1;
        if (mode == SM_ARTICLE)
        {
            g_sel_page.items[index].u.ap = index ? second : first;
        }
        else
        {
            g_sel_page.items[index].u.sp = index ? &m_second : &m_subject;
        }
    }
}

void SelectorInputTest::TearDown()
{
    drain_input();
    g_term_scrolled = m_old_term_scrolled;
    g_page_line = m_old_page_line;
    g_output_chase_phrase = m_old_chase_phrase;
    g_tc_VB = m_old_tc_vb;
    g_subj.count = m_old_subject_count;
    g_tc_SO = m_old_tc_so;
    g_tc_SE = m_old_tc_se;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    std::copy(m_old_items.begin(), m_old_items.end(), std::begin(g_sel_page.items));
    errno = m_old_errno;
    g_sel_page.item_index = m_old_index;
    g_sel_page.last_line = m_old_last_line;
    g_term_col = m_old_term_col;
    g_mouse_bar_cnt = m_old_mouse_count;
    g_sel_page.at_end = m_old_at_end;
    g_use_sel_num = m_old_numeric;
    g_can_home = m_old_can_home;
    g_s_default_cmd = m_old_default;
    g_univ_default_cmd = m_old_univ_default;
    g_verify = m_old_verify;
    g_general_mode = m_old_general_mode;
    g_mode = m_old_minor_mode;
    g_sel.mode_string = m_old_mode_string;
    g_sel.sort_string = m_old_sort_string;
    ArticleSelectorCommandTest::TearDown();
}

char SelectorInputTest::run_input(std::string_view commands)
{
    push_string(std::string(commands) + "q", 0200);
    testing::internal::CaptureStdout();
    const char result = g_sel_session.input();
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

class SelectorSessionInputState : public SelectorInputTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    char run_selector(std::string_view commands, bool rereading = false);
    void expect_next_command(char command);

private:
    ArticleLocation   m_old_target{g_nav.target};
    ArticleLocation   m_old_current{g_nav.current};
    ArticleNum        m_old_search_ahead{g_search_ahead};
    SelectionMode     m_old_default_mode{g_sel.default_mode};
    SelectionSortMode m_old_article_sort{g_sel.art_sort};
    std::string       m_old_commands{g_news_sel_cmds};
    std::string       m_old_display_mode{g_sel_art_display_mode};
    std::size_t       m_old_display_index{g_sel_art_display_mode_index};
    std::string       m_old_message{g_msg};
    std::string_view  m_old_home{g_tc_HO};
    std::string_view  m_old_up{g_tc_UP};
    std::string_view  m_old_motion{g_tc_CM};
    bool              m_old_bos{g_bos_on_stop};
    bool              m_old_in_ng{g_in_ng};
    bool              m_old_erase_screen{g_erase_screen};
    bool              m_old_erase_line{g_erase_each_line};
    char              m_old_erase{g_erase_char};
    char              m_old_kill{g_kill_char};
    int               m_old_added{g_added_articles};
    int               m_old_static{g_keep_the_group_static};
};

void SelectorSessionInputState::SetUp()
{
    SelectorInputTest::SetUp();
    g_nav.current.num = ArticleNum{3};
    g_nav.current.art = g_cache.ptr(g_nav.current.num);
    g_nav.target = g_nav.current;
    g_sel.default_mode = SM_ARTICLE;
    g_sel.art_sort = SS_NATURAL;
    g_news_sel_cmds = "Z>";
    g_sel_art_display_mode = "s";
    g_sel_art_display_mode_index = 0;
    g_tc_HO = "";
    g_tc_UP = "";
    g_tc_CM = "";
    g_erase_screen = false;
    g_erase_each_line = false;
    g_erase_char = Ctl('_');
    g_kill_char = Ctl('u');
    g_added_articles = 0;
    g_keep_the_group_static = 0;
    g_in_ng = true;
    prepare_page(SM_ARTICLE, false);
}

void SelectorSessionInputState::TearDown()
{
    g_nav.target = m_old_target;
    g_nav.current = m_old_current;
    g_search_ahead = m_old_search_ahead;
    g_sel.default_mode = m_old_default_mode;
    g_sel.art_sort = m_old_article_sort;
    g_news_sel_cmds = m_old_commands;
    g_sel_art_display_mode = m_old_display_mode;
    g_sel_art_display_mode_index = m_old_display_index;
    g_msg = m_old_message;
    g_tc_HO = m_old_home;
    g_tc_UP = m_old_up;
    g_tc_CM = m_old_motion;
    g_bos_on_stop = m_old_bos;
    g_in_ng = m_old_in_ng;
    g_erase_screen = m_old_erase_screen;
    g_erase_each_line = m_old_erase_line;
    g_erase_char = m_old_erase;
    g_kill_char = m_old_kill;
    g_added_articles = m_old_added;
    g_keep_the_group_static = m_old_static;
    SelectorInputTest::TearDown();
}

char SelectorSessionInputState::run_selector(std::string_view commands, bool rereading)
{
    push_string(std::string(commands) + "q", 0200);
    return article_selector(rereading ? 'U' : 'a');
}

void SelectorSessionInputState::expect_next_command(char command)
{
    ASSERT_TRUE(macro_pending());
    const std::string next = get_cmd();
    ASSERT_FALSE(next.empty());
    EXPECT_EQ(command, next.front());
    EXPECT_FALSE(macro_pending());
}

class SelectorCallbackTest : public SelectorSessionInputState
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    DisplayState dispatch(char command, std::string_view input = {});
    char         run_commands(std::string_view input);
    void         expect_next_command(char command);

private:
    void set_input(std::string_view input);

    TerminalInputReader m_old_reader{g_terminal_input_reader};
    std::string         m_input;
    std::size_t         m_input_index{};
    int                 m_old_dm_count{g_dm_count};
    bool                m_old_verbose{g_verbose};
};

void SelectorCallbackTest::SetUp()
{
    SelectorSessionInputState::SetUp();
    testing::internal::CaptureStdout();
    (void) run_selector("q");
    (void) testing::internal::GetCapturedStdout();
    drain_input();
    prepare_input(SM_ARTICLE, false);
    // This fixture has cached rows but no spool for idle prefetch.
    g_in_ng = false;
    g_dm_count = 0;
    g_verbose = true;
    g_term_line = g_sel_page.last_line + 1;
    g_terminal_input_reader = [this](char *buffer, int)
    {
        if (m_input_index == m_input.size())
        {
            ADD_FAILURE() << "Unexpected terminal read";
            *buffer = 'q';
        }
        else
        {
            *buffer = m_input[m_input_index++];
        }
        return 1;
    };
}

void SelectorCallbackTest::TearDown()
{
    g_terminal_input_reader = m_old_reader;
    g_dm_count = m_old_dm_count;
    g_verbose = m_old_verbose;
    SelectorSessionInputState::TearDown();
}

DisplayState SelectorCallbackTest::dispatch(char command, std::string_view input)
{
    set_input(input);
    testing::internal::CaptureStdout();
    const DisplayState result = g_sel_session.command(command);
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

void SelectorCallbackTest::set_input(std::string_view input)
{
    m_input = std::string(input) + 'q';
    m_input_index = 0;
}

char SelectorCallbackTest::run_commands(std::string_view input)
{
    set_input(input);
    testing::internal::CaptureStdout();
    const char result = g_sel_session.input();
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

void SelectorCallbackTest::expect_next_command(char command)
{
    ASSERT_EQ(m_input.size() - 1, m_input_index);
    EXPECT_EQ(command, read_tty_char());
    EXPECT_EQ(m_input.size(), m_input_index);
    EXPECT_FALSE(macro_pending());
}

TEST_F(SelectorCallbackTest, cancelledModePromptOnlyRequestsAnotherPrompt)
{
    Article **const page = g_sel_page.page_art;

    const DisplayState result = dispatch('S', "q");

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_EQ(page, g_sel_page.page_art);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_NE(std::string::npos, m_output.find("Selector mode:"));
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, unavailableYankReturnsItsStatusWithoutConsumingInput)
{
    const DisplayState result = dispatch('Y');

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("No marked articles to yank back.", g_msg);
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, cancelledMultilineMemorizeRequestsARowUpdateWithoutChangingSelection)
{
    const DisplayState result = dispatch('A', "hq");

    EXPECT_EQ(DS_UPDATE, result);
    EXPECT_EQ(ArticleNum{3}, g_nav.target.num);
    EXPECT_EQ(g_cache.ptr(g_nav.target.num), g_nav.target.art);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{3})->m_flags & AF_SEL);
    EXPECT_NE(std::string::npos, m_output.find("Memorize subject command:"));
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, reverseOrderRequestsRedisplayAndReversesOnlyOnce)
{
    const DisplayState result = dispatch('R');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(-1, g_sel.direction);
    ASSERT_EQ(2, g_art_order.ptr_list.size());
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_art_order.ptr_list.front());
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), g_art_order.ptr_list.back());
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, changingReadScopeRequestsASelectorRestart)
{
    const DisplayState result = dispatch('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_EQ(nullptr, g_sel_page.page_art);
    EXPECT_EQ(nullptr, g_sel_page.page_subj);
    expect_next_command('q');
}

class SelectorCallbackQuitTest : public SelectorCallbackTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorCallbackQuitTest, modeExitReturnsWithoutConsumingTheNextCommand)
{
    const DisplayState result = dispatch(GetParam());

    EXPECT_EQ(DS_QUIT, result);
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(NextPreviousAndScan, SelectorCallbackQuitTest, testing::Values('N', 'P', ';'));

TEST_F(SelectorCallbackTest, unknownCommandOnACleanScreenReturnsAHelpStatus)
{
    g_pushback.push('x' | 0200);

    const DisplayState result = dispatch('%');

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("Type ? for help.", g_msg);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_term_scrolled);
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, helpFollowupReversesOrderOnceWithoutConsumingTheNextCommand)
{
    const DisplayState result = dispatch('H', "R");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(-1, g_sel.direction);
    ASSERT_EQ(2, g_art_order.ptr_list.size());
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_art_order.ptr_list.front());
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), g_art_order.ptr_list.back());
    EXPECT_NE(std::string::npos, m_output.find("Selector Commands:"));
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, selectorLoopDisplaysTheModeStatusAndReadsTheFollowingExit)
{
    const char result = run_commands("Yq");

    EXPECT_EQ('Q', result);
    EXPECT_NE(std::string::npos, m_output.find("No marked articles to yank back."));
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, selectorLoopDisplaysUnknownCommandHelpAndReadsTheFollowingExit)
{
    const char result = run_commands("%q");

    EXPECT_EQ('Q', result);
    EXPECT_NE(std::string::npos, m_output.find("Type ? for help."));
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

TEST_F(SelectorCallbackTest, cancelledModePromptAfterHelpRedrawsTheSelectorBeforeReadingTheExit)
{
    g_in_ng = true;

    const char result = run_input("Shqq");

    EXPECT_EQ('Q', result);
    EXPECT_NE(std::string::npos, m_output.find("Type q to leave things as they are."));
    EXPECT_NE(std::string::npos, m_output.find("Second topic")) << m_output;
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_EQ(0, g_selected_count);
    SelectorSessionInputState::expect_next_command('q');
}

class SelectorCallbackSearchTest : public SelectorCallbackTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string    m_old_pattern{g_art_search.last_pat};
    CompiledRegex  m_old_regex{*g_art_search.bra_compex};
    ArtScope       m_old_scope{g_art_search.how_much};
    HeaderLineType m_old_header{g_art_search.srch_hdr};
    bool           m_old_do_read{g_art_search.do_read};
    bool           m_old_novice_delays{g_novice_delays};
    bool           m_old_error{g_error_occurred};
    bool           m_old_mouse_down{g_mouse_is_down};
    int            m_old_perform_count{g_status.perform_count};
    bool           m_old_performed_loop{g_status.performed_article_loop};
};

void SelectorCallbackSearchTest::SetUp()
{
    SelectorCallbackTest::SetUp();
    g_art_search.last_pat.clear();
    g_art_search.how_much = ARTSCOPE_SUBJECT;
    g_art_search.srch_hdr = SOME_LINE;
    g_art_search.do_read = false;
    g_novice_delays = false;
    g_error_occurred = false;
    g_status.perform_count = 0;
    search_init();
    g_art_search.init();
}

void SelectorCallbackSearchTest::TearDown()
{
    g_art_search.last_pat = m_old_pattern;
    *g_art_search.bra_compex = m_old_regex;
    g_art_search.how_much = m_old_scope;
    g_art_search.srch_hdr = m_old_header;
    g_art_search.do_read = m_old_do_read;
    g_novice_delays = m_old_novice_delays;
    g_error_occurred = m_old_error;
    g_mouse_is_down = m_old_mouse_down;
    g_status.perform_count = m_old_perform_count;
    g_status.performed_article_loop = m_old_performed_loop;
    SelectorCallbackTest::TearDown();
}

TEST_F(SelectorCallbackSearchTest, searchFollowupReversesOrderOnceWithoutConsumingTheNextCommand)
{
    const DisplayState result = dispatch('/', "\nR");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(-1, g_sel.direction);
    EXPECT_NE(std::string::npos, m_output.find("Null search string"));
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

TEST_F(SelectorCallbackSearchTest, invalidFollowupAfterSearchOutputRedrawsWithAHelpDiagnostic)
{
    const DisplayState result = dispatch('/', "\n\\\\\n");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ("Type ? for help.", g_msg);
    EXPECT_NE(std::string::npos, m_output.find("Null search string"));
    EXPECT_NE(std::string::npos, m_output.find("Type ? for help."));
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

TEST_F(SelectorCallbackSearchTest, commandAfterAScrolledErrorIsDispatchedOnceAndRetainsFollowingInput)
{
    const DisplayState result = dispatch('/', "\n\\\\R");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(-1, g_sel.direction);
    EXPECT_NE(std::string::npos, m_output.find("Type ? for help."));
    EXPECT_EQ(0, g_selected_count);
    expect_next_command('q');
}

class SelectorScrolledEscapeTest : public SelectorCallbackSearchTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorScrolledEscapeTest, blankEscapeAfterScrolledOutputRequestsRedisplayAndRetainsTheNextKey)
{
    const DisplayState result = dispatch('/', std::string("\n\\") + GetParam() + '\n');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find("Null search string"));
    EXPECT_NE(std::string::npos, m_output.find("Cmd: "));
    EXPECT_EQ(0, g_selected_count);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{3})->m_flags & AF_UNREAD);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{4})->m_flags & AF_UNREAD);
    expect_next_command('q');
}

TEST_P(SelectorScrolledEscapeTest, blankEscapeAfterScrolledOutputDispatchesTheFollowupCommand)
{
    const DisplayState result = dispatch('/', std::string("\n\\") + GetParam() + 'Y');

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("No marked articles to yank back.", g_msg);
    EXPECT_NE(std::string::npos, m_output.find("Null search string"));
    EXPECT_EQ(0, g_selected_count);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{3})->m_flags & AF_UNREAD);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{4})->m_flags & AF_UNREAD);
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(SpaceReturnAndNewline, SelectorScrolledEscapeTest, testing::Values(' ', '\r', '\n'));

class SelectorEscapedArticleTest : public SelectorCallbackTest
{
protected:
    void SetUp() override;
    void expect_article_page();
    char run_escaped_input(std::string_view commands);

private:
    std::array<ArticleFlags, 2> m_flags{};
    Article                   **m_page{};
};

void SelectorEscapedArticleTest::SetUp()
{
    SelectorCallbackTest::SetUp();
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
    g_cache.ptr(ArticleNum{4})->m_flags |= AF_DEL;
    g_sel_page.items[0].sel = 1;
    g_sel_page.items[1].sel = 2;
    g_sel_page.item_index = 1;
    m_flags = {g_cache.ptr(ArticleNum{3})->m_flags, g_cache.ptr(ArticleNum{4})->m_flags};
    m_page = g_sel_page.page_art;
}

void SelectorEscapedArticleTest::expect_article_page()
{
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(m_flags[0], g_cache.ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(m_flags[1], g_cache.ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(m_page, g_sel_page.page_art);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), g_sel_page.items[0].u.ap);
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_sel_page.items[1].u.ap);
}

char SelectorEscapedArticleTest::run_escaped_input(std::string_view commands)
{
    const std::string input = std::string(commands) + 'q';
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        g_pushback.push(static_cast<unsigned char>(*ch) | 0200);
    }
    g_in_ng = true;
    testing::internal::CaptureStdout();
    const char result = g_sel_session.input();
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

TEST_F(SelectorEscapedArticleTest, escapedModeCommandUsesTheArticleCallbackAndPreservesThePage)
{
    const DisplayState result = dispatch('\\', "Y");

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("No marked articles to yank back.", g_msg);
    EXPECT_NE(std::string::npos, m_output.find("Cmd: "));
    expect_article_page();
    expect_next_command('q');
}

TEST_F(SelectorEscapedArticleTest, doubleBackslashReportsTheArticleHandlersErrorWithoutReadingAnotherKey)
{
    const DisplayState result = dispatch('\\', "\\");

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("Type ? for help.", g_msg);
    expect_article_page();
    expect_next_command('q');
}

class SelectorEscapedExitTest : public SelectorEscapedArticleTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorEscapedExitTest, escapedEraseKeyUsesTheNormalSelectorExitPolicy)
{
    const DisplayState result = dispatch('\\', std::string(1, GetParam()));

    EXPECT_EQ(DS_QUIT, result);
    expect_article_page();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(BackspaceAndDelete, SelectorEscapedExitTest, testing::Values('\b', '\177'));

TEST_F(SelectorEscapedArticleTest, escapeOverridesASelectionLetterAndRunsTheCommandOnce)
{
    g_cache.ptr(ArticleNum{4})->m_flags &= ~AF_DEL;
    g_sel_page.items[1].sel = 0;
    g_sel_page.chars = "Rab";

    const char result = run_escaped_input("\\Rq");

    EXPECT_EQ('Q', result);
    EXPECT_EQ(-1, g_sel.direction);
    ASSERT_EQ(2, g_art_order.ptr_list.size());
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_art_order.ptr_list.front());
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), g_art_order.ptr_list.back());
    EXPECT_EQ(1, g_selected_count);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{3})->m_flags & AF_SEL);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{4})->m_flags & AF_SEL);
    const std::size_t second = m_output.find("Second topic");
    ASSERT_NE(std::string::npos, second);
    EXPECT_NE(std::string::npos, m_output.find("Selector mode", second));
    SelectorSessionInputState::expect_next_command('q');
}

class SelectorInformationTest : public SelectorEscapedArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_reading_context();
    void expect_version();

private:
    std::string m_old_version{g_patch_level};
    Multirc    *m_old_source{g_multirc};
    std::string m_old_mail{g_mail_call};
#ifdef MAIL_CALL
    int m_old_mail_count{g_mail_count};
#endif
};

void SelectorInformationTest::SetUp()
{
    g_mail_call.clear();
#ifdef MAIL_CALL
    g_mail_count = 1;
#endif
    SelectorEscapedArticleTest::SetUp();
    g_patch_level = "selector-test-version";
    g_multirc = nullptr;
}

void SelectorInformationTest::TearDown()
{
    g_patch_level = m_old_version;
    g_multirc = m_old_source;
    g_mail_call = m_old_mail;
#ifdef MAIL_CALL
    g_mail_count = m_old_mail_count;
#endif
    SelectorEscapedArticleTest::TearDown();
}

void SelectorInformationTest::expect_reading_context()
{
    expect_article_page();
    EXPECT_EQ(ArticleNum{3}, g_nav.target.num);
    EXPECT_EQ(g_cache.ptr(g_nav.target.num), g_nav.target.art);
    EXPECT_EQ(g_nav.target.num, g_nav.current.num);
    EXPECT_EQ(g_nav.target.art, g_nav.current.art);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_EQ(SS_NATURAL, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_THREAD_SELECTOR, g_mode);
    EXPECT_EQ(nullptr, g_multirc);
}

void SelectorInformationTest::expect_version()
{
    const std::size_t version = m_output.find("Trn version: selector-test-version.");
    ASSERT_NE(std::string::npos, version);
    EXPECT_EQ(std::string::npos, m_output.find("Trn version:", version + 1));
#ifdef HAS_LOCAL_SPOOL
    EXPECT_NE(std::string::npos, m_output.find("Configured for both NNTP and local news access."));
#else
    EXPECT_NE(std::string::npos, m_output.find("Configured for NNTP (plus individual local access)."));
#endif
    EXPECT_NE(std::string::npos, m_output.find("trn-users@lists.sourceforge.net"));
    EXPECT_NE(std::string::npos, m_output.find("trn-workers@lists.sourceforge.net"));
    EXPECT_NE(std::string::npos, m_output.find("[Type space or a command]"));
}

class SelectorVersionDismissalTest : public SelectorInformationTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorVersionDismissalTest, dismissingVersionInformationRequestsTheUnchangedArticlePage)
{
    const DisplayState result = dispatch('v', std::string(1, GetParam()));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_version();
    expect_reading_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(SpaceReturnAndNewline, SelectorVersionDismissalTest, testing::Values(' ', '\r', '\n'));

TEST_F(SelectorInformationTest, aCommandAtTheVersionPauseUsesTheArticleCallback)
{
    const DisplayState result = dispatch('v', "Y");

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("No marked articles to yank back.", g_msg);
    expect_version();
    expect_reading_context();
    expect_next_command('q');
}

class SelectorVersionQuitTest : public SelectorInformationTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorVersionQuitTest, quittingAtTheVersionPauseUsesTheSelectorExitPolicy)
{
    const DisplayState result = dispatch('v', std::string(1, GetParam()));

    EXPECT_EQ(DS_QUIT, result);
    expect_version();
    expect_reading_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(LowerAndUpperCase, SelectorVersionQuitTest, testing::Values('q', 'Q'));

TEST_F(SelectorInformationTest, aSelectionLetterAtTheVersionPauseIsReturnedToTheSelectorInputLoop)
{
    const DisplayState result = dispatch('v', "a");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_version();
    expect_reading_context();
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('a', read_tty_char() & 0177);
    expect_next_command('q');
}

TEST_F(SelectorInformationTest, closingVersionInformationRedisplaysRowsAndAcceptsTheNextCommand)
{
    g_in_ng = true;

    const char result = run_input("v q");

    EXPECT_EQ('Q', result);
    expect_version();
    const std::size_t pause = m_output.find("[Type space or a command]");
    EXPECT_NE(std::string::npos, m_output.find("Second topic", pause));
    expect_reading_context();
    SelectorSessionInputState::expect_next_command('q');
}

#ifdef MAIL_CALL
enum class SelectorMailboxState
{
    Missing,
    Empty,
    Read,
    Unread
};

class SelectorMailTest : public SelectorInformationTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_mail(SelectorMailboxState state);

    trn::testing::MockEnvironment m_env;

private:
    std::filesystem::path m_root;
    std::filesystem::path m_mailbox;
};

void SelectorMailTest::SetUp()
{
    EXPECT_CALL(m_env.getter, Call(testing::_)).WillRepeatedly(testing::Invoke(std::getenv));
    const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
    m_root = std::filesystem::path{TRN_TEST_TMP_DIR} / info->test_suite_name() / info->name();
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    ASSERT_FALSE(error) << error.message();
    std::filesystem::create_directories(m_root);
    m_mailbox = m_root / "mailbox with spaces";
    m_env.expect_env_repeatedly("MAILFILE", m_mailbox.generic_string().c_str());
    m_env.expect_no_envar("MAILCALL");
    SelectorInformationTest::SetUp();
}

void SelectorMailTest::TearDown()
{
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    EXPECT_FALSE(error) << error.message();
    SelectorInformationTest::TearDown();
}

void SelectorMailTest::prepare_mail(SelectorMailboxState state)
{
    if (state != SelectorMailboxState::Missing)
    {
        std::ofstream(m_mailbox, std::ios::binary) << (state == SelectorMailboxState::Empty ? "" : "New mail\n");
        const std::chrono::hours age{state == SelectorMailboxState::Read ? -1 : 1};
        std::filesystem::last_write_time(m_mailbox, std::filesystem::file_time_type::clock::now() + age);
    }
    g_mail_count = 3;
    g_mail_call = "Stale mail notice ";
}

struct SelectorMailCase
{
    const char          *name;
    SelectorMailboxState state;
    const char          *configured;
    const char          *expected;
};

void PrintTo(const SelectorMailCase &value, std::ostream *output)
{
    *output << value.name;
}

class SelectorMailRefreshTest : public SelectorMailTest, public testing::WithParamInterface<SelectorMailCase>
{
};

TEST_P(SelectorMailRefreshTest, anExplicitMailRequestRefreshesCachedStatusWithoutChangingTheArticlePage)
{
    const SelectorMailCase &value = GetParam();
    prepare_mail(value.state);
    if (value.configured)
    {
        m_env.expect_env_repeatedly("MAILCALL", value.configured);
    }

    const DisplayState result = dispatch(Ctl('^'));

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(value.expected, g_mail_call);
    EXPECT_EQ(1, g_mail_count);
    expect_reading_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(
    MailboxAndNotice, SelectorMailRefreshTest,
    testing::Values(SelectorMailCase{"Missing", SelectorMailboxState::Missing, nullptr, ""},
                    SelectorMailCase{"Empty", SelectorMailboxState::Empty, nullptr, ""},
                    SelectorMailCase{"AlreadyRead", SelectorMailboxState::Read, nullptr, ""},
                    SelectorMailCase{"NewMail", SelectorMailboxState::Unread, nullptr, "(Mail) "},
                    SelectorMailCase{"Configured", SelectorMailboxState::Unread, "[Mail %V] ", "[Mail %V] "},
                    SelectorMailCase{"EmptyUsesDefault", SelectorMailboxState::Unread, "", "(Mail) "}));

TEST_F(SelectorMailTest, aForcedMailRefreshDisplaysTheInterpolatedNoticeBeforeAcceptingAnotherCommand)
{
    prepare_mail(SelectorMailboxState::Unread);
    m_env.expect_env_repeatedly("MAILCALL", "[Mail %V] ");
    g_in_ng = true;

    const char result = run_input(std::string(1, Ctl('^')) + 'q');

    EXPECT_EQ('Q', result);
    const std::size_t stale = m_output.find("Stale mail notice ");
    ASSERT_NE(std::string::npos, stale);
    EXPECT_NE(std::string::npos, m_output.find("[Mail selector-test-version] -- Select articles", stale + 1));
    EXPECT_EQ(std::string::npos, m_output.find("Stale mail notice ", stale + 1));
    expect_reading_context();
    SelectorSessionInputState::expect_next_command('q');
}

TEST_F(SelectorMailTest, aForcedMailRefreshRemovesAStaleNoticeBeforeAcceptingAnotherCommand)
{
    prepare_mail(SelectorMailboxState::Missing);
    g_in_ng = true;

    const char result = run_input(std::string(1, Ctl('^')) + 'q');

    EXPECT_EQ('Q', result);
    const std::size_t stale = m_output.find("Stale mail notice ");
    ASSERT_NE(std::string::npos, stale);
    EXPECT_NE(std::string::npos, m_output.find("-- Select articles", stale + 1));
    EXPECT_EQ(std::string::npos, m_output.find("Stale mail notice ", stale + 1));
    EXPECT_TRUE(g_mail_call.empty());
    expect_reading_context();
    SelectorSessionInputState::expect_next_command('q');
}
#endif

class SelectorShellTest : public SelectorCallbackTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string shell_command(bool output = false, bool fail = false) const;
    std::string read_file(const std::filesystem::path &path) const;
    void        expect_directory(const std::filesystem::path &expected);
    void        expect_selector_preserved();
    void        expect_one_shell();

    std::filesystem::path m_start;
    std::filesystem::path m_private;
    std::filesystem::path m_root;
    trn::testing::MockEnvironment m_env;

private:
    using SignalHandler = void (*)(int);
    std::vector<std::pair<int, SignalHandler>> m_signals;
    std::filesystem::path                      m_old_directory{std::filesystem::current_path()};
    std::string                                m_old_private{g_priv_dir};
    bool                                       m_old_bizarre{g_bizarre};
    bool                                       m_old_check{g_check_flag};
    bool                                       m_old_waiting{g_waiting};
    bool                                       m_old_no_wait{g_no_wait_fork};
    bool                                       m_old_one_command{g_one_command};
    bool                                       m_old_mouse_down{g_mouse_is_down};
    std::string_view                           m_old_backspace{g_tc_BC};
    int                                        m_old_perform_count{g_status.perform_count};
    int                                        m_old_mail_count{g_mail_count};
    std::array<ArticleFlags, 2>                m_flags{};
    Article                                  **m_page{};
    ArticleLocation                            m_article_location{};
};

void SelectorShellTest::SetUp()
{
    EXPECT_CALL(m_env.getter, Call(testing::_)).WillRepeatedly(testing::Invoke(std::getenv));
    SelectorCallbackTest::SetUp();
    const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
    m_root = std::filesystem::path{TRN_TEST_TMP_DIR} / info->test_suite_name() / info->name();
    m_start = m_root / "spool dir";
    m_private = m_root / "private dir";
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    ASSERT_FALSE(error) << error.message();
    std::filesystem::create_directories(m_start);
    std::filesystem::create_directories(m_private);
    std::ofstream(m_start / "shell-input", std::ios::binary) << "Controlled selector shell output.\n";
    std::ofstream(m_private / "shell-input", std::ios::binary) << "Controlled selector shell output.\n";
    std::filesystem::current_path(m_start);
    g_priv_dir = m_private.generic_string();
    g_check_flag = false;
    g_waiting = g_no_wait_fork = g_one_command = false;
    g_status.perform_count = 0;
    for (const int number : {
             SIGINT,
#ifdef SIGQUIT
             SIGQUIT,
#endif
#ifdef SIGTSTP
             SIGTSTP,
             SIGTTOU,
             SIGTTIN,
#endif
         })
    {
        m_signals.emplace_back(number, std::signal(number, SIG_IGN));
    }
    m_env.expect_no_envars({"NNTPSERVER", "NNTP_FORCE_AUTH", "NEWSACTIVE", "NEWSDESCRIPTIONS", "QUOTECHARS"});
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", m_root.generic_string().c_str());
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "");
    m_env.expect_env_repeatedly("SHELL", TRN_TEST_PROCESS_TOOL);
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
    g_cache.ptr(ArticleNum{4})->m_flags |= AF_DEL;
    g_sel_page.items[0].sel = 1;
    g_sel_page.items[1].sel = 2;
    g_sel_page.item_index = 1;
    m_flags = {g_cache.ptr(ArticleNum{3})->m_flags, g_cache.ptr(ArticleNum{4})->m_flags};
    m_page = g_sel_page.page_art;
    m_article_location = g_nav.target;
    g_term_line = g_sel_page.last_line;
    g_term_col = 0;
    g_tc_BC = "\b";
}

void SelectorShellTest::TearDown()
{
    for (const std::pair<int, SignalHandler> &signal : m_signals)
    {
        std::signal(signal.first, signal.second);
    }
    g_priv_dir = m_old_private;
    g_bizarre = m_old_bizarre;
    g_check_flag = m_old_check;
    g_waiting = m_old_waiting;
    g_no_wait_fork = m_old_no_wait;
    g_one_command = m_old_one_command;
    g_mouse_is_down = m_old_mouse_down;
    g_tc_BC = m_old_backspace;
    g_status.perform_count = m_old_perform_count;
    g_mail_count = m_old_mail_count;
    std::error_code error;
    std::filesystem::current_path(m_old_directory, error);
    EXPECT_FALSE(error) << error.message();
    std::filesystem::remove_all(m_root, error);
    EXPECT_FALSE(error) << error.message();
    SelectorCallbackTest::TearDown();
}

std::string SelectorShellTest::shell_command(bool output, bool fail) const
{
#ifdef USE_SPAWN_SHELL
    return std::string(output ? "type shell-input & " : "") + "cd > shell-cwd" + (fail ? " & exit 7" : "");
#else
    return std::string(output ? "cat shell-input; " : "") + "pwd > shell-cwd" + (fail ? "; exit 7" : "");
#endif
}

std::string SelectorShellTest::read_file(const std::filesystem::path &path) const
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void SelectorShellTest::expect_directory(const std::filesystem::path &expected)
{
    std::string directory = read_file(expected / "shell-cwd");
    directory.erase(directory.find_last_not_of("\r\n") + 1);
    ASSERT_FALSE(directory.empty());
    EXPECT_TRUE(std::filesystem::equivalent(expected, directory));
    EXPECT_TRUE(std::filesystem::equivalent(m_start, std::filesystem::current_path()));
}

void SelectorShellTest::expect_selector_preserved()
{
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_EQ(m_article_location, g_nav.target);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(m_flags[0], g_cache.ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(m_flags[1], g_cache.ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(m_page, g_sel_page.page_art);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), g_sel_page.items[0].u.ap);
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_sel_page.items[1].u.ap);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(2, g_sel_page.items[1].sel);
    EXPECT_FALSE(g_one_command);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_THREAD_SELECTOR, g_mode);
}

void SelectorShellTest::expect_one_shell()
{
#ifdef USE_SPAWN_SHELL
    constexpr std::string_view role = "/c";
#else
    constexpr std::string_view role = "-c";
#endif
    EXPECT_EQ(role, read_file(m_root / "shell-used"));
    const std::string log = read_file(m_root / "tools.log");
    const std::string invocation = std::string(role) + '\n';
    EXPECT_EQ(0, log.find(invocation));
    EXPECT_EQ(std::string::npos, log.find(invocation, invocation.size()));
    EXPECT_EQ(1, g_status.perform_count);
}

class SelectorShellCancelTest : public SelectorShellTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorShellCancelTest, erasingTheEscapeReturnsToThePromptWithoutStartingAShell)
{
    const DisplayState result = dispatch('!', std::string(1, GetParam()));

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_FALSE(std::filesystem::exists(m_root / "tools.log"));
    EXPECT_FALSE(std::filesystem::exists(m_private / "shell-cwd"));
    EXPECT_TRUE(std::filesystem::equivalent(m_start, std::filesystem::current_path()));
    expect_selector_preserved();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(EraseAndKill, SelectorShellCancelTest, testing::Values(Ctl('_'), Ctl('u')));

class SelectorShellDirectoryTest : public SelectorShellTest, public testing::WithParamInterface<bool>
{
};

TEST_P(SelectorShellDirectoryTest, quietEscapeUsesTheRequestedDirectoryAndRequestsRedisplay)
{
    const bool spool = GetParam();

    const DisplayState result = dispatch('!', std::string(spool ? "!" : "") + shell_command() + "\n\n");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_directory(spool ? m_start : m_private);
    expect_one_shell();
    expect_selector_preserved();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(PrivateAndSpool, SelectorShellDirectoryTest, testing::Bool());

TEST_F(SelectorShellTest, scrolledEscapeDisplaysItsOutputAndRequestsAFullPage)
{
    g_sel_page.last_line = g_tc_LINES - 1;
    g_term_line = g_sel_page.last_line;

    const DisplayState result = dispatch('!', shell_command(true) + "\n\n");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_GT(g_term_scrolled, 0);
    EXPECT_NE(std::string::npos, m_output.find("Controlled selector shell output."));
    expect_directory(m_private);
    expect_one_shell();
    expect_selector_preserved();
    expect_next_command('q');
}

TEST_F(SelectorShellTest, failedEscapeRestoresTheDirectoryAndSelectorBeforeAcceptingMoreInput)
{
    g_term_line = g_sel_page.last_line + 1;

    const DisplayState result = dispatch('!', shell_command(true, true) + "\n\n");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find("Controlled selector shell output."));
    expect_directory(m_private);
    expect_one_shell();
    expect_selector_preserved();
    expect_next_command('q');
}

TEST_F(SelectorShellTest, followupCommandUsesTheRestoredArticleCallbackWithoutRepeatingTheShell)
{
    g_term_line = g_sel_page.last_line + 1;

    const DisplayState result = dispatch('!', shell_command() + "\nY");

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ("No marked articles to yank back.", g_msg);
    expect_directory(m_private);
    expect_one_shell();
    expect_selector_preserved();
    expect_next_command('q');
}

TEST_F(SelectorShellTest, selectorLoopRedrawsItsRowsAfterTheShellAndRetainsTheFollowingCommand)
{
    g_in_ng = true;
    g_term_line = g_sel_page.last_line + 1;

    const char result = run_input('!' + shell_command(true) + "\n\nq");

    EXPECT_EQ('Q', result);
    EXPECT_NE(std::string::npos, m_output.find("Controlled selector shell output."));
    EXPECT_NE(std::string::npos, m_output.find("Second topic"));
    expect_directory(m_private);
    expect_one_shell();
    expect_selector_preserved();
    SelectorSessionInputState::expect_next_command('q');
}

class SelectorKillEditorTest : public SelectorShellTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void use_file(std::string_view name, bool existing = true);
    void expect_edited_file(std::string_view original = "# Existing rules\n");

    std::filesystem::path m_kill_file;
    const std::string     m_editor{
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" kill-editor"};

private:
    std::FILE         *m_old_local{g_kill.local_kfp};
    KillFileStateFlags m_old_state{g_kf_state};
};

void SelectorKillEditorTest::SetUp()
{
    SelectorShellTest::SetUp();
    g_in_ng = true;
    g_kill.local_kfp = nullptr;
    g_kf_state = KFS_NONE;
    m_env.expect_env_repeatedly("VISUAL", m_editor.c_str());
    m_env.expect_env_repeatedly("EDITOR", "unused-editor");
    m_env.expect_env_repeatedly("KILLGLOBAL", (m_root / "unused-global").generic_string().c_str());
    use_file("rules/KILL");
}

void SelectorKillEditorTest::TearDown()
{
    g_kill.close(KF_LOCAL);
    g_kill.local_kfp = m_old_local;
    g_kf_state = m_old_state;
    SelectorShellTest::TearDown();
}

void SelectorKillEditorTest::use_file(std::string_view name, bool existing)
{
    g_kill.close(KF_LOCAL);
    m_kill_file = m_root / name;
    m_env.expect_env_repeatedly("KILLLOCAL", m_kill_file.generic_string().c_str());
    if (existing)
    {
        std::filesystem::create_directories(m_kill_file.parent_path());
        std::ofstream(m_kill_file, std::ios::binary) << "# Existing rules\n";
        g_kill.local_kfp = std::fopen(m_kill_file.string().c_str(), "r");
        ASSERT_NE(nullptr, g_kill.local_kfp);
    }
}

void SelectorKillEditorTest::expect_edited_file(std::string_view original)
{
    EXPECT_EQ(original, read_file(m_root / "kill-editor-input"));
    EXPECT_EQ(m_kill_file.generic_string(), read_file(m_root / "kill-editor-file"));
    EXPECT_EQ(m_start.generic_string(), read_file(m_root / "kill-editor-directory"));
    EXPECT_EQ(std::string(original) + "/controlled/:j\n", read_file(m_kill_file));
    EXPECT_EQ("kill-editor\n" + m_kill_file.generic_string() + '\n', read_file(m_root / "tools.log"));
    EXPECT_FALSE(std::filesystem::exists(m_root / "unused-global"));
    ASSERT_NE(nullptr, g_kill.local_kfp);
    EXPECT_TRUE(g_kf_state & KFS_NORMAL_LINES);
    std::rewind(g_kill.local_kfp);
    EXPECT_EQ(original.empty() ? "/controlled/:j\n" : "# Existing rules\n", get_a_line(g_kill.local_kfp));
}

class SelectorKillEditorPathTest : public SelectorKillEditorTest, public testing::WithParamInterface<const char *>
{
};

TEST_P(SelectorKillEditorPathTest, editsAndReloadsTheConfiguredLocalFileWithoutChangingTheArticlePage)
{
    use_file(GetParam());

    const DisplayState result = dispatch(Ctl('k'));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_edited_file();
    expect_selector_preserved();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(OrdinarySpacesAndQuotes, SelectorKillEditorPathTest,
                         testing::Values("rules/KILL", "rules with spaces/KILL", "reader's rules/KILL"));

TEST_F(SelectorKillEditorTest, usesEditorWhenVisualIsUnset)
{
    m_env.expect_no_envar("VISUAL");
    m_env.expect_env_repeatedly("EDITOR", m_editor.c_str());

    const DisplayState result = dispatch(Ctl('k'));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_edited_file();
    expect_selector_preserved();
    expect_next_command('q');
}

TEST_F(SelectorKillEditorTest, createsTheMissingLocalKillDirectoryBeforeEditing)
{
    use_file("new rules/nested/KILL", false);

    const DisplayState result = dispatch(Ctl('k'));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_edited_file("");
    expect_selector_preserved();
    expect_next_command('q');
}

TEST_F(SelectorKillEditorTest, selectorLoopRedisplaysTheArticleRowsAfterEditing)
{
    g_term_line = g_sel_page.last_line + 1;

    const char result = run_input(std::string(1, Ctl('k')) + 'q');

    EXPECT_EQ('Q', result);
    expect_edited_file();
    const std::size_t editing = m_output.find("Editing local KILL file:");
    ASSERT_NE(std::string::npos, editing);
    EXPECT_NE(std::string::npos, m_output.find("Second topic", editing));
    expect_selector_preserved();
    SelectorSessionInputState::expect_next_command('q');
}

TEST_F(SelectorKillEditorTest, anUnusableParentReturnsToTheSelectorWithoutStartingTheEditor)
{
    std::ofstream(m_root / "blocked") << "Not a directory\n";
    use_file("blocked/KILL", false);

    const DisplayState result = dispatch(Ctl('k'));

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_NE(std::string::npos, m_output.find("Can't make "));
    EXPECT_FALSE(std::filesystem::exists(m_root / "tools.log"));
    expect_selector_preserved();
    expect_next_command('q');
}

class SelectorKillEditorFailureTest : public SelectorKillEditorTest, public testing::WithParamInterface<const char *>
{
};

TEST_P(SelectorKillEditorFailureTest, anEditorFailureReturnsToTheUnchangedArticlePage)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam());

    const DisplayState result = dispatch(Ctl('k'));

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ("# Existing rules\n", read_file(m_kill_file));
    EXPECT_EQ("kill-editor\n" + m_kill_file.generic_string() + '\n', read_file(m_root / "tools.log"));
    EXPECT_FALSE(g_kf_state & KFS_NORMAL_LINES);
    expect_selector_preserved();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(FailureAndCancellation, SelectorKillEditorFailureTest,
                         testing::Values("kill-editor_failure", "kill-editor_cancel"));

class SelectorOptionTest : public SelectorCallbackTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_article_context();
    char run_option_session(std::string_view commands);

private:
    const OptionCatalog         m_catalog;
    OptionValueList             m_old_defaults{std::move(g_option_def_vals)};
    OptionValueList             m_old_saved{std::move(g_option_saved_vals)};
    OptionDraft                *m_old_draft{g_opt_edit.draft};
    OptionFlags                *m_old_flags{g_opt_edit.flags};
    int                         m_old_page{g_sel_page.page_op};
    bool                        m_old_one_command{g_one_command};
    bool                        m_old_mouse_down{g_mouse_is_down};
    bool                        m_old_novice_delays{g_novice_delays};
    int                         m_old_perform_count{g_status.perform_count};
    SelectionMode               m_old_thread_mode{g_sel.thread_mode};
    SelectionSortMode           m_old_thread_sort{g_sel.thread_sort};
    std::string                 m_old_commands{g_option_sel_cmds};
    std::string_view            m_old_backspace{g_tc_BC};
    std::string_view            m_old_clear{g_tc_CD};
    std::vector<OptionFlags>    m_flags;
    std::array<ArticleFlags, 2> m_article_flags{};
    ArticleLocation             m_article_location{};
};

void SelectorOptionTest::SetUp()
{
    SelectorCallbackTest::SetUp();
    m_flags.assign(static_cast<std::size_t>(m_catalog.row_count() + 1), OF_NONE);
    m_flags[m_catalog.previous_group_row(m_catalog.row_for(OI_TERSE_OUTPUT))] = OF_SEL;
    g_opt_edit.flags = m_flags.data();
    g_option_def_vals = OptionValueList(static_cast<std::size_t>(m_catalog.option_limit()));
    g_option_saved_vals = OptionValueList(static_cast<std::size_t>(m_catalog.option_limit()));
    g_opt_edit.draft = nullptr;
    g_option_sel_cmds = "Z>";
    g_one_command = false;
    g_novice_delays = false;
    g_status.perform_count = 0;
    g_tc_BC = "\b";
    g_tc_CD = "";
    g_term_line = g_sel_page.last_line;
    g_term_col = 0;
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
    g_cache.ptr(ArticleNum{4})->m_flags |= AF_DEL;
    g_sel_page.items[0].sel = 1;
    g_sel_page.items[1].sel = 2;
    g_sel_page.item_index = 1;
    m_article_flags = {g_cache.ptr(ArticleNum{3})->m_flags, g_cache.ptr(ArticleNum{4})->m_flags};
    m_article_location = g_nav.target;
}

void SelectorOptionTest::TearDown()
{
    g_option_def_vals = std::move(m_old_defaults);
    g_option_saved_vals = std::move(m_old_saved);
    g_opt_edit.draft = m_old_draft;
    g_opt_edit.flags = m_old_flags;
    g_sel_page.page_op = m_old_page;
    g_one_command = m_old_one_command;
    g_mouse_is_down = m_old_mouse_down;
    g_novice_delays = m_old_novice_delays;
    g_status.perform_count = m_old_perform_count;
    g_sel.thread_mode = m_old_thread_mode;
    g_sel.thread_sort = m_old_thread_sort;
    g_option_sel_cmds = m_old_commands;
    g_tc_BC = m_old_backspace;
    g_tc_CD = m_old_clear;
    SelectorCallbackTest::TearDown();
}

void SelectorOptionTest::expect_article_context()
{
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(m_article_flags[0], g_cache.ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(m_article_flags[1], g_cache.ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(m_article_location, g_nav.target);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_THREAD_SELECTOR, g_mode);
    EXPECT_EQ(nullptr, g_opt_edit.draft);
    EXPECT_FALSE(g_one_command);
}

char SelectorOptionTest::run_option_session(std::string_view commands)
{
    g_in_ng = true;
    g_cache.ptr(ArticleNum{4})->m_flags &= ~AF_DEL;
    push_string(std::string(commands) + 'q', 0200);
    testing::internal::CaptureStdout();
    const char result = article_selector('a');
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

class SelectorInlineOptionTest : public SelectorOptionTest, public testing::WithParamInterface<const char *>
{
};

TEST_P(SelectorInlineOptionTest, appliesTheOptionAndReturnsToTheArticlePrompt)
{
    Article **const page = g_sel_page.page_art;

    const DisplayState result = dispatch('&', std::string(GetParam()) + '\n');

    EXPECT_EQ(DS_ASK, result);
    EXPECT_FALSE(g_verbose);
    EXPECT_EQ(1, g_status.perform_count);
    EXPECT_EQ(page, g_sel_page.page_art);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    expect_article_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(SwitchAndAssignment, SelectorInlineOptionTest, testing::Values("-t", "Terse Output=yes"));

class SelectorOptionCancelTest : public SelectorOptionTest, public testing::WithParamInterface<char>
{
};

TEST_P(SelectorOptionCancelTest, erasedOptionCommandLeavesSettingsAndArticleContextUntouched)
{
    const DisplayState result = dispatch('&', std::string(1, GetParam()));

    EXPECT_EQ(DS_ASK, result);
    EXPECT_TRUE(g_verbose);
    EXPECT_EQ(0, g_status.perform_count);
    expect_article_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(EraseAndKill, SelectorOptionCancelTest, testing::Values(Ctl('_'), Ctl('u')));

class SelectorModeOptionTest : public SelectorOptionTest, public testing::WithParamInterface<const char *>
{
};

TEST_P(SelectorModeOptionTest, changingTheRequestedModeRestoresTheCallerAndRequestsARestart)
{
    const DisplayState result = dispatch('&', std::string(GetParam()) + '\n');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(SM_SUBJECT, g_sel.default_mode);
    EXPECT_EQ(1, g_status.perform_count);
    expect_article_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(SwitchAndAssignment, SelectorModeOptionTest,
                         testing::Values("-Os", "News Selector Mode=subjects"));

TEST_F(SelectorOptionTest, scrolledOptionInputDispatchesTheFollowupThroughTheArticleCallback)
{
    g_term_line = g_sel_page.last_line + 1;

    const DisplayState result = dispatch('&', "Terse Output=yes\nY");

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_FALSE(g_verbose);
    EXPECT_EQ("No marked articles to yank back.", g_msg);
    EXPECT_EQ(1, g_status.perform_count);
    expect_article_context();
    expect_next_command('q');
}

using NestedOptionExit = std::pair<char, bool>;

class SelectorNestedOptionTest : public SelectorOptionTest, public testing::WithParamInterface<NestedOptionExit>
{
};

TEST_P(SelectorNestedOptionTest, returningFromTheOptionSelectorAppliesOrAbandonsItsDraft)
{
    const auto [exit, accepted] = GetParam();

    const DisplayState result = dispatch('&', std::string("\nb \n") + exit);

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(!accepted, g_verbose);
    EXPECT_EQ(1, g_status.perform_count);
    EXPECT_NE(std::string::npos, m_output.find("Options"));
    EXPECT_NE(std::string::npos, m_output.find("Change `Terse Output'"));
    expect_article_context();
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(UseAndAbandon, SelectorNestedOptionTest,
                         testing::Values(NestedOptionExit{'\t', true}, NestedOptionExit{'Z', true},
                                         NestedOptionExit{'q', false}, NestedOptionExit{'Q', false},
                                         NestedOptionExit{'\033', false}));

TEST_F(SelectorOptionTest, acceptingAnUnchangedNestedValueLeavesTheSettingUnmodified)
{
    const DisplayState result = dispatch('&', "\nb\n\t");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_TRUE(g_verbose);
    EXPECT_FALSE(g_option_saved_vals[OI_TERSE_OUTPUT].has_value());
    expect_article_context();
    expect_next_command('q');
}

TEST_F(SelectorOptionTest, abandoningNestedOptionsPreservesTheReadScopeAndItsSelections)
{
    prepare_input(SM_ARTICLE, true);
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
    const ArticleFlags selected = g_cache.ptr(ArticleNum{3})->m_flags;
    const ArticleFlags other = g_cache.ptr(ArticleNum{4})->m_flags;
    ASSERT_EQ(1, g_selected_count);

    const DisplayState result = dispatch('&', "\nq");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(selected, g_cache.ptr(ArticleNum{3})->m_flags);
    EXPECT_EQ(other, g_cache.ptr(ArticleNum{4})->m_flags);
    EXPECT_EQ(nullptr, g_opt_edit.draft);
    EXPECT_TRUE(g_verbose);
    expect_next_command('q');
}

class SelectorOptionSaveTest : public SelectorOptionTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string           m_old_ini{g_ini_file};
    std::filesystem::path m_root;
};

void SelectorOptionSaveTest::SetUp()
{
    SelectorOptionTest::SetUp();
    const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
    m_root = std::filesystem::path{TRN_TEST_TMP_DIR} / info->test_suite_name() / info->name();
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    ASSERT_FALSE(error) << error.message();
    std::filesystem::create_directories(m_root);
    g_ini_file = (m_root / "trnrc").generic_string();
    std::ofstream(g_ini_file) << "[options]\nTerse Output = no\n";
}

void SelectorOptionSaveTest::TearDown()
{
    g_ini_file = m_old_ini;
    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    EXPECT_FALSE(error) << error.message();
    SelectorOptionTest::TearDown();
}

TEST_F(SelectorOptionSaveTest, savingTheNestedDraftAppliesItAndUpdatesTheLocalOptionFile)
{
    const DisplayState result = dispatch('&', "\nb \nS");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_verbose);
    std::ifstream     input{g_ini_file};
    const std::string saved{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    EXPECT_NE(std::string::npos, saved.find("Terse Output = yes\n"));
    EXPECT_FALSE(g_option_saved_vals[OI_TERSE_OUTPUT].has_value());
    expect_article_context();
    expect_next_command('q');
}

class SelectorOptionSessionTest : public SelectorOptionTest, public testing::WithParamInterface<NestedOptionExit>
{
};

TEST_P(SelectorOptionSessionTest, nestedOptionsReturnToArticleRowsAndRestoreTheArticleCommandHandler)
{
    const auto [exit, accepted] = GetParam();

    const char result = run_option_session(std::string("&\nb \n") + exit + "Yq");

    EXPECT_EQ('q', result);
    EXPECT_EQ(!accepted, g_verbose);
    const std::size_t options = m_output.rfind("Options");
    ASSERT_NE(std::string::npos, options);
    EXPECT_NE(std::string::npos, m_output.find("Second topic", options));
    EXPECT_NE(std::string::npos, m_output.find("No marked articles to yank back.", options));
    EXPECT_EQ(SM_ARTICLE, g_sel.mode);
    EXPECT_EQ(nullptr, g_opt_edit.draft);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_THREAD_SELECTOR, g_mode);
    EXPECT_FALSE(g_one_command);
    EXPECT_EQ(1, g_status.perform_count);
    SelectorSessionInputState::expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(UseAndAbandon, SelectorOptionSessionTest,
                         testing::Values(NestedOptionExit{'\t', true}, NestedOptionExit{'q', false}));

using SelectorExitCommand = std::pair<char, char>;

#ifdef I_SGTTY
constexpr char SELECTOR_CARRIAGE_RETURN = '\n';
#else
constexpr char SELECTOR_CARRIAGE_RETURN = '\r';
#endif

class SelectorExitInputTest : public SelectorSessionInputState,
                              public testing::WithParamInterface<std::tuple<SelectorExitCommand, bool>>
{
};

TEST_P(SelectorExitInputTest, returnsExitCommandWithoutConsumingTheFollowingCommand)
{
    const auto [exit_command, rereading] = GetParam();
    const auto [command, expected] = exit_command;

    const char result = run_selector(std::string(1, command), rereading);

    EXPECT_EQ(expected, result);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(
    ExitCommands, SelectorExitInputTest,
    testing::Combine(testing::Values(SelectorExitCommand{'q', 'q'}, SelectorExitCommand{'Q', 'Q'},
                                     SelectorExitCommand{'+', '+'}, SelectorExitCommand{'`', 'Q'},
                                     SelectorExitCommand{'\033', '+'}, SelectorExitCommand{Ctl('q'), '+'},
                                     SelectorExitCommand{'Z', 'Z'}, SelectorExitCommand{'\t', '\t'},
                                     SelectorExitCommand{'\r', SELECTOR_CARRIAGE_RETURN},
                                     SelectorExitCommand{'\n', '\n'}, SelectorExitCommand{'\b', '\b'},
                                     SelectorExitCommand{'\177', '\177'}, SelectorExitCommand{'N', 'N'},
                                     SelectorExitCommand{'P', 'P'}),
                     testing::Bool()));

class SelectorDefaultInputTest : public SelectorSessionInputState
{
};

TEST_F(SelectorDefaultInputTest, spaceUsesTheConfiguredEndCommandOnTheLastPage)
{
    g_news_sel_cmds = "N>";

    const char result = run_selector(" ");

    EXPECT_EQ('N', result);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    expect_next_command('q');
}

TEST_F(SelectorDefaultInputTest, readModeUsesItsOwnEndCommand)
{
    g_news_sel_cmds = "Nq";

    const char result = run_selector(" ", true);

    EXPECT_EQ('Z', result);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    expect_next_command('q');
}

TEST_F(SelectorDefaultInputTest, spaceUsesTheConfiguredPageCommandBeforeTheLastPage)
{
    g_news_sel_cmds = "NZ";
    g_tc_LINES = 6;

    const char result = run_selector(" ");

    EXPECT_EQ('Z', result);
    EXPECT_FALSE(g_sel_page.at_end);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    expect_next_command('q');
}

TEST_F(SelectorDefaultInputTest, spaceAdvancesThePageBeforeTheNextSelectionCommand)
{
    g_tc_LINES = 6;

    const char result = run_selector(" aq");

    EXPECT_EQ('q', result);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{3})->m_flags & AF_SEL);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{4})->m_flags & AF_SEL);
    EXPECT_TRUE(g_sel_page.at_end);
    expect_next_command('q');
}

TEST_F(SelectorDefaultInputTest, repeatedSpacesAdvanceThenAcceptAtTheEnd)
{
    g_tc_LINES = 6;

    const char result = run_selector("  ");

    EXPECT_EQ('Z', result);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_sel_page.items[0].u.ap);
    expect_next_command('q');
}

TEST_F(SelectorDefaultInputTest, readModeUsesItsOwnPageCommand)
{
    g_news_sel_cmds = "Nq";
    g_tc_LINES = 6;

    const char result = run_selector(" q", true);

    EXPECT_EQ('q', result);
    EXPECT_TRUE(g_sel_page.at_end);
    EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_sel_page.items[0].u.ap);
    expect_next_command('q');
}

using SelectorRestartCase = std::pair<bool, int>;

class SelectorRestartInputTest : public SelectorSessionInputState,
                                 public testing::WithParamInterface<SelectorRestartCase>
{
};

TEST_P(SelectorRestartInputTest, restartRebuildsTheScopeBeforeReadingTheExitCommand)
{
    const auto [rereading, restarts] = GetParam();
    const bool final_read_scope = rereading != (restarts % 2 != 0);

    const char result = run_selector(std::string(restarts, 'U') + "q", rereading);

    EXPECT_EQ('q', result);
    EXPECT_EQ(g_cache.ptr(ArticleNum{final_read_scope ? 1 : 3}), g_sel_page.items[0].u.ap);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(2, m_newsgroup.m_to_read);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    expect_next_command('q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopes, SelectorRestartInputTest,
                         testing::Values(SelectorRestartCase{false, 1}, SelectorRestartCase{true, 1},
                                         SelectorRestartCase{false, 2}, SelectorRestartCase{true, 2}));

class SelectorLetterInputTest : public SelectorInputTest,
                                public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
protected:
    void SetUp() override;
};

void SelectorLetterInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    const auto [mode, rereading] = GetParam();
    prepare_input(mode, rereading);
}

TEST_P(SelectorLetterInputTest, letterSelectsItsRowAndAdvancesToTheNextItem)
{
    g_sel_page.item_index = 1;

    ASSERT_EQ('Q', run_input("a"));

    EXPECT_NE(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(0, g_sel_page.items[1].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

TEST_P(SelectorLetterInputTest, lastLetterSelectsItsRowAndWrapsToTheFirstItem)
{
    ASSERT_EQ('Q', run_input("b"));

    EXPECT_EQ(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_NE(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(0, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(0, g_sel_page.item_index);
}

TEST_P(SelectorLetterInputTest, repeatingALetterDeselectsTheRow)
{
    ASSERT_EQ('Q', run_input("aa"));

    EXPECT_EQ(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(0, g_sel_page.items[1].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

TEST_P(SelectorLetterInputTest, selectingAnotherLetterRetainsThePreviousSelection)
{
    ASSERT_EQ('Q', run_input("ab"));

    EXPECT_NE(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_NE(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(0, g_sel_page.item_index);
}

TEST_P(SelectorLetterInputTest, deselectingALetterRetainsTheOtherSelection)
{
    ASSERT_EQ('Q', run_input("aba"));

    EXPECT_EQ(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_NE(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

TEST_P(SelectorLetterInputTest, letterBeyondThePageRetainsSelectionAndPosition)
{
    ASSERT_EQ('Q', run_input("ac"));

    EXPECT_NE(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(0, g_sel_page.items[1].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_NE(std::string::npos, m_output.find("No item 'c' on this page."));
}

TEST_P(SelectorLetterInputTest, letterCanReselectADeselectedRow)
{
    ASSERT_EQ('Q', run_input("aaa"));

    EXPECT_NE(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{3})->m_flags & AF_DEL);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, SelectorLetterInputTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

class SelectorCurrentInputTest : public SelectorInputTest,
                                 public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorCurrentInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_input(std::get<0>(GetParam()), std::get<1>(GetParam()));
    g_sel_page.item_index = 1;
}

TEST_P(SelectorCurrentInputTest, toggleSelectsTheCurrentRow)
{
    const char command = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    EXPECT_EQ(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_NE(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(0, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(command == Ctl('t') ? 1 : 0, g_sel_page.item_index);
}

TEST_P(SelectorCurrentInputTest, toggleDeselectsTheCurrentSelectedRow)
{
    g_cache.ptr(ArticleNum{4})->select_article(AUTO_KILL_NONE);
    g_sel_page.items[1].sel = 1;
    const char command = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    EXPECT_EQ(0, g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_sel_page.items[0].sel);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[1].sel);
    EXPECT_EQ(command == Ctl('t') ? 1 : 0, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndToggle, SelectorCurrentInputTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('.', 'y', Ctl('t'))));

class SelectorGroupedInputState : public SelectorInputTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_grouped(SelectionMode mode, bool rereading);
    void select_articles(std::initializer_list<long> numbers);
    void sync_page();
    void expect_selected(const std::vector<long> &numbers, int subjects) const;

private:
    Subject          m_reply{};
    Subject          m_unrelated{};
    ArticleFlags     m_opposite_flags{};
    std::string_view m_old_tc_home{g_tc_HO};
    std::string_view m_old_tc_up{g_tc_UP};
    std::string_view m_old_tc_cm{g_tc_CM};
};

void SelectorGroupedInputState::SetUp()
{
    SelectorInputTest::SetUp();
    g_tc_HO = "";
    g_tc_UP = "";
    g_tc_CM = "";
}

void SelectorGroupedInputState::TearDown()
{
    g_tc_HO = m_old_tc_home;
    g_tc_UP = m_old_tc_up;
    g_tc_CM = m_old_tc_cm;
    SelectorInputTest::TearDown();
}

void SelectorGroupedInputState::prepare_grouped(SelectionMode mode, bool rereading)
{
    g_subj.count = 3;
    g_last_art = ArticleNum{5};
    g_cache.last_cached = g_last_art;
    m_newsgroup.m_to_read = rereading ? 1 : 4;
    Article *root = g_cache.ptr(ArticleNum{1});
    Article *same_subject = g_cache.ptr(ArticleNum{2});
    Article *reply = g_cache.ptr(ArticleNum{3});
    Article *unrelated = g_cache.ptr(ArticleNum{4});
    Article *opposite_scope = g_cache.ptr(ArticleNum{5});
    for (long number = 1; number <= 5; ++number)
    {
        Article *article = g_cache.ptr(ArticleNum{number});
        article->m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
        article->m_date = number;
        if ((number != 5) != rereading)
        {
            article->m_flags |= AF_UNREAD;
        }
        article->m_subj_next = nullptr;
    }
    m_subject.m_articles = root;
    m_subject.m_thread = root;
    m_subject.m_next = &m_reply;
    m_subject.m_thread_link = &m_reply;
    root->m_subj_next = same_subject;
    same_subject->m_subj_next = opposite_scope;
    opposite_scope->m_subj = &m_subject;
    root->m_child1 = same_subject;
    same_subject->m_parent = root;
    same_subject->m_sibling = reply;
    reply->m_parent = root;
    reply->m_sibling = opposite_scope;
    opposite_scope->m_parent = root;
    reply->m_subj = &m_reply;
    m_reply.m_str = "Re: Changed subject in the same thread";
    m_reply.m_articles = reply;
    m_reply.m_thread = root;
    m_reply.m_thread_link = &m_subject;
    m_reply.m_prev = &m_subject;
    m_reply.m_next = &m_unrelated;
    unrelated->m_subj = &m_unrelated;
    m_unrelated.m_str = "Re: Unrelated thread";
    m_unrelated.m_articles = unrelated;
    m_unrelated.m_thread = unrelated;
    m_unrelated.m_thread_link = &m_unrelated;
    m_unrelated.m_prev = &m_reply;
    g_subj.last = &m_unrelated;
    opposite_scope->m_flags |= rereading ? AF_SEL : AF_DEL_SEL;
    prepare_page(mode, rereading);
    m_opposite_flags = opposite_scope->m_flags;
    g_sel_page.chars = "abcde";
    g_sel_page.page_item_cnt = mode == SM_ARTICLE ? 4 : mode == SM_SUBJECT ? 3 : 2;
    const std::array<Subject *, 3> subjects{&m_subject, &m_reply, &m_unrelated};
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        g_sel_page.items[index] = {};
        g_sel_page.items[index].line = index + 1;
        if (mode == SM_ARTICLE)
        {
            g_sel_page.items[index].u.ap = g_cache.ptr(ArticleNum{index + 1});
        }
        else
        {
            g_sel_page.items[index].u.sp = mode == SM_THREAD && index ? &m_unrelated : subjects[index];
        }
    }
    g_sel_page.page_obj_cnt = 4;
    g_sel_page.last_line = 10;
    g_sel_page.at_end = true;
    sync_page();
}

void SelectorGroupedInputState::select_articles(std::initializer_list<long> numbers)
{
    for (long number : numbers)
    {
        g_cache.ptr(ArticleNum{number})->select_article(AUTO_KILL_NONE);
    }
    sync_page();
}

void SelectorGroupedInputState::sync_page()
{
    testing::internal::CaptureStdout();
    update_page();
    (void) testing::internal::GetCapturedStdout();
    g_sel_page.item_index = g_sel.mode == SM_ARTICLE ? 1 : 0;
    g_term_line = 0;
    g_term_col = 0;
}

void SelectorGroupedInputState::expect_selected(const std::vector<long> &numbers, int subjects) const
{
    std::vector<long> selected;
    for (const auto &[number, article] : g_cache.article_list)
    {
        if (article.m_flags & g_sel.mask)
        {
            selected.push_back(number.value_of());
        }
        EXPECT_EQ((number != ArticleNum{5}) != g_sel.rereading, !!(article.m_flags & AF_UNREAD));
    }
    EXPECT_EQ(numbers, selected);
    EXPECT_EQ(numbers.size(), g_selected_count);
    EXPECT_EQ(subjects, g_selected_subj_cnt);
    EXPECT_EQ(m_opposite_flags, g_cache.ptr(ArticleNum{5})->m_flags);
    EXPECT_EQ(g_sel.rereading ? 1 : 4, m_newsgroup.m_to_read);
}

class SelectorMarkInputState : public SelectorGroupedInputState
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_marks(SelectionMode mode, bool rereading);
    void expect_current_deselected() const;
    void expect_current_mark(bool killed, bool delayed) const;
    void expect_pending(const std::vector<long> &articles, const std::vector<long> &subjects,
                        const std::vector<long> &delayed = {}) const;
    int  target_index() const;

private:
    int  m_old_delayed_count{g_dm_count};
    bool m_old_in_ng{g_in_ng};
    bool m_old_numeric_goto{g_sel_num_goto};
};

void SelectorMarkInputState::SetUp()
{
    SelectorGroupedInputState::SetUp();
    g_dm_count = 0;
    g_in_ng = true;
    g_sel_num_goto = false;
}

void SelectorMarkInputState::TearDown()
{
    g_dm_count = m_old_delayed_count;
    g_in_ng = m_old_in_ng;
    g_sel_num_goto = m_old_numeric_goto;
    SelectorGroupedInputState::TearDown();
}

void SelectorMarkInputState::prepare_marks(SelectionMode mode, bool rereading)
{
    prepare_grouped(mode, rereading);
    select_articles({1, 2, 3, 4});
}

int SelectorMarkInputState::target_index() const
{
    return g_sel.mode == SM_ARTICLE ? 1 : 0;
}

void SelectorMarkInputState::expect_current_deselected() const
{
    switch (g_sel.mode)
    {
    case SM_ARTICLE:
        expect_selected({1, 3, 4}, 3);
        break;
    case SM_SUBJECT:
        expect_selected({3, 4}, 2);
        break;
    default:
        expect_selected({4}, 1);
        break;
    }
    EXPECT_EQ((target_index() + 1) % g_sel_page.page_item_cnt, g_sel_page.item_index);
    EXPECT_EQ(g_sel.rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
}

void SelectorMarkInputState::expect_current_mark(bool killed, bool delayed) const
{
    const std::vector<long> affected = g_sel.mode == SM_ARTICLE   ? std::vector<long>{2}
                                       : g_sel.mode == SM_SUBJECT ? std::vector<long>{1, 2}
                                                                  : std::vector<long>{1, 2, 3};
    const std::vector<long> subjects = g_sel.mode == SM_SUBJECT  ? std::vector<long>{1}
                                       : g_sel.mode == SM_THREAD ? std::vector<long>{1, 3}
                                                                 : std::vector<long>{};

    expect_current_deselected();
    expect_pending(killed && g_sel.mode == SM_ARTICLE ? affected : std::vector<long>{},
                   killed ? subjects : std::vector<long>{}, delayed ? affected : std::vector<long>{});
    EXPECT_EQ(killed ? 2 : 0, g_sel_page.items[target_index()].sel);
}

void SelectorMarkInputState::expect_pending(const std::vector<long> &articles, const std::vector<long> &subjects,
                                            const std::vector<long> &delayed) const
{
    std::vector<long> actual_articles;
    std::vector<long> actual_subjects;
    std::vector<long> actual_delayed;
    for (long number = 1; number <= 4; ++number)
    {
        const Article *article = g_cache.ptr(ArticleNum{number});
        if (article->m_flags & AF_DEL)
        {
            actual_articles.push_back(number);
        }
        if (article->m_flags & AF_YANK_BACK)
        {
            actual_delayed.push_back(number);
        }
    }
    for (const Subject *subject = g_subj.first; subject; subject = subject->m_next)
    {
        if (subject->m_flags & SF_DEL)
        {
            actual_subjects.push_back(subject->m_articles->article_num().value_of());
        }
    }
    EXPECT_EQ(articles, actual_articles);
    EXPECT_EQ(subjects, actual_subjects);
    EXPECT_EQ(delayed, actual_delayed);
    EXPECT_EQ(delayed.size(), g_dm_count);
}

class SelectorInvertInputTest : public SelectorMarkInputState,
                                public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
protected:
    void SetUp() override;
    void expect_full_selection() const;
    void expect_scope() const;
    void prepare_mixed_page();
};

void SelectorInvertInputTest::SetUp()
{
    SelectorMarkInputState::SetUp();
    prepare_grouped(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

void SelectorInvertInputTest::expect_scope() const
{
    EXPECT_EQ(std::get<0>(GetParam()), g_sel.mode);
    EXPECT_EQ(std::get<1>(GetParam()), g_sel.rereading);
    EXPECT_EQ(std::get<1>(GetParam()) ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(0, g_sel_page.item_index);
}

void SelectorInvertInputTest::expect_full_selection() const
{
    expect_selected({1, 2, 3, 4}, 3);
    expect_pending({}, {});
    expect_scope();
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        EXPECT_EQ(1, g_sel_page.items[index].sel) << index;
    }
}

void SelectorInvertInputTest::prepare_mixed_page()
{
    switch (g_sel.mode)
    {
    case SM_ARTICLE:
        select_articles({1, 4});
        break;
    case SM_SUBJECT:
        select_articles({1, 2, 4});
        break;
    default:
        select_articles({1, 2, 3});
        break;
    }
    if (g_sel.mode != SM_THREAD)
    {
        --g_sel_page.page_item_cnt;
        g_sel_page.at_end = false;
    }
    g_sel_page.item_index = g_sel_page.page_item_cnt - 1;
}

TEST_P(SelectorInvertInputTest, inversionSelectsAnExplicitlyUnmarkedRow)
{
    ASSERT_EQ('Q', run_input("m@"));

    expect_full_selection();
}

TEST_P(SelectorInvertInputTest, inversionExchangesVisibleSelectionsAndRetainsHiddenSelections)
{
    prepare_mixed_page();

    ASSERT_EQ('Q', run_input("@"));

    expect_selected(g_sel.mode == SM_ARTICLE   ? std::vector<long>{2, 3, 4}
                    : g_sel.mode == SM_SUBJECT ? std::vector<long>{3, 4}
                                               : std::vector<long>{4},
                    g_sel.mode == SM_ARTICLE   ? 3
                    : g_sel.mode == SM_SUBJECT ? 2
                                               : 1);
    expect_pending(g_sel.rereading && g_sel.mode == SM_ARTICLE ? std::vector<long>{1} : std::vector<long>{},
                   !g_sel.rereading || g_sel.mode == SM_ARTICLE ? std::vector<long>{}
                   : g_sel.mode == SM_SUBJECT                   ? std::vector<long>{1}
                                                                : std::vector<long>{1, 3});
    expect_scope();
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    for (int index = 1; index < g_sel_page.page_item_cnt; ++index)
    {
        EXPECT_EQ(1, g_sel_page.items[index].sel) << index;
    }
}

TEST_P(SelectorInvertInputTest, inversionPreservesUnreadKillsAndSelectsReadKills)
{
    ASSERT_EQ('Q', run_input("k@"));

    if (g_sel.rereading)
    {
        expect_full_selection();
    }
    else
    {
        expect_selected(g_sel.mode == SM_ARTICLE   ? std::vector<long>{1, 3, 4}
                        : g_sel.mode == SM_SUBJECT ? std::vector<long>{3, 4}
                                                   : std::vector<long>{4},
                        g_sel.mode == SM_ARTICLE   ? 3
                        : g_sel.mode == SM_SUBJECT ? 2
                                                   : 1);
        expect_pending(g_sel.mode == SM_ARTICLE ? std::vector<long>{2} : std::vector<long>{},
                       g_sel.mode == SM_SUBJECT  ? std::vector<long>{1}
                       : g_sel.mode == SM_THREAD ? std::vector<long>{1, 3}
                                                 : std::vector<long>{});
        expect_scope();
        for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
        {
            EXPECT_EQ(index == target_index() ? 2 : 1, g_sel_page.items[index].sel) << index;
        }
    }
}

TEST_P(SelectorInvertInputTest, repeatingInversionRestoresVisibleSelectionsAndRetainsHiddenSelections)
{
    prepare_mixed_page();

    ASSERT_EQ('Q', run_input("@@"));

    expect_selected(g_sel.mode == SM_ARTICLE   ? std::vector<long>{1, 4}
                    : g_sel.mode == SM_SUBJECT ? std::vector<long>{1, 2, 4}
                                               : std::vector<long>{1, 2, 3},
                    g_sel.mode == SM_ARTICLE ? 3 : 2);
    expect_pending(g_sel.rereading && g_sel.mode == SM_ARTICLE ? std::vector<long>{2, 3} : std::vector<long>{},
                   !g_sel.rereading || g_sel.mode == SM_ARTICLE ? std::vector<long>{}
                   : g_sel.mode == SM_SUBJECT                   ? std::vector<long>{3}
                                                                : std::vector<long>{4});
    expect_scope();
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    for (int index = 1; index < g_sel_page.page_item_cnt; ++index)
    {
        EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[index].sel) << index;
    }
}

TEST_P(SelectorInvertInputTest, invertingAnEmptyPagePreservesSelectionsAndPendingMarks)
{
    select_articles({1, 4});
    std::array<ArticleFlags, 4> flags{};
    for (int index = 0; index < 4; ++index)
    {
        flags[index] = g_cache.ptr(ArticleNum{index + 1})->m_flags;
    }
    g_sel_page.page_item_cnt = 0;
    g_sel_page.item_index = 0;

    ASSERT_EQ('Q', run_input("@"));

    expect_selected({1, 4}, 2);
    expect_scope();
    for (int index = 0; index < 4; ++index)
    {
        EXPECT_EQ(flags[index], g_cache.ptr(ArticleNum{index + 1})->m_flags) << index;
    }
    EXPECT_EQ(0, g_dm_count);
}

INSTANTIATE_TEST_SUITE_P(ModeAndReadState, SelectorInvertInputTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

class SelectorPartialInvertInputTest : public SelectorInvertInputTest
{
};

TEST_P(SelectorPartialInvertInputTest, inversionCompletesAPartiallySelectedRow)
{
    select_articles({1});
    ASSERT_EQ(3, g_sel_page.items[0].sel);

    ASSERT_EQ('Q', run_input("@"));

    expect_full_selection();
}

INSTANTIATE_TEST_SUITE_P(GroupedModesAndReadState, SelectorPartialInvertInputTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool()));

class SelectorUnmarkInputTest : public SelectorMarkInputState,
                                public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorUnmarkInputTest::SetUp()
{
    SelectorMarkInputState::SetUp();
    prepare_marks(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(SelectorUnmarkInputTest, unmarkCancelsTheCurrentPendingKill)
{
    const std::string commands =
        std::string("k") + Ctl('g') + g_sel_page.chars[target_index()] + std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(commands));

    expect_current_deselected();
    expect_pending({}, {});
    EXPECT_EQ(0, g_sel_page.items[target_index()].sel);
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndCommand, SelectorUnmarkInputTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('m', '|')));

using SelectorMarkCommand = std::tuple<char, bool, bool>;

class SelectorMarkInputTest : public SelectorMarkInputState,
                              public testing::WithParamInterface<std::tuple<SelectionMode, bool, SelectorMarkCommand>>
{
protected:
    void SetUp() override;
};

void SelectorMarkInputTest::SetUp()
{
    SelectorMarkInputState::SetUp();
    prepare_marks(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(SelectorMarkInputTest, commandMarksOnlyTheCurrentRow)
{
    const auto [command, killed, delayed] = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_current_mark(killed, delayed);
}

TEST_P(SelectorMarkInputTest, repeatingTheCommandPreservesCountsAndPendingMarks)
{
    const auto [command, killed, delayed] = std::get<2>(GetParam());
    const std::string commands = std::string(1, command) + Ctl('g') + g_sel_page.chars[target_index()] + command;

    ASSERT_EQ('Q', run_input(commands));

    expect_current_mark(killed, delayed);
}

INSTANTIATE_TEST_SUITE_P(
    ModeReadStateAndCommand, SelectorMarkInputTest,
    testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                     testing::Values(SelectorMarkCommand{'m', false, false}, SelectorMarkCommand{'|', false, false},
                                     SelectorMarkCommand{'k', true, false}, SelectorMarkCommand{'j', true, false},
                                     SelectorMarkCommand{',', true, false}, SelectorMarkCommand{'M', true, true})));

using SelectorMarkRange = std::tuple<SelectionMode, bool, bool>;

class SelectorMarkRangeInputTest
    : public SelectorMarkInputState,
      public testing::WithParamInterface<std::tuple<SelectorMarkRange, bool, SelectorMarkCommand>>
{
protected:
    void SetUp() override;
};

void SelectorMarkRangeInputTest::SetUp()
{
    SelectorMarkInputState::SetUp();
    const auto [mode, numeric, reverse] = std::get<0>(GetParam());
    prepare_marks(mode, std::get<1>(GetParam()));
    g_use_sel_num = numeric;
    if (mode != SM_THREAD)
    {
        --g_sel_page.page_item_cnt;
        g_sel_page.at_end = false;
    }
    g_sel_page.item_index = reverse ? g_sel_page.page_item_cnt - 1 : 0;
}

TEST_P(SelectorMarkRangeInputTest, rangeRepeatsTheMarkingActionAndRetainsSelectionsOutsideIt)
{
    const auto [mode, numeric, reverse] = std::get<0>(GetParam());
    const auto [command, killed, delayed] = std::get<2>(GetParam());
    const int         endpoint = reverse ? 0 : g_sel_page.page_item_cnt - 1;
    const std::string destination = numeric ? std::to_string(endpoint + 1) : std::string(1, g_sel_page.chars[endpoint]);
    const std::string commands = std::string(1, command) + '-' + destination;
    const std::vector<long> affected = mode == SM_THREAD ? std::vector<long>{1, 2, 3, 4} : std::vector<long>{1, 2, 3};
    const std::vector<long> subjects = mode == SM_THREAD ? std::vector<long>{1, 3, 4} : std::vector<long>{1, 3};

    ASSERT_EQ('Q', run_input(commands));

    expect_selected(mode == SM_THREAD ? std::vector<long>{} : std::vector<long>{4}, mode == SM_ARTICLE   ? 3
                                                                                    : mode == SM_SUBJECT ? 1
                                                                                                         : 0);
    expect_pending(killed && mode == SM_ARTICLE ? affected : std::vector<long>{},
                   killed && mode != SM_ARTICLE ? subjects : std::vector<long>{},
                   delayed ? affected : std::vector<long>{});
    EXPECT_EQ(0, g_sel_page.item_index);
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        EXPECT_EQ(killed ? 2 : 0, g_sel_page.items[index].sel) << index;
    }
}

INSTANTIATE_TEST_SUITE_P(
    ModeInputDirectionReadStateAndCommand, SelectorMarkRangeInputTest,
    testing::Combine(
        testing::Values(SelectorMarkRange{SM_ARTICLE, false, false}, SelectorMarkRange{SM_ARTICLE, false, true},
                        SelectorMarkRange{SM_ARTICLE, true, false}, SelectorMarkRange{SM_ARTICLE, true, true},
                        SelectorMarkRange{SM_SUBJECT, false, false}, SelectorMarkRange{SM_SUBJECT, false, true},
                        SelectorMarkRange{SM_THREAD, false, false}, SelectorMarkRange{SM_THREAD, false, true}),
        testing::Bool(),
        testing::Values(SelectorMarkCommand{'m', false, false}, SelectorMarkCommand{'k', true, false},
                        SelectorMarkCommand{'M', true, true})));

class SelectorEmptyMarkInputTest : public SelectorMarkInputState,
                                   public testing::WithParamInterface<std::tuple<bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorEmptyMarkInputTest::SetUp()
{
    SelectorMarkInputState::SetUp();
    prepare_marks(SM_ARTICLE, std::get<0>(GetParam()));
    g_sel_page.page_item_cnt = 0;
    g_sel_page.item_index = 0;
}

TEST_P(SelectorEmptyMarkInputTest, markingAnEmptyPagePreservesSelectionsAndPendingCounts)
{
    const char command = std::get<1>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_selected({1, 2, 3, 4}, 3);
    expect_pending({}, {});
    EXPECT_EQ(0, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndCommand, SelectorEmptyMarkInputTest,
                         testing::Combine(testing::Bool(), testing::Values('m', 'k', 'M')));

class SelectorGroupedInputTest : public SelectorGroupedInputState,
                                 public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorGroupedInputTest::SetUp()
{
    SelectorGroupedInputState::SetUp();
    prepare_grouped(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(SelectorGroupedInputTest, selectingARowCountsEveryEligibleArticle)
{
    select_articles({4});
    const char command = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_selected(g_sel.mode == SM_SUBJECT ? std::vector<long>{1, 2, 4} : std::vector<long>{1, 2, 3, 4},
                    g_sel.mode == SM_SUBJECT ? 2 : 3);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

TEST_P(SelectorGroupedInputTest, togglingAFullySelectedRowRetainsSelectionsOutsideItsScope)
{
    select_articles({1, 2, 3, 4});
    const char command = std::get<2>(GetParam());
    ASSERT_EQ(1, g_sel_page.items[0].sel);

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_selected(g_sel.mode == SM_SUBJECT ? std::vector<long>{3, 4} : std::vector<long>{4},
                    g_sel.mode == SM_SUBJECT ? 2 : 1);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

TEST_P(SelectorGroupedInputTest, togglingAPartiallySelectedRowCompletesItsSelection)
{
    select_articles({1, 4});
    const char command = std::get<2>(GetParam());
    ASSERT_EQ(3, g_sel_page.items[0].sel);

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_selected(g_sel.mode == SM_SUBJECT ? std::vector<long>{1, 2, 4} : std::vector<long>{1, 2, 3, 4},
                    g_sel.mode == SM_SUBJECT ? 2 : 3);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndToggle, SelectorGroupedInputTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('a', '*')));

class SelectorPartialThreadInputTest : public SelectorGroupedInputState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SelectorPartialThreadInputTest::SetUp()
{
    SelectorGroupedInputState::SetUp();
    prepare_grouped(SM_THREAD, GetParam());
}

TEST_P(SelectorPartialThreadInputTest, selectingAThreadWithOnlyASelectedReplyAlsoSelectsItsOriginalSubject)
{
    select_articles({3, 4});
    ASSERT_EQ(3, g_sel_page.items[0].sel);

    ASSERT_EQ('Q', run_input("a"));

    expect_selected({1, 2, 3, 4}, 3);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, SelectorPartialThreadInputTest, testing::Bool());

class SelectorArticleStarInputTest : public SelectorGroupedInputState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SelectorArticleStarInputTest::SetUp()
{
    SelectorGroupedInputState::SetUp();
    prepare_grouped(SM_ARTICLE, GetParam());
}

TEST_P(SelectorArticleStarInputTest, starSelectsTheCurrentSubjectWithoutSelectingOtherSubjectsInItsThread)
{
    select_articles({4});

    ASSERT_EQ('Q', run_input("*"));

    expect_selected({1, 2, 4}, 2);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{1})->m_flags & AF_DEL);
    EXPECT_EQ(0, g_cache.ptr(ArticleNum{2})->m_flags & AF_DEL);
    EXPECT_EQ(2, g_sel_page.item_index);
}

TEST_P(SelectorArticleStarInputTest, starOnASelectedArticleDeselectsTheWholeSubject)
{
    select_articles({1, 2, 3, 4});

    ASSERT_EQ('Q', run_input("*"));

    expect_selected({3, 4}, 2);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[1].sel);
    EXPECT_EQ(2, g_sel_page.item_index);
}

TEST_P(SelectorArticleStarInputTest, starOnAnUnselectedArticleCompletesAPartialSubjectSelection)
{
    select_articles({1, 4});

    ASSERT_EQ('Q', run_input("*"));

    expect_selected({1, 2, 4}, 2);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(2, g_sel_page.item_index);
}

TEST_P(SelectorArticleStarInputTest, starOnASelectedArticleClearsAPartialSubjectSelection)
{
    select_articles({2, 4});

    ASSERT_EQ('Q', run_input("*"));

    expect_selected({4}, 1);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[1].sel);
    EXPECT_EQ(2, g_sel_page.item_index);
}

TEST_P(SelectorArticleStarInputTest, starAlsoSelectsMatchingArticlesOutsideTheDisplayedPage)
{
    select_articles({3, 4});
    std::move(std::begin(g_sel_page.items) + 1, std::begin(g_sel_page.items) + 4, std::begin(g_sel_page.items));
    g_sel_page.page_item_cnt = 3;
    g_sel_page.page_obj_cnt = 3;
    g_sel_page.prior_obj_cnt = 1;
    g_sel_page.item_index = 0;

    ASSERT_EQ('Q', run_input("*"));

    expect_selected({1, 2, 3, 4}, 3);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    EXPECT_EQ(1, g_sel_page.items[2].sel);
    EXPECT_EQ(1, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, SelectorArticleStarInputTest, testing::Bool());

class SelectorObjectInputState : public SelectorInputTest
{
protected:
    void TearDown() override;
    void prepare_objects(SelectionMode mode, bool rereading);
    void select_objects(std::initializer_list<int> indices);
    void expect_objects(std::initializer_list<int> selected, int current,
                        const std::optional<std::vector<int>> &deleted = std::nullopt) const;

private:
    int  object_flags(int index) const;
    void set_object_flags(int index, int flags);

    std::array<NewsgroupData, 3>          m_groups{};
    std::array<UniversalItem, 3>          m_items{};
    const std::array<std::string_view, 3> m_names{"comp.alpha", "comp.beta", "comp.hidden"};
    SelectionMode                         m_selector_mode{};
    bool                                  m_rereading{};
    int                                   m_preserved_flags{};
    int                                   m_delete_mask{};
    std::string                           m_old_name{g_newsgroup_name};
    std::string                           m_old_directory{g_newsgroup_dir};
    bool                                  m_old_in_ng{g_in_ng};
};

void SelectorObjectInputState::TearDown()
{
    g_newsgroup_name = m_old_name;
    g_newsgroup_dir = m_old_directory;
    g_in_ng = m_old_in_ng;
    SelectorInputTest::TearDown();
}

void SelectorObjectInputState::prepare_objects(SelectionMode mode, bool rereading)
{
    m_selector_mode = mode;
    m_rereading = rereading;
    g_sel.mode = mode;
    g_sel.rereading = rereading;
    g_sel.mask = rereading ? AGF_DEL_SEL : AGF_SEL;
    g_mode = mode == SM_NEWSGROUP ? MM_NEWSGROUP_SELECTOR : MM_UNIVERSAL;
    g_sel.mode_string = mode == SM_NEWSGROUP ? "newsgroups" : "items";
    g_in_ng = false;
    g_selected_count = 0;
    g_sel_page.total_obj_cnt = 3;
    g_sel_page.prior_obj_cnt = 0;
    g_sel_page.page_obj_cnt = 2;
    g_sel_page.page_item_cnt = 2;
    g_sel_page.at_end = false;
    g_sel_page.last_line = 10;
    g_sel_page.item_index = 0;
    g_term_line = 0;
    m_delete_mask = mode == SM_NEWSGROUP ? static_cast<int>(NF_DEL) : static_cast<int>(UF_DEL);
    m_preserved_flags =
        mode == SM_NEWSGROUP ? static_cast<int>(NF_INCLUDED | NF_UNTHREADED) : static_cast<int>(UF_INCLUDED);
    m_preserved_flags |= rereading ? static_cast<int>(AGF_SEL) : static_cast<int>(AGF_DEL_SEL);
    for (int index = 0; index < 3; ++index)
    {
        m_groups[index].m_rc_line = m_names[index];
        m_groups[index].m_to_read = rereading ? 0 : index + 2;
        m_items[index].m_num = index + 1;
        m_items[index].m_desc = m_names[index];
        m_items[index].m_data = UniversalNewsgroup{std::string(m_names[index])};
        set_object_flags(index, m_preserved_flags | (rereading ? m_delete_mask : 0));
        if (index < g_sel_page.page_item_cnt)
        {
            g_sel_page.items[index] = {};
            g_sel_page.items[index].line = index + 1;
            g_sel_page.items[index].sel = rereading ? 2 : 0;
            if (mode == SM_NEWSGROUP)
            {
                g_sel_page.items[index].u.np = &m_groups[index];
            }
            else
            {
                g_sel_page.items[index].u.un = &m_items[index];
            }
        }
    }
}

int SelectorObjectInputState::object_flags(int index) const
{
    return m_selector_mode == SM_NEWSGROUP ? static_cast<int>(m_groups[index].m_flags)
                                           : static_cast<int>(m_items[index].m_flags);
}

void SelectorObjectInputState::set_object_flags(int index, int flags)
{
    if (m_selector_mode == SM_NEWSGROUP)
    {
        m_groups[index].m_flags = static_cast<NewsgroupFlags>(flags);
    }
    else
    {
        m_items[index].m_flags = static_cast<UniversalItemFlags>(flags);
    }
}

void SelectorObjectInputState::select_objects(std::initializer_list<int> indices)
{
    for (int index : indices)
    {
        set_object_flags(index, (object_flags(index) & ~m_delete_mask) | g_sel.mask);
        ++g_selected_count;
        if (index < g_sel_page.page_item_cnt)
        {
            g_sel_page.items[index].sel = 1;
        }
    }
}

void SelectorObjectInputState::expect_objects(std::initializer_list<int> selected, int current,
                                              const std::optional<std::vector<int>> &deleted) const
{
    for (int index = 0; index < 3; ++index)
    {
        const bool is_selected = std::find(selected.begin(), selected.end(), index) != selected.end();
        const bool is_deleted = deleted ? std::find(deleted->begin(), deleted->end(), index) != deleted->end()
                                        : m_rereading && !is_selected;
        const int  expected =
            m_preserved_flags | (is_selected ? static_cast<int>(g_sel.mask) : 0) | (is_deleted ? m_delete_mask : 0);
        EXPECT_EQ(expected, object_flags(index)) << index;
        if (index < g_sel_page.page_item_cnt)
        {
            EXPECT_EQ(is_selected ? 1 : is_deleted ? 2 : 0, g_sel_page.items[index].sel) << index;
        }
        if (m_selector_mode == SM_NEWSGROUP)
        {
            EXPECT_EQ(m_rereading ? 0 : index + 2, m_groups[index].m_to_read);
            EXPECT_EQ(m_names[index], m_groups[index].m_rc_line);
        }
        else
        {
            EXPECT_EQ(m_names[index], m_items[index].group().ng);
            EXPECT_EQ(m_names[index], m_items[index].m_desc);
            EXPECT_EQ(index + 1, m_items[index].m_num);
            EXPECT_EQ(UIS_NORMAL, m_items[index].m_state);
        }
    }
    EXPECT_EQ(selected.size(), g_selected_count);
    EXPECT_EQ(current, g_sel_page.item_index);
    EXPECT_EQ(m_selector_mode, g_sel.mode);
    EXPECT_EQ(m_rereading, g_sel.rereading);
    EXPECT_EQ(m_rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    if (m_selector_mode == SM_NEWSGROUP)
    {
        EXPECT_EQ(&m_groups[current], g_newsgroup_ptr);
        EXPECT_EQ(m_names[current], g_newsgroup_name);
    }
    else
    {
        EXPECT_EQ(&m_newsgroup, g_newsgroup_ptr);
        EXPECT_EQ(m_old_name, g_newsgroup_name);
        EXPECT_EQ(m_old_directory, g_newsgroup_dir);
    }
}

class SelectorObjectInvertInputTest : public SelectorObjectInputState,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
protected:
    void SetUp() override;
};

void SelectorObjectInvertInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
    select_objects({2});
}

TEST_P(SelectorObjectInvertInputTest, inversionSelectsAnExplicitlyUnmarkedObject)
{
    ASSERT_EQ('Q', run_input("m@"));

    expect_objects({0, 1, 2}, 0);
}

TEST_P(SelectorObjectInvertInputTest, inversionExchangesVisibleSelectionsAndRetainsHiddenSelections)
{
    select_objects({0});
    g_sel_page.item_index = 1;

    ASSERT_EQ('Q', run_input("@"));

    expect_objects({1, 2}, 0);
}

TEST_P(SelectorObjectInvertInputTest, inversionPreservesUnreadKillsAndSelectsReadKills)
{
    ASSERT_EQ('Q', run_input("k@"));

    if (g_sel.rereading)
    {
        expect_objects({0, 1, 2}, 0);
    }
    else
    {
        expect_objects({1, 2}, 0, std::vector<int>{0});
    }
}

TEST_P(SelectorObjectInvertInputTest, repeatingInversionRestoresVisibleSelectionsAndRetainsHiddenSelections)
{
    select_objects({0});
    g_sel_page.item_index = 1;

    ASSERT_EQ('Q', run_input("@@"));

    expect_objects({0, 2}, 0);
}

INSTANTIATE_TEST_SUITE_P(ModeAndReadState, SelectorObjectInvertInputTest,
                         testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()));

class SelectorObjectUnmarkInputTest : public SelectorObjectInputState,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorObjectUnmarkInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
    select_objects({0, 1, 2});
}

TEST_P(SelectorObjectUnmarkInputTest, unmarkCancelsTheCurrentPendingKill)
{
    const std::string commands = std::string("k") + Ctl('g') + 'a' + std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(commands));

    expect_objects({1, 2}, 1, std::vector<int>{});
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndCommand, SelectorObjectUnmarkInputTest,
                         testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool(),
                                          testing::Values('m', '|')));

using SelectorObjectMarkCommand = std::pair<char, bool>;

class SelectorObjectMarkInputTest
    : public SelectorObjectInputState,
      public testing::WithParamInterface<std::tuple<SelectionMode, bool, SelectorObjectMarkCommand>>
{
protected:
    void SetUp() override;
};

void SelectorObjectMarkInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
    select_objects({0, 1, 2});
}

TEST_P(SelectorObjectMarkInputTest, commandMarksOnlyTheCurrentObject)
{
    const auto [command, killed] = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_objects({1, 2}, 1, killed ? std::vector<int>{0} : std::vector<int>{});
}

TEST_P(SelectorObjectMarkInputTest, repeatingTheCommandPreservesCountsAndPendingMarks)
{
    const auto [command, killed] = std::get<2>(GetParam());
    const std::string commands = std::string(1, command) + Ctl('g') + 'a' + command;

    ASSERT_EQ('Q', run_input(commands));

    expect_objects({1, 2}, 1, killed ? std::vector<int>{0} : std::vector<int>{});
}

TEST_P(SelectorObjectMarkInputTest, rangeRepeatsTheMarkingActionAndPreservesHiddenSelections)
{
    const auto [command, killed] = std::get<2>(GetParam());
    const std::string commands = std::string(1, command) + "-b";

    ASSERT_EQ('Q', run_input(commands));

    expect_objects({2}, 0, killed ? std::vector<int>{0, 1} : std::vector<int>{});
}

INSTANTIATE_TEST_SUITE_P(
    ModeReadStateAndCommand, SelectorObjectMarkInputTest,
    testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool(),
                     testing::Values(SelectorObjectMarkCommand{'m', false}, SelectorObjectMarkCommand{'|', false},
                                     SelectorObjectMarkCommand{'k', true}, SelectorObjectMarkCommand{'j', true},
                                     SelectorObjectMarkCommand{',', true})));

class SelectorObjectLetterInputTest : public SelectorObjectInputState,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
protected:
    void SetUp() override;
};

void SelectorObjectLetterInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(SelectorObjectLetterInputTest, letterSelectsItsObjectRatherThanTheCurrentRow)
{
    g_sel_page.item_index = 1;

    ASSERT_EQ('Q', run_input("a"));

    expect_objects({0}, 1);
}

TEST_P(SelectorObjectLetterInputTest, lastLetterSelectsItsObjectAndWrapsToTheFirstRow)
{
    ASSERT_EQ('Q', run_input("b"));

    expect_objects({1}, 0);
}

TEST_P(SelectorObjectLetterInputTest, repeatingALetterDeselectsItsObject)
{
    ASSERT_EQ('Q', run_input("aa"));

    expect_objects({}, 1);
}

TEST_P(SelectorObjectLetterInputTest, selectingAnotherLetterRetainsTheFirstSelection)
{
    ASSERT_EQ('Q', run_input("ab"));

    expect_objects({0, 1}, 0);
}

TEST_P(SelectorObjectLetterInputTest, deselectingALetterRetainsOtherVisibleAndHiddenSelections)
{
    select_objects({0, 1, 2});

    ASSERT_EQ('Q', run_input("a"));

    expect_objects({1, 2}, 1);
}

TEST_P(SelectorObjectLetterInputTest, aDeselectedObjectCanBeReselected)
{
    ASSERT_EQ('Q', run_input("aaa"));

    expect_objects({0}, 1);
}

TEST_P(SelectorObjectLetterInputTest, aLetterBeyondThePageRetainsSelectionsAndTheCurrentRow)
{
    select_objects({0, 2});
    g_sel_page.item_index = 1;

    ASSERT_EQ('Q', run_input("c"));

    expect_objects({0, 2}, 1);
    EXPECT_NE(std::string::npos, m_output.find("No item 'c' on this page."));
}

INSTANTIATE_TEST_SUITE_P(ModeAndReadState, SelectorObjectLetterInputTest,
                         testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool()));

class SelectorObjectCurrentInputTest : public SelectorObjectInputState,
                                       public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorObjectCurrentInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
    g_sel_page.item_index = 1;
}

TEST_P(SelectorObjectCurrentInputTest, toggleSelectsTheCurrentObject)
{
    const char command = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_objects({1}, command == Ctl('t') ? 1 : 0);
}

TEST_P(SelectorObjectCurrentInputTest, toggleDeselectsTheCurrentObjectAndRetainsHiddenSelections)
{
    select_objects({1, 2});
    const char command = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_objects({2}, command == Ctl('t') ? 1 : 0);
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndToggle, SelectorObjectCurrentInputTest,
                         testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool(),
                                          testing::Values('.', 'y', '*', Ctl('t'))));

class SelectorNumericInputState : public SelectorInputTest
{
protected:
    void TearDown() override;
    void prepare_numeric(bool rereading, int page_size = 12);
    void select_rows(std::initializer_list<int> indices);
    void expect_numeric(const std::vector<int> &selected, int current) const;

private:
    Article *row_article(int index) const;

    bool m_rereading{};
    bool m_old_numeric_goto{g_sel_num_goto};
    char m_old_erase{g_erase_char};
    char m_old_kill{g_kill_char};
};

void SelectorNumericInputState::TearDown()
{
    g_sel_num_goto = m_old_numeric_goto;
    g_erase_char = m_old_erase;
    g_kill_char = m_old_kill;
    SelectorInputTest::TearDown();
}

void SelectorNumericInputState::prepare_numeric(bool rereading, int page_size)
{
    m_rereading = rereading;
    g_use_sel_num = true;
    g_sel_num_goto = false;
    g_sel.mode = SM_ARTICLE;
    g_sel.rereading = rereading;
    g_sel.mask = rereading ? AGF_DEL_SEL : AGF_SEL;
    g_erase_char = '\b';
    g_kill_char = Ctl('u');
    g_cache.article_list.clear();
    g_art_order.ptr_list.clear();
    g_art_order.ptr = nullptr;
    g_abs_first = ArticleNum{100};
    g_last_art = ArticleNum{100 + MAX_SEL};
    g_first_art = rereading ? article_after(g_last_art) : g_abs_first;
    g_cache.first_cached = g_abs_first;
    g_cache.last_cached = g_last_art;
    m_subject = {};
    m_subject.m_str = "Re: Numeric selection";
    m_subject.m_thread_link = &m_subject;
    m_subject.m_misc = MAX_SEL + 1;
    m_newsgroup.m_to_read = rereading ? 0 : MAX_SEL + 1;
    Article *previous = nullptr;
    for (int index = 0; index <= MAX_SEL; ++index)
    {
        Article *article = row_article(index);
        article->m_flags = AF_EXISTS | AF_CACHED | AF_THREADED;
        article->m_flags |= rereading ? AF_SEL | AF_DEL : AF_DEL_SEL | AF_UNREAD;
        article->m_subj = &m_subject;
        if (previous)
        {
            previous->m_subj_next = article;
        }
        previous = article;
        if (index < page_size)
        {
            article->m_flags |= AF_INCLUDED;
            g_sel_page.items[index] = {};
            g_sel_page.items[index].u.ap = article;
            g_sel_page.items[index].line = index + 1;
            g_sel_page.items[index].sel = rereading ? 2 : 0;
        }
    }
    m_subject.m_articles = row_article(0);
    m_subject.m_thread = m_subject.m_articles;
    g_subj.first = &m_subject;
    g_subj.last = &m_subject;
    g_subj.count = 1;
    g_selected_count = 0;
    g_selected_subj_cnt = 0;
    g_sel_page.item_index = 0;
    g_sel_page.total_obj_cnt = MAX_SEL + 1;
    g_sel_page.prior_obj_cnt = 0;
    g_sel_page.page_obj_cnt = page_size;
    g_sel_page.page_item_cnt = page_size;
    g_sel_page.at_end = false;
    g_sel_page.last_line = page_size + 2;
    g_tc_LINES = page_size + 7;
    g_term_line = 0;
}

Article *SelectorNumericInputState::row_article(int index) const
{
    return g_cache.ptr(ArticleNum{100 + index});
}

void SelectorNumericInputState::select_rows(std::initializer_list<int> indices)
{
    for (int index : indices)
    {
        row_article(index)->select_article(AUTO_KILL_NONE);
        if (index < g_sel_page.page_item_cnt)
        {
            g_sel_page.items[index].sel = 1;
        }
    }
}

void SelectorNumericInputState::expect_numeric(const std::vector<int> &selected, int current) const
{
    std::vector<int> actual;
    for (int index = 0; index <= MAX_SEL; ++index)
    {
        const bool   is_selected = std::find(selected.begin(), selected.end(), index) != selected.end();
        ArticleFlags expected = AF_EXISTS | AF_CACHED | AF_THREADED;
        expected |= m_rereading ? AF_SEL : AF_DEL_SEL | AF_UNREAD;
        if (is_selected)
        {
            expected |= static_cast<ArticleFlags>(g_sel.mask);
        }
        else if (m_rereading)
        {
            expected |= AF_DEL;
        }
        if (index < g_sel_page.page_item_cnt)
        {
            expected |= AF_INCLUDED;
            EXPECT_EQ(is_selected ? 1 : m_rereading ? 2 : 0, g_sel_page.items[index].sel) << index;
        }
        const Article *article = row_article(index);
        EXPECT_EQ(expected, article->m_flags) << index;
        if (article->m_flags & g_sel.mask)
        {
            actual.push_back(index);
        }
    }
    EXPECT_EQ(selected, actual);
    EXPECT_EQ(selected.size(), g_selected_count);
    EXPECT_EQ(current, g_sel_page.item_index);
    EXPECT_EQ(m_rereading, g_sel.rereading);
    EXPECT_EQ(m_rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(m_rereading ? 0 : MAX_SEL + 1, m_newsgroup.m_to_read);
}

class SelectorNumericInvertInputTest : public SelectorNumericInputState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SelectorNumericInvertInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(GetParam(), MAX_SEL);
}

TEST_P(SelectorNumericInvertInputTest, inversionIncludesBothEndsOfTheLargestPageAndRetainsHiddenSelection)
{
    select_rows({0, MAX_SEL - 1, MAX_SEL});
    g_sel_num_goto = true;
    g_sel_page.item_index = MAX_SEL - 1;
    std::vector<int> selected;
    for (int index = 1; index < MAX_SEL - 1; ++index)
    {
        selected.push_back(index);
    }
    selected.push_back(MAX_SEL);

    ASSERT_EQ('Q', run_input("@"));

    expect_numeric(selected, 0);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, SelectorNumericInvertInputTest, testing::Bool());

struct CursorMove
{
    char command;
    int  cursor;
    int  page_size;
    int  destination;
};

void PrintTo(const CursorMove &move, std::ostream *output)
{
    *output << move.command << " from " << move.cursor << " on " << move.page_size << " rows";
}

const CursorMove CURSOR_MOVES[] = {{'p', 4, 12, 3},  {'[', 4, 12, 3},  {'n', 4, 12, 5},  {']', 4, 12, 5},
                                   {'p', 0, 12, 11}, {'[', 0, 12, 11}, {'n', 11, 12, 0}, {']', 11, 12, 0},
                                   {'p', 0, 1, 0},   {'n', 0, 1, 0},   {'p', 0, 0, 0},   {'n', 0, 0, 0}};

class SelectorCursorInputTest : public SelectorNumericInputState,
                                public testing::WithParamInterface<std::tuple<bool, bool, CursorMove>>
{
protected:
    void SetUp() override;
};

void SelectorCursorInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()), std::get<2>(GetParam()).page_size);
    g_use_sel_num = std::get<1>(GetParam());
    g_sel_page.chars = "abcdefghijkl";
    g_sel_page.item_index = std::get<2>(GetParam()).cursor;
    select_rows({0, MAX_SEL});
}

TEST_P(SelectorCursorInputTest, movementRetainsVisibleAndHiddenSelections)
{
    const CursorMove &move = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(std::string(1, move.command)));

    expect_numeric({0, MAX_SEL}, move.destination);
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeAndMovement, SelectorCursorInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::ValuesIn(CURSOR_MOVES)));

struct CursorGoto
{
    std::string_view letter;
    std::string_view number;
    int              destination;
    bool             invalid;
};

void PrintTo(const CursorGoto &position, std::ostream *output)
{
    *output << position.letter << '/' << position.number;
}

const CursorGoto CURSOR_GOTOS[] = {
    {"a", "01", 0, false}, {"f", "06", 5, false}, {"l", "12", 11, false}, {"d", "04", 3, false}, {"m", "13", 3, true}};

class SelectorCursorGotoInputTest : public SelectorNumericInputState,
                                    public testing::WithParamInterface<std::tuple<bool, bool, CursorGoto>>
{
protected:
    void SetUp() override;
};

void SelectorCursorGotoInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_use_sel_num = std::get<1>(GetParam());
    g_sel_page.chars = "abcdefghijklm";
    g_sel_page.item_index = 3;
    select_rows({0, 4, MAX_SEL});
}

TEST_P(SelectorCursorGotoInputTest, explicitGotoRetainsSelectionsAndRejectsAbsentRows)
{
    const CursorGoto &position = std::get<2>(GetParam());
    const std::string commands =
        std::string(1, Ctl('g')) + std::string(g_use_sel_num ? position.number : position.letter);

    ASSERT_EQ('Q', run_input(commands));

    expect_numeric({0, 4, MAX_SEL}, position.destination);
    EXPECT_EQ(position.invalid, m_output.find("No item") != std::string::npos) << m_output;
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeAndDestination, SelectorCursorGotoInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::ValuesIn(CURSOR_GOTOS)));

class SelectorGroupedCursorInputTest : public SelectorGroupedInputState,
                                       public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorGroupedCursorInputTest::SetUp()
{
    SelectorGroupedInputState::SetUp();
    prepare_grouped(std::get<0>(GetParam()), std::get<1>(GetParam()));
    select_articles({1, 4});
}

TEST_P(SelectorGroupedCursorInputTest, movementPreservesPartialGroupsAndOppositeScopeSelections)
{
    const char       command = std::get<2>(GetParam());
    const int        destination = command == 'p' || command == '[' ? g_sel_page.page_item_cnt - 1 : 1;
    std::vector<int> markers;
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        markers.push_back(g_sel_page.items[index].sel);
    }

    ASSERT_EQ('Q', run_input(std::string(1, command)));

    expect_selected({1, 4}, 2);
    EXPECT_EQ(destination, g_sel_page.item_index);
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        EXPECT_EQ(markers[index], g_sel_page.items[index].sel) << index;
    }
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndMovement, SelectorGroupedCursorInputTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('p', '[', 'n', ']')));

struct ObjectCursorMove
{
    std::string_view commands;
    int              cursor;
    int              destination;
    bool             invalid;
};

void PrintTo(const ObjectCursorMove &move, std::ostream *output)
{
    *output << testing::PrintToString(move.commands) << " from " << move.cursor;
}

const ObjectCursorMove OBJECT_CURSOR_MOVES[] = {{"p", 0, 1, false}, {"[", 0, 1, false},     {"n", 1, 0, false},
                                                {"]", 1, 0, false}, {"\007b", 0, 1, false}, {"\007c", 0, 0, true}};

class SelectorObjectCursorInputTest
    : public SelectorObjectInputState,
      public testing::WithParamInterface<std::tuple<SelectionMode, bool, ObjectCursorMove>>
{
protected:
    void SetUp() override;
};

void SelectorObjectCursorInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
    select_objects({0, 2});
    g_sel_page.item_index = std::get<2>(GetParam()).cursor;
}

TEST_P(SelectorObjectCursorInputTest, movementPreservesObjectsAndHiddenSelections)
{
    const ObjectCursorMove &move = std::get<2>(GetParam());

    ASSERT_EQ('Q', run_input(move.commands));

    expect_objects({0, 2}, move.destination);
    EXPECT_EQ(move.invalid, m_output.find("No item 'c' on this page.") != std::string::npos) << m_output;
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndMovement, SelectorObjectCursorInputTest,
                         testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool(),
                                          testing::ValuesIn(OBJECT_CURSOR_MOVES)));

struct CursorRange
{
    std::string_view name;
    int              cursor;
    std::string_view movement;
    int              endpoint;
    std::vector<int> selected;
    int              after;
};

void PrintTo(const CursorRange &range, std::ostream *output)
{
    *output << range.name;
}

const CursorRange CURSOR_RANGES[] = {
    {"initial forward", 3, "", 6, {3, 4, 5, 6}, 7}, {"initial backward", 6, "", 3, {3, 4, 5, 6}, 7},
    {"initial last row", 11, "", 10, {10, 11}, 0},  {"initial first row", 0, "", 3, {0, 1, 2, 3}, 4},
    {"moved forward", 2, "n", 6, {3, 4, 5, 6}, 7},  {"moved backward", 7, "p", 3, {3, 4, 5, 6}, 7},
    {"wrapped backward", 0, "[", 10, {10, 11}, 0},  {"wrapped forward", 11, "]", 3, {0, 1, 2, 3}, 4},
};

class SelectorCursorRangeInputTest : public SelectorNumericInputState,
                                     public testing::WithParamInterface<std::tuple<bool, bool, CursorRange>>
{
protected:
    void SetUp() override;
};

void SelectorCursorRangeInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_use_sel_num = std::get<1>(GetParam());
    g_sel_page.chars = "abcdefghijkl";
    g_sel_page.item_index = std::get<2>(GetParam()).cursor;
    select_rows({MAX_SEL});
}

TEST_P(SelectorCursorRangeInputTest, rangeIncludesTheUnmarkedCursorAndItsDestination)
{
    const CursorRange &range = std::get<2>(GetParam());
    const std::string  destination =
        g_use_sel_num ? std::to_string(range.endpoint + 1) : std::string(1, g_sel_page.chars[range.endpoint]);
    const std::string commands = std::string(range.movement) + '-' + destination;
    std::vector<int>  selected = range.selected;
    selected.push_back(MAX_SEL);

    ASSERT_EQ('Q', run_input(commands));

    expect_numeric(selected, range.after);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeAndCursor, SelectorCursorRangeInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::ValuesIn(CURSOR_RANGES)));

class SelectorCursorRangeResetInputTest : public SelectorNumericInputState,
                                          public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
protected:
    void SetUp() override;
};

void SelectorCursorRangeResetInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_use_sel_num = std::get<1>(GetParam());
    g_sel_page.chars = "abcdefghijkl";
    if (std::get<2>(GetParam()))
    {
        select_rows({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, MAX_SEL});
    }
    else
    {
        select_rows({MAX_SEL});
    }
}

TEST_P(SelectorCursorRangeResetInputTest, movementStartsTheNextRangeAtTheNewCursorAndRetainsItsAction)
{
    const std::string      commands = g_use_sel_num ? "03n-03" : "cn-c";
    const std::vector<int> selected = std::get<2>(GetParam()) ? std::vector<int>{0, 1, 5, 6, 7, 8, 9, 10, 11, MAX_SEL}
                                                              : std::vector<int>{2, 3, 4, MAX_SEL};

    ASSERT_EQ('Q', run_input(commands));

    expect_numeric(selected, 5);
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeAndDeselection, SelectorCursorRangeResetInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

using NumericPosition = std::pair<std::string_view, int>;

class SelectorNumericPositionInputTest : public SelectorNumericInputState,
                                         public testing::WithParamInterface<std::tuple<bool, NumericPosition>>
{
protected:
    void SetUp() override;
};

void SelectorNumericPositionInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
}

TEST_P(SelectorNumericPositionInputTest, numberSelectsThePagePositionAndAdvances)
{
    const auto &[input, index] = std::get<1>(GetParam());
    select_rows({MAX_SEL});
    g_sel_page.item_index = 4;

    ASSERT_EQ('Q', run_input(input));

    expect_numeric({index, MAX_SEL}, (index + 1) % g_sel_page.page_item_cnt);
}

TEST_P(SelectorNumericPositionInputTest, numberDeselectsItsSelectedRowAndRetainsHiddenSelections)
{
    const auto &[input, index] = std::get<1>(GetParam());
    select_rows({index, MAX_SEL});

    ASSERT_EQ('Q', run_input(input));

    expect_numeric({MAX_SEL}, (index + 1) % g_sel_page.page_item_cnt);
}

TEST_P(SelectorNumericPositionInputTest, numericGotoMovesWithoutChangingSelections)
{
    const auto &[input, index] = std::get<1>(GetParam());
    select_rows({0, MAX_SEL});
    g_sel_num_goto = true;

    ASSERT_EQ('Q', run_input(input));

    expect_numeric({0, MAX_SEL}, index);
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndPosition, SelectorNumericPositionInputTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(NumericPosition{"1", 0}, NumericPosition{"9", 8},
                                                          NumericPosition{"01", 0}, NumericPosition{"09", 8},
                                                          NumericPosition{"10", 9}, NumericPosition{"12", 11})));

using InvalidNumericPosition = std::pair<std::string_view, std::string_view>;

class SelectorNumericInvalidInputTest : public SelectorNumericInputState,
                                        public testing::WithParamInterface<std::tuple<bool, InvalidNumericPosition>>
{
protected:
    void SetUp() override;
};

void SelectorNumericInvalidInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
}

TEST_P(SelectorNumericInvalidInputTest, invalidNumberReportsItsPositionAndRetainsSelectionsAndCursor)
{
    const auto &[input, message] = std::get<1>(GetParam());
    select_rows({0, MAX_SEL});
    g_sel_page.item_index = 4;

    ASSERT_EQ('Q', run_input(input));

    expect_numeric({0, MAX_SEL}, 4);
    EXPECT_NE(std::string::npos, m_output.find(message));
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndInvalidPosition, SelectorNumericInvalidInputTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(InvalidNumericPosition{"0", "No item 0 on this page."},
                                                          InvalidNumericPosition{"00", "No item 0 on this page."},
                                                          InvalidNumericPosition{"13", "No item 13 on this page."},
                                                          InvalidNumericPosition{"99", "No item 99 on this page."})));

class SelectorNumericReadInputTest : public SelectorNumericInputState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SelectorNumericReadInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(GetParam());
}

TEST_P(SelectorNumericReadInputTest, nonDigitContinuationExecutesAsTheNextCommand)
{
    ASSERT_EQ('Q', run_input("1."));

    expect_numeric({0, 1}, 2);
}

TEST_P(SelectorNumericReadInputTest, aThirdDigitStartsTheNextNumber)
{
    ASSERT_EQ('Q', run_input("123"));

    expect_numeric({2, 11}, 3);
}

TEST_P(SelectorNumericReadInputTest, aSingleDigitPreservesTheFollowingQuitAndQueuedInput)
{
    ASSERT_EQ('Q', run_input("1qZ"));

    expect_numeric({0}, 1);
    ASSERT_TRUE(macro_pending());
    const std::string next = get_cmd();
    ASSERT_FALSE(next.empty());
    EXPECT_EQ('Z', next.front());
}

TEST_P(SelectorNumericReadInputTest, maximumTwoDigitPositionSelectsTheLastRowAndWraps)
{
    prepare_numeric(GetParam(), MAX_SEL);
    select_rows({MAX_SEL});

    ASSERT_EQ('Q', run_input("99"));

    expect_numeric({MAX_SEL - 1, MAX_SEL}, 0);
}

TEST_P(SelectorNumericReadInputTest, aRangeSelectsItsEndpointWhenNumericGotoIsEnabled)
{
    select_rows({MAX_SEL});
    g_sel_num_goto = true;

    ASSERT_EQ('Q', run_input("02.-05"));

    expect_numeric({1, 2, 3, 4, MAX_SEL}, 5);
}

TEST_P(SelectorNumericReadInputTest, aSingleDigitRangeEndpointPreservesTheFollowingCommand)
{
    select_rows({MAX_SEL});

    ASSERT_EQ('Q', run_input("02-5."));

    expect_numeric({1, 2, 3, 4, 5, MAX_SEL}, 6);
}

TEST_P(SelectorNumericReadInputTest, chainedRangesRetainTheLastSelectionAction)
{
    select_rows({MAX_SEL});

    ASSERT_EQ('Q', run_input("02-05-08"));

    expect_numeric({1, 2, 3, 4, 5, 6, 7, MAX_SEL}, 8);
}

TEST_P(SelectorNumericReadInputTest, chainedRangesRetainTheLastDeselectionAction)
{
    select_rows({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, MAX_SEL});

    ASSERT_EQ('Q', run_input("02-05-08"));

    expect_numeric({0, 8, 9, 10, 11, MAX_SEL}, 8);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, SelectorNumericReadInputTest, testing::Bool());

class SelectorNumericCancelInputTest : public SelectorNumericInputState,
                                       public testing::WithParamInterface<std::tuple<bool, char>>
{
protected:
    void SetUp() override;
};

void SelectorNumericCancelInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
}

TEST_P(SelectorNumericCancelInputTest, cancellingTheFirstDigitAllowsAFreshNumber)
{
    const std::string input = std::string("1") + std::get<1>(GetParam()) + "2";

    ASSERT_EQ('Q', run_input(input));

    expect_numeric({1}, 2);
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndCancelKey, SelectorNumericCancelInputTest,
                         testing::Combine(testing::Bool(), testing::Values('\b', Ctl('u'))));

using PromptReply = std::pair<std::string, std::string>;

class SelectorNumericPromptState : public SelectorNumericInputState
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    char        answer_after_prompt(std::string_view commands, const std::vector<PromptReply> &replies);
    std::string prompt_output(std::string_view prompt, int cursor) const;
    void        expect_prompt_cleared(std::string_view prompt, int cursor) const;

private:
    void read_output(const std::vector<PromptReply> &replies);

    std::array<int, 2> m_input_pipe{-1, -1};
    std::array<int, 2> m_output_pipe{-1, -1};
    int                m_saved_stdin{-1};
    std::size_t        m_answers{};
    bool               m_write_failed{};
    bool               m_old_allow_typeahead{g_allow_typeahead};
    bool               m_old_muck_up_clear{g_muck_up_clear};
    std::string_view   m_old_tc_cm{g_tc_CM};
    std::string_view   m_old_tc_ho{g_tc_HO};
    std::string_view   m_old_tc_up{g_tc_UP};
};

void SelectorNumericPromptState::SetUp()
{
    SelectorInputTest::SetUp();
#ifdef I_UNISTD
    ASSERT_EQ(0, pipe(m_input_pipe.data()));
    ASSERT_EQ(0, pipe(m_output_pipe.data()));
#else
    ASSERT_EQ(0, _pipe(m_input_pipe.data(), 4096, _O_BINARY));
    ASSERT_EQ(0, _pipe(m_output_pipe.data(), 4096, _O_BINARY));
#endif
    m_saved_stdin = dup(fileno(stdin));
    ASSERT_GE(m_saved_stdin, 0);
    ASSERT_GE(dup2(m_input_pipe[0], fileno(stdin)), 0);
    std::clearerr(stdin);
    g_allow_typeahead = true;
    g_can_home = true;
    g_muck_up_clear = false;
    g_tc_CM = "[%d]";
    g_tc_HO = "";
    g_tc_UP = "";
    g_tc_CE = "<erase>";
}

void SelectorNumericPromptState::TearDown()
{
    if (m_saved_stdin >= 0)
    {
        EXPECT_GE(dup2(m_saved_stdin, fileno(stdin)), 0);
        close(m_saved_stdin);
        std::clearerr(stdin);
    }
    for (int fd : {m_input_pipe[0], m_input_pipe[1], m_output_pipe[0], m_output_pipe[1]})
    {
        if (fd >= 0)
        {
            close(fd);
        }
    }
    g_allow_typeahead = m_old_allow_typeahead;
    g_muck_up_clear = m_old_muck_up_clear;
    g_tc_CM = m_old_tc_cm;
    g_tc_HO = m_old_tc_ho;
    g_tc_UP = m_old_tc_up;
    SelectorNumericInputState::TearDown();
}

void SelectorNumericPromptState::read_output(const std::vector<PromptReply> &replies)
{
    char ch;
    while (read(m_output_pipe[0], &ch, 1) == 1)
    {
        m_output += ch;
        if (m_answers < replies.size() && m_output.size() >= replies[m_answers].first.size() &&
            m_output.compare(m_output.size() - replies[m_answers].first.size(), replies[m_answers].first.size(),
                             replies[m_answers].first) == 0)
        {
            std::string input = replies[m_answers].second;
            for (char &key : input)
            {
                key ^= 0200;
            }
            m_write_failed |= write(m_input_pipe[1], input.data(), input.size()) != static_cast<int>(input.size());
            ++m_answers;
        }
    }
}

char SelectorNumericPromptState::answer_after_prompt(std::string_view commands, const std::vector<PromptReply> &replies)
{
    push_string(commands, 0200);
    std::fflush(stdout);
    const int saved_stdout = dup(fileno(stdout));
    EXPECT_GE(saved_stdout, 0);
    EXPECT_GE(dup2(m_output_pipe[1], fileno(stdout)), 0);
    close(m_output_pipe[1]);
    m_output_pipe[1] = -1;
    std::thread responder(&SelectorNumericPromptState::read_output, this, std::cref(replies));
    const char  result = g_sel_session.input();
    std::fflush(stdout);
    EXPECT_GE(dup2(saved_stdout, fileno(stdout)), 0);
    close(saved_stdout);
    responder.join();
    EXPECT_FALSE(m_write_failed);
    EXPECT_EQ(replies.size(), m_answers);
    return result;
}

std::string SelectorNumericPromptState::prompt_output(std::string_view prompt, int cursor) const
{
    return tgoto_string(g_tc_CM, 0, g_sel_page.last_line + 1) + std::string(prompt) +
           tgoto_string(g_tc_CM, 0, g_sel_page.items[cursor].line);
}

void SelectorNumericPromptState::expect_prompt_cleared(std::string_view prompt, int cursor) const
{
    const std::string trace = std::string(prompt) + tgoto_string(g_tc_CM, 0, g_sel_page.items[cursor].line) +
                              tgoto_string(g_tc_CM, 0, g_sel_page.last_line + 1) + "\r<erase>\r";
    EXPECT_NE(std::string::npos, m_output.find(trace)) << m_output;
}

struct CursorRangePrompt
{
    int              cursor;
    std::string_view movement;
    int              origin;
};

void PrintTo(const CursorRangePrompt &prompt, std::ostream *output)
{
    *output << "cursor " << prompt.cursor << ", movement " << prompt.movement << ", origin " << prompt.origin;
}

const CursorRangePrompt CURSOR_RANGE_PROMPTS[] = {
    {0, "", 1},
    {4, "", 5},
    {2, "n", 4},
    {0, "p", 12},
};

class SelectorCursorRangePromptInputTest : public SelectorNumericPromptState,
                                           public testing::WithParamInterface<std::tuple<bool, CursorRangePrompt>>
{
protected:
    void SetUp() override;
    void expect_completed_range() const;
    void expect_range_prompt(std::string_view suffix) const;
};

void SelectorCursorRangePromptInputTest::SetUp()
{
    SelectorNumericPromptState::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_sel_page.item_index = std::get<1>(GetParam()).cursor;
    select_rows({MAX_SEL});
}

void SelectorCursorRangePromptInputTest::expect_completed_range() const
{
    const int        origin = std::get<1>(GetParam()).origin;
    std::vector<int> selected;
    for (int index = origin - 1; index < 12; ++index)
    {
        selected.push_back(index);
    }
    selected.push_back(MAX_SEL);
    expect_numeric(selected, 0);
}

void SelectorCursorRangePromptInputTest::expect_range_prompt(std::string_view suffix) const
{
    const int         origin = std::get<1>(GetParam()).origin;
    const std::string label = g_use_sel_num ? std::to_string(origin) : std::string(1, g_sel_page.chars[origin - 1]);
    const std::string prompt = "Range: " + label + std::string(suffix);
    const std::string trace = prompt + tgoto_string(g_tc_CM, 0, g_sel_page.items[origin - 1].line);
    EXPECT_NE(std::string::npos, m_output.find(trace)) << m_output;
}

TEST_P(SelectorCursorRangePromptInputTest, rangePromptNamesTheCurrentCursorBeforeDestinationInput)
{
    const CursorRangePrompt &range = std::get<1>(GetParam());
    const std::string        commands = std::string(range.movement) + '-';

    ASSERT_EQ('Q', answer_after_prompt(commands, {{"Range: ", "12q"}}));

    expect_completed_range();
    expect_range_prompt("-");
}

TEST_P(SelectorCursorRangePromptInputTest, letterRangePromptNamesTheCurrentCursorBeforeDestinationInput)
{
    g_use_sel_num = false;
    g_sel_page.chars = "abcdefghijkl";
    const std::string commands = std::string(std::get<1>(GetParam()).movement) + '-';

    ASSERT_EQ('Q', answer_after_prompt(commands, {{"Range: ", "lq"}}));

    expect_completed_range();
    expect_range_prompt("-");
}

TEST_P(SelectorCursorRangePromptInputTest, rangePromptNamesTheCurrentCursorWhileReadingADestination)
{
    const CursorRangePrompt &range = std::get<1>(GetParam());
    const std::string        commands = std::string(range.movement) + "-1";

    ASSERT_EQ('Q', answer_after_prompt(commands, {{"Range: ", "2q"}}));

    expect_completed_range();
    expect_range_prompt("-1");
}

TEST_P(SelectorCursorRangePromptInputTest, erasingTheDestinationRetainsTheCurrentCursorInTheRangePrompt)
{
    const CursorRangePrompt &range = std::get<1>(GetParam());
    const std::string        commands = std::string(range.movement) + "-1";

    ASSERT_EQ('Q', answer_after_prompt(commands, {{"Range: ", "\b"}, {"Range: ", "12q"}}));

    expect_completed_range();
    expect_range_prompt("- ");
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndCursor, SelectorCursorRangePromptInputTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(CURSOR_RANGE_PROMPTS)));

struct NumericPrompt
{
    std::string_view commands;
    std::string_view prompt;
    std::string_view erased;
    int              cursor;
    std::vector<int> completed_rows;
    int              completed_cursor;
};

void PrintTo(const NumericPrompt &prompt, std::ostream *output)
{
    *output << prompt.prompt << " at cursor " << prompt.cursor;
}

const NumericPrompt NUMERIC_PROMPTS[] = {
    {"1", "1", "", 0, {11}, 0},
    {"\0071", "Go to number: 1", "Go to number?  ", 0, {}, 11},
    {"-1", "Range: 1-1", "Range: 1- ", 0, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 0},
    {"-1", "Range: 5-1", "Range: 5- ", 4, {4, 5, 6, 7, 8, 9, 10, 11}, 0},
};

class SelectorNumericPromptInputTest : public SelectorNumericPromptState,
                                       public testing::WithParamInterface<std::tuple<bool, NumericPrompt>>
{
protected:
    void SetUp() override;
};

void SelectorNumericPromptInputTest::SetUp()
{
    SelectorNumericPromptState::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_sel_page.item_index = std::get<1>(GetParam()).cursor;
}

TEST_P(SelectorNumericPromptInputTest, completingTheNumberClearsTheDisplayedFirstDigit)
{
    const NumericPrompt &scenario = std::get<1>(GetParam());
    select_rows({MAX_SEL});
    std::vector<int> selected = scenario.completed_rows;
    selected.push_back(MAX_SEL);

    ASSERT_EQ('Q', answer_after_prompt(scenario.commands, {{prompt_output(scenario.prompt, scenario.cursor), "2q"}}));

    expect_numeric(selected, scenario.completed_cursor);
    expect_prompt_cleared(scenario.prompt, scenario.cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndPrompt, SelectorNumericPromptInputTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(NUMERIC_PROMPTS)));

class SelectorNumericPromptCancelInputTest : public SelectorNumericPromptState,
                                             public testing::WithParamInterface<std::tuple<bool, NumericPrompt, char>>
{
protected:
    void SetUp() override;
};

void SelectorNumericPromptCancelInputTest::SetUp()
{
    SelectorNumericPromptState::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_sel_page.item_index = std::get<1>(GetParam()).cursor;
}

TEST_P(SelectorNumericPromptCancelInputTest, cancellingTheNumberClearsItsDigitAndPreservesSelections)
{
    const auto &[rereading, scenario, cancel] = GetParam();
    select_rows({MAX_SEL});
    const std::string input = std::string(1, cancel) + 'q';

    ASSERT_EQ('Q', answer_after_prompt(scenario.commands, {{prompt_output(scenario.prompt, scenario.cursor), input}}));

    expect_numeric({MAX_SEL}, scenario.cursor);
    expect_prompt_cleared(scenario.prompt, scenario.cursor);
    if (!scenario.erased.empty())
    {
        if (cancel == g_erase_char)
        {
            expect_prompt_cleared(scenario.erased, scenario.cursor);
        }
        else
        {
            EXPECT_EQ(std::string::npos, m_output.find(scenario.erased));
        }
    }
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

INSTANTIATE_TEST_SUITE_P(ReadStatePromptAndCancel, SelectorNumericPromptCancelInputTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(NUMERIC_PROMPTS),
                                          testing::Values('\b', Ctl('u'))));

struct SelectionRange
{
    std::string_view letters;
    std::string_view numbers;
    std::vector<int> rows;
    int              cursor;
};

void PrintTo(const SelectionRange &range, std::ostream *output)
{
    *output << range.letters;
}

const SelectionRange SELECTION_RANGES[] = {
    {"b-f", "02-06", {1, 2, 3, 4, 5}, 6},
    {"f-b", "06-02", {1, 2, 3, 4, 5}, 6},
    {"f-f", "06-06", {5}, 6},
    {"e-f", "05-06", {4, 5}, 6},
    {"f-e", "06-05", {4, 5}, 6},
    {"a-l", "01-12", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 0},
    {"l-a", "12-01", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, 0},
    {"l-l", "12-12", {11}, 0},
    {"l-k", "12-11", {10, 11}, 0},
    {"k-l", "11-12", {10, 11}, 0},
};

class SelectorRangeInputTest : public SelectorNumericInputState,
                               public testing::WithParamInterface<std::tuple<bool, bool, SelectionRange>>
{
protected:
    void             SetUp() override;
    std::string_view commands() const;
    void             prepare_range_selection();
};

void SelectorRangeInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_use_sel_num = std::get<1>(GetParam());
    g_sel_page.chars = "abcdefghijkl";
}

std::string_view SelectorRangeInputTest::commands() const
{
    const SelectionRange &range = std::get<2>(GetParam());
    return g_use_sel_num ? range.numbers : range.letters;
}

void SelectorRangeInputTest::prepare_range_selection()
{
    const SelectionRange &range = std::get<2>(GetParam());
    select_rows({MAX_SEL});
    if (range.letters.front() != range.letters.back())
    {
        select_rows({range.letters.back() - 'a'});
    }
}

TEST_P(SelectorRangeInputTest, rangeRepeatsSelectionAcrossItsInclusiveEndpoints)
{
    prepare_range_selection();
    const SelectionRange &range = std::get<2>(GetParam());
    std::vector<int>      selected = range.rows;
    selected.push_back(MAX_SEL);

    ASSERT_EQ('Q', run_input(commands()));

    expect_numeric(selected, range.cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

TEST_P(SelectorRangeInputTest, rangeRepeatsDeselectionAndPreservesSelectionsOutsideIt)
{
    select_rows({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, MAX_SEL});
    const SelectionRange &range = std::get<2>(GetParam());
    std::vector<int>      selected;
    for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
    {
        if (std::find(range.rows.begin(), range.rows.end(), index) == range.rows.end())
        {
            selected.push_back(index);
        }
    }
    selected.push_back(MAX_SEL);

    ASSERT_EQ('Q', run_input(commands()));

    expect_numeric(selected, range.cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeAndEndpoints, SelectorRangeInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::ValuesIn(SELECTION_RANGES)));

class SelectorGotoRangeInputTest : public SelectorRangeInputTest
{
protected:
    void SetUp() override;
};

void SelectorGotoRangeInputTest::SetUp()
{
    SelectorRangeInputTest::SetUp();
    g_sel_num_goto = g_use_sel_num;
}

TEST_P(SelectorGotoRangeInputTest, aRangeFromAnUnmarkedGotoDestinationIncludesBothEndpoints)
{
    select_rows({MAX_SEL});
    const SelectionRange &range = std::get<2>(GetParam());
    const std::string     input =
        g_use_sel_num ? std::string(commands()) : std::string(1, Ctl('g')) + std::string(commands());
    std::vector<int> selected = range.rows;
    selected.push_back(MAX_SEL);

    ASSERT_EQ('Q', run_input(input));

    expect_numeric(selected, range.cursor);
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeAndEndpoints, SelectorGotoRangeInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::ValuesIn(SELECTION_RANGES)));

class SelectorGotoRangePromptInputTest : public SelectorNumericPromptState, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void SelectorGotoRangePromptInputTest::SetUp()
{
    SelectorNumericPromptState::SetUp();
    prepare_numeric(GetParam());
    g_sel_num_goto = true;
}

TEST_P(SelectorGotoRangePromptInputTest, theRangePromptNamesTheUnmarkedGotoDestination)
{
    select_rows({MAX_SEL});

    ASSERT_EQ('Q', answer_after_prompt("04-1", {{prompt_output("Range: 4-1", 3), "2q"}}));

    expect_numeric({3, 4, 5, 6, 7, 8, 9, 10, 11, MAX_SEL}, 0);
    expect_prompt_cleared("Range: 4-1", 3);
}

INSTANTIATE_TEST_SUITE_P(ReadAndUnread, SelectorGotoRangePromptInputTest, testing::Bool());

class SelectorGroupedRangeInputTest : public SelectorGroupedInputState,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool, bool>>
{
protected:
    void        SetUp() override;
    std::string commands() const;
};

void SelectorGroupedRangeInputTest::SetUp()
{
    SelectorGroupedInputState::SetUp();
    prepare_grouped(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

std::string SelectorGroupedRangeInputTest::commands() const
{
    return std::get<2>(GetParam()) ? "b-a" : "a-b";
}

TEST_P(SelectorGroupedRangeInputTest, rangeSelectsAllEligibleArticlesInItsSubjectOrThreadRows)
{
    select_articles({1});

    ASSERT_EQ('Q', run_input(commands()));

    expect_selected(g_sel.mode == SM_SUBJECT ? std::vector<long>{1, 2, 3} : std::vector<long>{1, 2, 3, 4},
                    g_sel.mode == SM_SUBJECT ? 2 : 3);
    EXPECT_EQ(1, g_sel_page.items[0].sel);
    EXPECT_EQ(1, g_sel_page.items[1].sel);
    if (g_sel.mode == SM_SUBJECT)
    {
        EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[2].sel);
    }
    EXPECT_EQ(g_sel.mode == SM_SUBJECT ? 2 : 0, g_sel_page.item_index);
}

TEST_P(SelectorGroupedRangeInputTest, rangeDeselectsWholeRowsAndPreservesOtherSubjectsAndReadScopes)
{
    select_articles({1, 2, 3, 4});

    ASSERT_EQ('Q', run_input(commands()));

    expect_selected(g_sel.mode == SM_SUBJECT ? std::vector<long>{4} : std::vector<long>{},
                    g_sel.mode == SM_SUBJECT ? 1 : 0);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[0].sel);
    EXPECT_EQ(g_sel.rereading ? 2 : 0, g_sel_page.items[1].sel);
    if (g_sel.mode == SM_SUBJECT)
    {
        EXPECT_EQ(1, g_sel_page.items[2].sel);
    }
    EXPECT_EQ(g_sel.mode == SM_SUBJECT ? 2 : 0, g_sel_page.item_index);
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndDirection, SelectorGroupedRangeInputTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool(), testing::Bool()));

class SelectorObjectRangeInputTest : public SelectorObjectInputState,
                                     public testing::WithParamInterface<std::tuple<SelectionMode, bool, bool>>
{
protected:
    void        SetUp() override;
    std::string commands() const;
};

void SelectorObjectRangeInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_objects(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

std::string SelectorObjectRangeInputTest::commands() const
{
    return std::get<2>(GetParam()) ? "b-a" : "a-b";
}

TEST_P(SelectorObjectRangeInputTest, rangeSelectsItsGroupsOrItemsAndRetainsHiddenSelections)
{
    select_objects({2});

    ASSERT_EQ('Q', run_input(commands()));

    expect_objects({0, 1, 2}, 0);
}

TEST_P(SelectorObjectRangeInputTest, rangeDeselectsItsGroupsOrItemsAndRetainsHiddenSelections)
{
    select_objects({0, 1, 2});

    ASSERT_EQ('Q', run_input(commands()));

    expect_objects({2}, 0);
}

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndDirection, SelectorObjectRangeInputTest,
                         testing::Combine(testing::Values(SM_NEWSGROUP, SM_UNIVERSAL), testing::Bool(),
                                          testing::Bool()));

using SelectorCancelKey = std::pair<char, bool>;

class SelectorPrefixCancelInputTest
    : public SelectorNumericPromptState,
      public testing::WithParamInterface<std::tuple<bool, bool, SelectorCancelKey, char>>
{
protected:
    void             SetUp() override;
    std::string      prefix() const;
    std::string      prompt() const;
    std::string      fresh_selection() const;
    std::vector<int> retained_selections() const;
    int              retained_cursor() const;
    char             cancel_key() const;
};

void SelectorPrefixCancelInputTest::SetUp()
{
    SelectorNumericPromptState::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    g_use_sel_num = std::get<1>(GetParam());
    g_sel_page.chars = "abcdefghijkl";
    const auto [key, kill] = std::get<2>(GetParam());
    if (kill)
    {
        g_kill_char = key;
    }
    else
    {
        g_erase_char = key;
    }
    select_rows({0, 3, MAX_SEL});
    g_sel_page.item_index = 4;
}

std::string SelectorPrefixCancelInputTest::prefix() const
{
    return std::get<3>(GetParam()) == '-' ? (g_use_sel_num ? "02-" : "b-") : std::string(1, Ctl('g'));
}

std::string SelectorPrefixCancelInputTest::prompt() const
{
    return std::get<3>(GetParam()) == '-' ? (g_use_sel_num ? "Range: 2-" : "Range: b-")
                                          : (g_use_sel_num ? "Go to number?" : "Go to letter?");
}

std::string SelectorPrefixCancelInputTest::fresh_selection() const
{
    return g_use_sel_num ? "06" : "f";
}

std::vector<int> SelectorPrefixCancelInputTest::retained_selections() const
{
    return std::get<3>(GetParam()) == '-' ? std::vector<int>{0, 1, 3, MAX_SEL} : std::vector<int>{0, 3, MAX_SEL};
}

int SelectorPrefixCancelInputTest::retained_cursor() const
{
    return std::get<3>(GetParam()) == '-' ? 2 : 4;
}

char SelectorPrefixCancelInputTest::cancel_key() const
{
    return std::get<2>(GetParam()).first;
}

TEST_P(SelectorPrefixCancelInputTest, cancellingBeforeADestinationRetainsTheCursorAndExistingSelections)
{
    ASSERT_EQ('Q', run_input(prefix() + cancel_key()));

    expect_numeric(retained_selections(), retained_cursor());
}

TEST_P(SelectorPrefixCancelInputTest, cancellationAllowsTheNextInputToSelectASingleRow)
{
    std::vector<int> selected = retained_selections();
    selected.insert(selected.end() - 1, 5);

    ASSERT_EQ('Q', run_input(prefix() + cancel_key() + fresh_selection()));

    expect_numeric(selected, 6);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

TEST_P(SelectorPrefixCancelInputTest, cancellingADisplayedPrefixClearsItsPromptBeforeTheNextSelection)
{
    const std::string message = prompt();
    const int         cursor = retained_cursor();
    std::vector<int>  selected = retained_selections();
    selected.insert(selected.end() - 1, 5);
    const std::string answer = std::string(1, cancel_key()) + fresh_selection() + 'q';

    ASSERT_EQ('Q', answer_after_prompt(prefix(), {{prompt_output(message, cursor), answer}}));

    expect_numeric(selected, 6);
    expect_prompt_cleared(message, cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

INSTANTIATE_TEST_SUITE_P(ReadStateInputModeKeyAndPrefix, SelectorPrefixCancelInputTest,
                         testing::Combine(testing::Bool(), testing::Bool(),
                                          testing::Values(SelectorCancelKey{'\b', false},
                                                          SelectorCancelKey{'\177', false},
                                                          SelectorCancelKey{Ctl('u'), true}),
                                          testing::Values('-', Ctl('g'))));

struct SelectorNumericEdit
{
    std::string_view prefix;
    std::string_view replacement;
    std::vector<int> resumed;
    int              resumed_cursor;
    std::vector<int> cancelled;
    int              cancelled_cursor;
};

void PrintTo(const SelectorNumericEdit &edit, std::ostream *output)
{
    *output << testing::PrintToString(std::string(edit.prefix)) << " then " << edit.replacement;
}

const SelectorNumericEdit NUMERIC_EDITS[] = {
    {"02-1", "06", {0, 1, 2, 3, 4, 5, MAX_SEL}, 6, {0, 1, 5, MAX_SEL}, 6},
    {"06-1", "02", {0, 1, 2, 3, 4, 5, MAX_SEL}, 6, {0, 1, 5, MAX_SEL}, 2},
    {"12-1", "02", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, MAX_SEL}, 0, {0, 1, 11, MAX_SEL}, 2},
    {"\0071", "06", {0, MAX_SEL}, 5, {0, 5, MAX_SEL}, 6},
};

class SelectorNumericEditInputTest : public SelectorNumericInputState,
                                     public testing::WithParamInterface<std::tuple<bool, SelectorNumericEdit>>
{
protected:
    void SetUp() override;
};

void SelectorNumericEditInputTest::SetUp()
{
    SelectorInputTest::SetUp();
    prepare_numeric(std::get<0>(GetParam()));
    select_rows({0, MAX_SEL});
    g_sel_page.item_index = 4;
}

TEST_P(SelectorNumericEditInputTest, erasingTheFirstDigitAllowsTheRangeOrGotoDestinationToBeReplaced)
{
    const SelectorNumericEdit &edit = std::get<1>(GetParam());
    const std::string          input = std::string(edit.prefix) + g_erase_char + std::string(edit.replacement);

    ASSERT_EQ('Q', run_input(input));

    expect_numeric(edit.resumed, edit.resumed_cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

TEST_P(SelectorNumericEditInputTest, erasingTheDigitAndThenThePrefixAllowsAFreshSelection)
{
    const SelectorNumericEdit &edit = std::get<1>(GetParam());
    const std::string input = std::string(edit.prefix) + std::string(2, g_erase_char) + std::string(edit.replacement);

    ASSERT_EQ('Q', run_input(input));

    expect_numeric(edit.cancelled, edit.cancelled_cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

TEST_P(SelectorNumericEditInputTest, killingThePartialNumberCancelsItsPrefixAndAllowsAFreshSelection)
{
    const SelectorNumericEdit &edit = std::get<1>(GetParam());
    const std::string          input = std::string(edit.prefix) + g_kill_char + std::string(edit.replacement);

    ASSERT_EQ('Q', run_input(input));

    expect_numeric(edit.cancelled, edit.cancelled_cursor);
    EXPECT_EQ(std::string::npos, m_output.find("No item"));
}

INSTANTIATE_TEST_SUITE_P(ReadStateAndPartialDestination, SelectorNumericEditInputTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(NUMERIC_EDITS)));

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
    int               m_old_subject_count{g_subj.count};
    SelectionMode     m_old_thread_mode{g_sel.thread_mode};
    SelectionSortMode m_old_art_sort{g_sel.art_sort};
    SelectionSortMode m_old_thread_sort{g_sel.thread_sort};
    std::string_view  m_old_mode_string{g_sel.mode_string};
    std::string_view  m_old_sort_string{g_sel.sort_string};
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
    g_subj.count = 3;
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
    g_subj.last = &m_charlie;
    const std::array<Subject *, 4>    subjects{&m_subject, &m_bravo, &m_charlie, &m_subject};
    const std::array<std::time_t, 4>  dates{10, 30, 20, 40};
    const std::array<const char *, 4> authors{"Delta", "Charlie", "Bravo", "Alpha"};
    const std::array<long, 4>         lines{30, 10, 20, 40};
    const std::array<int, 4>          scores{20, 10, 50, 40};
    for (std::size_t i = 0; i < subjects.size(); ++i)
    {
        Article *article = g_cache.ptr(ArticleNum{static_cast<long>(i + 1)});
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
    g_cache.ptr(ArticleNum{1})->m_subj_next = g_cache.ptr(ArticleNum{4});
}

void ArticleSelectorSortTest::TearDown()
{
    drain_input();
    errno = m_old_errno;
    g_subj.count = m_old_subject_count;
    g_sel.thread_mode = m_old_thread_mode;
    g_sel.art_sort = m_old_art_sort;
    g_sel.thread_sort = m_old_thread_sort;
    g_sel.mode_string = m_old_mode_string;
    g_sel.sort_string = m_old_sort_string;
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
    for (auto &[number, article] : g_cache.article_list)
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
    g_sel.set_selector(mode, static_cast<SelectionSortMode>(SS_DATE * direction));
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
    if (g_sel.mode == SM_ARTICLE)
    {
        for (const Article *article : g_art_order.ptr_list)
        {
            result.push_back(article->article_num().value_of());
        }
    }
    else
    {
        for (const Subject *subject = g_subj.first; subject; subject = subject->m_next)
        {
            result.push_back(subject->m_articles->article_num().value_of());
        }
    }
    return result;
}

void ArticleSelectorSortTest::expect_selection_preserved(bool rereading) const
{
    std::vector<long> selected;
    for (const auto &[number, article] : g_cache.article_list)
    {
        if (article.m_flags & g_sel.mask)
        {
            selected.push_back(number.value_of());
        }
    }
    EXPECT_EQ((std::vector<long>{1, 4}), selected);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(rereading ? 0 : 4, m_newsgroup.m_to_read);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
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
    int m_old_item_index{g_sel_page.item_index};
};

void ArticleSelectorExclusiveStateTest::TearDown()
{
    g_sel_page.item_index = m_old_item_index;
    ArticleSelectorSortTest::TearDown();
}

void ArticleSelectorExclusiveStateTest::prepare_exclusive(SelectionMode mode, bool rereading, bool exclusive)
{
    prepare_sort(mode, rereading);
    g_sel.exclusive = exclusive;
    rebuild_page();
    if (mode == SM_ARTICLE)
    {
        g_sel_page.page_art = g_art_order.end() - 1;
    }
    else if (!exclusive)
    {
        g_sel_page.page_subj = &m_charlie;
    }
    g_sel_page.item_index = exclusive && mode != SM_ARTICLE ? 0 : 1;
}

void ArticleSelectorExclusiveStateTest::clear_selections()
{
    for (auto &[number, article] : g_cache.article_list)
    {
        article.m_flags &= ~(AF_SEL | AF_DEL_SEL);
    }
    for (Subject *subject = g_subj.first; subject; subject = subject->m_next)
    {
        subject->m_flags &= ~(SF_SEL | SF_DEL_SEL);
    }
}

std::vector<long> ArticleSelectorExclusiveStateTest::included_items() const
{
    std::vector<long> result;
    if (g_sel.mode == SM_ARTICLE)
    {
        for (const Article *article : g_art_order.ptr_list)
        {
            if (article->m_flags & AF_INCLUDED)
            {
                result.push_back(article->article_num().value_of());
            }
        }
    }
    else
    {
        for (const Subject *subject = g_subj.first; subject; subject = subject->m_next)
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
    EXPECT_EQ(total, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(SS_DATE, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    if (g_sel.mode == SM_ARTICLE)
    {
        EXPECT_EQ(g_art_order.begin(), g_sel_page.page_art);
    }
    else
    {
        EXPECT_EQ(g_subj.first, g_sel_page.page_subj);
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
    g_sel_page.page_art = nullptr;
    g_sel_page.page_subj = nullptr;
    rebuild_page();
    calculate_page();
}

void ArticleSelectorBulkStateTest::calculate_page()
{
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
    g_sel_page.item_index = 0;
}

void ArticleSelectorBulkStateTest::select_all()
{
    for (auto &[number, article] : g_cache.article_list)
    {
        article.m_flags |= static_cast<ArticleFlags>(g_sel.mask);
    }
    for (Subject *subject = g_subj.first; subject; subject = subject->m_next)
    {
        subject->m_flags |= static_cast<SubjectFlags>(g_sel.mask);
    }
    rebuild_page();
    calculate_page();
}

void ArticleSelectorBulkStateTest::join_reply_thread()
{
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_charlie.m_thread_link = &m_subject;
    g_sel.exclusive = true;
    g_sel_page.page_subj = nullptr;
    rebuild_page();
    calculate_page();
}

ArticleNum ArticleSelectorBulkStateTest::page_article() const
{
    return g_sel.mode == SM_ARTICLE ? (*g_sel_page.page_art)->article_num()
                                    : g_sel_page.page_subj->m_articles->article_num();
}

void ArticleSelectorBulkStateTest::expect_articles(std::initializer_list<long> unread,
                                                   std::initializer_list<long> selected) const
{
    std::vector<long> actual_unread;
    std::vector<long> actual_selected;
    for (const auto &[number, article] : g_cache.article_list)
    {
        if (article.m_flags & AF_UNREAD)
        {
            actual_unread.push_back(number.value_of());
        }
        if (article.m_flags & g_sel.mask)
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

class ArticleSelectorInlineStateTest : public ArticleSelectorBulkStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         prepare_inline(SelectionMode mode, bool rereading = false, bool selected = true);
    void         set_current_item();
    void         join_inline_thread();
    DisplayState run_inline(std::string_view input);

private:
    ArticleLocation            m_old_location{g_nav.target};
    SelectionItem              m_old_item{g_sel_page.items[0]};
    int                        m_old_page_line{g_page_line};
    int                        m_old_perform_count{g_status.perform_count};
    bool                       m_old_performed_loop{g_status.performed_article_loop};
    bool                       m_old_error{g_error_occurred};
    bool                       m_old_breadth_first{g_breadth_first};
    char                       m_old_erase{g_erase_char};
    char                       m_old_kill{g_kill_char};
    std::string_view           m_old_standout{g_tc_SO};
    std::string_view           m_old_end_standout{g_tc_SE};
    std::string_view           m_old_backspace{g_tc_BC};
    std::string                m_old_message{g_msg};
    std::optional<std::string> m_old_subject_line{g_subj_line};
};

void ArticleSelectorInlineStateTest::SetUp()
{
    ArticleSelectorBulkStateTest::SetUp();
    g_nav.target.art = nullptr;
    g_nav.target.num = ArticleNum{};
    g_page_line = 1;
    g_status.perform_count = 0;
    g_error_occurred = false;
    g_breadth_first = false;
    g_erase_char = '\b';
    g_kill_char = Ctl('u');
    g_tc_SO = "";
    g_tc_SE = "";
    g_tc_BC = "\b";
    g_cache.ptr(ArticleNum{1})->m_child1 = g_cache.ptr(ArticleNum{4});
    g_cache.ptr(ArticleNum{4})->m_parent = g_cache.ptr(ArticleNum{1});
}

void ArticleSelectorInlineStateTest::TearDown()
{
    g_nav.target = m_old_location;
    g_sel_page.items[0] = m_old_item;
    g_page_line = m_old_page_line;
    g_status.perform_count = m_old_perform_count;
    g_status.performed_article_loop = m_old_performed_loop;
    g_error_occurred = m_old_error;
    g_breadth_first = m_old_breadth_first;
    g_erase_char = m_old_erase;
    g_kill_char = m_old_kill;
    g_tc_SO = m_old_standout;
    g_tc_SE = m_old_end_standout;
    g_tc_BC = m_old_backspace;
    g_msg = m_old_message;
    g_subj_line = m_old_subject_line;
    ArticleSelectorBulkStateTest::TearDown();
}

void ArticleSelectorInlineStateTest::prepare_inline(SelectionMode mode, bool rereading, bool selected)
{
    prepare_bulk(mode, rereading, selected);
    set_current_item();
}

void ArticleSelectorInlineStateTest::set_current_item()
{
    if (g_sel.mode == SM_ARTICLE)
    {
        g_sel_page.items[0].u.ap = *g_sel_page.page_art;
    }
    else
    {
        g_sel_page.items[0].u.sp = g_sel_page.page_subj;
    }
}

void ArticleSelectorInlineStateTest::join_inline_thread()
{
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_charlie.m_thread_link = &m_subject;
    Article *reply = g_cache.ptr(ArticleNum{3});
    reply->m_parent = m_subject.m_articles;
    g_cache.ptr(ArticleNum{4})->m_sibling = reply;
    g_sel_page.page_subj = nullptr;
    rebuild_page();
    calculate_page();
    set_current_item();
}

DisplayState ArticleSelectorInlineStateTest::run_inline(std::string_view input)
{
    return run_command(':', std::string(input) + "\n\n");
}

class ArticleSelectorInlineModeTest : public ArticleSelectorInlineStateTest,
                                      public testing::WithParamInterface<SelectionMode>
{
};

class ArticleSelectorInlineCancelTest : public ArticleSelectorInlineStateTest,
                                        public testing::WithParamInterface<std::tuple<SelectionMode, char>>
{
};

class ArticleSelectorInlineScopeTest : public ArticleSelectorInlineStateTest,
                                       public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class ArticleSelectorInlinePendingMarkTest : public ArticleSelectorInlineStateTest,
                                             public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class ArticleSelectorSearchStateTest : public ArticleSelectorInlineStateTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         remember_search(std::string_view command);
    DisplayState run_search(std::string_view input, std::string_view followup = "\n");
    void         expect_search_selection(std::initializer_list<long> selected, bool rereading = false) const;

private:
    HeaderType     m_old_subject_header{g_header_rules.type[SUBJ_LINE]};
    HeaderPosition m_old_subject_header_pos{g_parsed_header.positions[SUBJ_LINE]};
    HeaderType     m_old_from_header{g_header_rules.type[FROM_LINE]};
    HeaderPosition m_old_from_header_pos{g_parsed_header.positions[FROM_LINE]};
    std::string    m_old_pattern{g_art_search.last_pat};
    CompiledRegex  m_old_regex{*g_art_search.bra_compex};
    ArtScope       m_old_scope{g_art_search.how_much};
    HeaderLineType m_old_header{g_art_search.srch_hdr};
    bool           m_old_do_read{g_art_search.do_read};
    bool           m_old_novice_delays{g_novice_delays};
    bool           m_old_mouse_down{g_mouse_is_down};
    ArticleNum     m_old_search_ahead{g_search_ahead};
};

void ArticleSelectorSearchStateTest::SetUp()
{
    ArticleSelectorInlineStateTest::SetUp();
    g_header_rules.type[SUBJ_LINE].flags |= HT_CACHED;
    g_header_rules.type[FROM_LINE].flags |= HT_CACHED;
    g_parsed_header.parsed_art = ArticleNum{};
    g_art_search.last_pat.clear();
    g_art_search.how_much = ARTSCOPE_SUBJECT;
    g_art_search.srch_hdr = SOME_LINE;
    g_art_search.do_read = false;
    g_novice_delays = false;
    g_search_ahead = ArticleNum{};
    search_init();
    g_art_search.init();
}

void ArticleSelectorSearchStateTest::TearDown()
{
    g_header_rules.type[SUBJ_LINE] = m_old_subject_header;
    g_parsed_header.positions[SUBJ_LINE] = m_old_subject_header_pos;
    g_header_rules.type[FROM_LINE] = m_old_from_header;
    g_parsed_header.positions[FROM_LINE] = m_old_from_header_pos;
    g_art_search.last_pat = m_old_pattern;
    *g_art_search.bra_compex = m_old_regex;
    g_art_search.how_much = m_old_scope;
    g_art_search.srch_hdr = m_old_header;
    g_art_search.do_read = m_old_do_read;
    g_novice_delays = m_old_novice_delays;
    g_mouse_is_down = m_old_mouse_down;
    g_search_ahead = m_old_search_ahead;
    ArticleSelectorInlineStateTest::TearDown();
}

void ArticleSelectorSearchStateTest::remember_search(std::string_view command)
{
    g_general_mode = GM_READ;
    g_nav.target.num = article_after(g_last_art);
    EXPECT_EQ(SRCH_FOUND, art_search(command, false));
    g_general_mode = GM_SELECTOR;
}

DisplayState ArticleSelectorSearchStateTest::run_search(std::string_view input, std::string_view followup)
{
    return run_command('/', std::string(input) + "\n" + std::string(followup));
}

void ArticleSelectorSearchStateTest::expect_search_selection(std::initializer_list<long> selected, bool rereading) const
{
    expect_articles(rereading ? std::initializer_list<long>{} : std::initializer_list<long>{1, 2, 3, 4}, selected);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_THREAD_SELECTOR, g_mode);
}

class ArticleSelectorSearchModeTest : public ArticleSelectorSearchStateTest,
                                      public testing::WithParamInterface<SelectionMode>
{
};

class ArticleSelectorSearchReadTest : public ArticleSelectorSearchStateTest,
                                      public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
};

class ArticleSelectorSearchCancelTest : public ArticleSelectorSearchStateTest,
                                        public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
};

} // namespace

TEST_F(SelectorCommandInputTest, readsCommandText)
{
    g_pushback.push('x');

    const std::string command = read_selector_command_for_test('>', 'Z', false);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('x', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_FALSE(g_s_default_cmd);
    EXPECT_FALSE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsPageDefaultCommand)
{
    g_pushback.push(' ');

    const std::string command = read_selector_command_for_test('>', 'Z', false);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('>', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsEndDefaultCommand)
{
    g_pushback.push(' ');

    const std::string command = read_selector_command_for_test('>', 'Z', true);

    ASSERT_EQ(2, command.size());
    EXPECT_EQ('Z', command[0]);
    EXPECT_EQ(FINISH_CMD, command[1]);
    EXPECT_TRUE(g_s_default_cmd);
    EXPECT_TRUE(g_univ_default_cmd);
}

TEST_F(SelectorCommandInputTest, readsEscapedCommandCharacter)
{
    g_pushback.push('x');

    EXPECT_EQ('x', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsEscapedDefaultCommandCharacter)
{
    g_pushback.push('\\');

    EXPECT_EQ('\\', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsEscapedBlankCommandCharacter)
{
    g_pushback.push(' ');

    EXPECT_EQ(' ', read_selector_escaped_command_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationDigit)
{
    g_pushback.push('7');

    EXPECT_EQ('7', read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationEraseCharacter)
{
    g_pushback.push(g_erase_char);

    EXPECT_EQ(g_erase_char, read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsNumericContinuationKillCharacter)
{
    g_pushback.push(g_kill_char);

    EXPECT_EQ(g_kill_char, read_selector_numeric_continuation_for_test());
}

TEST_F(SelectorCommandInputTest, readsPromptCommandText)
{
    g_pushback.push('s');

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
    g_pushback.push(' ');

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
    g_pushback.push('x');

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
    EXPECT_EQ(!rereading, g_sel.rereading);
    EXPECT_EQ(nullptr, g_sel_page.page_art);
    EXPECT_EQ(nullptr, g_sel_page.page_subj);
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(2, m_newsgroup.m_to_read);
}

TEST_P(ArticleSelectorToggleTest, rebuildsThePageWithTheOtherPopulation)
{
    const auto &[mode, rereading] = GetParam();
    prepare_page(mode, rereading);

    ASSERT_EQ(DS_RESTART, article_commands('U'));
    rebuild_page();

    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, m_subject.m_misc);
    EXPECT_EQ(!rereading, g_sel.rereading);
    EXPECT_EQ(ArticleNum{rereading ? 3 : 1}, g_first_art);
    if (mode == SM_ARTICLE)
    {
        ASSERT_EQ(2U, g_art_order.ptr_list.size());
        EXPECT_EQ(ArticleNum{rereading ? 3 : 1}, g_art_order.ptr_list[0]->article_num());
        EXPECT_EQ(ArticleNum{rereading ? 4 : 2}, g_art_order.ptr_list[1]->article_num());
        EXPECT_EQ(g_art_order.begin(), g_sel_page.page_art);
    }
    else
    {
        EXPECT_EQ(&m_subject, g_sel_page.page_subj);
        EXPECT_TRUE(m_subject.m_flags & SF_INCLUDED);
    }
}

TEST_P(ArticleSelectorModeTest, commitsPendingReadMarksBeforeShowingReadArticles)
{
    prepare_page(GetParam(), false);
    if (GetParam() == SM_ARTICLE)
    {
        g_cache.ptr(ArticleNum{3})->m_flags |= AF_DEL;
    }
    else
    {
        m_subject.m_flags |= SF_DEL;
    }
    g_cache.ptr(ArticleNum{4})->m_flags |= AF_SEL;
    m_subject.m_flags |= SF_SEL;
    g_selected_count = 1;
    g_selected_subj_cnt = 1;

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{3})->m_flags & (AF_UNREAD | AF_DEL));
    EXPECT_TRUE(g_cache.ptr(ArticleNum{4})->m_flags & AF_UNREAD);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{4})->m_flags & AF_SEL);
    EXPECT_EQ(1, m_newsgroup.m_to_read);
}

TEST_P(ArticleSelectorModeTest, restoresMarkedReadArticlesBeforeShowingUnreadArticles)
{
    prepare_page(GetParam(), true);
    g_cache.ptr(ArticleNum{1})->m_flags |= AF_DEL_SEL;
    m_subject.m_flags |= SF_DEL_SEL;
    g_sel_page.last_art = g_cache.ptr(ArticleNum{1});
    g_sel_page.last_subj = &m_subject;

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{1})->m_flags & AF_UNREAD);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{1})->m_flags & AF_SEL);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{1})->m_flags & AF_DEL_SEL);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{2})->m_flags & (AF_UNREAD | AF_DEL | AF_DEL_SEL));
    EXPECT_EQ(3, m_newsgroup.m_to_read);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(nullptr, g_sel_page.last_art);
    EXPECT_EQ(nullptr, g_sel_page.last_subj);
}

TEST_P(ArticleSelectorModeTest, retainsUnreadModeWhenReadArticleCachingIsInterrupted)
{
    prepare_page(GetParam(), false);
    g_cache.cached_all_in_range = false;
    // The current header is already parsed; interrupt the remaining cache work.
    g_parsed_header.parsed_art = ArticleNum{1};
    g_cache.ptr(ArticleNum{1})->m_flags &= ~(AF_CACHED | AF_THREADED);
    g_int_count = 1;

    const DisplayState result = article_commands('U');

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_EQ(nullptr, g_sel_page.page_art);
    EXPECT_EQ(nullptr, g_sel_page.page_subj);
    EXPECT_FALSE(g_cache.cached_all_in_range);
    EXPECT_LT(g_cache.last_cached, g_last_art);
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
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(sort, g_sel.sort);
    EXPECT_EQ(reverse ? -1 : 1, g_sel.direction);
    EXPECT_EQ(expected, displayed_order());
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    if (mode == SM_ARTICLE)
    {
        EXPECT_EQ(g_art_order.begin(), g_sel_page.page_art);
    }
    else
    {
        EXPECT_EQ(g_subj.first, g_sel_page.page_subj);
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
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(SS_DATE, g_sel.sort);
    EXPECT_EQ(-direction, g_sel.direction);
    EXPECT_EQ(expected, displayed_order());
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    expect_selection_preserved(rereading);
}

TEST_P(ArticleSelectorReadSortTest, reordersReadArticlesWithoutRestoringPendingSelections)
{
    prepare_sort(GetParam(), true);

    const DisplayState result = run_command('O', "S");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(GetParam(), g_sel.mode);
    EXPECT_EQ(SS_STRING, g_sel.sort);
    EXPECT_EQ(-1, g_sel.direction);
    EXPECT_EQ(GetParam() == SM_ARTICLE ? (std::vector<long>{3, 2, 4, 1}) : (std::vector<long>{3, 2, 1}),
              displayed_order());
    expect_selection_preserved(true);
}

TEST_F(ArticleSelectorSortTest, blankSortChoiceUsesForwardDateOrder)
{
    prepare_sort(SM_ARTICLE);
    g_sel.set_selector(SM_ARTICLE, static_cast<SelectionSortMode>(-SS_STRING));
    rebuild_page();

    const DisplayState result = run_command('O', " ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(SS_DATE, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
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
    Article **const         page_app = g_sel_page.page_art;
    Subject *const          page_sp = g_sel_page.page_subj;

    const DisplayState result = run_command('O', "q");

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(SS_DATE, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    EXPECT_EQ(before, displayed_order());
    EXPECT_EQ(page_app, g_sel_page.page_art);
    EXPECT_EQ(page_sp, g_sel_page.page_subj);
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
    EXPECT_EQ(SS_DATE, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
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
    EXPECT_EQ(4, g_sel_page.total_obj_cnt);
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
    EXPECT_EQ(!exclusive, g_sel.exclusive);
    EXPECT_EQ(mode, g_sel.mode);
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
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(mode, g_sel.mode);
    EXPECT_EQ(all_items, included_items());
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? 0 : 4, m_newsgroup.m_to_read);
    expect_first_page(4);
}

TEST_P(ArticleSelectorExclusiveCleanupTest, commitsPendingReadMarksBeforeFiltering)
{
    prepare_exclusive(GetParam(), false, false);
    if (GetParam() == SM_ARTICLE)
    {
        g_cache.ptr(ArticleNum{2})->m_flags |= AF_DEL;
    }
    else
    {
        m_bravo.m_flags |= SF_DEL;
    }

    const DisplayState result = run_command('E');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{2})->m_flags & (AF_UNREAD | AF_DEL));
    EXPECT_TRUE(g_cache.ptr(ArticleNum{3})->m_flags & AF_UNREAD);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{1})->m_flags & AF_SEL);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{4})->m_flags & AF_SEL);
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
    g_cache.ptr(ArticleNum{3})->m_flags |= rereading ? AF_DEL_SEL : AF_SEL;
    m_charlie.m_flags |= rereading ? SF_DEL_SEL : SF_SEL;
    m_charlie.m_thread = m_subject.m_thread;
    m_subject.m_thread_link = &m_charlie;
    m_charlie.m_thread_link = &m_subject;
    rebuild_page();

    const DisplayState result = run_command('E');

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ((std::vector<long>{1}), included_items());
    EXPECT_TRUE(m_subject.m_flags & SF_INCLUDED);
    EXPECT_FALSE(m_charlie.m_flags & SF_INCLUDED);
    EXPECT_FALSE(m_bravo.m_flags & SF_INCLUDED);
    EXPECT_TRUE(g_cache.ptr(ArticleNum{3})->m_flags & g_sel.mask);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{1})->m_flags & g_sel.mask);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{4})->m_flags & g_sel.mask);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(rereading, g_sel.rereading);
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
    ASSERT_TRUE(g_sel_page.next_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{3}, page_article());

    const DisplayState result = run_command('X');

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(GetParam(), g_sel.mode);
}

TEST_P(ArticleSelectorBulkModeTest, junkCommandMarksSelectedItemsReadAndRedisplaysSurvivors)
{
    prepare_bulk(GetParam());

    const DisplayState result = run_command('J');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2, 3}, {});
    EXPECT_EQ(ArticleNum{2}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(ArticleNum{3}, page_article());
}

TEST_P(ArticleSelectorBulkModeTest, pageCommandPreservesUnselectedItemsOnOtherPages)
{
    prepare_bulk(GetParam());
    ASSERT_TRUE(g_sel_page.next_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{3}, page_article());
    ASSERT_EQ(1, g_sel_page.page_item_cnt);

    const DisplayState result = run_command('D');

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 2, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorBulkModeTest, pageCommandWithoutSelectionsContinuesAtTheNextPage)
{
    prepare_bulk(GetParam(), false, false);
    ASSERT_TRUE(g_sel_page.next_page());
    calculate_page();
    ASSERT_EQ(ArticleNum{3}, page_article());

    const DisplayState result = run_command('D');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 4}, {});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(ArticleNum{2}, page_article());
    EXPECT_EQ(0, g_sel_page.item_index);
}

TEST_P(ArticleSelectorBulkModeTest, deletingTheLastPageRepairsThePageAnchor)
{
    prepare_bulk(GetParam(), false, false);
    ASSERT_TRUE(g_sel_page.last_page());
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
    EXPECT_EQ(0, g_sel_page.item_index);
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
        g_cache.ptr(ArticleNum{3})->m_flags |= AF_DEL;
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
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_EQ(AGF_DEL_SEL, g_sel.mask);
    EXPECT_EQ(mode, g_sel.mode);
}

TEST_P(ArticleSelectorBulkExclusiveTest, exclusiveCommandsPreserveHiddenUnselectedItems)
{
    const auto &[mode, command] = GetParam();
    prepare_bulk(mode);
    g_sel.exclusive = true;
    rebuild_page();
    calculate_page();

    const DisplayState result = run_command(command);

    EXPECT_EQ(DS_QUIT, result);
    expect_articles({1, 2, 3, 4}, {1, 4});
    EXPECT_EQ(ArticleNum{4}, g_obj_count);
    EXPECT_TRUE(g_sel.exclusive);
}

TEST_P(ArticleSelectorBulkThreadTest, exclusiveCommandsIncludeUnselectedRepliesInTheDisplayedThread)
{
    prepare_bulk(SM_THREAD);
    join_reply_thread();
    ASSERT_EQ(3, g_sel_page.page_obj_cnt);
    ASSERT_EQ(&m_bravo, g_sel_page.next_subj);

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
    g_cache.ptr(ArticleNum{3})->m_flags |= AF_SEL;
    m_charlie.m_flags |= SF_SEL;
    join_reply_thread();

    const DisplayState result = run_command('J');

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 4}, {});
    EXPECT_EQ(ArticleNum{3}, g_obj_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_FALSE(g_sel.exclusive);
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

TEST_P(ArticleSelectorInlineModeTest, junkCommandAppliesToSelectedArticlesAcrossPages)
{
    prepare_inline(GetParam());
    ASSERT_TRUE(g_sel_page.next_page());
    calculate_page();
    set_current_item();

    const DisplayState result = run_inline("j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2, 3}, {});
    EXPECT_EQ(2, g_status.perform_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(GetParam(), g_sel.mode);
}

TEST_P(ArticleSelectorInlineModeTest, doubledColonJunksUnselectedArticlesAndRetainsSelection)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline(":j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 4}, {1, 4});
    EXPECT_EQ(2, g_status.perform_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorInlineModeTest, deselectCommandKeepsArticlesUnread)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline("-");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 3, 4}, {});
    EXPECT_EQ(2, g_status.perform_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorInlineModeTest, doubledColonSelectsPreviouslyUnselectedArticles)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline(":+");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 3, 4}, {1, 2, 3, 4});
    EXPECT_EQ(2, g_status.perform_count);
    EXPECT_EQ(3, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorInlineModeTest, selectedCommandWithNoSelectionLeavesArticlesAlone)
{
    prepare_inline(GetParam(), false, false);

    const DisplayState result = run_inline("j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 3, 4}, {});
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_EQ("No articles affected.", g_msg);
}

TEST_P(ArticleSelectorInlineModeTest, readModeJunkCommandClearsSelectedReadArticles)
{
    prepare_inline(GetParam(), true);

    const DisplayState result = run_inline("j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({}, {});
    EXPECT_EQ(2, g_status.perform_count);
    EXPECT_TRUE(g_sel.rereading);
    EXPECT_EQ(AGF_DEL_SEL, g_sel.mask);
}

TEST_P(ArticleSelectorInlineModeTest, emptyCommandRetainsSelection)
{
    prepare_inline(GetParam());

    const DisplayState result = run_inline("");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_selection_preserved(false);
    EXPECT_EQ(0, g_status.perform_count);
}

TEST_P(ArticleSelectorInlineModeTest, selectedUnreadCommandSkipsAlreadyReadArticles)
{
    prepare_inline(GetParam());
    g_cache.ptr(ArticleNum{4})->one_less();
    rebuild_page();
    calculate_page();
    set_current_item();

    const DisplayState result = run_inline("j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2, 3}, {});
    EXPECT_EQ(1, g_status.perform_count);
}

TEST_P(ArticleSelectorInlineCancelTest, erasedCommandRetainsSelection)
{
    const auto &[mode, cancel] = GetParam();
    prepare_inline(mode);

    const DisplayState result = run_inline(std::string(1, cancel));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_selection_preserved(false);
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_EQ(g_cache.ptr(ArticleNum{1}), g_nav.target.art);
    EXPECT_EQ(ArticleNum{1}, g_nav.target.num);
}

TEST_P(ArticleSelectorInlineScopeTest, dotCommandLimitsJunkingToCurrentSubjectOrThread)
{
    const auto &[mode, unselected] = GetParam();
    prepare_inline(mode);
    g_cache.ptr(ArticleNum{2})->m_flags |= AF_SEL;
    if (!unselected)
    {
        g_cache.ptr(ArticleNum{3})->m_flags |= AF_SEL;
    }
    else
    {
        g_cache.ptr(ArticleNum{4})->m_flags &= ~AF_SEL;
    }
    join_inline_thread();

    const DisplayState result = run_inline(unselected ? ":.j" : ".j");

    EXPECT_EQ(DS_DISPLAY, result);
    if (mode == SM_SUBJECT)
    {
        if (unselected)
        {
            expect_articles({1, 2, 3}, {1, 2});
        }
        else
        {
            expect_articles({2, 3}, {2, 3});
        }
    }
    else if (unselected)
    {
        expect_articles({1, 2}, {1, 2});
    }
    else
    {
        expect_articles({2}, {2});
    }
}

TEST_F(ArticleSelectorInlineStateTest, threadCommandFindsTheFirstSubjectWithUnreadArticles)
{
    prepare_inline(SM_THREAD);
    g_cache.ptr(ArticleNum{1})->m_flags &= ~(AF_UNREAD | AF_SEL);
    g_cache.ptr(ArticleNum{4})->m_flags &= ~(AF_UNREAD | AF_SEL);
    g_cache.ptr(ArticleNum{3})->m_flags |= AF_SEL;
    m_charlie.m_flags |= SF_SEL;
    m_newsgroup.m_to_read = 2;
    join_inline_thread();
    ASSERT_EQ(0, m_subject.m_misc);
    ASSERT_EQ(&m_subject, g_sel_page.items[0].u.sp);

    const DisplayState result = run_inline(".j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2}, {});
    EXPECT_EQ(1, g_status.perform_count);
}

TEST_F(ArticleSelectorInlineStateTest, currentSubjectSelectionDoesNotSelectItsThreadRoot)
{
    prepare_inline(SM_SUBJECT);
    join_inline_thread();
    ASSERT_TRUE(g_sel_page.next_page());
    calculate_page();
    set_current_item();
    ASSERT_EQ(&m_charlie, g_sel_page.items[0].u.sp);

    const DisplayState result = run_inline(":.++");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 2, 3, 4}, {1, 3, 4});
    EXPECT_EQ(1, g_status.perform_count);
    EXPECT_EQ(2, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorInlinePendingMarkTest, completedCommandCommitsPendingReadMarksOnlyInUnreadMode)
{
    const auto &[mode, rereading] = GetParam();
    prepare_inline(mode, rereading);
    join_inline_thread();
    m_charlie.m_flags |= SF_DEL;

    const DisplayState result = run_inline("+");

    EXPECT_EQ(DS_DISPLAY, result);
    if (rereading)
    {
        expect_articles({}, {1, 4});
        EXPECT_TRUE(m_charlie.m_flags & SF_DEL);
    }
    else
    {
        expect_articles({1, 2, 4}, {1, 4});
        EXPECT_FALSE(m_charlie.m_flags & SF_DEL);
    }
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(rereading, g_sel.rereading);
}

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, ArticleSelectorInlineModeTest,
                         testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD));

INSTANTIATE_TEST_SUITE_P(ModeAndEraseKey, ArticleSelectorInlineCancelTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD),
                                          testing::Values('\b', Ctl('u'))));

INSTANTIATE_TEST_SUITE_P(ModeAndSelection, ArticleSelectorInlineScopeTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeAndReadState, ArticleSelectorInlinePendingMarkTest,
                         testing::Combine(testing::Values(SM_SUBJECT, SM_THREAD), testing::Bool()));

TEST_P(ArticleSelectorSearchReadTest, matchingSearchAddsSelectionsStartingAtTheBeginningOfTheGroup)
{
    const auto &[mode, rereading] = GetParam();
    prepare_inline(mode, rereading);
    ASSERT_TRUE(g_sel_page.last_page());
    calculate_page();
    g_nav.target.num = ArticleNum{4};
    g_nav.target.art = g_cache.ptr(g_nav.target.num);

    const DisplayState result = run_search("Bravo/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({1, 2, 4}, rereading);
    EXPECT_EQ(2, g_selected_subj_cnt);
    EXPECT_EQ(1, g_status.perform_count);
    EXPECT_EQ("Bravo", g_art_search.last_pat);
    EXPECT_EQ(0, g_sel_page.item_index);
    EXPECT_EQ(mode, g_sel.mode);
}

TEST_P(ArticleSelectorSearchReadTest, noMatchesRetainSelectionAndReportNoArticlesAffected)
{
    const auto &[mode, rereading] = GetParam();
    prepare_inline(mode, rereading);

    const DisplayState result = run_search("Absent/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_selection_preserved(rereading);
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_EQ("No articles affected.", g_msg);
    EXPECT_NE(std::string::npos, m_output.find("No articles affected."));
}

TEST_P(ArticleSelectorSearchReadTest, malformedPatternReportsAnErrorAndRetainsSelection)
{
    const auto &[mode, rereading] = GetParam();
    prepare_inline(mode, rereading);

    const DisplayState result = run_search("[/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_selection_preserved(rereading);
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_NE(std::string::npos, m_output.find("Missing ]"));
    EXPECT_FALSE(g_error_occurred);
}

TEST_P(ArticleSelectorSearchReadTest, blankSearchReusesThePreviousPattern)
{
    const auto &[mode, rereading] = GetParam();
    prepare_inline(mode, rereading);
    remember_search(rereading ? "/Bravo/r" : "/Bravo/");

    const DisplayState result = run_search("");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({1, 2, 4}, rereading);
    EXPECT_EQ(1, g_status.perform_count);
    EXPECT_EQ("Bravo", g_art_search.last_pat);
}

TEST_P(ArticleSelectorSearchModeTest, authorMatchesSelectTheScopeChosenByTheSelectorMode)
{
    const SelectionMode mode = GetParam();
    prepare_inline(mode);
    join_inline_thread();
    clear_selections();
    g_cache.ptr(ArticleNum{2})->select_article(AUTO_KILL_NONE);
    rebuild_page();

    const DisplayState result = run_search("Delta/f");

    EXPECT_EQ(DS_DISPLAY, result);
    if (mode == SM_ARTICLE)
    {
        expect_search_selection({1, 2});
    }
    else if (mode == SM_SUBJECT)
    {
        expect_search_selection({1, 2, 4});
    }
    else
    {
        expect_search_selection({1, 2, 3, 4});
    }
    EXPECT_EQ(1, g_status.perform_count);
    EXPECT_EQ(ARTSCOPE_FROM, g_art_search.how_much);
}

TEST_P(ArticleSelectorSearchModeTest, explicitJunkCommandVisitsEveryMatchAcrossTheGroup)
{
    prepare_inline(GetParam());

    const DisplayState result = run_search("Alpha/j");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({2, 3}, {});
    EXPECT_EQ(2, g_status.perform_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
}

TEST_P(ArticleSelectorSearchModeTest, unreadSearchSkipsMatchingReadArticles)
{
    prepare_inline(GetParam());
    g_cache.ptr(ArticleNum{2})->one_less();
    rebuild_page();

    const DisplayState result = run_search("Bravo/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_articles({1, 3, 4}, {1, 4});
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_EQ("No articles affected.", g_msg);
}

TEST_P(ArticleSelectorSearchModeTest, matchingSearchIncludesNewSelectionsInTheExclusiveDisplay)
{
    const SelectionMode mode = GetParam();
    prepare_inline(mode);
    g_sel.exclusive = true;
    rebuild_page();

    const DisplayState result = run_search("Bravo/");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_search_selection({1, 2, 4});
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ((mode == SM_ARTICLE ? std::vector<long>{1, 2, 4} : std::vector<long>{1, 2}), included_items());
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
}

TEST_P(ArticleSelectorSearchModeTest, followupCommandReturnsToSelectorCommandDispatch)
{
    prepare_inline(GetParam());

    const DisplayState result = run_search("Bravo/", "q");

    EXPECT_EQ(DS_DO_COMMAND, result);
    expect_search_selection({1, 2, 4});
    EXPECT_FALSE(macro_pending());
}

TEST_P(ArticleSelectorSearchModeTest, blankSearchWithoutAPreviousPatternReportsAnError)
{
    prepare_inline(GetParam());

    const DisplayState result = run_search("");

    EXPECT_EQ(DS_DISPLAY, result);
    expect_selection_preserved(false);
    EXPECT_EQ(0, g_status.perform_count);
    EXPECT_NE(std::string::npos, m_output.find("Null search string"));
}

TEST_P(ArticleSelectorSearchCancelTest, cancellingSearchRetainsThePreviousPatternSelectionAndPosition)
{
    const auto &[mode, rereading, cancel] = GetParam();
    prepare_inline(mode, rereading);
    remember_search(rereading ? "/Bravo/r" : "/Bravo/");
    g_nav.target.num = ArticleNum{4};
    g_nav.target.art = g_cache.ptr(g_nav.target.num);

    const DisplayState result = run_search(std::string(1, cancel));

    EXPECT_EQ(DS_DISPLAY, result);
    expect_selection_preserved(rereading);
    EXPECT_EQ("Bravo", g_art_search.last_pat);
    EXPECT_EQ(ArticleNum{4}, g_nav.target.num);
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_nav.target.art);
    EXPECT_EQ(0, g_status.perform_count);
}

INSTANTIATE_TEST_SUITE_P(ArticleSubjectAndThread, ArticleSelectorSearchModeTest,
                         testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD));

INSTANTIATE_TEST_SUITE_P(ModeAndReadState, ArticleSelectorSearchReadTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeReadStateAndEraseKey, ArticleSelectorSearchCancelTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('\b', Ctl('u'))));
