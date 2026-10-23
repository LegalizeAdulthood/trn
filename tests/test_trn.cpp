// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/trn.h>

#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <trn/addng.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/decode.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/intrp.h>
#include <trn/mime.h>
#include <trn/ng-internal.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngsrch.h>
#include <trn/nntp.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/respond-internal.h>
#include <trn/respond.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/scorefile.h>
#include <trn/scoresave.h>
#include <trn/search.h>
#include <trn/smisc.h>
#include <trn/sorder.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/univ.h>
#include <trn/util.h>
#include <util/env.h>

#include <config/fdio.h>
#include <config/user_id.h>

#ifdef I_UNISTD
#include <unistd.h>
#endif

#include <test_config.h>
#include <test_process_config.h>

#include "mock_env.h"
#include "MockNNTPConnection.h"
#include "support/MockNntpServer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
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
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_first_newsgroup = g_first_newsgroup;
        m_old_last_newsgroup = g_last_newsgroup;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_current_newsgroup = g_current_newsgroup;
        m_old_article_list = std::move(g_article_list);
        m_old_art_ptr_list = std::move(g_art_ptr_list);
        m_old_art_ptr = g_art_ptr;
        m_old_first_cached = g_first_cached;
        m_old_last_cached = g_last_cached;
        m_old_cached_all_in_range = g_cached_all_in_range;
        m_old_art = g_art;
        m_old_recent_art = g_recent_art;
        m_old_curr_art = g_curr_art;
        m_old_artp = g_artp;
        m_old_recent_artp = g_recent_artp;
        m_old_curr_artp = g_curr_artp;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_multirc = g_multirc;
        m_old_data_source = g_data_source;
        m_old_nntp_link = g_nntp_link;
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
        m_old_threaded_group = g_threaded_group;
        m_old_in_ng = g_in_ng;
        m_old_use_threads = g_use_threads;
        m_old_thread_always = g_thread_always;
        m_old_unsafe_rc_saves = g_unsafe_rc_saves;
        m_old_use_news_selector = g_use_news_selector;
        m_old_selected_only = g_selected_only;
        m_old_selected_count = g_selected_count;
        m_old_keep_the_group_static = g_keep_the_group_static;
        m_old_sa_initialized = g_sa_initialized;
        m_old_sc_initialized = g_sc_initialized;
        m_old_univ_ng_virt_flag = g_univ_ng_virt_flag;
        m_old_univ_read_virt_flag = g_univ_read_virt_flag;
        m_old_univ_default_cmd = g_univ_default_cmd;
        m_old_univ_follow = g_univ_follow;
        m_old_univ_follow_temp = g_univ_follow_temp;
        m_old_s_follow_temp = g_s_follow_temp;
        m_old_parsed_art = g_parsed_art;
        m_old_head_buf = g_head_buf;
        m_old_in_header = g_in_header;
        m_old_art_pos = g_art_pos;
        m_old_art_line_num = g_art_line_num;
        m_old_art_fp = g_art_fp;
        m_old_open_art = g_open_art;
        m_old_raw_art_size = g_raw_art_size;
        m_old_art_size = g_art_size;
        m_old_our_pid = g_our_pid;
        m_old_tmp_dir = g_tmp_dir;
        m_old_current_path = fs::current_path();
        std::copy(std::begin(g_header_type), std::end(g_header_type), m_old_header_type.begin());
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

        g_tmp_dir = m_output_dir.generic_string();
        drain_macro_buffer();
        g_newsgroup_data.clear();
        g_newsgroup_order.clear();
        g_newsgroup_data.reserve(3);
        g_newsgroup_order.reserve(3);
        g_newsgroup_count = NewsgroupNum{};
        g_newsgroup_to_read = NewsgroupNum{};
        g_newsgroup_min_to_read = TR_NONE;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{};
        g_first_newsgroup = nullptr;
        g_last_newsgroup = nullptr;
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
        g_article_list.clear();
        g_art_ptr_list.clear();
        g_art_ptr = nullptr;
        g_first_cached = ArticleNum{};
        g_last_cached = ArticleNum{};
        g_cached_all_in_range = false;
        g_art = ArticleNum{};
        g_recent_art = ArticleNum{};
        g_curr_art = ArticleNum{};
        g_artp = nullptr;
        g_recent_artp = nullptr;
        g_curr_artp = nullptr;
        g_newsrc_hash = hash_create(17, compare_newsgroup_name);
        g_multirc = &m_multirc;
        g_data_source = &m_data_source;
        init_nntp();
        g_nntp_link.connection.reset();
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
        g_threaded_group = false;
        g_in_ng = false;
        g_use_threads = false;
        g_thread_always = false;
        g_unsafe_rc_saves = true;
        g_use_news_selector = -1;
        g_selected_only = false;
        g_selected_count = TR_NONE;
        g_keep_the_group_static = 0;
        g_sa_initialized = false;
        g_sc_initialized = false;
        g_univ_ng_virt_flag = false;
        g_univ_read_virt_flag = false;
        g_univ_default_cmd = false;
        g_univ_follow = false;
        g_univ_follow_temp = false;
        g_s_follow_temp = false;
        g_parsed_art = ArticleNum{};
        g_head_buf.clear();
        g_in_header = HEAD_FIRST;
        g_art_pos = ArticlePosition{};
        g_art_line_num = ArticleLine{};
        g_art_fp = nullptr;
        g_open_art = ArticleNum{};
        g_raw_art_size = ArticlePosition{};
        g_art_size = ArticlePosition{};
        g_our_pid = 24680;
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

        head_init();
        art_io_init();
        back_page_init();
        cache_init();
        (void) nntp_art_name(ArticleNum{}, false);

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
        if (g_art_fp != nullptr)
        {
            std::fclose(g_art_fp);
            g_art_fp = nullptr;
        }
        (void) nntp_art_name(ArticleNum{}, false);
        art_io_final();
        back_page_final();
        head_final();
        std::error_code error;
        fs::current_path(m_old_current_path, error);
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
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_first_newsgroup = m_old_first_newsgroup;
        g_last_newsgroup = m_old_last_newsgroup;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_current_newsgroup = m_old_current_newsgroup;
        g_article_list.clear();
        g_article_list = std::move(m_old_article_list);
        g_art_ptr_list = std::move(m_old_art_ptr_list);
        g_art_ptr = m_old_art_ptr;
        g_first_cached = m_old_first_cached;
        g_last_cached = m_old_last_cached;
        g_cached_all_in_range = m_old_cached_all_in_range;
        g_art = m_old_art;
        g_recent_art = m_old_recent_art;
        g_curr_art = m_old_curr_art;
        g_artp = m_old_artp;
        g_recent_artp = m_old_recent_artp;
        g_curr_artp = m_old_curr_artp;
        g_newsrc_hash = m_old_newsrc_hash;
        g_multirc = m_old_multirc;
        g_data_source = m_old_data_source;
        g_nntp_link.connection.reset();
        g_nntp_link = m_old_nntp_link;
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
        g_threaded_group = m_old_threaded_group;
        g_in_ng = m_old_in_ng;
        g_use_threads = m_old_use_threads;
        g_thread_always = m_old_thread_always;
        g_unsafe_rc_saves = m_old_unsafe_rc_saves;
        g_use_news_selector = m_old_use_news_selector;
        g_selected_only = m_old_selected_only;
        g_selected_count = m_old_selected_count;
        g_keep_the_group_static = m_old_keep_the_group_static;
        g_sa_initialized = m_old_sa_initialized;
        g_sc_initialized = m_old_sc_initialized;
        g_univ_ng_virt_flag = m_old_univ_ng_virt_flag;
        g_univ_read_virt_flag = m_old_univ_read_virt_flag;
        g_univ_default_cmd = m_old_univ_default_cmd;
        g_univ_follow = m_old_univ_follow;
        g_univ_follow_temp = m_old_univ_follow_temp;
        g_s_follow_temp = m_old_s_follow_temp;
        g_parsed_art = m_old_parsed_art;
        g_head_buf = std::move(m_old_head_buf);
        g_in_header = m_old_in_header;
        g_art_pos = m_old_art_pos;
        g_art_line_num = m_old_art_line_num;
        g_art_fp = m_old_art_fp;
        g_open_art = m_old_open_art;
        g_raw_art_size = m_old_raw_art_size;
        g_art_size = m_old_art_size;
        g_our_pid = m_old_our_pid;
        g_tmp_dir = m_old_tmp_dir;
        std::copy(m_old_header_type.begin(), m_old_header_type.end(), std::begin(g_header_type));
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
        add_newsgroup(name, numbers, ArticleNum{1}, ArticleNum{3});
    }

    void add_newsgroup(std::string_view name, std::string_view numbers, ArticleNum low, ArticleNum high)
    {
        const std::string active_line =
            std::string{name} + " " + std::to_string(high.value_of()) + " " + std::to_string(low.value_of()) + " y\n";
        (void) m_data_source.m_act_sf.append(active_line, static_cast<int>(name.size()));

        NewsgroupData &group = g_newsgroup_data.emplace_back();
        group = {};
        group.m_rc = &m_newsrc;
        group.m_rc_line = name;
        group.m_rc_line += ": ";
        group.m_rc_line += numbers;
        group.m_num_offset = static_cast<int>(name.size()) + 1;
        group.m_subscribe_char = ':';
        group.m_abs_first = low;
        group.m_ng_max = high;
        group.m_to_read = TR_NONE;
        group.hide_subscribe_char();
        append_newsgroup_order(&group);
        g_newsgroup_count = NewsgroupNum{static_cast<long>(g_newsgroup_data.size())};

        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&group);
        data.dat_len = static_cast<unsigned>(group.rc_name().size());
        hash_store(g_newsrc_hash, group.rc_name(), data);
    }

    static std::vector<std::string> test_article_header()
    {
        return {
            "Path: " TRN_TEST_HEADER_PATH,
            "From: " TRN_TEST_HEADER_FROM,
            "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS,
            "Followup-To: " TRN_TEST_HEADER_FOLLOWUP_TO,
            "Article: " + std::to_string(TRN_TEST_ARTICLE_NUM),
            "Subject: " TRN_TEST_HEADER_SUBJECT,
            "Reply-To: " TRN_TEST_HEADER_REPLY_TO,
            "Date: " TRN_TEST_HEADER_DATE,
            "Message-Id: " TRN_TEST_HEADER_MESSAGE_ID,
            "References: " TRN_TEST_HEADER_REFERENCES,
            "X-Boogie-Nights: " TRN_TEST_HEADER_X_BOOGIE_NIGHTS,
            "Distribution: " TRN_TEST_HEADER_DISTRIBUTION,
            "Bytes: " TRN_TEST_HEADER_BYTES,
            "Lines: " TRN_TEST_HEADER_LINES,
        };
    }

    static std::string test_article_read_numbers()
    {
        return "1-" + std::to_string(TRN_TEST_ARTICLE_NUM - 1);
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

    DataSource                        m_data_source{};
    Newsrc                            m_newsrc{};
    Multirc                           m_multirc{};
    fs::path                          m_output_dir;
    std::vector<NewsgroupData>        m_old_newsgroup_data;
    std::vector<NewsgroupData *>      m_old_newsgroup_order;
    NewsgroupNum                      m_old_newsgroup_count{};
    NewsgroupNum                      m_old_newsgroup_to_read{};
    ArticleUnread                     m_old_newsgroup_min_to_read{};
    ArticleNum                        m_old_abs_first{};
    ArticleNum                        m_old_first_art{};
    ArticleNum                        m_old_last_art{};
    NewsgroupData                    *m_old_first_newsgroup{};
    NewsgroupData                    *m_old_last_newsgroup{};
    NewsgroupData                    *m_old_newsgroup_ptr{};
    NewsgroupData                    *m_old_current_newsgroup{};
    std::map<ArticleNum, Article>     m_old_article_list;
    std::vector<Article *>            m_old_art_ptr_list;
    Article                         **m_old_art_ptr{};
    ArticleNum                        m_old_first_cached{};
    ArticleNum                        m_old_last_cached{};
    bool                              m_old_cached_all_in_range{};
    ArticleNum                        m_old_art{};
    ArticleNum                        m_old_recent_art{};
    ArticleNum                        m_old_curr_art{};
    Article                          *m_old_artp{};
    Article                          *m_old_recent_artp{};
    Article                          *m_old_curr_artp{};
    HashTable                        *m_old_newsrc_hash{};
    Multirc                          *m_old_multirc{};
    DataSource                       *m_old_data_source{};
    NNTPLink                          m_old_nntp_link{};
    std::string                       m_old_newsgroup_name;
    std::string                       m_old_newsgroup_dir;
    std::string                       m_old_tmp_dir;
    std::string                       m_old_default_cmd;
    AddNewType                        m_old_add_new_by_default{};
    bool                              m_old_fuzzy_get{};
    bool                              m_old_novice_delays{};
    bool                              m_old_verbose{};
    bool                              m_old_verify{};
    GeneralMode                       m_old_general_mode{};
    MinorMode                         m_old_mode{};
    bool                              m_old_to_read_quiet{};
    char                              m_old_int_count{};
    bool                              m_old_threaded_group{};
    bool                              m_old_in_ng{};
    bool                              m_old_use_threads{};
    bool                              m_old_thread_always{};
    bool                              m_old_unsafe_rc_saves{};
    int                               m_old_use_news_selector{};
    bool                              m_old_selected_only{};
    ArticleUnread                     m_old_selected_count{};
    int                               m_old_keep_the_group_static{};
    bool                              m_old_sa_initialized{};
    bool                              m_old_sc_initialized{};
    bool                              m_old_univ_ng_virt_flag{};
    bool                              m_old_univ_read_virt_flag{};
    bool                              m_old_univ_default_cmd{};
    bool                              m_old_univ_follow{};
    bool                              m_old_univ_follow_temp{};
    bool                              m_old_s_follow_temp{};
    ArticleNum                        m_old_parsed_art{};
    std::string                       m_old_head_buf;
    HeaderLineType                    m_old_in_header{};
    ArticlePosition                   m_old_art_pos{};
    ArticleLine                       m_old_art_line_num{};
    std::FILE                        *m_old_art_fp{};
    ArticleNum                        m_old_open_art{};
    ArticlePosition                   m_old_raw_art_size{};
    ArticlePosition                   m_old_art_size{};
    long                              m_old_our_pid{};
    fs::path                          m_old_current_path;
    std::array<HeaderType, HEAD_LAST> m_old_header_type;
    int                               m_old_errno{};
    char                              m_old_erase_char{};
    char                              m_old_kill_char{};
    bool                              m_old_erase_screen{};
    int                               m_old_term_line{};
    int                               m_old_term_col{};
    int                               m_old_tc_lines{};
    int                               m_old_tc_cols{};
    std::string_view                  m_old_tc_bc;
    std::string_view                  m_old_tc_up;
    std::string_view                  m_old_tc_cr;
    std::string_view                  m_old_tc_vb;
    std::string_view                  m_old_tc_ce;
    std::string_view                  m_old_tc_cm;
    std::string_view                  m_old_tc_ho;
    std::string_view                  m_old_tc_il;
    std::string_view                  m_old_tc_cd;
    std::string_view                  m_old_tc_so;
    std::string_view                  m_old_tc_se;
    std::string_view                  m_old_tc_us;
    std::string_view                  m_old_tc_ue;
    std::string_view                  m_old_tc_uc;
    bool                              m_old_tc_am{};
    char                              m_empty_tc[1]{};
};

class InputNewsgroupNavigationTest : public InputNewsgroupTest
{
protected:
    void                 SetUp() override;
    void                 TearDown() override;
    void                 prepare_groups();
    InputNewsgroupResult run_command(std::string_view command);

    std::string m_output;

private:
    NewsgroupData *m_old_recent{g_recent_newsgroup};
};

void InputNewsgroupNavigationTest::SetUp()
{
    InputNewsgroupTest::SetUp();
    g_recent_newsgroup = nullptr;
}

void InputNewsgroupNavigationTest::TearDown()
{
    InputNewsgroupTest::TearDown();
    g_recent_newsgroup = m_old_recent;
}

void InputNewsgroupNavigationTest::prepare_groups()
{
    add_newsgroup("comp.alpha", "1-2");
    add_newsgroup("comp.bravo", "1-2");
    add_newsgroup("comp.charlie", "1-2");
    set_current_newsgroup(1);
}

InputNewsgroupResult InputNewsgroupNavigationTest::run_command(std::string_view command)
{
    push_command(command);
    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

using NewsgroupNavigationCommand = std::pair<char, InputNewsgroupResult>;

class InputNewsgroupForwardTest : public InputNewsgroupNavigationTest,
                                  public testing::WithParamInterface<NewsgroupNavigationCommand>
{
};

class InputNewsgroupBackwardTest : public InputNewsgroupNavigationTest,
                                   public testing::WithParamInterface<NewsgroupNavigationCommand>
{
};

class InputNewsgroupFirstTest : public InputNewsgroupNavigationTest,
                                public testing::WithParamInterface<NewsgroupNavigationCommand>
{
};

class InputNewsgroupPositionTest : public InputNewsgroupNavigationTest, public testing::WithParamInterface<bool>
{
};

class InputNewsgroupLookupTest : public InputNewsgroupNavigationTest
{
protected:
    void SetUp() override;
    void start_at_end();
};

void InputNewsgroupLookupTest::SetUp()
{
    InputNewsgroupNavigationTest::SetUp();
    prepare_groups();
    m_newsrc.flags |= RF_ADD_GROUPS;
}

void InputNewsgroupLookupTest::start_at_end()
{
    g_newsgroup_ptr = nullptr;
    g_current_newsgroup = nullptr;
}

using NumericNewsgroupTarget = std::pair<std::string_view, std::size_t>;

class InputNewsgroupNumericLookupTest : public InputNewsgroupLookupTest,
                                        public testing::WithParamInterface<NumericNewsgroupTarget>
{
};

class InputNewsgroupInvalidNumberTest : public InputNewsgroupLookupTest,
                                        public testing::WithParamInterface<std::string_view>
{
};

using NewsgroupNameFailure = std::pair<std::string_view, std::string_view>;

class InputNewsgroupNameFailureTest : public InputNewsgroupLookupTest,
                                      public testing::WithParamInterface<std::tuple<NewsgroupNameFailure, bool>>
{
};

class InputNewsgroupEmptyTargetTest : public InputNewsgroupLookupTest,
                                      public testing::WithParamInterface<std::tuple<std::string_view, bool>>
{
};

class InputNewsgroupSubscriptionLookupTest : public InputNewsgroupLookupTest, public testing::WithParamInterface<bool>
{
};

class InputNewsgroupLookupCancelTest : public InputNewsgroupLookupTest, public testing::WithParamInterface<char>
{
};

class InputNewsgroupSearchTest : public InputNewsgroupNavigationTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_search_origin(bool at_end);

private:
    AddGroupTable              m_old_add_groups;
    bool                       m_old_sel_rereading{g_sel_rereading};
    int                        m_old_perform_count{g_perform_count};
    bool                       m_old_error_occurred{g_error_occurred};
    std::optional<std::string> m_old_subj_line{g_subj_line};
    int                        m_old_page_line{g_page_line};
    bool                       m_old_performed_article_loop{g_performed_article_loop};
    int                        m_old_spin_count{g_spin_count};
};

void InputNewsgroupSearchTest::SetUp()
{
    InputNewsgroupNavigationTest::SetUp();
    prepare_groups();
    m_old_add_groups = std::move(g_add_groups);
    g_add_groups = {};
    g_sel_rereading = false;
    search_init();
    newsgroup_search_init();
}

void InputNewsgroupSearchTest::TearDown()
{
    newsgroup_search_init();
    g_add_groups = std::move(m_old_add_groups);
    g_sel_rereading = m_old_sel_rereading;
    g_perform_count = m_old_perform_count;
    g_error_occurred = m_old_error_occurred;
    g_subj_line = m_old_subj_line;
    g_page_line = m_old_page_line;
    g_performed_article_loop = m_old_performed_article_loop;
    g_spin_count = m_old_spin_count;
    InputNewsgroupNavigationTest::TearDown();
}

void InputNewsgroupSearchTest::set_search_origin(bool at_end)
{
    if (at_end)
    {
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
    }
}

class InputNewsgroupSearchMissTest : public InputNewsgroupSearchTest,
                                     public testing::WithParamInterface<std::tuple<char, bool, bool>>
{
};

using NewsgroupSearchError = std::pair<std::string_view, std::string_view>;

class InputNewsgroupSearchErrorTest : public InputNewsgroupSearchTest,
                                      public testing::WithParamInterface<std::tuple<char, bool, NewsgroupSearchError>>
{
};

class InputNewsgroupSearchCancelTest : public InputNewsgroupSearchTest,
                                       public testing::WithParamInterface<std::tuple<char, bool, std::string_view>>
{
};

class InputNewsgroupSearchDirectionTest : public InputNewsgroupSearchTest,
                                          public testing::WithParamInterface<std::tuple<char, bool>>
{
};

class InputNewsgroupSearchWrapTest : public InputNewsgroupSearchTest, public testing::WithParamInterface<char>
{
};

class LocalSpoolArticleTest : public InputNewsgroupTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        open_group(bool cache_all = true);

private:
    Subject           *m_old_first_subject{g_first_subject};
    Subject           *m_old_last_subject{g_last_subject};
    HashTable         *m_old_msg_id_hash{g_msg_id_hash};
    int                m_old_subject_count{g_subject_count};
    ArticleNum         m_old_obj_count{g_obj_count};
    SelectionMode      m_old_sel_mode{g_sel_mode};
    SelectionSortMode  m_old_sel_sort{g_sel_sort};
    SelectionSortMode  m_old_sel_art_sort{g_sel_art_sort};
    std::string_view   m_old_sel_mode_string{g_sel_mode_string};
    std::string_view   m_old_sel_sort_string{g_sel_sort_string};
    int                m_old_sel_direction{g_sel_direction};
    AddGroupFlags      m_old_sel_mask{g_sel_mask};
    bool               m_old_sel_rereading{g_sel_rereading};
    bool               m_old_sel_exclusive{g_sel_exclusive};
    int                m_old_selected_subj_cnt{g_selected_subj_cnt};
    int                m_old_added_articles{g_added_articles};
    KillFileStateFlags m_old_kf_state{g_kf_state};
    bool               m_old_reread{g_reread};
    bool               m_old_force_last{g_force_last};
    bool               m_old_do_fseek{g_do_fseek};
    ArticleLine        m_old_top_line{g_top_line};
    int                m_old_page_line{g_page_line};
    bool               m_old_erase_each_line{g_erase_each_line};
    bool               m_old_s_default_cmd{g_s_default_cmd};
};

