// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/rcstuff.h>

#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <test_config.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "MockNNTPConnection.h"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

void drain_macro_buffer()
{
    while (macro_pending())
    {
        char discarded{};
        read_tty(&discarded, 1);
    }
}

std::vector<std::string> read_lines(const fs::path &path)
{
    std::ifstream            input{path};
    std::vector<std::string> lines;
    std::string              line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

class NewsrcRotationTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsgroup_data = std::move(g_newsgroup_data);
        m_old_newsgroup_order = std::move(g_newsgroup_order);
        m_old_newsgroup_count = g_newsgroup_count;
        m_old_newsgroup_to_read = g_newsgroup_to_read;
        m_old_first_newsgroup = g_first_newsgroup;
        m_old_last_newsgroup = g_last_newsgroup;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_current_newsgroup = g_current_newsgroup;
        m_old_recent_newsgroup = g_recent_newsgroup;
        m_old_start_here = g_start_here;
        m_old_sel_page_np = g_sel_page_np;
        m_old_sel_next_np = g_sel_next_np;
        m_old_ng_go_newsgroup_ptr = g_ng_go_newsgroup_ptr;
        m_old_multirc = g_multirc;
        m_saved_multircs.swap(g_multircs);
        m_old_data_source = g_data_source;
        m_saved_data_sources.swap(g_data_sources);
        m_old_trn_access_text = g_trn_access_text;
        m_old_use_newsrc_selector = g_use_newsrc_selector;
        m_old_nntp_link = g_nntp_link;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_sel_sort = g_sel_sort;
        m_old_sel_newsgroup_sort = g_sel_newsgroup_sort;
        m_old_sel_direction = g_sel_direction;
        m_old_add_new_by_default = g_add_new_by_default;
        m_old_append_unsub = g_append_unsub;
        m_old_fuzzy_get = g_fuzzy_get;
        m_old_novice_delays = g_novice_delays;
        m_old_verbose = g_verbose;
        m_old_verify = g_verify;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_check_flag = g_check_flag;
        m_old_int_count = g_int_count;
        m_old_erase_screen = g_erase_screen;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_am = g_tc_AM;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_sel_sort = SS_NATURAL;
        g_sel_newsgroup_sort = SS_NATURAL;
        g_sel_direction = 1;
        g_newsgroup_data.clear();
        g_newsgroup_order.clear();
        g_newsgroup_count = NewsgroupNum{};
        g_newsgroup_to_read = NewsgroupNum{};
        g_first_newsgroup = nullptr;
        g_last_newsgroup = nullptr;
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
        g_recent_newsgroup = nullptr;
        g_start_here = nullptr;
        g_sel_page_np = nullptr;
        g_sel_next_np = nullptr;
        g_ng_go_newsgroup_ptr = nullptr;
        g_multirc = nullptr;
        g_multircs.clear();
        g_data_sources.clear();
        g_trn_access_text.clear();
        g_use_newsrc_selector = false;
        g_data_source = nullptr;
        nntp_gets_clear_buffer();
        g_newsrc_hash = nullptr;
        g_check_flag = false;
        g_add_new_by_default = ADDNEW_ASK;
        g_append_unsub = false;
        g_fuzzy_get = false;
        g_novice_delays = false;
        g_verbose = true;
        g_verify = false;
        g_general_mode = GM_READ;
        g_mode = MM_NONE;
        g_int_count = 0;
        g_erase_screen = false;
        g_tc_LINES = 200;
        g_tc_COLS = 200;
        g_tc_SO = m_empty_tc_so;
        g_tc_SE = m_empty_tc_se;
        g_tc_AM = false;
    }

    void TearDown() override
    {
        if (g_multirc != nullptr && g_multirc != m_old_multirc)
        {
            unuse_multirc(g_multirc);
        }
        m_data_source.close();
        if (g_newsrc_hash != nullptr && g_newsrc_hash != m_old_newsrc_hash)
        {
            hash_destroy(g_newsrc_hash);
            g_newsrc_hash = nullptr;
        }
        drain_macro_buffer();

        std::error_code error;
        fs::remove_all(m_output_dir, error);

        g_newsgroup_data = std::move(m_old_newsgroup_data);
        g_newsgroup_order = std::move(m_old_newsgroup_order);
        g_newsgroup_count = m_old_newsgroup_count;
        g_newsgroup_to_read = m_old_newsgroup_to_read;
        g_first_newsgroup = m_old_first_newsgroup;
        g_last_newsgroup = m_old_last_newsgroup;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_current_newsgroup = m_old_current_newsgroup;
        g_recent_newsgroup = m_old_recent_newsgroup;
        g_start_here = m_old_start_here;
        g_sel_page_np = m_old_sel_page_np;
        g_sel_next_np = m_old_sel_next_np;
        g_ng_go_newsgroup_ptr = m_old_ng_go_newsgroup_ptr;
        g_multirc = m_old_multirc;
        g_multircs.clear();
        m_saved_multircs.swap(g_multircs);
        g_data_sources.clear();
        m_saved_data_sources.swap(g_data_sources);
        g_trn_access_text = m_old_trn_access_text;
        g_use_newsrc_selector = m_old_use_newsrc_selector;
        g_data_source = m_old_data_source;
        g_nntp_link = m_old_nntp_link;
        nntp_gets_clear_buffer();
        g_newsrc_hash = m_old_newsrc_hash;
        g_sel_sort = m_old_sel_sort;
        g_sel_newsgroup_sort = m_old_sel_newsgroup_sort;
        g_sel_direction = m_old_sel_direction;
        g_add_new_by_default = m_old_add_new_by_default;
        g_append_unsub = m_old_append_unsub;
        g_fuzzy_get = m_old_fuzzy_get;
        g_novice_delays = m_old_novice_delays;
        g_verbose = m_old_verbose;
        g_verify = m_old_verify;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_check_flag = m_old_check_flag;
        g_int_count = m_old_int_count;
        g_erase_screen = m_old_erase_screen;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_AM = m_old_tc_am;
    }

    Newsrc make_newsrc()
    {
        Newsrc newsrc{};
        newsrc.data_source = &m_data_source;
        newsrc.name = m_output_dir / "newsrc";
        newsrc.old_name = m_output_dir / "old-newsrc";
        newsrc.new_name = m_output_dir / "new-newsrc";
        newsrc.info_name = m_output_dir / "newsrc.info";
        newsrc.flags = RF_ACTIVE | RF_RC_CHANGED;
        return newsrc;
    }

    void add_newsgroup(Newsrc &newsrc, std::string line)
    {
        NewsgroupData &group = g_newsgroup_data.emplace_back();
        group.m_rc = &newsrc;
        group.m_rc_line = std::move(line);
        append_newsgroup_order(&group);
        g_newsgroup_count = NewsgroupNum{static_cast<long>(g_newsgroup_data.size())};
    }

    fs::path                     m_output_dir;
    DataSource                   m_data_source{};
    std::vector<NewsgroupData>   m_old_newsgroup_data;
    std::vector<NewsgroupData *> m_old_newsgroup_order;
    NewsgroupNum                 m_old_newsgroup_count{};
    NewsgroupNum                 m_old_newsgroup_to_read{};
    NewsgroupData               *m_old_first_newsgroup{};
    NewsgroupData               *m_old_last_newsgroup{};
    NewsgroupData               *m_old_newsgroup_ptr{};
    NewsgroupData               *m_old_current_newsgroup{};
    NewsgroupData               *m_old_recent_newsgroup{};
    NewsgroupData               *m_old_start_here{};
    NewsgroupData               *m_old_sel_page_np{};
    NewsgroupData               *m_old_sel_next_np{};
    NewsgroupData               *m_old_ng_go_newsgroup_ptr{};
    Multirc                     *m_old_multirc{};
    std::vector<Multirc>         m_saved_multircs;
    DataSource                  *m_old_data_source{};
    std::vector<DataSource>      m_saved_data_sources;
    std::string                  m_old_trn_access_text;
    bool                         m_old_use_newsrc_selector{};
    NNTPLink                     m_old_nntp_link{};
    HashTable                   *m_old_newsrc_hash{};
    SelectionSortMode            m_old_sel_sort{};
    SelectionSortMode            m_old_sel_newsgroup_sort{};
    int                          m_old_sel_direction{};
    AddNewType                   m_old_add_new_by_default{};
    bool                         m_old_append_unsub{};
    bool                         m_old_fuzzy_get{};
    bool                         m_old_novice_delays{};
    bool                         m_old_verbose{};
    bool                         m_old_verify{};
    GeneralMode                  m_old_general_mode{};
    MinorMode                    m_old_mode{};
    bool                         m_old_check_flag{};
    char                         m_old_int_count{};
    bool                         m_old_erase_screen{};
    int                          m_old_tc_lines{};
    int                          m_old_tc_cols{};
    const char                  *m_old_tc_so{};
    const char                  *m_old_tc_se{};
    bool                         m_old_tc_am{};
    char                         m_empty_tc_so[1]{};
    char                         m_empty_tc_se[1]{};
    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
};

} // namespace

