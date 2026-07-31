// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/trn.h>

#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/terminal.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

namespace fs = std::filesystem;

void drain_macro_buffer()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
}

int compare_newsgroup_name(std::string_view key, HashDatum data)
{
    const NewsgroupData *group = reinterpret_cast<NewsgroupData *>(data.dat_ptr);

    return key.compare(group->rc_name());
}

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

class InputNewsgroupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsgroup_data = std::move(g_newsgroup_data);
        m_old_newsgroup_order = std::move(g_newsgroup_order);
        m_old_newsgroup_count = g_newsgroup_count;
        m_old_newsgroup_to_read = g_newsgroup_to_read;
        m_old_newsgroup_min_to_read = g_newsgroup_min_to_read;
        m_old_first_newsgroup = g_first_newsgroup;
        m_old_last_newsgroup = g_last_newsgroup;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_current_newsgroup = g_current_newsgroup;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_multirc = g_multirc;
        m_old_data_source = g_data_source;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_newsgroup_dir = g_newsgroup_dir;
        m_old_default_cmd = g_default_cmd;
        m_old_add_new_by_default = g_add_new_by_default;
        m_old_fuzzy_get = g_fuzzy_get;
        m_old_novice_delays = g_novice_delays;
        m_old_verbose = g_verbose;
        m_old_verify = g_verify;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_to_read_quiet = g_to_read_quiet;
        m_old_int_count = g_int_count;
        m_old_errno = errno;
        m_old_erase_char = g_erase_char;
        m_old_kill_char = g_kill_char;
        m_old_erase_screen = g_erase_screen;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_tc_bc = g_tc_BC;
        m_old_tc_up = g_tc_UP;
        m_old_tc_cr = g_tc_CR;
        m_old_tc_vb = g_tc_VB;
        m_old_tc_ce = g_tc_CE;
        m_old_tc_cm = g_tc_CM;
        m_old_tc_ho = g_tc_HO;
        m_old_tc_il = g_tc_IL;
        m_old_tc_cd = g_tc_CD;
        m_old_tc_so = g_tc_SO;
        m_old_tc_se = g_tc_SE;
        m_old_tc_us = g_tc_US;
        m_old_tc_ue = g_tc_UE;
        m_old_tc_uc = g_tc_UC;
        m_old_tc_am = g_tc_AM;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        drain_macro_buffer();
        g_newsgroup_data.clear();
        g_newsgroup_order.clear();
        g_newsgroup_data.reserve(3);
        g_newsgroup_order.reserve(3);
        g_newsgroup_count = NewsgroupNum{};
        g_newsgroup_to_read = NewsgroupNum{};
        g_newsgroup_min_to_read = TR_NONE;
        g_first_newsgroup = nullptr;
        g_last_newsgroup = nullptr;
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
        g_newsrc_hash = hash_create(17, compare_newsgroup_name);
        g_multirc = &m_multirc;
        g_data_source = &m_data_source;
        g_newsgroup_name.clear();
        g_newsgroup_dir.clear();
        g_default_cmd = "npq";
        g_add_new_by_default = ADDNEW_ASK;
        g_fuzzy_get = false;
        g_novice_delays = false;
        g_verbose = false;
        g_verify = false;
        g_general_mode = GM_READ;
        g_mode = MM_NONE;
        g_to_read_quiet = true;
        g_int_count = 0;
        errno = 0;
        g_erase_char = '\b';
        g_kill_char = Ctl('u');
        g_erase_screen = false;
        g_term_line = 0;
        g_term_col = 0;
        g_tc_LINES = 200;
        g_tc_COLS = 200;
        g_tc_BC = m_empty_tc;
        g_tc_UP = m_empty_tc;
        g_tc_CR = m_empty_tc;
        g_tc_VB = m_empty_tc;
        g_tc_CE = m_empty_tc;
        g_tc_CM = m_empty_tc;
        g_tc_HO = m_empty_tc;
        g_tc_IL = m_empty_tc;
        g_tc_CD = m_empty_tc;
        g_tc_SO = m_empty_tc;
        g_tc_SE = m_empty_tc;
        g_tc_US = m_empty_tc;
        g_tc_UE = m_empty_tc;
        g_tc_UC = m_empty_tc;
        g_tc_AM = false;

        m_data_source.m_flags = DF_REMOTE;
        m_data_source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
        m_newsrc.data_source = &m_data_source;
        m_newsrc.flags = RF_ACTIVE;
        m_multirc.m_first = &m_newsrc;
        m_multirc.m_num = 1;
        ASSERT_EQ(1, m_data_source.m_act_sf.open({}, "", ""));
    }

    void TearDown() override
    {
        drain_macro_buffer();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        m_data_source.close();
        if (g_newsrc_hash != nullptr && g_newsrc_hash != m_old_newsrc_hash)
        {
            hash_destroy(g_newsrc_hash);
            g_newsrc_hash = nullptr;
        }

        g_newsgroup_data = std::move(m_old_newsgroup_data);
        g_newsgroup_order = std::move(m_old_newsgroup_order);
        g_newsgroup_count = m_old_newsgroup_count;
        g_newsgroup_to_read = m_old_newsgroup_to_read;
        g_newsgroup_min_to_read = m_old_newsgroup_min_to_read;
        g_first_newsgroup = m_old_first_newsgroup;
        g_last_newsgroup = m_old_last_newsgroup;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_current_newsgroup = m_old_current_newsgroup;
        g_newsrc_hash = m_old_newsrc_hash;
        g_multirc = m_old_multirc;
        g_data_source = m_old_data_source;
        g_newsgroup_name = std::move(m_old_newsgroup_name);
        g_newsgroup_dir = std::move(m_old_newsgroup_dir);
        g_default_cmd = std::move(m_old_default_cmd);
        g_add_new_by_default = m_old_add_new_by_default;
        g_fuzzy_get = m_old_fuzzy_get;
        g_novice_delays = m_old_novice_delays;
        g_verbose = m_old_verbose;
        g_verify = m_old_verify;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_to_read_quiet = m_old_to_read_quiet;
        g_int_count = m_old_int_count;
        errno = m_old_errno;
        g_erase_char = m_old_erase_char;
        g_kill_char = m_old_kill_char;
        g_erase_screen = m_old_erase_screen;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_tc_BC = m_old_tc_bc;
        g_tc_UP = m_old_tc_up;
        g_tc_CR = m_old_tc_cr;
        g_tc_VB = m_old_tc_vb;
        g_tc_CE = m_old_tc_ce;
        g_tc_CM = m_old_tc_cm;
        g_tc_HO = m_old_tc_ho;
        g_tc_IL = m_old_tc_il;
        g_tc_CD = m_old_tc_cd;
        g_tc_SO = m_old_tc_so;
        g_tc_SE = m_old_tc_se;
        g_tc_US = m_old_tc_us;
        g_tc_UE = m_old_tc_ue;
        g_tc_UC = m_old_tc_uc;
        g_tc_AM = m_old_tc_am;
    }

    void add_newsgroup(std::string_view name, std::string_view numbers)
    {
        const std::string active_line = std::string{name} + " 0000000003 0000000001 y\n";
        (void) m_data_source.m_act_sf.append(active_line, static_cast<int>(name.size()));

        NewsgroupData &group = g_newsgroup_data.emplace_back();
        group = {};
        group.m_rc = &m_newsrc;
        group.m_rc_line = name;
        group.m_rc_line += ": ";
        group.m_rc_line += numbers;
        group.m_num_offset = static_cast<int>(name.size()) + 1;
        group.m_subscribe_char = ':';
        group.m_abs_first = ArticleNum{1};
        group.m_ng_max = ArticleNum{3};
        group.m_to_read = TR_NONE;
        group.hide_subscribe_char();
        append_newsgroup_order(&group);
        g_newsgroup_count = NewsgroupNum{static_cast<long>(g_newsgroup_data.size())};

        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&group);
        data.dat_len = static_cast<unsigned>(group.rc_name().size());
        hash_store(g_newsrc_hash, group.rc_name(), data);
    }

    void add_test_newsgroups()
    {
        add_newsgroup("comp.lang.apl", "1-3");
        add_newsgroup("comp.lang.cpp", "1-3");
    }

    void set_current_newsgroup(std::size_t index)
    {
        g_newsgroup_ptr = &g_newsgroup_data[index];
        g_current_newsgroup = g_newsgroup_ptr;
        set_newsgroup_name(g_newsgroup_ptr->rc_line());
    }

    static void push_command(std::string_view command)
    {
        for (std::string_view::const_reverse_iterator ch = command.rbegin(); ch != command.rend(); ++ch)
        {
            push_char(*ch);
        }
    }

    DataSource                   m_data_source{};
    Newsrc                       m_newsrc{};
    Multirc                      m_multirc{};
    fs::path                     m_output_dir;
    std::vector<NewsgroupData>   m_old_newsgroup_data;
    std::vector<NewsgroupData *> m_old_newsgroup_order;
    NewsgroupNum                 m_old_newsgroup_count{};
    NewsgroupNum                 m_old_newsgroup_to_read{};
    ArticleUnread                m_old_newsgroup_min_to_read{};
    NewsgroupData               *m_old_first_newsgroup{};
    NewsgroupData               *m_old_last_newsgroup{};
    NewsgroupData               *m_old_newsgroup_ptr{};
    NewsgroupData               *m_old_current_newsgroup{};
    HashTable                   *m_old_newsrc_hash{};
    Multirc                     *m_old_multirc{};
    DataSource                  *m_old_data_source{};
    std::string                  m_old_newsgroup_name;
    std::string                  m_old_newsgroup_dir;
    std::string                  m_old_default_cmd;
    AddNewType                   m_old_add_new_by_default{};
    bool                         m_old_fuzzy_get{};
    bool                         m_old_novice_delays{};
    bool                         m_old_verbose{};
    bool                         m_old_verify{};
    GeneralMode                  m_old_general_mode{};
    MinorMode                    m_old_mode{};
    bool                         m_old_to_read_quiet{};
    char                         m_old_int_count{};
    int                          m_old_errno{};
    char                         m_old_erase_char{};
    char                         m_old_kill_char{};
    bool                         m_old_erase_screen{};
    int                          m_old_term_line{};
    int                          m_old_term_col{};
    int                          m_old_tc_lines{};
    int                          m_old_tc_cols{};
    std::string_view             m_old_tc_bc;
    std::string_view             m_old_tc_up;
    std::string_view             m_old_tc_cr;
    std::string_view             m_old_tc_vb;
    std::string_view             m_old_tc_ce;
    std::string_view             m_old_tc_cm;
    std::string_view             m_old_tc_ho;
    std::string_view             m_old_tc_il;
    std::string_view             m_old_tc_cd;
    std::string_view             m_old_tc_so;
    std::string_view             m_old_tc_se;
    std::string_view             m_old_tc_us;
    std::string_view             m_old_tc_ue;
    std::string_view             m_old_tc_uc;
    bool                         m_old_tc_am{};
    char                         m_empty_tc[1]{};
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