void LocalSpoolArticleTest::SetUp()
{
    InputNewsgroupTest::SetUp();
    m_data_source.m_flags = DF_NONE;
    g_first_subject = g_last_subject = nullptr;
    g_msg_id_hash = nullptr;
    g_subject_count = 0;
    g_obj_count = ArticleNum{};
    g_sel_mode = SM_ARTICLE;
    g_sel_art_sort = SS_NATURAL;
    g_sel_mask = AGF_SEL;
    g_sel_rereading = g_sel_exclusive = false;
    g_selected_subj_cnt = 0;
    g_kf_state = KFS_NONE;
    g_in_ng = true;
    g_mode = MM_ARTICLE;
    g_reread = g_force_last = g_do_fseek = false;
    g_erase_each_line = false;
    g_art_line_num = ArticleLine{6};
    g_top_line = ArticleLine{};
    g_tc_LINES = 24;
    g_tc_COLS = 80;
    g_tc_SO = "<so>";
    g_tc_SE = "<se>";
    fs::current_path(m_output_dir);

    std::ofstream{"1"} << "From: Root <root@example.test>\n"
                          "Subject: Topic\n"
                          "Date: 21 Oct 2026 10:00:00 GMT\n"
                          "Message-ID: <root@example.test>\n\nRoot first line\nRoot next line\n";
    std::ofstream{"2"} << "From: Reply <reply@example.test>\n"
                          "Subject: Re: Topic\n"
                          "Date: 21 Oct 2026 11:00:00 GMT\n"
                          "Message-ID: <reply@example.test>\n"
                          "References: <root@example.test>\n\nReply body\n";
    std::ofstream{"3"} << "From: Branch <branch@example.test>\n"
                          "Subject: Branch\n"
                          "Date: 21 Oct 2026 12:00:00 GMT\n"
                          "Message-ID: <branch@example.test>\n"
                          "References: <root@example.test>\n\nBranch body\n";
}

void LocalSpoolArticleTest::TearDown()
{
    close_cache();
    hash_destroy(g_msg_id_hash);
    g_msg_id_hash = m_old_msg_id_hash;
    g_first_subject = m_old_first_subject;
    g_last_subject = m_old_last_subject;
    g_subject_count = m_old_subject_count;
    g_obj_count = m_old_obj_count;
    g_sel_mode = m_old_sel_mode;
    g_sel_sort = m_old_sel_sort;
    g_sel_art_sort = m_old_sel_art_sort;
    g_sel_mode_string = m_old_sel_mode_string;
    g_sel_sort_string = m_old_sel_sort_string;
    g_sel_direction = m_old_sel_direction;
    g_sel_mask = m_old_sel_mask;
    g_sel_rereading = m_old_sel_rereading;
    g_sel_exclusive = m_old_sel_exclusive;
    g_selected_subj_cnt = m_old_selected_subj_cnt;
    g_added_articles = m_old_added_articles;
    g_kf_state = m_old_kf_state;
    g_reread = m_old_reread;
    g_force_last = m_old_force_last;
    g_do_fseek = m_old_do_fseek;
    g_top_line = m_old_top_line;
    g_page_line = m_old_page_line;
    g_erase_each_line = m_old_erase_each_line;
    g_s_default_cmd = m_old_s_default_cmd;
    InputNewsgroupTest::TearDown();
}

void LocalSpoolArticleTest::open_group(bool cache_all)
{
    add_newsgroup("comp.lang.cpp", "", ArticleNum{1}, ArticleNum{3});
    set_current_newsgroup(0);
    g_last_art = ArticleNum{3};
    g_thread_always = cache_all;
    build_cache();
    g_art = g_curr_art = ArticleNum{1};
    g_artp = g_curr_artp = article_ptr(g_art);
    g_recent_art = ArticleNum{2};
    g_recent_artp = article_ptr(g_recent_art);
    ASSERT_TRUE(parse_header(g_curr_art));
    ASSERT_NE(nullptr, art_open(g_curr_art, g_header_type[PAST_HEADER].min_pos));
    std::string line;
    ASSERT_TRUE(read_art(line));
    ASSERT_EQ("Root first line\n", line);
    g_art_pos = tell_art();
    ASSERT_FALSE(g_threaded_group);
    ASSERT_EQ(AF_NONE, g_curr_artp->m_flags & AF_THREADED);
}

class InputNewsgroupReadingTest : public LocalSpoolArticleTest
{
protected:
    void                 SetUp() override;
    void                 TearDown() override;
    InputNewsgroupResult run_command(std::string_view command);

    std::string m_output;

private:
    bool                               m_old_doing_ng{g_doing_ng};
    DoNewsgroupResult                  m_old_exit_code{g_newsgroup_exit_code};
    std::string                        m_old_sel_chars{g_sel_chars};
    SelectionMode                      m_old_default_mode{g_sel_default_mode};
    ArticleLine                        m_old_init_lines{g_init_lines};
    Article                          **m_old_page_app{g_sel_page_app};
    Article                          **m_old_next_app{g_sel_next_app};
    Article                           *m_old_last_ap{g_sel_last_ap};
    Subject                           *m_old_page_sp{g_sel_page_sp};
    Subject                           *m_old_next_sp{g_sel_next_sp};
    Subject                           *m_old_last_sp{g_sel_last_sp};
    int                                m_old_total{g_sel_total_obj_cnt};
    int                                m_old_prior{g_sel_prior_obj_cnt};
    int                                m_old_page_objects{g_sel_page_obj_cnt};
    int                                m_old_page_items{g_sel_page_item_cnt};
    int                                m_old_item_index{g_sel_item_index};
    int                                m_old_last_line{g_sel_last_line};
    bool                               m_old_at_end{g_sel_at_end};
    std::array<SelectionItem, MAX_SEL> m_old_items;
};

void InputNewsgroupReadingTest::SetUp()
{
    std::copy(std::begin(g_sel_items), std::end(g_sel_items), m_old_items.begin());
    LocalSpoolArticleTest::SetUp();
    const fs::path group_dir = m_output_dir / "comp/lang/cpp";
    fs::create_directories(group_dir);
    for (const char *name : {"1", "2", "3"})
    {
        fs::copy_file(m_output_dir / name, group_dir / name);
    }
    m_data_source.m_spool_dir = m_output_dir.generic_string();
    add_newsgroup("comp.previous", "1-2");
    add_newsgroup("comp.lang.cpp", "1", ArticleNum{1}, ArticleNum{3});
    add_newsgroup("comp.next", "1-2");
    const fs::path active_file = m_output_dir / "active";
    std::ofstream{active_file, std::ios::binary} << "comp.previous 3 1 y\ncomp.lang.cpp 3 1 y\ncomp.next 3 1 y\n";
    m_data_source.m_act_sf.close();
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_file, "", ""));
    set_current_newsgroup(1);
    g_in_ng = false;
    g_mode = MM_NEWSGROUP_LIST;
    g_sel_chars = "abcdefghijklmnopqrstuvwxyz";
    g_sel_default_mode = SM_ARTICLE;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_sel_page_app = g_sel_next_app = nullptr;
    g_sel_last_ap = nullptr;
    g_sel_page_sp = g_sel_next_sp = g_sel_last_sp = nullptr;
}

void InputNewsgroupReadingTest::TearDown()
{
    LocalSpoolArticleTest::TearDown();
    g_doing_ng = m_old_doing_ng;
    g_newsgroup_exit_code = m_old_exit_code;
    g_sel_chars = m_old_sel_chars;
    g_sel_default_mode = m_old_default_mode;
    g_init_lines = m_old_init_lines;
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
    g_sel_item_index = m_old_item_index;
    g_sel_last_line = m_old_last_line;
    g_sel_at_end = m_old_at_end;
    std::copy(m_old_items.begin(), m_old_items.end(), std::begin(g_sel_items));
}

InputNewsgroupResult InputNewsgroupReadingTest::run_command(std::string_view command)
{
    push_command(command);
    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    m_output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_FALSE(macro_pending());
    return result;
}

class InputNewsgroupNormalEntryTest : public InputNewsgroupReadingTest, public testing::WithParamInterface<char>
{
};

class InputNewsgroupDefaultEntryTest : public InputNewsgroupReadingTest, public testing::WithParamInterface<char>
{
};

class InputNewsgroupSelectorExitTest
    : public InputNewsgroupReadingTest,
      public testing::WithParamInterface<std::tuple<char, InputNewsgroupResult, std::size_t>>
{
};

class InputNewsgroupExplicitArticleTest
    : public InputNewsgroupReadingTest,
      public testing::WithParamInterface<std::tuple<char, std::string_view, ArticleUnread>>
{
};

class InputNewsgroupStartingCommandCancelTest : public InputNewsgroupReadingTest,
                                                public testing::WithParamInterface<std::string_view>
{
};

class InputNewsgroupNoReadingGroupTest : public InputNewsgroupNavigationTest, public testing::WithParamInterface<char>
{
};

class OnDemandThreadArticleTest : public LocalSpoolArticleTest
{
protected:
    std::string display_tree();
};

std::string OnDemandThreadArticleTest::display_tree()
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticlePosition                  position = g_art_pos;
    const ArticlePosition                  file_position = tell_art();
    const ArticlePosition                  header_end = g_header_type[PAST_HEADER].min_pos;
    const std::string                      header = g_head_buf;
    const ArticleUnread                    unread = g_newsgroup_ptr->m_to_read;
    const ArticleUnread                    selected = g_selected_count;
    const bool                             selected_only = g_selected_only;
    push_char('q');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("t");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_TRUE(g_threaded_group);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(current, std::make_pair(g_art, g_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_only, g_selected_only);
    EXPECT_EQ(position, g_art_pos);
    EXPECT_EQ(g_curr_art, g_open_art);
    EXPECT_EQ(file_position, tell_art());
    EXPECT_EQ(g_curr_art, g_parsed_art);
    EXPECT_EQ(header_end, g_header_type[PAST_HEADER].min_pos);
    EXPECT_EQ(header, g_head_buf);
    EXPECT_EQ(ArticleLine{6}, g_art_line_num);
    EXPECT_EQ(ArticleLine{}, g_top_line);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(0, g_int_count);
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
    std::string next_line;
    EXPECT_TRUE(read_art(next_line));
    EXPECT_EQ("Root next line\n", next_line);
    return output;
}

class SelectedOnDemandThreadArticleTest : public OnDemandThreadArticleTest, public testing::WithParamInterface<bool>
{
};

class SaveArticleTest : public LocalSpoolArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string save(char command, std::string_view input, ArticleSwitchResult expected = AS_ASK,
                     bool next_command = true);
    std::string saved_text() const;
    std::string expected_text(char command, bool mailbox = false) const;
    void        expect_mailbox(char command, std::string_view prefix = {});

    fs::path m_destination;
    fs::path m_spool;
    trn::testing::MockEnvironment m_env;

    static constexpr std::string_view HEADER = "From: Root <root@example.test>\n"
                                               "Subject: Topic\n"
                                               "Date: 21 Oct 2026 10:00:00 GMT\n"
                                               "Message-ID: <root@example.test>\n\n";
    static constexpr std::string_view BODY = "Root first line\nRoot next line\nFrom a body line\nLast line\n";

private:
    std::string                   m_old_priv_dir{g_priv_dir};
    std::string                   m_old_save_dest{g_save_dest};
    ArticlePosition               m_old_save_from{g_save_from};
    bool                          m_old_norm_always{g_norm_always};
    bool                          m_old_mbox_always{g_mbox_always};
    MimeSection                   m_old_mime_article{std::move(g_mime_article)};
    MimeSection                  *m_old_mime_section{g_mime_section};
    MimeState                     m_old_mime_state{g_mime_state};
    bool                          m_old_is_mime{g_is_mime};
    bool                          m_old_multimedia_mime{g_multimedia_mime};
    std::string                   m_old_msg{g_msg};
};

void SaveArticleTest::SetUp()
{
    EXPECT_CALL(m_env.getter, Call(testing::_)).Times(testing::AnyNumber()).WillRepeatedly(testing::Return(nullptr));
    LocalSpoolArticleTest::SetUp();
    m_spool = m_output_dir / "spool" / "comp" / "lang" / "cpp";
    fs::create_directories(m_spool);
    for (const char *number : {"1", "2", "3"})
    {
        fs::rename(m_output_dir / number, m_spool / number);
    }
    std::ofstream{m_spool / "1"} << HEADER << BODY;
    fs::current_path(m_spool);
    m_data_source.m_spool_dir = (m_output_dir / "spool").generic_string();
    g_priv_dir = (m_output_dir / "saved").generic_string();
    fs::create_directories(g_priv_dir);
    m_destination = fs::path{g_priv_dir} / "saved article";
    m_env.expect_env_repeatedly("SAVEDIR", g_priv_dir.c_str());
    m_env.expect_env_repeatedly("SAVENAME", "%a");
    m_env.expect_no_envars({"NORMSAVER", "MBOXSAVER"});
    g_norm_always = true;
    g_mbox_always = false;
    g_mime_article = {};
    g_mime_section = &g_mime_article;
    open_group(false);
    g_curr_artp->select_article(AUTO_KILL_NONE);
    g_selected_only = true;
    ASSERT_EQ(0, std::fseek(g_art_fp, 0, SEEK_END));
    g_art_pos = tell_art();
}

void SaveArticleTest::TearDown()
{
    g_mime_article.mime_clear_struct();
    g_mime_article = std::move(m_old_mime_article);
    g_mime_section = m_old_mime_section;
    g_mime_state = m_old_mime_state;
    g_is_mime = m_old_is_mime;
    g_multimedia_mime = m_old_multimedia_mime;
    g_priv_dir = m_old_priv_dir;
    g_save_dest = m_old_save_dest;
    g_save_from = m_old_save_from;
    g_norm_always = m_old_norm_always;
    g_mbox_always = m_old_mbox_always;
    g_msg = m_old_msg;
    LocalSpoolArticleTest::TearDown();
}

std::string SaveArticleTest::save(char command, std::string_view input, ArticleSwitchResult expected, bool next_command)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticlePosition                  position = g_art_pos;
    const ArticleLine                      line = g_art_line_num;
    const ArticleLine                      top = g_top_line;
    const ArticleNum                       first = g_first_art;
    const SubjectFlags                     subject_flags = g_curr_artp->m_subj->m_flags;
    const ArticleUnread                    unread = g_newsgroup_ptr->m_to_read;
    const ArticleUnread                    selected = g_selected_count;
    const int                              selected_subjects = g_selected_subj_cnt;
    std::vector<ArticleFlags>              flags;
    for (long number = 1; number <= 3; ++number)
    {
        flags.push_back(article_ptr(ArticleNum{number})->m_flags);
    }
    if (next_command)
    {
        push_char('n');
    }
    push_command(input);
    testing::internal::CaptureStdout();

    ArticleSwitchResult result = AS_NORM;
    EXPECT_NO_THROW(result = art_switch(std::string{command, FINISH_CMD}));

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_EQ(current, std::make_pair(g_curr_art, g_curr_artp));
    EXPECT_EQ(current, std::make_pair(g_art, g_artp));
    EXPECT_EQ(recent, std::make_pair(g_recent_art, g_recent_artp));
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(position, g_art_pos);
    EXPECT_EQ(line, g_art_line_num);
    EXPECT_EQ(top, g_top_line);
    EXPECT_EQ(g_curr_art, g_parsed_art);
    EXPECT_EQ(g_curr_art, g_open_art);
    EXPECT_EQ(subject_flags, g_curr_artp->m_subj->m_flags);
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_TRUE(g_selected_only);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(m_spool, fs::current_path());
    EXPECT_EQ(0, g_int_count);
    for (long number = 1; number <= 3; ++number)
    {
        EXPECT_EQ(flags[number - 1], article_ptr(ArticleNum{number})->m_flags);
    }
    EXPECT_EQ(next_command, macro_pending());
    if (next_command)
    {
        EXPECT_EQ('n', read_tty_char());
    }
    EXPECT_FALSE(macro_pending());
    return output;
}

class ScoreArticleTest : public SaveArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string score(char command, std::string_view input, bool next_command = true);
    std::string score_file(std::string_view name = "comp.lang.cpp") const;
    void        expect_scores(std::array<int, 3> expected) const;

    fs::path                          m_scores;
    static constexpr std::string_view GLOBAL_RULES = "savescores off\n10 subject: topic\n";

private:
    bool       m_old_sc_delay{g_sc_delay};
    bool       m_old_sc_scoring{g_sc_scoring};
    bool       m_old_sc_saves_cores{g_sc_saves_cores};
    ArticleNum m_old_sc_fill_max{g_sc_fill_max};
    bool       m_old_sc_fill_read{g_sc_fill_read};
    int        m_old_sc_loaded_count{g_sc_loaded_count};
    bool       m_old_sf_verbose{g_sf_verbose};
    int        m_old_sf_score_verbose{g_sf_score_verbose};
    bool       m_old_kill_thresh_active{g_kill_thresh_active};
    int        m_old_kill_thresh{g_kill_thresh};
    bool       m_old_s_order_changed{g_s_order_changed};
};

void ScoreArticleTest::SetUp()
{
    SaveArticleTest::SetUp();
    const ArticlePosition position = g_art_pos;
    m_scores = m_output_dir / "scores";
    fs::create_directories(m_scores);
    std::ofstream{m_scores / "global"} << GLOBAL_RULES;
    m_env.expect_env_repeatedly("SCOREDIR", m_scores.generic_string().c_str());
    m_env.expect_env_repeatedly("SAVESCOREFILE", (m_output_dir / "saved-scores").generic_string().c_str());
    g_sc_delay = false;
    g_sf_verbose = false;
    g_sf_score_verbose = 0;
    sf_file_clear();
    sc_init(false);
    ASSERT_TRUE(g_sc_initialized);
    for (long number = 1; number <= 3; ++number)
    {
        (void) sc_score_art(ArticleNum{number}, true);
    }
    ASSERT_TRUE(parse_header(g_art));
    ASSERT_NE(nullptr, art_open(g_art, position));
    g_art_pos = position;
    g_s_order_changed = false;
    expect_scores({10, 10, 0});
}

void ScoreArticleTest::TearDown()
{
    g_sc_saves_cores = false;
    sc_cleanup();
    sf_file_clear();
    g_sc_delay = m_old_sc_delay;
    g_sc_scoring = m_old_sc_scoring;
    g_sc_saves_cores = m_old_sc_saves_cores;
    g_sc_fill_max = m_old_sc_fill_max;
    g_sc_fill_read = m_old_sc_fill_read;
    g_sc_loaded_count = m_old_sc_loaded_count;
    g_sf_verbose = m_old_sf_verbose;
    g_sf_score_verbose = m_old_sf_score_verbose;
    g_kill_thresh_active = m_old_kill_thresh_active;
    g_kill_thresh = m_old_kill_thresh;
    g_s_order_changed = m_old_s_order_changed;
    SaveArticleTest::TearDown();
}

std::string ScoreArticleTest::score(char command, std::string_view input, bool next_command)
{
    const ArticlePosition position = tell_art();
    const std::string     output = save(command, input, AS_ASK, next_command);
    EXPECT_EQ(position, tell_art());
    EXPECT_TRUE(g_sc_initialized);
    return output;
}

std::string ScoreArticleTest::score_file(std::string_view name) const
{
    std::ifstream input{m_scores / name};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void ScoreArticleTest::expect_scores(std::array<int, 3> expected) const
{
    for (long number = 1; number <= 3; ++number)
    {
        EXPECT_TRUE(article_scored(ArticleNum{number}));
        EXPECT_EQ(expected[number - 1], article_ptr(ArticleNum{number})->m_score);
    }
}

struct AppendScoreCase
{
    const char *name;
    char        command;
    const char *input;
    const char *filename;
    const char *line;
};

void PrintTo(const AppendScoreCase &value, std::ostream *output)
{
    *output << value.name;
}

class AppendScoreArticleTest : public ScoreArticleTest, public testing::WithParamInterface<AppendScoreCase>
{
};

struct UnchangedScoreCase
{
    const char *name;
    char        command;
    const char *input;
    const char *message;
};

void PrintTo(const UnchangedScoreCase &value, std::ostream *output)
{
    *output << value.name;
}

class UnchangedScoreArticleTest : public ScoreArticleTest, public testing::WithParamInterface<UnchangedScoreCase>
{
};

class TemporaryScoreArticleTest : public ScoreArticleTest, public testing::WithParamInterface<const char *>
{
};

class ExecuteScoreArticleTest : public ScoreArticleTest, public testing::WithParamInterface<bool>
{
};

class DeferredScoreArticleTest : public ScoreArticleTest
{
protected:
    void SetUp() override;
};

void DeferredScoreArticleTest::SetUp()
{
    ScoreArticleTest::SetUp();
    sc_cleanup();
    g_sc_delay = true;
}

class EditScoreArticleTest : public ScoreArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string edit_score(std::string_view input);
    std::string editor_text(const char *name) const;
    void        expect_edited(const fs::path &file, std::string_view before, std::string_view argument = {}) const;

    fs::path                          m_editor_output;
    std::string                       m_editor_command;
    static constexpr std::string_view EDITED_RULE = "25 subject: topic\n";

private:
    bool m_old_waiting{g_waiting};
};

void EditScoreArticleTest::SetUp()
{
    ScoreArticleTest::SetUp();
    m_editor_output = m_output_dir / "score-editor";
    fs::create_directories(m_editor_output);
    m_editor_command =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" score-editor";
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", m_editor_output.generic_string().c_str());
    m_env.expect_no_envars({"PNEWS_TEST_MODE"});
    m_env.expect_env_repeatedly("VISUAL", m_editor_command.c_str());
    m_env.expect_env_repeatedly("EDITOR", "missing-editor");
    g_waiting = false;
}

void EditScoreArticleTest::TearDown()
{
    g_waiting = m_old_waiting;
    ScoreArticleTest::TearDown();
}

std::string EditScoreArticleTest::edit_score(std::string_view input)
{
    const std::string output = score('\'', input);
    EXPECT_FALSE(g_waiting);
    return output;
}

std::string EditScoreArticleTest::editor_text(const char *name) const
{
    std::ifstream input{m_editor_output / name};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void EditScoreArticleTest::expect_edited(const fs::path &file, std::string_view before, std::string_view argument) const
{
    EXPECT_EQ("score-editor\n" + (argument.empty() ? file.generic_string() : std::string{argument}) + '\n',
              editor_text("tools.log"));
    EXPECT_EQ(before, editor_text("score-editor-input"));
    EXPECT_EQ(m_spool.generic_string(), editor_text("score-editor-directory"));
    std::ifstream edited{file};
    ASSERT_TRUE(edited.is_open());
    EXPECT_EQ(std::string{before} + std::string{EDITED_RULE},
              (std::string{std::istreambuf_iterator<char>{edited}, std::istreambuf_iterator<char>{}}));
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_s_order_changed);
}

struct EditScoreCase
{
    const char *name;
    const char *input;
    const char *filename;
    bool        spaced_directory;
    bool        existing;
    bool        absolute;
};

void PrintTo(const EditScoreCase &value, std::ostream *output)
{
    *output << value.name;
}

class ChosenEditScoreArticleTest : public EditScoreArticleTest, public testing::WithParamInterface<EditScoreCase>
{
protected:
    void SetUp() override;

    fs::path    m_chosen_file;
    std::string m_before;
    std::string m_input;
    std::string m_argument;
};

