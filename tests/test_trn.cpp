// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/trn.h>

#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/rcstuff.h>
#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <utility>

namespace
{

class TrnVersionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_patch_level = g_patch_level;
        m_old_multirc = g_multirc;
        m_old_int_count = g_int_count;
        m_old_erase_screen = g_erase_screen;
        m_old_tc_am = g_tc_AM;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_page_line = g_page_line;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;

        g_patch_level = "test-version";
        g_multirc = nullptr;
        g_int_count = 0;
        g_erase_screen = false;
        g_tc_AM = false;
        g_tc_LINES = 100;
        g_tc_COLS = 200;
        g_page_line = 1;
        g_term_line = 0;
        g_term_col = 0;
    }

    void TearDown() override
    {
        g_patch_level = std::move(m_old_patch_level);
        g_multirc = m_old_multirc;
        g_int_count = m_old_int_count;
        g_erase_screen = m_old_erase_screen;
        g_tc_AM = m_old_tc_am;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_page_line = m_old_page_line;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
    }

    std::string m_old_patch_level;
    Multirc    *m_old_multirc{};
    char        m_old_int_count{};
    bool        m_old_erase_screen{};
    bool        m_old_tc_am{};
    int         m_old_tc_lines{};
    int         m_old_tc_cols{};
    int         m_old_page_line{};
    int         m_old_term_line{};
    int         m_old_term_col{};
};

std::string version_header()
{
    return "\nTrn version: test-version.\nConfigured for "
#ifdef HAS_LOCAL_SPOOL
           "both NNTP and local news access.\n";
#else
           "NNTP (plus individual local access).\n";
#endif
}

constexpr const char *HELP_TEXT = "You can request help from:  trn-users@lists.sourceforge.net\n"
                                  "Send bug reports, suggestions, etc. to:  trn-workers@lists.sourceforge.net\n";

} // namespace

TEST_F(TrnVersionTest, displaysVersionWithoutNewsSources)
{
    testing::internal::CaptureStdout();
    trn_version();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(version_header() + HELP_TEXT, output);
}

TEST_F(TrnVersionTest, displaysNewsSourceDetails)
{
    DataSource remote_copy{};
    remote_copy.m_name = "remote-copy";
    remote_copy.m_news_id = "copy.example";
    remote_copy.m_group_desc = "copy.desc";
    remote_copy.m_act_sf.m_fp = stdout;
    remote_copy.m_act_sf.m_refetch_secs = 300;
    remote_copy.m_desc_sf.m_hp = reinterpret_cast<HashTable *>(1);
    remote_copy.m_desc_sf.m_refetch_secs = 2;
    remote_copy.m_flags = DF_REMOTE | DF_TMP_ACTIVE_FILE | DF_TRY_OVERVIEW;

    DataSource remote_local{};
    remote_local.m_name = "remote-local";
    remote_local.m_news_id = "local.example";
    remote_local.m_extra_name = "active.local";
    remote_local.m_group_desc = "local.desc";
    remote_local.m_act_sf.m_fp = stdout;
    remote_local.m_desc_sf.m_fp = stdout;
    remote_local.m_flags = DF_REMOTE | DF_TMP_GROUP_DESC;

    DataSource remote_dynamic{};
    remote_dynamic.m_name = "remote-dynamic";
    remote_dynamic.m_news_id = "dynamic.example";
    remote_dynamic.m_group_desc = "group.desc";
    remote_dynamic.m_desc_sf.m_fp = stdout;
    remote_dynamic.m_flags = DF_REMOTE;

    DataSource local{};
    local.m_name = "local";
    local.m_news_id = "active";
    local.m_spool_dir = "/news/spool";
    local.m_over_dir = "/news/overview";
    local.m_flags = DF_TRY_OVERVIEW;

    Newsrc local_rc{};
    local_rc.data_source = &local;
    local_rc.name = "local.newsrc";
    local_rc.flags = RF_ACTIVE;

    Newsrc remote_dynamic_rc{};
    remote_dynamic_rc.next = &local_rc;
    remote_dynamic_rc.data_source = &remote_dynamic;
    remote_dynamic_rc.name = "dynamic.newsrc";
    remote_dynamic_rc.flags = RF_ACTIVE;

    Newsrc remote_local_rc{};
    remote_local_rc.next = &remote_dynamic_rc;
    remote_local_rc.data_source = &remote_local;
    remote_local_rc.name = "remote-local.newsrc";
    remote_local_rc.flags = RF_ACTIVE;

    Newsrc remote_copy_rc{};
    remote_copy_rc.next = &remote_local_rc;
    remote_copy_rc.data_source = &remote_copy;
    remote_copy_rc.name = "remote-copy.newsrc";
    remote_copy_rc.flags = RF_ACTIVE;

    Multirc multirc{};
    multirc.m_first = &remote_copy_rc;
    multirc.m_num = 7;
    g_multirc = &multirc;

    testing::internal::CaptureStdout();
    trn_version();
    const std::string output = testing::internal::GetCapturedStdout();

    const std::string expected = version_header() +
                                 "\nNews source group #7:\n\n"
                                 "ID remote-copy:\nNewsrc remote-copy.newsrc.\n"
                                 "News from server copy.example.\n"
                                 "Copy of remote active file (refetch: 5 minutes).\n"
                                 "Dynamic group desc. file (refetch if missing).\n"
                                 "Overview files from the server.\n\n"
                                 "ID remote-local:\nNewsrc remote-local.newsrc.\n"
                                 "News from server local.example.\n"
                                 "Local active file: active.local.\n"
                                 "Copy of remote group desc. file.\n\n"
                                 "ID remote-dynamic:\nNewsrc dynamic.newsrc.\n"
                                 "News from server dynamic.example.\n"
                                 "Dynamic active file.\n"
                                 "Group desc. file: group.desc.\n\n"
                                 "ID local:\nNewsrc local.newsrc.\n"
                                 "News from /news/spool.\nLocal active file active.\n"
                                 "Overview files from /news/overview.\n\n" +
                                 HELP_TEXT;
    EXPECT_EQ(expected, output);
}