TEST_F(NewsrcRotationTest, writeNewsrcsReplacesNewsrcWithNewFile)
{
    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;
    add_newsgroup(newsrc, "comp.lang.apl: 1-3");
    std::ofstream{newsrc.name} << "old contents\n";

    ASSERT_TRUE(write_newsrcs(&multirc));

    EXPECT_EQ((std::vector<std::string>{"comp.lang.apl: 1-3"}), read_lines(newsrc.name));
    EXPECT_FALSE(fs::exists(newsrc.new_name));
}

TEST_F(NewsrcRotationTest, getOldNewsrcsRestoresBackupFile)
{
    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;
    std::ofstream{newsrc.name} << "current contents\n";
    std::ofstream{newsrc.old_name} << "old contents\n";
    std::ofstream{newsrc.new_name} << "temporary contents\n";

    get_old_newsrcs(&multirc);

    EXPECT_EQ((std::vector<std::string>{"old contents"}), read_lines(newsrc.name));
    EXPECT_EQ((std::vector<std::string>{"current contents"}), read_lines(newsrc.new_name));
    EXPECT_FALSE(fs::exists(newsrc.old_name));
}

TEST_F(NewsrcRotationTest, useMultircRefreshesBackupFile)
{
    const fs::path active_path = m_output_dir / "active";
    std::ofstream{active_path} << "comp.lang.apl 0000000003 0000000001 y\n";
    m_data_source.m_news_id = active_path.generic_string();

    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;
    newsrc.flags = RF_NONE;
    std::ofstream{newsrc.name} << "comp.lang.apl: 1\n";
    std::ofstream{newsrc.old_name} << "stale backup\n";

    ASSERT_TRUE(multirc.use_multirc());
    const fs::path lock_path{newsrc.lock_name};

    EXPECT_EQ(fs::path{newsrc.name.generic_string() + ".LOCK"}, newsrc.lock_name);
    EXPECT_EQ(fs::path{newsrc.name.generic_string() + ".info"}, newsrc.info_name);
    EXPECT_TRUE(fs::exists(lock_path));
    EXPECT_EQ((std::vector<std::string>{"comp.lang.apl: 1"}), read_lines(newsrc.old_name));
    unuse_multirc(&multirc);
    EXPECT_FALSE(fs::exists(lock_path));
}