void ChosenEditScoreArticleTest::SetUp()
{
    EditScoreArticleTest::SetUp();
    if (GetParam().spaced_directory)
    {
        m_scores = m_output_dir / "score files";
        fs::create_directories(m_scores);
        std::ofstream{m_scores / "global"} << GLOBAL_RULES;
        m_env.expect_env_repeatedly("SCOREDIR", m_scores.generic_string().c_str());
    }
    const fs::path base = GetParam().absolute                                                         ? m_output_dir
                          : std::string_view{GetParam().filename}.find('/') == std::string_view::npos ? m_scores
                                                                                                      : m_spool;
    m_chosen_file = base / GetParam().filename;
    m_argument = base == m_spool ? GetParam().filename : m_chosen_file.generic_string();
    fs::create_directories(m_chosen_file.parent_path());
    if (std::string_view{GetParam().filename} == "global")
    {
        m_before = GLOBAL_RULES;
    }
    else if (GetParam().existing)
    {
        m_before = "# Existing score rules\n";
        std::ofstream{m_chosen_file} << m_before;
    }
    sf_append("!file @ custom-score");
    m_input = GetParam().input;
    const std::size_t target = m_input.find("TARGET");
    if (target != std::string::npos)
    {
        m_input.replace(target, 6, GetParam().absolute ? m_chosen_file.generic_string() : GetParam().filename);
    }
}

class FailedEditScoreArticleTest : public EditScoreArticleTest, public testing::WithParamInterface<const char *>
{
protected:
    void SetUp() override;
};

void FailedEditScoreArticleTest::SetUp()
{
    EditScoreArticleTest::SetUp();
    std::ofstream{m_scores / "comp.lang.cpp"} << "# Keep these rules\n";
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam());
}

std::string SaveArticleTest::saved_text() const
{
    std::ifstream input{m_destination};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string SaveArticleTest::expected_text(char command, bool mailbox) const
{
    std::string text;
    if (command == 's' || command == 'S')
    {
        text = "Article: 1 of comp.lang.cpp\n";
        text += HEADER;
    }
    text += BODY;
    if (mailbox && MBOX_CHAR != '\001')
    {
        text.insert(text.find("From a body line"), ">");
    }
    text += "\n\n";
    return text;
}

void SaveArticleTest::expect_mailbox(char command, std::string_view prefix)
{
    std::string text = saved_text();
    ASSERT_EQ(prefix, std::string_view{text}.substr(0, prefix.size()));
    text.erase(0, prefix.size());
#if MBOX_CHAR == '\001'
    EXPECT_EQ("\001\001\001\001\n" + expected_text(command, true) + "\001\001\001\001\n", text);
#else
    const std::size_t end = text.find('\n');
    ASSERT_NE(std::string::npos, end);
    EXPECT_TRUE(std::regex_match(text.substr(0, end),
                                 std::regex{"From root@example[.]test [A-Z][a-z]{2} [A-Z][a-z]{2} +[0-9]{1,2} "
                                            "[0-9]{2}:[0-9]{2}:[0-9]{2}( [^ ]+)? [0-9]{4}"}))
        << text.substr(0, end);
    EXPECT_EQ(expected_text(command, true), text.substr(end + 1));
#endif
}

class PlainSaveArticleTest : public SaveArticleTest, public testing::WithParamInterface<char>
{
};

class AppendSaveArticleTest : public SaveArticleTest, public testing::WithParamInterface<char>
{
};

struct SaveFailureCase
{
    char command;
    bool mailbox;
    bool close_failure;
};

class FailedSaveArticleTest : public SaveArticleTest, public testing::WithParamInterface<SaveFailureCase>
{
protected:
    void SetUp() override;
    void TearDown() override;

    bool m_closed{};

private:
    SaveFileOperations m_old_operations{g_save_file_operations};
    std::FILE         *m_stream{};
};

void FailedSaveArticleTest::SetUp()
{
    SaveArticleTest::SetUp();
    std::ofstream destination{m_destination};
    ASSERT_TRUE(destination.is_open());
    destination.close();
    ASSERT_FALSE(destination.fail());
    g_mbox_always = GetParam().mailbox;
    g_save_file_operations.open = [this](const char *path, const char *mode)
    {
        m_stream = std::fopen(path, GetParam().close_failure ? mode : "r");
        return m_stream;
    };
    g_save_file_operations.close = [this](std::FILE *stream)
    {
        const int result = std::fclose(stream);
        m_stream = nullptr;
        m_closed = true;
        return GetParam().close_failure ? EOF : result;
    };
}

void FailedSaveArticleTest::TearDown()
{
    if (m_stream)
    {
        std::fclose(m_stream);
    }
    g_save_file_operations = std::move(m_old_operations);
    SaveArticleTest::TearDown();
}

struct ConfiguredSaverCase
{
    char command;
    bool mailbox;
};

class ExternalSaveArticleTest : public SaveArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string receiver_text(const char *filename) const;

private:
    using SignalHandler = void (*)(int);
    std::vector<std::pair<int, SignalHandler>> m_signals;
    bool                                       m_old_waiting{g_waiting};
    bool                                       m_old_no_wait_fork{g_no_wait_fork};
};

void ExternalSaveArticleTest::SetUp()
{
    SaveArticleTest::SetUp();
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
    g_waiting = g_no_wait_fork = false;
    m_env.expect_no_envars({"NNTPSERVER", "NNTP_FORCE_AUTH", "NEWSACTIVE", "NEWSDESCRIPTIONS", "QUOTECHARS"});
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", g_priv_dir.c_str());
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "");
    m_env.expect_env_repeatedly("SHELL", TRN_TEST_PROCESS_TOOL);
}

void ExternalSaveArticleTest::TearDown()
{
    for (const std::pair<int, SignalHandler> &signal : m_signals)
    {
        std::signal(signal.first, signal.second);
    }
    g_waiting = m_old_waiting;
    g_no_wait_fork = m_old_no_wait_fork;
    SaveArticleTest::TearDown();
}

std::string ExternalSaveArticleTest::receiver_text(const char *filename) const
{
    std::ifstream input{fs::path{g_priv_dir} / filename};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class ConfiguredSaveArticleTest : public ExternalSaveArticleTest,
                                  public testing::WithParamInterface<ConfiguredSaverCase>
{
protected:
    void        SetUp() override;
    std::string requested_text() const;
    void        expect_receiver() const;
};

void ConfiguredSaveArticleTest::SetUp()
{
    ExternalSaveArticleTest::SetUp();
    const std::string saver =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" article-saver \"%A\" %B \"%b\" %a %C";
    m_env.expect_env_repeatedly(GetParam().mailbox ? "MBOXSAVER" : "NORMSAVER", saver.c_str());
    g_mbox_always = GetParam().mailbox;
    g_norm_always = !GetParam().mailbox;
}

std::string ConfiguredSaveArticleTest::requested_text() const
{
    return GetParam().command == 's' || GetParam().command == 'S' ? std::string{HEADER} + std::string{BODY}
                                                                  : std::string{BODY};
}

void ConfiguredSaveArticleTest::expect_receiver() const
{
    const std::string arguments = "article-saver\n" + (m_spool / "1").generic_string() + '\n' +
                                  std::to_string(g_save_from.value_of()) + '\n' + m_destination.generic_string() +
                                  "\n1\ncomp.lang.cpp\n";
    EXPECT_EQ(arguments, receiver_text("article-saver.log"));
    const bool preferred = GetParam().command == 'S' || GetParam().command == 'W';
    EXPECT_EQ(preferred, fs::exists(fs::path{g_priv_dir} / "shell-used"));
    if (preferred)
    {
#ifdef USE_SPAWN_SHELL
        EXPECT_EQ("/c", receiver_text("shell-used"));
#else
        EXPECT_EQ("-c", receiver_text("shell-used"));
#endif
    }
}

class PipeSaveArticleTest : public ExternalSaveArticleTest
{
protected:
    void        SetUp() override;
    void        expect_receiver() const;
    std::string m_receiver;
};

void PipeSaveArticleTest::SetUp()
{
    ExternalSaveArticleTest::SetUp();
    m_env.expect_no_envars({"PIPESAVER"});
    m_receiver = "\"" TRN_TEST_PROCESS_TOOL "\" poster -h";
}

void PipeSaveArticleTest::expect_receiver() const
{
    EXPECT_EQ("poster\n-h\n", receiver_text("tools.log"));
    EXPECT_EQ(m_receiver, g_save_dest);
    EXPECT_FALSE(g_waiting);
    EXPECT_FALSE(fs::exists(m_destination));
}

class FailedPipeSaveArticleTest : public PipeSaveArticleTest, public testing::WithParamInterface<const char *>
{
};

class ExtractSaveArticleTest : public ExternalSaveArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        article(std::string_view body, std::string_view headers = {});
    std::string extracted_text(const char *filename) const;
    std::string custom_input() const;
    void        expect_custom_receiver() const;

    std::string m_article_text;
    std::string m_receiver;

    static constexpr std::string_view YENC = "Posted attachment follows\n\n"
                                             "=ybegin line=128 size=3 name=attachment.bin\n"
                                             "klm\n=yend size=3 crc32=a3830348\n";

private:
    std::string      m_old_extract_dest{g_extract_dest};
    std::string      m_old_extract_prog{g_extract_prog};
    std::string      m_old_decode_filename{g_decode_filename};
    std::string      m_old_art_line{std::move(g_art_line)};
    std::string_view m_old_mime_getc_line{g_mime_getc_line};
};

void ExtractSaveArticleTest::SetUp()
{
    ExternalSaveArticleTest::SetUp();
    m_destination = fs::path{g_priv_dir} / "attachments";
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", ".");
    m_env.expect_env_repeatedly("CUSTOMSAVER", "< \"%A\" %e");
    m_receiver = "\"" TRN_TEST_PROCESS_TOOL "\" poster -h";
    g_extract_dest.clear();
    g_extract_prog.clear();
    g_mime_getc_line = {};
}

void ExtractSaveArticleTest::TearDown()
{
    ExternalSaveArticleTest::TearDown();
    g_extract_dest = m_old_extract_dest;
    g_extract_prog = m_old_extract_prog;
    g_decode_filename = m_old_decode_filename;
    g_art_line = std::move(m_old_art_line);
    g_mime_getc_line = m_old_mime_getc_line;
}

void ExtractSaveArticleTest::article(std::string_view body, std::string_view headers)
{
    ASSERT_EQ(0, std::fclose(g_art_fp));
    g_art_fp = nullptr;
    g_open_art = {};
    g_parsed_art = {};
    m_article_text = HEADER;
    m_article_text.pop_back();
    m_article_text += headers;
    m_article_text += '\n';
    m_article_text += body;
    std::ofstream output{m_spool / "1"};
    output << m_article_text;
    output.close();
    ASSERT_FALSE(output.fail());
    ASSERT_TRUE(parse_header(g_art));
    ASSERT_NE(nullptr, art_open(g_art, ArticlePosition{}));
    ASSERT_EQ(0, std::fseek(g_art_fp, 0, SEEK_END));
    g_art_pos = tell_art();
}

std::string ExtractSaveArticleTest::extracted_text(const char *filename) const
{
    std::ifstream input{m_destination / filename};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string ExtractSaveArticleTest::custom_input() const
{
    return "attachments |   " + m_receiver + '\n';
}

void ExtractSaveArticleTest::expect_custom_receiver() const
{
    EXPECT_EQ("poster\n-h\n", extracted_text("tools.log"));
    EXPECT_EQ(m_receiver, g_extract_prog);
    EXPECT_EQ(m_destination.generic_string(), g_extract_dest);
    EXPECT_FALSE(g_waiting);
}

struct EncodedExtractCase
{
    const char *name;
    const char *headers;
    const char *body;
    const char *options;
    const char *announcement;
};

void PrintTo(const EncodedExtractCase &value, std::ostream *output)
{
    *output << value.name;
}

class EncodedExtractSaveArticleTest : public ExtractSaveArticleTest,
                                      public testing::WithParamInterface<EncodedExtractCase>
{
protected:
    void SetUp() override;
};

void EncodedExtractSaveArticleTest::SetUp()
{
    ExtractSaveArticleTest::SetUp();
    article(GetParam().body, GetParam().headers);
}

class FailedExtractSaveArticleTest : public ExtractSaveArticleTest, public testing::WithParamInterface<const char *>
{
};

class ExternalExtractTest : public ExtractSaveArticleTest
{
protected:
    void        SetUp() override;
    std::string source_bytes() const;
    std::string delivered_bytes(const char *filename) const;

    static constexpr std::string_view SHAR =
        "Posted archive follows\n\n#!/bin/sh\nprintf 'Extracted archive\\n' > 'archive output'\n";
};

void ExternalExtractTest::SetUp()
{
    ExtractSaveArticleTest::SetUp();
    m_env.expect_no_envars({"EXSAVER", "CUSTOMSAVER", "UNSHAR", "SHARSAVER"});
    const fs::path spool = m_output_dir / "spool with spaces";
    fs::copy(m_output_dir / "spool", spool, fs::copy_options::recursive);
    m_spool = spool / "comp" / "lang" / "cpp";
    m_data_source.m_spool_dir = spool.generic_string();
    fs::current_path(m_spool);
    article(BODY);
}

std::string ExternalExtractTest::source_bytes() const
{
    std::ifstream input{m_spool / "1", std::ios::binary};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::string ExternalExtractTest::delivered_bytes(const char *filename) const
{
    std::ifstream input{m_destination / filename, std::ios::binary};
    EXPECT_TRUE(input.is_open());
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

struct ExtractOverrideCase
{
    const char *name;
    const char *variable;
    bool        legacy_also_set;
    bool        shell_archive;
};

void PrintTo(const ExtractOverrideCase &value, std::ostream *output)
{
    *output << value.name;
}

class ExtractOverrideTest : public ExternalExtractTest, public testing::WithParamInterface<ExtractOverrideCase>
{
protected:
    void SetUp() override;
};

void ExtractOverrideTest::SetUp()
{
    ExternalExtractTest::SetUp();
    if (GetParam().shell_archive)
    {
        article(SHAR);
    }
    const std::string saver =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" article-saver \"%A\" %B payload %a %C";
    m_env.expect_env_repeatedly(GetParam().variable, saver.c_str());
    if (GetParam().legacy_also_set)
    {
        m_env.expect_env_repeatedly(GetParam().shell_archive ? "SHARSAVER" : "CUSTOMSAVER", "invalid-legacy-handler");
    }
}

class ViewArticleTest : public ExtractSaveArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        mailcap(std::string_view entries);
    std::string view();
    void        expect_viewed(const char *filename, std::string_view contents) const;

    fs::path                          m_parts;
    std::string                       m_viewer;
    static constexpr std::string_view MIME_HEADERS =
        "MIME-Version: 1.0\nContent-Type: application/octet-stream; name=image.bin\n"
        "Content-Transfer-Encoding: base64\n";

private:
    fs::path    m_mailcap;
    std::string m_old_tmp_dir{g_tmp_dir};
    std::string m_old_login_name{g_login_name};
};

void ViewArticleTest::SetUp()
{
    ExtractSaveArticleTest::SetUp();
    g_tmp_dir = (m_output_dir / "viewer temp").generic_string();
    g_login_name = "view-user";
#ifdef USE_WINDOWS_FILENAME_RULES
    m_parts = fs::path{g_tmp_dir} / "parts";
#else
    m_parts = fs::path{g_tmp_dir} / "m-prts-view-user";
#endif
    m_mailcap = m_output_dir / "mailcap";
    m_viewer =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" viewer \"%s\"";
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", g_priv_dir.c_str());
    m_env.expect_env_repeatedly("MIMECAPS", m_mailcap.generic_string().c_str());
    mailcap("application/octet-stream; " + m_viewer + "\nimage/jpeg; " + m_viewer + '\n');
    article("QUJD\n", MIME_HEADERS);
}

void ViewArticleTest::TearDown()
{
    mime_final();
    g_tmp_dir = m_old_tmp_dir;
    g_login_name = m_old_login_name;
    ExtractSaveArticleTest::TearDown();
}

void ViewArticleTest::mailcap(std::string_view entries)
{
    std::ofstream output{m_mailcap};
    output << entries;
    output.close();
    ASSERT_FALSE(output.fail());
    mime_init();
}

std::string ViewArticleTest::view()
{
    EXPECT_EQ(0, seek_art(g_header_type[PAST_HEADER].min_pos));
    g_art_pos = tell_art();
    g_int_count = 1;
    const std::string output = save('a', {});
    EXPECT_FALSE(g_waiting);
    if (fs::exists(m_parts))
    {
        EXPECT_TRUE(fs::is_empty(m_parts));
    }
    return output;
}

void ViewArticleTest::expect_viewed(const char *filename, std::string_view contents) const
{
    EXPECT_EQ(contents, receiver_text((std::string{"viewed-"} + filename).c_str()));
    EXPECT_FALSE(fs::exists(m_parts / filename));
}

struct ViewAttachmentCase
{
    const char *name;
    const char *headers;
    const char *body;
    const char *filename;
};

void PrintTo(const ViewAttachmentCase &value, std::ostream *output)
{
    *output << value.name;
}

class EncodedViewArticleTest : public ViewArticleTest, public testing::WithParamInterface<ViewAttachmentCase>
{
protected:
    void SetUp() override;
};

void EncodedViewArticleTest::SetUp()
{
    ViewArticleTest::SetUp();
    article(GetParam().body, GetParam().headers);
}

class FailedViewArticleTest : public ViewArticleTest, public testing::WithParamInterface<const char *>
{
};

class ResponseArticleTest : public ExtractSaveArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        answers(std::string_view input);
    void        prompt_answers(std::string_view input);
    std::string respond_with(char command);
    void        expect_sent(std::string_view draft, bool old_article = true) const;
    void        expect_retained(std::string_view draft) const;

    fs::path                          m_home;
    fs::path                          m_answers;
    static constexpr std::string_view REPLY_HEADERS = "Newsgroups: comp.lang.cpp\nReferences: <parent@example.test>\n";

private:
    std::string m_old_head_name{g_head_name};
    std::string m_old_orig_dir{g_orig_dir};
    std::string m_old_indent_string{g_indent_string};
    char        m_old_wrapped_nl{g_wrapped_nl};
    int         m_saved_stdin{-1};
};

void ResponseArticleTest::SetUp()
{
    ExtractSaveArticleTest::SetUp();
    search_init();
    m_home = fs::path{g_priv_dir} / "home";
    fs::create_directories(m_home);
    const fs::path temporary = m_output_dir / "temporary";
    fs::create_directories(temporary);
    g_orig_dir = (m_output_dir / "origin").generic_string();
    fs::create_directories(g_orig_dir);
    g_head_name = (fs::path{g_priv_dir} /
#ifdef USE_SPAWN_SHELL
                   "reply header"
#else
                   "reply-header"
#endif
                   )
                      .generic_string();
    g_indent_string = "> ";
    g_wrapped_nl = WRAPPED_NL;
    m_answers = m_output_dir / "answers";
    answers("s\n");
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", g_priv_dir.c_str());
    for (const char *name : {"HOME", "LOGDIR", "DOTDIR"})
    {
        m_env.expect_env_repeatedly(name, m_home.generic_string().c_str());
    }
    for (const char *name : {"TMP", "TMPDIR"})
    {
        m_env.expect_env_repeatedly(name, temporary.generic_string().c_str());
    }
    m_env.expect_no_envars({"MAILHEADER", "YOUSAID", "FROM", "REPLYTO"});
    m_env.expect_env_repeatedly("NEWSORG", "Test Organization");
    m_env.expect_env_repeatedly("FAST_RNMAIL", "yes");
    m_env.expect_env_repeatedly("MAILRECORD", "none");
    m_env.expect_env_repeatedly("MAILSIGNATURE", (m_home / "missing-signature").generic_string().c_str());
    const std::string tool =
#ifdef USE_SPAWN_SHELL
        "\"" TRN_TEST_PROCESS_TOOL "\"";
#else
        TRN_TEST_PROCESS_TOOL;
#endif
    m_env.expect_env_repeatedly("VISUAL", (tool + " editor").c_str());
    m_env.expect_env_repeatedly("EDITOR", "missing-editor");
    m_env.expect_env_repeatedly("PGPCOMMAND", (tool + " pgp").c_str());
    m_env.expect_env_repeatedly("RNMAILER", (tool + " mailer").c_str());
#ifdef USE_SPAWN_SHELL
    const fs::path account = m_output_dir / "account.json";
    std::ofstream{account}
        << R"({"url":"smtps://smtp.example.test:465","sender":"reader@example.test","timeoutSeconds":3})";
    m_env.expect_env_repeatedly("RNMAILER_CONFIG", account.generic_string().c_str());
    const std::string poster = "< \"" + m_answers.generic_string() + "\" call \"" TRN_TEST_RNMAIL "\" -h \"%h\" \"%A\"";
#else
    const std::string poster = "sh \"" TRN_TEST_RNMAIL "\" -h \"%h\" \"%A\" < \"" + m_answers.generic_string() + '"';
#endif
    m_env.expect_env_repeatedly("MAILPOSTER", poster.c_str());
    article(BODY, REPLY_HEADERS);
}

void ResponseArticleTest::TearDown()
{
    if (m_saved_stdin >= 0)
    {
        EXPECT_GE(dup2(m_saved_stdin, fileno(stdin)), 0);
        close(m_saved_stdin);
        std::clearerr(stdin);
    }
    g_head_name = m_old_head_name;
    g_orig_dir = m_old_orig_dir;
    g_indent_string = m_old_indent_string;
    g_wrapped_nl = m_old_wrapped_nl;
    ExtractSaveArticleTest::TearDown();
}

void ResponseArticleTest::answers(std::string_view input)
{
    std::ofstream output{m_answers};
    output << input;
    output.close();
    ASSERT_FALSE(output.fail());
}

std::string ResponseArticleTest::respond_with(char command)
{
    EXPECT_EQ(0, seek_art(g_header_type[PAST_HEADER].min_pos));
    g_art_pos = tell_art();
    const std::string output = save(command, {});
    EXPECT_FALSE(g_waiting);
    EXPECT_EQ(WRAPPED_NL, g_wrapped_nl);
    return output;
}

class ReplyArticleTest : public ResponseArticleTest
{
protected:
    std::string expected_header(std::string_view recipient = "root@example.test") const;

    static constexpr std::string_view QUOTED_BODY =
        "In article <root@example.test> you write:\n"
        "> Root first line\n> Root next line\n> From a body line\n> Last line\n\n";
};

std::string ReplyArticleTest::expected_header(std::string_view recipient) const
{
    return "To: " + std::string{recipient} +
           "\nSubject: Re: Topic\nX-Newsgroups: comp.lang.cpp\n"
           "In-Reply-To: <root@example.test>\nReferences: <parent@example.test>\n"
           "Organization: Test Organization\nCc: \nBcc: \n\n";
}

void ResponseArticleTest::expect_sent(std::string_view draft, bool old_article) const
{
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    EXPECT_EQ(std::string{draft} + '\n', receiver_text("editor-input"));
    EXPECT_EQ(g_orig_dir, receiver_text("editor-directory"));
    const std::string mailed = receiver_text("mailed");
    EXPECT_NE(std::string::npos, mailed.find("mispelled editor body\n"));
    const std::size_t body = draft.find("\n\n");
    ASSERT_NE(std::string_view::npos, body);
    EXPECT_NE(std::string::npos, mailed.find(draft.substr(body + 2)));
    const std::string log = receiver_text("tools.log");
    const std::size_t editor = log.find("editor\n");
    const std::size_t mailer = log.find("mailer\n");
    ASSERT_NE(std::string::npos, editor);
    ASSERT_NE(std::string::npos, mailer);
    EXPECT_LT(editor, mailer);
    EXPECT_EQ(old_article, log.find('\n' + (m_spool / "1").generic_string() + '\n') != std::string::npos);
    EXPECT_FALSE(fs::exists(m_home / "dead.letter"));
}

void ResponseArticleTest::expect_retained(std::string_view draft) const
{
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "mailed"));
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    const std::string edited = std::string{draft} + "\nmispelled editor body\n";
    EXPECT_EQ(edited, receiver_text("home/dead.letter"));
    EXPECT_EQ(edited, receiver_text("home/.letter"));
}