TEST_F(InputNewsgroupTest, gotoExplicitGroupName)
{
    add_test_newsgroups();
    set_current_newsgroup(0);
    push_command("g comp.lang.cpp\n");

    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ("comp.lang.cpp", g_newsgroup_name);
}

TEST_F(InputNewsgroupTest, gotoNumericGroup)
{
    add_test_newsgroups();
    set_current_newsgroup(0);
    push_command("g 1\n");

    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ("comp.lang.cpp", g_newsgroup_name);
}

TEST_F(InputNewsgroupTest, searchFindsNamedGroup)
{
    add_test_newsgroups();
    set_current_newsgroup(0);
    push_command("/cpp\n");

    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
}

TEST_F(InputNewsgroupTest, exitAbandonConfirmationQuits)
{
    push_command("xy");

    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(ING_QUIT, result);
}

TEST_F(InputNewsgroupTest, abandonCurrentNewsgroupConfirmed)
{
    add_test_newsgroups();
    set_current_newsgroup(0);
    m_newsrc.old_name = m_output_dir / "old-newsrc";
    std::ofstream{m_newsrc.old_name} << "comp.lang.apl: 2-3\n";
    push_command("Ay");

    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ("comp.lang.apl", std::string{g_newsgroup_data[0].rc_name()});
    EXPECT_EQ(" 2-3", std::string{g_newsgroup_data[0].rc_numbers()});
}