TEST_F(NewsrcRotationTest, useMultircReadsLongOptionsLine)
{
    const fs::path active_path = m_output_dir / "active";
    std::ofstream{active_path};
    m_data_source.m_news_id = active_path.generic_string();

    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;
    newsrc.flags = RF_NONE;
    const std::string options_line = "options " + std::string(LINE_BUF_LEN * 2, 'x');
    std::ofstream{newsrc.name} << options_line << '\n';

    ASSERT_TRUE(multirc.use_multirc());

    ASSERT_EQ(1, g_newsgroup_order.size());
    EXPECT_EQ(options_line, g_newsgroup_order[0]->m_rc_line);
    EXPECT_EQ(TR_JUNK, g_newsgroup_order[0]->m_to_read);
    unuse_multirc(&multirc);
}

TEST_F(NewsrcRotationTest, abandonNewsgroupRestoresMatchingOldNewsrcLine)
{
    const fs::path active_path = m_output_dir / "active";
    std::ofstream{active_path} << "comp.lang.apl 0000000099 0000000001 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_path, "", ""));

    Newsrc            newsrc = make_newsrc();
    const std::string group_name{"comp.lang.apl"};
    std::ofstream{newsrc.old_name} << "comp.lang.apl.extra: 1\n"
                                   << "comp.lang.apl: 2-9\n";

    NewsgroupData group{};
    group.m_rc = &newsrc;
    group.m_rc_line = group_name + ": 99";
    group.m_num_offset = static_cast<int>(group_name.size() + 1);
    group.m_subscribe_char = ':';
    group.m_abs_first = ArticleNum{42};

    group.abandon_newsgroup();

    group.show_subscribe_char();
    EXPECT_EQ("comp.lang.apl: 2-9", group.m_rc_line);
    EXPECT_EQ(':', group.m_subscribe_char);
    EXPECT_EQ(ArticleNum{42}, group.m_abs_first);
}