struct ReplyCommandCase
{
    const char *name;
    char        command;
    bool        reply_to;
};

void PrintTo(const ReplyCommandCase &value, std::ostream *output)
{
    *output << value.name;
}

class AddressedReplyArticleTest : public ReplyArticleTest, public testing::WithParamInterface<ReplyCommandCase>
{
protected:
    void SetUp() override;
};

void AddressedReplyArticleTest::SetUp()
{
    ReplyArticleTest::SetUp();
    if (GetParam().reply_to)
    {
        article(BODY, std::string{REPLY_HEADERS} + "Reply-To: Reply Desk <desk@example.test>\n");
    }
}

class ForwardArticleTest : public ResponseArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string forward_article();
    std::string default_draft() const;
    std::string mime_draft(std::string_view header, std::string_view boundary,
                           std::string_view introduction = "Content-Type: text/plain\n") const;
    std::string plain_draft(std::string_view introduction = "------- start of forwarded message -------",
                            std::string_view ending = "------- end of forwarded message -------") const;
    void        use_plain_header();

    static constexpr std::string_view PLAIN_HEADER = "To: desk@example.test\nSubject: Forwarded Topic\n\n";

private:
    bool m_old_force_grow{g_force_grow};
};

void ForwardArticleTest::SetUp()
{
    ResponseArticleTest::SetUp();
    m_env.expect_no_envars({"FORWARDHEADER", "FORWARDMSG", "FORWARDMSGEND"});
    const std::string poster =
#ifdef USE_SPAWN_SHELL
        "< \"" + m_answers.generic_string() + "\" call \"" TRN_TEST_RNMAIL "\" -h \"%h\"";
#else
        "sh \"" TRN_TEST_RNMAIL "\" -h \"%h\" < \"" + m_answers.generic_string() + '"';
#endif
    m_env.expect_env_repeatedly("FORWARDPOSTER", poster.c_str());
    prompt_answers("desk@example.test\n");
    g_force_grow = false;
}

void ForwardArticleTest::TearDown()
{
    g_force_grow = m_old_force_grow;
    ResponseArticleTest::TearDown();
}

std::string ForwardArticleTest::forward_article()
{
    const std::string output = respond_with(Ctl('f'));
    EXPECT_FALSE(g_force_grow);
    return output;
}

std::string ForwardArticleTest::mime_draft(std::string_view header, std::string_view boundary,
                                           std::string_view introduction) const
{
    const std::string delimiter = "--" + std::string{boundary};
    return std::string{header} + delimiter + '\n' + std::string{introduction} +
           "\n[Replace this with your comments.]\n\n" + delimiter + "\nContent-Type: message/rfc822\n\n" +
           m_article_text + '\n' + delimiter + "--\n";
}

std::string ForwardArticleTest::default_draft() const
{
    const std::string draft = receiver_text(fs::path{g_head_name}.filename().string().c_str());
    std::smatch       match;
    const bool        found = std::regex_search(draft, match, std::regex{"boundary=\"(=[0-9]+=--)\""});
    EXPECT_TRUE(found);
    if (!found)
    {
        return {};
    }
    const std::string boundary = match[1];
    EXPECT_EQ(0, boundary.find("=" + std::to_string(g_our_pid)));
    const std::string header = "To: desk@example.test\nSubject: Topic (fwd)\nX-Newsgroups: comp.lang.cpp\n"
                               "In-Reply-To: <root@example.test>\nReferences: <parent@example.test>\n"
                               "Organization: Test Organization\nMime-Version: 1.0\n"
                               "Content-Type: multipart/mixed; boundary=\"" +
                               boundary + "\"\nCc: \nBcc: \n\n";
    return mime_draft(header, boundary);
}

std::string ForwardArticleTest::plain_draft(std::string_view introduction, std::string_view ending) const
{
    return std::string{PLAIN_HEADER} + (introduction.empty() ? "" : std::string{introduction} + '\n') + m_article_text +
           (ending.empty() ? "" : std::string{ending} + '\n');
}

void ForwardArticleTest::use_plain_header()
{
    m_env.expect_env_repeatedly("FORWARDHEADER", std::string{PLAIN_HEADER}.c_str());
}

class OriginalForwardArticleTest : public ForwardArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void OriginalForwardArticleTest::SetUp()
{
    ForwardArticleTest::SetUp();
    if (GetParam())
    {
        article("--original\nContent-Type: text/plain\n\nOriginal body\n"
                "--original\nContent-Type: application/octet-stream\n"
                "Content-Transfer-Encoding: base64\nContent-Disposition: attachment; filename=image.bin\n\n"
                "QUJD\n--original--\n",
                std::string{REPLY_HEADERS} + "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=original\n");
    }
}

struct ForwardMimeHeaderCase
{
    const char *name;
    const char *content_type;
    const char *introduction;
};

void PrintTo(const ForwardMimeHeaderCase &value, std::ostream *output)
{
    *output << value.name;
}

class ConfiguredMimeForwardArticleTest : public ForwardArticleTest,
                                         public testing::WithParamInterface<ForwardMimeHeaderCase>
{
protected:
    void SetUp() override;
};

void ConfiguredMimeForwardArticleTest::SetUp()
{
    ForwardArticleTest::SetUp();
    const std::string header =
        "To: desk@example.test\nSubject: Forwarded Topic\n" + std::string{GetParam().content_type} + "\n\n";
    m_env.expect_env_repeatedly("FORWARDHEADER", header.c_str());
    m_env.expect_env_repeatedly("FORWARDMSG", GetParam().introduction);
}

struct ForwardFailureCase
{
    const char *name;
    const char *mode;
    const char *input;
    bool        mailer_invoked;
};

void PrintTo(const ForwardFailureCase &value, std::ostream *output)
{
    *output << value.name;
}

class FailedForwardArticleTest : public ForwardArticleTest, public testing::WithParamInterface<ForwardFailureCase>
{
protected:
    void SetUp() override;
};

void FailedForwardArticleTest::SetUp()
{
    ForwardArticleTest::SetUp();
    use_plain_header();
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam().mode);
    answers(GetParam().input);
}

class DisplayedForwardHeaderArticleTest : public ForwardArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void DisplayedForwardHeaderArticleTest::SetUp()
{
    ForwardArticleTest::SetUp();
    use_plain_header();
    m_env.expect_env_repeatedly("FORWARD_DRAFT", g_head_name.c_str());
    const std::string poster =
#ifdef USE_SPAWN_SHELL
        "< \"" + m_answers.generic_string() + "\" call \"" TRN_TEST_RNMAIL "\" -h \"%{FORWARD_DRAFT}\"";
#else
        "sh \"" TRN_TEST_RNMAIL "\" -h \"%{FORWARD_DRAFT}\" < \"" + m_answers.generic_string() + '"';
#endif
    m_env.expect_env_repeatedly("FORWARDPOSTER", poster.c_str());
    g_verbose = GetParam();
}

class FollowupArticleTest : public ResponseArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string follow_with(char command, std::string_view input = {});
    std::string expected_news_header(std::string_view groups = "comp.lang.cpp", bool original_groups = false) const;
    void        expect_posted(std::string_view draft) const;
    void        expect_news_retained(std::string_view draft) const;

    static constexpr std::string_view NEWS_ARTICLE_HEADERS =
        "Newsgroups: comp.lang.cpp\nReferences: <parent@example.test>\n"
        "Distribution: world\nKeywords: example\n";
    static constexpr std::string_view QUOTED_ARTICLE =
        "In article <root@example.test>, Root <root@example.test> wrote:\n"
        "> Root first line\n> Root next line\n> From a body line\n> Last line\n\n";

private:
    bool m_old_force_grow{g_force_grow};
};

void FollowupArticleTest::SetUp()
{
    ResponseArticleTest::SetUp();
    const fs::path news = m_output_dir / "news";
    fs::create_directories(news);
    std::ofstream{news / "active"} << "comp.lang.cpp 3 1 y\ncomp.test 1 1 y\n";
    std::ofstream{news / "newsgroups"} << "comp.lang.cpp C++\ncomp.test Test group\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(news / "active", "", ""));
    m_env.expect_env_repeatedly("NEWSLIB", news.generic_string().c_str());
    m_env.expect_env_repeatedly("NEWSACTIVE", (news / "active").generic_string().c_str());
    m_env.expect_env_repeatedly("NEWSDESCRIPTIONS", (news / "newsgroups").generic_string().c_str());
    m_env.expect_env_repeatedly("FAST_PNEWS", "yes");
    m_env.expect_env_repeatedly("NETSPEED", "0");
    m_env.expect_env_repeatedly("TRN_VERSION", "5.0");
    m_env.expect_env_repeatedly("AUTHORCOPY", "none");
    m_env.expect_env_repeatedly("NEWSSIGNATURE", (m_home / "missing-signature").generic_string().c_str());
    m_env.expect_no_envars({"NEWSHEADER", "ATTRIBUTION"});
#ifdef USE_SPAWN_SHELL
    m_env.expect_env_repeatedly("INEWS", "\"" TRN_TEST_PROCESS_TOOL "\" poster");
    m_env.expect_env_repeatedly("ARTCHECK", "\"" TRN_TEST_PROCESS_TOOL "\" artcheck");
    const std::string poster = "< \"" + m_answers.generic_string() + "\" call \"" TRN_TEST_PNEWS "\" -h \"%h\"";
#else
    const std::string poster = "sh \"" TRN_TEST_PNEWS "\" -h \"%h\" < \"" + m_answers.generic_string() + '"';
#endif
    m_env.expect_env_repeatedly("NEWSPOSTER", poster.c_str());
    article(BODY, NEWS_ARTICLE_HEADERS);
    g_force_grow = false;
}

void FollowupArticleTest::TearDown()
{
    g_force_grow = m_old_force_grow;
    ResponseArticleTest::TearDown();
}

void ResponseArticleTest::prompt_answers(std::string_view input)
{
    const fs::path prompt_input = m_output_dir / "prompt-input";
    std::ofstream{prompt_input} << input;
    std::FILE *fp = std::fopen(prompt_input.string().c_str(), "r");
    ASSERT_NE(nullptr, fp);
    m_saved_stdin = dup(fileno(stdin));
    EXPECT_GE(m_saved_stdin, 0);
    EXPECT_GE(dup2(fileno(fp), fileno(stdin)), 0);
    std::fclose(fp);
    std::clearerr(stdin);
}

std::string FollowupArticleTest::follow_with(char command, std::string_view input)
{
    EXPECT_EQ(0, seek_art(g_header_type[PAST_HEADER].min_pos));
    g_art_pos = tell_art();
    const std::string output = save(command, input);
    EXPECT_TRUE(g_force_grow);
    EXPECT_FALSE(g_waiting);
    EXPECT_EQ(WRAPPED_NL, g_wrapped_nl);
    return output;
}

std::string FollowupArticleTest::expected_news_header(std::string_view groups, bool original_groups) const
{
    const std::string original = original_groups ? "X-ORIGINAL-NEWSGROUPS: comp.lang.cpp\n" : "";
    return original + "Newsgroups: " + std::string{groups} +
           "\nSubject: Re: Topic\nSummary: \nExpires: \nReferences: <parent@example.test> <root@example.test>\n"
           "Sender: \nFollowup-To: \nDistribution: world\nOrganization: Test Organization\nKeywords: example\nCc: \n\n";
}

void FollowupArticleTest::expect_posted(std::string_view draft) const
{
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    EXPECT_EQ(std::string{draft} + '\n', receiver_text("editor-input"));
    EXPECT_EQ(g_orig_dir, receiver_text("editor-directory"));
    const std::string posted = receiver_text("posted");
    const std::size_t body = draft.find("\n\n");
    ASSERT_NE(std::string_view::npos, body);
    EXPECT_NE(std::string::npos, posted.find(draft.substr(body + 2)));
    EXPECT_NE(std::string::npos, posted.find("mispelled editor body\n"));
    const std::string log = receiver_text("tools.log");
    const std::size_t editor = log.find("editor\n");
    const std::size_t poster = log.find("poster\n-h\n");
    ASSERT_NE(std::string::npos, editor);
    ASSERT_NE(std::string::npos, poster);
    EXPECT_LT(editor, poster);
    EXPECT_FALSE(fs::exists(m_home / "dead.article"));
}

void FollowupArticleTest::expect_news_retained(std::string_view draft) const
{
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    const std::string edited = std::string{draft} + "\nmispelled editor body\n";
    EXPECT_EQ(edited, receiver_text("home/dead.article"));
    EXPECT_EQ(edited, receiver_text("home/.article"));
}

class ArticleRevisionTest : public FollowupArticleTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string revise(char command);
    std::string revision_header(char command, std::string_view reply_to = {}) const;
    std::string revision_draft(char command, std::string_view reply_to = {}) const;
    void        expect_revision_posted(char command, std::string_view draft) const;

private:
    std::string m_old_login_name{g_login_name};
    std::string m_old_host_name{g_host_name};
#ifdef HAS_NEWS_ADMIN
    int m_old_news_uid{g_news_uid};
#endif
};

void ArticleRevisionTest::SetUp()
{
    FollowupArticleTest::SetUp();
    g_login_name = "root";
    g_host_name = "example.test";
#ifdef HAS_NEWS_ADMIN
    g_news_uid = -1;
#endif
    m_env.expect_no_envars({"CANCELHEADER", "SUPERSEDEHEADER"});
    m_env.expect_env_repeatedly("CANCEL",
#ifdef USE_SPAWN_SHELL
                                "call "
#endif
                                "\"" TRN_TEST_PROCESS_TOOL "\" poster -h <\"%h\"");
}

void ArticleRevisionTest::TearDown()
{
    g_login_name = m_old_login_name;
    g_host_name = m_old_host_name;
#ifdef HAS_NEWS_ADMIN
    g_news_uid = m_old_news_uid;
#endif
    FollowupArticleTest::TearDown();
}

std::string ArticleRevisionTest::revise(char command)
{
    const std::string output = respond_with(command);
    EXPECT_FALSE(g_force_grow);
    std::ifstream original{m_spool / "1"};
    EXPECT_EQ(m_article_text,
              (std::string{std::istreambuf_iterator<char>{original}, std::istreambuf_iterator<char>{}}));
    return output;
}

std::string ArticleRevisionTest::revision_header(char command, std::string_view reply_to) const
{
    if (command == 'C')
    {
        return "Newsgroups: comp.lang.cpp\nSubject: cancel\nControl: cancel <root@example.test>\n"
               "Distribution: world\n\n<root@example.test> was cancelled from within trn.\n";
    }
    return "Newsgroups: comp.lang.cpp\nSubject: Topic\nSummary: \nExpires: \n"
           "References: <parent@example.test>\nFrom: Root <root@example.test>\nReply-To: " +
           std::string{reply_to} +
           "\nSupersedes: <root@example.test>\nSender: \nFollowup-To: \n"
           "Distribution: world\nOrganization: Test Organization\nKeywords: example\n\n";
}

std::string ArticleRevisionTest::revision_draft(char command, std::string_view reply_to) const
{
    return revision_header(command, reply_to) + (command == 'Z' ? std::string{BODY} : "");
}

void ArticleRevisionTest::expect_revision_posted(char command, std::string_view draft) const
{
    if (command == 'C')
    {
        EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
        EXPECT_EQ(draft, receiver_text("posted"));
        EXPECT_EQ("poster\n-h\n", receiver_text("tools.log"));
        EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "editor-input"));
        return;
    }
    expect_posted(draft);
    EXPECT_THAT(receiver_text("posted"), testing::HasSubstr("Supersedes: <root@example.test>\n"));
    EXPECT_THAT(receiver_text("posted"), testing::HasSubstr("From: Root <root@example.test>\n"));
}

class ArticleRevisionCommandTest : public ArticleRevisionTest, public testing::WithParamInterface<char>
{
};

class ArticleRevisionDisplayTest : public ArticleRevisionTest,
                                   public testing::WithParamInterface<std::tuple<char, bool>>
{
protected:
    void SetUp() override;
};

void ArticleRevisionDisplayTest::SetUp()
{
    ArticleRevisionTest::SetUp();
    g_verbose = std::get<1>(GetParam());
}

class FailedCancelArticleTest : public ArticleRevisionTest, public testing::WithParamInterface<const char *>
{
};

struct SupersedeFailureCase
{
    const char *name;
    char        command;
    const char *mode;
    const char *input;
    bool        poster_invoked;
};

void PrintTo(const SupersedeFailureCase &value, std::ostream *output)
{
    *output << value.name;
}

class FailedSupersedeArticleTest : public ArticleRevisionTest, public testing::WithParamInterface<SupersedeFailureCase>
{
protected:
    void SetUp() override;
};

void FailedSupersedeArticleTest::SetUp()
{
    ArticleRevisionTest::SetUp();
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam().mode);
    answers(GetParam().input);
}

struct FollowupCommandCase
{
    const char *name;
    char        command;
    const char *followup_groups;
};

void PrintTo(const FollowupCommandCase &value, std::ostream *output)
{
    *output << value.name;
}

class GroupedFollowupArticleTest : public FollowupArticleTest, public testing::WithParamInterface<FollowupCommandCase>
{
protected:
    void SetUp() override;
};

void GroupedFollowupArticleTest::SetUp()
{
    FollowupArticleTest::SetUp();
    if (*GetParam().followup_groups)
    {
        article(BODY, std::string{NEWS_ARTICLE_HEADERS} + "Followup-To: " + GetParam().followup_groups + '\n');
    }
}

struct EmailFollowupCase
{
    const char *name;
    const char *route;
    const char *reply_to;
    const char *recipient;
};

void PrintTo(const EmailFollowupCase &value, std::ostream *output)
{
    *output << value.name;
}

class EmailFollowupArticleTest : public FollowupArticleTest, public testing::WithParamInterface<EmailFollowupCase>
{
protected:
    void SetUp() override;
};

void EmailFollowupArticleTest::SetUp()
{
    FollowupArticleTest::SetUp();
    std::string headers = std::string{NEWS_ARTICLE_HEADERS} + "Followup-To: " + GetParam().route + '\n';
    if (*GetParam().reply_to)
    {
        headers += std::string{"Reply-To: "} + GetParam().reply_to + '\n';
    }
    article(BODY, headers);
}

class ModeratedFollowupArticleTest : public FollowupArticleTest
{
protected:
    void        SetUp() override;
    std::string moderated_draft(std::string_view groups, std::string_view recipients) const;
    void        expect_mailed(std::string_view draft) const;
    void        expect_mail_retained(std::string_view draft) const;
};

void ModeratedFollowupArticleTest::SetUp()
{
    FollowupArticleTest::SetUp();
    const fs::path active = m_output_dir / "news" / "active";
    m_data_source.m_act_sf.close();
    std::ofstream{active} << "comp.lang.cpp 3 1 m\nlist.shared 1 1 m\ncomp.test 1 1 m\nopen.test 1 1 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active, "", ""));
    m_data_source.m_moderators_file = (m_output_dir / "news" / "moderators").generic_string();
    std::ofstream{m_data_source.m_moderators_file}
        << "comp.lang.cpp,list.shared:z-moderator@example.test\ncomp.test:a-moderator@example.test\n";
    m_env.expect_env_repeatedly("NEWSHEADER", "Newsgroups: %F\nTo: obsolete@example.test\n"
                                              " obsolete-copy@example.test\nSubject: Re: %S\nReferences: %R\n\n");
}

std::string ModeratedFollowupArticleTest::moderated_draft(std::string_view groups, std::string_view recipients) const
{
    return "To: " + std::string{recipients} + "\nNewsgroups: " + std::string{groups} +
           "\nSubject: Re: Topic\nReferences: <parent@example.test> <root@example.test>\n\n" +
           std::string{QUOTED_ARTICLE};
}

void ModeratedFollowupArticleTest::expect_mailed(std::string_view draft) const
{
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    EXPECT_EQ(std::string{draft} + '\n', receiver_text("editor-input"));
    EXPECT_EQ(g_orig_dir, receiver_text("editor-directory"));
    const std::string mailed = receiver_text("mailed");
    EXPECT_THAT(mailed, testing::HasSubstr(draft.substr(0, draft.find('\n'))));
    EXPECT_THAT(mailed, testing::HasSubstr(QUOTED_ARTICLE));
    EXPECT_THAT(mailed, testing::HasSubstr("mispelled editor body\n"));
    EXPECT_THAT(mailed, testing::Not(testing::HasSubstr("obsolete")));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
    EXPECT_FALSE(fs::exists(m_home / "dead.letter"));
}

void ModeratedFollowupArticleTest::expect_mail_retained(std::string_view draft) const
{
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    const std::string edited = std::string{draft} + "\nmispelled editor body\n";
    EXPECT_EQ(edited, receiver_text("home/dead.letter"));
    EXPECT_EQ(edited, receiver_text("home/.letter"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "mailed"));
}

struct ModeratorFollowupCase
{
    const char *name;
    const char *groups;
    const char *recipients;
};

void PrintTo(const ModeratorFollowupCase &value, std::ostream *output)
{
    *output << value.name;
}

class CrosspostedFollowupArticleTest : public ModeratedFollowupArticleTest,
                                       public testing::WithParamInterface<ModeratorFollowupCase>
{
protected:
    void SetUp() override;
};

void CrosspostedFollowupArticleTest::SetUp()
{
    ModeratedFollowupArticleTest::SetUp();
    article(BODY, std::string{NEWS_ARTICLE_HEADERS} + "Followup-To: " + GetParam().groups + '\n');
}

struct FailedModeratorFollowupCase
{
    const char *name;
    const char *mode;
    const char *input;
    bool        mailer_invoked;
};

void PrintTo(const FailedModeratorFollowupCase &value, std::ostream *output)
{
    *output << value.name;
}

class FailedModeratorFollowupArticleTest : public ModeratedFollowupArticleTest,
                                           public testing::WithParamInterface<FailedModeratorFollowupCase>
{
protected:
    void SetUp() override;
};

void FailedModeratorFollowupArticleTest::SetUp()
{
    ModeratedFollowupArticleTest::SetUp();
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam().mode);
    answers(GetParam().input);
}

class LegacyFollowupArticleTest : public FollowupArticleTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_handoff(std::string_view draft) const;

private:
    std::string m_old_dot_dir{g_dot_dir};
};

void LegacyFollowupArticleTest::SetUp()
{
    FollowupArticleTest::SetUp();
    g_dot_dir = m_home.generic_string();
    const fs::path inews = fs::path{g_orig_dir} /
#ifdef USE_SPAWN_SHELL
                           "inews.exe";
    const char separator = ';';
#else
                           "inews";
    const char separator = ':';
#endif
    fs::copy_file(TRN_TEST_PROCESS_TOOL, inews);
    const char       *old_path = std::getenv("PATH");
    const std::string path = g_orig_dir + separator + (old_path ? old_path : "");
    m_env.expect_env_repeatedly("PATH", path.c_str());
    const std::string poster =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" handoff \"%h\"";
    m_env.expect_env_repeatedly("NEWSPOSTER", poster.c_str());
}

