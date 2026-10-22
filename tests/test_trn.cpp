// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/trn.h>

#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/mime.h>
#include <trn/ng-internal.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/respond-internal.h>
#include <trn/respond.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/smisc.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/univ.h>
#include <util/env.h>

#include <test_config.h>

#include "mock_env.h"
#include "support/MockNntpServer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
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
    std::string save(char command, std::string_view input, ArticleSwitchResult expected = AS_ASK);
    std::string saved_text() const;
    std::string expected_text(char command, bool mailbox = false) const;
    void        expect_mailbox(char command, std::string_view prefix = {});

    fs::path m_destination;
    fs::path m_spool;

    static constexpr std::string_view HEADER = "From: Root <root@example.test>\n"
                                               "Subject: Topic\n"
                                               "Date: 21 Oct 2026 10:00:00 GMT\n"
                                               "Message-ID: <root@example.test>\n\n";
    static constexpr std::string_view BODY = "Root first line\nRoot next line\nFrom a body line\nLast line\n";

private:
    trn::testing::MockEnvironment m_env;
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

std::string SaveArticleTest::save(char command, std::string_view input, ArticleSwitchResult expected)
{
    const std::pair<ArticleNum, Article *> current = std::make_pair(g_curr_art, g_curr_artp);
    const std::pair<ArticleNum, Article *> recent = std::make_pair(g_recent_art, g_recent_artp);
    const ArticlePosition                  position = g_art_pos;
    const ArticleLine                      line = g_art_line_num;
    const ArticleLine                      top = g_top_line;
    const ArticleNum                       first = g_first_art;
    const SubjectFlags                     subject_flags = g_curr_artp->m_subj->m_flags;
    std::vector<ArticleFlags>              flags;
    for (long number = 1; number <= 3; ++number)
    {
        flags.push_back(article_ptr(ArticleNum{number})->m_flags);
    }
    push_char('n');
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
    EXPECT_EQ(3, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(1, g_selected_subj_cnt);
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
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('n', read_tty_char());
    EXPECT_FALSE(macro_pending());
    return output;
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
    bool m_write_error{};

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
        m_write_error = std::ferror(stream) != 0;
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
    EXPECT_EQ(!GetParam().close_failure, m_write_error);
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
