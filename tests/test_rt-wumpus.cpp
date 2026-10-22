// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-wumpus.h>

#include <trn/Article.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/charsubst.h>
#include <trn/final.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/utf.h>
#include <util/env.h>

#include <test_config.h>

#include "support/UnderlineFixture.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

class TreeRenderingTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_tmp_dir = g_tmp_dir;
        m_old_curr_artp = g_curr_artp;
        m_old_recent_artp = g_recent_artp;
        m_old_selected_only = g_selected_only;
        m_old_do_hiding = g_do_hiding;
        m_old_char_subst = current_char_subst_mode();
        m_old_max_tree_lines = g_max_tree_lines;
        m_old_threaded_group = g_threaded_group;
        m_old_tc_cols = g_tc_COLS;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_us = g_tc_US;
        m_old_tc_ue = g_tc_UE;
        m_old_tc_uc = g_tc_UC;
        m_old_tc_bc = g_tc_BC;
        m_old_fire_is_out = g_fire_is_out;
        m_old_erase_screen = g_erase_screen;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = std::filesystem::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        std::filesystem::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        g_tmp_dir = m_output_dir.generic_string();
        back_page_init();

        m_subject.m_thread = &m_current;
        m_subject.m_thread_link = &m_subject;
        m_current.m_subj = &m_subject;
        m_current.m_child1 = &m_recent;
        m_current.m_flags = AF_EXISTS | AF_CACHED | AF_UNREAD;
        m_recent.m_subj = &m_subject;
        m_recent.m_parent = &m_current;
        m_recent.m_flags = AF_EXISTS | AF_CACHED | AF_UNREAD;

        g_curr_artp = &m_current;
        g_recent_artp = &m_recent;
        g_selected_only = false;
        g_do_hiding = false;
        set_char_subst_mode('\0');
        g_max_tree_lines = 6;
        g_tc_COLS = 40;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
        g_tc_US = m_empty_capability;
        g_tc_UE = m_empty_capability;
        g_tc_UC = m_empty_capability;
        g_tc_BC = "\b";
        g_fire_is_out = 0;
        g_erase_screen = false;
    }

    void TearDown() override
    {
        back_page_final();
        g_tmp_dir = m_old_tmp_dir;
        std::error_code error;
        std::filesystem::remove_all(m_output_dir, error);
        EXPECT_FALSE(error) << error.message();

        g_curr_artp = nullptr;
        init_tree();

        g_curr_artp = m_old_curr_artp;
        g_recent_artp = m_old_recent_artp;
        g_selected_only = m_old_selected_only;
        g_do_hiding = m_old_do_hiding;
        set_char_subst_mode(m_old_char_subst);
        g_max_tree_lines = m_old_max_tree_lines;
        g_threaded_group = m_old_threaded_group;
        g_tc_COLS = m_old_tc_cols;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_US = m_old_tc_us;
        g_tc_UE = m_old_tc_ue;
        g_tc_UC = m_old_tc_uc;
        g_tc_BC = m_old_tc_bc;
        g_fire_is_out = m_old_fire_is_out;
        g_erase_screen = m_old_erase_screen;
    }

    char    m_empty_capability[1]{};
    char    m_standout_start[5]{"<so>"};
    char    m_standout_end[5]{"<se>"};
    Article m_current{};
    Article m_recent{};
    Subject m_subject{};

    std::filesystem::path m_output_dir;
    std::string           m_old_tmp_dir;

    Article    *m_old_curr_artp{};
    Article    *m_old_recent_artp{};
    bool        m_old_selected_only{};
    bool        m_old_do_hiding{};
    char        m_old_char_subst{};
    int         m_old_max_tree_lines{};
    bool        m_old_threaded_group{};
    int         m_old_tc_cols{};
    std::string_view m_old_tc_so;
    std::string_view m_old_tc_se;
    std::string_view m_old_tc_us;
    std::string_view m_old_tc_ue;
    std::string_view m_old_tc_uc;
    std::string_view m_old_tc_bc;
    int         m_old_fire_is_out{};
    bool        m_old_erase_screen{};
};