TEST_F(NewsrcRotationTest, rcstuffInitCreatesCompanionPathsFromNewsrcName)
{
    const fs::path newsrc_path = m_output_dir / "custom.newsrc";
    g_use_newsrc_selector = true;
    g_data_sources.emplace_back();
    DataSource &source = g_data_sources.back();
    source.m_name = "default";
    g_trn_access_text = "[Group 1]\n"
                        "ID = default\n"
                        "Newsrc = " +
                        newsrc_path.generic_string() + "\n";

    ASSERT_TRUE(rcstuff_init());

    Multirc *multirc = multirc_ptr(1);
    ASSERT_NE(nullptr, multirc);
    ASSERT_NE(nullptr, multirc->m_first);
    EXPECT_EQ(newsrc_path, multirc->m_first->name);
    EXPECT_EQ(fs::path{newsrc_path.generic_string() + ".old"}, multirc->m_first->old_name);
    EXPECT_EQ(fs::path{newsrc_path.generic_string() + ".new"}, multirc->m_first->new_name);
}

TEST_F(NewsrcRotationTest, useMultircCreatesNewsrcFromRemoteSubscriptions)
{
    const fs::path active_path = m_output_dir / "active";
    std::ofstream{active_path} << "215 0000000001 0000000001 y\n"
                                  "comp.lang.apl 0000000042 0000000007 y\n"
                                  "comp.lang.c++ 0000000200 0000000100 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_path, "", ""));

    m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    m_data_source.m_flags = DF_REMOTE | DF_OPEN;
    m_data_source.m_nntp_link.connection = m_connection;
    m_data_source.m_nntp_link.flags = NNTP_NEW_CMD_OK;

    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;

    testing::InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LIST SUBSCRIPTIONS"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("215 subscriptions follow"))
        .WillOnce(testing::Return("comp.lang.apl:"))
        .WillOnce(testing::Return("comp.lang.c++!"))
        .WillOnce(testing::Return("."));
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("QUIT"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_)).WillOnce(testing::Return("205 closing"));

    ASSERT_TRUE(multirc.use_multirc());

    EXPECT_EQ((std::vector<std::string>{"comp.lang.apl: 1-6", "comp.lang.c++!"}), read_lines(newsrc.name));
    unuse_multirc(&multirc);
}

TEST_F(NewsrcRotationTest, getNewsgroupPromptsForMissingNewsrcGroup)
{
    const fs::path active_path = m_output_dir / "active";
    std::ofstream{active_path} << "comp.lang.apl 0000000003 0000000001 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_path, "", ""));

    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    newsrc.flags = RF_ADD_GROUPS | RF_ACTIVE;
    multirc.m_first = &newsrc;
    g_multirc = &multirc;
    g_newsrc_hash = hash_create(3001, nullptr);

    push_char('n');
    testing::internal::CaptureStdout();
    const bool        found = get_newsgroup("comp.lang.apl", GNG_NONE);
    const std::string output = testing::internal::GetCapturedStdout();
    g_multirc = nullptr;

    EXPECT_FALSE(found);
    EXPECT_EQ("\nNewsgroup comp.lang.apl not in .newsrc -- subscribe? [ynYN] \n", output);
}

TEST_F(NewsrcRotationTest, listNewsgroupsPrintsStatusAndNames)
{
    Newsrc newsrc = make_newsrc();
    g_newsgroup_data.reserve(2);
    g_newsgroup_order.reserve(2);
    add_newsgroup(newsrc, "comp.lang.apl: 1-3");
    add_newsgroup(newsrc, "comp.lang.c++! 1-2");
    g_newsgroup_order[0]->m_to_read = ArticleUnread{-1};
    g_newsgroup_order[1]->m_to_read = ArticleUnread{-2};

    testing::internal::CaptureStdout();
    list_newsgroups();
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("\n  #  Status  Newsgroup\n  0 (UNSUB)  comp.lang.apl: 1-3\n"
              "  1   (DUP)  comp.lang.c++! 1-2\n",
              output);
}
