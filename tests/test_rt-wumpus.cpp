// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-wumpus.h>

#include <trn/Article.h>
#include <trn/artstate.h>
#include <trn/charsubst.h>
#include <trn/ng.h>
#include <trn/rt-select.h>
#include <trn/Subject.h>
#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <string>

namespace
{

class TreeRenderingTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_curr_artp = g_curr_artp;
        m_old_recent_artp = g_recent_artp;
        m_old_selected_only = g_selected_only;
        m_old_do_hiding = g_do_hiding;
        m_old_char_subst = g_char_subst;
        m_old_max_tree_lines = g_max_tree_lines;
        m_old_tc_cols = g_tc_COLS;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_us = g_tc_US;
        m_old_tc_ue = g_tc_UE;
        m_old_tc_uc = g_tc_UC;
        m_old_fire_is_out = g_fire_is_out;
        m_old_erase_screen = g_erase_screen;

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
        g_char_subst = "";
        g_max_tree_lines = 6;
        g_tc_COLS = 40;
        g_tc_SO = m_standout_start;
        g_tc_SE = m_standout_end;
        g_tc_US = m_empty_capability;
        g_tc_UE = m_empty_capability;
        g_tc_UC = m_empty_capability;
        g_fire_is_out = 0;
        g_erase_screen = false;
    }

    void TearDown() override
    {
        g_curr_artp = nullptr;
        init_tree();

        g_curr_artp = m_old_curr_artp;
        g_recent_artp = m_old_recent_artp;
        g_selected_only = m_old_selected_only;
        g_do_hiding = m_old_do_hiding;
        g_char_subst = m_old_char_subst;
        g_max_tree_lines = m_old_max_tree_lines;
        g_tc_COLS = m_old_tc_cols;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_US = m_old_tc_us;
        g_tc_UE = m_old_tc_ue;
        g_tc_UC = m_old_tc_uc;
        g_fire_is_out = m_old_fire_is_out;
        g_erase_screen = m_old_erase_screen;
    }

    char    m_empty_capability[1]{};
    char    m_standout_start[5]{"<so>"};
    char    m_standout_end[5]{"<se>"};
    Article m_current{};
    Article m_recent{};
    Subject m_subject{};

    Article    *m_old_curr_artp{};
    Article    *m_old_recent_artp{};
    bool        m_old_selected_only{};
    bool        m_old_do_hiding{};
    const char *m_old_char_subst{};
    int         m_old_max_tree_lines{};
    int         m_old_tc_cols{};
    char       *m_old_tc_so{};
    char       *m_old_tc_se{};
    char       *m_old_tc_us{};
    char       *m_old_tc_ue{};
    const char *m_old_tc_uc{};
    int         m_old_fire_is_out{};
    bool        m_old_erase_screen{};
};

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