class ThreadTreeArticleTest : public TreeRenderingTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string display_tree(std::string_view input = {});

    Article       m_branch{};
    Subject       m_branch_subject{};
    NewsgroupData m_group{};

private:
    ArticleNum        m_old_art{g_art};
    Article          *m_old_artp{g_artp};
    ArticleNum        m_old_curr_art{g_curr_art};
    ArticleNum        m_old_recent_art{g_recent_art};
    ArticleNum        m_old_last_art{g_last_art};
    NewsgroupData    *m_old_newsgroup_ptr{g_newsgroup_ptr};
    DoNewsgroupResult m_old_exit_code{g_newsgroup_exit_code};
    ArticleUnread     m_old_selected_count{g_selected_count};
    int               m_old_selected_subj_cnt{g_selected_subj_cnt};
    bool              m_old_reread{g_reread};
    bool              m_old_force_last{g_force_last};
    bool              m_old_do_fseek{g_do_fseek};
    bool              m_old_rotate{g_rotate};
    ArticlePosition   m_old_art_pos{g_art_pos};
    ArticleLine       m_old_art_line_num{g_art_line_num};
    ArticleLine       m_old_top_line{g_top_line};
    GeneralMode       m_old_general_mode{g_general_mode};
    MinorMode         m_old_mode{g_mode};
    int               m_old_tc_lines{g_tc_LINES};
    int               m_old_page_line{g_page_line};
    int               m_old_term_line{g_term_line};
    int               m_old_term_col{g_term_col};
    char              m_old_int_count{g_int_count};
    bool              m_old_verbose{g_verbose};
    bool              m_old_in_ng{g_in_ng};
    bool              m_old_erase_each_line{g_erase_each_line};
    bool              m_old_allow_typeahead{g_allow_typeahead};
    bool              m_old_verify{g_verify};
    bool              m_old_s_default_cmd{g_s_default_cmd};
    bool              m_old_univ_default_cmd{g_univ_default_cmd};
    std::string_view  m_old_tc_cr{g_tc_CR};
    std::string_view  m_old_tc_ce{g_tc_CE};
    std::string_view  m_old_tc_cd{g_tc_CD};
    std::string       m_old_typeahead;
};

void ThreadTreeArticleTest::SetUp()
{
    TreeRenderingTest::SetUp();
    m_subject.m_str = "Re: Topic";
    m_subject.m_articles = &m_current;
    m_subject.m_thread_link = &m_branch_subject;
    m_subject.m_flags = SF_VISIT | SF_SEL;
    m_current.m_num = ArticleNum{1};
    m_current.m_flags |= AF_THREADED | AF_SEL;
    m_current.m_subj_next = &m_recent;
    m_recent.m_num = ArticleNum{2};
    m_recent.m_flags |= AF_THREADED;
    m_recent.m_sibling = &m_branch;
    m_branch.m_num = ArticleNum{3};
    m_branch.m_subj = &m_branch_subject;
    m_branch.m_parent = &m_current;
    m_branch.m_flags = AF_EXISTS | AF_CACHED | AF_THREADED | AF_UNREAD;
    m_branch_subject.m_str = "Re: Branch";
    m_branch_subject.m_articles = &m_branch;
    m_branch_subject.m_thread = &m_current;
    m_branch_subject.m_thread_link = &m_subject;
    m_branch_subject.m_flags = SF_VISIT;
    m_group.m_to_read = 3;
    g_newsgroup_ptr = &m_group;
    g_newsgroup_exit_code = NG_NORM;
    g_selected_count = 1;
    g_selected_subj_cnt = 1;
    g_art = g_recent_art = ArticleNum{2};
    g_artp = &m_recent;
    g_curr_art = ArticleNum{1};
    g_last_art = ArticleNum{3};
    g_threaded_group = true;
    g_reread = g_force_last = g_do_fseek = g_rotate = false;
    g_art_pos = ArticlePosition{123};
    g_art_line_num = ArticleLine{41};
    g_top_line = ArticleLine{15};
    g_general_mode = GM_READ;
    g_mode = MM_ARTICLE;
    g_tc_LINES = 24;
    g_page_line = 9;
    g_term_line = 12;
    g_term_col = 9;
    g_int_count = 0;
    g_verbose = false;
    g_in_ng = true;
    g_erase_each_line = false;
    g_allow_typeahead = true;
    g_verify = false;
    g_tc_CR = "<cr>";
    g_tc_CE = "<ce>";
    g_tc_CD = "<cd>";
    while (macro_pending())
    {
        m_old_typeahead += read_tty_char();
    }
}