void LegacyFollowupArticleTest::TearDown()
{
    g_dot_dir = m_old_dot_dir;
    FollowupArticleTest::TearDown();
}

void LegacyFollowupArticleTest::expect_handoff(std::string_view draft) const
{
    EXPECT_EQ(draft, receiver_text("handoff-input"));
    EXPECT_EQ(g_orig_dir, receiver_text("handoff-directory"));
    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
}

class LegacyDraftPathFollowupArticleTest : public LegacyFollowupArticleTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void LegacyDraftPathFollowupArticleTest::SetUp()
{
    LegacyFollowupArticleTest::SetUp();
    g_head_name = (fs::path{g_priv_dir} / (GetParam() ? "followup draft" : "followup")).generic_string();
}

struct RemoteFollowupCase
{
    const char *name;
    const char *response;
    bool        accepted;
};

void PrintTo(const RemoteFollowupCase &value, std::ostream *output)
{
    *output << value.name;
}

class RemoteLegacyFollowupArticleTest : public LegacyFollowupArticleTest,
                                        public testing::WithParamInterface<RemoteFollowupCase>
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string m_old_server_line{g_ser_line};
    std::string m_old_last_command{g_last_command};
};

void RemoteLegacyFollowupArticleTest::SetUp()
{
    LegacyFollowupArticleTest::SetUp();
    const auto connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    g_nntp_link.connection = connection;
    g_nntp_link.flags = NNTP_NEW_CMD_OK;
    m_data_source.m_flags = DF_REMOTE;
    m_data_source.m_spool_dir = m_spool.generic_string();
    EXPECT_CALL(*connection, write_line(testing::StrEq("DATE"), testing::_));
    EXPECT_CALL(*connection, read_line(testing::_)).WillOnce(testing::Return(GetParam().response));
}

void RemoteLegacyFollowupArticleTest::TearDown()
{
    g_ser_line = m_old_server_line;
    g_last_command = m_old_last_command;
    LegacyFollowupArticleTest::TearDown();
}

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

TEST_P(PlainSaveArticleTest, savesTheRequestedHeadersOrBodyToANewFile)
{
    const std::string input = m_destination.generic_string() + '\n';

    const std::string output = save(GetParam(), input);

    EXPECT_EQ(expected_text(GetParam()), saved_text());
    EXPECT_NE(std::string::npos, output.find("Saved to file " + m_destination.generic_string()));
    EXPECT_EQ(m_destination.generic_string(), g_save_dest);
}

INSTANTIATE_TEST_SUITE_P(SaveCommands, PlainSaveArticleTest, testing::Values('s', 'S', 'w', 'W'));

TEST_P(AppendSaveArticleTest, appendsHeadersOrBodyWithoutReplacingExistingPlainText)
{
    std::ofstream{m_destination} << "Existing text\n";

    const std::string output = save(GetParam(), m_destination.generic_string() + '\n');

    EXPECT_EQ("Existing text\n" + expected_text(GetParam()), saved_text());
    EXPECT_NE(std::string::npos, output.find("Appended to file "));
}

TEST_P(AppendSaveArticleTest, createsAMailboxWithAnEnvelopeAndQuotedBodyFromLines)
{
    g_norm_always = false;

    const std::string output = save(GetParam(), m_destination.generic_string() + "\ny");

    expect_mailbox(GetParam());
    EXPECT_NE(std::string::npos, output.find("Saved to mailbox "));
}

TEST_P(AppendSaveArticleTest, detectsAndAppendsToAnExistingMailbox)
{
#if MBOX_CHAR == '\001'
    const std::string prefix = "\001\001\001\001\nPrevious message\n\001\001\001\001\n";
#else
    const std::string prefix = "From old@example.test Wed Oct 21 00:00:00 2026\nPrevious message\n\n";
#endif
    std::ofstream{m_destination} << prefix;

    const std::string output = save(GetParam(), m_destination.generic_string() + '\n');

    expect_mailbox(GetParam(), prefix);
    EXPECT_NE(std::string::npos, output.find("Appended to mailbox "));
}

INSTANTIATE_TEST_SUITE_P(HeadersOrBody, AppendSaveArticleTest, testing::Values('s', 'w'));

TEST_P(AppendScoreArticleTest, appendsTheRequestedRuleWithoutChangingCurrentScores)
{
    const std::string output = score(GetParam().command, GetParam().input);

    const std::string prefix = std::string_view{GetParam().filename} == "global" ? std::string{GLOBAL_RULES} : "";
    EXPECT_EQ(prefix + GetParam().line + '\n', score_file(GetParam().filename));
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_s_order_changed);
    EXPECT_THAT(output, testing::HasSubstr("type RETURN for a menu"));
}

INSTANTIATE_TEST_SUITE_P(
    EntryMethods, AppendScoreArticleTest,
    testing::Values(
        AppendScoreCase{"TypedLocalRule", '"', "\" 25 subject: Topic\n", "comp.lang.cpp", "25 subject: topic"},
        AppendScoreCase{"SubjectShortcut", '"', "\" 25 S\n", "comp.lang.cpp", "25 subject: topic"},
        AppendScoreCase{"GlobalRule", '"', "* 12 subject: Topic\n", "global", "12 subject: topic"},
        AppendScoreCase{"SubjectMenu", '"', "\n3115\n1", "comp.lang.cpp", "15 subject: topic"},
        AppendScoreCase{"AuthorMenu", '"', "\n31-6\n2", "comp.lang.cpp", "-6 from: root <root@example.test>"},
        AppendScoreCase{"CommentMenu", '"', "\n32# Reader note\n", "comp.lang.cpp", "# Reader note"},
        AppendScoreCase{"CommandMenuAppend", '\'', "\n13115\n1", "comp.lang.cpp", "15 subject: topic"}),
    [](const testing::TestParamInfo<AppendScoreCase> &info) { return info.param.name; });

TEST_P(UnchangedScoreArticleTest, returnsToReadingWithoutChangingRulesOrScores)
{
    const std::string output = score(GetParam().command, GetParam().input);

    EXPECT_THAT(output, testing::HasSubstr(GetParam().message));
    EXPECT_FALSE(fs::exists(m_scores / "comp.lang.cpp"));
    EXPECT_EQ(GLOBAL_RULES, score_file("global"));
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_s_order_changed);
}

INSTANTIATE_TEST_SUITE_P(
    CancelledOrRejectedInput, UnchangedScoreArticleTest,
    testing::Values(UnchangedScoreCase{"EraseAppendPrompt", '"', "\b", "Enter score append command"},
                    UnchangedScoreCase{"EraseExecutePrompt", '\'', "\b", "Enter scoring command"},
                    UnchangedScoreCase{"ExitAppendMenu", '"', "\n0", "Scorefile easy append mode."},
                    UnchangedScoreCase{"ExitExecuteMenu", '\'', "\n0", "Scoring easy command mode."},
                    UnchangedScoreCase{"ExitRuleTypeMenu", '"', "\n30", "What type of line do you want to add?"},
                    UnchangedScoreCase{"CancelMissingScore", '"', "\" subject: Topic\n\b", "Score entry aborted."},
                    UnchangedScoreCase{"InvalidRule", '"', "\" 20 unknown-header: topic\n", "Bad score line (ignored)"},
                    UnchangedScoreCase{"UnknownFile", '"', "@ 25 subject: topic\n", "Bad file abbreviation: @"},
                    UnchangedScoreCase{"UnknownCommand", '\'', "x\n", "Unknown scoring command |x|"}),
    [](const testing::TestParamInfo<UnchangedScoreCase> &info) { return info.param.name; });

TEST_P(TemporaryScoreArticleTest, immediatelyAppliesTheRuleWithoutPersistingIt)
{
    const std::string output = score('"', GetParam());

    expect_scores({50, 50, 0});
    EXPECT_TRUE(g_s_order_changed);
    EXPECT_FALSE(fs::exists(m_scores / "comp.lang.cpp"));
    EXPECT_EQ(GLOBAL_RULES, score_file("global"));
    EXPECT_THAT(output, testing::HasSubstr("Rescoring articles..."));
    EXPECT_THAT(output, testing::HasSubstr("Done."));
}

INSTANTIATE_TEST_SUITE_P(TypedOrMenu, TemporaryScoreArticleTest, testing::Values("! 40 subject: Topic\n", "\n5140\n1"));

TEST_P(ExecuteScoreArticleTest, explainsTheCurrentArticlesMatchingRulesAndTotal)
{
    const std::string output = score('\'', GetParam() ? "\n3" : "s\n");

    EXPECT_THAT(output, testing::HasSubstr("10 subject: topic"));
    EXPECT_THAT(output, testing::HasSubstr("Scorefile total score: 10\n"));
    EXPECT_THAT(output, testing::HasSubstr("Total score is 10\n"));
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("Other scoring total:")));
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_s_order_changed);
}

TEST_P(ExecuteScoreArticleTest, rescoringAppliesChangesToPersistedRules)
{
    std::ofstream{m_scores / "comp.lang.cpp"} << "25 subject: topic\n";

    const std::string output = score('\'', GetParam() ? "\n2" : "r\n", false);

    expect_scores({35, 35, 0});
    EXPECT_TRUE(g_s_order_changed);
    EXPECT_THAT(output, testing::HasSubstr("Rescoring articles..."));
    EXPECT_EQ("25 subject: topic\n", score_file());
}

TEST_P(ExecuteScoreArticleTest, continuesScoringUnscoredArticles)
{
    article_ptr(ArticleNum{2})->m_score_flags = SFLAG_NONE;
    article_ptr(ArticleNum{3})->m_score_flags = SFLAG_NONE;
    g_sc_fill_max = ArticleNum{1};

    const std::string output = score('\'', GetParam() ? "\n5" : "f\n", false);

    expect_scores({10, 10, 0});
    EXPECT_TRUE(g_s_order_changed);
    EXPECT_EQ(ArticleNum{3}, g_sc_fill_max);
    EXPECT_THAT(output, testing::HasSubstr("Scoring more articles..."));
}

INSTANTIATE_TEST_SUITE_P(TypedOrMenu, ExecuteScoreArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Menu" : "Typed"; });

TEST_F(ScoreArticleTest, queuedInputDefersFurtherScoringAndRemainsAvailableToReading)
{
#ifndef PENDING
    GTEST_SKIP() << "this build does not defer scoring for pending input";
#endif
    article_ptr(ArticleNum{2})->m_score_flags = SFLAG_NONE;
    article_ptr(ArticleNum{3})->m_score_flags = SFLAG_NONE;
    g_sc_fill_max = ArticleNum{1};

    const std::string output = score('\'', "f\n");

    EXPECT_TRUE(article_scored(ArticleNum{1}));
    EXPECT_FALSE(article_scored(ArticleNum{2}));
    EXPECT_FALSE(article_scored(ArticleNum{3}));
    EXPECT_EQ(ArticleNum{1}, g_sc_fill_max);
    EXPECT_FALSE(g_s_order_changed);
    EXPECT_THAT(output, testing::HasSubstr("Scoring more articles..."));
}

TEST_F(ScoreArticleTest, anUnwritableScoreFileReturnsToReadingWithoutApplyingTheRule)
{
    fs::create_directory(m_scores / "comp.lang.cpp");

    const std::string output = score('"', "\" 25 subject: Topic\n");

    EXPECT_THAT(output, testing::HasSubstr("Could not open (for append) file " +
                                           (m_scores / "comp.lang.cpp").generic_string()));
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_s_order_changed);
    EXPECT_EQ(GLOBAL_RULES, score_file("global"));
}

TEST_F(ScoreArticleTest, rescoringClearsAuthorMatchesFromRemovedRules)
{
    sf_append("! 5 from: root@example.test");
    g_artp->m_score_flags = SFLAG_NONE;
    ASSERT_EQ(15, sc_score_art(g_art, true));
    ASSERT_NE(0, g_artp->m_score_flags & SFLAG_AUTHOR);
    std::ofstream{m_scores / "comp.lang.cpp"} << "newauthor=7\n";

    score('\'', "r\n", false);

    expect_scores({17, 17, 7});
    EXPECT_EQ(0, g_artp->m_score_flags & SFLAG_AUTHOR);
}

TEST_F(ScoreArticleTest, interruptedRescoringInvalidatesOldScoresForLaterOnDemandScoring)
{
#ifndef PENDING
    GTEST_SKIP() << "this build does not defer scoring for pending input";
#endif
    std::ofstream{m_scores / "comp.lang.cpp"} << "25 subject: topic\n";

    score('\'', "r\n");

    EXPECT_FALSE(article_scored(g_art));
    EXPECT_EQ(35, sc_score_art(g_art, true));
    EXPECT_TRUE(article_scored(g_art));
}

TEST_F(DeferredScoreArticleTest, appendingARuleInitializesDeferredScoring)
{
    score('"', "\" 25 subject: Topic\n");

    EXPECT_FALSE(g_sc_delay);
    EXPECT_EQ("25 subject: topic\n", score_file());
}

TEST_F(DeferredScoreArticleTest, explainingTheCurrentScoreInitializesDeferredScoring)
{
    const std::string output = score('\'', "s\n");

    EXPECT_FALSE(g_sc_delay);
    EXPECT_TRUE(article_scored(g_art));
    EXPECT_EQ(10, g_artp->m_score);
    EXPECT_THAT(output, testing::HasSubstr("Total score is 10\n"));
}

TEST_P(ChosenEditScoreArticleTest, editsTheChosenScoreFileAndReturnsToReading)
{
    edit_score(m_input);

    expect_edited(m_chosen_file, m_before, m_argument);
}

INSTANTIATE_TEST_SUITE_P(
    FileChoices, ChosenEditScoreArticleTest,
    testing::Values(EditScoreCase{"DefaultLocal", "e\n", "comp.lang.cpp", false, true, false},
                    EditScoreCase{"WhitespaceDefault", "e \t\n", "comp.lang.cpp", false, true, false},
                    EditScoreCase{"MenuLocal", "\n4", "comp.lang.cpp", false, true, false},
                    EditScoreCase{"ExplicitLocal", "e \"\n", "comp.lang.cpp", false, true, false},
                    EditScoreCase{"Global", "e *\n", "global", false, true, false},
                    EditScoreCase{"NamedFile", "e TARGET\n", "custom-score", false, true, false},
                    EditScoreCase{"Abbreviation", "e @\n", "custom-score", false, true, false},
                    EditScoreCase{"NewLocalFile", "e\n", "comp.lang.cpp", false, false, false},
                    EditScoreCase{"NestedFile", "e TARGET\n", "nested/custom-score", false, true, false},
                    EditScoreCase{"Apostrophe", "e TARGET\n", "custom'score", false, true, false},
                    EditScoreCase{"Ampersand", "e TARGET\n", "custom&score", false, true, false},
                    EditScoreCase{"SpacedScoreDirectory", "e\n", "comp.lang.cpp", true, true, false},
                    EditScoreCase{"AbsoluteSpacedFile", "e TARGET\n", "other rules/custom score", false, true, true}),
    [](const testing::TestParamInfo<EditScoreCase> &info) { return info.param.name; });

TEST_F(EditScoreArticleTest, usesTheEditorSettingWhenVisualIsUnset)
{
    m_env.expect_no_envars({"VISUAL"});
    m_env.expect_env_repeatedly("EDITOR", m_editor_command.c_str());

    edit_score("e\n");

    expect_edited(m_scores / "comp.lang.cpp", "");
}

TEST_F(EditScoreArticleTest, rescoringUsesTheEditorsChangedRules)
{
    edit_score("e\n");

    score('\'', "r\n", false);

    expect_scores({35, 35, 0});
    EXPECT_TRUE(g_s_order_changed);
    EXPECT_EQ(EDITED_RULE, score_file());
}

TEST_P(FailedEditScoreArticleTest, aFailedEditorLeavesTheRulesAndScoresUnchanged)
{
    edit_score("e\n");

    EXPECT_EQ("# Keep these rules\n", score_file());
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_s_order_changed);
    EXPECT_EQ("score-editor\n" + (m_scores / "comp.lang.cpp").generic_string() + '\n', editor_text("tools.log"));
}

INSTANTIATE_TEST_SUITE_P(ProcessExit, FailedEditScoreArticleTest,
                         testing::Values("score-editor_failure", "score-editor_cancel"));

TEST_F(EditScoreArticleTest, anUnknownFileAbbreviationDoesNotInvokeTheEditor)
{
    const std::string output = edit_score("e ?\n");

    EXPECT_THAT(output, testing::HasSubstr("Bad file abbreviation: ?"));
    EXPECT_FALSE(fs::exists(m_editor_output / "tools.log"));
    EXPECT_EQ(GLOBAL_RULES, score_file("global"));
    expect_scores({10, 10, 0});
}

TEST_F(EditScoreArticleTest, aBlockedParentDirectoryDoesNotInvokeTheEditor)
{
    const fs::path blocker = m_output_dir / "blocked";
    std::ofstream{blocker} << "Keep this file\n";
    const fs::path target = blocker / "score";

    const std::string output = edit_score("e " + target.generic_string() + '\n');

    EXPECT_THAT(output, testing::HasSubstr("Can't make " + target.generic_string()));
    EXPECT_FALSE(fs::exists(m_editor_output / "tools.log"));
    EXPECT_EQ(GLOBAL_RULES, score_file("global"));
    expect_scores({10, 10, 0});
}

TEST_F(SaveArticleTest, erasingTheSaveCommandCancelsBeforeOpeningADestination)
{
    const std::string old_destination = g_save_dest;

    const std::string output = save('s', "\b", AS_INP);

    EXPECT_FALSE(fs::exists(m_destination));
    EXPECT_EQ(old_destination, g_save_dest);
    EXPECT_EQ("Save aborted at article 1", g_msg);
    EXPECT_EQ(std::string::npos, output.find("Saved to "));
}

TEST_F(SaveArticleTest, cancellingTheFormatPromptLeavesTheDestinationAbsent)
{
    g_norm_always = false;

    const std::string output = save('w', m_destination.generic_string() + "\nq");

    EXPECT_FALSE(fs::exists(m_destination));
    EXPECT_NE(std::string::npos, output.find("use mailbox format?"));
    EXPECT_EQ(std::string::npos, output.find("Saved to "));
}

TEST_F(SaveArticleTest, reportsAnUnwritableDestinationAndReturnsToTheArticlePrompt)
{
    const fs::path blocker = fs::path{g_priv_dir} / "blocker";
    std::ofstream{blocker} << "Keep this file\n";
    m_destination = blocker / "article";

    const std::string output = save('s', m_destination.generic_string() + '\n');

    EXPECT_NE(std::string::npos, output.find("Not saved"));
    EXPECT_EQ(std::string::npos, output.find("Saved to "));
    EXPECT_FALSE(fs::exists(m_destination));
    EXPECT_TRUE(fs::is_regular_file(blocker));
}

TEST_P(FailedSaveArticleTest, reportsTheStreamFailureAndReturnsToTheArticlePrompt)
{
    const std::string input = m_destination.generic_string() + '\n';

    const std::string output = save(GetParam().command, input);

    EXPECT_NE(std::string::npos, output.find("Not saved"));
    EXPECT_EQ(std::string::npos, output.find("Saved to "));
    EXPECT_EQ(std::string::npos, output.find("Appended to "));
    EXPECT_TRUE(m_closed);
    if (!GetParam().close_failure)
    {
        EXPECT_TRUE(saved_text().empty());
    }
    else if (GetParam().mailbox)
    {
        expect_mailbox(GetParam().command);
    }
    else
    {
        EXPECT_EQ(expected_text(GetParam().command), saved_text());
    }
}

INSTANTIATE_TEST_SUITE_P(StreamErrors, FailedSaveArticleTest,
                         testing::Values(SaveFailureCase{'s', false, false}, SaveFailureCase{'w', false, false},
                                         SaveFailureCase{'s', true, false}, SaveFailureCase{'w', true, false},
                                         SaveFailureCase{'s', false, true}, SaveFailureCase{'w', false, true},
                                         SaveFailureCase{'s', true, true}, SaveFailureCase{'w', true, true}),
                         [](const testing::TestParamInfo<SaveFailureCase> &info)
                         {
                             return std::string{info.param.command} + (info.param.mailbox ? "Mailbox" : "Plain") +
                                    (info.param.close_failure ? "Close" : "Write");
                         });

TEST_F(SaveArticleTest, anEmptyFilenameUsesTheConfiguredDirectoryAndArticleNumber)
{
    m_destination = fs::path{g_priv_dir} / "1";

    const std::string output = save('s', "\n");

    EXPECT_EQ(expected_text('s'), saved_text());
    EXPECT_NE(std::string::npos, output.find("Saved to file " + m_destination.generic_string()));
}

TEST_P(ConfiguredSaveArticleTest, runsTheConfiguredSaverWithTheRequestedArticleContent)
{
    const char command = GetParam().command;

    const std::string output = save(command, m_destination.generic_string() + '\n');

    expect_receiver();
    EXPECT_EQ(requested_text(), saved_text());
    EXPECT_NE(std::string::npos, output.find(GetParam().mailbox ? "Saved to mailbox " : "Saved to file "));
}

TEST_P(ConfiguredSaveArticleTest, closesTheProbeAndAppendsUsingTheDetectedDestinationFormat)
{
#if MBOX_CHAR == '\001'
    const std::string prefix = GetParam().mailbox ? "\001\001\001\001\nPrevious message\n" : "Existing text\n";
#else
    const std::string prefix =
        GetParam().mailbox ? "From old@example.test Wed Oct 21 00:00:00 2026\nPrevious message\n" : "Existing text\n";
#endif
    std::ofstream{m_destination} << prefix;
    g_mbox_always = !GetParam().mailbox;
    g_norm_always = GetParam().mailbox;

    const std::string output = save(GetParam().command, m_destination.generic_string() + '\n');

    expect_receiver();
    EXPECT_EQ(prefix + requested_text(), saved_text());
    EXPECT_NE(std::string::npos, output.find(GetParam().mailbox ? "Appended to mailbox " : "Appended to file "));
}

TEST_P(ConfiguredSaveArticleTest, reportsTheSaverProcessFailureAndReturnsToTheArticlePrompt)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "article-saver_failure");

    const std::string output = save(GetParam().command, m_destination.generic_string() + '\n');

    expect_receiver();
    EXPECT_NE(std::string::npos, output.find("Not saved"));
    EXPECT_EQ(std::string::npos, output.find("Saved to "));
    EXPECT_FALSE(fs::exists(m_destination));
}

INSTANTIATE_TEST_SUITE_P(CommandsAndFormats, ConfiguredSaveArticleTest,
                         testing::Values(ConfiguredSaverCase{'s', false}, ConfiguredSaverCase{'w', false},
                                         ConfiguredSaverCase{'S', false}, ConfiguredSaverCase{'W', false},
                                         ConfiguredSaverCase{'s', true}, ConfiguredSaverCase{'w', true},
                                         ConfiguredSaverCase{'S', true}, ConfiguredSaverCase{'W', true}),
                         [](const testing::TestParamInfo<ConfiguredSaverCase> &info)
                         {
                             return std::string{info.param.command == 'S' || info.param.command == 'W' ? "Preferred"
                                                                                                       : "Default"} +
                                    (info.param.command == 's' || info.param.command == 'S' ? "Headers" : "Body") +
                                    (info.param.mailbox ? "Mailbox" : "Plain");
                         });

