// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/trn.h>

#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/smisc.h>
#include <trn/terminal.h>
#include <trn/univ.h>

#include <test_config.h>

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