void ThreadTreeArticleTest::TearDown()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    for (auto ch = m_old_typeahead.rbegin(); ch != m_old_typeahead.rend(); ++ch)
    {
        push_char(*ch);
    }
    g_art = m_old_art;
    g_artp = m_old_artp;
    g_curr_art = m_old_curr_art;
    g_recent_art = m_old_recent_art;
    g_last_art = m_old_last_art;
    g_newsgroup_ptr = m_old_newsgroup_ptr;
    g_newsgroup_exit_code = m_old_exit_code;
    g_selected_count = m_old_selected_count;
    g_selected_subj_cnt = m_old_selected_subj_cnt;
    g_reread = m_old_reread;
    g_force_last = m_old_force_last;
    g_do_fseek = m_old_do_fseek;
    g_rotate = m_old_rotate;
    g_art_pos = m_old_art_pos;
    g_art_line_num = m_old_art_line_num;
    g_top_line = m_old_top_line;
    g_general_mode = m_old_general_mode;
    g_mode = m_old_mode;
    g_tc_LINES = m_old_tc_lines;
    g_page_line = m_old_page_line;
    g_term_line = m_old_term_line;
    g_term_col = m_old_term_col;
    g_int_count = m_old_int_count;
    g_verbose = m_old_verbose;
    g_in_ng = m_old_in_ng;
    g_erase_each_line = m_old_erase_each_line;
    g_allow_typeahead = m_old_allow_typeahead;
    g_verify = m_old_verify;
    g_s_default_cmd = m_old_s_default_cmd;
    g_univ_default_cmd = m_old_univ_default_cmd;
    g_tc_CR = m_old_tc_cr;
    g_tc_CE = m_old_tc_ce;
    g_tc_CD = m_old_tc_cd;
    TreeRenderingTest::TearDown();
}

std::string ThreadTreeArticleTest::display_tree(std::string_view input)
{
    const std::pair<ArticleNum, Article *> article = std::make_pair(g_art, g_artp);
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticleFlags                     current_flags = m_current.m_flags;
    const ArticleFlags                     recent_flags = m_recent.m_flags;
    const ArticleFlags                     branch_flags = m_branch.m_flags;
    const SubjectFlags                     subject_flags = m_subject.m_flags;
    const SubjectFlags                     branch_subject_flags = m_branch_subject.m_flags;
    const ArticleUnread                    unread = m_group.m_to_read;
    const ArticleUnread                    selected = g_selected_count;
    const bool                             selected_only = g_selected_only;
    const MinorMode                        mode = g_mode;
    const char                             interrupts = g_int_count;
    push_char('q');
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        push_char(*ch);
    }
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("t");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_EQ(article, std::make_pair(g_art, g_artp));
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(current_flags, m_current.m_flags);
    EXPECT_EQ(recent_flags, m_recent.m_flags);
    EXPECT_EQ(branch_flags, m_branch.m_flags);
    EXPECT_EQ(subject_flags, m_subject.m_flags);
    EXPECT_EQ(branch_subject_flags, m_branch_subject.m_flags);
    EXPECT_EQ(unread, m_group.m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(mode, g_mode);
    EXPECT_EQ(NG_NORM, g_newsgroup_exit_code);
    EXPECT_EQ(ArticlePosition{123}, g_art_pos);
    EXPECT_EQ(ArticleLine{41}, g_art_line_num);
    EXPECT_EQ(ArticleLine{15}, g_top_line);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_do_hiding);
    EXPECT_FALSE(g_rotate);
    EXPECT_TRUE(g_threaded_group);
    EXPECT_EQ(interrupts, g_int_count);
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
}