TEST_F(PipeSaveArticleTest, pipesTheWholeArticleFromTheBeginningAndReturnsToThePrompt)
{
    g_int_count = 1;

    save('|', "   " + m_receiver + '\n');

    SCOPED_TRACE(do_interp(PIPE_SAVER));
    expect_receiver();
    EXPECT_EQ(std::string{HEADER} + std::string{BODY}, receiver_text("posted"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "shell-used"));
}

TEST_F(PipeSaveArticleTest, usesTheConfiguredPipeSaverToDeliverTheArticle)
{
    m_env.expect_env_repeatedly("PIPESAVER", "(\"" TRN_TEST_PROCESS_TOOL "\" %b) < \"%A\"");
    m_receiver = "poster -h";

    save('|', m_receiver + '\n');

    expect_receiver();
    EXPECT_EQ(std::string{HEADER} + std::string{BODY}, receiver_text("posted"));
}

TEST_F(PipeSaveArticleTest, anUppercaseSavePipeUsesThePreferredShell)
{
    save('S', "| " + m_receiver + '\n');

    SCOPED_TRACE(do_interp(PIPE_SAVER));
    EXPECT_EQ(std::string{HEADER} + std::string{BODY}, receiver_text("posted"));
#ifdef USE_SPAWN_SHELL
    EXPECT_EQ("/c", receiver_text("shell-used"));
#else
    EXPECT_EQ("-c", receiver_text("shell-used"));
#endif
    EXPECT_EQ(m_receiver, g_save_dest);
    EXPECT_FALSE(g_waiting);
    EXPECT_FALSE(fs::exists(m_destination));
}

TEST_F(PipeSaveArticleTest, erasingThePipeCommandCancelsBeforeLaunchingAReceiver)
{
    const std::string old_destination = g_save_dest;

    save('|', "\b", AS_INP);

    EXPECT_EQ(old_destination, g_save_dest);
    EXPECT_EQ("Save aborted at article 1", g_msg);
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
}

TEST_P(FailedPipeSaveArticleTest, returnsToThePromptAfterTheReceiverStopsWithoutAcceptingTheArticle)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam());
    g_int_count = 1;

    save('|', m_receiver + '\n');

    expect_receiver();
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
}

INSTANTIATE_TEST_SUITE_P(ReceiverExit, FailedPipeSaveArticleTest, testing::Values("poster_failure", "poster_cancel"),
                         [](const testing::TestParamInfo<const char *> &info)
                         { return std::string{info.index == 0 ? "Failure" : "Interruption"}; });

TEST_P(EncodedExtractSaveArticleTest, decodesTheAttachmentIntoTheRequestedDirectory)
{
    g_int_count = 1;

    const std::string output = save('e', std::string{GetParam().options} + "attachments\n");

    EXPECT_EQ("ABC", extracted_text("attachment.bin"));
    EXPECT_EQ(m_destination.generic_string(), g_extract_dest);
    EXPECT_NE(std::string::npos, output.find(GetParam().announcement));
    EXPECT_EQ(1, std::distance(fs::directory_iterator{m_destination}, fs::directory_iterator{}));
}

INSTANTIATE_TEST_SUITE_P(AttachmentFormats, EncodedExtractSaveArticleTest,
                         testing::Values(EncodedExtractCase{"Mime",
                                                            "MIME-Version: 1.0\n"
                                                            "Content-Type: application/octet-stream; "
                                                            "name=attachment.bin\n"
                                                            "Content-Transfer-Encoding: base64\n",
                                                            "QUJD\n", "", "Extracting MIME article"},
                                         EncodedExtractCase{"MultipartMime",
                                                            "MIME-Version: 1.0\n"
                                                            "Content-Type: multipart/mixed; boundary=parts\n",
                                                            "Multipart message\n--parts\n"
                                                            "Content-Type: text/plain\n\nReadme text\n"
                                                            "--parts\nContent-Type: application/octet-stream; "
                                                            "name=attachment.bin\n"
                                                            "Content-Transfer-Encoding: base64\n"
                                                            "Content-Disposition: attachment; "
                                                            "filename=attachment.bin\n\nQUJD\n--parts--\n",
                                                            "", "Extracting MIME article"},
                                         EncodedExtractCase{"Uuencode", "",
                                                            "Posted attachment follows\n\n \n"
                                                            "begin 644 attachment.bin\n#04)#\n`\nend\n",
                                                            "-1/1 ", "Extracting uuencoded file"},
                                         EncodedExtractCase{"Yenc", "",
                                                            "Posted attachment follows\n\n"
                                                            "=ybegin line=128 size=3 name=attachment.bin\n"
                                                            "klm\n=yend size=3 crc32=a3830348\n",
                                                            "", "Extracting yEnc file"}),
                         [](const testing::TestParamInfo<EncodedExtractCase> &info) { return info.param.name; });

TEST_F(ExtractSaveArticleTest, rejectsACorruptYencAttachmentWithoutReplacingAnExistingFile)
{
    article("=ybegin line=128 size=3 name=attachment.bin\nklm\n=yend size=3 crc32=00000000\n");
    fs::create_directories(m_destination);
    std::ofstream{m_destination / "attachment.bin"} << "Keep this file";

    const std::string output = save('e', "attachments\n");

    EXPECT_EQ("Keep this file", extracted_text("attachment.bin"));
    EXPECT_FALSE(g_msg.empty());
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_F(ExtractSaveArticleTest, reportsAnUnrecognizedArticleWithoutCreatingAnAttachment)
{
    article("This article has no encoded attachment.\n");

    const std::string output = save('e', "attachments\n");

    EXPECT_TRUE(fs::is_empty(m_destination));
    EXPECT_NE(std::string::npos, output.find("Unable to determine type of file."));
}

TEST_F(ExtractSaveArticleTest, reportsAnUnsupportedMimeEncodingWithoutCreatingAnAttachment)
{
    article("QUJD\n", "MIME-Version: 1.0\n"
                      "Content-Type: application/octet-stream; name=attachment.bin\n"
                      "Content-Transfer-Encoding: x-unsupported\n");

    const std::string output = save('e', "attachments\n");

    EXPECT_TRUE(fs::is_empty(m_destination));
    EXPECT_EQ("Unhandled encoding type -- aborting.", g_msg);
    EXPECT_NE(std::string::npos, output.find(g_msg));
}

TEST_F(ExtractSaveArticleTest, aBlockedDestinationReturnsToTheReadingDirectory)
{
    article(YENC);
    std::ofstream{fs::path{g_priv_dir} / "blocker"} << "Keep this file";
    m_destination = fs::path{g_priv_dir} / "blocker" / "attachments";
    g_int_count = 1;

    save('e', "blocker/attachments\n");

    EXPECT_FALSE(fs::exists(m_destination));
    EXPECT_TRUE(fs::is_regular_file(fs::path{g_priv_dir} / "blocker"));
}

TEST_F(ExtractSaveArticleTest, erasingTheExtractCommandCancelsBeforeCreatingADirectory)
{
    const std::string old_destination = g_extract_dest;

    save('e', "\b", AS_INP);

    EXPECT_FALSE(fs::exists(m_destination));
    EXPECT_EQ(old_destination, g_extract_dest);
    EXPECT_EQ("Save aborted at article 1", g_msg);
}

TEST_F(ExtractSaveArticleTest, anEmptyDestinationReusesThePreviousExtractionDirectory)
{
    article(YENC);
    save('e', "attachments\n");
    ASSERT_TRUE(fs::remove(m_destination / "attachment.bin"));

    save('e', "\n");

    EXPECT_EQ("ABC", extracted_text("attachment.bin"));
    EXPECT_EQ(m_destination.generic_string(), g_extract_dest);
}

TEST_F(ExtractSaveArticleTest, passesTheArticleToTheConfiguredExtractionCommandInTheDestination)
{
    article(BODY);

    const std::string output = save('e', custom_input());

    expect_custom_receiver();
    EXPECT_EQ(m_article_text, extracted_text("posted"));
    EXPECT_NE(std::string::npos, output.find("Extracting article into " + m_destination.generic_string()));
}

TEST_P(FailedExtractSaveArticleTest, returnsToThePromptAfterTheExtractionProcessStopsWithoutOutput)
{
    article(BODY);
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam());
    g_int_count = 1;

    save('e', custom_input());

    expect_custom_receiver();
    EXPECT_FALSE(fs::exists(m_destination / "posted"));
}

INSTANTIATE_TEST_SUITE_P(ExtractorExit, FailedExtractSaveArticleTest,
                         testing::Values("poster_failure", "poster_cancel"),
                         [](const testing::TestParamInfo<const char *> &info)
                         { return std::string{info.index == 0 ? "Failure" : "Interruption"}; });

TEST_F(ExtractSaveArticleTest, anEmptyCommandReusesThePreviousCustomExtractor)
{
    article(BODY);
    save('e', custom_input());
    ASSERT_TRUE(fs::remove(m_destination / "posted"));

    save('e', "\n");

    EXPECT_EQ(m_article_text, extracted_text("posted"));
    EXPECT_EQ("poster\n-h\nposter\n-h\n", extracted_text("tools.log"));
    EXPECT_EQ(m_receiver, g_extract_prog);
    EXPECT_EQ(m_destination.generic_string(), g_extract_dest);
}

TEST_F(ExtractSaveArticleTest, anExplicitDirectoryReturnsToAutomaticAttachmentDetection)
{
    article(BODY);
    save('e', custom_input());
    article(YENC);
    m_destination = fs::path{g_priv_dir} / "decoded";

    const std::string output = save('e', "decoded\n");

    EXPECT_EQ("ABC", extracted_text("attachment.bin"));
    EXPECT_EQ(m_destination.generic_string(), g_extract_dest);
    EXPECT_NE(std::string::npos, output.find("Extracting yEnc file"));
    EXPECT_FALSE(fs::exists(m_destination / "tools.log"));
}

TEST_F(ExtractSaveArticleTest, passesTheDetectedShellArchiveOffsetToTheConfiguredHandler)
{
    constexpr std::string_view archive = "#!/bin/sh\necho controlled archive input\n";
    article(std::string{"Posted archive follows\n\n"} + std::string{archive});
    const std::string saver =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" article-saver \"%A\" %B \"shar source\" %a %C";
    m_env.expect_env_repeatedly("SHARSAVER", saver.c_str());

    const std::string output = save('e', "attachments\n");

    EXPECT_EQ(archive, extracted_text("shar source"));
    EXPECT_EQ("article-saver\n" + (m_spool / "1").generic_string() + '\n' + std::to_string(g_save_from.value_of()) +
                  "\nshar source\n1\ncomp.lang.cpp\n",
              extracted_text("article-saver.log"));
    EXPECT_NE(std::string::npos, output.find("Extracting shar into "));
    EXPECT_FALSE(g_waiting);
}

TEST_F(ExternalExtractTest, defaultCustomHandlerDeliversTheArticleInput)
{
    const std::string source = source_bytes();
#ifdef USE_SPAWN_SHELL
    const std::string expected = source;
#else
    const std::string expected = source.substr(source.find("\n\n") + 2);
#endif

    save('e', custom_input());

    EXPECT_EQ(expected, delivered_bytes("posted"));
    expect_custom_receiver();
}

TEST_P(ExtractOverrideTest, configuredHandlerReceivesTheSelectedByteRange)
{
    const std::string source = source_bytes();
    const std::size_t offset = source.find(GetParam().shell_archive ? "#!/bin/sh" : "Root first line");
    ASSERT_NE(std::string::npos, offset);

    save('e', GetParam().shell_archive ? "attachments\n" : custom_input());

    EXPECT_EQ(source.substr(offset), delivered_bytes("payload"));
    EXPECT_EQ(offset, g_save_from.value_of());
    EXPECT_EQ("article-saver\n" + (m_spool / "1").generic_string() + '\n' + std::to_string(offset) +
                  "\npayload\n1\ncomp.lang.cpp\n",
              extracted_text("article-saver.log"));
    EXPECT_FALSE(g_waiting);
}

INSTANTIATE_TEST_SUITE_P(Environment, ExtractOverrideTest,
                         testing::Values(ExtractOverrideCase{"CustomDocumented", "EXSAVER", false, false},
                                         ExtractOverrideCase{"CustomLegacy", "CUSTOMSAVER", false, false},
                                         ExtractOverrideCase{"CustomPrecedence", "EXSAVER", true, false},
                                         ExtractOverrideCase{"SharDocumented", "UNSHAR", false, true},
                                         ExtractOverrideCase{"SharLegacy", "SHARSAVER", false, true},
                                         ExtractOverrideCase{"SharPrecedence", "UNSHAR", true, true}),
                         [](const testing::TestParamInfo<ExtractOverrideCase> &info) { return info.param.name; });

#ifdef USE_SPAWN_SHELL
TEST_F(ExternalExtractTest, defaultShellArchiveHandlerReportsUnsupported)
{
    article(SHAR);
    testing::internal::CaptureStderr();

    save('e', "attachments\n");

    const std::string error = testing::internal::GetCapturedStderr();
    EXPECT_THAT(error, testing::HasSubstr("SHARSAVER is unsupported on Windows"));
    EXPECT_TRUE(fs::is_empty(m_destination));
}
#else
TEST_F(ExternalExtractTest, defaultShellArchiveHandlerExtractsTheDetectedScript)
{
    article(SHAR);

    save('e', "attachments\n");

    EXPECT_EQ("Extracted archive\n", delivered_bytes("archive output"));
    EXPECT_EQ(1, std::distance(fs::directory_iterator{m_destination}, fs::directory_iterator{}));
}
#endif

TEST_P(EncodedViewArticleTest, displaysTheDecodedAttachmentAndRemovesTheTemporaryFile)
{
    const std::string output = view();

    expect_viewed(GetParam().filename, "ABC");
    EXPECT_EQ("viewer\n" + std::string{GetParam().filename} + '\n', receiver_text("tools.log"));
    EXPECT_NE(std::string::npos, output.find("Processing attachments..."));
}

INSTANTIATE_TEST_SUITE_P(AttachmentFormats, EncodedViewArticleTest,
                         testing::Values(ViewAttachmentCase{"Mime",
                                                            "MIME-Version: 1.0\n"
                                                            "Content-Type: application/octet-stream; name=image.bin\n"
                                                            "Content-Transfer-Encoding: base64\n",
                                                            "QUJD\n", "image.bin"},
                                         ViewAttachmentCase{"Uuencode", "",
                                                            "Posted attachment follows\n\n"
                                                            "begin 644 image.jpg\n#04)#\n`\nend\n",
                                                            "image.jpg"}),
                         [](const testing::TestParamInfo<ViewAttachmentCase> &info) { return info.param.name; });

TEST_F(ViewArticleTest, displaysMultipartAttachmentsInOrderWithoutViewingTheText)
{
    article("--parts\nContent-Type: text/plain\n\nReadme text\n"
            "--parts\nContent-Type: application/octet-stream; name=one.bin\n"
            "Content-Transfer-Encoding: base64\n\nQUJD\n"
            "--parts\nContent-Type: application/octet-stream; name=two.bin\n"
            "Content-Transfer-Encoding: base64\n\nREVG\n--parts--\n",
            "MIME-Version: 1.0\nContent-Type: multipart/mixed; boundary=parts\n");

    view();

    expect_viewed("one.bin", "ABC");
    expect_viewed("two.bin", "DEF");
    EXPECT_EQ("viewer\none.bin\nviewer\ntwo.bin\n", receiver_text("tools.log"));
    EXPECT_EQ((m_parts / "two.bin").generic_string(), receiver_text("viewer-directory"));
}

TEST_P(FailedViewArticleTest, removesTheTemporaryAttachmentWhenTheViewerStops)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam());

    view();

    EXPECT_EQ("viewer\nimage.bin\n", receiver_text("tools.log"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "viewed-image.bin"));
    EXPECT_FALSE(fs::exists(m_parts / "image.bin"));
}

INSTANTIATE_TEST_SUITE_P(ViewerExit, FailedViewArticleTest, testing::Values("viewer_failure", "viewer_cancel"),
                         [](const testing::TestParamInfo<const char *> &info)
                         { return info.index == 0 ? "Failure" : "Cancellation"; });

TEST_F(ViewArticleTest, reportsAnAttachmentWithoutAMatchingViewer)
{
    mailcap("image/jpeg; " + m_viewer + '\n');

    const std::string output = view();

    EXPECT_NE(std::string::npos, output.find("No view method for application/octet-stream -- skipping."));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
    EXPECT_FALSE(fs::exists(m_parts));
}

TEST_F(ViewArticleTest, reportsAnOrdinaryArticleWithoutInvokingAViewer)
{
    article(BODY);

    const std::string output = view();

    EXPECT_NE(std::string::npos, output.find("Unable to determine type of file."));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
    EXPECT_FALSE(fs::exists(m_parts));
}

TEST_F(ViewArticleTest, selectsTheNextViewerWhenAMailcapTestCommandFails)
{
    const std::string probe =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" pager";
    mailcap("application/octet-stream; invalid-first-viewer; test=" + probe + '\n' + "application/octet-stream; " +
            m_viewer + '\n');
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "pager_failure");

    view();

    expect_viewed("image.bin", "ABC");
    EXPECT_EQ("pager\nviewer\nimage.bin\n", receiver_text("tools.log"));
}

TEST_P(AddressedReplyArticleTest, sendsThePreparedReplyToTheArticleRecipient)
{
    const std::string header = expected_header(GetParam().reply_to ? "desk@example.test" : "root@example.test");
    const std::string quote = GetParam().command == 'R' ? std::string{QUOTED_BODY} : std::string{};

    respond_with(GetParam().command);

    expect_sent(header + quote);
    const std::string mailed = receiver_text("mailed");
    EXPECT_NE(std::string::npos, mailed.find(header.substr(0, header.find('\n'))));
    EXPECT_NE(std::string::npos, mailed.find("Subject: Re: Topic"));
    EXPECT_EQ(GetParam().command == 'R', mailed.find("> Root first line") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(ReplyCommands, AddressedReplyArticleTest,
                         testing::Values(ReplyCommandCase{"PlainFrom", 'r', false},
                                         ReplyCommandCase{"QuotedFrom", 'R', false},
                                         ReplyCommandCase{"PlainReplyTo", 'r', true},
                                         ReplyCommandCase{"QuotedReplyTo", 'R', true}),
                         [](const testing::TestParamInfo<ReplyCommandCase> &info) { return info.param.name; });

TEST_F(ReplyArticleTest, abortingTheReplyRetainsTheDraftWithoutSendingMail)
{
    answers("a\n");

    respond_with('R');

    expect_retained(expected_header() + std::string{QUOTED_BODY});
    EXPECT_NE(std::string::npos, receiver_text("tools.log").find("editor\n"));
    EXPECT_EQ(std::string::npos, receiver_text("tools.log").find("mailer\n"));
}

TEST_F(ReplyArticleTest, aMailerFailureRetainsTheDraftAndReturnsToReading)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "mailer_failure");
    answers("s\na\n");

    respond_with('R');

    expect_retained(expected_header() + std::string{QUOTED_BODY});
    EXPECT_NE(std::string::npos, receiver_text("tools.log").find("mailer\n"));
}

