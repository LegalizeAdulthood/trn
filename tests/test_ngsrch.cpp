// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngsrch.h>

#include <trn/addng.h>
#include <trn/final.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

#include <utility>

namespace
{

class NewsgroupSearchTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_groups = std::move(g_add_groups);
        g_add_groups = {};
        m_old_sel_page_gp = g_sel_page_gp;
        m_old_sel_next_gp = g_sel_next_gp;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_newsgroup_to_read = g_newsgroup_to_read;
        m_old_general_mode = g_general_mode;
        m_old_use_threads = g_use_threads;
        m_old_page_line = g_page_line;
        m_old_perform_count = g_perform_count;
        m_old_selected_count = g_selected_count;
        m_old_sel_mask = g_sel_mask;
        m_old_int_count = g_int_count;

        m_group = g_add_groups.add("comp/lang", 0, 0, nullptr);
        g_sel_page_gp = AddGroupIndex::None;
        g_sel_next_gp = AddGroupIndex::None;
        g_newsgroup_ptr = nullptr;
        g_newsgroup_to_read = NewsgroupNum{1};
        g_general_mode = GM_READ;
        g_use_threads = false;
        g_page_line = 0;
        g_perform_count = 0;
        g_selected_count = ArticleUnread{};
        g_sel_mask = AGF_SEL;
        g_int_count = 0;

        newsgroup_search_init();
    }

    void TearDown() override
    {
        g_add_groups = std::move(m_old_groups);
        g_sel_page_gp = m_old_sel_page_gp;
        g_sel_next_gp = m_old_sel_next_gp;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_newsgroup_to_read = m_old_newsgroup_to_read;
        g_general_mode = m_old_general_mode;
        g_use_threads = m_old_use_threads;
        g_page_line = m_old_page_line;
        g_perform_count = m_old_perform_count;
        g_selected_count = m_old_selected_count;
        g_sel_mask = m_old_sel_mask;
        g_int_count = m_old_int_count;
    }

    AddGroupTable  m_old_groups;
    AddGroupIndex  m_group{};
    AddGroupIndex  m_old_sel_page_gp{};
    AddGroupIndex  m_old_sel_next_gp{};
    NewsgroupData *m_old_newsgroup_ptr{};
    NewsgroupNum   m_old_newsgroup_to_read{};
    GeneralMode    m_old_general_mode{};
    bool           m_old_use_threads{};
    int            m_old_page_line{};
    int            m_old_perform_count{};
    ArticleUnread  m_old_selected_count{};
    AddGroupFlags  m_old_sel_mask{};
    char           m_old_int_count{};
};

} // namespace

TEST_F(NewsgroupSearchTest, selectsAddGroupWithEscapedDelimiterAndCommand)
{
    EXPECT_EQ(NGS_DONE, newsgroup_search("/comp\\/lang/:+", false));
    EXPECT_EQ(AGF_SEL, g_add_groups[m_group].m_flags & AGF_SEL);
    EXPECT_EQ(ArticleUnread{1}, g_selected_count);
    EXPECT_EQ(1, g_perform_count);
}

TEST_F(NewsgroupSearchTest, emptyPatternReusesPreviousSearch)
{
    testing::internal::CaptureStdout();
    EXPECT_EQ(NGS_FOUND, newsgroup_search("/comp\\/lang/", false));
    EXPECT_EQ(NGS_FOUND, newsgroup_search("//", false));
    (void) testing::internal::GetCapturedStdout();
}