class ThreadTreeContextArticleTest : public ThreadTreeArticleTest, public testing::WithParamInterface<bool>
{
};

class ThreadTreeUnavailableArticleTest : public ThreadTreeArticleTest, public testing::WithParamInterface<bool>
{
};

constexpr char BRANCH_TREE[]{"\n[1] Topic\n[2] Branch\n\n  <so>[1]<se>+-[<so>1<se>]\n     \\-[2]\n\n"};

} // namespace

TEST_F(TreeRenderingTest, currentAndRecentArticlesAreMarked)
{
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("+", ArticleLine{}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    std::string expected{"+"};
    expected.append(27, ' ');
    expected += "  <so>[1]<se>--[<so>1<se>]\n";
    EXPECT_EQ(expected, output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, normalHeaderKeywordIsSplitFromValue)
{
    g_curr_artp = nullptr;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: Tester\nignored", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Tester\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, hiddenHeaderDecodesEncodedWords)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: =?US-ASCII?B?Qm96byB0aGUgQ2xvd24=?=\nignored", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Bozo the Clown\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, hiddenHeaderSubstitutesDecodedLatinScalars)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    set_char_subst_mode('a');
    init_tree();
    const std::string raw{"From: =?ISO-8859-1?Q?=C4?=\xE2\x80\x99"};

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts(raw, ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Ae\xE2\x80\x99\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
    EXPECT_EQ("From: =?ISO-8859-1?Q?=C4?=\xE2\x80\x99", raw);
}

TEST_F(TreeRenderingTest, unicodeModeSubstitutesDecodedHeaders)
{
    const std::string previous_charsets = g_charsets;
    g_charsets = "up";
    reset_char_subst_mode();
    g_curr_artp = nullptr;
    g_do_hiding = true;
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts("From: =?UTF-8?Q?=E2=80=99?=", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();
    g_charsets = previous_charsets;
    EXPECT_EQ("From: '\n", output);
}

TEST_F(TreeRenderingTest, texConversionDoesNotLeakBetweenHeaders)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    set_char_subst_mode('t');
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts("From: =?US-ASCII?Q?=22A?=", ArticleLine{1}, 0);
    tree_puts("Other: \"", ArticleLine{2}, 0);
    tree_puts("From: A", ArticleLine{3}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: \xC3\x84\nOther: \"\nFrom: A\n", output);
}

TEST_F(TreeRenderingTest, headerWrapMeasuresUtf8Columns)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    g_tc_COLS = 10;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: =?Latin1?Q?=C4=C4=C4?=", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: \xC3\x84\xC3\x84\xC3\x84\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, headerHardWrapRetainsWholeUnicodeScalars)
{
    g_curr_artp = nullptr;
    g_do_hiding = true;
    g_tc_COLS = 8;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: =?Latin1?Q?=C4=C4?=", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: \xC3\x84\n+     \xC3\x84\n", output);
    EXPECT_EQ(ArticleLine{2}, lines);
}

TEST_F(TreeRenderingTest, continuationLineUsesPreviousHeaderIndent)
{
    g_curr_artp = nullptr;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine first_lines = tree_puts("From: Tester", ArticleLine{1}, 0);
    const ArticleLine second_lines = tree_puts(" continuation", ArticleLine{2}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: Tester\n+     continuation\n", output);
    EXPECT_EQ(ArticleLine{1}, first_lines);
    EXPECT_EQ(ArticleLine{1}, second_lines);
}

TEST_F(TreeRenderingTest, longHeaderValueWrapsAtWordBoundary)
{
    g_curr_artp = nullptr;
    g_tc_COLS = 20;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("From: alpha beta gamma delta", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("From: alpha beta\n+     gamma delta\n", output);
    EXPECT_EQ(ArticleLine{2}, lines);
}

TEST_F(TreeRenderingTest, subjectLineGetsSubjectPrefixWhenUnthreaded)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("Thread title", ArticleLine{1}, 1);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Subject: Thread title\n", output);
    EXPECT_EQ(ArticleLine{1}, lines);
}

TEST_F(TreeRenderingTest, issue30SubjectRestoresUnderlineBeforeFollowingText)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    g_do_hiding = true;
    g_tc_COLS = 120;
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";
    g_fire_is_out = UNDERLINE | STANDOUT;
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts(UNDERLINE_SUBJECT, ArticleLine{1}, 1);
    std::fputs("After\n", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Subject: \x1B[4m" + std::string{UNDERLINE_DECODED_SUBJECT} + "\x1B[24m\nAfter\n", output);
}

TEST_F(TreeRenderingTest, issue30SubjectUnderlinesWholeScalarsOnUndercharTerminal)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    g_do_hiding = true;
    g_tc_COLS = 120;
    g_tc_UC = "<uc>";
    init_tree();
    std::string expected{"Subject: "};
    expected += "\xE2\x80\x9C\b<uc>";
    for (char ch : std::string_view{"Microsoft CEO says up to 30% of the company"})
    {
        expected += std::string{ch} + "\b<uc>";
    }
    expected += "\xE2\x80\x99\b<uc>";
    for (char ch : std::string_view{"s code was written by AI"})
    {
        expected += std::string{ch} + "\b<uc>";
    }
    expected += "\xE2\x80\x9D\b<uc>\nAfter\n";

    testing::internal::CaptureStdout();
    tree_puts(UNDERLINE_SUBJECT, ArticleLine{1}, 1);
    std::fputs("After\n", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(expected, output);
}

TEST_F(TreeRenderingTest, issue30WrappedSubjectRestoresUnderlineOnEveryLine)
{
    g_curr_artp = nullptr;
    g_threaded_group = false;
    g_do_hiding = true;
    g_tc_COLS = 40;
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";
    g_fire_is_out = UNDERLINE | STANDOUT;
    init_tree();

    testing::internal::CaptureStdout();
    tree_puts(UNDERLINE_SUBJECT, ArticleLine{1}, 1);
    std::fputs("After\n", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Subject: \x1B[4m\xE2\x80\x9CMicrosoft CEO says up to 30%\x1B[24m\n"
              "+        \x1B[4mof the company\xE2\x80\x99s code was\x1B[24m\n"
              "+        \x1B[4mwritten by AI\xE2\x80\x9D\x1B[24m\nAfter\n",
              output);
}

TEST_F(TreeRenderingTest, underPrintPreservesIssue30SubjectAndRestoresAttributes)
{
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";

    testing::internal::CaptureStdout();
    under_print(UNDERLINE_DECODED_SUBJECT);
    std::fputs("After", stdout);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("\x1B[4m" + std::string{UNDERLINE_DECODED_SUBJECT} + "\x1B[24mAfter", output);
}

TEST_F(TreeRenderingTest, underPrintHandlesWideCombiningAndMalformedScalars)
{
    g_tc_UC = "<uc>";
    const std::string old_input{input_charset_name()};
    const std::string old_output{output_charset_name()};
    utf_init("Latin1", "Latin1");

    testing::internal::CaptureStdout();
    under_print("\xE7\x95\x8C"
                "e\xCC\x81\xFFZ");
    const std::string output = testing::internal::GetCapturedStdout();
    utf_init(old_input, old_output);

    EXPECT_EQ("\xE7\x95\x8C\b\b<uc><uc>e\b<uc>\xCC\x81\xEF\xBF\xBD\b<uc>Z\b<uc>", output);
}

TEST_F(TreeRenderingTest, underPrintDisplaysControlsWithoutExecutingThem)
{
    g_tc_US = "\x1B[4m";
    g_tc_UE = "\x1B[24m";
    std::string text{"\x1B[4m"};
    text += '\0';
    text += "\x7F\xC2\x9B";
    for (const std::string_view capability : {std::string_view{}, std::string_view{"<uc>"}})
    {
        g_tc_UC = capability;
        std::string expected;
        for (char ch : std::string_view{"^[[4m^@^?^["})
        {
            expected += ch;
            if (!capability.empty())
            {
                expected += "\b<uc>";
            }
        }
        if (capability.empty())
        {
            expected = "\x1B[4m" + expected + "\x1B[24m";
        }

        testing::internal::CaptureStdout();
        under_print(text);
        const std::string output = testing::internal::GetCapturedStdout();

        EXPECT_EQ(expected, output);
    }
}

TEST_F(TreeRenderingTest, siblingBranchesAreRenderedOnSeparateLines)
{
    Article sibling{};
    sibling.m_subj = &m_subject;
    sibling.m_parent = &m_current;
    sibling.m_flags = AF_EXISTS | AF_CACHED | AF_UNREAD;
    m_recent.m_sibling = &sibling;

    init_tree();

    testing::internal::CaptureStdout();
    const ArticleLine lines = tree_puts("+", ArticleLine{}, 0);
    const ArticleLine second_lines = tree_puts("+", ArticleLine{1}, 0);
    const std::string output = testing::internal::GetCapturedStdout();

    std::string expected{"+"};
    expected.append(27, ' ');
    expected += "  <so>[1]<se>+-[<so>1<se>]\n";
    expected += "+";
    expected.append(27, ' ');
    expected += "     \\-[1]\n";
    EXPECT_EQ(expected, output);
    EXPECT_EQ(ArticleLine{1}, lines);
    EXPECT_EQ(ArticleLine{1}, second_lines);
}

TEST_P(ThreadTreeContextArticleTest, displaysTheLastReadArticleTreeWhilePreservingTheProspectiveArticle)
{
    if (GetParam())
    {
        g_art = ArticleNum{4};
        g_artp = nullptr;
        g_mode = MM_ARTICLE_END;
    }

    const std::string output = display_tree();

    EXPECT_EQ(std::string{"<cr><ce><cr>"} + BRANCH_TREE, output);
    EXPECT_EQ(8, g_page_line);
    EXPECT_EQ(0, m_subject.m_misc);
    EXPECT_EQ(1, m_branch_subject.m_misc);
    EXPECT_EQ(19, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

INSTANTIATE_TEST_SUITE_P(ProspectiveArticleOrGroupEnd, ThreadTreeContextArticleTest, testing::Bool());

TEST_F(ThreadTreeArticleTest, selectedOnlyModeDistinguishesUnselectedUnreadReplies)
{
    g_selected_only = true;

    const std::string output = display_tree();

    EXPECT_EQ("<cr><ce><cr>\n[1] Topic\n[2] Branch\n\n  <so>[1]<se>+-<<so>1<se>>\n     \\-<2>\n\n", output);
}

TEST_F(ThreadTreeArticleTest, readRepliesUseParenthesesWhileRetainingTheirRecentArticleMarker)
{
    m_recent.m_flags &= ~AF_UNREAD;
    --m_group.m_to_read;

    const std::string output = display_tree();

    EXPECT_EQ("<cr><ce><cr>\n[1] Topic\n[2] Branch\n\n  <so>[1]<se>+-(<so>1<se>)\n     \\-[2]\n\n", output);
}

TEST_F(ThreadTreeArticleTest, aSingleArticleThreadDisplaysItsSubjectAndCurrentArticleMarker)
{
    m_current.m_child1 = nullptr;
    m_current.m_subj_next = nullptr;
    m_subject.m_thread_link = &m_subject;

    const std::string output = display_tree();

    EXPECT_EQ("<cr><ce><cr>\n[1] Topic\n\n  <so>[1]<se>\n\n", output);
    EXPECT_EQ(6, g_page_line);
}

TEST_P(ThreadTreeUnavailableArticleTest, aGroupWithNoLastReadArticleReportsThatNoTreeIsAvailable)
{
    g_art = g_curr_art = ArticleNum{4};
    g_artp = g_curr_artp = nullptr;
    g_mode = MM_ARTICLE_END;
    g_verbose = GetParam();

    const std::string output = display_tree();

    EXPECT_EQ(std::string{"<cr><ce><cr>"} + (GetParam() ? "\nNo article tree to display.\n" : "\nNo tree.\n"), output);
    EXPECT_EQ(1, g_page_line);
    EXPECT_EQ(14, g_term_line);
    EXPECT_EQ(0, g_term_col);
}

INSTANTIATE_TEST_SUITE_P(TerseOrVerbose, ThreadTreeUnavailableArticleTest, testing::Bool());

TEST_F(ThreadTreeArticleTest, screenAndLineErasureOptionsClearToTheEndOfTheScreenBeforeDisplayingTheTree)
{
    g_erase_screen = true;
    g_erase_each_line = true;

    const std::string output = display_tree();

    EXPECT_EQ(std::string{"<cr><cd><cr>"} + BRANCH_TREE, output);
}

TEST_F(ThreadTreeArticleTest, quittingTheMorePromptAfterTheSubjectLegendStopsBeforeTheTree)
{
    g_tc_LINES = 5;

    const std::string output = display_tree("q");

    EXPECT_NE(std::string::npos, output.find("\n[1] Topic\n[2] Branch\n\n"));
    EXPECT_NE(std::string::npos, output.find("<so>[MORE] <se>"));
    EXPECT_EQ(std::string::npos, output.find("+-["));
    EXPECT_EQ(-1, g_page_line);
}

TEST_F(ThreadTreeArticleTest, continuingTheMorePromptDisplaysTheCompleteTreeAndReturnsToReading)
{
    g_tc_LINES = 5;

    const std::string output = display_tree(" ");

    EXPECT_NE(std::string::npos, output.find("\n[1] Topic\n[2] Branch\n\n"));
    EXPECT_NE(std::string::npos, output.find("<so>[MORE] <se>"));
    EXPECT_NE(std::string::npos, output.find("  <so>[1]<se>+-[<so>1<se>]\n     \\-[2]\n\n"));
    EXPECT_EQ(4, g_page_line);
}

TEST_F(ThreadTreeArticleTest, aPendingInterruptStopsRenderingWithoutConsumingTheNextArticleCommand)
{
    g_int_count = 1;

    const std::string output = display_tree();

    EXPECT_EQ("<cr><ce><cr>", output);
    EXPECT_EQ(-1, g_page_line);
}

TEST_F(ThreadTreeArticleTest, aDeepReplyChainRetainsItsVisibleAncestorsWhenTheWidthLimitClipsDescendants)
{
    std::vector<Article> replies(18);
    m_subject.m_thread_link = &m_subject;
    m_current.m_child1 = &replies.front();
    for (std::size_t index = 0; index < replies.size(); ++index)
    {
        Article &reply = replies[index];
        reply.m_num = ArticleNum{static_cast<long>(index) + 2};
        reply.m_subj = &m_subject;
        reply.m_parent = index ? &replies[index - 1] : &m_current;
        reply.m_child1 = index + 1 < replies.size() ? &replies[index + 1] : nullptr;
        reply.m_flags = AF_EXISTS | AF_CACHED | AF_THREADED | AF_UNREAD;
    }
    g_art = ArticleNum{1};
    g_artp = &m_current;
    g_recent_art = ArticleNum{};
    g_recent_artp = nullptr;
    g_last_art = ArticleNum{19};
    m_group.m_to_read = 19;
    g_tc_COLS = 80;
    std::string expected{"<cr><ce><cr>\n[1] Topic\n\n  <so>[1]<se>"};
    for (int depth = 0; depth < 16; ++depth)
    {
        expected += "--[1]";
    }
    expected += "-\n\n";

    const std::string output = display_tree();

    EXPECT_EQ(expected, output);
}