TEST_F(ReplyArticleTest, aBlockedDraftDoesNotInvokeTheReplyProgram)
{
    const fs::path blocker = fs::path{g_priv_dir} / "blocked";
    std::ofstream{blocker} << "Keep this file\n";
    g_head_name = (blocker / "reply").generic_string();

    const std::string output = respond_with('r');

    EXPECT_NE(std::string::npos, output.find("Can't create " + g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

TEST_F(ReplyArticleTest, usesConfiguredHeadersAttributionAndQuotePrefix)
{
    m_env.expect_env_repeatedly("MAILHEADER", "To: custom@example.test\nSubject: Custom %S\n\n");
    m_env.expect_env_repeatedly("YOUSAID", "Reply to %i:");
    g_indent_string = "| ";

    respond_with('R');

    expect_sent("To: custom@example.test\nSubject: Custom Topic\n\n"
                "Reply to <root@example.test>:\n| Root first line\n| Root next line\n"
                "| From a body line\n| Last line\n\n");
}

TEST_F(ReplyArticleTest, quotesTheDecodedMimeText)
{
    article("Original=20body\nSecond=20line\n", std::string{REPLY_HEADERS} +
                                                    "MIME-Version: 1.0\nContent-Type: text/plain; charset=us-ascii\n"
                                                    "Content-Transfer-Encoding: quoted-printable\n");

    respond_with('R');

    expect_sent(expected_header() + "In article <root@example.test> you write:\n"
                                    "> Original body\n> Second line\n\n");
}

TEST_P(OriginalForwardArticleTest, mailsTheCompleteOriginalArticleAsAMimeAttachment)
{
    const std::string output = forward_article();

    expect_sent(default_draft(), false);
    EXPECT_THAT(output, testing::HasSubstr("To: "));
    EXPECT_THAT(receiver_text("mailed"), testing::HasSubstr(m_article_text));
    EXPECT_THAT(receiver_text("mailed"), testing::HasSubstr("Subject: Topic (fwd)\n"));
}

INSTANTIATE_TEST_SUITE_P(OriginalContent, OriginalForwardArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Multipart" : "Plain"; });

TEST_P(ConfiguredMimeForwardArticleTest, usesTheConfiguredMimeBoundaryAndCommentHeaders)
{
    const std::string header =
        "To: desk@example.test\nSubject: Forwarded Topic\n" + std::string{GetParam().content_type} + "\n\n";

    forward_article();

    expect_sent(mime_draft(header, "forward-boundary", GetParam().introduction), false);
}

INSTANTIATE_TEST_SUITE_P(
    MimeHeaders, ConfiguredMimeForwardArticleTest,
    testing::Values(
        ForwardMimeHeaderCase{"OneLine", "Content-Type: multipart/mixed; boundary=\"forward-boundary\"",
                              "Content-Type: text/html\n"},
        ForwardMimeHeaderCase{"FoldedSpace", "Content-Type: multipart/mixed;\n boundary=\"forward-boundary\"",
                              "Content-Type: text/html\n"},
        ForwardMimeHeaderCase{"FoldedTab", "Content-Type: multipart/mixed;\n\tboundary=\"forward-boundary\"",
                              "Content-Type: text/html\n"},
        ForwardMimeHeaderCase{"LowercaseType", "content-type: multipart/mixed; boundary=\"forward-boundary\"",
                              "Content-Type: text/html\n"}),
    [](const testing::TestParamInfo<ForwardMimeHeaderCase> &info) { return info.param.name; });

TEST_F(ForwardArticleTest, usesConfiguredPlainForwardingMarkers)
{
    use_plain_header();
    m_env.expect_env_repeatedly("FORWARDMSG", "Begin %i");
    m_env.expect_env_repeatedly("FORWARDMSGEND", "End %S");

    forward_article();

    expect_sent(plain_draft("Begin <root@example.test>", "End Topic"), false);
}

TEST_F(ForwardArticleTest, omitsEmptyInterpolatedPlainForwardingMarkers)
{
    use_plain_header();
    m_env.expect_env_repeatedly("FORWARDMSG", "%{FORWARD_EMPTY}");
    m_env.expect_env_repeatedly("FORWARDMSGEND", "%{FORWARD_EMPTY}");
    m_env.expect_no_envars({"FORWARD_EMPTY"});

    forward_article();

    expect_sent(plain_draft("", ""), false);
}

TEST_F(ForwardArticleTest, escapesDashLeadingLinesInThePlainForwardedDraft)
{
    use_plain_header();
    article("- First line\n-- signature\nLast line\n", REPLY_HEADERS);
    std::string draft = plain_draft();
    draft.insert(draft.find("- First line"), "- ");
    draft.insert(draft.find("-- signature"), "- ");

    const std::string output = forward_article();

    expect_sent(draft, false);
    EXPECT_THAT(output, testing::Not(testing::HasSubstr("- ")));
}

TEST_P(FailedForwardArticleTest, retainsTheForwardedDraftAndReturnsToReading)
{
    forward_article();

    expect_retained(plain_draft());
    EXPECT_EQ(GetParam().mailer_invoked, receiver_text("tools.log").find("mailer\n") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(DeliveryOutcomes, FailedForwardArticleTest,
                         testing::Values(ForwardFailureCase{"Abort", "", "a\n", false},
                                         ForwardFailureCase{"MailerFailure", "mailer_failure", "s\na\n", true}),
                         [](const testing::TestParamInfo<ForwardFailureCase> &info) { return info.param.name; });

TEST_F(ForwardArticleTest, aBlockedDraftDoesNotInvokeTheMailComposer)
{
    const fs::path blocker = fs::path{g_priv_dir} / "blocked";
    std::ofstream{blocker} << "Keep this file\n";
    g_head_name = (blocker / "forward").generic_string();

    const std::string output = forward_article();

    EXPECT_THAT(output, testing::HasSubstr("Can't create " + g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

TEST_P(DisplayedForwardHeaderArticleTest, displaysTheHeaderWhenTheComposerDoesNotReferenceTheHeaderPlaceholder)
{
    const std::string output = forward_article();

    const std::string description = GetParam() ? "(Above lines saved in file " : "(Header in ";
    EXPECT_THAT(output, testing::HasSubstr(std::string{PLAIN_HEADER} + '\n' + description + g_head_name + ")\n"));
    expect_sent(plain_draft(), false);
}

INSTANTIATE_TEST_SUITE_P(HeaderDisplay, DisplayedForwardHeaderArticleTest, testing::Bool(),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Verbose" : "Compact"; });

TEST_P(ArticleRevisionCommandTest, postsTheRequestedRevisionOfTheCurrentArticle)
{
    const std::string output = revise(GetParam());

    expect_revision_posted(GetParam(), revision_draft(GetParam()));
    if (GetParam() == 'C')
    {
        EXPECT_THAT(output, testing::HasSubstr("Canceling..."));
    }
}

TEST_P(ArticleRevisionCommandTest, usesTheConfiguredRevisionHeader)
{
    const std::string control = GetParam() == 'C' ? "Control: cancel " : "Supersedes: ";
    const std::string header = "Newsgroups: comp.lang.cpp\nSubject: Replacement %S\nFrom: %[from]\n" + control +
                               "%i\nX-Original-Article: %a\n\n";
    m_env.expect_env_repeatedly(GetParam() == 'C' ? "CANCELHEADER" : "SUPERSEDEHEADER", header.c_str());

    revise(GetParam());

    const std::string draft =
        "Newsgroups: comp.lang.cpp\nSubject: Replacement Topic\nFrom: Root <root@example.test>\n" + control +
        "<root@example.test>\nX-Original-Article: 1\n\n" + (GetParam() == 'Z' ? std::string{BODY} : "");
    expect_revision_posted(GetParam(), draft);
}

TEST_P(ArticleRevisionCommandTest, acceptsTheLocalAuthorsMatchingReplyTo)
{
    g_login_name = "alias";
    article(BODY, std::string{NEWS_ARTICLE_HEADERS} + "Reply-To: alias@example.test\n");

    revise(GetParam());

    expect_revision_posted(GetParam(), revision_draft(GetParam(), "alias@example.test"));
}

TEST_P(ArticleRevisionCommandTest, rejectsTheSameLoginNameOnAnotherHost)
{
    if (current_user_id() == ROOT_UID)
    {
        GTEST_SKIP() << "root is permitted to revise other authors' articles";
    }
    g_host_name = "elsewhere.test";

    const std::string output = revise(GetParam());

    EXPECT_THAT(output, testing::HasSubstr(g_verbose ? "someone else's article" : "Not your article"));
    EXPECT_FALSE(fs::exists(g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

TEST_P(ArticleRevisionCommandTest, aBlockedDraftDoesNotInvokeThePostingProgram)
{
    const fs::path blocker = fs::path{g_priv_dir} / "blocked";
    std::ofstream{blocker} << "Keep this file\n";
    g_head_name = (blocker / "revision").generic_string();

    const std::string output = revise(GetParam());

    EXPECT_THAT(output, testing::HasSubstr("Can't create " + g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

INSTANTIATE_TEST_SUITE_P(Commands, ArticleRevisionCommandTest, testing::Values('C', 'z', 'Z'),
                         [](const testing::TestParamInfo<char> &info)
                         { return info.param == 'C'   ? "Cancel"
                                  : info.param == 'z' ? "Replace"
                                                      : "EditOriginal"; });

TEST_F(ArticleRevisionTest, aMatchingConfiguredFromAddressAuthorizesCancellation)
{
    g_login_name = "reader";
    g_host_name = "elsewhere.test";
    m_env.expect_env_repeatedly("FROM", "root <ROOT@EXAMPLE.TEST>");

    revise('C');

    expect_revision_posted('C', "Newsgroups: comp.lang.cpp\nSubject: cancel\nFrom: root <ROOT@EXAMPLE.TEST>\n"
                                "Control: cancel <root@example.test>\nDistribution: world\n\n"
                                "<root@example.test> was cancelled from within trn.\n");
}

TEST_F(ArticleRevisionTest, theDefaultCancelCommandInvokesInewsWithTheControlDraft)
{
    const fs::path tools = m_output_dir / "tools";
    fs::create_directories(tools);
    const fs::path inews = tools /
#ifdef USE_SPAWN_SHELL
                           "inews.exe";
    const char separator = ';';
#else
                           "inews";
    const char separator = ':';
#endif
    fs::copy_file(TRN_TEST_PROCESS_TOOL, inews);
    const char       *old_path = std::getenv("PATH");
    const std::string path = tools.generic_string() + separator + (old_path ? old_path : "");
    m_env.expect_env_repeatedly("PATH", path.c_str());
    m_env.expect_no_envars({"CANCEL"});

    revise('C');

    EXPECT_EQ(revision_draft('C'), receiver_text("posted"));
    EXPECT_EQ("inews\n-h\n", receiver_text("tools.log"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "editor-input"));
}

TEST_P(ArticleRevisionDisplayTest, rejectsAnotherAuthorsArticleWithoutCreatingADraft)
{
    if (current_user_id() == ROOT_UID)
    {
        GTEST_SKIP() << "root is permitted to revise other authors' articles";
    }
    g_login_name = "reader";

    const std::string output = revise(std::get<0>(GetParam()));

    EXPECT_THAT(output, testing::HasSubstr(g_verbose ? "someone else's article" : "Not your article"));
    EXPECT_FALSE(fs::exists(g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

TEST_P(ArticleRevisionDisplayTest, aMissingArticleDoesNotCreateARevisionDraft)
{
    const char            command = std::get<0>(GetParam());
    const ArticlePosition position = g_art_pos;
    art_close();
    ASSERT_TRUE(fs::remove(m_spool / "1"));
    push_char('n');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{command, FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    const std::string message =
        g_verbose ? command == 'C' ? "Can't cancel an empty article." : "Can't supersede an empty article."
                  : "Empty article.";
    EXPECT_THAT(output, testing::HasSubstr(message));
    EXPECT_EQ(g_curr_art, g_art);
    EXPECT_EQ(g_curr_artp, g_artp);
    EXPECT_EQ(position, g_art_pos);
    EXPECT_EQ(2, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_artp->m_flags & (AF_EXISTS | AF_UNREAD | AF_SEL));
    EXPECT_TRUE(g_selected_only);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(m_spool, fs::current_path());
    EXPECT_FALSE(fs::exists(g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(CommandsAndVerbosity, ArticleRevisionDisplayTest,
                         testing::Combine(testing::Values('C', 'z', 'Z'), testing::Bool()));

TEST_P(FailedCancelArticleTest, aFailedCancelPostRetainsTheControlDraftAndReturnsToReading)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", GetParam());

    revise('C');

    EXPECT_EQ(revision_draft('C'), receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
    EXPECT_EQ("poster\n-h\n", receiver_text("tools.log"));
}

INSTANTIATE_TEST_SUITE_P(ProcessExit, FailedCancelArticleTest, testing::Values("poster_failure", "poster_cancel"),
                         [](const testing::TestParamInfo<const char *> &info)
                         { return std::string{info.param} == "poster_failure" ? "Failure" : "Cancellation"; });

TEST_P(FailedSupersedeArticleTest, retainsTheReplacementWhenPostingDoesNotComplete)
{
    revise(GetParam().command);

    expect_news_retained(revision_draft(GetParam().command));
    EXPECT_EQ(GetParam().poster_invoked, receiver_text("tools.log").find("poster\n-h\n") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    CommandsAndOutcomes, FailedSupersedeArticleTest,
    testing::Values(SupersedeFailureCase{"ReplaceAborted", 'z', "", "a\n", false},
                    SupersedeFailureCase{"EditOriginalAborted", 'Z', "", "a\n", false},
                    SupersedeFailureCase{"ReplacePostFailed", 'z', "poster_failure", "s\na\n", true},
                    SupersedeFailureCase{"EditOriginalPostFailed", 'Z', "poster_failure", "s\na\n", true}),
    [](const testing::TestParamInfo<SupersedeFailureCase> &info) { return info.param.name; });

TEST_P(GroupedFollowupArticleTest, postsTheFollowupToTheRequestedGroups)
{
    const std::string groups = *GetParam().followup_groups ? GetParam().followup_groups : "comp.lang.cpp";
    const std::string header = expected_news_header(groups, *GetParam().followup_groups != '\0');
    const std::string quote = GetParam().command == 'F' ? std::string{QUOTED_ARTICLE} : std::string{};

    follow_with(GetParam().command, GetParam().command == 'f' ? "n\n" : "");

    expect_posted(header + quote);
    const std::string posted = receiver_text("posted");
    EXPECT_NE(std::string::npos, posted.find("Newsgroups: " + groups + '\n'));
    EXPECT_NE(std::string::npos, posted.find("References: <parent@example.test> <root@example.test>\n"));
    EXPECT_EQ(GetParam().command == 'F', posted.find("> Root first line") != std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(FollowupCommands, GroupedFollowupArticleTest,
                         testing::Values(FollowupCommandCase{"PlainCurrent", 'f', ""},
                                         FollowupCommandCase{"QuotedCurrent", 'F', ""},
                                         FollowupCommandCase{"PlainRedirected", 'f', "comp.test"},
                                         FollowupCommandCase{"QuotedRedirected", 'F', "comp.test"}),
                         [](const testing::TestParamInfo<FollowupCommandCase> &info) { return info.param.name; });

TEST_F(FollowupArticleTest, quittingTheNewTopicPromptDoesNotCreateADraft)
{
    follow_with('f', "q\n");

    EXPECT_FALSE(fs::exists(g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

TEST_F(FollowupArticleTest, aNewTopicStartsWithoutTheOriginalSubjectOrReferences)
{
    prompt_answers("New topic\n\n");

    follow_with('f', "\n");

    expect_posted("Newsgroups: comp.lang.cpp\nSubject: New topic\nSummary: \nExpires: \n"
                  "Sender: \nFollowup-To: \nDistribution: \nOrganization: Test Organization\n"
                  "Keywords: \nCc: \n\n");
    const std::string posted = receiver_text("posted");
    EXPECT_NE(std::string::npos, posted.find("Subject: New topic\n"));
    EXPECT_EQ(std::string::npos, posted.find("References:"));
    EXPECT_EQ(std::string::npos, posted.find("root@example.test"));
}

TEST_F(FollowupArticleTest, abortingPostingRetainsTheEditedFollowup)
{
    answers("a\n");

    follow_with('F');

    expect_news_retained(expected_news_header() + std::string{QUOTED_ARTICLE});
    EXPECT_EQ(std::string::npos, receiver_text("tools.log").find("poster\n"));
}

TEST_F(FollowupArticleTest, aPosterFailureRetainsTheFollowupAndReturnsToReading)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "poster_failure");
    answers("s\na\n");

    follow_with('F');

    expect_news_retained(expected_news_header() + std::string{QUOTED_ARTICLE});
    EXPECT_NE(std::string::npos, receiver_text("tools.log").find("poster\n-h\n"));
}

TEST_F(FollowupArticleTest, aBlockedDraftDoesNotInvokeThePostingProgram)
{
    const fs::path blocker = fs::path{g_priv_dir} / "blocked";
    std::ofstream{blocker} << "Keep this file\n";
    g_head_name = (blocker / "followup").generic_string();

    const std::string output = follow_with('F');

    EXPECT_NE(std::string::npos, output.find("Can't create " + g_head_name));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "tools.log"));
}

TEST_F(FollowupArticleTest, usesConfiguredNewsHeadersAttributionAndQuotePrefix)
{
    m_env.expect_env_repeatedly("NEWSHEADER", "Newsgroups: comp.test\nSubject: Custom %S\nReferences: %R\n\n");
    m_env.expect_env_repeatedly("ATTRIBUTION", "Followup to %i:");
    g_indent_string = "| ";

    follow_with('F');

    expect_posted("Newsgroups: comp.test\nSubject: Custom Topic\n"
                  "References: <parent@example.test> <root@example.test>\n\n"
                  "Followup to <root@example.test>:\n| Root first line\n| Root next line\n"
                  "| From a body line\n| Last line\n\n");
}

TEST_F(FollowupArticleTest, quotesTheDecodedMimeTextInThePostedFollowup)
{
    article("Original=20body\nSecond=20line\n", std::string{NEWS_ARTICLE_HEADERS} +
                                                    "MIME-Version: 1.0\nContent-Type: text/plain; charset=us-ascii\n"
                                                    "Content-Transfer-Encoding: quoted-printable\n");

    follow_with('F');

    expect_posted(expected_news_header() + "In article <root@example.test>, Root <root@example.test> wrote:\n"
                                           "> Original body\n> Second line\n\n");
}

TEST_P(EmailFollowupArticleTest, honorsTheRequestedEmailFollowupRoute)
{
    const std::string recipient = GetParam().recipient;
    std::string       header = expected_news_header(GetParam().route, true);
    header.replace(header.find("Cc: \n"), 5, "Cc: " + recipient + '\n');
    std::string quote{QUOTED_ARTICLE};
    if (*GetParam().reply_to)
    {
        const std::string          address = std::string{"<"} + GetParam().reply_to + "> wrote:";
        constexpr std::string_view original = "<root@example.test> wrote:";
        quote.replace(quote.find(original), original.size(), address);
    }
    const std::string draft = header + quote;

    const std::string output = follow_with('F');

    EXPECT_EQ(draft, receiver_text(fs::path{g_head_name}.filename().string().c_str()));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
#ifdef USE_SPAWN_SHELL
    EXPECT_EQ(recipient + '\n', receiver_text("recipients"));
    EXPECT_THAT(receiver_text("mailed"), testing::HasSubstr(quote));
    EXPECT_EQ(draft + '\n', receiver_text("editor-input"));
    EXPECT_FALSE(fs::exists(m_home / "dead.article"));
#else
    EXPECT_THAT(output, testing::HasSubstr("mail your reply instead?"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "mailed"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "editor-input"));
#endif
}

INSTANTIATE_TEST_SUITE_P(
    EmailRoutes, EmailFollowupArticleTest,
    testing::Values(EmailFollowupCase{"PosterFrom", "poster", "", "root@example.test"},
                    EmailFollowupCase{"PosterReplyTo", "poster", "desk@example.test", "desk@example.test"},
                    EmailFollowupCase{"ExplicitAddress", "desk@example.test", "", "desk@example.test"}),
    [](const testing::TestParamInfo<EmailFollowupCase> &info) { return info.param.name; });

#ifdef USE_SPAWN_SHELL
TEST_F(FollowupArticleTest, aFailedEmailFollowupRetainsTheDraftWithoutPostingNews)
{
    article(BODY, std::string{NEWS_ARTICLE_HEADERS} + "Followup-To: poster\n");
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "mailer_failure");
    answers("s\na\n");
    std::string header = expected_news_header("poster", true);
    header.replace(header.find("Cc: \n"), 5, "Cc: root@example.test\n");

    follow_with('F');

    expect_news_retained(header + std::string{QUOTED_ARTICLE});
    EXPECT_THAT(receiver_text("tools.log"), testing::HasSubstr("mailer\n"));
    EXPECT_THAT(receiver_text("tools.log"), testing::Not(testing::HasSubstr("poster\n")));
}
#endif

TEST_P(CrosspostedFollowupArticleTest, mailsTheFollowupToItsDistinctModerators)
{
    const std::string draft = moderated_draft(GetParam().groups, GetParam().recipients);

    follow_with('F');

    expect_mailed(draft);
    EXPECT_THAT(receiver_text("tools.log"), testing::HasSubstr("mailer\n"));
    EXPECT_THAT(receiver_text("tools.log"), testing::Not(testing::HasSubstr("poster\n")));
}

INSTANTIATE_TEST_SUITE_P(
    ModeratorRoutes, CrosspostedFollowupArticleTest,
    testing::Values(ModeratorFollowupCase{"SingleGroup", "comp.lang.cpp", "z-moderator@example.test"},
                    ModeratorFollowupCase{"SharedModerator", "comp.lang.cpp,list.shared", "z-moderator@example.test"},
                    ModeratorFollowupCase{"DistinctModerators", "comp.lang.cpp,comp.test",
                                          "a-moderator@example.test, z-moderator@example.test"},
                    ModeratorFollowupCase{"MixedGroups", "open.test,comp.lang.cpp", "z-moderator@example.test"}),
    [](const testing::TestParamInfo<ModeratorFollowupCase> &info) { return info.param.name; });

TEST_P(FailedModeratorFollowupArticleTest, retainsTheModeratedDraftWithoutFallingBackToNews)
{
    follow_with('F');

    expect_mail_retained(moderated_draft("comp.lang.cpp", "z-moderator@example.test"));
    EXPECT_EQ(GetParam().mailer_invoked, receiver_text("tools.log").find("mailer\n") != std::string::npos);
    EXPECT_THAT(receiver_text("tools.log"), testing::Not(testing::HasSubstr("poster\n")));
}

INSTANTIATE_TEST_SUITE_P(MailOutcomes, FailedModeratorFollowupArticleTest,
                         testing::Values(FailedModeratorFollowupCase{"Abort", "", "a\n", false},
                                         FailedModeratorFollowupCase{"MailerFailure", "mailer_failure", "s\na\n",
                                                                     true}),
                         [](const testing::TestParamInfo<FailedModeratorFollowupCase> &info)
                         { return info.param.name; });

TEST_F(ModeratedFollowupArticleTest, anApprovedFollowupIsPostedWithoutModeratorMail)
{
    m_env.expect_env_repeatedly("NEWSHEADER", "Newsgroups: %F\nSubject: Re: %S\nApproved: editor@example.test\n\n");

    follow_with('F');

    expect_posted("Newsgroups: comp.lang.cpp\nSubject: Re: Topic\nApproved: editor@example.test\n\n" +
                  std::string{QUOTED_ARTICLE});
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "mailed"));
}

TEST_P(LegacyDraftPathFollowupArticleTest, postsTheDraftWhenTheComposerRequestsAnInewsHandoff)
{
    follow_with('F');

    const std::string draft = expected_news_header() + std::string{QUOTED_ARTICLE};
    expect_handoff(draft);
    EXPECT_EQ(draft, receiver_text("posted"));
    EXPECT_THAT(receiver_text("tools.log"), testing::HasSubstr("inews\n-h\n"));
    EXPECT_FALSE(fs::exists(m_home / "dead.article"));
}

INSTANTIATE_TEST_SUITE_P(DraftPaths, LegacyDraftPathFollowupArticleTest, testing::Values(false, true),
                         [](const testing::TestParamInfo<bool> &info) { return info.param ? "Spaces" : "Plain"; });

TEST_F(LegacyFollowupArticleTest, aFailedInewsHandoffAppendsTheDraftToTheRecoveryFile)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "inews_failure");
    std::ofstream{m_home / "dead.article"} << "An older draft\n";

    const std::string output = follow_with('F');

    const std::string draft = expected_news_header() + std::string{QUOTED_ARTICLE};
    expect_handoff(draft);
    EXPECT_EQ("An older draft\n" + draft, receiver_text("home/dead.article"));
    EXPECT_THAT(output, testing::HasSubstr("Article appended to "));
    EXPECT_THAT(receiver_text("tools.log"), testing::HasSubstr("inews\n-h\n"));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
}

TEST_F(LegacyFollowupArticleTest, aBlockedRecoveryFileLeavesTheOriginalDraftAvailable)
{
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "inews_failure");
    fs::create_directory(m_home / "dead.article");

    const std::string output = follow_with('F');

    expect_handoff(expected_news_header() + std::string{QUOTED_ARTICLE});
    EXPECT_THAT(output, testing::HasSubstr("Unable to append article to "));
    EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
}

TEST_P(RemoteLegacyFollowupArticleTest, checksTheServerBeforeHandingTheDraftToInews)
{
    const std::string output = follow_with('F');

    const std::string draft = expected_news_header() + std::string{QUOTED_ARTICLE};
    expect_handoff(draft);
    if (GetParam().accepted)
    {
        EXPECT_EQ(draft, receiver_text("posted"));
        EXPECT_FALSE(fs::exists(m_home / "dead.article"));
    }
    else
    {
        EXPECT_EQ(draft, receiver_text("home/dead.article"));
        EXPECT_THAT(output, testing::HasSubstr("Article appended to "));
        EXPECT_FALSE(fs::exists(fs::path{g_priv_dir} / "posted"));
        EXPECT_THAT(receiver_text("tools.log"), testing::Not(testing::HasSubstr("inews\n")));
    }
}

INSTANTIATE_TEST_SUITE_P(ServerStatus, RemoteLegacyFollowupArticleTest,
                         testing::Values(RemoteFollowupCase{"DateAccepted", "111 20261022100000", true},
                                         RemoteFollowupCase{"DateUnsupported", "500 Unknown command", true},
                                         RemoteFollowupCase{"AccessDenied", "502 Permission denied", false}),
                         [](const testing::TestParamInfo<RemoteFollowupCase> &info) { return info.param.name; });

TEST_P(SelectedOnDemandThreadArticleTest, threadsSpoolHeadersAndRetainsReadAndSelectionState)
{
    open_group();
    article_ptr(ArticleNum{3})->m_flags &= ~AF_UNREAD;
    g_newsgroup_ptr->m_to_read--;
    article_ptr(ArticleNum{2})->select_article(AUTO_KILL_NONE);
    g_selected_only = GetParam();
    ASSERT_EQ(1, g_selected_count);

    const std::string output = display_tree();

    SCOPED_TRACE(output);
    EXPECT_NE(std::string::npos, output.find("Threading the group. "));
    EXPECT_NE(std::string::npos, output.find("[1] Topic\n[2] Branch\n"));
    const std::string current_marker = GetParam() ? "<1>" : "[1]";
    EXPECT_NE(std::string::npos, output.find("<so>" + current_marker + "<se>+-[<so>1<se>]"));
    EXPECT_NE(std::string::npos, output.find("\\-(2)"));
    EXPECT_EQ(g_curr_artp, article_ptr(ArticleNum{2})->m_parent);
    EXPECT_EQ(g_curr_artp, article_ptr(ArticleNum{3})->m_parent);
    EXPECT_EQ(article_ptr(ArticleNum{2}), g_curr_artp->m_child1);
    EXPECT_EQ(article_ptr(ArticleNum{3}), article_ptr(ArticleNum{2})->m_sibling);
    EXPECT_NE(AF_NONE, article_ptr(ArticleNum{2})->m_flags & AF_SEL);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{3})->m_flags & AF_UNREAD);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_first_cached);
    EXPECT_EQ(ArticleNum{3}, g_last_cached);
    EXPECT_TRUE(g_cached_all_in_range);
    for (const auto &[number, article] : g_article_list)
    {
        if (number > g_last_art)
        {
            break;
        }
        SCOPED_TRACE(number.value_of());
        EXPECT_NE(AF_NONE, article.m_flags & AF_THREADED);
        EXPECT_EQ(g_curr_artp, article.m_subj->m_thread);
    }
}

INSTANTIATE_TEST_SUITE_P(ReadingScope, SelectedOnDemandThreadArticleTest, testing::Bool());

TEST_F(OnDemandThreadArticleTest, threadsCurrentHeaderWithoutEagerCaching)
{
    open_group(false);
    ASSERT_EQ(AF_NONE, article_ptr(ArticleNum{2})->m_flags & AF_CACHED);

    const std::string output = display_tree();

    SCOPED_TRACE(output);
    EXPECT_NE(std::string::npos, output.find("Threading the group. "));
    EXPECT_NE(std::string::npos, output.find("[1] Topic\n\n  <so>[1]<se>\n"));
    EXPECT_NE(AF_NONE, g_curr_artp->m_flags & AF_THREADED);
    EXPECT_EQ(g_curr_artp, g_curr_artp->m_subj->m_thread);
    EXPECT_EQ(nullptr, g_curr_artp->m_child1);
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{2})->m_flags & (AF_CACHED | AF_THREADED));
    EXPECT_EQ(AF_NONE, article_ptr(ArticleNum{3})->m_flags & (AF_CACHED | AF_THREADED));
    EXPECT_EQ(3, g_newsgroup_ptr->m_to_read);
    EXPECT_FALSE(g_thread_always);
}

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

TEST_P(InputNewsgroupForwardTest, advancesToTheNextGroupWithTheRequestedTraversalPolicy)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
}

TEST_P(InputNewsgroupForwardTest, advancingFromTheLastGroupMovesPastTheEnd)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();
    set_current_newsgroup(2);

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[2], g_current_newsgroup);
}

TEST_P(InputNewsgroupForwardTest, advancingWhenAlreadyPastTheEndBreaksTraversal)
{
    const char command = GetParam().first;
    prepare_groups();
    g_newsgroup_ptr = nullptr;

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(ING_BREAK, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
}

TEST_P(InputNewsgroupForwardTest, advancingFollowsTheCurrentGroupOrder)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();
    ASSERT_TRUE(move_newsgroup_order(&g_newsgroup_data[2], NewsgroupNum{1}));
    set_current_newsgroup(0);

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[0], g_current_newsgroup);
}

TEST_P(InputNewsgroupBackwardTest, retreatsToThePreviousGroupWithTheRequestedTraversalPolicy)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
}

TEST_P(InputNewsgroupBackwardTest, retreatingFromTheFirstGroupStaysAtTheFirstGroup)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();
    set_current_newsgroup(0);

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
}

TEST_P(InputNewsgroupBackwardTest, retreatingFromPastTheEndStartsWithTheLastGroup)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();
    g_newsgroup_ptr = nullptr;

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
}

TEST_P(InputNewsgroupBackwardTest, retreatingInAnEmptyCatalogHasNoTarget)
{
    const auto &[command, expected_result] = GetParam();

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
}

TEST_P(InputNewsgroupBackwardTest, retreatingFollowsTheCurrentGroupOrder)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();
    ASSERT_TRUE(move_newsgroup_order(&g_newsgroup_data[2], NewsgroupNum{1}));

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
}

TEST_P(InputNewsgroupFirstTest, jumpsFromTheCurrentGroupToTheFirstGroup)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
}

TEST_P(InputNewsgroupFirstTest, jumpsFromPastTheEndToTheFirstGroup)
{
    const auto &[command, expected_result] = GetParam();
    prepare_groups();
    g_newsgroup_ptr = nullptr;

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
}

TEST_P(InputNewsgroupFirstTest, jumpingToFirstInAnEmptyCatalogHasNoTarget)
{
    const auto &[command, expected_result] = GetParam();

    const InputNewsgroupResult result = run_command(std::string(1, command));

    EXPECT_EQ(expected_result, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
}

TEST_P(InputNewsgroupPositionTest, jumpingToEndLeavesTraversalPastTheLastGroup)
{
    prepare_groups();
    if (GetParam())
    {
        g_newsgroup_ptr = nullptr;
    }

    const InputNewsgroupResult result = run_command("$");

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
}

TEST_P(InputNewsgroupPositionTest, recallsTheRecentGroupFromTheCurrentPosition)
{
    prepare_groups();
    g_recent_newsgroup = &g_newsgroup_data[0];
    g_add_new_by_default = ADDNEW_SUB;
    if (GetParam())
    {
        g_newsgroup_ptr = nullptr;
    }

    const InputNewsgroupResult result = run_command("-");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ("comp.alpha", g_newsgroup_name);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(ADDNEW_ASK, g_add_new_by_default);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
}

TEST_F(InputNewsgroupNavigationTest, jumpingToEndInAnEmptyCatalogHasNoTarget)
{
    const InputNewsgroupResult result = run_command("$");

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
}

TEST_F(InputNewsgroupNavigationTest, recallingWithoutARecentGroupHasNoTarget)
{
    prepare_groups();

    const InputNewsgroupResult result = run_command("-");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
}

TEST_F(InputNewsgroupNavigationTest, decliningToResubscribeTheRecentGroupRestoresTheCurrentGroup)
{
    prepare_groups();
    g_recent_newsgroup = &g_newsgroup_data[0];
    g_recent_newsgroup->m_subscribe_char = '!';
    g_recent_newsgroup->m_to_read = TR_UNSUB;

    const InputNewsgroupResult result = run_command("-n");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_EQ('!', g_recent_newsgroup->m_subscribe_char);
    EXPECT_EQ(TR_UNSUB, g_recent_newsgroup->m_to_read);
}

INSTANTIATE_TEST_SUITE_P(TraversalPolicy, InputNewsgroupForwardTest,
                         testing::Values(NewsgroupNavigationCommand{'n', ING_NORM},
                                         NewsgroupNavigationCommand{'N', ING_SPECIAL}));

INSTANTIATE_TEST_SUITE_P(TraversalPolicy, InputNewsgroupBackwardTest,
                         testing::Values(NewsgroupNavigationCommand{'p', ING_NORM},
                                         NewsgroupNavigationCommand{'P', ING_SPECIAL}));

INSTANTIATE_TEST_SUITE_P(TraversalPolicy, InputNewsgroupFirstTest,
                         testing::Values(NewsgroupNavigationCommand{'^', ING_NORM},
                                         NewsgroupNavigationCommand{'1', ING_SPECIAL}));

INSTANTIATE_TEST_SUITE_P(FromGroupOrEnd, InputNewsgroupPositionTest, testing::Bool());

TEST_P(InputNewsgroupNumericLookupTest, selectsTheRequestedNumericPosition)
{
    const auto &[target, index] = GetParam();

    const InputNewsgroupResult result = run_command("g " + std::string{target} + '\n');

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[index], g_newsgroup_ptr);
    EXPECT_EQ(g_newsgroup_data[index].rc_name(), g_newsgroup_name);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
}

TEST_P(InputNewsgroupInvalidNumberTest, invalidPositionsPreserveTheCurrentGroupAndAskAgain)
{
    const std::string_view target = GetParam();

    const InputNewsgroupResult result = run_command("g " + std::string{target} + '\n');

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_THAT(m_output, testing::HasSubstr("Only 3 groups. Try again."));
}

TEST_P(InputNewsgroupEmptyTargetTest, aMissingTargetDoesNotSelectGroupZero)
{
    const auto &[spaces, at_end] = GetParam();
    if (at_end)
    {
        start_at_end();
    }
    NewsgroupData *original = g_newsgroup_ptr;

    const InputNewsgroupResult result = run_command("g" + std::string{spaces} + '\n');

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_THAT(m_output, testing::HasSubstr("Newsgroup name or number required."));
}

TEST_P(InputNewsgroupNameFailureTest, aFailedNamedLookupPreservesTheCurrentGroup)
{
    const auto &[failure, at_end] = GetParam();
    const auto &[target, diagnostic] = failure;
    if (at_end)
    {
        start_at_end();
    }
    NewsgroupData *original = g_newsgroup_ptr;
    g_add_new_by_default = ADDNEW_SUB;

    const InputNewsgroupResult result = run_command("g " + std::string{target} + '\n');

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr(diagnostic));
    EXPECT_EQ(ADDNEW_ASK, g_add_new_by_default);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_count);
}

TEST_P(InputNewsgroupSubscriptionLookupTest, decliningResubscriptionPreservesTheCurrentGroup)
{
    if (GetParam())
    {
        start_at_end();
    }
    NewsgroupData *original = g_newsgroup_ptr;
    g_newsgroup_data[0].m_subscribe_char = '!';
    g_newsgroup_data[0].m_to_read = TR_UNSUB;

    const InputNewsgroupResult result = run_command("g comp.alpha\nn");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_EQ('!', g_newsgroup_data[0].m_subscribe_char);
    EXPECT_EQ(TR_UNSUB, g_newsgroup_data[0].m_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_P(InputNewsgroupSubscriptionLookupTest, acceptingResubscriptionSelectsTheRequestedGroup)
{
    if (GetParam())
    {
        start_at_end();
    }
    g_newsgroup_data[0].m_subscribe_char = '!';
    g_newsgroup_data[0].m_to_read = TR_UNSUB;

    const InputNewsgroupResult result = run_command("g comp.alpha\ny");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ("comp.alpha", g_newsgroup_name);
    EXPECT_EQ(':', g_newsgroup_data[0].m_subscribe_char);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[0].m_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_P(InputNewsgroupLookupCancelTest, erasingTheLookupCommandPreservesTheCurrentGroup)
{
    const char erase = GetParam();

    const InputNewsgroupResult result = run_command(std::string{"g"} + erase);

    EXPECT_EQ(ING_INPUT, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
}

TEST_F(InputNewsgroupLookupTest, numericLookupUsesTheDisplayedGroupNumberAfterReordering)
{
    ASSERT_TRUE(move_newsgroup_order(&g_newsgroup_data[2], NewsgroupNum{0}));

    const InputNewsgroupResult result = run_command("g 0\n");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ("comp.charlie", g_newsgroup_name);
}

TEST_F(InputNewsgroupNavigationTest, numericLookupInAnEmptyCatalogAsksAgain)
{
    const InputNewsgroupResult result = run_command("g 0\n");

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Only 0 groups. Try again."));
}

INSTANTIATE_TEST_SUITE_P(ValidPositions, InputNewsgroupNumericLookupTest,
                         testing::Values(NumericNewsgroupTarget{"0", 0}, NumericNewsgroupTarget{"2", 2},
                                         NumericNewsgroupTarget{"0002", 2}));

INSTANTIATE_TEST_SUITE_P(OutOfRangePositions, InputNewsgroupInvalidNumberTest,
                         testing::Values("3", "99", "2147483647", "2147483648", "999999999999999999999999999999"));

INSTANTIATE_TEST_SUITE_P(FailureAndPosition, InputNewsgroupNameFailureTest,
                         testing::Combine(testing::Values(NewsgroupNameFailure{"comp.missing", "No comp.missing!"},
                                                          NewsgroupNameFailure{"comp/invalid", "Bad newsgroup name."}),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(WhitespaceAndPosition, InputNewsgroupEmptyTargetTest,
                         testing::Combine(testing::Values<std::string_view>("", "   "), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(FromGroupOrEnd, InputNewsgroupSubscriptionLookupTest, testing::Bool());

INSTANTIATE_TEST_SUITE_P(EraseAndKill, InputNewsgroupLookupCancelTest, testing::Values('\b', Ctl('u')));

TEST_P(InputNewsgroupSearchMissTest, aMissPreservesTheCurrentGroupOrEndPrompt)
{
    const auto &[direction, at_end, verbose] = GetParam();
    set_search_origin(at_end);
    NewsgroupData *original = g_current_newsgroup;
    g_verbose = verbose;

    const InputNewsgroupResult result = run_command(std::string{direction} + "missing\n");

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_EQ(original, g_current_newsgroup);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_THAT(m_output, testing::HasSubstr("Searching..."));
    EXPECT_THAT(m_output, testing::HasSubstr(verbose ? "Not found -- use a or g to add newsgroups\n" : "Not found\n"));
}

TEST_P(InputNewsgroupSearchErrorTest, anInvalidExpressionAsksAgainAtTheOriginalPosition)
{
    const auto &[direction, at_end, error] = GetParam();
    const auto &[pattern, diagnostic] = error;
    set_search_origin(at_end);
    NewsgroupData *original = g_current_newsgroup;

    const InputNewsgroupResult result = run_command(std::string{direction} + std::string{pattern} + '\n');

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_EQ(original, g_current_newsgroup);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_THAT(m_output, testing::HasSubstr(diagnostic));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Searching...")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Not found")));
}

TEST_P(InputNewsgroupSearchCancelTest, cancellingSearchInputPreservesTheOriginalPosition)
{
    const auto &[direction, at_end, input] = GetParam();
    set_search_origin(at_end);
    NewsgroupData *original = g_current_newsgroup;

    const InputNewsgroupResult result = run_command(std::string{direction} + std::string{input});

    EXPECT_EQ(ING_INPUT, result);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_EQ(original, g_current_newsgroup);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Searching...")));
    EXPECT_FALSE(macro_pending());
}

TEST_P(InputNewsgroupSearchDirectionTest, aMatchSelectsTheNextGroupInTheRequestedDirection)
{
    const auto &[direction, at_end] = GetParam();
    set_search_origin(at_end);
    NewsgroupData    *original = g_current_newsgroup;
    const std::size_t expected = direction == '/' ? (at_end ? 0 : 2) : (at_end ? 2 : 0);

    const InputNewsgroupResult result = run_command(std::string{direction} + "comp\n");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[expected], g_newsgroup_ptr);
    EXPECT_EQ(original, g_current_newsgroup);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Not found")));
}

TEST_P(InputNewsgroupSearchWrapTest, searchingPastTheListBoundaryWrapsToTheOtherEnd)
{
    const char direction = GetParam();
    set_current_newsgroup(direction == '/' ? 2 : 0);
    NewsgroupData *original = g_current_newsgroup;

    const InputNewsgroupResult result = run_command(std::string{direction} + "comp\n");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[direction == '/' ? 0 : 2], g_newsgroup_ptr);
    EXPECT_EQ(original, g_current_newsgroup);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
}

INSTANTIATE_TEST_SUITE_P(DirectionPositionAndVerbosity, InputNewsgroupSearchMissTest,
                         testing::Combine(testing::Values('/', '?'), testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(DirectionPositionAndError, InputNewsgroupSearchErrorTest,
                         testing::Combine(testing::Values('/', '?'), testing::Bool(),
                                          testing::Values(NewsgroupSearchError{"[abc", "Missing ]"},
                                                          NewsgroupSearchError{"", "No previous search pattern"})));

INSTANTIATE_TEST_SUITE_P(DirectionPositionAndCancellation, InputNewsgroupSearchCancelTest,
                         testing::Combine(testing::Values('/', '?'), testing::Bool(),
                                          testing::Values<std::string_view>("\b", "needle\025")));

INSTANTIATE_TEST_SUITE_P(DirectionAndPosition, InputNewsgroupSearchDirectionTest,
                         testing::Combine(testing::Values('/', '?'), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ForwardAndBackward, InputNewsgroupSearchWrapTest, testing::Values('/', '?'));

TEST_P(InputNewsgroupNormalEntryTest, normalEntryReadsTheFirstUnreadArticleAndBypassesTheAutomaticSelector)
{
    g_use_news_selector = 0;

    const InputNewsgroupResult result = run_command(std::string{GetParam()} + 'q');

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Root first line")));
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[1].m_to_read);
}

TEST_P(InputNewsgroupDefaultEntryTest, defaultEntryUsesTheSelectorAtTheConfiguredUnreadThreshold)
{
    g_use_news_selector = 2;
    g_default_cmd = "+npq";

    const InputNewsgroupResult result = run_command(std::string{GetParam()} + 'Q');

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
}

TEST_P(InputNewsgroupSelectorExitTest, selectorExitPreservesTheRequestedGroupDestination)
{
    const auto &[command, expected, index] = GetParam();

    const InputNewsgroupResult result = run_command(std::string{"+"} + command);

    EXPECT_EQ(expected, result);
    EXPECT_EQ(&g_newsgroup_data[index], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
}

TEST_P(InputNewsgroupExplicitArticleTest, anExplicitStartingCommandReadsTheRequestedArticle)
{
    const auto &[number, body, unread] = GetParam();

    const InputNewsgroupResult result = run_command(std::string{"."} + number + "\nq");

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr(body));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ(unread, g_newsgroup_data[1].m_to_read);
}

TEST_P(InputNewsgroupStartingCommandCancelTest, cancellingTheStartingCommandDoesNotEnterTheGroup)
{
    const InputNewsgroupResult result = run_command(GetParam());

    EXPECT_EQ(ING_INPUT, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_EQ(ArticleNum{}, g_last_art);
}

TEST_P(InputNewsgroupNoReadingGroupTest, entryWithoutACurrentGroupAsksAgain)
{
    const InputNewsgroupResult result = run_command(std::string{GetParam()});

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Not on a newsgroup."));
}

TEST_F(InputNewsgroupReadingTest, defaultEntryBelowTheUnreadThresholdDoesNotEnterTheSelector)
{
    g_use_news_selector = 3;
    g_default_cmd = "ynpq";

    const InputNewsgroupResult result = run_command(" Q");

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply")));
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(NG_ASK, g_newsgroup_exit_code);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(InputNewsgroupReadingTest, readSelectorEntryListsPreviouslyReadArticles)
{
    const InputNewsgroupResult result = run_command("U+Q");

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Root"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Root first line")));
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    EXPECT_FALSE(g_sel_rereading);
}

TEST_F(InputNewsgroupReadingTest, anEmptyStartingCommandUsesTheConfiguredSelector)
{
    g_use_news_selector = 0;

    const InputNewsgroupResult result = run_command(".\nQ");

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
}

TEST_F(InputNewsgroupReadingTest, theSubjectListingCommandRunsBeforeArticleReading)
{
    const InputNewsgroupResult result = run_command("=q");

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Topic"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
}

TEST_F(InputNewsgroupReadingTest, quittingBackToTheGroupPromptKeepsTheCurrentGroup)
{
    const InputNewsgroupResult result = run_command("yQ");

    EXPECT_EQ(ING_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[1].m_to_read);
}

TEST_F(InputNewsgroupReadingTest, anUnavailableSpoolAdvancesToTheNextGroup)
{
    m_data_source.m_spool_dir = (m_output_dir / "missing").generic_string();
    const std::string original_numbers{g_newsgroup_data[1].rc_numbers()};

    const InputNewsgroupResult result = run_command("y");

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Can't chdir to directory"));
    EXPECT_EQ(original_numbers, g_newsgroup_data[1].rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ForcedReading, InputNewsgroupNormalEntryTest, testing::Values('y', 'Y', '\t'));

INSTANTIATE_TEST_SUITE_P(DefaultCommands, InputNewsgroupDefaultEntryTest, testing::Values(' ', '\r', '\n'));

INSTANTIATE_TEST_SUITE_P(GroupDestinations, InputNewsgroupSelectorExitTest,
                         testing::Values(std::make_tuple('Q', ING_ASK, std::size_t{1}),
                                         std::make_tuple('q', ING_NORM, std::size_t{2}),
                                         std::make_tuple('N', ING_NORM, std::size_t{2}),
                                         std::make_tuple('P', ING_NORM, std::size_t{0})));

INSTANTIATE_TEST_SUITE_P(ReadAndUnreadArticles, InputNewsgroupExplicitArticleTest,
                         testing::Values(std::make_tuple('1', std::string_view{"Root first line"}, ArticleUnread{2}),
                                         std::make_tuple('3', std::string_view{"Branch body"}, ArticleUnread{1})));

INSTANTIATE_TEST_SUITE_P(EraseAndKill, InputNewsgroupStartingCommandCancelTest,
                         testing::Values<std::string_view>(".\b", ".3\025"));

INSTANTIATE_TEST_SUITE_P(ReadingCommands, InputNewsgroupNoReadingGroupTest,
                         testing::Values('y', '+', 'U', '.', '=', ';'));

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
    add_newsgroup("comp.lang.apl", "1-3");
    add_newsgroup("comp.lang.cpp", "1-2");
    set_current_newsgroup(0);
    push_command("/cpp\n");

    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
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

TEST_F(InputNewsgroupTest, readsRemoteArticleFromMockServer)
{
    add_newsgroup(TRN_TEST_NNTP_NEWSGROUP, test_article_read_numbers(),
                  ArticleNum{TRN_TEST_ARTICLE_NUM}, ArticleNum{TRN_TEST_ARTICLE_NUM});
    set_current_newsgroup(0);
    m_data_source.m_flags = DF_REMOTE | DF_NO_XREFS;
    m_data_source.m_news_id = "127.0.0.1";
    m_data_source.m_spool_dir = m_output_dir.generic_string();

    MockNntpServer server;
    server.expect_capabilities({"VERSION 2", "MODE-READER"});
    server.expect_mode_reader();
    server.expect_capabilities({"VERSION 2", "READER", "LIST ACTIVE NEWSGROUPS"});
    server.expect_group(TRN_TEST_NNTP_NEWSGROUP, 1, TRN_TEST_ARTICLE_NUM, TRN_TEST_ARTICLE_NUM);
    server.expect_listgroup({TRN_TEST_ARTICLE_NUM});
    server.expect_head(TRN_TEST_ARTICLE_NUM, test_article_header());
    server.expect_body(TRN_TEST_ARTICLE_NUM, {TRN_TEST_BODY});
    server.expect_quit();
    server.start();

    g_nntp_link.connection.reset();
    g_nntp_link.port_number = server.port();
    g_nntp_link.flags = NNTP_NEW_CMD_OK;
    ASSERT_EQ(1, nntp_connect("127.0.0.1", false));
    m_data_source.m_nntp_link = g_nntp_link;
    push_command(" qq");

    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::optional<std::string>{});
    const std::string       output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(NG_NEXT, result);
    EXPECT_THAT(output, testing::HasSubstr(TRN_TEST_BODY));

    nntp_close(true);
    server.verify();
}
