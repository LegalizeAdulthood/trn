// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/trn.h>

#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>
#include <trn/addng.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artsrch.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/decode.h>
#include <trn/final.h>
#include <trn/hash.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/last.h>
#include <trn/mime.h>
#include <trn/ng-internal.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/ngsrch.h>
#include <trn/nntp.h>
#include <trn/only.h>
#include <trn/opt.h>
#include <trn/OptionCatalog.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/respond-internal.h>
#include <trn/respond.h>
#include <trn/rt-page.h>
#include <trn/rt-select-internal.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/scan.h>
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

#ifdef _WIN32
#include <share.h>
#endif

#ifdef I_UNISTD
#include <unistd.h>
#endif

#include <test_config.h>
#include <test_process_config.h>

#include "mock_env.h"
#include "MockNNTPConnection.h"
#include "support/MockNntpServer.h"

#include <boost/asio/error.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
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
        m_old_newsgroup_order = std::move(g_ng_order.order);
        m_old_newsgroup_count = g_newsgroup_count;
        m_old_newsgroup_to_read = g_newsgroup_to_read;
        m_old_newsgroup_min_to_read = g_newsgroup_min_to_read;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_first_newsgroup = g_ng_order.first_newsgroup;
        m_old_last_newsgroup = g_ng_order.last_newsgroup;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_current_newsgroup = g_current_newsgroup;
        m_old_article_list = std::move(g_cache.article_list);
        m_old_art_ptr_list = std::move(g_art_order.ptr_list);
        m_old_art_ptr = g_art_order.ptr;
        m_old_first_cached = g_cache.first_cached;
        m_old_last_cached = g_cache.last_cached;
        m_old_cached_all_in_range = g_cache.cached_all_in_range;
        m_old_target = g_nav.target;
        m_old_recent = g_nav.recent;
        m_old_current = g_nav.current;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_multirc = g_multirc;
        m_old_data_source = g_data_source;
        m_old_nntp_link = g_nntp.link;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_newsgroup_dir = g_newsgroup_dir;
        m_old_default_cmd = g_default_cmd;
        m_old_restore_old_newsrc = g_restore_old_newsrc;
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
        m_old_selected_only = g_sel.selected_only;
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
        m_old_parsed_art = g_parsed_header.parsed_art;
        m_old_head_buf = g_parsed_header.head_buf;
        m_old_in_header = g_parsed_header.in_header;
        m_old_art_pos = g_art_input.pos;
        m_old_art_line_num = g_art_input.line_num;
        m_old_art_fp = g_art_input.fp;
        m_old_open_art = g_art_input.open_art;
        m_old_raw_art_size = g_raw_art_size;
        m_old_art_size = g_art_size;
        m_old_our_pid = g_our_pid;
        m_old_tmp_dir = g_tmp_dir;
        m_old_current_path = fs::current_path();
        std::copy(std::begin(g_header_rules.type), std::end(g_header_rules.type), m_old_header_type.begin());
        m_old_header_positions = g_parsed_header.positions;
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
        g_ng_order.order.clear();
        g_newsgroup_data.reserve(3);
        g_ng_order.order.reserve(3);
        g_newsgroup_count = NewsgroupNum{};
        g_newsgroup_to_read = NewsgroupNum{};
        g_newsgroup_min_to_read = TR_NONE;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{};
        g_ng_order.first_newsgroup = nullptr;
        g_ng_order.last_newsgroup = nullptr;
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
        g_cache.article_list.clear();
        g_art_order.ptr_list.clear();
        g_art_order.ptr = nullptr;
        g_cache.first_cached = ArticleNum{};
        g_cache.last_cached = ArticleNum{};
        g_cache.cached_all_in_range = false;
        g_nav.target.num = ArticleNum{};
        g_nav.recent.num = ArticleNum{};
        g_nav.current.num = ArticleNum{};
        g_nav.target.art = nullptr;
        g_nav.recent.art = nullptr;
        g_nav.current.art = nullptr;
        g_newsrc_hash = new HashTable(17, compare_newsgroup_name);
        g_multirc = &m_multirc;
        g_data_source = &m_data_source;
        init_nntp();
        g_nntp.link.connection.reset();
        g_newsgroup_name.clear();
        g_newsgroup_dir.clear();
        g_default_cmd = "npq";
        g_restore_old_newsrc = false;
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
        g_sel.selected_only = false;
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
        g_parsed_header.parsed_art = ArticleNum{};
        g_parsed_header.head_buf.clear();
        g_parsed_header.in_header = HEAD_FIRST;
        g_art_input.pos = ArticlePosition{};
        g_art_input.line_num = ArticleLine{};
        g_art_input.fp = nullptr;
        g_art_input.open_art = ArticleNum{};
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
        g_art_buf.init();
        g_back_page.init();
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
        if (g_art_input.fp != nullptr)
        {
            std::fclose(g_art_input.fp);
            g_art_input.fp = nullptr;
        }
        (void) nntp_art_name(ArticleNum{}, false);
        g_art_buf.final();
        g_back_page.final();
        head_final();
        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);
        m_data_source.close();
        if (g_newsrc_hash != nullptr && g_newsrc_hash != m_old_newsrc_hash)
        {
            delete g_newsrc_hash;
            g_newsrc_hash = nullptr;
        }

        g_newsgroup_data = std::move(m_old_newsgroup_data);
        g_ng_order.order = std::move(m_old_newsgroup_order);
        g_newsgroup_count = m_old_newsgroup_count;
        g_newsgroup_to_read = m_old_newsgroup_to_read;
        g_newsgroup_min_to_read = m_old_newsgroup_min_to_read;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_ng_order.first_newsgroup = m_old_first_newsgroup;
        g_ng_order.last_newsgroup = m_old_last_newsgroup;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_current_newsgroup = m_old_current_newsgroup;
        g_cache.article_list.clear();
        g_cache.article_list = std::move(m_old_article_list);
        g_art_order.ptr_list = std::move(m_old_art_ptr_list);
        g_art_order.ptr = m_old_art_ptr;
        g_cache.first_cached = m_old_first_cached;
        g_cache.last_cached = m_old_last_cached;
        g_cache.cached_all_in_range = m_old_cached_all_in_range;
        g_nav.target = m_old_target;
        g_nav.recent = m_old_recent;
        g_nav.current = m_old_current;
        g_newsrc_hash = m_old_newsrc_hash;
        g_multirc = m_old_multirc;
        g_data_source = m_old_data_source;
        g_nntp.link.connection.reset();
        g_nntp.link = m_old_nntp_link;
        g_newsgroup_name = std::move(m_old_newsgroup_name);
        g_newsgroup_dir = std::move(m_old_newsgroup_dir);
        g_default_cmd = std::move(m_old_default_cmd);
        g_restore_old_newsrc = m_old_restore_old_newsrc;
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
        g_sel.selected_only = m_old_selected_only;
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
        g_parsed_header.parsed_art = m_old_parsed_art;
        g_parsed_header.head_buf = std::move(m_old_head_buf);
        g_parsed_header.in_header = m_old_in_header;
        g_art_input.pos = m_old_art_pos;
        g_art_input.line_num = m_old_art_line_num;
        g_art_input.fp = m_old_art_fp;
        g_art_input.open_art = m_old_open_art;
        g_raw_art_size = m_old_raw_art_size;
        g_art_size = m_old_art_size;
        g_our_pid = m_old_our_pid;
        g_tmp_dir = m_old_tmp_dir;
        std::copy(m_old_header_type.begin(), m_old_header_type.end(), std::begin(g_header_rules.type));
        g_parsed_header.positions = m_old_header_positions;
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
        g_ng_order.append(&group);
        g_newsgroup_count = NewsgroupNum{static_cast<long>(g_newsgroup_data.size())};

        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&group);
        data.dat_len = static_cast<unsigned>(group.rc_name().size());
        g_newsrc_hash->store(group.rc_name(), data);
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
            g_pushback.push(*ch);
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
    ArticleLocation                   m_old_target{};
    ArticleLocation                   m_old_recent{};
    ArticleLocation                   m_old_current{};
    HashTable                        *m_old_newsrc_hash{};
    Multirc                          *m_old_multirc{};
    DataSource                       *m_old_data_source{};
    NNTPLink                          m_old_nntp_link{};
    std::string                       m_old_newsgroup_name;
    std::string                       m_old_newsgroup_dir;
    std::string                       m_old_tmp_dir;
    std::string                       m_old_default_cmd;
    bool                              m_old_restore_old_newsrc{};
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
    std::array<HeaderPosition, HEAD_LAST> m_old_header_positions;
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

class MultircLoopContext
{
public:
    MultircLoopContext(Newsrc &newsrc, const fs::path &directory, bool save_restrictions = true);
    ~MultircLoopContext();

private:
    bool           m_save_restrictions;
    bool           m_old_univ_selector{g_use_univ_selector};
    bool           m_old_group_selector{g_use_newsgroup_selector};
    bool           m_old_paranoid{g_paranoid};
    int            m_old_find_last{g_find_last};
    NewsgroupData *m_old_start{g_start_here};
    NewsgroupData *m_old_recent{g_recent_newsgroup};
    std::string    m_old_message{g_msg};
};

MultircLoopContext::MultircLoopContext(Newsrc &newsrc, const fs::path &directory, bool save_restrictions) :
    m_save_restrictions{save_restrictions}
{
    search_init();
    if (m_save_restrictions)
    {
        g_only.push_only();
    }
    g_use_univ_selector = g_use_newsgroup_selector = false;
    g_paranoid = false;
    g_find_last = 0;
    g_start_here = g_recent_newsgroup = nullptr;
    newsrc.info_name = directory / "newsrc.info";
    newsrc.name = directory / "newsrc";
    newsrc.new_name = directory / "newsrc.new";
    std::ofstream{newsrc.name};
}

MultircLoopContext::~MultircLoopContext()
{
    if (m_save_restrictions)
    {
        pop_only();
    }
    g_use_univ_selector = m_old_univ_selector;
    g_use_newsgroup_selector = m_old_group_selector;
    g_paranoid = m_old_paranoid;
    g_find_last = m_old_find_last;
    g_start_here = m_old_start;
    g_recent_newsgroup = m_old_recent;
    g_msg = m_old_message;
}

std::vector<std::string> multirc_restrictions()
{
    std::vector<std::string> restrictions;
    for (int index = 0; index < g_only.max_to_do; ++index)
    {
        restrictions.emplace_back(g_only.restriction(index));
    }
    return restrictions;
}

struct MultircGroupPrompt
{
    std::string   group;
    ArticleUnread unread;
    MinorMode     mode;
    std::string   default_command;
    std::vector<std::string> restrictions;
    ArticleUnread            minimum_unread;
};

class MultircTraversalStateTest : public InputNewsgroupTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void run_groups(std::string commands);
    void expect_prompts(std::vector<std::string> groups, std::string_view end_default = "npq") const;

    std::vector<MultircGroupPrompt> m_prompts;
    std::string                     m_output;

private:
    TerminalInputReader m_old_input_reader{g_terminal_input_reader};
    bool                m_old_typeahead{g_allow_typeahead};
    std::optional<MultircLoopContext> m_loop_context;
    std::string         m_commands;
    std::size_t         m_input_pos{};
};

void MultircTraversalStateTest::SetUp()
{
    InputNewsgroupTest::SetUp();
    m_loop_context.emplace(m_newsrc, m_output_dir);
    g_newsgroup_data.reserve(8);
    g_ng_order.order.reserve(8);
    g_newsgroup_min_to_read = ArticleUnread{2};
    g_allow_typeahead = true;
    m_data_source.m_flags = DF_NONE;
    g_terminal_input_reader = [this](char *buffer, int)
    {
        if (g_general_mode == GM_READ)
        {
            EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
            m_prompts.push_back({g_newsgroup_ptr ? std::string{g_newsgroup_ptr->rc_name()} : "",
                                 g_newsgroup_ptr ? g_newsgroup_ptr->m_to_read : TR_NONE, g_mode, g_default_cmd,
                                 multirc_restrictions(), g_newsgroup_min_to_read});
        }
        else
        {
            EXPECT_EQ(GM_INPUT, g_general_mode);
        }
        if (m_input_pos == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected group prompt";
            *buffer = 'q';
        }
        else
        {
            *buffer = m_commands[m_input_pos++];
        }
        return 1;
    };
}

void MultircTraversalStateTest::TearDown()
{
    m_loop_context.reset();
    g_terminal_input_reader = m_old_input_reader;
    g_allow_typeahead = m_old_typeahead;
    InputNewsgroupTest::TearDown();
}

void MultircTraversalStateTest::run_groups(std::string commands)
{
    m_data_source.m_act_sf.close();
    const fs::path active_path = m_output_dir / "active";
    std::ofstream  active{active_path, std::ios::binary};
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        if (group.m_num_offset)
        {
            active << group.rc_name() << " 6 1 y\n";
        }
    }
    active.close();
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_path, "", ""));
    m_commands = std::move(commands);
    errno = 0;

    testing::internal::CaptureStdout();
    do_multirc();
    m_output = testing::internal::GetCapturedStdout();

    SCOPED_TRACE(m_output);
    EXPECT_EQ(m_commands.size(), m_input_pos);
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NONE, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_restore_old_newsrc);
}

void MultircTraversalStateTest::expect_prompts(std::vector<std::string> groups, std::string_view end_default) const
{
    SCOPED_TRACE(m_output);
    ASSERT_EQ(groups.size(), m_prompts.size());
    for (std::size_t index = 0; index < groups.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(groups[index], m_prompts[index].group);
        EXPECT_EQ(groups[index].empty() ? MM_FINISH_NEWSGROUP_LIST : MM_NEWSGROUP_LIST, m_prompts[index].mode);
        EXPECT_EQ(groups[index].empty() ? end_default : "ynq", m_prompts[index].default_command);
    }
}

class MultircRecoveryStateTest : public MultircTraversalStateTest
{
protected:
    void SetUp() override;
    void prepare_context(bool at_end, bool verbose);
    void expect_groups_preserved() const;
    void expect_visible_prompts(int count) const;

    std::array<std::string, 3> m_original_ranges;
};

void MultircRecoveryStateTest::SetUp()
{
    MultircTraversalStateTest::SetUp();
    add_newsgroup("comp.first", "1-4", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.middle", "1-3", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.last", "1-2", ArticleNum{1}, ArticleNum{6});
    for (std::size_t index = 0; index < m_original_ranges.size(); ++index)
    {
        m_original_ranges[index] = g_newsgroup_data[index].m_rc_line;
    }
}

void MultircRecoveryStateTest::prepare_context(bool at_end, bool verbose)
{
    g_verbose = verbose;
    g_start_here = &g_newsgroup_data[at_end ? 2 : 1];
    g_only.set_to_do("comp.*");
}

void MultircRecoveryStateTest::expect_groups_preserved() const
{
    for (std::size_t index = 0; index < m_original_ranges.size(); ++index)
    {
        EXPECT_EQ(m_original_ranges[index], g_newsgroup_data[index].m_rc_line);
        EXPECT_EQ(':', g_newsgroup_data[index].m_subscribe_char);
    }
    EXPECT_EQ(nullptr, g_start_here);
    EXPECT_EQ(nullptr, g_art_input.fp);
}

void MultircRecoveryStateTest::expect_visible_prompts(int count) const
{
    SCOPED_TRACE(m_output);
    EXPECT_EQ(count, std::count(m_output.begin(), m_output.end(), '?'));
}

struct MultircRecoveryInput
{
    const char *name;
    const char *command;
    bool        redraw;
    const char *message;
};

class MultircCommandRecoveryTest : public MultircRecoveryStateTest,
                                   public testing::WithParamInterface<std::tuple<MultircRecoveryInput, bool, bool>>
{
};

TEST_P(MultircCommandRecoveryTest, recoveryRetainsThePromptContextAndFollowingCommands)
{
    const auto [input, at_end, verbose] = GetParam();
    prepare_context(at_end, verbose);

    run_groups((at_end ? "n" : "") + std::string{input.command} + (at_end ? "pq" : "nq"));

    expect_prompts(at_end ? std::vector<std::string>{"comp.last", "", "", "comp.last"}
                          : std::vector<std::string>{"comp.middle", "comp.middle", "comp.last"});
    expect_visible_prompts(2 + at_end + input.redraw);
    EXPECT_NE(std::string::npos, m_output.find(input.message));
    EXPECT_EQ(std::string_view{input.command} == "@", m_output.find("Type h for help.") != std::string::npos);
    EXPECT_EQ((std::vector<std::string>{"comp.*"}), multirc_restrictions());
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(
    CommandContextAndVerbosity, MultircCommandRecoveryTest,
    testing::Combine(
        testing::Values(MultircRecoveryInput{"InvalidKey", "@", true, "Type h for help."},
                        MultircRecoveryInput{"Erase", "\b", false, ""}, MultircRecoveryInput{"Kill", "\025", false, ""},
                        MultircRecoveryInput{"Refresh", "\f", true, ""},
                        MultircRecoveryInput{"CancelledLookup", "g\b", false, ""},
                        MultircRecoveryInput{"MissingGroup", "g\n", true, "Newsgroup name or number required."},
                        MultircRecoveryInput{"CancelledSearch", "/comp.partial\025", false, ""},
                        MultircRecoveryInput{"CancelledRestriction", "O comp.*\025", false, ""}),
        testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<MultircRecoveryInput, bool, bool>> &info)
    {
        return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "End" : "Group") +
               (std::get<2>(info.param) ? "Verbose" : "Terse");
    });

class MultircPromptRecoveryTest : public MultircRecoveryStateTest,
                                  public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircPromptRecoveryTest::SetUp()
{
    MultircRecoveryStateTest::SetUp();
    prepare_context(std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(MultircPromptRecoveryTest, interruptedInputRedrawsTheSamePromptBeforeReadingTheNextCommand)
{
    const bool                at_end = std::get<0>(GetParam());
    const TerminalInputReader reader = g_terminal_input_reader;
    int                       interruptions = 0;
    g_terminal_input_reader = [&, reader](char *buffer, int count)
    {
        if (!interruptions && g_general_mode == GM_READ && (!at_end || !g_newsgroup_ptr))
        {
            EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
            EXPECT_EQ(at_end ? MM_FINISH_NEWSGROUP_LIST : MM_NEWSGROUP_LIST, g_mode);
            EXPECT_EQ(at_end ? "" : "comp.middle", g_newsgroup_ptr ? g_newsgroup_ptr->rc_name() : "");
            ++interruptions;
            errno = EINTR;
            return -1;
        }
        return reader(buffer, count);
    };

    run_groups(at_end ? "npq" : "nq");

    expect_prompts(at_end ? std::vector<std::string>{"comp.last", "", "comp.last"}
                          : std::vector<std::string>{"comp.middle", "comp.last"});
    expect_visible_prompts(3 + at_end);
    EXPECT_EQ(1, interruptions);
    EXPECT_EQ(0, errno);
    EXPECT_EQ((std::vector<std::string>{"comp.*"}), multirc_restrictions());
    expect_groups_preserved();
}

TEST_P(MultircPromptRecoveryTest, restrictionRemovalDisplaysItsMessageAndConsumesTheQueuedQuit)
{
    const bool                at_end = std::get<0>(GetParam());
    const TerminalInputReader reader = g_terminal_input_reader;
    bool                      queued = false;
    g_terminal_input_reader = [&, reader](char *buffer, int count)
    {
        const int result = reader(buffer, count);
        if (!queued && g_general_mode == GM_INPUT && *buffer == '\n')
        {
            g_pushback.push('q' | 0200);
            queued = true;
        }
        return result;
    };

    run_groups(at_end ? "no\n" : "o\n");

    expect_prompts(at_end ? std::vector<std::string>{"comp.last", ""} : std::vector<std::string>{"comp.middle"});
    expect_visible_prompts(2 + at_end);
    EXPECT_TRUE(queued);
    EXPECT_NE(std::string::npos, m_output.find(g_verbose ? "Restriction comp.* removed." : "Exiting \"only\"."));
    EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
    EXPECT_EQ("comp.first", std::string{g_newsgroup_ptr->rc_name()});
    EXPECT_TRUE(multirc_restrictions().empty());
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_min_to_read);
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(ContextAndVerbosity, MultircPromptRecoveryTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class MultircQueuedRecoveryTest : public MultircRecoveryStateTest,
                                  public testing::WithParamInterface<std::tuple<std::string, bool, bool>>
{
};

TEST_P(MultircQueuedRecoveryTest, cancellationRetainsQueuedNavigationBeforeTheFollowingTypedQuit)
{
    const auto [command, at_end, verbose] = GetParam();
    prepare_context(at_end, verbose);
    const TerminalInputReader reader = g_terminal_input_reader;
    bool                      queued = false;
    g_terminal_input_reader = [&, reader](char *buffer, int count)
    {
        const int result = reader(buffer, count);
        if (!queued && *buffer == command.back())
        {
            g_pushback.push((at_end ? 'p' : 'n') | 0200);
            queued = true;
        }
        return result;
    };

    run_groups((at_end ? "n" : "") + command + "q");

    expect_prompts(at_end ? std::vector<std::string>{"comp.last", "", "comp.last"}
                          : std::vector<std::string>{"comp.middle", "comp.last"});
    expect_visible_prompts(2 + at_end);
    EXPECT_TRUE(queued);
    EXPECT_EQ((std::vector<std::string>{"comp.*"}), multirc_restrictions());
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(CancellationContextAndVerbosity, MultircQueuedRecoveryTest,
                         testing::Combine(testing::Values(std::string{"\b"}, std::string{"g\b"},
                                                          std::string{"O comp.*\025"}),
                                          testing::Bool(), testing::Bool()));

struct MultircRefetchPolicy
{
    const char *name;
    bool        expired;
    int         interval;
};

class MultircRefetchTest : public MultircTraversalStateTest,
                           public testing::WithParamInterface<std::tuple<MultircRefetchPolicy, bool>>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_sources(bool refreshed, std::string_view first_range = " 1-6") const;

private:
    std::vector<DataSource> m_saved_sources;
    bool                    m_replaced{};
};

void MultircRefetchTest::SetUp()
{
    MultircTraversalStateTest::SetUp();
    g_verbose = std::get<1>(GetParam());
    add_newsgroup("comp.first", "1-6", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.middle", "1-6", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.last", "1-6", ArticleNum{1}, ArticleNum{6});
    m_saved_sources.swap(g_data_sources);
    g_data_sources.reserve(4);
    for (int index = 0; index < 4; ++index)
    {
        DataSource &source = g_data_sources.emplace_back();
        source.m_flags = index == 1 ? DF_OPEN : DF_OPEN | DF_ACTIVE;
        const fs::path original = m_output_dir / ("original-active-" + std::to_string(index));
        std::ofstream{original} << "comp.first 6 1 y\ncomp.middle 6 1 y\ncomp.last 6 1 y\n";
        source.m_news_id = original.generic_string();
        ASSERT_EQ(1, source.m_act_sf.open(original, "", ""));
        source.m_act_sf.m_last_fetch = 1;
        source.m_act_sf.m_refetch_secs = index == 3 ? 0 : 1;
        if (index == 2)
        {
            source.m_act_sf.close();
        }
    }
    m_newsrc.data_source = &g_data_sources.front();
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int count)
    {
        if (!m_replaced && g_mode == MM_FINISH_NEWSGROUP_LIST)
        {
            m_replaced = true;
            for (std::size_t index = 0; index < g_data_sources.size(); ++index)
            {
                const fs::path replacement = m_output_dir / ("replacement-active-" + std::to_string(index));
                std::ofstream{replacement} << "comp.first 8 1 y\ncomp.middle 8 1 y\ncomp.last 8 1 y\n";
                g_data_sources[index].m_news_id = replacement.generic_string();
            }
            const MultircRefetchPolicy &policy = std::get<0>(GetParam());
            g_data_sources[0].m_act_sf.m_last_fetch = policy.expired ? 1 : std::time(nullptr) + 3600;
            g_data_sources[0].m_act_sf.m_refetch_secs = policy.interval;
        }
        return reader(buffer, count);
    };
}

void MultircRefetchTest::TearDown()
{
    for (DataSource &source : g_data_sources)
    {
        source.close();
    }
    g_data_sources.clear();
    m_saved_sources.swap(g_data_sources);
    m_newsrc.data_source = &m_data_source;
    MultircTraversalStateTest::TearDown();
}

void MultircRefetchTest::expect_sources(bool refreshed, std::string_view first_range) const
{
    EXPECT_TRUE(m_replaced);
    for (int index : {0, 1, 3})
    {
        SCOPED_TRACE(index);
        ASSERT_EQ(3, g_data_sources[index].m_act_sf.m_lines.size());
        EXPECT_EQ(index == 0 && refreshed ? "comp.first 8 1 y\n" : "comp.first 6 1 y\n",
                  g_data_sources[index].m_act_sf.m_lines[0]);
    }
    EXPECT_EQ(nullptr, g_data_sources[2].m_act_sf.m_fp);
    EXPECT_EQ(1, g_data_sources[1].m_act_sf.m_last_fetch);
    EXPECT_EQ(1, g_data_sources[3].m_act_sf.m_last_fetch);
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        EXPECT_EQ(':', group.m_subscribe_char);
        EXPECT_EQ(group.rc_name() == "comp.first" ? first_range : " 1-6", group.rc_numbers());
    }
}

TEST_P(MultircRefetchTest, anotherTraversalOffersArrivalsOnlyWhenRefetchIsDue)
{
    const MultircRefetchPolicy &policy = std::get<0>(GetParam());
    const bool                  refreshed = policy.expired && policy.interval != 0;

    run_groups("nq");

    expect_prompts(refreshed ? std::vector<std::string>{"", "comp.first"} : std::vector<std::string>{"", ""}, "qnp");
    expect_sources(refreshed);
    EXPECT_EQ(refreshed ? ArticleUnread{2} : TR_NONE, m_prompts.back().unread);
    EXPECT_EQ(refreshed ? ArticleNum{8} : ArticleNum{6}, g_newsgroup_data[0].m_ng_max);
    EXPECT_TRUE(multirc_restrictions().empty());
}

TEST_P(MultircRefetchTest, anotherTraversalRetainsRestrictionsWhileRecalculatingUnreadCounts)
{
    const MultircRefetchPolicy &policy = std::get<0>(GetParam());
    const bool                  refreshed = policy.expired && policy.interval != 0;
    g_newsgroup_data[0].m_rc_line.resize(g_newsgroup_data[0].m_num_offset);
    g_newsgroup_data[0].m_rc_line += " 1-4";
    g_only.set_to_do("comp.first");

    run_groups("nnq");

    expect_prompts({"comp.first", "", "comp.first"});
    EXPECT_EQ(ArticleUnread{2}, m_prompts[0].unread);
    EXPECT_EQ(refreshed ? ArticleUnread{4} : ArticleUnread{2}, m_prompts.back().unread);
    EXPECT_EQ(" 1-4", g_newsgroup_data[0].rc_numbers());
    EXPECT_EQ((std::vector<std::string>{"comp.first"}), multirc_restrictions());
    for (const MultircGroupPrompt &prompt : m_prompts)
    {
        EXPECT_EQ((std::vector<std::string>{"comp.first"}), prompt.restrictions);
        EXPECT_EQ(ArticleUnread{2}, prompt.minimum_unread);
    }
    expect_sources(refreshed, " 1-4");
}

INSTANTIATE_TEST_SUITE_P(IntervalAndVerbosity, MultircRefetchTest,
                         testing::Combine(testing::Values(MultircRefetchPolicy{"Expired", true, 1},
                                                          MultircRefetchPolicy{"Fresh", false, 3600},
                                                          MultircRefetchPolicy{"Disabled", true, 0}),
                                          testing::Bool()),
                         [](const testing::TestParamInfo<std::tuple<MultircRefetchPolicy, bool>> &info)
                         {
                             return std::string{std::get<0>(info.param).name} +
                                    (std::get<1>(info.param) ? "Verbose" : "Terse");
                         });

struct IneligibleMultircGroup
{
    const char   *name;
    const char   *read_ranges;
    ArticleUnread cached_count;
    char          subscription;
    ArticleUnread expected_count;
};

class MultircIneligibleGroupTest : public MultircTraversalStateTest,
                                   public testing::WithParamInterface<std::tuple<IneligibleMultircGroup, bool>>
{
protected:
    void SetUp() override;
    void expect_skipped_groups() const;
};

void MultircIneligibleGroupTest::SetUp()
{
    MultircTraversalStateTest::SetUp();
    const auto [skipped, verbose] = GetParam();
    g_verbose = verbose;
    add_newsgroup("comp.leading", skipped.read_ranges, ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.first", "1-4", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.middle", skipped.read_ranges, ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.last", "1-3", ArticleNum{1}, ArticleNum{6});
    for (int index : {0, 2})
    {
        NewsgroupData &group = g_newsgroup_data[index];
        group.m_to_read = skipped.cached_count;
        group.m_subscribe_char = skipped.subscription;
        if (skipped.cached_count == TR_JUNK)
        {
            group.m_rc_line = "options -x";
            group.m_num_offset = 0;
        }
    }
}

void MultircIneligibleGroupTest::expect_skipped_groups() const
{
    const IneligibleMultircGroup &skipped = std::get<0>(GetParam());
    for (int index : {0, 2})
    {
        EXPECT_EQ(skipped.expected_count, g_newsgroup_data[index].m_to_read);
        EXPECT_EQ(skipped.subscription, g_newsgroup_data[index].m_subscribe_char);
    }
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(ArticleUnread{3}, g_newsgroup_data[3].m_to_read);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
}

TEST_P(MultircIneligibleGroupTest, forwardTraversalSkipsIneligibleGroupsAndStopsAtTheEndPrompt)
{
    run_groups("nnq");

    expect_prompts({"comp.first", "comp.last", ""});
    EXPECT_EQ(ArticleUnread{2}, m_prompts[0].unread);
    EXPECT_EQ(ArticleUnread{3}, m_prompts[1].unread);
    expect_skipped_groups();
}

TEST_P(MultircIneligibleGroupTest, backwardTraversalSkipsIneligibleGroupsAndRetainsTheFirstEligibleGroup)
{
    run_groups("nppq");

    expect_prompts({"comp.first", "comp.last", "comp.first", "comp.first"});
    expect_skipped_groups();
}

INSTANTIATE_TEST_SUITE_P(
    EligibilityAndVerbosity, MultircIneligibleGroupTest,
    testing::Combine(testing::Values(IneligibleMultircGroup{"FullyRead", "1-6", TR_NONE, ':', TR_NONE},
                                     IneligibleMultircGroup{"BelowThreshold", "1-5", TR_NONE, ':', ArticleUnread{1}},
                                     IneligibleMultircGroup{"Unsubscribed", "1", TR_UNSUB, '!', TR_UNSUB},
                                     IneligibleMultircGroup{"Bogus", "1", TR_BOGUS, ':', TR_BOGUS},
                                     IneligibleMultircGroup{"Ignored", "1", TR_IGNORE, ':', TR_IGNORE},
                                     IneligibleMultircGroup{"Junk", "", TR_JUNK, ':', TR_JUNK}),
                     testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<IneligibleMultircGroup, bool>> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Verbose" : "Terse"); });

class MultircGroupTraversalTest : public MultircTraversalStateTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void MultircGroupTraversalTest::SetUp()
{
    MultircTraversalStateTest::SetUp();
    g_verbose = GetParam();
}

TEST_P(MultircGroupTraversalTest, restrictionsSkipNonmatchingGroupsInBothDirections)
{
    add_newsgroup("comp.excluded.first", "1", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.match.first", "1-4", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.excluded.middle", "1", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.match.last", "1-3", ArticleNum{1}, ArticleNum{6});
    g_only.set_to_do("comp.match.*");

    run_groups("npq");

    expect_prompts({"comp.match.first", "comp.match.last", "comp.match.first"});
    EXPECT_EQ(TR_NONE, g_newsgroup_data[0].m_to_read);
    EXPECT_EQ(TR_NONE, g_newsgroup_data[2].m_to_read);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(1, g_only.max_to_do);
}

TEST_P(MultircGroupTraversalTest, previousAtTheFirstGroupRetainsItsPrompt)
{
    add_newsgroup("comp.first", "1-4", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.last", "1-3", ArticleNum{1}, ArticleNum{6});

    run_groups("pq");

    expect_prompts({"comp.first", "comp.first"});
    EXPECT_EQ(ArticleUnread{2}, m_prompts[0].unread);
    EXPECT_EQ(m_prompts[0].unread, m_prompts[1].unread);
}

TEST_P(MultircGroupTraversalTest, recalculationDiscoversArrivalsThatMeetTheUnreadThreshold)
{
    add_newsgroup("comp.growing", "1-2");
    g_newsgroup_data[0].m_to_read = ArticleUnread{1};

    run_groups("nq");

    expect_prompts({"comp.growing", ""});
    EXPECT_EQ(ArticleUnread{4}, m_prompts[0].unread);
    EXPECT_EQ(ArticleNum{6}, g_newsgroup_data[0].m_ng_max);
    EXPECT_EQ(" 1-2", g_newsgroup_data[0].rc_numbers());
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
}

INSTANTIATE_TEST_SUITE_P(Verbosity, MultircGroupTraversalTest, testing::Bool());

class MultircStartupStateTest : public MultircTraversalStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_groups();
    void set_group_state(int index, std::string_view read_ranges, char subscription = ':');

private:
    std::string m_old_last_group{g_last_run.newsgroup_name};
};

void MultircStartupStateTest::SetUp()
{
    MultircTraversalStateTest::SetUp();
    g_last_run.newsgroup_name.clear();
}

void MultircStartupStateTest::TearDown()
{
    g_last_run.newsgroup_name = std::move(m_old_last_group);
    MultircTraversalStateTest::TearDown();
}

void MultircStartupStateTest::prepare_groups()
{
    add_newsgroup("comp.first", "1-4", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.middle", "1-3", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.last", "1-2", ArticleNum{1}, ArticleNum{6});
}

void MultircStartupStateTest::set_group_state(int index, std::string_view read_ranges, char subscription)
{
    NewsgroupData &group = g_newsgroup_data[index];
    group.m_rc_line.resize(group.m_num_offset);
    group.m_rc_line += ' ';
    group.m_rc_line += read_ranges;
    group.m_subscribe_char = subscription;
    group.m_to_read = subscription == UNSUBSCRIBED_CHAR ? TR_UNSUB : TR_NONE;
}

struct MultircStartPosition
{
    const char   *name;
    int           group;
    const char   *read_ranges;
    char          subscription;
    int           expected_group;
    ArticleUnread expected_unread;
};

class MultircExplicitStartTest : public MultircStartupStateTest,
                                 public testing::WithParamInterface<std::tuple<MultircStartPosition, bool>>
{
protected:
    void SetUp() override;
};

void MultircExplicitStartTest::SetUp()
{
    MultircStartupStateTest::SetUp();
    prepare_groups();
    const MultircStartPosition &position = std::get<0>(GetParam());
    g_verbose = std::get<1>(GetParam());
    set_group_state(position.group, position.read_ranges, position.subscription);
    g_start_here = &g_newsgroup_data[position.group];
}

TEST_P(MultircExplicitStartTest, startupOffersTheFirstEligibleGroupAtOrAfterTheStartingPosition)
{
    run_groups("q");

    const MultircStartPosition &position = std::get<0>(GetParam());
    expect_prompts({std::string{g_newsgroup_data[position.expected_group].rc_name()}});
    EXPECT_EQ(position.expected_unread, m_prompts[0].unread);
    EXPECT_EQ(nullptr, g_start_here);
    EXPECT_EQ(0, g_find_last);
    EXPECT_EQ(position.subscription, g_newsgroup_data[position.group].m_subscribe_char);
    EXPECT_EQ(" " + std::string{position.read_ranges}, g_newsgroup_data[position.group].rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(
    PositionAndVerbosity, MultircExplicitStartTest,
    testing::Combine(testing::Values(MultircStartPosition{"First", 0, "1-4", ':', 0, ArticleUnread{2}},
                                     MultircStartPosition{"Middle", 1, "1-3", ':', 1, ArticleUnread{3}},
                                     MultircStartPosition{"Last", 2, "1-2", ':', 2, ArticleUnread{4}},
                                     MultircStartPosition{"ReadMiddle", 1, "1-6", ':', 2, ArticleUnread{4}},
                                     MultircStartPosition{"UnsubscribedMiddle", 1, "1-3", '!', 2, ArticleUnread{4}},
                                     MultircStartPosition{"BelowThresholdMiddle", 1, "1-5", ':', 2, ArticleUnread{4}}),
                     testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<MultircStartPosition, bool>> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Verbose" : "Terse"); });

struct MultircResumePosition
{
    const char   *name;
    const char   *last_group;
    const char   *middle_read_ranges;
    char          middle_subscription;
    int           expected_group;
    ArticleUnread expected_unread;
};

class MultircResumeTest : public MultircStartupStateTest,
                          public testing::WithParamInterface<std::tuple<MultircResumePosition, bool>>
{
protected:
    void SetUp() override;
};

void MultircResumeTest::SetUp()
{
    MultircStartupStateTest::SetUp();
    prepare_groups();
    const MultircResumePosition &position = std::get<0>(GetParam());
    g_verbose = std::get<1>(GetParam());
    set_group_state(1, position.middle_read_ranges, position.middle_subscription);
    g_last_run.newsgroup_name = position.last_group;
    g_find_last = 1;
    g_start_here = &g_newsgroup_data[0];
}

TEST_P(MultircResumeTest, startupResumesAnUnreadGroupOrFallsBackToTheFirstEligibleGroup)
{
    run_groups("q");

    const MultircResumePosition &position = std::get<0>(GetParam());
    expect_prompts({std::string{g_newsgroup_data[position.expected_group].rc_name()}});
    EXPECT_EQ(position.expected_unread, m_prompts[0].unread);
    EXPECT_EQ(-1, g_find_last);
    EXPECT_EQ(nullptr, g_start_here);
    EXPECT_EQ(position.middle_subscription, g_newsgroup_data[1].m_subscribe_char);
    EXPECT_EQ(" " + std::string{position.middle_read_ranges}, g_newsgroup_data[1].rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(
    SavedGroupAndVerbosity, MultircResumeTest,
    testing::Combine(
        testing::Values(MultircResumePosition{"UnreadMiddle", "comp.middle", "1-3", ':', 1, ArticleUnread{3}},
                        MultircResumePosition{"UnreadLast", "comp.last", "1-3", ':', 2, ArticleUnread{4}},
                        MultircResumePosition{"ReadMiddle", "comp.middle", "1-6", ':', 0, ArticleUnread{2}},
                        MultircResumePosition{"UnsubscribedMiddle", "comp.middle", "1-3", '!', 0, ArticleUnread{2}},
                        MultircResumePosition{"Missing", "comp.removed", "1-3", ':', 0, ArticleUnread{2}},
                        MultircResumePosition{"NoSavedGroup", "", "1-3", ':', 0, ArticleUnread{2}}),
        testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<MultircResumePosition, bool>> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Verbose" : "Terse"); });

class MultircStartupTest : public MultircStartupStateTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void MultircStartupTest::SetUp()
{
    MultircStartupStateTest::SetUp();
    g_verbose = GetParam();
    prepare_groups();
}

TEST_P(MultircStartupTest, restartingAfterAnExplicitStartReturnsToEarlierUnreadNews)
{
    g_start_here = &g_newsgroup_data[1];

    run_groups("nnnq");

    expect_prompts({"comp.middle", "comp.last", "", "comp.first"});
    EXPECT_EQ(ArticleUnread{2}, m_prompts.back().unread);
    EXPECT_EQ(nullptr, g_start_here);
}

TEST_P(MultircStartupTest, restartingAfterResumingReturnsToEarlierUnreadNews)
{
    g_find_last = 1;
    g_last_run.newsgroup_name = "comp.middle";
    g_start_here = &g_newsgroup_data[0];

    run_groups("nnnq");

    expect_prompts({"comp.middle", "comp.last", "", "comp.first"});
    EXPECT_EQ(ArticleUnread{2}, m_prompts.back().unread);
    EXPECT_EQ(-1, g_find_last);
    EXPECT_EQ(nullptr, g_start_here);
}

TEST_P(MultircStartupTest, aFullyReadCatalogOffersTheEndPromptWithoutChangingReadRanges)
{
    for (int index = 0; index < 3; ++index)
    {
        set_group_state(index, "1-6");
    }
    g_start_here = &g_newsgroup_data.back();

    run_groups("q");

    expect_prompts({""}, "qnp");
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        EXPECT_EQ(TR_NONE, group.m_to_read);
        EXPECT_EQ(" 1-6", group.rc_numbers());
        EXPECT_EQ(':', group.m_subscribe_char);
    }
    EXPECT_EQ(nullptr, g_start_here);
}

INSTANTIATE_TEST_SUITE_P(Verbosity, MultircStartupTest, testing::Bool());

class MultircEmptyStartupTest : public MultircStartupStateTest,
                                public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircEmptyStartupTest::SetUp()
{
    MultircStartupStateTest::SetUp();
    const auto [resume, verbose] = GetParam();
    g_find_last = resume ? 1 : 0;
    g_verbose = verbose;
}

TEST_P(MultircEmptyStartupTest, nextOnAnEmptyCatalogRepeatsTheEndPromptAndAllowsQuit)
{
    run_groups("nq");

    expect_prompts({"", ""}, "qnp");
    EXPECT_TRUE(g_newsgroup_data.empty());
    EXPECT_TRUE(g_ng_order.order.empty());
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_count);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(nullptr, g_current_newsgroup);
    EXPECT_EQ(nullptr, g_start_here);
    EXPECT_EQ(std::get<0>(GetParam()) ? -1 : 0, g_find_last);
}

INSTANTIATE_TEST_SUITE_P(ResumeAndVerbosity, MultircEmptyStartupTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class MultircRestrictionStateTest : public MultircTraversalStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_groups_preserved() const;
    void expect_exhaustion(std::string_view pattern) const;

private:
    char                     m_old_empty_only_char{g_empty_only_char};
    std::vector<std::string> m_original_lines;
};

void MultircRestrictionStateTest::SetUp()
{
    MultircTraversalStateTest::SetUp();
    g_empty_only_char = 'o';
    g_newsgroup_min_to_read = TR_ONE;
    add_newsgroup("comp.first", "1-2", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.match.first", "1-3", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.match.last", "1-4", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.last", "1-5", ArticleNum{1}, ArticleNum{6});
    add_newsgroup("comp.empty", "1-6", ArticleNum{1}, ArticleNum{6});
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        m_original_lines.push_back(group.m_rc_line);
    }
}

void MultircRestrictionStateTest::TearDown()
{
    g_empty_only_char = m_old_empty_only_char;
    MultircTraversalStateTest::TearDown();
}

void MultircRestrictionStateTest::expect_groups_preserved() const
{
    ASSERT_EQ(m_original_lines.size(), g_newsgroup_data.size());
    for (std::size_t index = 0; index < m_original_lines.size(); ++index)
    {
        EXPECT_EQ(m_original_lines[index], g_newsgroup_data[index].m_rc_line) << index;
        EXPECT_EQ(':', g_newsgroup_data[index].m_subscribe_char) << index;
    }
}

void MultircRestrictionStateTest::expect_exhaustion(std::string_view pattern) const
{
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "No articles under restriction." : "No \"only\" articles."));
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "Restriction " + std::string{pattern} + " removed."
                                                       : "Exiting \"only\"."));
}

class MultircRetainedRestrictionTest : public MultircRestrictionStateTest,
                                       public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircRetainedRestrictionTest::SetUp()
{
    MultircRestrictionStateTest::SetUp();
    const auto [multiple, verbose] = GetParam();
    g_verbose = verbose;
    if (multiple)
    {
        g_only.set_to_do("comp.match.first");
        g_only.set_to_do("comp.match.last");
    }
    else
    {
        g_only.set_to_do("comp.match.*");
    }
}

TEST_P(MultircRetainedRestrictionTest, skippingMatchingUnreadGroupsRetainsTheRestrictionForAnotherTraversal)
{
    const std::vector<std::string> patterns = multirc_restrictions();

    run_groups("nnnq");

    expect_prompts({"comp.match.first", "comp.match.last", "", "comp.match.first"});
    for (const MultircGroupPrompt &prompt : m_prompts)
    {
        EXPECT_EQ(patterns, prompt.restrictions);
        EXPECT_EQ(TR_ONE, prompt.minimum_unread);
    }
    const std::string warning =
        g_verbose ? "Restriction " + patterns.front() + (patterns.size() > 1 ? ", etc." : "") + " still in effect."
                  : "(\"Only\" mode.)";
    EXPECT_THAT(m_output, testing::HasSubstr(warning));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("removed.")));
    EXPECT_EQ(patterns, multirc_restrictions());
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(PatternCountAndVerbosity, MultircRetainedRestrictionTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class MultircExhaustedRestrictionTest : public MultircRestrictionStateTest,
                                        public testing::WithParamInterface<std::tuple<const char *, bool>>
{
protected:
    void SetUp() override;
};

void MultircExhaustedRestrictionTest::SetUp()
{
    MultircRestrictionStateTest::SetUp();
    const auto [pattern, verbose] = GetParam();
    g_verbose = verbose;
    g_only.set_to_do(pattern);
}

TEST_P(MultircExhaustedRestrictionTest, anExhaustedRestrictionIsReleasedBeforeRestartingWithOtherUnreadGroups)
{
    run_groups("nq");

    expect_prompts({"", "comp.first"});
    EXPECT_TRUE(m_prompts[0].restrictions.empty());
    EXPECT_TRUE(m_prompts[1].restrictions.empty());
    EXPECT_EQ(ArticleUnread{4}, m_prompts[1].unread);
    EXPECT_EQ(0, g_only.max_to_do);
    EXPECT_EQ(TR_ONE, g_newsgroup_min_to_read);
    expect_exhaustion(std::get<0>(GetParam()));
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(MissingOrReadGroupAndVerbosity, MultircExhaustedRestrictionTest,
                         testing::Combine(testing::Values("soc.missing", "comp.empty"), testing::Bool()));

class MultircRestrictionTest : public MultircRestrictionStateTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void MultircRestrictionTest::SetUp()
{
    MultircRestrictionStateTest::SetUp();
    g_verbose = GetParam();
    g_only.set_to_do("comp.match.*");
}

TEST_P(MultircRestrictionTest, aBareRestrictionCommandReturnsToEarlierUnrestrictedNews)
{
    run_groups("o\nq");

    expect_prompts({"comp.match.first", "comp.first"});
    EXPECT_EQ(std::vector<std::string>{"comp.match.*"}, m_prompts[0].restrictions);
    EXPECT_TRUE(m_prompts[1].restrictions.empty());
    EXPECT_EQ(0, g_only.max_to_do);
    EXPECT_EQ(TR_ONE, m_prompts[1].minimum_unread);
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "Restriction comp.match.* removed." : "Exiting \"only\"."));
    expect_groups_preserved();
}

TEST_P(MultircRestrictionTest, aNewRestrictionReplacesThePreviousPatternAndRestartsTraversal)
{
    run_groups("O comp.last\nq");

    expect_prompts({"comp.match.first", "comp.last"});
    EXPECT_EQ(std::vector<std::string>{"comp.match.*"}, m_prompts[0].restrictions);
    EXPECT_EQ(std::vector<std::string>{"comp.last"}, m_prompts[1].restrictions);
    EXPECT_EQ(std::vector<std::string>{"comp.last"}, multirc_restrictions());
    EXPECT_EQ(TR_ONE, m_prompts[1].minimum_unread);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[1].unread);
    expect_groups_preserved();
}

TEST_P(MultircRestrictionTest, multipleNewPatternsReplaceThePreviousRestrictionWithoutLosingEitherMatch)
{
    run_groups("O comp.first comp.last\nnnq");

    expect_prompts({"comp.match.first", "comp.first", "comp.last", ""});
    const std::vector<std::string> patterns{"comp.first", "comp.last"};
    EXPECT_EQ(patterns, m_prompts[1].restrictions);
    EXPECT_EQ(patterns, m_prompts[2].restrictions);
    EXPECT_EQ(patterns, m_prompts[3].restrictions);
    EXPECT_EQ(patterns, multirc_restrictions());
    EXPECT_THAT(m_output,
                testing::HasSubstr(g_verbose ? "Restriction comp.first, etc. still in effect." : "(\"Only\" mode.)"));
    expect_groups_preserved();
}

TEST_P(MultircRestrictionTest, aNewRestrictionWithNoMatchesReleasesAndResumesUnrestrictedTraversal)
{
    run_groups("O soc.missing\nnq");

    expect_prompts({"comp.match.first", "", "comp.first"});
    EXPECT_EQ(std::vector<std::string>{"comp.match.*"}, m_prompts[0].restrictions);
    EXPECT_TRUE(m_prompts[1].restrictions.empty());
    EXPECT_TRUE(m_prompts[2].restrictions.empty());
    EXPECT_EQ(0, g_only.max_to_do);
    expect_exhaustion("soc.missing");
    expect_groups_preserved();
}

TEST_P(MultircRestrictionTest, aNewRestrictionAtTheEndOffersItsMatchingGroup)
{
    run_groups("nnO comp.last\nq");

    expect_prompts({"comp.match.first", "comp.match.last", "", "comp.last"});
    EXPECT_EQ(std::vector<std::string>{"comp.match.*"}, m_prompts[2].restrictions);
    EXPECT_EQ(std::vector<std::string>{"comp.last"}, m_prompts[3].restrictions);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[3].unread);
    EXPECT_EQ(std::vector<std::string>{"comp.last"}, multirc_restrictions());
    expect_groups_preserved();
}

TEST_P(MultircRestrictionTest, clearingTheRestrictionAtTheEndOffersEarlierUnrestrictedNews)
{
    run_groups("nno\nq");

    expect_prompts({"comp.match.first", "comp.match.last", "", "comp.first"});
    EXPECT_EQ(std::vector<std::string>{"comp.match.*"}, m_prompts[2].restrictions);
    EXPECT_TRUE(m_prompts[3].restrictions.empty());
    EXPECT_EQ(ArticleUnread{4}, m_prompts[3].unread);
    EXPECT_EQ(0, g_only.max_to_do);
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "Restriction comp.match.* removed." : "Exiting \"only\"."));
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(Verbosity, MultircRestrictionTest, testing::Bool());

class MultircRestrictionScopeTest : public MultircRestrictionStateTest,
                                    public testing::WithParamInterface<std::tuple<char, bool>>
{
protected:
    void SetUp() override;
};

void MultircRestrictionScopeTest::SetUp()
{
    MultircRestrictionStateTest::SetUp();
    g_verbose = std::get<1>(GetParam());
    g_only.set_to_do("comp.match.*");
}

TEST_P(MultircRestrictionScopeTest, theRestrictionCommandControlsWhetherMatchingReadGroupsAreOffered)
{
    const char        command = std::get<0>(GetParam());
    const bool        include_read = command == 'o';
    const std::string input = std::string{command} + " comp.*\n" + (include_read ? "nnnnnq" : "nnnnq");

    run_groups(input);

    std::vector<std::string> groups{"comp.match.first", "comp.first", "comp.match.first", "comp.match.last",
                                    "comp.last"};
    if (include_read)
    {
        groups.emplace_back("comp.empty");
    }
    groups.emplace_back();
    expect_prompts(groups);
    for (std::size_t index = 1; index < m_prompts.size(); ++index)
    {
        EXPECT_EQ(std::vector<std::string>{"comp.*"}, m_prompts[index].restrictions) << index;
        EXPECT_EQ(include_read ? TR_NONE : TR_ONE, m_prompts[index].minimum_unread) << index;
    }
    EXPECT_EQ(std::vector<std::string>{"comp.*"}, multirc_restrictions());
    expect_groups_preserved();
}

INSTANTIATE_TEST_SUITE_P(CommandAndVerbosity, MultircRestrictionScopeTest,
                         testing::Combine(testing::Values('o', 'O'), testing::Bool()));

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

class InputNewsgroupConfirmationTest : public InputNewsgroupNavigationTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_exit_origin(bool at_end);
    void expect_changes_preserved();
    void expect_files_unchanged();
    void prompt_answer(std::string_view answer);

private:
    int  m_saved_stdin{-1};
    bool m_old_allow_typeahead{g_allow_typeahead};
};

void InputNewsgroupConfirmationTest::SetUp()
{
    InputNewsgroupNavigationTest::SetUp();
    prepare_groups();
    for (NewsgroupData &group : g_newsgroup_data)
    {
        group.set_to_read(ST_LAX);
    }
    g_newsgroup_to_read = NewsgroupNum{3};
    m_newsrc.name = m_output_dir / "newsrc";
    m_newsrc.old_name = m_output_dir / "old-newsrc";
    m_newsrc.new_name = m_output_dir / "newsrc.new";
    m_newsrc.flags |= RF_RC_CHANGED;
    std::ofstream{m_newsrc.name} << "comp.alpha: 1-2\ncomp.bravo: 1-2\ncomp.charlie: 1-2\n";
    std::ofstream{m_newsrc.old_name} << "comp.alpha: 1\ncomp.bravo: 1\ncomp.charlie: 1\n";
    g_mode = MM_NEWSGROUP_LIST;
}

void InputNewsgroupConfirmationTest::TearDown()
{
    if (m_saved_stdin >= 0)
    {
        EXPECT_GE(dup2(m_saved_stdin, fileno(stdin)), 0);
        close(m_saved_stdin);
        std::clearerr(stdin);
    }
    g_allow_typeahead = m_old_allow_typeahead;
    InputNewsgroupNavigationTest::TearDown();
}

void InputNewsgroupConfirmationTest::set_exit_origin(bool at_end)
{
    if (at_end)
    {
        g_newsgroup_ptr = nullptr;
        g_current_newsgroup = nullptr;
    }
}

void InputNewsgroupConfirmationTest::expect_changes_preserved()
{
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        EXPECT_EQ(" 1-2", group.rc_numbers());
        EXPECT_EQ(ArticleUnread{1}, group.m_to_read);
        EXPECT_EQ(':', group.m_subscribe_char);
    }
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

void InputNewsgroupConfirmationTest::expect_files_unchanged()
{
    std::ifstream current{m_newsrc.name};
    std::ifstream previous{m_newsrc.old_name};
    EXPECT_EQ("comp.alpha: 1-2\ncomp.bravo: 1-2\ncomp.charlie: 1-2\n",
              (std::string{std::istreambuf_iterator<char>{current}, std::istreambuf_iterator<char>{}}));
    EXPECT_EQ("comp.alpha: 1\ncomp.bravo: 1\ncomp.charlie: 1\n",
              (std::string{std::istreambuf_iterator<char>{previous}, std::istreambuf_iterator<char>{}}));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(macro_pending());
}

void InputNewsgroupConfirmationTest::prompt_answer(std::string_view answer)
{
    const fs::path input_file = m_output_dir / "prompt-input";
    std::ofstream{input_file} << answer;
    std::FILE *fp = std::fopen(input_file.string().c_str(), "r");
    ASSERT_NE(nullptr, fp);
    m_saved_stdin = dup(fileno(stdin));
    EXPECT_GE(m_saved_stdin, 0);
    EXPECT_GE(dup2(fileno(fp), fileno(stdin)), 0);
    std::fclose(fp);
    std::clearerr(stdin);
    g_allow_typeahead = true;
}

using NewsgroupExitInput = std::tuple<char, bool>;

class InputNewsgroupQuitTest : public InputNewsgroupConfirmationTest,
                               public testing::WithParamInterface<NewsgroupExitInput>
{
};

class InputNewsgroupExitCancelTest : public InputNewsgroupConfirmationTest,
                                     public testing::WithParamInterface<NewsgroupExitInput>
{
};

class InputNewsgroupExitConfirmTest : public InputNewsgroupConfirmationTest,
                                      public testing::WithParamInterface<NewsgroupExitInput>
{
};

class InputNewsgroupAbandonCancelTest : public InputNewsgroupConfirmationTest,
                                        public testing::WithParamInterface<NewsgroupExitInput>
{
};

class InputNewsgroupAbandonConfirmTest : public InputNewsgroupConfirmationTest,
                                         public testing::WithParamInterface<NewsgroupExitInput>
{
};

class InputNewsgroupAbandonPromptTest : public InputNewsgroupConfirmationTest, public testing::WithParamInterface<bool>
{
};

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
    bool                       m_old_sel_rereading{g_sel.rereading};
    int                        m_old_perform_count{g_status.perform_count};
    bool                       m_old_error_occurred{g_error_occurred};
    std::optional<std::string> m_old_subj_line{g_subj_line};
    int                        m_old_page_line{g_page_line};
    bool                       m_old_performed_article_loop{g_status.performed_article_loop};
    int                        m_old_spin_count{g_progress.count};
};

void InputNewsgroupSearchTest::SetUp()
{
    InputNewsgroupNavigationTest::SetUp();
    prepare_groups();
    m_old_add_groups = std::move(g_add_groups);
    g_add_groups = {};
    g_sel.rereading = false;
    search_init();
    newsgroup_search_init();
}

void InputNewsgroupSearchTest::TearDown()
{
    newsgroup_search_init();
    g_add_groups = std::move(m_old_add_groups);
    g_sel.rereading = m_old_sel_rereading;
    g_status.perform_count = m_old_perform_count;
    g_error_occurred = m_old_error_occurred;
    g_subj_line = m_old_subj_line;
    g_page_line = m_old_page_line;
    g_status.performed_article_loop = m_old_performed_article_loop;
    g_progress.count = m_old_spin_count;
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
    Subject           *m_old_first_subject{g_subj.first};
    Subject           *m_old_last_subject{g_subj.last};
    HashTable         *m_old_msg_id_hash{g_msg_id_hash};
    int                m_old_subject_count{g_subj.count};
    ArticleNum         m_old_obj_count{g_obj_count};
    SelectionMode      m_old_sel_mode{g_sel.mode};
    SelectionSortMode  m_old_sel_sort{g_sel.sort};
    SelectionSortMode  m_old_sel_art_sort{g_sel.art_sort};
    std::string_view   m_old_sel_mode_string{g_sel.mode_string};
    std::string_view   m_old_sel_sort_string{g_sel.sort_string};
    int                m_old_sel_direction{g_sel.direction};
    AddGroupFlags      m_old_sel_mask{g_sel.mask};
    bool               m_old_sel_rereading{g_sel.rereading};
    bool               m_old_sel_exclusive{g_sel.exclusive};
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
    g_subj.first = g_subj.last = nullptr;
    g_msg_id_hash = nullptr;
    g_subj.count = 0;
    g_obj_count = ArticleNum{};
    g_sel.mode = SM_ARTICLE;
    g_sel.art_sort = SS_NATURAL;
    g_sel.mask = AGF_SEL;
    g_sel.rereading = g_sel.exclusive = false;
    g_selected_subj_cnt = 0;
    g_kf_state = KFS_NONE;
    g_in_ng = true;
    g_mode = MM_ARTICLE;
    g_reread = g_force_last = g_do_fseek = false;
    g_erase_each_line = false;
    g_art_input.line_num = ArticleLine{6};
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
    delete g_msg_id_hash;
    g_msg_id_hash = m_old_msg_id_hash;
    g_subj.first = m_old_first_subject;
    g_subj.last = m_old_last_subject;
    g_subj.count = m_old_subject_count;
    g_obj_count = m_old_obj_count;
    g_sel.mode = m_old_sel_mode;
    g_sel.sort = m_old_sel_sort;
    g_sel.art_sort = m_old_sel_art_sort;
    g_sel.mode_string = m_old_sel_mode_string;
    g_sel.sort_string = m_old_sel_sort_string;
    g_sel.direction = m_old_sel_direction;
    g_sel.mask = m_old_sel_mask;
    g_sel.rereading = m_old_sel_rereading;
    g_sel.exclusive = m_old_sel_exclusive;
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
    g_nav.target.num = g_nav.current.num = ArticleNum{1};
    g_nav.target.art = g_nav.current.art = g_cache.ptr(g_nav.target.num);
    g_nav.recent.num = ArticleNum{2};
    g_nav.recent.art = g_cache.ptr(g_nav.recent.num);
    ASSERT_TRUE(parse_header(g_nav.current.num));
    ASSERT_NE(nullptr, art_open(g_nav.current.num, g_parsed_header.positions[PAST_HEADER].min_pos));
    std::string line;
    ASSERT_TRUE(read_art(line));
    ASSERT_EQ("Root first line\n", line);
    g_art_input.pos = tell_art();
    ASSERT_FALSE(g_threaded_group);
    ASSERT_EQ(AF_NONE, g_nav.current.art->m_flags & AF_THREADED);
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
    std::string                        m_old_sel_chars{g_sel_page.chars};
    SelectionMode                      m_old_default_mode{g_sel.default_mode};
    ArticleLine                        m_old_init_lines{g_init_lines};
    Article                          **m_old_page_app{g_sel_page.page_art};
    Article                          **m_old_next_app{g_sel_page.next_art};
    Article                           *m_old_last_ap{g_sel_page.last_art};
    Subject                           *m_old_page_sp{g_sel_page.page_subj};
    Subject                           *m_old_next_sp{g_sel_page.next_subj};
    Subject                           *m_old_last_sp{g_sel_page.last_subj};
    int                                m_old_total{g_sel_page.total_obj_cnt};
    int                                m_old_prior{g_sel_page.prior_obj_cnt};
    int                                m_old_page_objects{g_sel_page.page_obj_cnt};
    int                                m_old_page_items{g_sel_page.page_item_cnt};
    int                                m_old_item_index{g_sel_page.item_index};
    int                                m_old_last_line{g_sel_page.last_line};
    bool                               m_old_at_end{g_sel_page.at_end};
    std::array<SelectionItem, MAX_SEL> m_old_items;
    decltype(g_missing_count)          m_old_missing_count{g_missing_count};
    int                                m_old_dm_count{g_dm_count};
};

void InputNewsgroupReadingTest::SetUp()
{
    std::copy(std::begin(g_sel_page.items), std::end(g_sel_page.items), m_old_items.begin());
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
    g_sel_page.chars = "abcdefghijklmnopqrstuvwxyz";
    g_sel.default_mode = SM_ARTICLE;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_sel_page.page_art = g_sel_page.next_art = nullptr;
    g_sel_page.last_art = nullptr;
    g_sel_page.page_subj = g_sel_page.next_subj = g_sel_page.last_subj = nullptr;
}

void InputNewsgroupReadingTest::TearDown()
{
    LocalSpoolArticleTest::TearDown();
    g_doing_ng = m_old_doing_ng;
    g_newsgroup_exit_code = m_old_exit_code;
    g_sel_page.chars = m_old_sel_chars;
    g_sel.default_mode = m_old_default_mode;
    g_init_lines = m_old_init_lines;
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
    g_sel_page.item_index = m_old_item_index;
    g_sel_page.last_line = m_old_last_line;
    g_sel_page.at_end = m_old_at_end;
    std::copy(m_old_items.begin(), m_old_items.end(), std::begin(g_sel_page.items));
    g_missing_count = m_old_missing_count;
    g_dm_count = m_old_dm_count;
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

struct MultircReadingPrompt
{
    int                        group;
    int                        recent_group;
    MinorMode                  mode;
    ArticleNum                 article;
    ArticleUnread              unread;
    std::string                default_command;
    std::array<std::string, 3> read_ranges;
    std::vector<std::string>   restrictions;
};

class MultircReadingStateTest : public InputNewsgroupReadingTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void run_session(std::string commands, bool restore_old = false);
    void expect_prompts(std::vector<std::pair<int, MinorMode>> prompts) const;
    void expect_body_order(std::vector<std::string> bodies) const;

    std::vector<MultircReadingPrompt> m_prompts;

private:
    std::optional<MultircLoopContext> m_loop_context;
    TerminalInputReader               m_old_input_reader{g_terminal_input_reader};
    bool                              m_old_typeahead{g_allow_typeahead};
    std::string                       m_commands;
    std::size_t                       m_input_pos{};
};

void MultircReadingStateTest::SetUp()
{
    InputNewsgroupReadingTest::SetUp();
    m_loop_context.emplace(m_newsrc, m_output_dir);
    g_thread_always = true;
    g_newsgroup_min_to_read = TR_ONE;
    g_allow_typeahead = true;
    for (const char *directory : {"comp/previous", "comp/next"})
    {
        fs::create_directories(m_output_dir / directory);
        for (const char *number : {"1", "2", "3"})
        {
            fs::copy_file(m_output_dir / number, m_output_dir / directory / number);
        }
    }
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        std::string directory{group.rc_name()};
        std::replace(directory.begin(), directory.end(), '.', '/');
        for (int number = 1; number <= 3; ++number)
        {
            std::ofstream{m_output_dir / directory / std::to_string(number), std::ios::app}
                << "Group body " << group.rc_name() << " article " << number << '\n';
        }
    }
    g_terminal_input_reader = [this](char *buffer, int)
    {
        EXPECT_EQ(g_mode == MM_ADD_NEWSGROUP_PROMPT ? GM_PROMPT : GM_READ, g_general_mode);
        EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
        const bool reading = g_mode == MM_ARTICLE || g_mode == MM_ARTICLE_END;
        if (reading)
        {
            EXPECT_TRUE(g_in_ng);
            EXPECT_TRUE(g_doing_ng);
            EXPECT_EQ(g_nav.target.num, g_nav.current.num);
            EXPECT_EQ(g_nav.target.art, g_nav.current.art);
            EXPECT_EQ(g_cache.find(g_nav.target.num), g_nav.target.art);
        }
        else
        {
            EXPECT_FALSE(g_in_ng);
            EXPECT_FALSE(g_doing_ng);
            EXPECT_EQ(nullptr, g_art_input.fp);
        }
        m_prompts.push_back(
            {g_newsgroup_ptr ? static_cast<int>(g_newsgroup_ptr->m_num.value_of()) : -1,
             g_recent_newsgroup ? static_cast<int>(g_recent_newsgroup->m_num.value_of()) : -1,
             g_mode,
             reading ? g_nav.target.num : ArticleNum{},
             g_newsgroup_ptr ? g_newsgroup_ptr->m_to_read : TR_NONE,
             g_default_cmd,
             {std::string{g_newsgroup_data[0].rc_numbers()}, std::string{g_newsgroup_data[1].rc_numbers()},
              std::string{g_newsgroup_data[2].rc_numbers()}},
             multirc_restrictions()});
        if (m_input_pos == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected reading-session prompt";
            *buffer = reading ? 'Q' : 'q';
        }
        else
        {
            *buffer = m_commands[m_input_pos++];
        }
        return 1;
    };
}

void MultircReadingStateTest::TearDown()
{
    m_loop_context.reset();
    g_terminal_input_reader = m_old_input_reader;
    g_allow_typeahead = m_old_typeahead;
    InputNewsgroupReadingTest::TearDown();
}

void MultircReadingStateTest::run_session(std::string commands, bool restore_old)
{
    m_commands = std::move(commands);
    errno = 0;

    testing::internal::CaptureStdout();
    do_multirc();
    m_output = testing::internal::GetCapturedStdout();

    SCOPED_TRACE(m_output);
    EXPECT_EQ(m_commands.size(), m_input_pos);
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_EQ(nullptr, g_art_input.fp);
    EXPECT_EQ(restore_old, g_restore_old_newsrc);
}

void MultircReadingStateTest::expect_prompts(std::vector<std::pair<int, MinorMode>> prompts) const
{
    SCOPED_TRACE(m_output);
    ASSERT_EQ(prompts.size(), m_prompts.size());
    for (std::size_t index = 0; index < prompts.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(prompts[index].first, m_prompts[index].group);
        EXPECT_EQ(prompts[index].second, m_prompts[index].mode);
        if (prompts[index].second == MM_NEWSGROUP_LIST)
        {
            EXPECT_EQ("ynq", m_prompts[index].default_command);
        }
    }
}

void MultircReadingStateTest::expect_body_order(std::vector<std::string> bodies) const
{
    SCOPED_TRACE(m_output);
    std::size_t position = 0;
    for (const std::string &body : bodies)
    {
        position = m_output.find("Group body " + body, position);
        ASSERT_NE(std::string::npos, position) << body;
        ++position;
    }
}

class MultircReadingTest : public MultircReadingStateTest, public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircReadingTest::SetUp()
{
    MultircReadingStateTest::SetUp();
    const auto [verbose, threaded] = GetParam();
    g_verbose = verbose;
    g_use_threads = threaded;
}

TEST_P(MultircReadingTest, returningToTheGroupPromptRetainsThePartiallyReadGroup)
{
    run_session("nyQq");

    expect_prompts({{0, MM_NEWSGROUP_LIST}, {1, MM_NEWSGROUP_LIST}, {1, MM_ARTICLE}, {1, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(ArticleNum{2}, m_prompts[2].article);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[3].unread);
    EXPECT_EQ(0, m_prompts[3].recent_group);
    EXPECT_EQ(" 1-2", m_prompts[3].read_ranges[1]);
    expect_body_order({"comp.lang.cpp article 2"});
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Group body comp.lang.cpp article 3")));
}

TEST_P(MultircReadingTest, leavingAnArticleForTheNextGroupRetainsItsReadMark)
{
    run_session("nyqq");

    expect_prompts({{0, MM_NEWSGROUP_LIST}, {1, MM_NEWSGROUP_LIST}, {1, MM_ARTICLE}, {2, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(1, m_prompts[3].recent_group);
    EXPECT_EQ(" 1-2", m_prompts[3].read_ranges[1]);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[1].m_to_read);
    expect_body_order({"comp.lang.cpp article 2"});
}

TEST_P(MultircReadingTest, exhaustingAGroupAdvancesToTheNextUnreadGroup)
{
    run_session("nynnq");

    expect_prompts(
        {{0, MM_NEWSGROUP_LIST}, {1, MM_NEWSGROUP_LIST}, {1, MM_ARTICLE}, {1, MM_ARTICLE}, {2, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(ArticleNum{2}, m_prompts[2].article);
    EXPECT_EQ(ArticleNum{3}, m_prompts[3].article);
    EXPECT_EQ(1, m_prompts[4].recent_group);
    EXPECT_EQ(" 1-3", m_prompts[4].read_ranges[1]);
    EXPECT_EQ(TR_NONE, g_newsgroup_data[1].m_to_read);
    expect_body_order({"comp.lang.cpp article 2", "comp.lang.cpp article 3"});
}

TEST_P(MultircReadingTest, readingAllGroupsReachesTheEndWithTheirReadRangesRetained)
{
    run_session("ynynnynq");

    expect_prompts({{0, MM_NEWSGROUP_LIST},
                    {0, MM_ARTICLE},
                    {1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {1, MM_ARTICLE},
                    {2, MM_NEWSGROUP_LIST},
                    {2, MM_ARTICLE},
                    {-1, MM_FINISH_NEWSGROUP_LIST}});
    EXPECT_EQ(2, m_prompts[7].recent_group);
    EXPECT_EQ("npq", m_prompts[7].default_command);
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(" 1-3", m_prompts[7].read_ranges[index]);
        EXPECT_EQ(TR_NONE, g_newsgroup_data[index].m_to_read);
    }
    expect_body_order(
        {"comp.previous article 3", "comp.lang.cpp article 2", "comp.lang.cpp article 3", "comp.next article 3"});
}

TEST_P(MultircReadingTest, previousUnreadSkipsTheGroupJustExhausted)
{
    run_session("nynnpq");

    expect_prompts({{0, MM_NEWSGROUP_LIST},
                    {1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {1, MM_ARTICLE},
                    {2, MM_NEWSGROUP_LIST},
                    {0, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(2, m_prompts[5].recent_group);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[5].unread);
    EXPECT_EQ(" 1-3", m_prompts[5].read_ranges[1]);
    expect_body_order({"comp.lang.cpp article 2", "comp.lang.cpp article 3"});
}

TEST_P(MultircReadingTest, recentGroupCanReturnToTheGroupJustExhausted)
{
    run_session("nynn-q");

    expect_prompts({{0, MM_NEWSGROUP_LIST},
                    {1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {1, MM_ARTICLE},
                    {2, MM_NEWSGROUP_LIST},
                    {1, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(2, m_prompts[5].recent_group);
    EXPECT_EQ(TR_NONE, m_prompts[5].unread);
    EXPECT_EQ(" 1-3", m_prompts[5].read_ranges[1]);
    expect_body_order({"comp.lang.cpp article 2", "comp.lang.cpp article 3"});
}

TEST_P(MultircReadingTest, previousAtTheEndReturnsToTheLastUnreadGroupAfterReading)
{
    run_session("nyqnpq");

    expect_prompts({{0, MM_NEWSGROUP_LIST},
                    {1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {2, MM_NEWSGROUP_LIST},
                    {-1, MM_FINISH_NEWSGROUP_LIST},
                    {2, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(2, m_prompts[4].recent_group);
    EXPECT_EQ(-1, m_prompts[5].recent_group);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[5].unread);
    EXPECT_EQ(" 1-2", m_prompts[5].read_ranges[1]);
    expect_body_order({"comp.lang.cpp article 2"});
}

TEST_P(MultircReadingTest, nextAtTheEndRestartsWithTheFirstUnreadGroupAfterReading)
{
    run_session("nyqnnq");

    expect_prompts({{0, MM_NEWSGROUP_LIST},
                    {1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {2, MM_NEWSGROUP_LIST},
                    {-1, MM_FINISH_NEWSGROUP_LIST},
                    {0, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(-1, m_prompts[5].recent_group);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[5].unread);
    EXPECT_EQ(" 1-2", m_prompts[5].read_ranges[1]);
    expect_body_order({"comp.lang.cpp article 2"});
}

TEST_P(MultircReadingTest, explicitPreviousAndNextVisitAReadGroupAfterReading)
{
    g_newsgroup_data[0].m_rc_line.resize(g_newsgroup_data[0].m_num_offset);
    g_newsgroup_data[0].m_rc_line += " 1-3";

    run_session("yQPNq");

    expect_prompts({{1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {1, MM_NEWSGROUP_LIST},
                    {0, MM_NEWSGROUP_LIST},
                    {1, MM_NEWSGROUP_LIST}});
    EXPECT_EQ(TR_NONE, m_prompts[3].unread);
    EXPECT_EQ(1, m_prompts[3].recent_group);
    EXPECT_EQ(0, m_prompts[4].recent_group);
    EXPECT_EQ(ArticleUnread{1}, m_prompts[4].unread);
    EXPECT_EQ(" 1-2", m_prompts[4].read_ranges[1]);
    expect_body_order({"comp.lang.cpp article 2"});
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndThreading, MultircReadingTest, testing::Combine(testing::Bool(), testing::Bool()));

class MultircRestrictionReadingTest : public MultircReadingTest
{
protected:
    void SetUp() override;
};

void MultircRestrictionReadingTest::SetUp()
{
    MultircReadingTest::SetUp();
    g_only.set_to_do("comp.lang.cpp");
}

TEST_P(MultircRestrictionReadingTest, readingAllRestrictedNewsReleasesTheRestrictionOnTheNextTraversal)
{
    run_session("ynnnnq");

    expect_prompts({{1, MM_NEWSGROUP_LIST},
                    {1, MM_ARTICLE},
                    {1, MM_ARTICLE},
                    {-1, MM_FINISH_NEWSGROUP_LIST},
                    {-1, MM_FINISH_NEWSGROUP_LIST},
                    {0, MM_NEWSGROUP_LIST}});
    expect_body_order({"comp.lang.cpp article 2", "comp.lang.cpp article 3"});
    EXPECT_EQ(std::vector<std::string>{"comp.lang.cpp"}, m_prompts[3].restrictions);
    EXPECT_TRUE(m_prompts[4].restrictions.empty());
    EXPECT_TRUE(m_prompts[5].restrictions.empty());
    EXPECT_EQ("npq", m_prompts[3].default_command);
    EXPECT_EQ("npq", m_prompts[4].default_command);
    EXPECT_EQ(" 1-3", g_newsgroup_data[1].rc_numbers());
    EXPECT_EQ(TR_NONE, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(" 1-2", g_newsgroup_data[0].rc_numbers());
    EXPECT_EQ(" 1-2", g_newsgroup_data[2].rc_numbers());
    EXPECT_EQ(ArticleUnread{1}, m_prompts[5].unread);
    EXPECT_EQ(0, g_only.max_to_do);
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "Restriction comp.lang.cpp removed." : "Exiting \"only\"."));
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndThreading, MultircRestrictionReadingTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class MultircSaveReadingStateTest : public MultircReadingStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_saved_files(std::string_view first, std::string_view second) const;
    void               expect_restored_files(std::string_view first, std::string_view second) const;
    static std::string read_file(const fs::path &path);

    DataSource m_second_source{};
    Newsrc     m_second_newsrc{};

private:
    SelectionSortMode m_old_group_sort{g_sel.newsgroup_sort};
    std::string       m_saved_score_file;
    const std::string m_first_backup{"comp.previous: 1\ncomp.next! 1\n"};
    const std::string m_second_backup{"comp.lang.cpp: 1\n"};
};

void MultircSaveReadingStateTest::SetUp()
{
    set_environment([](const char *) -> char * { return nullptr; });
    MultircReadingStateTest::SetUp();
    m_saved_score_file = (m_output_dir / "saved-scores").generic_string();
    set_environment([this](const char *name) -> char *
                    { return std::string_view{name} == "SAVESCOREFILE" ? m_saved_score_file.data() : nullptr; });
    g_sel.newsgroup_sort = SS_NATURAL;
    m_second_source.m_flags = DF_NONE;
    m_second_source.m_spool_dir = m_data_source.m_spool_dir;
    m_second_source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
    ASSERT_EQ(1, m_second_source.m_act_sf.open(m_output_dir / "active", "", ""));
    m_second_newsrc.data_source = &m_second_source;
    m_second_newsrc.flags = RF_ACTIVE;
    m_second_newsrc.name = m_output_dir / "second-newsrc";
    m_second_newsrc.old_name = m_output_dir / "second-newsrc.old";
    m_second_newsrc.new_name = m_output_dir / "second-newsrc.new";
    m_second_newsrc.info_name = m_output_dir / "second-newsrc.info";
    m_newsrc.next = &m_second_newsrc;
    m_newsrc.old_name = m_output_dir / "newsrc.old";
    g_newsgroup_data[1].m_rc = &m_second_newsrc;
    g_newsgroup_data[2].m_subscribe_char = UNSUBSCRIBED_CHAR;
    g_newsgroup_data[2].m_to_read = TR_UNSUB;
    std::ofstream{m_newsrc.name} << "comp.previous: 1-2\ncomp.next! 1-2\n";
    std::ofstream{m_newsrc.old_name} << m_first_backup;
    std::ofstream{m_second_newsrc.name} << "comp.lang.cpp: 1\n";
    std::ofstream{m_second_newsrc.old_name} << m_second_backup;
}

void MultircSaveReadingStateTest::TearDown()
{
    m_second_source.close();
    m_data_source.m_act_sf.close();
    g_sel.newsgroup_sort = m_old_group_sort;
    MultircReadingStateTest::TearDown();
    set_environment(nullptr);
}

std::string MultircSaveReadingStateTest::read_file(const fs::path &path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void MultircSaveReadingStateTest::expect_saved_files(std::string_view first, std::string_view second) const
{
    EXPECT_EQ(first, read_file(m_newsrc.name));
    EXPECT_EQ(second, read_file(m_second_newsrc.name));
    EXPECT_EQ(m_first_backup, read_file(m_newsrc.old_name));
    EXPECT_EQ(m_second_backup, read_file(m_second_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
    EXPECT_FALSE(fs::exists(m_second_newsrc.new_name));
    EXPECT_TRUE(fs::exists(m_newsrc.info_name));
    EXPECT_TRUE(fs::exists(m_second_newsrc.info_name));
}

void MultircSaveReadingStateTest::expect_restored_files(std::string_view first, std::string_view second) const
{
    EXPECT_EQ(m_first_backup, read_file(m_newsrc.name));
    EXPECT_EQ(m_second_backup, read_file(m_second_newsrc.name));
    EXPECT_EQ(first, read_file(m_newsrc.new_name));
    EXPECT_EQ(second, read_file(m_second_newsrc.new_name));
    EXPECT_FALSE(fs::exists(m_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_second_newsrc.old_name));
    EXPECT_TRUE(fs::exists(m_newsrc.info_name));
    EXPECT_TRUE(fs::exists(m_second_newsrc.info_name));
}

class MultircSaveReadingTest : public MultircSaveReadingStateTest,
                               public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircSaveReadingTest::SetUp()
{
    MultircSaveReadingStateTest::SetUp();
    const auto [verbose, threaded] = GetParam();
    g_verbose = verbose;
    g_use_threads = threaded;
}

TEST_P(MultircSaveReadingTest, normalQuitSavesPartialReadsToTheirRespectiveNewsrcFiles)
{
    run_session("yQnyQq");

    expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2"});
    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1-2\n");
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_EQ(RF_NONE, m_second_newsrc.flags & RF_RC_CHANGED);
}

TEST_P(MultircSaveReadingTest, normalQuitAtTheEndSavesAllCompletedReadsAcrossNewsrcFiles)
{
    run_session("ynynnq");

    expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2", "comp.lang.cpp article 3"});
    ASSERT_FALSE(m_prompts.empty());
    EXPECT_EQ(MM_FINISH_NEWSGROUP_LIST, m_prompts.back().mode);
    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1-3\n");
}

TEST_P(MultircSaveReadingTest, normalQuitSavesAnUnsubscriptionWithItsCompletedReads)
{
    run_session("yQnyQuq");

    expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2"});
    EXPECT_EQ(UNSUBSCRIBED_CHAR, g_newsgroup_data[1].m_subscribe_char);
    EXPECT_EQ(TR_UNSUB, g_newsgroup_data[1].m_to_read);
    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp! 1-2\n");
}

TEST_P(MultircSaveReadingTest, normalQuitLeavesAnUnvisitedNewsrcFileUnchanged)
{
    run_session("yQq");

    expect_body_order({"comp.previous article 3"});
    EXPECT_EQ(std::string::npos, m_output.find("Group body comp.lang.cpp"));
    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1\n");
}

TEST_P(MultircSaveReadingTest, normalQuitSavesRestoredDelayedArticlesWithoutLosingOtherCompletedReads)
{
    run_session("yQnyMnQq");

    expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2", "comp.lang.cpp article 3"});
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(0, g_dm_count);
    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1-1,3\n");
}

TEST_P(MultircSaveReadingTest, processExitPreservesSavedReadsAndRemovesBothNewsrcLocks)
{
    m_newsrc.lock_name = m_output_dir / "newsrc.lock";
    m_second_newsrc.lock_name = m_output_dir / "second-newsrc.lock";
    std::ofstream{m_newsrc.lock_name} << "private first lock\n";
    std::ofstream{m_second_newsrc.lock_name} << "private second lock\n";
    m_data_source.m_act_sf.close();
    m_second_source.m_act_sf.close();
    g_back_page.final();
    fs::current_path(m_output_dir.parent_path());

    EXPECT_EXIT(
        {
            if (m_data_source.m_act_sf.open(m_output_dir / "active", "", "") != 1 ||
                m_second_source.m_act_sf.open(m_output_dir / "active", "", "") != 1)
            {
                std::exit(2);
            }
            g_back_page.init();
            run_session("yQnyQq");
            g_check_flag = false;
            g_bizarre = false;
            g_tmp_dir = m_output_dir.generic_string();
            g_head_name.clear();
            finalize(testing::Test::HasFailure() ? 1 : 0);
        },
        testing::ExitedWithCode(0), ".*");

    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1-2\n");
    EXPECT_FALSE(fs::exists(m_newsrc.lock_name));
    EXPECT_FALSE(fs::exists(m_second_newsrc.lock_name));
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndThreading, MultircSaveReadingTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class MultircExitRecoveryStateTest : public MultircSaveReadingStateTest
{
protected:
    void SetUp() override;
    void expect_unrelated_files() const;
    void release_child_files();
    void initialize_child();
    void finalize_child();

    Newsrc m_inactive_newsrc{};
};

void MultircExitRecoveryStateTest::SetUp()
{
    MultircSaveReadingStateTest::SetUp();
    m_second_newsrc.next = &m_inactive_newsrc;
    m_inactive_newsrc.data_source = &m_second_source;
    m_inactive_newsrc.name = m_output_dir / "inactive-newsrc";
    m_inactive_newsrc.old_name = m_output_dir / "inactive-newsrc.old";
    m_inactive_newsrc.new_name = m_output_dir / "inactive-newsrc.new";
    std::ofstream{m_inactive_newsrc.name} << "inactive current\n";
    std::ofstream{m_inactive_newsrc.old_name} << "inactive backup\n";
    std::ofstream{m_inactive_newsrc.new_name} << "inactive working\n";
    std::ofstream{m_output_dir / "unrelated"} << "unrelated file\n";
}

void MultircExitRecoveryStateTest::expect_unrelated_files() const
{
    EXPECT_EQ("inactive current\n", read_file(m_inactive_newsrc.name));
    EXPECT_EQ("inactive backup\n", read_file(m_inactive_newsrc.old_name));
    EXPECT_EQ("inactive working\n", read_file(m_inactive_newsrc.new_name));
    EXPECT_EQ("unrelated file\n", read_file(m_output_dir / "unrelated"));
}

void MultircExitRecoveryStateTest::release_child_files()
{
    m_newsrc.lock_name = m_output_dir / "newsrc.lock";
    m_second_newsrc.lock_name = m_output_dir / "second-newsrc.lock";
    std::ofstream{m_newsrc.lock_name} << "private first lock\n";
    std::ofstream{m_second_newsrc.lock_name} << "private second lock\n";
    m_data_source.m_act_sf.close();
    m_second_source.m_act_sf.close();
    g_back_page.final();
    fs::current_path(m_output_dir.parent_path());
}

void MultircExitRecoveryStateTest::initialize_child()
{
    ASSERT_EQ(1, m_data_source.m_act_sf.open(m_output_dir / "active", "", ""));
    ASSERT_EQ(1, m_second_source.m_act_sf.open(m_output_dir / "active", "", ""));
    g_back_page.init();
}

void MultircExitRecoveryStateTest::finalize_child()
{
    g_check_flag = false;
    g_bizarre = false;
    g_tmp_dir = m_output_dir.generic_string();
    g_head_name.clear();
    finalize(testing::Test::HasFailure() ? 1 : 0);
}

class MultircAbandonExitTest : public MultircExitRecoveryStateTest,
                               public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircAbandonExitTest::SetUp()
{
    MultircExitRecoveryStateTest::SetUp();
    g_verbose = std::get<1>(GetParam());
    g_use_threads = std::get<2>(GetParam());
}

TEST_P(MultircAbandonExitTest, confirmedAbandonmentRestoresBothBackupsAndRetainsTheDiscardedReads)
{
    const bool at_end = std::get<0>(GetParam());
    release_child_files();

    EXPECT_EXIT(
        {
            initialize_child();
            if (testing::Test::HasFailure())
            {
                std::exit(2);
            }
            run_session(std::string{"yQnyQ"} + (at_end ? "$" : "") + "xy", true);
            expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2"});
            EXPECT_THAT(m_output, testing::HasSubstr("The abandoned changes are in"));
            ASSERT_FALSE(m_prompts.empty());
            EXPECT_EQ(at_end ? -1 : 1, m_prompts.back().group);
            EXPECT_EQ(MM_ADD_NEWSGROUP_PROMPT, m_prompts.back().mode);
            finalize_child();
        },
        testing::ExitedWithCode(0), ".*");

    expect_restored_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1-2\n");
    expect_unrelated_files();
    EXPECT_FALSE(fs::exists(m_newsrc.lock_name));
    EXPECT_FALSE(fs::exists(m_second_newsrc.lock_name));
}

TEST_P(MultircAbandonExitTest, decliningAbandonmentThenQuittingSavesTheCompletedReads)
{
    const bool at_end = std::get<0>(GetParam());

    run_session(std::string{"yQnyQ"} + (at_end ? "$" : "") + "xnq");

    expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2"});
    EXPECT_THAT(m_output, testing::HasSubstr("Confirm: exit and abandon newsrc changes?"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("The abandoned changes are in")));
    ASSERT_GE(m_prompts.size(), 2);
    EXPECT_EQ(at_end ? -1 : 1, m_prompts.back().group);
    EXPECT_EQ(at_end ? MM_FINISH_NEWSGROUP_LIST : MM_NEWSGROUP_LIST, m_prompts.back().mode);
    expect_saved_files("comp.previous: 1-3\ncomp.next! 1-2\n", "comp.lang.cpp: 1-2\n");
    expect_unrelated_files();
}

INSTANTIATE_TEST_SUITE_P(EndPromptVerbosityAndThreading, MultircAbandonExitTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

class MultircFailedSaveTest : public MultircExitRecoveryStateTest,
                              public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>>
{
protected:
    void SetUp() override;
    void expect_recovered_files() const;

    Newsrc *m_failed_newsrc{};
};

void MultircFailedSaveTest::SetUp()
{
    MultircExitRecoveryStateTest::SetUp();
    const auto [second, at_end, verbose, threaded] = GetParam();
    g_verbose = verbose;
    g_use_threads = threaded;
    m_failed_newsrc = second ? &m_second_newsrc : &m_newsrc;
    fs::create_directory(m_failed_newsrc->new_name);
}

void MultircFailedSaveTest::expect_recovered_files() const
{
    const bool second = std::get<0>(GetParam());
    expect_restored_files(second ? "comp.previous: 1-3\ncomp.next! 1-2\n" : "comp.previous: 1-2\ncomp.next! 1-2\n",
                          second ? "comp.lang.cpp: 1\n" : "comp.lang.cpp: 1-2\n");
    expect_unrelated_files();
}

TEST_P(MultircFailedSaveTest, aFailedWriteRestoresBothSourcesBeforeReturningToTheCaller)
{
    const bool at_end = std::get<1>(GetParam());

    run_session(std::string{"yQnyQ"} + (at_end ? "$q" : "q"), true);

    expect_body_order({"comp.previous article 3", "comp.lang.cpp article 2"});
    EXPECT_THAT(m_output, testing::HasSubstr("Can't recreate " + m_failed_newsrc->name.generic_string()));
    EXPECT_THAT(m_output, testing::HasSubstr("restoring older version"));
    ASSERT_FALSE(m_prompts.empty());
    EXPECT_EQ(at_end ? -1 : 1, m_prompts.back().group);
    EXPECT_EQ(at_end ? MM_FINISH_NEWSGROUP_LIST : MM_NEWSGROUP_LIST, m_prompts.back().mode);
    expect_recovered_files();
}

TEST_P(MultircFailedSaveTest, finalizationPreservesTheRecoveredSourcesAndRemovesTheirLocks)
{
    const bool at_end = std::get<1>(GetParam());
    release_child_files();

    EXPECT_EXIT(
        {
            initialize_child();
            if (testing::Test::HasFailure())
            {
                std::exit(2);
            }
            run_session(std::string{"yQnyQ"} + (at_end ? "$q" : "q"), true);
            expect_recovered_files();
            finalize_child();
        },
        testing::ExitedWithCode(0), ".*");

    expect_recovered_files();
    EXPECT_FALSE(fs::exists(m_newsrc.lock_name));
    EXPECT_FALSE(fs::exists(m_second_newsrc.lock_name));
}

INSTANTIATE_TEST_SUITE_P(FailedSourceEndPromptVerbosityAndThreading, MultircFailedSaveTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool(), testing::Bool()));

struct LocalArticleReadingState
{
    ArticleLocation     article;
    ArticleLocation     current;
    ArticleLocation     recent;
    ArticleUnread       unread;
    std::array<bool, 3> read;
};

class SuccessiveLocalArticlesTest : public InputNewsgroupReadingTest, public testing::WithParamInterface<char>
{
protected:
    void SetUp() override;
    void TearDown() override;

    std::vector<LocalArticleReadingState> m_states;

private:
    TerminalInputReader m_old_input_reader{g_terminal_input_reader};
    bool                m_old_allow_typeahead{g_allow_typeahead};
};

void SuccessiveLocalArticlesTest::SetUp()
{
    InputNewsgroupReadingTest::SetUp();
    g_newsgroup_ptr->m_rc_line.resize(g_newsgroup_ptr->m_num_offset);
    g_newsgroup_ptr->m_rc_line += ' ';
    g_allow_typeahead = true;
    g_terminal_input_reader = [this](char *buffer, int)
    {
        m_states.push_back(
            {g_nav.target,
             g_nav.current,
             g_nav.recent,
             g_newsgroup_ptr->m_to_read,
             {g_cache.was_read(ArticleNum{1}), g_cache.was_read(ArticleNum{2}), g_cache.was_read(ArticleNum{3})}});
        *buffer = m_states.size() <= 3 ? GetParam() : 'q';
        return 1;
    };
}

void SuccessiveLocalArticlesTest::TearDown()
{
    g_terminal_input_reader = m_old_input_reader;
    g_allow_typeahead = m_old_allow_typeahead;
    InputNewsgroupReadingTest::TearDown();
}

enum class LocalReadingOrder
{
    Numeric,
    Date,
    Subject,
    Thread
};

class LocalArticleTraversalTest : public InputNewsgroupReadingTest,
                                  public testing::WithParamInterface<LocalReadingOrder>
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    void              write_article(int number, std::string_view subject, int hour, int parent = 0);
    std::vector<int>  reading_order() const;
    DoNewsgroupResult run_reading(std::vector<std::string>   commands,
                                  std::optional<std::string> start_command = std::nullopt);
    void              expect_articles(const std::vector<int> &expected) const;

    std::vector<int>           m_articles;
    std::vector<ArticleUnread> m_unread;

private:
    TerminalInputReader      m_old_input_reader{g_terminal_input_reader};
    bool                     m_old_allow_typeahead{g_allow_typeahead};
    int                      m_old_scan_on{g_scan_on};
    bool                     m_old_breadth_first{g_breadth_first};
    SelectionMode            m_old_thread_mode{g_sel.thread_mode};
    SelectionSortMode        m_old_thread_sort{g_sel.thread_sort};
    std::vector<std::string> m_commands;
    std::size_t              m_input_pos{};
};

void LocalArticleTraversalTest::SetUp()
{
    InputNewsgroupReadingTest::SetUp();
    write_article(1, "Zebra", 10);
    write_article(2, "Alpha", 11);
    write_article(3, "Re: Zebra", 14, 1);
    write_article(4, "Branch", 12, 1);
    write_article(5, "Re: Zebra", 13, 1);
    write_article(7, "Re: Alpha", 15, 2);
    g_newsgroup_ptr->m_rc_line.resize(g_newsgroup_ptr->m_num_offset);
    g_newsgroup_ptr->m_rc_line += " 5";
    g_newsgroup_ptr->m_ng_max = ArticleNum{7};
    m_data_source.m_act_sf.close();
    const fs::path active_file = m_output_dir / "active";
    std::ofstream{active_file, std::ios::binary} << "comp.previous 3 1 y\ncomp.lang.cpp 7 1 y\ncomp.next 3 1 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_file, "", ""));
    g_thread_always = true;
    g_use_threads = g_threaded_group = GetParam() == LocalReadingOrder::Thread;
    g_breadth_first = false;
    g_scan_on = GetParam() == LocalReadingOrder::Subject ? 1 : 0;
    const SelectionMode mode = GetParam() == LocalReadingOrder::Thread    ? SM_THREAD
                               : GetParam() == LocalReadingOrder::Subject ? SM_SUBJECT
                                                                          : SM_ARTICLE;
    g_sel.set_selector(mode, GetParam() == LocalReadingOrder::Numeric ? SS_NATURAL : SS_DATE);
    fs::current_path(m_output_dir);
    ASSERT_EQ(1, access_newsgroup());
    if (GetParam() == LocalReadingOrder::Date)
    {
        g_art_order.sort();
    }
    g_in_ng = false;
    g_mode = MM_NEWSGROUP_LIST;
    g_allow_typeahead = true;
    g_terminal_input_reader = [this](char *buffer, int)
    {
        EXPECT_EQ(MM_ARTICLE, g_mode);
        EXPECT_EQ(g_nav.target.num, g_nav.current.num);
        EXPECT_EQ(g_nav.target.art, g_nav.current.art);
        EXPECT_EQ(g_cache.find(g_nav.target.num), g_nav.target.art);
        m_articles.push_back(static_cast<int>(g_nav.target.num.value_of()));
        m_unread.push_back(g_newsgroup_ptr->m_to_read);
        if (m_input_pos == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected terminal input request";
            *buffer = 'Q';
            return 1;
        }
        const std::string &command = m_commands[m_input_pos++];
        push_command(std::string_view{command}.substr(1));
        *buffer = command.front();
        return 1;
    };
}

void LocalArticleTraversalTest::TearDown()
{
    g_terminal_input_reader = m_old_input_reader;
    g_allow_typeahead = m_old_allow_typeahead;
    g_scan_on = m_old_scan_on;
    g_breadth_first = m_old_breadth_first;
    InputNewsgroupReadingTest::TearDown();
    g_sel.thread_mode = m_old_thread_mode;
    g_sel.thread_sort = m_old_thread_sort;
}

void LocalArticleTraversalTest::write_article(int number, std::string_view subject, int hour, int parent)
{
    std::ofstream article{m_output_dir / "comp/lang/cpp" / std::to_string(number), std::ios::binary};
    article << "From: Reader <reader@example.test>\nSubject: " << subject << "\nDate: 21 Oct 2026 " << hour
            << ":00:00 GMT\nMessage-ID: <" << number << "@example.test>\n";
    if (parent)
    {
        article << "References: <" << parent << "@example.test>\n";
    }
    article << "\nTraversal body " << number << '\n';
}

std::vector<int> LocalArticleTraversalTest::reading_order() const
{
    switch (GetParam())
    {
    case LocalReadingOrder::Numeric:
        return {1, 2, 3, 4, 7};
    case LocalReadingOrder::Date:
        return {1, 2, 4, 3, 7};
    case LocalReadingOrder::Subject:
        return {1, 3, 2, 7, 4};
    case LocalReadingOrder::Thread:
        return {1, 4, 3, 2, 7};
    }
    return {};
}

DoNewsgroupResult LocalArticleTraversalTest::run_reading(std::vector<std::string>   commands,
                                                         std::optional<std::string> start_command)
{
    m_commands = std::move(commands);
    errno = 0;
    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::move(start_command));
    m_output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(m_commands.size(), m_input_pos);
    EXPECT_FALSE(macro_pending());
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_EQ(nullptr, g_art_input.fp);
    return result;
}

void LocalArticleTraversalTest::expect_articles(const std::vector<int> &expected) const
{
    EXPECT_EQ(expected, m_articles);
    std::string bodies;
    for (int number : expected)
    {
        bodies += "Traversal body " + std::to_string(number) + '\n';
    }
    std::string      displayed;
    const std::regex body{"Traversal body [0-9]+\\n"};
    for (auto match = std::sregex_iterator(m_output.begin(), m_output.end(), body); match != std::sregex_iterator{};
         ++match)
    {
        displayed += match->str();
    }
    EXPECT_EQ(bodies, displayed);
}

struct NewsgroupEndReadingState
{
    MinorMode               mode;
    ArticleNum              article;
    ArticleUnread           unread;
    ArticleUnread           selected;
    std::string             default_command;
    std::vector<ArticleNum> selector_articles;
    SelectionMode           selector_mode{};
    ArticleNum              current;
    ArticleNum              recent;
    bool                    selected_only{};
    std::array<bool, 3>     read{};
    bool                    selector_rereading{};
    bool                    selector_threaded{};
    AddGroupFlags           selector_mask{};
};

class NewsgroupEndReadingStateTest : public InputNewsgroupReadingTest
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    void              set_read_range(std::string_view numbers);
    void              configure_display(bool verbose, bool erase_screen);
    void              empty_spool();
    void              select_reply();
    void              prepare_commands(std::vector<std::string> commands);
    void              expect_return_to_caller() const;
    DoNewsgroupResult run_reading(std::vector<std::string>   commands,
                                  std::optional<std::string> start_command = std::nullopt);
    void              expect_end_heading() const;

    std::vector<NewsgroupEndReadingState> m_states;

private:
    TerminalInputReader      m_old_input_reader{g_terminal_input_reader};
    bool                     m_old_allow_typeahead{g_allow_typeahead};
    bool                     m_old_can_home{g_can_home};
    bool                     m_old_use_sel_num{g_use_sel_num};
    std::string              m_old_news_sel_cmds{g_news_sel_cmds};
    int                      m_old_mouse_count{g_mouse_bar_cnt};
    int                      m_old_mouse_width{g_mouse_bar_width};
    std::vector<std::string> m_commands;
    std::size_t              m_input_pos{};
};

void NewsgroupEndReadingStateTest::SetUp()
{
    InputNewsgroupReadingTest::SetUp();
    g_allow_typeahead = true;
    g_can_home = false;
    g_use_sel_num = false;
    g_news_sel_cmds = "Z>";
    g_mouse_bar_cnt = g_mouse_bar_width = 0;
    g_terminal_input_reader = [this](char *buffer, int)
    {
        NewsgroupEndReadingState state{g_mode,           g_nav.target.num, g_newsgroup_ptr->m_to_read,
                                       g_selected_count, g_default_cmd,    {}};
        state.selector_mode = g_sel.mode;
        state.current = g_nav.current.num;
        state.recent = g_nav.recent.num;
        state.selected_only = g_sel.selected_only;
        state.selector_rereading = g_sel.rereading;
        state.selector_threaded = g_threaded_group;
        state.selector_mask = g_sel.mask;
        state.read = {g_cache.was_read(ArticleNum{1}), g_cache.was_read(ArticleNum{2}),
                      g_cache.was_read(ArticleNum{3})};
        if (g_mode == MM_THREAD_SELECTOR)
        {
            EXPECT_EQ(GM_SELECTOR, g_general_mode);
            for (int item = 0; item < g_sel_page.page_item_cnt; ++item)
            {
                if (g_sel.mode == SM_ARTICLE)
                {
                    state.selector_articles.push_back(g_sel_page.items[item].u.ap->article_num());
                }
                else
                {
                    Subject *subject = g_sel_page.items[item].u.sp;
                    for (Article *article = g_sel.mode == SM_THREAD ? subject->first_art() : subject->m_articles;
                         article; article = g_sel.mode == SM_THREAD ? article->next_article() : article->m_subj_next)
                    {
                        const ArticleFlags scope = g_sel.rereading ? AF_EXISTS : AF_EXISTS | AF_UNREAD;
                        if ((article->m_flags & (AF_EXISTS | AF_UNREAD)) == scope)
                        {
                            state.selector_articles.push_back(article->article_num());
                        }
                    }
                }
            }
        }
        else if (g_mode == MM_Q || g_mode == MM_SELECTOR_ORDER_PROMPT || g_mode == MM_CONFIRM_CATCH_UP_PROMPT ||
                 g_mode == MM_UNKILL_PROMPT)
        {
            EXPECT_EQ(GM_PROMPT, g_general_mode);
        }
        else if (g_mode == MM_PAGER)
        {
            EXPECT_EQ(GM_READ, g_general_mode);
            EXPECT_EQ(g_nav.target.num, g_parsed_header.parsed_art);
            EXPECT_EQ(g_nav.target.num, g_art_input.open_art);
        }
        else
        {
            EXPECT_EQ(GM_READ, g_general_mode);
            EXPECT_EQ(g_nav.target.num, g_nav.current.num);
            EXPECT_EQ(g_nav.target.art, g_nav.current.art);
            if (g_nav.target.num == 0 && g_nav.target.art && (g_nav.target.art->m_flags & AF_TMP_MEM))
            {
                EXPECT_EQ(ArticleNum{}, g_nav.target.art->article_num());
            }
            else
            {
                EXPECT_EQ(g_cache.find(g_nav.target.num), g_nav.target.art);
            }
        }
        m_states.push_back(std::move(state));
        if (m_input_pos == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected terminal input request";
            *buffer = 'Q';
            return 1;
        }
        const std::string &command = m_commands[m_input_pos++];
        push_command(std::string_view{command}.substr(1));
        *buffer = command.front();
        return 1;
    };
}

void NewsgroupEndReadingStateTest::TearDown()
{
    g_terminal_input_reader = m_old_input_reader;
    g_allow_typeahead = m_old_allow_typeahead;
    g_can_home = m_old_can_home;
    g_use_sel_num = m_old_use_sel_num;
    g_news_sel_cmds = m_old_news_sel_cmds;
    g_mouse_bar_cnt = m_old_mouse_count;
    g_mouse_bar_width = m_old_mouse_width;
    InputNewsgroupReadingTest::TearDown();
}

void NewsgroupEndReadingStateTest::set_read_range(std::string_view numbers)
{
    g_newsgroup_ptr->m_rc_line.resize(g_newsgroup_ptr->m_num_offset);
    g_newsgroup_ptr->m_rc_line += ' ';
    g_newsgroup_ptr->m_rc_line += numbers;
}

void NewsgroupEndReadingStateTest::configure_display(bool verbose, bool erase_screen)
{
    g_verbose = verbose;
    g_erase_screen = erase_screen;
}

void NewsgroupEndReadingStateTest::empty_spool()
{
    for (ArticleNum number{1}; number <= g_newsgroup_ptr->m_ng_max; ++number)
    {
        fs::remove(m_output_dir / "comp/lang/cpp" / std::to_string(number.value_of()));
    }
    g_newsgroup_ptr->m_ng_max = ArticleNum{};
    m_data_source.m_act_sf.close();
    const fs::path active_file = m_output_dir / "active";
    std::ofstream{active_file, std::ios::binary} << "comp.previous 3 1 y\ncomp.lang.cpp 0 1 y\ncomp.next 3 1 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_file, "", ""));
}

void NewsgroupEndReadingStateTest::select_reply()
{
    g_thread_always = true;
    fs::current_path(m_output_dir);
    ASSERT_EQ(1, access_newsgroup());
    g_cache.ptr(ArticleNum{2})->select_article(AUTO_KILL_NONE);
    g_in_ng = false;
    g_mode = MM_NEWSGROUP_LIST;
}

DoNewsgroupResult NewsgroupEndReadingStateTest::run_reading(std::vector<std::string>   commands,
                                                            std::optional<std::string> start_command)
{
    prepare_commands(std::move(commands));
    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::move(start_command));
    m_output = testing::internal::GetCapturedStdout();
    expect_return_to_caller();
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    return result;
}

void NewsgroupEndReadingStateTest::prepare_commands(std::vector<std::string> commands)
{
    m_commands = std::move(commands);
    errno = 0;
}

void NewsgroupEndReadingStateTest::expect_return_to_caller() const
{
    EXPECT_EQ(m_commands.size(), m_input_pos);
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_EQ(nullptr, g_art_input.fp);
}

void NewsgroupEndReadingStateTest::expect_end_heading() const
{
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "End of newsgroup comp.lang.cpp." : "End of comp.lang.cpp"));
    EXPECT_THAT(m_output, testing::HasSubstr("What next? [npq]"));
}

class NewsgroupPromptRecoveryStateTest : public NewsgroupEndReadingStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_retained_prompt(std::size_t before, std::size_t after) const;
    void expect_completed_reading(int reply_displays = 1) const;
    void expect_text_count(std::string_view text, int expected) const;

private:
    std::string    m_old_last_pattern{g_art_search.last_pat};
    ArtScope       m_old_scope{g_art_search.how_much};
    HeaderLineType m_old_search_header{g_art_search.srch_hdr};
    bool           m_old_search_read{g_art_search.do_read};
};

void NewsgroupPromptRecoveryStateTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    g_thread_always = true;
    g_art_search.last_pat.clear();
    g_art_search.how_much = ARTSCOPE_SUBJECT;
    g_art_search.srch_hdr = SOME_LINE;
    g_art_search.do_read = false;
    search_init();
    g_art_search.init();
}

void NewsgroupPromptRecoveryStateTest::TearDown()
{
    g_art_search.last_pat = m_old_last_pattern;
    g_art_search.how_much = m_old_scope;
    g_art_search.srch_hdr = m_old_search_header;
    g_art_search.do_read = m_old_search_read;
    NewsgroupEndReadingStateTest::TearDown();
}

void NewsgroupPromptRecoveryStateTest::expect_retained_prompt(std::size_t before, std::size_t after) const
{
    ASSERT_LT(before, m_states.size());
    ASSERT_LT(after, m_states.size());
    const NewsgroupEndReadingState &original = m_states[before];
    const NewsgroupEndReadingState &recovered = m_states[after];
    EXPECT_EQ(original.mode, recovered.mode);
    EXPECT_EQ(original.article, recovered.article);
    EXPECT_EQ(original.current, recovered.current);
    EXPECT_EQ(original.recent, recovered.recent);
    EXPECT_EQ(original.unread, recovered.unread);
    EXPECT_EQ(original.selected, recovered.selected);
    EXPECT_EQ(original.selected_only, recovered.selected_only);
    EXPECT_EQ(original.read, recovered.read);
    EXPECT_EQ(original.default_command, recovered.default_command);
}

void NewsgroupPromptRecoveryStateTest::expect_text_count(std::string_view text, int expected) const
{
    int count{};
    for (std::size_t pos = m_output.find(text); pos != std::string::npos; pos = m_output.find(text, pos + text.size()))
    {
        ++count;
    }
    EXPECT_EQ(expected, count) << text;
}

void NewsgroupPromptRecoveryStateTest::expect_completed_reading(int reply_displays) const
{
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(ArticleNum{3}, m_states.back().article);
    expect_text_count("Reply body", reply_displays);
    expect_text_count("Branch body", 1);
}

class NewsgroupPromptRecoveryTest : public NewsgroupPromptRecoveryStateTest,
                                    public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void NewsgroupPromptRecoveryTest::SetUp()
{
    NewsgroupPromptRecoveryStateTest::SetUp();
    const auto [verbose, erase_screen] = GetParam();
    configure_display(verbose, erase_screen);
    g_erase_each_line = erase_screen;
}

TEST_P(NewsgroupPromptRecoveryTest, anUnknownCommandRepromptsWithoutRedisplayingOrChangingReadState)
{
    const DoNewsgroupResult result = run_reading({"~", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    expect_retained_prompt(0, 1);
    EXPECT_THAT(m_output, testing::HasSubstr("Type h for help."));
    expect_text_count("End of article 2", 2);
    expect_completed_reading();
}

TEST_P(NewsgroupPromptRecoveryTest, aCorrectedSearchContinuesAfterAnInvalidPattern)
{
    const DoNewsgroupResult result = run_reading({"/[\n", "/Branch\n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    expect_retained_prompt(0, 1);
    EXPECT_THAT(m_output, testing::HasSubstr("Missing ]"));
    EXPECT_EQ("Branch", g_art_search.last_pat);
    expect_text_count("End of article 2", 1);
    expect_completed_reading();
}

TEST_P(NewsgroupPromptRecoveryTest, anUnsuccessfulSearchRepromptsAndAllowsOrdinaryReading)
{
    const DoNewsgroupResult result = run_reading({"/absent-subject\n", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    expect_retained_prompt(0, 1);
    EXPECT_THAT(m_output, testing::HasSubstr("Not found."));
    expect_text_count("End of article 2", 2);
    expect_completed_reading();
}

TEST_P(NewsgroupPromptRecoveryTest, refreshingRedisplaysTheArticleAndPreservesTheReadingAnchor)
{
    const DoNewsgroupResult result = run_reading({"\f", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    expect_retained_prompt(0, 1);
    expect_text_count("End of article 2", 2);
    expect_completed_reading(2);
}

TEST_P(NewsgroupPromptRecoveryTest, anUnknownCommandAtGroupEndRetainsTheUnreadArticleForContinuedReading)
{
    const DoNewsgroupResult result = run_reading({"$", "~", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(4U, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[1].mode);
    EXPECT_EQ(ArticleNum{4}, m_states[1].article);
    EXPECT_EQ(ArticleUnread{1}, m_states[1].unread);
    expect_retained_prompt(1, 2);
    EXPECT_THAT(m_output, testing::HasSubstr("Type h for help."));
    expect_text_count("End of comp.lang.cpp", std::get<0>(GetParam()) ? 0 : 1);
    expect_completed_reading();
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndErasure, NewsgroupPromptRecoveryTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class NewsgroupCancelledCommandReadingTest
    : public NewsgroupPromptRecoveryStateTest,
      public testing::WithParamInterface<std::tuple<bool, bool, std::string_view>>
{
protected:
    void SetUp() override;
};

void NewsgroupCancelledCommandReadingTest::SetUp()
{
    NewsgroupPromptRecoveryStateTest::SetUp();
    configure_display(std::get<0>(GetParam()), std::get<1>(GetParam()));
    g_erase_each_line = g_erase_screen;
}

TEST_P(NewsgroupCancelledCommandReadingTest, cancellingReturnsToSingleCharacterInputWithoutReprintingThePrompt)
{
    const std::string command{std::get<2>(GetParam())};

    const DoNewsgroupResult result = run_reading({command, "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    expect_retained_prompt(0, 1);
    EXPECT_TRUE(g_art_search.last_pat.empty());
    expect_text_count("End of article 2", 1);
    expect_completed_reading();
}

INSTANTIATE_TEST_SUITE_P(DisplayAndCommand, NewsgroupCancelledCommandReadingTest,
                         testing::Combine(testing::Bool(), testing::Bool(),
                                          testing::Values(std::string_view{"3\b"}, std::string_view{"/Bra\x15"},
                                                          std::string_view{":\b"})));

using PromptInterruption = std::pair<int, bool>;

class NewsgroupInterruptedInputReadingTest : public NewsgroupPromptRecoveryStateTest,
                                             public testing::WithParamInterface<PromptInterruption>
{
};

TEST_P(NewsgroupInterruptedInputReadingTest, interruptedInputRetainsContextAndContinuesWithTheNextCommand)
{
    const auto [rows, pending_signal] = GetParam();
    g_tc_LINES = rows;
    bool                      interrupted{};
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [reading_input, pending_signal, &interrupted](char *buffer, int size)
    {
        if (!interrupted)
        {
            interrupted = true;
            EXPECT_EQ(ArticleNum{2}, g_nav.current.num);
            EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_nav.current.art);
            EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
            EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
            errno = EINTR;
            g_int_count = pending_signal ? 1 : 0;
            return -1;
        }
        return reading_input(buffer, size);
    };

    const DoNewsgroupResult result = run_reading({"n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    EXPECT_TRUE(interrupted);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(ArticleNum{2}, m_states[0].current);
    EXPECT_EQ(ArticleNum{4}, m_states[0].recent);
    EXPECT_EQ(ArticleUnread{1}, m_states[0].unread);
    expect_completed_reading(rows < 100 && !pending_signal ? 2 : 1);
}

INSTANTIATE_TEST_SUITE_P(ResumeSignalAndTallTerminal, NewsgroupInterruptedInputReadingTest,
                         testing::Values(PromptInterruption{24, false}, PromptInterruption{24, true},
                                         PromptInterruption{100, false}));

class NewsgroupSearchInputRecoveryTest : public NewsgroupPromptRecoveryStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

TEST_P(NewsgroupSearchInputRecoveryTest, rewritingAnIncompleteSearchRetainsItsTextAndFindsTheRequestedArticle)
{
    const auto [erase_screen, interrupt] = GetParam();
    g_erase_screen = g_erase_each_line = erase_screen;
    int                       continuation_requests{};
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [this, reading_input, interrupt, &continuation_requests](char *buffer, int size)
    {
        if (g_general_mode != GM_INPUT)
        {
            return reading_input(buffer, size);
        }
        EXPECT_EQ(ArticleNum{2}, g_nav.current.num);
        EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_nav.current.art);
        if (++continuation_requests == 1)
        {
            if (interrupt)
            {
                errno = EINTR;
                return -1;
            }
            *buffer = '\f';
            return 1;
        }
        EXPECT_EQ(2, continuation_requests);
        push_command("ch\n");
        *buffer = 'n';
        return 1;
    };

    const DoNewsgroupResult result = run_reading({"/Bra", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    EXPECT_EQ(2, continuation_requests);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ("Branch", g_art_search.last_pat);
    expect_completed_reading();
}

INSTANTIATE_TEST_SUITE_P(ErasureAndInterruption, NewsgroupSearchInputRecoveryTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class NewsgroupExitReadingTest : public NewsgroupEndReadingStateTest,
                                 public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
protected:
    void               SetUp() override;
    void               TearDown() override;
    void               expect_final_state(std::string_view numbers, ArticleUnread unread) const;
    void               set_kill_commands(std::string commands);
    static std::string read_file(const fs::path &path);

private:
    trn::testing::MockEnvironment m_env;
    std::FILE                    *m_old_local_kfp{g_local_kfp};
    SelectionSortMode             m_old_group_sort{g_sel.newsgroup_sort};
    int                           m_old_check_count{g_check_count};
    std::string                   m_original_newsrc;
    std::string                   m_original_backup{"comp.previous: 1\ncomp.lang.cpp: 1\ncomp.next: 1\n"};
    std::string                   m_kill_contents{"# Private reading-session commands\n"};
    fs::path                      m_kill_file;
};

void NewsgroupExitReadingTest::SetUp()
{
    EXPECT_CALL(m_env.getter, Call(testing::_)).Times(testing::AnyNumber()).WillRepeatedly(testing::Return(nullptr));
    NewsgroupEndReadingStateTest::SetUp();
    const auto [unsafe_saves, verbose, threaded] = GetParam();
    g_unsafe_rc_saves = unsafe_saves;
    g_verbose = verbose;
    g_use_threads = g_threaded_group = g_thread_always = threaded;
    g_sel.newsgroup_sort = SS_NATURAL;
    g_check_count = 0;
    g_local_kfp = nullptr;
    m_kill_file = m_output_dir / "KILL";
    std::ofstream{m_kill_file} << m_kill_contents;
    m_env.expect_env_repeatedly("KILLLOCAL", m_kill_file.generic_string().c_str());
    m_env.expect_env_repeatedly("KILLGLOBAL", (m_output_dir / "global-KILL").generic_string().c_str());
    m_newsrc.name = m_output_dir / "newsrc";
    m_newsrc.old_name = m_output_dir / "old-newsrc";
    m_newsrc.new_name = m_output_dir / "newsrc.new";
    m_newsrc.info_name = m_output_dir / "newsrc.info";
    m_original_newsrc = "comp.previous: 1-2\ncomp.lang.cpp: 1\ncomp.next: 1-2\n";
    std::ofstream{m_newsrc.name} << m_original_newsrc;
    std::ofstream{m_newsrc.old_name} << m_original_backup;
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [reading_input](char *buffer, int size)
    {
        EXPECT_NE(nullptr, g_local_kfp);
        EXPECT_NE(nullptr, g_art_input.fp);
        return reading_input(buffer, size);
    };
}

void NewsgroupExitReadingTest::TearDown()
{
    if (g_local_kfp)
    {
        std::fclose(g_local_kfp);
    }
    g_local_kfp = m_old_local_kfp;
    g_sel.newsgroup_sort = m_old_group_sort;
    g_check_count = m_old_check_count;
    NewsgroupEndReadingStateTest::TearDown();
}

std::string NewsgroupExitReadingTest::read_file(const fs::path &path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void NewsgroupExitReadingTest::set_kill_commands(std::string commands)
{
    m_kill_contents = std::move(commands);
    std::ofstream{m_kill_file} << m_kill_contents;
}

void NewsgroupExitReadingTest::expect_final_state(std::string_view numbers, ArticleUnread unread) const
{
    EXPECT_EQ(numbers, g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-2", g_newsgroup_data[0].rc_numbers());
    EXPECT_EQ(" 1-2", g_newsgroup_data[2].rc_numbers());
    EXPECT_EQ(0, g_dm_count);
    EXPECT_EQ(TR_NONE, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_FALSE(g_sel.selected_only);
    EXPECT_EQ(nullptr, g_sel_page.page_subj);
    EXPECT_EQ(nullptr, g_sel_page.page_art);
    EXPECT_EQ(nullptr, g_local_kfp);
    EXPECT_EQ(nullptr, g_art_input.fp);
    EXPECT_EQ(ArticleNum{}, g_art_input.open_art);
    for (const long number : {1L, 2L, 3L})
    {
        EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{number})->m_flags & (AF_YANK_BACK | AF_SEL)) << number;
    }
    const std::string saved = "comp.previous: 1-2\ncomp.lang.cpp:" + std::string{numbers} + "\ncomp.next: 1-2\n";
    const bool        unsafe_saves = std::get<0>(GetParam());
    EXPECT_EQ(unsafe_saves ? m_original_newsrc : saved, read_file(m_newsrc.name));
    EXPECT_EQ(m_original_backup, read_file(m_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
    EXPECT_EQ(unsafe_saves ? RF_RC_CHANGED : RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_EQ(!unsafe_saves, fs::exists(m_newsrc.info_name));
    EXPECT_EQ("THRU " + m_newsrc.name.generic_string() + " 3\n" + m_kill_contents, read_file(m_kill_file));
}

TEST_P(NewsgroupExitReadingTest, leavingAfterOneArticleSavesOnlyTheCompletedRead)
{
    const DoNewsgroupResult result = run_reading({"Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(1U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    expect_final_state(" 1-2", ArticleUnread{1});
}

TEST_P(NewsgroupExitReadingTest, exhaustionSavesAllCompletedArticlesWithoutAnotherPrompt)
{
    const DoNewsgroupResult result = run_reading({"n", "n"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ(ArticleNum{3}, m_states[1].article);
    expect_final_state(" 1-3", TR_NONE);
}

TEST_P(NewsgroupExitReadingTest, leavingRestoresTheCurrentDelayedArticleBeforeSaving)
{
    const DoNewsgroupResult result = run_reading({"M", "q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NEXT, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_TRUE(m_states[1].read[1]);
    EXPECT_THAT(m_output, testing::HasSubstr("Returning 1 Marked article..."));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    expect_final_state(" 1-1", ArticleUnread{2});
}

TEST_P(NewsgroupExitReadingTest, leavingRestoresAnEarlierDelayedArticleAndKeepsLaterCompletedReads)
{
    const DoNewsgroupResult result = run_reading({"M", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_TRUE(m_states[2].read[1]);
    EXPECT_TRUE(m_states[2].read[2]);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    expect_final_state(" 1-1,3", ArticleUnread{1});
}

TEST_P(NewsgroupExitReadingTest, exhaustionRestoresEveryDelayedArticleBeforeSaving)
{
    const DoNewsgroupResult result = run_reading({"M", "n", "M", "n"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(4U, m_states.size());
    EXPECT_TRUE(m_states[3].read[1]);
    EXPECT_TRUE(m_states[3].read[2]);
    EXPECT_THAT(m_output, testing::HasSubstr("Returning 2 Marked articles..."));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    expect_final_state(" 1-1", ArticleUnread{2});
}

TEST_P(NewsgroupExitReadingTest, leavingSelectedOnlyReadingClearsSelectionsBeforeRestoringDelayedArticles)
{
    select_reply();
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);

    const DoNewsgroupResult result = run_reading({"M", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_TRUE(m_states[1].selected_only);
    EXPECT_EQ(ArticleUnread{1}, m_states[1].selected);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    expect_final_state(" 1-1", ArticleUnread{2});
}

TEST_P(NewsgroupExitReadingTest, leavingPreservesAnExplicitUnreadMarkWithoutADelayedReturn)
{
    const DoNewsgroupResult result = run_reading({"m", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_FALSE(m_states[1].read[1]);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Returning ")));
    expect_final_state(" 1-1", ArticleUnread{2});
}

TEST_P(NewsgroupExitReadingTest, exitKillCommandsRunBeforeDelayedRestorationAndSerialization)
{
    set_kill_commands("X*j\n");

    const DoNewsgroupResult result = run_reading({"M", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_FALSE(m_states[1].read[2]);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    expect_final_state(" 1-1,3", ArticleUnread{1});
}

TEST_P(NewsgroupExitReadingTest, leavingRetainsStarCommandsForTheNextReadingSession)
{
    select_reply();
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
    set_kill_commands("# Keep selected articles\n*X\n");

    const DoNewsgroupResult result = run_reading({"M", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_FALSE(m_states[1].read[2]);
    expect_final_state(" 1-1", ArticleUnread{2});
}

INSTANTIATE_TEST_SUITE_P(SavePolicyVerbosityAndThreading, NewsgroupExitReadingTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

enum class ReadingServerFailure
{
    EOF_RESPONSE,
    CONNECTION_RESET,
    SERVER_ERROR
};

class RemoteNewsgroupReadingStateTest : public NewsgroupEndReadingStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_command(std::string command, std::vector<std::string> response,
                        std::function<void()> first_payload = {});
    void expect_header(int number, bool missing_parent = false);
    void expect_body(int number, std::function<void()> first_line = {});
    void expect_entry(bool threaded = false);
    void expect_saved_range(std::string_view range, ArticleUnread unread) const;

    std::shared_ptr<testing::StrictMock<MockNNTPConnection>> m_connection;
    testing::Sequence                                        m_protocol;

private:
    bool                                                     m_old_allow_timeout{g_nntp.allow_timeout};
    bool                                                     m_old_force_grow{g_force_grow};
    ArticleNum                                               m_old_go_article{g_ng_go_art_num};
    std::string                                              m_old_server_line{g_nntp.ser_line};
    std::string                                              m_old_last_command{g_nntp.last_command};
};

void RemoteNewsgroupReadingStateTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    g_nntp.allow_timeout = false;
    g_force_grow = false;
    m_data_source.m_flags = DF_REMOTE | DF_NO_XREFS;
    m_connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    g_nntp.link.connection = m_connection;
    g_nntp.link.flags = NNTP_NEW_CMD_OK;
    m_data_source.m_nntp_link = g_nntp.link;
}

void RemoteNewsgroupReadingStateTest::TearDown()
{
    g_nntp.allow_timeout = m_old_allow_timeout;
    g_force_grow = m_old_force_grow;
    g_ng_go_art_num = m_old_go_article;
    g_nntp.ser_line = m_old_server_line;
    g_nntp.last_command = m_old_last_command;
    NewsgroupEndReadingStateTest::TearDown();
}

void RemoteNewsgroupReadingStateTest::expect_command(std::string command, std::vector<std::string> response,
                                                     std::function<void()> first_payload)
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq(command), testing::_)).InSequence(m_protocol);
    for (std::size_t index = 0; index < response.size(); ++index)
    {
        EXPECT_CALL(*m_connection, read_line(testing::_))
            .InSequence(m_protocol)
            .WillOnce(
                [line = response[index],
                 callback = index == 1 ? first_payload : std::function<void()>{}](error_code_t &)
                {
                    if (callback)
                    {
                        callback();
                    }
                    return line;
                });
    }
}

void RemoteNewsgroupReadingStateTest::expect_header(int number, bool missing_parent)
{
    std::ifstream            article{m_output_dir / std::to_string(number)};
    std::vector<std::string> lines{"221 Header follows"};
    for (std::string line; std::getline(article, line) && !line.empty();)
    {
        if (missing_parent && line.find("References:") == 0)
        {
            line = "References: <missing@example.test>";
        }
        lines.push_back(std::move(line));
    }
    lines.emplace_back(".");
    expect_command("HEAD " + std::to_string(number), std::move(lines));
}

void RemoteNewsgroupReadingStateTest::expect_body(int number, std::function<void()> first_line)
{
    std::ifstream article{m_output_dir / std::to_string(number)};
    std::string   line;
    while (std::getline(article, line) && !line.empty())
    {
    }
    std::vector<std::string> lines{"222 Body follows"};
    while (std::getline(article, line))
    {
        lines.push_back(line);
    }
    lines.emplace_back(".");
    expect_command("BODY " + std::to_string(number), std::move(lines), std::move(first_line));
}

void RemoteNewsgroupReadingStateTest::expect_entry(bool threaded)
{
    g_use_threads = g_thread_always = g_threaded_group = threaded;
    expect_command("GROUP comp.lang.cpp", {"211 3 1 3 comp.lang.cpp"});
    expect_command("LISTGROUP", {"211 Article numbers follow", "1", "2", "3", "."});
    if (threaded)
    {
        expect_header(1);
    }
    expect_header(2, threaded);
    if (threaded)
    {
        expect_header(3);
        expect_header(2, true);
        g_ng_go_art_num = ArticleNum{2};
    }
    expect_body(2);
    expect_header(3);
}

class MultircServerLossTest : public RemoteNewsgroupReadingStateTest,
                              public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_server_loss();
    void run_outer(std::string commands);
    void expect_recovery(std::string_view ranges) const;

    std::vector<std::string> m_group_prompts;
    std::vector<ArticleNum>  m_read_articles;

private:
    static std::string read_file(const fs::path &path);

    std::optional<MultircLoopContext> m_loop_context;
    DataSource                        m_remaining_source{};
    Newsrc                            m_remaining_newsrc{};
    std::string                       m_original_failed;
    std::string                       m_original_remaining;
    std::string                       m_commands;
    std::size_t                       m_position{};
    int                               m_pauses{};
    std::string                       m_diagnostic;
    TerminalInputReader               m_parent_reader;
    bool                              m_old_check{g_check_flag};
    bool                              m_old_suppress{g_suppress_cn};
    std::string                       m_old_last_group{g_last_run.newsgroup_name};
    long                              m_old_last_new_time{g_last_run.new_time};
};

void MultircServerLossTest::SetUp()
{
    RemoteNewsgroupReadingStateTest::SetUp();
    const auto [remaining, verbose, threaded] = GetParam();
    g_verbose = verbose;
    g_check_flag = false;
    g_suppress_cn = true;
    g_thread_always = true;
    g_newsgroup_min_to_read = TR_ONE;
    m_loop_context.emplace(m_newsrc, m_output_dir);
    m_newsrc.old_name = m_output_dir / "newsrc.old";
    m_data_source.m_flags |= DF_OPEN | DF_ACTIVE;
    m_remaining_source.m_flags = DF_OPEN | DF_ACTIVE;
    m_remaining_source.m_news_id = (m_output_dir / "active").generic_string();
    m_remaining_source.m_spool_dir = m_output_dir.generic_string();
    ASSERT_EQ(1, m_remaining_source.m_act_sf.open(m_remaining_source.m_news_id, "", ""));
    m_remaining_newsrc.data_source = &m_remaining_source;
    m_remaining_newsrc.name = m_output_dir / "remaining-newsrc";
    m_remaining_newsrc.old_name = m_output_dir / "remaining-newsrc.old";
    m_remaining_newsrc.new_name = m_output_dir / "remaining-newsrc.new";
    m_remaining_newsrc.info_name = m_output_dir / "remaining-newsrc.info";
    m_remaining_newsrc.lock_name = m_output_dir / "remaining-newsrc.lock";
    m_remaining_newsrc.flags = remaining ? RF_ACTIVE : RF_NONE;
    m_newsrc.next = remaining ? &m_remaining_newsrc : nullptr;
    for (NewsgroupData &group : g_newsgroup_data)
    {
        const std::string line = std::string{group.rc_name()} + ":" + std::string{group.rc_numbers()} + "\n";
        if (remaining && group.rc_name() != "comp.lang.cpp")
        {
            group.m_rc = &m_remaining_newsrc;
            m_original_remaining += line;
        }
        else
        {
            m_original_failed += line;
        }
    }
    std::ofstream{m_newsrc.name} << m_original_failed;
    std::ofstream{m_newsrc.old_name} << m_original_failed;
    std::ofstream{m_remaining_newsrc.name} << m_original_remaining;
    std::ofstream{m_remaining_newsrc.old_name} << m_original_remaining;
    std::ofstream{m_output_dir / "unrelated"} << "unrelated file\n";
    g_start_here = &g_newsgroup_data[1];
    g_only.set_to_do("comp.*");
    m_parent_reader = g_terminal_input_reader;
    g_terminal_input_reader = [this](char *buffer, int)
    {
        if (g_mode == MM_ANY_KEY_PROMPT)
        {
            ++m_pauses;
        }
        else if (g_mode == MM_NEWSGROUP_LIST)
        {
            EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
            EXPECT_FALSE(g_in_ng);
            EXPECT_EQ(nullptr, g_art_input.fp);
            m_group_prompts.emplace_back(g_newsgroup_ptr->rc_name());
        }
        else
        {
            EXPECT_TRUE(g_in_ng);
            EXPECT_EQ(g_nav.target.num, g_nav.current.num);
            EXPECT_EQ(g_nav.target.art, g_nav.current.art);
            m_read_articles.push_back(g_nav.target.num);
        }
        if (m_position == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected outer server-loss input";
            *buffer = 'q';
        }
        else
        {
            *buffer = m_commands[m_position++];
        }
        return 1;
    };
    expect_entry(threaded);
}

void MultircServerLossTest::TearDown()
{
    if (g_multirc == &m_multirc)
    {
        unuse_multirc(&m_multirc);
    }
    g_nntp.link.connection.reset();
    m_data_source.m_nntp_link.connection.reset();
    m_data_source.close();
    m_remaining_source.close();
    g_terminal_input_reader = m_parent_reader;
    g_check_flag = m_old_check;
    g_suppress_cn = m_old_suppress;
    g_last_run.newsgroup_name = m_old_last_group;
    g_last_run.new_time = m_old_last_new_time;
    m_loop_context.reset();
    RemoteNewsgroupReadingStateTest::TearDown();
}

void MultircServerLossTest::expect_server_loss()
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("GROUP comp.lang.cpp"), testing::_)).InSequence(m_protocol);
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .InSequence(m_protocol)
        .WillOnce(
            [](error_code_t &error)
            {
                error = boost::asio::error::connection_reset;
                return std::string{};
            });
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("QUIT"), testing::_))
        .InSequence(m_protocol)
        .WillOnce([](const std::string &, error_code_t &error) { error = boost::asio::error::connection_reset; });
}

void MultircServerLossTest::run_outer(std::string commands)
{
    m_commands = std::move(commands);
    set_mode(GM_READ, MM_NEWSGROUP_LIST);
    errno = 0;

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    do_multirc();
    m_diagnostic = testing::internal::GetCapturedStderr();
    m_output = testing::internal::GetCapturedStdout();

    SCOPED_TRACE(m_output);
    EXPECT_EQ(m_commands.size(), m_position);
    EXPECT_EQ(2, m_pauses);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(nullptr, g_art_input.fp);
    EXPECT_FALSE(g_restore_old_newsrc);
}

std::string MultircServerLossTest::read_file(const fs::path &path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void MultircServerLossTest::expect_recovery(std::string_view ranges) const
{
    const bool        remaining = std::get<0>(GetParam());
    const std::string failed_text =
        remaining ? "comp.lang.cpp:" + std::string{ranges} + "\n"
                  : "comp.previous: 1-2\ncomp.lang.cpp:" + std::string{ranges} + "\ncomp.next: 1-2\n";
    EXPECT_EQ(failed_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_original_failed, read_file(m_newsrc.old_name));
    EXPECT_EQ(m_original_remaining, read_file(m_remaining_newsrc.name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
    EXPECT_FALSE(fs::exists(m_remaining_newsrc.new_name));
    EXPECT_EQ("unrelated file\n", read_file(m_output_dir / "unrelated"));
    EXPECT_NE(DF_NONE, m_data_source.m_flags & DF_UNAVAILABLE);
    EXPECT_EQ(DF_NONE, m_data_source.m_flags & DF_OPEN);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_ACTIVE);
    EXPECT_EQ((std::vector<std::string>{"comp.*"}), multirc_restrictions());
    EXPECT_THAT(m_diagnostic, testing::HasSubstr("503 Server closed connection."));
    EXPECT_EQ(remaining ? &m_multirc : nullptr, g_multirc);
    EXPECT_EQ(remaining ? 2 : 0, g_newsgroup_data.size());
    EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
    if (remaining)
    {
        ASSERT_NE(nullptr, g_newsgroup_ptr);
        EXPECT_EQ("comp.previous", g_newsgroup_ptr->rc_name());
        EXPECT_EQ(&m_remaining_newsrc, g_newsgroup_ptr->m_rc);
        EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
        EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
        EXPECT_EQ("ynq", g_default_cmd);
        EXPECT_EQ(RF_NONE, m_remaining_newsrc.flags & RF_RC_CHANGED);
    }
    else
    {
        EXPECT_EQ(nullptr, g_newsgroup_ptr);
    }
}

TEST_P(MultircServerLossTest, groupAccessFailureAfterReadingRestartsOrQuitsWithTheReadMarksSaved)
{
    expect_server_loss();

    run_outer(std::string{"yQy  "} + (std::get<0>(GetParam()) ? "q" : ""));

    expect_recovery(" 1-2");
    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{2}}), m_read_articles);
    EXPECT_EQ((std::get<0>(GetParam()) ? std::vector<std::string>{"comp.lang.cpp", "comp.lang.cpp", "comp.previous"}
                                       : std::vector<std::string>{"comp.lang.cpp", "comp.lang.cpp"}),
              m_group_prompts);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Branch body")));
}

TEST_P(MultircServerLossTest, serverLossAtExhaustionRestartsOrQuitsWithAllCompletedReadsSaved)
{
    expect_body(3);
    expect_server_loss();

    run_outer(std::string{"ynn  "} + (std::get<0>(GetParam()) ? "q" : ""));

    expect_recovery(" 1-3");
    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{2}, ArticleNum{3}}), m_read_articles);
    EXPECT_EQ((std::get<0>(GetParam()) ? std::vector<std::string>{"comp.lang.cpp", "comp.previous"}
                                       : std::vector<std::string>{"comp.lang.cpp"}),
              m_group_prompts);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
}

INSTANTIATE_TEST_SUITE_P(RemainingSourceVerbosityAndThreading, MultircServerLossTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

class RemoteNewsgroupFailureReadingTest : public RemoteNewsgroupReadingStateTest,
                                          public testing::WithParamInterface<std::tuple<ReadingServerFailure, bool>>
{
protected:
    void SetUp() override;
    void expect_failure(std::string command);
};

void RemoteNewsgroupFailureReadingTest::SetUp()
{
    RemoteNewsgroupReadingStateTest::SetUp();
    g_verbose = std::get<1>(GetParam());
}

void RemoteNewsgroupFailureReadingTest::expect_failure(std::string command)
{
    EXPECT_CALL(*m_connection, write_line(testing::StrEq(command), testing::_)).InSequence(m_protocol);
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .InSequence(m_protocol)
        .WillOnce(
            [failure = std::get<0>(GetParam())](error_code_t &error)
            {
                if (failure == ReadingServerFailure::SERVER_ERROR)
                {
                    return std::string{"503 Service unavailable"};
                }
                error = failure == ReadingServerFailure::EOF_RESPONSE
                            ? boost::asio::error::make_error_code(boost::asio::error::eof)
                            : boost::asio::error::make_error_code(boost::asio::error::connection_reset);
                return std::string{};
            });
}

void RemoteNewsgroupReadingStateTest::expect_saved_range(std::string_view range, ArticleUnread unread) const
{
    EXPECT_EQ(range, g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(TR_NONE, g_selected_count);
    EXPECT_EQ(0, g_selected_subj_cnt);
    EXPECT_NE(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_P(RemoteNewsgroupFailureReadingTest, serverLossAtExhaustionReturnsToTheCallerAfterSavingReadMarks)
{
    expect_entry();
    expect_body(3);
    expect_failure("GROUP comp.lang.cpp");

    const DoNewsgroupResult result = run_reading({"n", "n"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NO_SERVER, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(ArticleNum{3}, m_states[1].article);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    expect_saved_range(" 1-3", TR_NONE);
}

TEST_P(RemoteNewsgroupFailureReadingTest, serverLossAfterAnEndJumpPreservesSkippedUnreadArticles)
{
    expect_entry();
    expect_failure("GROUP comp.lang.cpp");

    const DoNewsgroupResult result = run_reading({"$"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NO_SERVER, result);
    ASSERT_EQ(1U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    expect_saved_range(" 1-2", ArticleUnread{1});
}

TEST_P(RemoteNewsgroupFailureReadingTest, serverLossDuringRefreshClosesTheCurrentArticle)
{
    expect_entry();
    expect_failure("GROUP comp.lang.cpp");
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [reading_input](char *buffer, int size)
    {
        EXPECT_NE(nullptr, g_art_input.fp);
        g_force_grow = true;
        return reading_input(buffer, size);
    };

    const DoNewsgroupResult result = run_reading({"\x12"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NO_SERVER, result);
    ASSERT_EQ(1U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(ArticleNum{2}, g_nav.current.num);
    EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_nav.current.art);
    expect_saved_range(" 1-2", ArticleUnread{1});
}

TEST_P(RemoteNewsgroupFailureReadingTest, serverLossResolvingAMissingParentReturnsToTheCaller)
{
    expect_entry(true);
    expect_failure("STAT <missing@example.test>");

    const DoNewsgroupResult result = run_reading({"["});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NO_SERVER, result);
    ASSERT_EQ(1U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_EQ(ArticleNum{2}, g_nav.recent.num);
    EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_nav.recent.art);
    expect_saved_range(" 1-2", ArticleUnread{1});
}

INSTANTIATE_TEST_SUITE_P(TransportAndDisplay, RemoteNewsgroupFailureReadingTest,
                         testing::Combine(testing::Values(ReadingServerFailure::EOF_RESPONSE,
                                                          ReadingServerFailure::CONNECTION_RESET,
                                                          ReadingServerFailure::SERVER_ERROR),
                                          testing::Bool()));

class RemoteNewsgroupParentReadingTest : public RemoteNewsgroupReadingStateTest,
                                         public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
    void expect_uncached_parent_entry(bool missing_parent = false);
    void expect_article(int number);
};

void RemoteNewsgroupParentReadingTest::SetUp()
{
    RemoteNewsgroupReadingStateTest::SetUp();
    g_verbose = GetParam();
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [reading_input](char *buffer, int size)
    {
        if (g_nav.recent.art)
        {
            EXPECT_EQ(g_nav.recent.num, g_nav.recent.art->article_num());
        }
        else
        {
            EXPECT_EQ(article_after(g_last_art), g_nav.recent.num);
        }
        return reading_input(buffer, size);
    };
}

void RemoteNewsgroupParentReadingTest::expect_article(int number)
{
    std::ifstream            article{m_output_dir / std::to_string(number)};
    std::vector<std::string> response{"220 Article follows"};
    for (std::string line; std::getline(article, line);)
    {
        response.push_back(std::move(line));
    }
    response.emplace_back(".");
    expect_command("ARTICLE " + std::to_string(number), std::move(response));
}

void RemoteNewsgroupParentReadingTest::expect_uncached_parent_entry(bool missing_parent)
{
    g_use_threads = g_threaded_group = true;
    g_thread_always = false;
    expect_command("GROUP comp.lang.cpp", {"211 3 1 3 comp.lang.cpp"});
    expect_command("LISTGROUP", {"211 Article numbers follow", "1", "2", "3", "."});
    expect_header(2, missing_parent);
    expect_body(2, [this] { push_command("["); });
    expect_header(3);
}

TEST_P(RemoteNewsgroupParentReadingTest, anAvailableUncachedParentCanBeReadBeforeReturningToItsReply)
{
    expect_uncached_parent_entry();
    expect_command("STAT <root@example.test>", {"223 1 <root@example.test>"});
    expect_article(1);
    expect_header(3);
    expect_header(3);
    expect_body(3);

    const DoNewsgroupResult result = run_reading({"]", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    EXPECT_EQ(ArticleNum{1}, m_states[0].article);
    EXPECT_EQ(ArticleNum{1}, m_states[0].current);
    EXPECT_EQ(ArticleNum{2}, m_states[0].recent);
    EXPECT_EQ(ArticleNum{2}, m_states[1].article);
    EXPECT_EQ(ArticleNum{1}, m_states[1].recent);
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_THAT(m_output, testing::HasSubstr("Root first line"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("article is not available")));
    expect_saved_range(" 1-3", TR_NONE);
}

TEST_P(RemoteNewsgroupParentReadingTest, anUnavailableParentShowsADiagnosticAndAllowsReturnToItsReply)
{
    expect_entry(true);
    expect_command("STAT <missing@example.test>", {"430 No such article"});
    expect_header(3);
    expect_body(3);

    const DoNewsgroupResult result = run_reading({"[", "]", "n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(4U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(ArticleNum{}, m_states[1].article);
    EXPECT_EQ(ArticleNum{2}, m_states[1].recent);
    EXPECT_EQ(m_states[0].read, m_states[1].read);
    EXPECT_EQ(ArticleUnread{1}, m_states[1].unread);
    EXPECT_EQ(ArticleNum{2}, m_states[2].article);
    EXPECT_EQ(ArticleNum{}, m_states[2].recent);
    EXPECT_EQ(ArticleNum{3}, m_states[3].article);
    EXPECT_THAT(m_output, testing::HasSubstr("article is not available."));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    EXPECT_EQ(ArticleNum{2}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
    expect_saved_range(" 1-3", TR_NONE);
}

TEST_P(RemoteNewsgroupParentReadingTest, anUncachedParentCanBeLeftWhileTheHeaderCacheIsIncomplete)
{
    expect_uncached_parent_entry(true);
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("STAT <missing@example.test>"), testing::_))
        .InSequence(m_protocol);
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .InSequence(m_protocol)
        .WillOnce(
            [this](error_code_t &)
            {
                EXPECT_LT(g_cache.last_cached, g_last_art);
                push_command("]");
                return std::string{"430 No such article"};
            });
    expect_header(3);
    expect_header(1);
    expect_header(3);
    expect_body(3);

    const DoNewsgroupResult result = run_reading({"n", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(ArticleNum{}, m_states[0].recent);
    EXPECT_EQ(ArticleUnread{1}, m_states[0].unread);
    EXPECT_EQ(ArticleNum{3}, m_states[1].article);
    EXPECT_THAT(m_output, testing::HasSubstr("article may show up in a moment."));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    expect_saved_range(" 1-3", TR_NONE);
}

INSTANTIATE_TEST_SUITE_P(Verbosity, RemoteNewsgroupParentReadingTest, testing::Bool());

class RemoteNewsgroupRefreshReadingTest : public RemoteNewsgroupReadingStateTest,
                                          public testing::WithParamInterface<std::tuple<bool, int>>
{
protected:
    void SetUp() override;
    void expect_refresh();
    void expect_subjects(int first);
    void expect_arrivals(bool read) const;

    int m_arrivals{};
};

void RemoteNewsgroupRefreshReadingTest::SetUp()
{
    RemoteNewsgroupReadingStateTest::SetUp();
    g_verbose = std::get<0>(GetParam());
    m_arrivals = std::get<1>(GetParam());
    for (int number = 4; number < 4 + m_arrivals; ++number)
    {
        std::ofstream{m_output_dir / std::to_string(number)}
            << "From: Arrival <arrival@example.test>\nSubject: Arrival " << number
            << "\nDate: Thu, 1 Jan 2026 12:00:00 +0000\nMessage-ID: <arrival" << number
            << "@example.test>\nNewsgroups: comp.lang.cpp\n\nArrival body " << number << '\n';
    }
}

void RemoteNewsgroupRefreshReadingTest::expect_refresh()
{
    expect_command("GROUP comp.lang.cpp", {"211 " + std::to_string(3 + m_arrivals) + " 1 " +
                                           std::to_string(3 + m_arrivals) + " comp.lang.cpp"});
}

void RemoteNewsgroupRefreshReadingTest::expect_subjects(int first)
{
    if (first > 3 + m_arrivals)
    {
        return;
    }
    std::vector<std::string> response{"225 Headers follow"};
    for (int number = first; number < 4 + m_arrivals; ++number)
    {
        response.push_back(std::to_string(number) + (number == 3 ? " Branch" : " Arrival " + std::to_string(number)));
    }
    response.emplace_back(".");
    expect_command("HDR subject " + std::to_string(first) + "-" + std::to_string(3 + m_arrivals), std::move(response));
}

void RemoteNewsgroupRefreshReadingTest::expect_arrivals(bool read) const
{
    EXPECT_EQ(ArticleNum{1}, g_abs_first);
    EXPECT_EQ(ArticleNum{3 + m_arrivals}, g_last_art);
    EXPECT_EQ(g_last_art, g_newsgroup_ptr->m_ng_max);
    EXPECT_FALSE(g_force_grow);
    for (int number = 4; number < 4 + m_arrivals; ++number)
    {
        const Article *article = g_cache.find(ArticleNum{number});
        ASSERT_NE(nullptr, article);
        EXPECT_NE(AF_NONE, article->m_flags & AF_EXISTS);
        EXPECT_EQ(read, g_cache.was_read(ArticleNum{number}));
        EXPECT_EQ(read, m_output.find("Arrival body " + std::to_string(number)) != std::string::npos);
    }
}

TEST_P(RemoteNewsgroupRefreshReadingTest, exhaustionRefreshesTheGroupAndContinuesThroughArrivals)
{
    expect_entry();
    expect_body(3);
    expect_refresh();
    for (int number = 4; number < 4 + m_arrivals; ++number)
    {
        if (number == 4)
        {
            expect_header(number);
        }
        expect_body(number);
        if (number + 1 < 4 + m_arrivals)
        {
            expect_header(number + 1);
        }
    }
    if (m_arrivals)
    {
        expect_refresh();
    }

    const DoNewsgroupResult result = run_reading(std::vector<std::string>(2 + m_arrivals, "n"));

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(2 + m_arrivals, m_states.size());
    for (std::size_t index = 0; index < m_states.size(); ++index)
    {
        EXPECT_EQ(ArticleNum{2 + static_cast<long>(index)}, m_states[index].article);
        EXPECT_EQ(MM_ARTICLE, m_states[index].mode);
    }
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    expect_arrivals(true);
    expect_saved_range(" 1-" + std::to_string(3 + m_arrivals), TR_NONE);
    EXPECT_EQ(ArticleNum{3 + m_arrivals}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
}

TEST_P(RemoteNewsgroupRefreshReadingTest, aForcedEndJumpRefreshesBoundsAndPreservesSkippedArticles)
{
    expect_entry();
    expect_refresh();
    expect_subjects(4);
    for (int number = 4; number < 4 + m_arrivals; ++number)
    {
        expect_header(number);
    }

    const DoNewsgroupResult result = run_reading({"$", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[1].mode);
    EXPECT_EQ(ArticleNum{4 + m_arrivals}, m_states[1].article);
    EXPECT_EQ(ArticleUnread{1 + m_arrivals}, m_states[1].unread);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    expect_end_heading();
    expect_arrivals(false);
    expect_saved_range(" 1-2", ArticleUnread{1 + m_arrivals});
}

TEST_P(RemoteNewsgroupRefreshReadingTest, aRequestedRefreshReusesTheCachedBodyAndKeepsTheCurrentArticle)
{
    expect_entry();
    expect_refresh();
    expect_header(3);
    expect_subjects(4);
    for (int number = 4; number < 4 + m_arrivals; ++number)
    {
        expect_header(number);
    }
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [this, reading_input](char *buffer, int size)
    {
        if (m_states.empty())
        {
            g_force_grow = true;
        }
        return reading_input(buffer, size);
    };

    const DoNewsgroupResult result = run_reading({"\022", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2U, m_states.size());
    EXPECT_EQ(ArticleNum{2}, m_states[1].article);
    EXPECT_EQ(m_states[0].current, m_states[1].current);
    EXPECT_EQ(m_states[0].recent, m_states[1].recent);
    EXPECT_EQ(m_states[0].read, m_states[1].read);
    const std::size_t first = m_output.find("Reply body");
    ASSERT_NE(std::string::npos, first);
    EXPECT_NE(std::string::npos, m_output.find("Reply body", first + 1));
    expect_arrivals(false);
    expect_saved_range(" 1-2", ArticleUnread{1 + m_arrivals});
}

TEST_P(RemoteNewsgroupRefreshReadingTest, leavingThePagerFinishesTheBodyBeforeRefreshingTheGroup)
{
    std::ofstream article{m_output_dir / "2", std::ios::app};
    for (int line = 0; line < 100; ++line)
    {
        article << "Pending body line " << line << '\n';
    }
    article.close();
    g_tc_LINES = 12;
    expect_command("GROUP comp.lang.cpp", {"211 3 1 3 comp.lang.cpp"});
    expect_command("LISTGROUP", {"211 Article numbers follow", "1", "2", "3", "."});
    expect_header(2);
    bool interrupted_body = false;
    expect_body(2,
                [this, &interrupted_body]
                {
                    EXPECT_LT(nntp_art_size(), ArticlePosition{});
                    interrupted_body = true;
                    push_command("$");
                });
    expect_refresh();
    expect_subjects(3);
    expect_header(3);
    for (int number = 4; number < 4 + m_arrivals; ++number)
    {
        expect_header(number);
    }
    const TerminalInputReader reading_input = g_terminal_input_reader;
    g_terminal_input_reader = [this, reading_input](char *buffer, int size)
    {
        const std::string name = nntp_art_name(ArticleNum{2}, false);
        std::ifstream     cached{name, std::ios::binary};
        const std::string body{std::istreambuf_iterator<char>{cached}, {}};
        EXPECT_THAT(body, testing::HasSubstr("Pending body line 99\n"));
        EXPECT_GE(nntp_art_size(), ArticlePosition{});
        return reading_input(buffer, size);
    };

    const DoNewsgroupResult result = run_reading({"Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    EXPECT_TRUE(interrupted_body);
    ASSERT_EQ(1U, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ(ArticleNum{4 + m_arrivals}, m_states[0].article);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Pending body line 99")));
    expect_arrivals(false);
    expect_saved_range(" 1-1", ArticleUnread{2 + m_arrivals});
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndArrivals, RemoteNewsgroupRefreshReadingTest,
                         testing::Combine(testing::Bool(), testing::Values(0, 1, 2)));

class NewsgroupEndReadingTest : public NewsgroupEndReadingStateTest,
                                public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void NewsgroupEndReadingTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto [verbose, erase_screen] = GetParam();
    configure_display(verbose, erase_screen);
}

class EmptyNewsgroupEndTest : public NewsgroupEndReadingStateTest,
                              public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
protected:
    void SetUp() override;
};

void EmptyNewsgroupEndTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto [verbose, erase_screen, no_files] = GetParam();
    configure_display(verbose, erase_screen);
    set_read_range("1-3");
    if (no_files)
    {
        empty_spool();
        set_read_range("1-0");
    }
}

using ForcedEndReadRange = std::pair<std::string_view, ArticleUnread>;

class ForcedNewsgroupEndCountTest : public NewsgroupEndReadingStateTest,
                                    public testing::WithParamInterface<std::tuple<bool, bool, ForcedEndReadRange>>
{
protected:
    void SetUp() override;
};

void ForcedNewsgroupEndCountTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto &[verbose, erase_screen, range] = GetParam();
    configure_display(verbose, erase_screen);
    set_read_range(range.first);
}

class NewsgroupSelectorReadingStateTest : public NewsgroupEndReadingStateTest
{
protected:
    void                 TearDown() override;
    void                 configure_selector(SelectionMode mode, bool erase_screen);
    void                 expect_selector(std::size_t index, std::vector<ArticleNum> articles) const;
    void                 expect_body_count(std::string_view body, int expected) const;
    InputNewsgroupResult run_from_group_prompt(std::vector<std::string> commands);

private:
    SelectionMode     m_old_thread_mode{g_sel.thread_mode};
    SelectionSortMode m_old_thread_sort{g_sel.thread_sort};
    bool              m_old_breadth_first{g_breadth_first};
    bool              m_old_bos_on_stop{g_bos_on_stop};
    int               m_old_scan_on{g_scan_on};
    SelectionMode     m_mode{};
};

void NewsgroupSelectorReadingStateTest::TearDown()
{
    NewsgroupEndReadingStateTest::TearDown();
    g_sel.thread_mode = m_old_thread_mode;
    g_sel.thread_sort = m_old_thread_sort;
    g_breadth_first = m_old_breadth_first;
    g_bos_on_stop = m_old_bos_on_stop;
    g_scan_on = m_old_scan_on;
}

void NewsgroupSelectorReadingStateTest::configure_selector(SelectionMode mode, bool erase_screen)
{
    m_mode = mode;
    g_sel.default_mode = mode;
    g_use_threads = g_threaded_group = mode == SM_THREAD;
    g_thread_always = true;
    g_breadth_first = false;
    g_scan_on = 0;
    g_erase_screen = erase_screen;
    g_tc_LINES = 40;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_sel.set_selector(mode, mode == SM_ARTICLE ? SS_NATURAL : SS_DATE);
    std::ofstream{m_output_dir / "comp/lang/cpp/3", std::ios::binary}
        << "From: Branch <branch@example.test>\nSubject: Branch\n"
           "Date: 21 Oct 2026 12:00:00 GMT\nMessage-ID: <branch@example.test>\n\nBranch body\n";
}

void NewsgroupSelectorReadingStateTest::expect_selector(std::size_t index, std::vector<ArticleNum> articles) const
{
    ASSERT_LT(index, m_states.size());
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[index].mode);
    EXPECT_EQ(m_mode, m_states[index].selector_mode);
    EXPECT_EQ(articles, m_states[index].selector_articles);
}

void NewsgroupSelectorReadingStateTest::expect_body_count(std::string_view body, int expected) const
{
    int count = 0;
    for (std::size_t pos = m_output.find(body); pos != std::string::npos; pos = m_output.find(body, pos + body.size()))
    {
        ++count;
    }
    EXPECT_EQ(expected, count) << body;
}

InputNewsgroupResult NewsgroupSelectorReadingStateTest::run_from_group_prompt(std::vector<std::string> commands)
{
    prepare_commands(std::move(commands));
    push_command("y");
    testing::internal::CaptureStdout();
    const InputNewsgroupResult result = input_newsgroup();
    m_output = testing::internal::GetCapturedStdout();
    expect_return_to_caller();
    return result;
}

class NewsgroupSelectorReadingTest : public NewsgroupSelectorReadingStateTest,
                                     public testing::WithParamInterface<std::tuple<SelectionMode, bool>>
{
protected:
    void SetUp() override;
};

void NewsgroupSelectorReadingTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto [mode, erase_screen] = GetParam();
    configure_selector(mode, erase_screen);
}

struct PagedArticleSelectorCase
{
    const char      *name;
    SelectionMode    mode;
    std::vector<int> first_page;
    std::vector<int> second_page;
    std::vector<int> batch;
    std::vector<int> all_unread;
    std::vector<int> remaining_page;
    const char      *batch_range;
    int              pages;
};

const PagedArticleSelectorCase PAGED_ARTICLE_SELECTOR_CASES[]{
    {"Articles", SM_ARTICLE, {2, 3}, {4, 5}, {3, 4}, {2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {2, 5}, " 1-1,3-4", 5},
    {"Subjects",
     SM_SUBJECT,
     {2, 7, 3, 8},
     {4, 5, 10},
     {3, 8, 4},
     {2, 7, 3, 8, 4, 5, 10, 6, 11, 9},
     {2, 7, 5, 10},
     " 1-1,3-4,8",
     3},
    {"Threads",
     SM_THREAD,
     {2, 7, 3, 8},
     {4, 9, 5, 10},
     {3, 8, 4, 9},
     {2, 7, 3, 8, 4, 9, 5, 10, 6, 11},
     {2, 7, 5, 10},
     " 1-1,3-4,8-9",
     3}};

void PrintTo(const PagedArticleSelectorCase &value, std::ostream *output)
{
    *output << value.name;
}

class PagedArticleSelectorReadingStateTest : public NewsgroupSelectorReadingStateTest
{
protected:
    void                     SetUp() override;
    void                     configure_pages(const PagedArticleSelectorCase &input, bool erase_screen);
    void                     expect_page(std::size_t index, const std::vector<int> &articles) const;
    void                     expect_selected(std::size_t index, std::vector<int> articles) const;
    void expect_reading(const std::vector<int> &articles, bool selected_only, std::vector<int> completed = {1}) const;
    void                     expect_bodies(const std::vector<int> &articles) const;
    void                     expect_returned_context(std::size_t index, std::size_t before = 0) const;
    std::vector<std::string> selection_commands() const;
    void                     append_reading(std::vector<std::string> &commands, const std::vector<int> &articles,
                                            bool exhaust = false) const;

    std::vector<std::vector<int>> m_selected;
    std::vector<std::vector<int>> m_read;

private:
    void write_article(int number, std::string_view subject, int parent = 0);

    trn::testing::MockEnvironment m_environment;
};

void PagedArticleSelectorReadingStateTest::SetUp()
{
    EXPECT_CALL(m_environment.getter, Call(testing::_))
        .Times(testing::AnyNumber())
        .WillRepeatedly(testing::Return(nullptr));
    NewsgroupEndReadingStateTest::SetUp();
    m_environment.expect_env_repeatedly("SELECTCHARS", "ab");
    write_article(1, "Opening");
    write_article(2, "Alpha");
    write_article(3, "Beta");
    write_article(4, "Gamma");
    write_article(5, "Delta");
    write_article(6, "Epsilon");
    write_article(7, "Re: Alpha", 2);
    write_article(8, "Re: Beta", 3);
    write_article(9, "Changed branch", 4);
    write_article(10, "Re: Delta", 5);
    write_article(11, "Re: Epsilon", 6);
    set_read_range("");
    g_newsgroup_ptr->m_ng_max = ArticleNum{11};
    m_data_source.m_act_sf.close();
    const fs::path active_file = m_output_dir / "active";
    std::ofstream{active_file, std::ios::binary} << "comp.previous 3 1 y\ncomp.lang.cpp 11 1 y\ncomp.next 3 1 y\n";
    ASSERT_EQ(1, m_data_source.m_act_sf.open(active_file, "", ""));
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        std::vector<int> selected;
        std::vector<int> read;
        for (int number = 1; number <= 11; ++number)
        {
            Article *article = g_cache.find(ArticleNum{number});
            if (article && (article->m_flags & (AF_UNREAD | AF_SEL)) == (AF_UNREAD | AF_SEL))
            {
                selected.push_back(number);
            }
            if (g_cache.was_read(ArticleNum{number}))
            {
                read.push_back(number);
            }
        }
        m_selected.push_back(std::move(selected));
        m_read.push_back(std::move(read));
        return reader(buffer, size);
    };
}

void PagedArticleSelectorReadingStateTest::write_article(int number, std::string_view subject, int parent)
{
    std::ofstream article{m_output_dir / "comp/lang/cpp" / std::to_string(number), std::ios::binary};
    article << "From: Reader <reader@example.test>\nSubject: " << subject << "\nDate: 21 Oct 2026 " << number + 8
            << ":00:00 GMT\nMessage-ID: <page" << number << "@example.test>\n";
    if (parent)
    {
        article << "References: <page" << parent << "@example.test>\n";
    }
    article << "\nPaged selector body " << number << " END\n";
}

void PagedArticleSelectorReadingStateTest::configure_pages(const PagedArticleSelectorCase &input, bool erase_screen)
{
    configure_selector(input.mode, erase_screen);
    write_article(3, "Beta");
}

void PagedArticleSelectorReadingStateTest::expect_page(std::size_t index, const std::vector<int> &articles) const
{
    ASSERT_LT(index, m_states.size());
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[index].mode);
    std::vector<int> actual;
    for (ArticleNum number : m_states[index].selector_articles)
    {
        actual.push_back(static_cast<int>(number.value_of()));
    }
    EXPECT_EQ(articles, actual);
}

void PagedArticleSelectorReadingStateTest::expect_selected(std::size_t index, std::vector<int> articles) const
{
    ASSERT_LT(index, m_selected.size());
    std::sort(articles.begin(), articles.end());
    EXPECT_EQ(articles, m_selected[index]);
}

void PagedArticleSelectorReadingStateTest::expect_reading(const std::vector<int> &articles, bool selected_only,
                                                          std::vector<int> completed) const
{
    std::vector<int> actual;
    for (std::size_t index = 0; index < m_states.size(); ++index)
    {
        if (m_states[index].mode == MM_ARTICLE)
        {
            actual.push_back(static_cast<int>(m_states[index].article.value_of()));
            if (actual.size() > 1)
            {
                EXPECT_EQ(selected_only, m_states[index].selected_only);
                completed.push_back(actual.back());
                std::sort(completed.begin(), completed.end());
                completed.erase(std::unique(completed.begin(), completed.end()), completed.end());
                EXPECT_EQ(ArticleUnread{11 - static_cast<long>(completed.size())}, m_states[index].unread);
                EXPECT_EQ(completed, m_read[index]);
            }
        }
    }
    std::vector<int> expected{1};
    expected.insert(expected.end(), articles.begin(), articles.end());
    EXPECT_EQ(expected, actual);
    expect_bodies(expected);
}

void PagedArticleSelectorReadingStateTest::expect_bodies(const std::vector<int> &articles) const
{
    std::size_t position = 0;
    for (int number : articles)
    {
        const std::string body = "Paged selector body " + std::to_string(number) + " END";
        position = m_output.find(body, position);
        ASSERT_NE(std::string::npos, position) << body;
        position += body.size();
    }
    for (int number = 1; number <= 11; ++number)
    {
        expect_body_count("Paged selector body " + std::to_string(number) + " END",
                          std::count(articles.begin(), articles.end(), number));
    }
}

std::vector<std::string> PagedArticleSelectorReadingStateTest::selection_commands() const
{
    return {"+", "b", ">", "a", "<", ">"};
}

void PagedArticleSelectorReadingStateTest::append_reading(std::vector<std::string> &commands,
                                                          const std::vector<int> &articles, bool exhaust) const
{
    commands.insert(commands.end(), articles.size() - (exhaust ? 0 : 1), " ");
    commands.push_back("Q");
}

class PagedArticleSelectorAcceptTest
    : public PagedArticleSelectorReadingStateTest,
      public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool, char>>
{
protected:
    void SetUp() override;
};

void PagedArticleSelectorAcceptTest::SetUp()
{
    PagedArticleSelectorReadingStateTest::SetUp();
    const auto &[input, erase_screen, accept] = GetParam();
    configure_pages(input, erase_screen);
}

class PagedArticleSelectorDefaultTest : public PagedArticleSelectorReadingStateTest,
                                        public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool>>
{
protected:
    void SetUp() override;
};

void PagedArticleSelectorDefaultTest::SetUp()
{
    PagedArticleSelectorReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    configure_pages(input, erase_screen);
}

class ArticleSelectorScopeReadingTest : public PagedArticleSelectorReadingStateTest,
                                        public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool>>
{
protected:
    void SetUp() override;
    void expect_scope(std::size_t index, bool rereading, const std::vector<int> &read, const std::vector<int> &selected,
                      const std::vector<int> &marked = {}) const;

    const std::vector<int> m_initial_read{1, 2, 3, 4, 7};
    std::vector<int>       m_delta;
    std::vector<int>       m_alpha;
    std::vector<int>       m_revived_read;
    std::vector<int>       m_combined;
    std::vector<int>       m_batch;
    bool                   m_grouped{};

private:
    std::vector<std::vector<int>> m_marks;
    std::vector<int>              m_population;
};

void ArticleSelectorScopeReadingTest::SetUp()
{
    PagedArticleSelectorReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    configure_pages(input, erase_screen);
    set_read_range("1-3,7");
    m_grouped = input.mode != SM_ARTICLE;
    m_delta = m_grouped ? std::vector<int>{5, 10} : std::vector<int>{5};
    m_alpha = m_grouped ? std::vector<int>{2, 7} : std::vector<int>{2};
    m_revived_read = m_grouped ? std::vector<int>{1, 3, 4} : std::vector<int>{1, 3, 4, 7};
    m_combined = m_grouped ? std::vector<int>{2, 5, 7, 10} : std::vector<int>{2, 5};
    m_batch = m_grouped ? std::vector<int>{2, 7, 5, 10} : std::vector<int>{2, 5};
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        std::vector<int> marks;
        for (int number = 1; number <= 11; ++number)
        {
            Article *article = g_cache.find(ArticleNum{number});
            if (article && (article->m_flags & AF_DEL_SEL))
            {
                marks.push_back(number);
            }
        }
        m_marks.push_back(std::move(marks));
        m_population.push_back(g_sel_page.total_obj_cnt);
        return reader(buffer, size);
    };
}

void ArticleSelectorScopeReadingTest::expect_scope(std::size_t index, bool rereading, const std::vector<int> &read,
                                                   const std::vector<int> &selected,
                                                   const std::vector<int> &marked) const
{
    ASSERT_LT(index, m_states.size());
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[index].mode);
    EXPECT_EQ(std::get<0>(GetParam()).mode, m_states[index].selector_mode);
    EXPECT_EQ(rereading, m_states[index].selector_rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, m_states[index].selector_mask);
    EXPECT_EQ(read, m_read[index]);
    EXPECT_EQ(marked, m_marks[index]);
    expect_selected(index, selected);
    const std::size_t count = rereading ? marked.size() : selected.size();
    EXPECT_EQ(ArticleUnread{static_cast<long>(count)}, m_states[index].selected);
    EXPECT_EQ(ArticleUnread{11 - static_cast<long>(read.size())}, m_states[index].unread);
    EXPECT_EQ(rereading ? read.size() : 11 - read.size(), m_population[index]);
    for (ArticleNum number : m_states[index].selector_articles)
    {
        const bool was_read = std::find(read.begin(), read.end(), number.value_of()) != read.end();
        EXPECT_EQ(rereading, was_read) << number.value_of();
    }
}

void PagedArticleSelectorReadingStateTest::expect_returned_context(std::size_t index, std::size_t before) const
{
    ASSERT_LT(index, m_states.size());
    ASSERT_LT(before, m_states.size());
    EXPECT_EQ(m_states[before].mode, m_states[index].mode);
    EXPECT_EQ(m_states[before].article, m_states[index].article);
    EXPECT_EQ(m_states[before].current, m_states[index].current);
    EXPECT_EQ(m_states[before].recent, m_states[index].recent);
    EXPECT_EQ(m_states[before].selected_only, m_states[index].selected_only);
    EXPECT_FALSE(m_states[index].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[index].selector_mask);
}

struct SelectorPresentationState
{
    SelectionSortMode sort;
    int               direction;
    char              style;
    int               prior;
    int               population;
    ArticleNum        anchor;
};

class ArticleSelectorPresentationReadingStateTest : public PagedArticleSelectorReadingStateTest
{
protected:
    void                            SetUp() override;
    void                            TearDown() override;
    const PagedArticleSelectorCase &configure_presentation(SelectionMode mode, bool erase_screen);
    void expect_view(std::size_t index, SelectionMode mode, SelectionSortMode sort, int direction,
                     const std::vector<int> &page, const std::vector<int> &selected, int prior = 0) const;

    std::vector<SelectorPresentationState> m_presentation;

private:
    std::string m_old_display_mode{g_sel_art_display_mode};
    std::size_t m_old_display_index{g_sel_art_display_mode_index};
};

void ArticleSelectorPresentationReadingStateTest::SetUp()
{
    PagedArticleSelectorReadingStateTest::SetUp();
    g_sel_art_display_mode = "lmds";
    g_sel_art_display_mode_index = 3;
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        ArticleNum anchor;
        if (g_mode == MM_THREAD_SELECTOR && g_sel_page.page_item_cnt)
        {
            const Selection &item = g_sel_page.items[0].u;
            Article         *article = g_sel.mode == SM_ARTICLE  ? item.ap
                                       : g_sel.mode == SM_THREAD ? item.sp->first_art()
                                                                 : item.sp->m_articles;
            anchor = article->article_num();
        }
        m_presentation.push_back({g_sel.sort, g_sel.direction, current_article_display_mode(), g_sel_page.prior_obj_cnt,
                                  g_sel_page.total_obj_cnt, anchor});
        return reader(buffer, size);
    };
}

void ArticleSelectorPresentationReadingStateTest::TearDown()
{
    g_sel_art_display_mode = m_old_display_mode;
    g_sel_art_display_mode_index = m_old_display_index;
    PagedArticleSelectorReadingStateTest::TearDown();
}

void ArticleSelectorPresentationReadingStateTest::expect_view(std::size_t index, SelectionMode mode,
                                                              SelectionSortMode sort, int direction,
                                                              const std::vector<int> &page,
                                                              const std::vector<int> &selected, int prior) const
{
    expect_page(index, page);
    ASSERT_LT(index, m_presentation.size());
    EXPECT_EQ(mode, m_states[index].selector_mode);
    EXPECT_EQ(sort, m_presentation[index].sort);
    EXPECT_EQ(direction, m_presentation[index].direction);
    EXPECT_EQ(prior, m_presentation[index].prior);
    EXPECT_EQ(10, m_presentation[index].population);
    EXPECT_EQ(ArticleNum{page.front()}, m_presentation[index].anchor);
    EXPECT_FALSE(m_states[index].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[index].selector_mask);
    EXPECT_EQ(ArticleUnread{10}, m_states[index].unread);
    EXPECT_EQ(ArticleUnread{static_cast<long>(selected.size())}, m_states[index].selected);
    EXPECT_EQ((std::vector<int>{1}), m_read[index]);
    expect_selected(index, selected);
}

const PagedArticleSelectorCase &ArticleSelectorPresentationReadingStateTest::configure_presentation(SelectionMode mode,
                                                                                                    bool erase_screen)
{
    const auto page = std::find_if(std::begin(PAGED_ARTICLE_SELECTOR_CASES), std::end(PAGED_ARTICLE_SELECTOR_CASES),
                                   [mode](const PagedArticleSelectorCase &value) { return value.mode == mode; });
    configure_pages(*page, erase_screen);
    return *page;
}

class ArticleSelectorPresentationReadingTest
    : public ArticleSelectorPresentationReadingStateTest,
      public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool>>
{
protected:
    void SetUp() override;
};

void ArticleSelectorPresentationReadingTest::SetUp()
{
    ArticleSelectorPresentationReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    configure_pages(input, erase_screen);
}

struct SelectorSubjectOrderCase
{
    SelectionMode    mode;
    std::vector<int> forward_page;
    std::vector<int> reverse_page;
    std::vector<int> forward_batch;
    std::vector<int> reverse_batch;
    const char      *read_range;
};

const SelectorSubjectOrderCase SELECTOR_SUBJECT_ORDER_CASES[]{
    {SM_ARTICLE, {2, 7}, {4, 11}, {3, 4}, {4, 3}, " 1-1,3-4"},
    {SM_SUBJECT, {2, 7, 3, 8}, {4, 6, 11}, {3, 8, 4}, {4, 3, 8}, " 1-1,3-4,8"},
    {SM_THREAD, {2, 7, 3, 8}, {4, 9, 6, 11}, {3, 8, 4, 9}, {4, 9, 3, 8}, " 1-1,3-4,8-9"}};

void PrintTo(const SelectorSubjectOrderCase &value, std::ostream *output)
{
    *output << (value.mode == SM_ARTICLE ? "Articles" : value.mode == SM_SUBJECT ? "Subjects" : "Threads");
}

class ArticleSelectorSubjectOrderReadingTest
    : public ArticleSelectorPresentationReadingStateTest,
      public testing::WithParamInterface<std::tuple<SelectorSubjectOrderCase, bool, bool>>
{
protected:
    void SetUp() override;
};

void ArticleSelectorSubjectOrderReadingTest::SetUp()
{
    ArticleSelectorPresentationReadingStateTest::SetUp();
    const auto &[input, reverse, erase_screen] = GetParam();
    configure_presentation(input.mode, erase_screen);
}

struct SelectorGroupingReadingCase
{
    const char      *name;
    SelectionMode    initial;
    SelectionMode    target;
    char             response;
    std::vector<int> prompt_page;
    int              prompt_prior;
    std::vector<int> toggle_page;
    int              toggle_prior;
    std::vector<int> batch;
};

const SelectorGroupingReadingCase SELECTOR_GROUPING_READING_CASES[]{
    {"ArticleToSubject", SM_ARTICLE, SM_SUBJECT, 's', {2, 7, 3, 8}, 0, {4, 5, 10}, 4, {3, 4}},
    {"ArticleToThread", SM_ARTICLE, SM_THREAD, 't', {2, 7, 3, 8}, 0, {4, 9, 5, 10}, 4, {3, 4}},
    {"SubjectToArticle", SM_SUBJECT, SM_ARTICLE, 'a', {4, 5}, 2, {4, 5}, 2, {3, 4, 8}},
    {"ThreadToArticle", SM_THREAD, SM_ARTICLE, 'a', {4, 5}, 2, {4, 5}, 2, {3, 4, 8, 9}},
    {"SubjectToThread", SM_SUBJECT, SM_THREAD, 't', {2, 7, 3, 8}, 0, {}, 0, {3, 8, 4}},
    {"ThreadToSubject", SM_THREAD, SM_SUBJECT, 's', {4, 5, 10}, 4, {}, 0, {3, 8, 4, 9}}};

void PrintTo(const SelectorGroupingReadingCase &value, std::ostream *output)
{
    *output << value.name;
}

class ArticleSelectorGroupingReadingTest
    : public ArticleSelectorPresentationReadingStateTest,
      public testing::WithParamInterface<std::tuple<SelectorGroupingReadingCase, bool>>
{
protected:
    void SetUp() override;

    const PagedArticleSelectorCase *m_initial{};
};

void ArticleSelectorGroupingReadingTest::SetUp()
{
    ArticleSelectorPresentationReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    m_initial = &configure_presentation(input.initial, erase_screen);
}

class ArticleSelectorToggleGroupingReadingTest : public ArticleSelectorGroupingReadingTest
{
protected:
    void SetUp() override;
};

void ArticleSelectorToggleGroupingReadingTest::SetUp()
{
    ArticleSelectorGroupingReadingTest::SetUp();
    const SelectorGroupingReadingCase &input = std::get<0>(GetParam());
    if (input.initial == SM_ARTICLE)
    {
        g_sel.thread_mode = input.target;
        g_sel.thread_sort = SS_DATE;
        g_use_threads = g_threaded_group = input.target == SM_THREAD;
    }
}

struct SelectorCleanupState
{
    std::vector<int> pending;
    std::vector<int> delayed;
    int              delayed_count;
};

class ArticleSelectorCleanupReadingTest : public PagedArticleSelectorReadingStateTest,
                                          public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool>>
{
protected:
    void SetUp() override;
    void expect_marks(std::size_t index, const std::vector<int> &pending, const std::vector<int> &delayed = {}) const;
    void expect_read(std::size_t index, const std::vector<int> &articles) const;
    void expect_saved(std::string_view range, const std::vector<int> &read) const;
    std::vector<int> completed_with(std::vector<int> read, const std::vector<int> &articles) const;

    std::vector<int>                  m_gamma;
    std::vector<int>                  m_beta;
    std::vector<int>                  m_delta;
    std::vector<SelectorCleanupState> m_cleanup;
};

void ArticleSelectorCleanupReadingTest::SetUp()
{
    PagedArticleSelectorReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    configure_pages(input, erase_screen);
    m_gamma = input.mode == SM_THREAD ? std::vector<int>{4, 9} : std::vector<int>{4};
    m_beta = input.mode == SM_ARTICLE ? std::vector<int>{3} : std::vector<int>{3, 8};
    m_delta = input.mode == SM_ARTICLE ? std::vector<int>{5} : std::vector<int>{5, 10};
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        SelectorCleanupState state{{}, {}, g_dm_count};
        for (int number = 1; number <= 11; ++number)
        {
            const Article *article = g_cache.find(ArticleNum{number});
            if (!article)
            {
                continue;
            }
            if (!g_sel.rereading &&
                ((article->m_flags & AF_DEL) ||
                 ((article->m_flags & AF_UNREAD) && article->m_subj && (article->m_subj->m_flags & SF_DEL))))
            {
                state.pending.push_back(number);
            }
            if (article->m_flags & AF_YANK_BACK)
            {
                state.delayed.push_back(number);
            }
        }
        m_cleanup.push_back(std::move(state));
        return reader(buffer, size);
    };
}

void ArticleSelectorCleanupReadingTest::expect_marks(std::size_t index, const std::vector<int> &pending,
                                                     const std::vector<int> &delayed) const
{
    ASSERT_LT(index, m_cleanup.size());
    EXPECT_EQ(pending, m_cleanup[index].pending);
    EXPECT_EQ(delayed, m_cleanup[index].delayed);
    EXPECT_EQ(delayed.size(), m_cleanup[index].delayed_count);
}

void ArticleSelectorCleanupReadingTest::expect_read(std::size_t index, const std::vector<int> &articles) const
{
    ASSERT_LT(index, m_read.size());
    EXPECT_EQ(articles, m_read[index]);
    EXPECT_EQ(ArticleUnread{11 - static_cast<long>(articles.size())}, m_states[index].unread);
}

void ArticleSelectorCleanupReadingTest::expect_saved(std::string_view range, const std::vector<int> &read) const
{
    EXPECT_EQ(range, g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(ArticleUnread{11 - static_cast<long>(read.size())}, g_newsgroup_ptr->m_to_read);
    std::vector<int> actual;
    for (int number = 1; number <= 11; ++number)
    {
        if (g_cache.was_read(ArticleNum{number}))
        {
            actual.push_back(number);
        }
        EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{number})->m_flags & (AF_YANK_BACK | AF_SEL));
    }
    EXPECT_EQ(read, actual);
    EXPECT_EQ(0, g_dm_count);
    EXPECT_EQ(TR_NONE, g_selected_count);
    EXPECT_EQ(" 1-2", g_newsgroup_data[0].rc_numbers());
    EXPECT_EQ(" 1-2", g_newsgroup_data[2].rc_numbers());
}

std::vector<int> ArticleSelectorCleanupReadingTest::completed_with(std::vector<int>        read,
                                                                   const std::vector<int> &articles) const
{
    read.insert(read.end(), articles.begin(), articles.end());
    std::sort(read.begin(), read.end());
    read.erase(std::unique(read.begin(), read.end()), read.end());
    return read;
}

struct SelectorCacheReadingState
{
    ArticleNum last_cached;
    bool       all_cached;
    int        items;
    int        objects;
    int        total;
    int        prior;
    int        cursor;
    bool       empty_anchor;
};

class ArticleSelectorRecoveryReadingStateTest : public PagedArticleSelectorReadingStateTest
{
protected:
    void SetUp() override;
    void configure_recovery(const PagedArticleSelectorCase &input, bool erase_screen, bool lazy = false);
    void expect_empty(std::size_t index, bool rereading = false) const;

    std::optional<std::size_t>             m_interrupt_input;
    int                                    m_interruptions{};
    std::vector<SelectorCacheReadingState> m_cache;
};

void ArticleSelectorRecoveryReadingStateTest::SetUp()
{
    PagedArticleSelectorReadingStateTest::SetUp();
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        const std::size_t index = m_states.size();
        const bool        empty_anchor = g_sel.mode == SM_ARTICLE
                                             ? !g_sel_page.page_art || g_sel_page.page_art == g_art_order.end()
                                             : !g_sel_page.page_subj || g_sel_page.page_subj->m_misc == 0;
        m_cache.push_back({g_cache.last_cached, g_cache.cached_all_in_range, g_sel_page.page_item_cnt,
                           g_sel_page.page_obj_cnt, g_sel_page.total_obj_cnt, g_sel_page.prior_obj_cnt,
                           g_sel_page.item_index, empty_anchor});
        const int result = reader(buffer, size);
        if (m_interrupt_input && index == *m_interrupt_input)
        {
            EXPECT_FALSE(g_cache.cached_all_in_range);
            g_int_count = 1;
            ++m_interruptions;
        }
        return result;
    };
}

void ArticleSelectorRecoveryReadingStateTest::configure_recovery(const PagedArticleSelectorCase &input,
                                                                 bool erase_screen, bool lazy)
{
    configure_pages(input, erase_screen);
    g_thread_always = !lazy;
}

void ArticleSelectorRecoveryReadingStateTest::expect_empty(std::size_t index, bool rereading) const
{
    expect_page(index, {});
    ASSERT_LT(index, m_cache.size());
    EXPECT_EQ(rereading, m_states[index].selector_rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, m_states[index].selector_mask);
    EXPECT_EQ(TR_NONE, m_states[index].selected);
    EXPECT_EQ(0, m_cache[index].items);
    EXPECT_EQ(0, m_cache[index].objects);
    EXPECT_EQ(0, m_cache[index].total);
    EXPECT_EQ(0, m_cache[index].prior);
    EXPECT_EQ(0, m_cache[index].cursor);
    EXPECT_TRUE(m_cache[index].empty_anchor);
}

class EmptyArticleSelectorReadingTest
    : public ArticleSelectorRecoveryReadingStateTest,
      public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool, bool>>
{
protected:
    void SetUp() override;
};

void EmptyArticleSelectorReadingTest::SetUp()
{
    ArticleSelectorRecoveryReadingStateTest::SetUp();
    const auto &[input, erase_screen, no_files] = GetParam();
    configure_recovery(input, erase_screen);
    set_read_range("1-11");
    if (no_files)
    {
        empty_spool();
        set_read_range("1-0");
    }
}

class EmptyReadArticleSelectorReadingTest
    : public ArticleSelectorRecoveryReadingStateTest,
      public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool>>
{
protected:
    void SetUp() override;
};

void EmptyReadArticleSelectorReadingTest::SetUp()
{
    ArticleSelectorRecoveryReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    configure_recovery(input, erase_screen);
    std::ofstream article{m_output_dir / "comp/lang/cpp/1", std::ios::binary};
    article << "From: Reader <reader@example.test>\nSubject: Opening\n"
               "Date: 21 Oct 2026 09:00:00 GMT\nMessage-ID: <page1@example.test>\n\n";
    for (int line = 1; line <= 80; ++line)
    {
        article << "Partial selector body line " << line << '\n';
    }
    article << "Partial selector body END\n";
}

class InterruptedArticleSelectorReadingTest
    : public ArticleSelectorRecoveryReadingStateTest,
      public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool, bool, bool>>
{
protected:
    void SetUp() override;
    char interrupt_entry();
    void expect_interrupted_return() const;
};

void InterruptedArticleSelectorReadingTest::SetUp()
{
    ArticleSelectorRecoveryReadingStateTest::SetUp();
    const auto &[input, erase_screen, selected_only, rereading] = GetParam();
    configure_recovery(input, erase_screen, true);
    set_read_range("1-4");
    fs::current_path(m_output_dir);
    ASSERT_EQ(1, access_newsgroup());
    if (selected_only)
    {
        ASSERT_TRUE(parse_header(ArticleNum{5}));
        g_cache.ptr(ArticleNum{5})->select_article(AUTO_KILL_NONE);
    }
    ASSERT_TRUE(parse_header(ArticleNum{4}));
    g_nav.target.num = g_nav.current.num = ArticleNum{4};
    g_nav.target.art = g_nav.current.art = g_cache.ptr(ArticleNum{4});
    g_nav.recent.num = ArticleNum{11};
    g_nav.recent.art = g_cache.ptr(ArticleNum{11});
    g_in_ng = true;
    set_mode(GM_READ, MM_ARTICLE);
}

char InterruptedArticleSelectorReadingTest::interrupt_entry()
{
    prepare_commands({"Q"});
    EXPECT_LT(g_cache.last_cached, g_last_art);
    EXPECT_FALSE(g_cache.ptr(ArticleNum{6})->m_flags & AF_CACHED);
    g_int_count = 1;
    testing::internal::CaptureStdout();
    const char result = article_selector(std::get<3>(GetParam()) ? 'U' : '+');
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

void InterruptedArticleSelectorReadingTest::expect_interrupted_return() const
{
    const bool selected_only = std::get<2>(GetParam());
    EXPECT_TRUE(m_states.empty());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(ArticleNum{4}, g_nav.target.num);
    EXPECT_EQ(ArticleNum{4}, g_nav.current.num);
    EXPECT_EQ(g_cache.ptr(ArticleNum{4}), g_nav.target.art);
    EXPECT_EQ(g_nav.target.art, g_nav.current.art);
    EXPECT_EQ(ArticleNum{11}, g_nav.recent.num);
    EXPECT_EQ(g_cache.ptr(ArticleNum{11}), g_nav.recent.art);
    EXPECT_EQ(selected_only, g_sel.selected_only);
    EXPECT_FALSE(g_sel.rereading);
    EXPECT_EQ(AGF_SEL, g_sel.mask);
    EXPECT_EQ(ArticleUnread{7}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(ArticleUnread{selected_only ? 1 : 0}, g_selected_count);
    std::vector<int> selected;
    std::vector<int> read;
    for (int number = 1; number <= 11; ++number)
    {
        const Article *article = g_cache.ptr(ArticleNum{number});
        if (article->m_flags & AF_SEL)
        {
            selected.push_back(number);
        }
        if (g_cache.was_read(ArticleNum{number}))
        {
            read.push_back(number);
        }
    }
    EXPECT_EQ(selected_only ? std::vector<int>{5} : std::vector<int>{}, selected);
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4}), read);
    EXPECT_EQ(0, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_int_count);
    EXPECT_LT(g_cache.last_cached, g_last_art);
    EXPECT_FALSE(g_cache.cached_all_in_range);
    EXPECT_FALSE(macro_pending());
}

class InterruptedSelectorRestartReadingTest
    : public ArticleSelectorRecoveryReadingStateTest,
      public testing::WithParamInterface<std::tuple<PagedArticleSelectorCase, bool>>
{
protected:
    void SetUp() override;

    std::vector<int> m_delta;
    std::vector<int> m_unread_page;
};

void InterruptedSelectorRestartReadingTest::SetUp()
{
    ArticleSelectorRecoveryReadingStateTest::SetUp();
    const auto &[input, erase_screen] = GetParam();
    configure_recovery(input, erase_screen, true);
    set_read_range("1-3");
    m_interrupt_input = 2;
    m_delta = input.mode == SM_ARTICLE ? std::vector<int>{5} : std::vector<int>{5, 10};
    m_unread_page = input.mode == SM_ARTICLE ? std::vector<int>{5, 6} : std::vector<int>{5, 10, 6, 11};
}

struct SelectorEntryReadingCase
{
    const char   *name;
    const char   *command;
    SelectionMode default_mode;
    SelectionMode expected_mode;
    bool          threaded;
};

void PrintTo(const SelectorEntryReadingCase &value, std::ostream *output)
{
    *output << value.name;
}

class NewsgroupSelectorShortcutReadingTest
    : public NewsgroupSelectorReadingStateTest,
      public testing::WithParamInterface<std::tuple<SelectorEntryReadingCase, bool>>
{
protected:
    void SetUp() override;
};

void NewsgroupSelectorShortcutReadingTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto &[entry, erase_screen] = GetParam();
    configure_selector(entry.expected_mode, erase_screen);
    g_sel.default_mode = entry.default_mode;
    g_use_threads = g_threaded_group = entry.threaded;
    g_sel.set_selector(entry.default_mode, SS_MAGIC_NUMBER);
    set_read_range("");
}

class NewsgroupReadSelectorEntryTest : public NewsgroupSelectorReadingStateTest,
                                       public testing::WithParamInterface<std::tuple<SelectionMode, bool, bool>>
{
protected:
    void SetUp() override;
};

void NewsgroupReadSelectorEntryTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto &[mode, erase_screen, at_end] = GetParam();
    configure_selector(mode, erase_screen);
    if (at_end)
    {
        set_read_range("1-2");
    }
}

class NewsgroupSelectorCancelReadingTest : public NewsgroupSelectorReadingStateTest,
                                           public testing::WithParamInterface<std::tuple<SelectionMode, bool, char>>
{
protected:
    void        SetUp() override;
    void        expect_restored_context(std::size_t before, std::size_t after) const;
    std::string exit_command() const;
};

void NewsgroupSelectorCancelReadingTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto &[mode, erase_screen, exit] = GetParam();
    configure_selector(mode, erase_screen);
    g_dm_count = 0;
}

void NewsgroupSelectorCancelReadingTest::expect_restored_context(std::size_t before, std::size_t after) const
{
    ASSERT_LT(before, m_states.size());
    ASSERT_LT(after, m_states.size());
    EXPECT_EQ(m_states[before].mode, m_states[after].mode);
    EXPECT_EQ(m_states[before].article, m_states[after].article);
    EXPECT_EQ(m_states[before].current, m_states[after].current);
    EXPECT_EQ(m_states[before].recent, m_states[after].recent);
    EXPECT_EQ(m_states[before].selected_only, m_states[after].selected_only);
    EXPECT_FALSE(m_states[after].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[after].selector_mask);
}

std::string NewsgroupSelectorCancelReadingTest::exit_command() const
{
    return std::string(1, std::get<2>(GetParam()));
}

using SelectorReadingExit = std::tuple<char, InputNewsgroupResult, DoNewsgroupResult, std::size_t>;

class NewsgroupSelectorReadingExitTest
    : public NewsgroupSelectorReadingStateTest,
      public testing::WithParamInterface<std::tuple<SelectionMode, bool, SelectorReadingExit>>
{
protected:
    void SetUp() override;
};

void NewsgroupSelectorReadingExitTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto &[mode, erase_screen, exit] = GetParam();
    configure_selector(mode, erase_screen);
}

struct LocalArrivalReadingState
{
    ArticleNum last;
    int        static_policy;
    int        added;
};

class LocalNewsgroupArrivalTest : public NewsgroupSelectorReadingStateTest,
                                  public testing::WithParamInterface<std::tuple<bool, bool, int>>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void publish_articles();
    void expect_arrival_bodies(int expected) const;

    int                                   m_arrival_count{};
    bool                                  m_force_refresh{};
    std::vector<LocalArrivalReadingState> m_arrival_states;

private:
    bool m_old_force_grow{g_force_grow};
};

void LocalNewsgroupArrivalTest::SetUp()
{
    NewsgroupEndReadingStateTest::SetUp();
    const auto [threaded, verbose, count] = GetParam();
    configure_selector(threaded ? SM_THREAD : SM_ARTICLE, false);
    g_verbose = verbose;
    g_force_grow = false;
    m_arrival_count = count;
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        if (m_arrival_states.empty())
        {
            publish_articles();
            if (m_force_refresh)
            {
                g_force_grow = true;
            }
        }
        m_arrival_states.push_back({g_last_art, g_keep_the_group_static, g_added_articles});
        return reader(buffer, size);
    };
}

void LocalNewsgroupArrivalTest::TearDown()
{
    NewsgroupSelectorReadingStateTest::TearDown();
    g_force_grow = m_old_force_grow;
}

void LocalNewsgroupArrivalTest::publish_articles()
{
    for (int number = 4; number < 4 + m_arrival_count; ++number)
    {
        std::ofstream{m_output_dir / "comp/lang/cpp" / std::to_string(number), std::ios::binary}
            << "From: Arrival <arrival@example.test>\nSubject: Arrival " << number << "\nDate: 21 Oct 2026 "
            << number + 9 << ":00:00 GMT\nMessage-ID: <arrival" << number
            << "@example.test>\nReferences: <root@example.test>\n\nArrival body " << number << '\n';
    }
    std::ofstream{m_output_dir / "active", std::ios::binary} << "comp.previous 3 1 y\ncomp.lang.cpp "
                                                             << 3 + m_arrival_count << " 1 y\ncomp.next 3 1 y\n";
}

void LocalNewsgroupArrivalTest::expect_arrival_bodies(int expected) const
{
    for (int number = 4; number < 4 + m_arrival_count; ++number)
    {
        expect_body_count("Arrival body " + std::to_string(number), expected);
    }
}

struct PagerReadingState
{
    MinorMode     mode;
    ArticleNum    article;
    bool          read;
    ArticleUnread unread;
};

class NewsgroupPagerReadingTest : public InputNewsgroupReadingTest, public testing::WithParamInterface<bool>
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    DoNewsgroupResult run_reading(std::string_view commands);

    std::vector<PagerReadingState> m_states;

private:
    TerminalInputReader m_old_input_reader{g_terminal_input_reader};
    bool                m_old_allow_typeahead{g_allow_typeahead};
    std::string         m_commands;
    std::size_t         m_input_pos{};
};

void NewsgroupPagerReadingTest::SetUp()
{
    InputNewsgroupReadingTest::SetUp();
    g_tc_LINES = 12;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_erase_screen = GetParam();
    g_allow_typeahead = true;
    std::ofstream article{m_output_dir / "comp/lang/cpp/2", std::ios::binary};
    article << "From: Reply <reply@example.test>\n"
               "Subject: Re: Topic\n"
               "Date: 21 Oct 2026 11:00:00 GMT\n"
               "Message-ID: <reply@example.test>\n"
               "References: <root@example.test>\n\n";
    for (int line = 1; line <= 12; ++line)
    {
        article << "Paged body " << line << '\n';
    }
    g_terminal_input_reader = [this](char *buffer, int)
    {
        m_states.push_back({g_mode, g_nav.target.num, g_cache.was_read(g_nav.target.num), g_newsgroup_ptr->m_to_read});
        if (g_mode == MM_PAGER)
        {
            EXPECT_EQ(g_nav.target.num, g_parsed_header.parsed_art);
            EXPECT_EQ(g_nav.target.num, g_art_input.open_art);
        }
        if (m_input_pos == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected terminal input request";
            *buffer = 'Q';
            return 1;
        }
        *buffer = m_commands[m_input_pos++];
        if (m_input_pos == m_commands.size())
        {
            push_command("~");
        }
        return 1;
    };
}

void NewsgroupPagerReadingTest::TearDown()
{
    g_terminal_input_reader = m_old_input_reader;
    g_allow_typeahead = m_old_allow_typeahead;
    InputNewsgroupReadingTest::TearDown();
}

DoNewsgroupResult NewsgroupPagerReadingTest::run_reading(std::string_view commands)
{
    m_commands = commands;
    errno = 0;
    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::nullopt);
    m_output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(m_commands.size(), m_input_pos);
    EXPECT_TRUE(macro_pending());
    EXPECT_EQ('~', read_tty_char());
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_EQ(nullptr, g_art_input.fp);
    EXPECT_EQ(g_nav.target.num, g_nav.current.num);
    EXPECT_EQ(g_nav.target.art, g_nav.current.art);
    EXPECT_EQ(g_cache.find(g_nav.target.num), g_nav.target.art);
    return result;
}

class LocalArticleFailureReadingTest : public InputNewsgroupReadingTest
{
protected:
    void              TearDown() override;
    DoNewsgroupResult run_reading(std::string_view commands, std::optional<std::string> start_command = std::nullopt);

    std::FILE *m_locked_file{};

private:
    TerminalInputReader m_old_input_reader{g_terminal_input_reader};
};

void LocalArticleFailureReadingTest::TearDown()
{
    if (m_locked_file)
    {
        std::fclose(m_locked_file);
    }
    g_terminal_input_reader = m_old_input_reader;
    InputNewsgroupReadingTest::TearDown();
}

DoNewsgroupResult LocalArticleFailureReadingTest::run_reading(std::string_view           commands,
                                                              std::optional<std::string> start_command)
{
    push_command(commands);
    errno = 0;
    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::move(start_command));
    m_output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_EQ(nullptr, g_art_input.fp);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    return result;
}

class LocalArticleAccessFailureTest : public LocalArticleFailureReadingTest, public testing::WithParamInterface<bool>
{
};

using HeaderlessLocalArticle = std::pair<std::string_view, std::string_view>;

class HeaderlessLocalArticleTest : public LocalArticleFailureReadingTest,
                                   public testing::WithParamInterface<HeaderlessLocalArticle>
{
};

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

class NewsgroupSelectorDelegationStateTest : public InputNewsgroupReadingTest
{
protected:
    void         SetUp() override;
    void         TearDown() override;
    void         prepare_selector(bool rereading = false);
    DisplayState run_delegated(char command, std::string_view input);
    char         run_selector_session(std::string_view input);
    void         expect_selector_context(bool rereading = false, int direction = 1) const;
    void         expect_groups_preserved() const;
    void         expect_next_command(char command);

private:
    void queue_input(std::string_view input);

    std::array<std::string, 3>    m_numbers;
    std::array<ArticleUnread, 3>  m_counts{};
    std::array<NewsgroupFlags, 3> m_flags{};
    NewsgroupData                *m_old_page{g_sel_page.page_ngd};
    NewsgroupData                *m_old_next{g_sel_page.next_ngd};
    NewsgroupData                *m_old_recent{g_recent_newsgroup};
    SelectionSortMode             m_old_group_sort{g_sel.newsgroup_sort};
    std::string                   m_old_display{g_sel_grp_display_mode};
    std::size_t                   m_old_display_index{g_sel_grp_display_mode_index};
    std::string                   m_old_group_commands{g_newsgroup_sel_cmds};
    std::string                   m_old_option_commands{g_option_sel_cmds};
    OptionFlags                  *m_old_options{g_opt_edit.flags};
    int                           m_old_option_page{g_sel_page.page_op};
    bool                          m_old_can_home{g_can_home};
    bool                          m_old_numbered{g_use_sel_num};
    bool                          m_old_bos{g_bos_on_stop};
    int                           m_old_mouse_count{g_mouse_bar_cnt};
    int                           m_old_mouse_width{g_mouse_bar_width};
    std::vector<OptionFlags>      m_options;
};

void NewsgroupSelectorDelegationStateTest::SetUp()
{
    InputNewsgroupReadingTest::SetUp();
    g_sel_page.page_ngd = g_sel_page.next_ngd = nullptr;
    g_recent_newsgroup = &g_newsgroup_data[0];
    g_sel.newsgroup_sort = SS_STRING;
    g_sel_grp_display_mode = "s";
    g_sel_grp_display_mode_index = 0;
    g_newsgroup_sel_cmds = g_option_sel_cmds = "Z>";
    g_can_home = false;
    g_use_sel_num = false;
    g_mouse_bar_cnt = g_mouse_bar_width = 0;
    const OptionCatalog catalog;
    m_options.assign(catalog.row_count() + 1, OF_NONE);
    g_opt_edit.flags = m_options.data();
    search_init();
    g_only.push_only();
}

void NewsgroupSelectorDelegationStateTest::TearDown()
{
    pop_only();
    g_sel_page.page_ngd = m_old_page;
    g_sel_page.next_ngd = m_old_next;
    g_recent_newsgroup = m_old_recent;
    g_sel.newsgroup_sort = m_old_group_sort;
    g_sel_grp_display_mode = m_old_display;
    g_sel_grp_display_mode_index = m_old_display_index;
    g_newsgroup_sel_cmds = m_old_group_commands;
    g_option_sel_cmds = m_old_option_commands;
    g_opt_edit.flags = m_old_options;
    g_sel_page.page_op = m_old_option_page;
    g_can_home = m_old_can_home;
    g_use_sel_num = m_old_numbered;
    g_bos_on_stop = m_old_bos;
    g_mouse_bar_cnt = m_old_mouse_count;
    g_mouse_bar_width = m_old_mouse_width;
    InputNewsgroupReadingTest::TearDown();
}

void NewsgroupSelectorDelegationStateTest::prepare_selector(bool rereading)
{
    g_general_mode = GM_SELECTOR;
    g_mode = MM_NEWSGROUP_SELECTOR;
    g_sel.rereading = rereading;
    g_sel.exclusive = false;
    g_sel.mask = rereading ? AGF_DEL_SEL : AGF_SEL;
    g_newsgroup_min_to_read = TR_ONE;
    g_newsgroup_to_read = NewsgroupNum{rereading ? 0 : 3};
    for (int index = 0; index < 3; ++index)
    {
        NewsgroupData &group = g_newsgroup_data[index];
        group.m_to_read = rereading ? 0 : (index == 1 ? 2 : 1);
        group.m_flags = index == 1 ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE;
        if (rereading)
        {
            group.m_rc_line.resize(group.m_num_offset);
            group.m_rc_line += " 1-3";
        }
    }
    g_sel.set_selector(SM_NEWSGROUP, SS_STRING);
    g_sel_page.page_ngd = &g_newsgroup_data[1];
    init_pages(PRESERVE_PAGE);
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
    g_sel_page.item_index = g_sel_page.page_item_cnt;
    ASSERT_EQ(DS_DISPLAY, g_sel_session.command(Ctl('l')));
    g_sel_page.item_index = 0;
    set_newsgroup(&g_newsgroup_data[1]);
    g_sel_page.last_line = 3;
    g_term_line = 3;
    for (int index = 0; index < 3; ++index)
    {
        m_numbers[index] = g_newsgroup_data[index].rc_numbers();
        m_counts[index] = g_newsgroup_data[index].m_to_read;
        m_flags[index] = g_newsgroup_data[index].m_flags;
    }
}

void NewsgroupSelectorDelegationStateTest::queue_input(std::string_view input)
{
    for (auto ch = input.rbegin(); ch != input.rend(); ++ch)
    {
        g_pushback.push(static_cast<unsigned char>(*ch) | 0200);
    }
}

DisplayState NewsgroupSelectorDelegationStateTest::run_delegated(char command, std::string_view input)
{
    queue_input(input);
    testing::internal::CaptureStdout();
    const DisplayState result = newsgroup_commands(command);
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

char NewsgroupSelectorDelegationStateTest::run_selector_session(std::string_view input)
{
    queue_input(input);
    testing::internal::CaptureStdout();
    const char result = newsgroup_selector();
    m_output = testing::internal::GetCapturedStdout();
    return result;
}

void NewsgroupSelectorDelegationStateTest::expect_selector_context(bool rereading, int direction) const
{
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_EQ(SS_STRING, g_sel.sort);
    EXPECT_EQ(direction, g_sel.direction);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
}

void NewsgroupSelectorDelegationStateTest::expect_groups_preserved() const
{
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(m_numbers[index], g_newsgroup_data[index].rc_numbers()) << index;
        EXPECT_EQ(m_counts[index], g_newsgroup_data[index].m_to_read) << index;
        EXPECT_EQ(m_flags[index], g_newsgroup_data[index].m_flags) << index;
        EXPECT_EQ(':', g_newsgroup_data[index].m_subscribe_char) << index;
    }
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_EQ(1, g_selected_count);
}

void NewsgroupSelectorDelegationStateTest::expect_next_command(char command)
{
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ(command, read_tty_char() & 0177);
    EXPECT_FALSE(macro_pending());
}

class NewsgroupSelectorKillEditorTest : public NewsgroupSelectorDelegationStateTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string editor_text(const char *name) const;

    fs::path m_kill_file;

private:
    using SignalHandler = void (*)(int);
    trn::testing::MockEnvironment              m_env;
    std::vector<std::pair<int, SignalHandler>> m_signals;
    std::FILE                                 *m_old_local{g_local_kfp};
    bool                                       m_old_waiting{g_waiting};
    bool                                       m_old_no_wait{g_no_wait_fork};
    bool                                       m_old_bizarre{g_bizarre};
};

void NewsgroupSelectorKillEditorTest::SetUp()
{
    EXPECT_CALL(m_env.getter, Call(testing::_)).WillRepeatedly(testing::Invoke(std::getenv));
    NewsgroupSelectorDelegationStateTest::SetUp();
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
    g_local_kfp = nullptr;
    const std::string editor =
#ifdef USE_SPAWN_SHELL
        "call "
#endif
        "\"" TRN_TEST_PROCESS_TOOL "\" kill-editor";
    m_kill_file = m_output_dir / "global rules/KILL";
    fs::create_directories(m_kill_file.parent_path());
    std::ofstream(m_kill_file, std::ios::binary) << "# Global rules\n";
    m_env.expect_env_repeatedly("VISUAL", editor.c_str());
    m_env.expect_env_repeatedly("EDITOR", "unused-editor");
    m_env.expect_env_repeatedly("KILLGLOBAL", m_kill_file.generic_string().c_str());
    m_env.expect_env_repeatedly("KILLLOCAL", (m_output_dir / "unused-local").generic_string().c_str());
    m_env.expect_env_repeatedly("PNEWS_TEST_DIRECTORY", m_output_dir.generic_string().c_str());
    m_env.expect_env_repeatedly("PNEWS_TEST_MODE", "");
    m_env.expect_no_envars({"NNTPSERVER", "NNTP_FORCE_AUTH", "NEWSACTIVE", "NEWSDESCRIPTIONS", "QUOTECHARS"});
    prepare_selector();
}

void NewsgroupSelectorKillEditorTest::TearDown()
{
    m_env.expect_env_repeatedly("KILLGLOBAL", (m_output_dir / "absent-global").generic_string().c_str());
    open_kill_file(KF_GLOBAL);
    for (const std::pair<int, SignalHandler> &signal : m_signals)
    {
        std::signal(signal.first, signal.second);
    }
    g_local_kfp = m_old_local;
    g_waiting = m_old_waiting;
    g_no_wait_fork = m_old_no_wait;
    g_bizarre = m_old_bizarre;
    NewsgroupSelectorDelegationStateTest::TearDown();
}

std::string NewsgroupSelectorKillEditorTest::editor_text(const char *name) const
{
    std::ifstream input{m_output_dir / name};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

TEST_F(NewsgroupSelectorKillEditorTest, editingTheGlobalRulesRedisplaysTheSameSelectedNewsgroup)
{
    NewsgroupData *const cursor = g_newsgroup_ptr;
    const ArticleLocation article_location = g_nav.target;

    const char result = run_selector_session(std::string(1, Ctl('k')) + "qQ");

    EXPECT_EQ('q', result);
    EXPECT_EQ("# Global rules\n", editor_text("kill-editor-input"));
    EXPECT_EQ(m_kill_file.generic_string(), editor_text("kill-editor-file"));
    EXPECT_EQ(m_output_dir.generic_string(), editor_text("kill-editor-directory"));
    EXPECT_EQ("kill-editor\n" + m_kill_file.generic_string() + '\n', editor_text("tools.log"));
    EXPECT_EQ("# Global rules\n/controlled/:j\n", editor_text("global rules/KILL"));
    EXPECT_FALSE(fs::exists(m_output_dir / "unused-local"));
    EXPECT_EQ(nullptr, g_local_kfp);
    EXPECT_EQ(cursor, g_newsgroup_ptr);
    EXPECT_EQ(article_location, g_nav.target);
    EXPECT_FALSE(g_in_ng);
    EXPECT_EQ(&m_multirc, g_multirc);
    EXPECT_EQ(&m_data_source, g_data_source);
    const std::size_t editing = m_output.find("Editing global KILL file:");
    ASSERT_NE(std::string::npos, editing);
    EXPECT_NE(std::string::npos, m_output.find("comp.lang.cpp", editing));
    expect_selector_context();
    expect_groups_preserved();
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorDelegationStateTest, versionInformationReturnsToTheSameSelectedNewsgroup)
{
    m_newsrc.flags |= RF_ACTIVE;
    m_newsrc.name = "selector.newsrc";
    m_data_source.m_name = "selector-source";
    m_data_source.m_news_id = "active";
    g_tc_COLS = 240;
    g_verbose = true;
    prepare_selector();
    NewsgroupData *const cursor = g_newsgroup_ptr;
    const ArticleLocation article_location = g_nav.target;

    const char result = run_selector_session("\\v qQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("Trn version: " + g_patch_level + '.'));
    EXPECT_THAT(m_output, testing::HasSubstr("News source group #1:"));
    EXPECT_THAT(m_output, testing::HasSubstr("ID selector-source:"));
    EXPECT_THAT(m_output, testing::HasSubstr("Newsrc " + m_newsrc.name.generic_string() + '.'));
    EXPECT_THAT(m_output, testing::HasSubstr("News from " + m_data_source.m_spool_dir + '.'));
    EXPECT_THAT(m_output, testing::HasSubstr("Local active file active."));
    EXPECT_THAT(m_output, testing::HasSubstr("trn-workers@lists.sourceforge.net"));
    const std::size_t pause = m_output.find("[Type space or a command]");
    ASSERT_NE(std::string::npos, pause);
    EXPECT_NE(std::string::npos, m_output.find("comp.lang.cpp", pause));
    EXPECT_EQ(cursor, g_newsgroup_ptr);
    EXPECT_EQ(article_location, g_nav.target);
    EXPECT_EQ(&m_multirc, g_multirc);
    EXPECT_EQ(&m_data_source, g_data_source);
    expect_selector_context();
    expect_groups_preserved();
    expect_next_command('Q');
}

struct MultircSelectorPrompt
{
    MinorMode                mode;
    int                      source;
    std::string              group;
    std::vector<std::string> rows;
    std::vector<std::string> ranges;
    std::vector<std::string> restrictions;
    ArticleNum               article;
};

class MultircSelectorStateTest : public NewsgroupSelectorDelegationStateTest
{
protected:
    void               SetUp() override;
    void               TearDown() override;
    void               run_outer(std::string commands, bool restore_old = false);
    void               prepare_sources();
    void               expect_modes(std::vector<MinorMode> modes) const;
    static std::string read_file(const fs::path &path);

    std::vector<MultircSelectorPrompt> m_prompts;
    std::array<Newsrc, 2>              m_other_newsrcs{};
    std::array<DataSource, 2>          m_other_sources{};
    const std::string                  m_first_text{"comp.previous: 1-2\ncomp.lang.cpp: 1\ncomp.next: 1-2\n"};
    const std::string                  m_second_text{"comp.previous: 1-3\ncomp.lang.cpp: 1-2\ncomp.next: 1-3\n"};
    const std::string                  m_third_text{"comp.previous: 1-2\ncomp.lang.cpp: 1-3\ncomp.next: 1-3\n"};

private:
    std::optional<MultircLoopContext> m_loop_context;
    TerminalInputReader               m_old_input{g_terminal_input_reader};
    bool                              m_old_typeahead{g_allow_typeahead};
    bool                              m_old_check{g_check_flag};
    bool                              m_old_suppress{g_suppress_cn};
    std::string                       m_old_last_group{g_last_run.newsgroup_name};
    long                              m_old_last_time{g_last_run.time};
    long                              m_old_last_new_time{g_last_run.new_time};
    long                              m_old_last_active{g_last_run.active_size};
    long                              m_old_last_extra{g_last_run.extra_num};
    std::vector<Multirc>              m_old_multircs;
    bool                              m_sources_prepared{};
    std::string                       m_commands;
    std::size_t                       m_input_pos{};
};

void MultircSelectorStateTest::SetUp()
{
    NewsgroupSelectorDelegationStateTest::SetUp();
    m_loop_context.emplace(m_newsrc, m_output_dir, false);
    m_newsrc.old_name = m_output_dir / "newsrc.old";
    std::ofstream{m_newsrc.name} << m_first_text;
    std::ofstream{m_newsrc.old_name} << m_first_text;
    m_data_source.m_name = "first";
    m_data_source.m_flags = DF_OPEN | DF_ACTIVE;
    g_use_newsgroup_selector = true;
    g_newsgroup_min_to_read = TR_ONE;
    g_thread_always = true;
    g_allow_typeahead = true;
    g_check_flag = false;
    g_suppress_cn = false;
    g_newsgroup_to_read = NewsgroupNum{};
    for (NewsgroupData &group : g_newsgroup_data)
    {
        group.set_to_read(ST_LAX);
    }
    g_terminal_input_reader = [this](char *buffer, int)
    {
        MultircSelectorPrompt prompt{};
        prompt.mode = g_mode;
        prompt.source = g_multirc ? g_multirc->m_num : -1;
        prompt.group = g_newsgroup_ptr ? std::string{g_newsgroup_ptr->rc_name()} : "";
        prompt.restrictions = multirc_restrictions();
        prompt.article = g_in_ng ? g_nav.target.num : ArticleNum{};
        for (const NewsgroupData &group : g_newsgroup_data)
        {
            prompt.ranges.emplace_back(group.rc_numbers());
        }
        if (g_general_mode == GM_SELECTOR && g_sel.mode == SM_NEWSGROUP)
        {
            for (int index = 0; index < g_sel_page.page_item_cnt; ++index)
            {
                prompt.rows.emplace_back(g_sel_page.items[index].u.np->rc_name());
            }
        }
        m_prompts.push_back(std::move(prompt));
        if (m_input_pos == m_commands.size())
        {
            ADD_FAILURE() << "Unexpected outer selector-session input";
            *buffer = 'q';
        }
        else
        {
            *buffer = m_commands[m_input_pos++];
        }
        return 1;
    };
}

void MultircSelectorStateTest::TearDown()
{
    if (m_sources_prepared)
    {
        unuse_multirc(g_multirc);
        g_multircs = std::move(m_old_multircs);
        for (DataSource &source : m_other_sources)
        {
            source.close();
        }
    }
    m_data_source.m_act_sf.close();
    g_terminal_input_reader = m_old_input;
    g_allow_typeahead = m_old_typeahead;
    g_check_flag = m_old_check;
    g_suppress_cn = m_old_suppress;
    g_last_run.newsgroup_name = m_old_last_group;
    g_last_run.time = m_old_last_time;
    g_last_run.new_time = m_old_last_new_time;
    g_last_run.active_size = m_old_last_active;
    g_last_run.extra_num = m_old_last_extra;
    m_loop_context.reset();
    NewsgroupSelectorDelegationStateTest::TearDown();
}

void MultircSelectorStateTest::run_outer(std::string commands, bool restore_old)
{
    m_commands = std::move(commands);
    errno = 0;

    testing::internal::CaptureStdout();
    do_multirc();
    m_output = testing::internal::GetCapturedStdout();

    SCOPED_TRACE(m_output);
    EXPECT_EQ(m_commands.size(), m_input_pos);
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_EQ(nullptr, g_art_input.fp);
    EXPECT_EQ(restore_old, g_restore_old_newsrc);
}

void MultircSelectorStateTest::prepare_sources()
{
    m_old_multircs = std::move(g_multircs);
    g_multircs = {{&m_newsrc, 1, MF_NONE}, {&m_other_newsrcs[0], 2, MF_NONE}, {&m_other_newsrcs[1], 3, MF_NONE}};
    g_multirc = &g_multircs[0];
    m_sources_prepared = true;
    for (std::size_t index = 0; index < m_other_newsrcs.size(); ++index)
    {
        DataSource &source = m_other_sources[index];
        source.m_name = index == 0 ? "second" : "third";
        source.m_flags = DF_OPEN;
        source.m_spool_dir = m_data_source.m_spool_dir;
        source.m_act_sf.m_refetch_secs = DEFAULT_REFETCH_SECS;
        ASSERT_EQ(1, source.m_act_sf.open(m_output_dir / "active", "", ""));
        Newsrc &newsrc = m_other_newsrcs[index];
        newsrc.data_source = &source;
        newsrc.name = m_output_dir / (source.m_name + "-newsrc");
        newsrc.old_name = newsrc.name;
        newsrc.old_name += ".old";
        newsrc.new_name = newsrc.name;
        newsrc.new_name += ".new";
        newsrc.info_name = newsrc.name;
        newsrc.info_name += ".info";
        std::ofstream{newsrc.name} << (index == 0 ? m_second_text : m_third_text);
    }
}

void MultircSelectorStateTest::expect_modes(std::vector<MinorMode> modes) const
{
    SCOPED_TRACE(m_output);
    ASSERT_EQ(modes.size(), m_prompts.size());
    for (std::size_t index = 0; index < modes.size(); ++index)
    {
        EXPECT_EQ(modes[index], m_prompts[index].mode) << index;
    }
}

std::string MultircSelectorStateTest::read_file(const fs::path &path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

class MultircSelectorExitTest : public MultircSelectorStateTest,
                                public testing::WithParamInterface<std::tuple<char, bool>>
{
protected:
    void SetUp() override;
};

void MultircSelectorExitTest::SetUp()
{
    MultircSelectorStateTest::SetUp();
    g_verbose = std::get<1>(GetParam());
}

TEST_P(MultircSelectorExitTest, selectorQuitAndLeaveCommandsReturnThroughTheOuterLoop)
{
    const char  command = std::get<0>(GetParam());
    std::string input{command};
    if (command != 'q')
    {
        input += 'q';
    }

    run_outer(input);

    if (command == 'q')
    {
        expect_modes({MM_NEWSGROUP_SELECTOR});
    }
    else
    {
        expect_modes({MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_LIST});
        EXPECT_EQ("comp.lang.cpp", m_prompts[1].group);
        EXPECT_FALSE(g_use_newsgroup_selector);
    }
    EXPECT_EQ((std::vector<std::string>{"comp.lang.cpp", "comp.next", "comp.previous"}), m_prompts[0].rows);
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.old_name));
    EXPECT_TRUE(fs::exists(m_newsrc.info_name));
}

INSTANTIATE_TEST_SUITE_P(CommandAndVerbosity, MultircSelectorExitTest,
                         testing::Combine(testing::Values('q', 'Q', '`', '\033'), testing::Bool()));

class MultircSelectorSourceTest : public MultircSelectorStateTest,
                                  public testing::WithParamInterface<std::tuple<char, bool>>
{
protected:
    void SetUp() override;
};

void MultircSelectorSourceTest::SetUp()
{
    MultircSelectorStateTest::SetUp();
    g_verbose = std::get<1>(GetParam());
    prepare_sources();
}

TEST_P(MultircSelectorSourceTest, switchingSourcesAndReturningRestoresEachSourcesGroupState)
{
    const char command = std::get<0>(GetParam());
    const char reverse = command == Ctl('n') ? Ctl('p') : Ctl('n');
    g_only.set_to_do("comp.lang.cpp");

    run_outer(std::string{command, reverse, 'q'});

    expect_modes({MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_SELECTOR});
    EXPECT_EQ(1, m_prompts[0].source);
    EXPECT_EQ(command == Ctl('n') ? 2 : 3, m_prompts[1].source);
    EXPECT_EQ(1, m_prompts[2].source);
    EXPECT_EQ(std::vector<std::string>{"comp.lang.cpp"}, m_prompts[0].rows);
    EXPECT_EQ(std::vector<std::string>{command == Ctl('n') ? "comp.lang.cpp" : "comp.previous"}, m_prompts[1].rows);
    EXPECT_EQ((std::vector<std::string>{"comp.lang.cpp", "comp.next", "comp.previous"}), m_prompts[2].rows);
    EXPECT_FALSE(m_prompts[0].restrictions.empty());
    EXPECT_TRUE(m_prompts[1].restrictions.empty());
    EXPECT_TRUE(m_prompts[2].restrictions.empty());
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_second_text, read_file(m_other_newsrcs[0].name));
    EXPECT_EQ(m_third_text, read_file(m_other_newsrcs[1].name));
}

TEST_P(MultircSelectorSourceTest, leavingTheSelectorThenSwitchingSourcesHonorsTheRequestedDirection)
{
    const char command = std::get<0>(GetParam());

    run_outer(std::string{'Q', command, 'q'});

    expect_modes({MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_LIST, MM_NEWSGROUP_LIST});
    EXPECT_EQ(1, m_prompts[1].source);
    EXPECT_EQ(command == Ctl('n') ? 2 : 3, m_prompts[2].source);
    EXPECT_EQ(command == Ctl('n') ? "comp.lang.cpp" : "comp.previous", m_prompts[2].group);
    EXPECT_FALSE(g_use_newsgroup_selector);
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_second_text, read_file(m_other_newsrcs[0].name));
    EXPECT_EQ(m_third_text, read_file(m_other_newsrcs[1].name));
}

TEST_P(MultircSelectorSourceTest, switchingSourcesRetainsTheQueuedQuitAtTheRestartedGroupPrompt)
{
    const char                command = std::get<0>(GetParam());
    const TerminalInputReader reader = g_terminal_input_reader;
    bool                      queued = false;
    g_terminal_input_reader = [&, reader](char *buffer, int count)
    {
        const int result = reader(buffer, count);
        if (!queued && g_mode == MM_NEWSGROUP_LIST && *buffer == command)
        {
            g_pushback.push('q' | 0200);
            queued = true;
        }
        return result;
    };

    run_outer(std::string{'Q', command});

    expect_modes({MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_LIST});
    EXPECT_TRUE(queued);
    EXPECT_EQ(command == Ctl('n') ? 2 : 3, g_multirc->m_num);
    EXPECT_EQ(g_newsgroup_ptr, g_current_newsgroup);
    EXPECT_EQ(command == Ctl('n') ? "comp.lang.cpp" : "comp.previous", std::string{g_newsgroup_ptr->rc_name()});
    EXPECT_FALSE(g_use_newsgroup_selector);
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_second_text, read_file(m_other_newsrcs[0].name));
    EXPECT_EQ(m_third_text, read_file(m_other_newsrcs[1].name));
}

INSTANTIATE_TEST_SUITE_P(DirectionAndVerbosity, MultircSelectorSourceTest,
                         testing::Combine(testing::Values(Ctl('n'), Ctl('p')), testing::Bool()));

class MultircSelectorReadTest : public MultircSelectorStateTest,
                                public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
};

void MultircSelectorReadTest::SetUp()
{
    MultircSelectorStateTest::SetUp();
    const auto [verbose, threaded] = GetParam();
    g_verbose = verbose;
    g_use_threads = threaded;
}

TEST_P(MultircSelectorReadTest, readingFromTheStartupSelectorReturnsToItsPageAndSavesCompletedReads)
{
    run_outer("\nQq");

    expect_modes({MM_NEWSGROUP_SELECTOR, MM_ARTICLE, MM_NEWSGROUP_SELECTOR});
    EXPECT_EQ("comp.lang.cpp", m_prompts[1].group);
    EXPECT_EQ(ArticleNum{2}, m_prompts[1].article);
    EXPECT_EQ(" 1-2", m_prompts[2].ranges[1]);
    EXPECT_EQ("comp.previous: 1-2\ncomp.lang.cpp: 1-2\ncomp.next: 1-2\n", read_file(m_newsrc.name));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.old_name));
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
}

TEST_P(MultircSelectorReadTest, readingAnotherSourceDoesNotChangeTheFirstSourcesReadRanges)
{
    prepare_sources();

    run_outer(std::string{Ctl('n')} + "\nQ" + std::string{Ctl('p')} + "q");

    expect_modes(
        {MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_SELECTOR, MM_ARTICLE, MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_SELECTOR});
    EXPECT_EQ(2, m_prompts[2].source);
    EXPECT_EQ(ArticleNum{3}, m_prompts[2].article);
    EXPECT_TRUE(m_prompts[3].rows.empty());
    EXPECT_EQ(1, m_prompts[4].source);
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ("comp.previous: 1-3\ncomp.lang.cpp: 1-3\ncomp.next: 1-3\n", read_file(m_other_newsrcs[0].name));
    EXPECT_EQ(m_third_text, read_file(m_other_newsrcs[1].name));
}

INSTANTIATE_TEST_SUITE_P(VerbosityAndThreading, MultircSelectorReadTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class MultircSelectorRecoveryExitTest : public MultircSelectorStateTest, public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void MultircSelectorRecoveryExitTest::SetUp()
{
    MultircSelectorStateTest::SetUp();
    g_verbose = GetParam();
    g_newsgroup_data[1].m_rc_line.resize(g_newsgroup_data[1].m_num_offset);
    g_newsgroup_data[1].m_rc_line += " 1-2";
    g_newsgroup_data[1].m_to_read = ArticleUnread{1};
    m_newsrc.flags |= RF_RC_CHANGED;
}

TEST_P(MultircSelectorRecoveryExitTest, confirmedAbandonmentFromStartupRestoresTheBackupAndRetainsPendingChanges)
{
    run_outer("\\xy", true);

    expect_modes({MM_NEWSGROUP_SELECTOR, MM_NEWSGROUP_SELECTOR, MM_ADD_NEWSGROUP_PROMPT});
    EXPECT_THAT(m_output, testing::HasSubstr("The abandoned changes are in"));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ("comp.previous: 1-2\ncomp.lang.cpp: 1-2\ncomp.next: 1-2\n", read_file(m_newsrc.new_name));
    EXPECT_FALSE(fs::exists(m_newsrc.old_name));
}

TEST_P(MultircSelectorRecoveryExitTest, aFailedWriteOnStartupQuitRestoresTheBackup)
{
    fs::create_directory(m_newsrc.new_name);

    run_outer("q", true);

    expect_modes({MM_NEWSGROUP_SELECTOR});
    EXPECT_THAT(m_output, testing::HasSubstr("restoring older version"));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.new_name));
    EXPECT_FALSE(fs::exists(m_newsrc.old_name));
}

INSTANTIATE_TEST_SUITE_P(Verbosity, MultircSelectorRecoveryExitTest, testing::Bool());

class MultircUniversalStartupTest : public MultircSelectorStateTest,
                                    public testing::WithParamInterface<std::tuple<bool, bool>>
{
protected:
    void SetUp() override;
    void TearDown() override;
    void release_child_files();
    void initialize_child();
    void prepare_pending_changes();

private:
    std::string m_old_trn_dir{g_trn_dir};
    std::string m_score_file;
};

void MultircUniversalStartupTest::SetUp()
{
    MultircSelectorStateTest::SetUp();
    const auto [group_selector, verbose] = GetParam();
    g_verbose = verbose;
    g_use_newsgroup_selector = group_selector;
    g_use_univ_selector = true;
    g_trn_dir = (m_output_dir / "private").generic_string();
    fs::create_directories(fs::path{g_trn_dir} / "univ");
    std::ofstream{fs::path{g_trn_dir} / "univ/top"} << "\"Local groups\" comp.*\n";
    m_score_file = (m_output_dir / "saved-scores").generic_string();
}

void MultircUniversalStartupTest::TearDown()
{
    g_trn_dir = m_old_trn_dir;
    MultircSelectorStateTest::TearDown();
}

void MultircUniversalStartupTest::release_child_files()
{
    m_data_source.m_act_sf.close();
    g_back_page.final();
    fs::current_path(m_output_dir.parent_path());
}

void MultircUniversalStartupTest::initialize_child()
{
    ASSERT_EQ(1, m_data_source.m_act_sf.open(m_output_dir / "active", "", ""));
    g_back_page.init();
    univ_init();
    g_bizarre = false;
    g_head_name.clear();
    set_environment([this](const char *name) -> char *
                    { return std::string_view{name} == "SAVESCOREFILE" ? m_score_file.data() : nullptr; });
}

TEST_P(MultircUniversalStartupTest, leavingUniversalStartupContinuesToTheConfiguredNextSelector)
{
    release_child_files();

    EXPECT_EXIT(
        {
            initialize_child();
            if (testing::Test::HasFailure())
            {
                std::exit(2);
            }
            run_outer("Qq");
            expect_modes({MM_UNIVERSAL, std::get<0>(GetParam()) ? MM_NEWSGROUP_SELECTOR : MM_NEWSGROUP_LIST});
            EXPECT_EQ(0, g_univ.level);
            EXPECT_TRUE(g_univ.items.empty());
            EXPECT_THAT(m_output, testing::HasSubstr("Local groups"));
            EXPECT_THAT(m_output, testing::HasSubstr("Top Level"));
            EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
            finalize(testing::Test::HasFailure() ? 1 : 0);
        },
        testing::ExitedWithCode(0), ".*");

    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.old_name));
}

void MultircUniversalStartupTest::prepare_pending_changes()
{
    g_newsgroup_data[1].m_rc_line.resize(g_newsgroup_data[1].m_num_offset);
    g_newsgroup_data[1].m_rc_line += " 1-2";
    g_newsgroup_data[1].m_to_read = ArticleUnread{1};
    m_newsrc.flags |= RF_RC_CHANGED;
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        const int result = reader(buffer, size);
        std::ofstream(m_output_dir / "universal-input", std::ios::app)
            << static_cast<int>(g_mode) << ' ' << g_univ.level << ' ' << g_univ.items.size() << '\n';
        if (testing::Test::HasFailure())
        {
            std::exit(3);
        }
        return result;
    };
}

TEST_P(MultircUniversalStartupTest, quittingUniversalStartupSavesPendingChangesAndFinalizesWithoutAnotherSelector)
{
    m_newsrc.lock_name = m_output_dir / "universal-newsrc.lock";
    std::ofstream{m_newsrc.lock_name} << "private session lock\n";
    release_child_files();

    EXPECT_EXIT(
        {
            initialize_child();
            if (testing::Test::HasFailure())
            {
                std::exit(2);
            }
            prepare_pending_changes();
            run_outer("q");
            std::exit(4);
        },
        testing::ExitedWithCode(0), ".*");

    EXPECT_EQ("comp.previous: 1-2\ncomp.lang.cpp: 1-2\ncomp.next: 1-2\n", read_file(m_newsrc.name));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.old_name));
    EXPECT_EQ(std::to_string(static_cast<int>(MM_UNIVERSAL)) + " 1 1\n", read_file(m_output_dir / "universal-input"));
    EXPECT_FALSE(fs::exists(m_newsrc.lock_name));
    EXPECT_TRUE(fs::exists(m_newsrc.info_name));
}

TEST_P(MultircUniversalStartupTest, aFailedWriteOnUniversalQuitPreservesTheRestoredBackupThroughFinalization)
{
    m_newsrc.lock_name = m_output_dir / "universal-newsrc.lock";
    std::ofstream{m_newsrc.lock_name} << "private session lock\n";
    std::ofstream{m_output_dir / "unrelated"} << "unrelated file\n";
    fs::create_directory(m_newsrc.new_name);
    release_child_files();

    EXPECT_EXIT(
        {
            initialize_child();
            if (testing::Test::HasFailure())
            {
                std::exit(2);
            }
            prepare_pending_changes();
            run_outer("q");
            std::exit(4);
        },
        testing::ExitedWithCode(0), ".*");

    EXPECT_EQ(m_first_text, read_file(m_newsrc.name));
    EXPECT_EQ(m_first_text, read_file(m_newsrc.new_name));
    EXPECT_FALSE(fs::exists(m_newsrc.old_name));
    EXPECT_EQ(std::to_string(static_cast<int>(MM_UNIVERSAL)) + " 1 1\n", read_file(m_output_dir / "universal-input"));
    EXPECT_EQ("unrelated file\n", read_file(m_output_dir / "unrelated"));
    EXPECT_FALSE(fs::exists(m_newsrc.lock_name));
    EXPECT_TRUE(fs::exists(m_newsrc.info_name));
}

INSTANTIATE_TEST_SUITE_P(NextSelectorAndVerbosity, MultircUniversalStartupTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

class NewsgroupSelectorDelegatedRecoveryTest : public NewsgroupSelectorDelegationStateTest
{
protected:
    void TearDown() override;
    void prepare_recovery(bool rereading, bool later_page);
    void expect_page(bool later_page, int cursor) const;

private:
    std::string m_old_message{g_msg};
    int         m_old_scrolled{g_term_scrolled};
};

void NewsgroupSelectorDelegatedRecoveryTest::TearDown()
{
    g_msg = m_old_message;
    g_term_scrolled = m_old_scrolled;
    NewsgroupSelectorDelegationStateTest::TearDown();
}

void NewsgroupSelectorDelegatedRecoveryTest::prepare_recovery(bool rereading, bool later_page)
{
    g_tc_LINES = 7;
    prepare_selector(rereading);
    g_sel_page.page_ngd = &g_newsgroup_data[later_page ? 2 : 1];
    init_pages(PRESERVE_PAGE);
    Selection no_search{};
    no_search.op = OI_NONE;
    (void) calc_page(no_search);
    g_sel_page.item_index = 1;
    g_newsgroup_ptr = &g_newsgroup_data[later_page ? 0 : 2];
    set_newsgroup_name(g_newsgroup_ptr->rc_line());
    g_term_line = g_sel_page.last_line;
    g_term_scrolled = 0;
}

void NewsgroupSelectorDelegatedRecoveryTest::expect_page(bool later_page, int cursor) const
{
    EXPECT_EQ(&g_newsgroup_data[later_page ? 2 : 1], g_sel_page.page_ngd);
    EXPECT_EQ(later_page ? 1 : 0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(cursor, g_sel_page.item_index);
}

class NewsgroupSelectorDelegatedEraseTest : public NewsgroupSelectorDelegatedRecoveryTest,
                                            public testing::WithParamInterface<std::tuple<bool, bool, bool, char>>
{
};

class NewsgroupSelectorDelegatedPromptTest : public NewsgroupSelectorDelegatedRecoveryTest,
                                             public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
};

class NewsgroupSelectorDelegatedOutputTest : public NewsgroupSelectorDelegatedRecoveryTest,
                                             public testing::WithParamInterface<char>
{
};

class NewsgroupSelectorDelegatedMessageTest : public NewsgroupSelectorDelegatedRecoveryTest,
                                              public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
};

class NewsgroupSelectorDelegatedRedisplayTest : public NewsgroupSelectorDelegatedRecoveryTest,
                                                public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorDelegatedExitStateTest : public NewsgroupSelectorDelegatedRecoveryTest
{
protected:
    void               prepare_exit(bool rereading, bool later_page, bool at_end = false);
    void               prepare_selector_session(bool rereading);
    void               remember_pending_changes();
    void               expect_pending_changes() const;
    void               expect_exit_files_unchanged() const;
    void               expect_selector_session_preserved(bool rereading, int direction = 1) const;
    static std::string newsrc_text(const Newsrc &newsrc);
    static std::string read_newsrc(const fs::path &path);

    std::string m_original_newsrc;

private:
    std::array<NewsgroupData, 3> m_pending;
};

void NewsgroupSelectorDelegatedExitStateTest::prepare_exit(bool rereading, bool later_page, bool at_end)
{
    prepare_recovery(rereading, later_page);
    if (at_end)
    {
        g_newsgroup_ptr = nullptr;
    }
    m_newsrc.name = m_output_dir / "newsrc";
    m_newsrc.old_name = m_output_dir / "old-newsrc";
    m_newsrc.new_name = m_output_dir / "newsrc.new";
    m_newsrc.info_name = m_output_dir / "newsrc.info";
    m_newsrc.flags |= RF_RC_CHANGED;
    remember_pending_changes();
    m_original_newsrc = "comp.previous: 1\ncomp.lang.cpp: 1\ncomp.next: 1\n";
    std::ofstream{m_newsrc.name} << m_original_newsrc;
    std::ofstream{m_newsrc.old_name} << m_original_newsrc;
}

std::string NewsgroupSelectorDelegatedExitStateTest::newsrc_text(const Newsrc &newsrc)
{
    std::string result;
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        if (group.m_rc == &newsrc)
        {
            result += std::string{group.rc_name()} + group.m_subscribe_char + std::string{group.rc_numbers()} + '\n';
        }
    }
    return result;
}

std::string NewsgroupSelectorDelegatedExitStateTest::read_newsrc(const fs::path &path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void NewsgroupSelectorDelegatedExitStateTest::remember_pending_changes()
{
    std::copy(g_newsgroup_data.begin(), g_newsgroup_data.end(), m_pending.begin());
}

void NewsgroupSelectorDelegatedExitStateTest::expect_pending_changes() const
{
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(m_pending[index].rc_numbers(), g_newsgroup_data[index].rc_numbers()) << index;
        EXPECT_EQ(m_pending[index].m_to_read, g_newsgroup_data[index].m_to_read) << index;
        EXPECT_EQ(m_pending[index].m_subscribe_char, g_newsgroup_data[index].m_subscribe_char) << index;
        EXPECT_EQ(m_pending[index].m_flags, g_newsgroup_data[index].m_flags) << index;
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

void NewsgroupSelectorDelegatedExitStateTest::expect_exit_files_unchanged() const
{
    EXPECT_EQ(m_original_newsrc, read_newsrc(m_newsrc.name));
    EXPECT_EQ(m_original_newsrc, read_newsrc(m_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
}

void NewsgroupSelectorDelegatedExitStateTest::prepare_selector_session(bool rereading)
{
    prepare_exit(rereading, true);
    g_tc_LINES = 24;
    g_newsgroup_data[1].m_flags = (g_newsgroup_data[1].m_flags & ~NF_DEL_SEL) | NF_SEL;
    m_original_newsrc = newsrc_text(m_newsrc);
    std::ofstream{m_newsrc.name} << m_original_newsrc;
    std::ofstream{m_newsrc.old_name} << m_original_newsrc;
    m_newsrc.flags &= ~RF_RC_CHANGED;
}

void NewsgroupSelectorDelegatedExitStateTest::expect_selector_session_preserved(bool rereading, int direction) const
{
    for (int index = 0; index < 3; ++index)
    {
        const NewsgroupData &group = g_newsgroup_data[index];
        EXPECT_EQ(rereading ? " 1-3" : index == 1 ? " 1" : " 1-2", group.rc_numbers()) << index;
        EXPECT_EQ(ArticleUnread{rereading ? 0 : index == 1 ? 2 : 1}, group.m_to_read) << index;
        EXPECT_EQ(':', group.m_subscribe_char) << index;
        EXPECT_EQ(index == 1 ? NF_SEL : NF_NONE, group.m_flags & (NF_SEL | NF_DEL_SEL | NF_DEL)) << index;
        EXPECT_EQ(ArticleNum{1}, group.m_abs_first) << index;
        EXPECT_EQ(ArticleNum{3}, group.m_ng_max) << index;
    }
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 3}, g_newsgroup_to_read);
    EXPECT_FALSE(g_sel.exclusive);
    expect_exit_files_unchanged();
    expect_selector_context(rereading, direction);
}

class NewsgroupSelectorExitRequestTest : public NewsgroupSelectorDelegatedExitStateTest,
                                         public testing::WithParamInterface<std::tuple<char, bool, bool>>
{
};

class NewsgroupSelectorSourceExitTest : public NewsgroupSelectorDelegatedExitStateTest,
                                        public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorScanEntryTest : public NewsgroupSelectorDelegatedExitStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    SaDisplayOrder m_old_order{g_sa_mode_order};
    bool           m_old_never_initialized{g_sa_never_initialized};
    bool           m_old_in{g_sa_in};
    bool           m_old_go{g_sa_go};
    bool           m_old_explicit{g_sa_go_explicit};
    bool           m_old_read_eligible{g_sa_mode_read_elig};
    bool           m_old_select_threads{g_sa_do_sel_threads};
    bool           m_old_fold{g_sa_mode_fold};
    ArticleNum     m_old_article{g_sa_art};
};

void NewsgroupSelectorScanEntryTest::SetUp()
{
    NewsgroupSelectorDelegatedExitStateTest::SetUp();
    g_sa_mode_order = SA_ORDER_ARRIVAL;
    g_sa_never_initialized = true;
}

void NewsgroupSelectorScanEntryTest::TearDown()
{
    sa_cleanup();
    g_sa_mode_order = m_old_order;
    g_sa_never_initialized = m_old_never_initialized;
    g_sa_in = m_old_in;
    g_sa_go = m_old_go;
    g_sa_go_explicit = m_old_explicit;
    g_sa_mode_read_elig = m_old_read_eligible;
    g_sa_do_sel_threads = m_old_select_threads;
    g_sa_mode_fold = m_old_fold;
    g_sa_art = m_old_article;
    NewsgroupSelectorDelegatedExitStateTest::TearDown();
}

class NewsgroupSelectorHelpStateTest : public NewsgroupSelectorDelegatedExitStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_help(bool rereading, bool complete_page = false);
    void expect_help_preserved(bool rereading, bool restart = false) const;

private:
    std::string m_old_library{g_rn_lib};
    int         m_initial_univ_level{g_univ.level};
};

void NewsgroupSelectorHelpStateTest::SetUp()
{
    NewsgroupSelectorDelegatedExitStateTest::SetUp();
    g_rn_lib = m_output_dir.generic_string();
    fs::create_directory(m_output_dir / "HelpFiles");
    std::ofstream{m_output_dir / "HelpFiles/top"} << "\"Local help topic\" &selector-help\n";
}

void NewsgroupSelectorHelpStateTest::TearDown()
{
    EXPECT_EQ(m_initial_univ_level, g_univ.level);
    g_rn_lib = m_old_library;
    NewsgroupSelectorDelegatedExitStateTest::TearDown();
}

void NewsgroupSelectorHelpStateTest::prepare_help(bool rereading, bool complete_page)
{
    prepare_exit(rereading, true);
    if (complete_page)
    {
        g_tc_LINES = 200;
    }
}

void NewsgroupSelectorHelpStateTest::expect_help_preserved(bool rereading, bool restart) const
{
    expect_pending_changes();
    expect_exit_files_unchanged();
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_EQ(SS_STRING, g_sel.sort);
    EXPECT_EQ(1, g_sel.direction);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[0], g_recent_newsgroup);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 3}, g_newsgroup_to_read);
    EXPECT_EQ("s", g_sel_grp_display_mode);
    EXPECT_EQ(0, g_sel_grp_display_mode_index);
    if (!restart)
    {
        expect_page(true, 1);
        EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, g_sel.mask);
    }
}

class NewsgroupSelectorOnlineHelpTest : public NewsgroupSelectorHelpStateTest,
                                        public testing::WithParamInterface<std::tuple<bool, char, char>>
{
};

class NewsgroupSelectorFinishedHelpTest : public NewsgroupSelectorHelpStateTest,
                                          public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorHelpScopeTest : public NewsgroupSelectorHelpStateTest, public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorHelpCommandTest : public NewsgroupSelectorHelpStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, bool, char>>
{
};

class NewsgroupSelectorHelpSessionTest : public NewsgroupSelectorHelpStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorDisplayStateTest : public NewsgroupSelectorDelegatedExitStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void prepare_display(bool rereading, std::string_view modes = "slm", std::size_t index = 0);
    void prepare_display_session(bool rereading, std::string_view modes = "slm");
    void load_descriptions();
    void expect_display_preserved(bool rereading) const;
};

void NewsgroupSelectorDisplayStateTest::SetUp()
{
    NewsgroupSelectorDelegatedExitStateTest::SetUp();
    m_data_source.m_flags |= DF_OPEN | DF_ACTIVE;
    m_data_source.m_group_desc = (m_output_dir / "newsgroups").generic_string();
    std::ofstream{m_data_source.m_group_desc} << "comp.previous Previous discussion\n"
                                                 "comp.lang.cpp Cplusplus discussion\n"
                                                 "comp.next Next discussion\n";
}

void NewsgroupSelectorDisplayStateTest::TearDown()
{
    m_data_source.m_desc_sf.close();
    NewsgroupSelectorDelegatedExitStateTest::TearDown();
}

void NewsgroupSelectorDisplayStateTest::prepare_display(bool rereading, std::string_view modes, std::size_t index)
{
    prepare_exit(rereading, true);
    g_sel_grp_display_mode = modes;
    g_sel_grp_display_mode_index = index;
}

void NewsgroupSelectorDisplayStateTest::load_descriptions()
{
    ASSERT_EQ("Cplusplus discussion\n", m_data_source.find_group_desc("comp.lang.cpp"));
    ASSERT_NE(nullptr, m_data_source.m_desc_sf.m_hp);
}

void NewsgroupSelectorDisplayStateTest::prepare_display_session(bool rereading, std::string_view modes)
{
    prepare_selector_session(rereading);
    g_sel_grp_display_mode = modes;
    g_sel_grp_display_mode_index = 0;
}

void NewsgroupSelectorDisplayStateTest::expect_display_preserved(bool rereading) const
{
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_selector_context(rereading);
    expect_page(true, 1);
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[0], g_recent_newsgroup);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 3}, g_newsgroup_to_read);
}

struct GroupDisplayChoice
{
    std::string_view modes;
    std::size_t      before;
    std::size_t      after;
    std::string_view style;
};

void PrintTo(const GroupDisplayChoice &choice, std::ostream *output)
{
    *output << choice.modes << '[' << choice.before << "] -> " << choice.style;
}

class NewsgroupSelectorDisplayModeTest : public NewsgroupSelectorDisplayStateTest,
                                         public testing::WithParamInterface<std::tuple<bool, GroupDisplayChoice>>
{
};

class NewsgroupSelectorDisplayLoadingTest : public NewsgroupSelectorDisplayStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorDisplaySessionTest : public NewsgroupSelectorDisplayStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, char, bool>>
{
};

class NewsgroupSelectorDisplayScopeTest : public NewsgroupSelectorDisplayStateTest,
                                          public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorRefreshStateTest : public NewsgroupSelectorDelegatedExitStateTest
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    void        prepare_refresh(bool rereading);
    DataSource &add_refresh_source();
    void        replace_active_counts(DataSource &source, int high);
    void        recount_groups();
    void        expect_active_counts(const DataSource &source, int high) const;

private:
    std::vector<DataSource> m_saved_sources;
    ArticleUnread           m_old_missing{g_missing_count};
    std::string             m_old_server_line{g_nntp.ser_line};
    std::string             m_old_last_command{g_nntp.last_command};
};

void NewsgroupSelectorRefreshStateTest::SetUp()
{
    NewsgroupSelectorDelegatedExitStateTest::SetUp();
    m_saved_sources.swap(g_data_sources);
    g_data_sources.reserve(4);
    (void) add_refresh_source();
    m_newsrc.data_source = &g_data_sources.front();
    g_data_source = &g_data_sources.front();
}

void NewsgroupSelectorRefreshStateTest::TearDown()
{
    g_nntp.link.connection.reset();
    for (DataSource &source : g_data_sources)
    {
        source.m_nntp_link.connection.reset();
        source.close();
        source.m_act_sf.close();
    }
    g_data_sources.clear();
    m_saved_sources.swap(g_data_sources);
    m_newsrc.data_source = &m_data_source;
    g_data_source = &m_data_source;
    g_missing_count = m_old_missing;
    g_nntp.ser_line = m_old_server_line;
    g_nntp.last_command = m_old_last_command;
    NewsgroupSelectorDelegatedExitStateTest::TearDown();
}

DataSource &NewsgroupSelectorRefreshStateTest::add_refresh_source()
{
    EXPECT_LT(g_data_sources.size(), g_data_sources.capacity());
    DataSource &source = g_data_sources.emplace_back();
    source.m_flags = DF_OPEN | DF_ACTIVE;
    source.m_news_id = (m_output_dir / "active").generic_string();
    source.m_act_sf.m_refetch_secs = 3600;
    EXPECT_EQ(1, source.m_act_sf.open(source.m_news_id, "", ""));
    source.m_act_sf.m_last_fetch = std::time(nullptr);
    return source;
}

void NewsgroupSelectorRefreshStateTest::prepare_refresh(bool rereading)
{
    prepare_exit(rereading, false);
    m_original_newsrc = newsrc_text(m_newsrc);
    std::ofstream{m_newsrc.name} << m_original_newsrc;
    std::ofstream{m_newsrc.old_name} << m_original_newsrc;
    m_newsrc.flags &= ~RF_RC_CHANGED;
    g_missing_count = 17;
}

void NewsgroupSelectorRefreshStateTest::replace_active_counts(DataSource &source, int high)
{
    const std::size_t index = static_cast<std::size_t>(&source - g_data_sources.data());
    const fs::path    active_file = m_output_dir / ("refreshed-active-" + std::to_string(index));
    std::ofstream     output{active_file};
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        output << group.rc_name() << ' ' << high << " 1 y\n";
    }
    source.m_news_id = active_file.generic_string();
}

void NewsgroupSelectorRefreshStateTest::recount_groups()
{
    g_newsgroup_to_read = NewsgroupNum{
        static_cast<long>(std::count_if(g_newsgroup_data.begin(), g_newsgroup_data.end(), [](const NewsgroupData &group)
                                        { return group.m_to_read >= g_newsgroup_min_to_read; }))};
}

void NewsgroupSelectorRefreshStateTest::expect_active_counts(const DataSource &source, int high) const
{
    ASSERT_EQ(3, source.m_act_sf.m_lines.size());
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(std::string{g_newsgroup_data[index].rc_name()} + " " + std::to_string(high) + " 1 y\n",
                  source.m_act_sf.m_lines[index]);
    }
}

class NewsgroupSelectorRefreshInvalidationTest : public NewsgroupSelectorRefreshStateTest,
                                                 public testing::WithParamInterface<std::tuple<bool, ArticleUnread>>
{
};

class NewsgroupSelectorRefreshCleanupTest : public NewsgroupSelectorRefreshStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

class NewsgroupSelectorRefreshRebuildTest : public NewsgroupSelectorRefreshStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, bool>>
{
};

struct RefreshSourceState
{
    const char     *name;
    DataSourceFlags flags;
    bool            refetch;
    bool            cached_file;
    bool            refreshed;
};

void PrintTo(const RefreshSourceState &state, std::ostream *output)
{
    *output << state.name;
}

class NewsgroupSelectorRefreshSourceTest : public NewsgroupSelectorRefreshStateTest,
                                           public testing::WithParamInterface<RefreshSourceState>
{
};

class NewsgroupSelectorDelegatedQuitTest : public NewsgroupSelectorDelegatedExitStateTest,
                                           public testing::WithParamInterface<std::tuple<char, bool, bool>>
{
};

class NewsgroupSelectorDelegatedAbandonExitTest : public NewsgroupSelectorDelegatedExitStateTest,
                                                  public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorDelegatedDeclineExitTest : public NewsgroupSelectorDelegatedExitStateTest,
                                                  public testing::WithParamInterface<std::tuple<bool, char>>
{
};

class NewsgroupSelectorDelegatedErrorTest : public NewsgroupSelectorDelegatedExitStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
};

class NewsgroupSelectorDelegatedQuitSessionTest : public NewsgroupSelectorDelegatedExitStateTest,
                                                  public testing::WithParamInterface<char>
{
};

class NewsgroupSelectorDelegatedErrorSessionTest : public NewsgroupSelectorDelegatedExitStateTest
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    int  m_saved_stdin{-1};
    bool m_old_allow_typeahead{g_allow_typeahead};
};

void NewsgroupSelectorDelegatedErrorSessionTest::SetUp()
{
    NewsgroupSelectorDelegatedExitStateTest::SetUp();
    prepare_exit(false, false);
    const fs::path input_file = m_output_dir / "selector-input";
    std::ofstream{input_file} << "RqQ";
    std::FILE *input = std::fopen(input_file.string().c_str(), "r");
    ASSERT_NE(nullptr, input);
    m_saved_stdin = dup(fileno(stdin));
    ASSERT_GE(m_saved_stdin, 0);
    EXPECT_GE(dup2(fileno(input), fileno(stdin)), 0);
    std::fclose(input);
    std::clearerr(stdin);
    g_allow_typeahead = true;
}

void NewsgroupSelectorDelegatedErrorSessionTest::TearDown()
{
    if (m_saved_stdin >= 0)
    {
        EXPECT_GE(dup2(m_saved_stdin, fileno(stdin)), 0);
        close(m_saved_stdin);
        std::clearerr(stdin);
    }
    g_allow_typeahead = m_old_allow_typeahead;
    NewsgroupSelectorDelegatedExitStateTest::TearDown();
}

class NewsgroupSelectorDelegatedServerLossTest : public NewsgroupSelectorDelegatedExitStateTest,
                                                 public testing::WithParamInterface<std::tuple<bool, bool, bool>>
{
protected:
    void TearDown() override;
    void prepare_server_loss(bool rereading, bool later_page, bool remaining_source);
    void expect_saved_changes() const;
    void expect_selector_modes(bool rereading) const;

    std::string m_diagnostic;

private:
    DataSource     m_remaining_source{};
    Newsrc         m_remaining_newsrc{};
    std::string    m_saved_failed_text;
    std::string    m_saved_remaining_text;
    std::string    m_original_remaining_text;
    std::string    m_old_server_line{g_nntp.ser_line};
    std::string    m_old_last_command{g_nntp.last_command};
    std::string    m_old_last_group{g_last_run.newsgroup_name};
    long           m_old_last_new_time{g_last_run.new_time};
    bool           m_old_allow_timeout{g_nntp.allow_timeout};
    bool           m_old_check{g_check_flag};
    bool           m_old_suppress{g_suppress_cn};
    bool           m_old_paranoid{g_paranoid};
    NewsgroupData *m_old_start{g_start_here};
    NewsgroupData *m_old_go_group{g_ng_go_newsgroup_ptr};
};

void NewsgroupSelectorDelegatedServerLossTest::prepare_server_loss(bool rereading, bool later_page,
                                                                   bool remaining_source)
{
    prepare_exit(rereading, later_page);
    const int target = later_page ? 0 : 1;
    Selection origin{};
    origin.np = &g_newsgroup_data[target];
    (void) calc_page(origin);
    g_newsgroup_ptr = origin.np;
    set_newsgroup_name(origin.np->rc_name());
    g_term_line = g_sel_page.last_line;
    g_check_flag = false;
    g_suppress_cn = true;
    g_paranoid = false;
    g_nntp.allow_timeout = false;
    if (remaining_source)
    {
        m_remaining_source.m_flags = DF_OPEN | DF_ACTIVE;
        m_remaining_source.m_news_id = (m_output_dir / "active").generic_string();
        m_remaining_source.m_spool_dir = m_output_dir.generic_string();
        ASSERT_EQ(1, m_remaining_source.m_act_sf.open(m_remaining_source.m_news_id, "", ""));
        m_remaining_newsrc.data_source = &m_remaining_source;
        m_remaining_newsrc.name = m_output_dir / "remaining-newsrc";
        m_remaining_newsrc.old_name = m_output_dir / "remaining-old-newsrc";
        m_remaining_newsrc.new_name = m_output_dir / "remaining-newsrc.new";
        m_remaining_newsrc.info_name = m_output_dir / "remaining-newsrc.info";
        m_remaining_newsrc.flags = RF_ACTIVE | RF_RC_CHANGED;
        m_newsrc.next = &m_remaining_newsrc;
        m_original_newsrc.clear();
        for (int index = 0; index < 3; ++index)
        {
            const std::string original_line = std::string{g_newsgroup_data[index].rc_name()} + ": 1\n";
            if (index != target)
            {
                g_newsgroup_data[index].m_rc = &m_remaining_newsrc;
                m_original_remaining_text += original_line;
            }
            else
            {
                m_original_newsrc += original_line;
            }
        }
        std::ofstream{m_remaining_newsrc.name} << m_original_remaining_text;
        std::ofstream{m_remaining_newsrc.old_name} << m_original_remaining_text;
        std::ofstream{m_newsrc.name} << m_original_newsrc;
        std::ofstream{m_newsrc.old_name} << m_original_newsrc;
    }
    m_saved_failed_text = newsrc_text(m_newsrc);
    m_saved_remaining_text = newsrc_text(m_remaining_newsrc);
    const auto connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    g_nntp.link.connection = connection;
    g_nntp.link.flags = NNTP_NEW_CMD_OK;
    m_data_source.m_nntp_link = g_nntp.link;
    m_data_source.m_flags = DF_REMOTE | DF_OPEN | DF_ACTIVE;
    testing::InSequence sequence;
    EXPECT_CALL(*connection, write_line(testing::StrEq("GROUP " + std::string{origin.np->rc_name()}), testing::_));
    EXPECT_CALL(*connection, read_line(testing::_))
        .WillOnce(
            [](error_code_t &error)
            {
                error = boost::asio::error::connection_reset;
                return std::string{};
            });
    EXPECT_CALL(*connection, write_line(testing::StrEq("QUIT"), testing::_))
        .WillOnce([](const std::string &, error_code_t &error) { error = boost::asio::error::connection_reset; });
}

void NewsgroupSelectorDelegatedServerLossTest::TearDown()
{
    if (g_multirc == &m_multirc)
    {
        unuse_multirc(&m_multirc);
    }
    m_remaining_source.close();
    g_nntp.ser_line = m_old_server_line;
    g_nntp.last_command = m_old_last_command;
    g_last_run.newsgroup_name = m_old_last_group;
    g_last_run.new_time = m_old_last_new_time;
    g_nntp.allow_timeout = m_old_allow_timeout;
    g_check_flag = m_old_check;
    g_suppress_cn = m_old_suppress;
    g_paranoid = m_old_paranoid;
    g_start_here = m_old_start;
    g_ng_go_newsgroup_ptr = m_old_go_group;
    NewsgroupSelectorDelegatedExitStateTest::TearDown();
}

void NewsgroupSelectorDelegatedServerLossTest::expect_saved_changes() const
{
    EXPECT_EQ(m_saved_failed_text, read_newsrc(m_newsrc.name));
    EXPECT_EQ(m_original_newsrc, read_newsrc(m_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
    if (!m_saved_remaining_text.empty())
    {
        EXPECT_EQ(m_saved_remaining_text, read_newsrc(m_remaining_newsrc.name));
        EXPECT_EQ(m_saved_remaining_text, read_newsrc(m_remaining_newsrc.old_name));
        EXPECT_FALSE(fs::exists(m_remaining_newsrc.new_name));
    }
    EXPECT_EQ(DF_UNAVAILABLE, m_data_source.m_flags & DF_UNAVAILABLE);
    EXPECT_EQ(DF_NONE, m_data_source.m_flags & (DF_OPEN | DF_ACTIVE));
    EXPECT_FALSE(m_data_source.m_nntp_link.connection);
    EXPECT_FALSE(g_restore_old_newsrc);
}

void NewsgroupSelectorDelegatedServerLossTest::expect_selector_modes(bool rereading) const
{
    EXPECT_EQ(GM_SELECTOR, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_SELECTOR, g_mode);
    EXPECT_EQ(SM_NEWSGROUP, g_sel.mode);
    EXPECT_EQ(SS_STRING, g_sel.sort);
    EXPECT_EQ(rereading, g_sel.rereading);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_restore_old_newsrc);
}

class NewsgroupSelectorDelegatedEntryTest : public NewsgroupSelectorDelegationStateTest,
                                            public testing::WithParamInterface<std::tuple<char, bool>>
{
};

class NewsgroupSelectorDelegatedRetryTest : public NewsgroupSelectorDelegationStateTest,
                                            public testing::WithParamInterface<std::tuple<bool, std::string_view>>
{
};

struct DelegatedReadingEntry
{
    char             command;
    std::string_view input;
    std::string_view content;
    ArticleUnread    unread;
};

void PrintTo(const DelegatedReadingEntry &entry, std::ostream *output)
{
    *output << entry.command << ", " << entry.content;
}

class NewsgroupSelectorDelegatedReadingTest : public NewsgroupSelectorDelegationStateTest,
                                              public testing::WithParamInterface<DelegatedReadingEntry>
{
};

class NewsgroupSelectorDelegatedOptionsTest : public NewsgroupSelectorDelegationStateTest,
                                              public testing::WithParamInterface<bool>
{
};

struct DelegatedGroupNavigation
{
    std::string_view input;
    int              start;
    int              target;
    int              row;
};

void PrintTo(const DelegatedGroupNavigation &navigation, std::ostream *output)
{
    *output << testing::PrintToString(navigation.input) << ", " << navigation.start << " -> " << navigation.target;
}

class NewsgroupSelectorDelegatedNavigationTest : public NewsgroupSelectorDelegationStateTest,
                                                 public testing::WithParamInterface<DelegatedGroupNavigation>
{
};

class NewsgroupSelectorDelegatedPageTest : public NewsgroupSelectorDelegationStateTest
{
protected:
    void SetUp() override;
    void locate_group(int index);
};

void NewsgroupSelectorDelegatedPageTest::SetUp()
{
    NewsgroupSelectorDelegationStateTest::SetUp();
    g_tc_LINES = 6;
    prepare_selector();
}

void NewsgroupSelectorDelegatedPageTest::locate_group(int index)
{
    Selection target{};
    target.np = &g_newsgroup_data[index];
    (void) calc_page(target);
    g_newsgroup_ptr = target.np;
    g_term_line = g_sel_page.last_line;
}

class NewsgroupSelectorDelegatedPagedNavigationTest : public NewsgroupSelectorDelegatedPageTest,
                                                      public testing::WithParamInterface<DelegatedGroupNavigation>
{
};

class NewsgroupSelectorDelegatedReadGroupTest : public NewsgroupSelectorDelegationStateTest
{
protected:
    void SetUp() override;
};

void NewsgroupSelectorDelegatedReadGroupTest::SetUp()
{
    NewsgroupSelectorDelegationStateTest::SetUp();
    prepare_selector();
    NewsgroupData &group = g_newsgroup_data[0];
    group.m_rc_line.resize(group.m_num_offset);
    group.m_rc_line += " 1-3";
    group.m_to_read = TR_NONE;
    g_newsgroup_to_read = NewsgroupNum{2};
    init_pages(PRESERVE_PAGE);
}

class NewsgroupSelectorDelegatedReadLookupTest : public NewsgroupSelectorDelegatedReadGroupTest,
                                                 public testing::WithParamInterface<std::pair<std::string_view, int>>
{
};

class NewsgroupSelectorDelegatedRestrictedLookupTest : public NewsgroupSelectorDelegationStateTest,
                                                       public testing::WithParamInterface<std::string_view>
{
};

class NewsgroupSelectorDelegatedEndTest : public NewsgroupSelectorDelegatedPageTest,
                                          public testing::WithParamInterface<bool>
{
};

class NewsgroupSelectorDelegatedPopulationTest : public NewsgroupSelectorDelegationStateTest
{
protected:
    void prepare_population(bool rereading, int target, bool selected = false);
    void remember_population();
    void expect_other_groups_preserved(int target) const;
    void expect_files_unchanged() const;
    void expect_checkpoint(int target, std::string_view numbers) const;
    void expect_rebuilt_page();

private:
    static std::string read_file(const fs::path &path);

    std::array<NewsgroupData, 3> m_before;
    std::string                  m_original_newsrc;
};

void NewsgroupSelectorDelegatedPopulationTest::prepare_population(bool rereading, int target, bool selected)
{
    g_tc_LINES = 6;
    prepare_selector(rereading);
    g_newsgroup_data[0].m_flags = static_cast<NewsgroupFlags>(g_sel.mask) | NF_DEL;
    g_newsgroup_data[1].m_flags = NF_NONE;
    g_newsgroup_data[target].m_flags = selected ? static_cast<NewsgroupFlags>(g_sel.mask) : NF_NONE;
    g_sel_page.page_ngd = &g_newsgroup_data[target];
    init_pages(PRESERVE_PAGE);
    Selection origin{};
    origin.np = &g_newsgroup_data[target];
    (void) calc_page(origin);
    g_newsgroup_ptr = origin.np;
    set_newsgroup_name(origin.np->rc_name());
    g_term_line = g_sel_page.last_line;
    m_newsrc.name = m_output_dir / "newsrc";
    m_newsrc.new_name = m_output_dir / "newsrc.new";
    m_newsrc.old_name = m_output_dir / "old-newsrc";
    remember_population();
}

void NewsgroupSelectorDelegatedPopulationTest::remember_population()
{
    std::copy(g_newsgroup_data.begin(), g_newsgroup_data.end(), m_before.begin());
    m_original_newsrc.clear();
    for (const NewsgroupData &group : m_before)
    {
        m_original_newsrc +=
            std::string{group.rc_name()} + group.m_subscribe_char + std::string{group.rc_numbers()} + '\n';
    }
    std::ofstream{m_newsrc.name} << m_original_newsrc;
    std::ofstream{m_newsrc.old_name} << m_original_newsrc;
    m_newsrc.flags &= ~RF_RC_CHANGED;
}

std::string NewsgroupSelectorDelegatedPopulationTest::read_file(const fs::path &path)
{
    std::ifstream input{path};
    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void NewsgroupSelectorDelegatedPopulationTest::expect_other_groups_preserved(int target) const
{
    for (int index = 0; index < 3; ++index)
    {
        if (index == target)
        {
            continue;
        }
        const NewsgroupData &group = g_newsgroup_data[index];
        EXPECT_EQ(m_before[index].rc_numbers(), group.rc_numbers()) << index;
        EXPECT_EQ(m_before[index].m_to_read, group.m_to_read) << index;
        EXPECT_EQ(m_before[index].m_subscribe_char, group.m_subscribe_char) << index;
        EXPECT_EQ(m_before[index].m_flags & ~NF_INCLUDED, group.m_flags & ~NF_INCLUDED) << index;
    }
    EXPECT_EQ(NF_DEL, g_newsgroup_data[0].m_flags & NF_DEL);
    EXPECT_EQ(static_cast<NewsgroupFlags>(g_sel.mask), g_newsgroup_data[0].m_flags & g_sel.mask);
}

void NewsgroupSelectorDelegatedPopulationTest::expect_files_unchanged() const
{
    EXPECT_EQ(m_original_newsrc, read_file(m_newsrc.name));
    EXPECT_EQ(m_original_newsrc, read_file(m_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
}

void NewsgroupSelectorDelegatedPopulationTest::expect_checkpoint(int target, std::string_view numbers) const
{
    std::string expected;
    for (int index = 0; index < 3; ++index)
    {
        const NewsgroupData &group = m_before[index];
        expected += std::string{group.rc_name()} + group.m_subscribe_char;
        expected += index == target ? numbers : group.rc_numbers();
        expected += '\n';
    }
    EXPECT_EQ(expected, read_file(m_newsrc.name));
    EXPECT_EQ(m_original_newsrc, read_file(m_newsrc.old_name));
    EXPECT_FALSE(fs::exists(m_newsrc.new_name));
}

void NewsgroupSelectorDelegatedPopulationTest::expect_rebuilt_page()
{
    EXPECT_EQ(0, g_sel_page.item_index);
    testing::internal::CaptureStdout();
    display_page();
    testing::internal::GetCapturedStdout();
    ASSERT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(1, g_sel_page.page_obj_cnt);
    ASSERT_NE(nullptr, g_sel_page.items[0].u.np);
    EXPECT_EQ(NF_INCLUDED, g_sel_page.items[0].u.np->m_flags & NF_INCLUDED);
    EXPECT_NE('!', g_sel_page.items[0].u.np->m_subscribe_char);
}

using DelegatedUnsubscribeState = std::tuple<bool, int, bool>;

class NewsgroupSelectorDelegatedUnsubscribeTest : public NewsgroupSelectorDelegatedPopulationTest,
                                                  public testing::WithParamInterface<DelegatedUnsubscribeState>
{
};

class NewsgroupSelectorDelegatedSubscriptionTest : public NewsgroupSelectorDelegatedPopulationTest,
                                                   public testing::WithParamInterface<bool>
{
protected:
    void SetUp() override;
};

void NewsgroupSelectorDelegatedSubscriptionTest::SetUp()
{
    NewsgroupSelectorDelegationStateTest::SetUp();
    prepare_population(GetParam(), 1);
    g_newsgroup_data[2].m_subscribe_char = '!';
    g_newsgroup_data[2].m_to_read = TR_UNSUB;
    g_newsgroup_data[2].m_flags = NF_DEL | NF_DEL_SEL;
    g_newsgroup_to_read = NewsgroupNum{GetParam() ? 0 : 2};
    init_pages(PRESERVE_PAGE);
    remember_population();
}

class NewsgroupSelectorDelegatedCatchupTest : public NewsgroupSelectorDelegatedPopulationTest,
                                              public testing::WithParamInterface<std::pair<bool, int>>
{
};

class NewsgroupSelectorDelegatedSelectedCatchupTest : public NewsgroupSelectorDelegatedPopulationTest,
                                                      public testing::WithParamInterface<int>
{
};

class NewsgroupSelectorDelegatedCatchupCancelTest : public NewsgroupSelectorDelegatedPopulationTest,
                                                    public testing::WithParamInterface<std::pair<bool, char>>
{
};

class OnDemandThreadArticleTest : public LocalSpoolArticleTest
{
protected:
    std::string display_tree();
};

std::string OnDemandThreadArticleTest::display_tree()
{
    const ArticleLocation                  current = g_nav.current;
    const ArticleLocation                  recent = g_nav.recent;
    const ArticlePosition                  position = g_art_input.pos;
    const ArticlePosition                  file_position = tell_art();
    const ArticlePosition                  header_end = g_parsed_header.positions[PAST_HEADER].min_pos;
    const std::string                      header = g_parsed_header.head_buf;
    const ArticleUnread                    unread = g_newsgroup_ptr->m_to_read;
    const ArticleUnread                    selected = g_selected_count;
    const bool                             selected_only = g_sel.selected_only;
    g_pushback.push('q');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch("t");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    EXPECT_TRUE(g_threaded_group);
    EXPECT_EQ(current, g_nav.current);
    EXPECT_EQ(current, g_nav.target);
    EXPECT_EQ(recent, g_nav.recent);
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_only, g_sel.selected_only);
    EXPECT_EQ(position, g_art_input.pos);
    EXPECT_EQ(g_nav.current.num, g_art_input.open_art);
    EXPECT_EQ(file_position, tell_art());
    EXPECT_EQ(g_nav.current.num, g_parsed_header.parsed_art);
    EXPECT_EQ(header_end, g_parsed_header.positions[PAST_HEADER].min_pos);
    EXPECT_EQ(header, g_parsed_header.head_buf);
    EXPECT_EQ(ArticleLine{6}, g_art_input.line_num);
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
    MimeSection                   m_old_mime_article{std::move(g_mime.article)};
    MimeSection                  *m_old_mime_section{g_mime.section};
    MimeState                     m_old_mime_state{g_mime.state};
    bool                          m_old_is_mime{g_mime.is_mime};
    bool                          m_old_multimedia_mime{g_mime.multimedia_mime};
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
    g_mime.article = {};
    g_mime.section = &g_mime.article;
    open_group(false);
    g_nav.current.art->select_article(AUTO_KILL_NONE);
    g_sel.selected_only = true;
    ASSERT_EQ(0, std::fseek(g_art_input.fp, 0, SEEK_END));
    g_art_input.pos = tell_art();
}

void SaveArticleTest::TearDown()
{
    g_mime.article.mime_clear_struct();
    g_mime.article = std::move(m_old_mime_article);
    g_mime.section = m_old_mime_section;
    g_mime.state = m_old_mime_state;
    g_mime.is_mime = m_old_is_mime;
    g_mime.multimedia_mime = m_old_multimedia_mime;
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
    const ArticleLocation                  current = g_nav.current;
    const ArticleLocation                  recent = g_nav.recent;
    const ArticlePosition                  position = g_art_input.pos;
    const ArticleLine                      line = g_art_input.line_num;
    const ArticleLine                      top = g_top_line;
    const ArticleNum                       first = g_first_art;
    const SubjectFlags                     subject_flags = g_nav.current.art->m_subj->m_flags;
    const ArticleUnread                    unread = g_newsgroup_ptr->m_to_read;
    const ArticleUnread                    selected = g_selected_count;
    const int                              selected_subjects = g_selected_subj_cnt;
    std::vector<ArticleFlags>              flags;
    for (long number = 1; number <= 3; ++number)
    {
        flags.push_back(g_cache.ptr(ArticleNum{number})->m_flags);
    }
    if (next_command)
    {
        g_pushback.push('n');
    }
    push_command(input);
    testing::internal::CaptureStdout();

    ArticleSwitchResult result = AS_NORM;
    EXPECT_NO_THROW(result = art_switch(std::string{command, FINISH_CMD}));

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(expected, result);
    EXPECT_EQ(current, g_nav.current);
    EXPECT_EQ(current, g_nav.target);
    EXPECT_EQ(recent, g_nav.recent);
    EXPECT_EQ(first, g_first_art);
    EXPECT_EQ(position, g_art_input.pos);
    EXPECT_EQ(line, g_art_input.line_num);
    EXPECT_EQ(top, g_top_line);
    EXPECT_EQ(g_nav.current.num, g_parsed_header.parsed_art);
    EXPECT_EQ(g_nav.current.num, g_art_input.open_art);
    EXPECT_EQ(subject_flags, g_nav.current.art->m_subj->m_flags);
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(selected, g_selected_count);
    EXPECT_EQ(selected_subjects, g_selected_subj_cnt);
    EXPECT_TRUE(g_sel.selected_only);
    EXPECT_FALSE(g_reread);
    EXPECT_FALSE(g_force_last);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(m_spool, fs::current_path());
    EXPECT_EQ(0, g_int_count);
    for (long number = 1; number <= 3; ++number)
    {
        EXPECT_EQ(flags[number - 1], g_cache.ptr(ArticleNum{number})->m_flags);
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
    int        m_old_sc_loaded_count{g_saved_scores.loaded_count};
    bool       m_old_sf_verbose{g_sf_verbose};
    int        m_old_sf_score_verbose{g_sf_score_verbose};
    bool       m_old_kill_thresh_active{g_kill_thresh_active};
    int        m_old_kill_thresh{g_kill_thresh};
    bool       m_old_s_order_changed{g_art_scan.order_changed};
};

void ScoreArticleTest::SetUp()
{
    SaveArticleTest::SetUp();
    const ArticlePosition position = g_art_input.pos;
    m_scores = m_output_dir / "scores";
    fs::create_directories(m_scores);
    std::ofstream{m_scores / "global"} << GLOBAL_RULES;
    m_env.expect_env_repeatedly("SCOREDIR", m_scores.generic_string().c_str());
    m_env.expect_env_repeatedly("SAVESCOREFILE", (m_output_dir / "saved-scores").generic_string().c_str());
    g_sc_delay = false;
    g_sf_verbose = false;
    g_sf_score_verbose = 0;
    g_score_cache.file_clear();
    sc_init(false);
    ASSERT_TRUE(g_sc_initialized);
    for (long number = 1; number <= 3; ++number)
    {
        (void) sc_score_art(ArticleNum{number}, true);
    }
    ASSERT_TRUE(parse_header(g_nav.target.num));
    ASSERT_NE(nullptr, art_open(g_nav.target.num, position));
    g_art_input.pos = position;
    g_art_scan.order_changed = false;
    expect_scores({10, 10, 0});
}

void ScoreArticleTest::TearDown()
{
    g_sc_saves_cores = false;
    sc_cleanup();
    g_score_cache.file_clear();
    g_sc_delay = m_old_sc_delay;
    g_sc_scoring = m_old_sc_scoring;
    g_sc_saves_cores = m_old_sc_saves_cores;
    g_sc_fill_max = m_old_sc_fill_max;
    g_sc_fill_read = m_old_sc_fill_read;
    g_saved_scores.loaded_count = m_old_sc_loaded_count;
    g_sf_verbose = m_old_sf_verbose;
    g_sf_score_verbose = m_old_sf_score_verbose;
    g_kill_thresh_active = m_old_kill_thresh_active;
    g_kill_thresh = m_old_kill_thresh;
    g_art_scan.order_changed = m_old_s_order_changed;
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
        EXPECT_TRUE(g_cache.scored(ArticleNum{number}));
        EXPECT_EQ(expected[number - 1], g_cache.ptr(ArticleNum{number})->m_score);
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
    EXPECT_FALSE(g_art_scan.order_changed);
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
    std::string_view m_old_mime_getc_line{g_mime.getc_line};
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
    g_mime.getc_line = {};
}

void ExtractSaveArticleTest::TearDown()
{
    ExternalSaveArticleTest::TearDown();
    g_extract_dest = m_old_extract_dest;
    g_extract_prog = m_old_extract_prog;
    g_decode_filename = m_old_decode_filename;
    g_art_line = std::move(m_old_art_line);
    g_mime.getc_line = m_old_mime_getc_line;
}

void ExtractSaveArticleTest::article(std::string_view body, std::string_view headers)
{
    ASSERT_EQ(0, std::fclose(g_art_input.fp));
    g_art_input.fp = nullptr;
    g_art_input.open_art = {};
    g_parsed_header.parsed_art = {};
    m_article_text = HEADER;
    m_article_text.pop_back();
    m_article_text += headers;
    m_article_text += '\n';
    m_article_text += body;
    std::ofstream output{m_spool / "1"};
    output << m_article_text;
    output.close();
    ASSERT_FALSE(output.fail());
    ASSERT_TRUE(parse_header(g_nav.target.num));
    ASSERT_NE(nullptr, art_open(g_nav.target.num, ArticlePosition{}));
    ASSERT_EQ(0, std::fseek(g_art_input.fp, 0, SEEK_END));
    g_art_input.pos = tell_art();
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
    g_mimecap.final();
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
    g_mimecap.init();
}

std::string ViewArticleTest::view()
{
    EXPECT_EQ(0, seek_art(g_parsed_header.positions[PAST_HEADER].min_pos));
    g_art_input.pos = tell_art();
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
    EXPECT_EQ(0, seek_art(g_parsed_header.positions[PAST_HEADER].min_pos));
    g_art_input.pos = tell_art();
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
    EXPECT_EQ(0, seek_art(g_parsed_header.positions[PAST_HEADER].min_pos));
    g_art_input.pos = tell_art();
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
    std::string m_old_server_line{g_nntp.ser_line};
    std::string m_old_last_command{g_nntp.last_command};
};

void RemoteLegacyFollowupArticleTest::SetUp()
{
    LegacyFollowupArticleTest::SetUp();
    const auto connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    g_nntp.link.connection = connection;
    g_nntp.link.flags = NNTP_NEW_CMD_OK;
    m_data_source.m_flags = DF_REMOTE;
    m_data_source.m_spool_dir = m_spool.generic_string();
    EXPECT_CALL(*connection, write_line(testing::StrEq("DATE"), testing::_));
    EXPECT_CALL(*connection, read_line(testing::_)).WillOnce(testing::Return(GetParam().response));
}

void RemoteLegacyFollowupArticleTest::TearDown()
{
    g_nntp.ser_line = m_old_server_line;
    g_nntp.last_command = m_old_last_command;
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
    EXPECT_FALSE(g_art_scan.order_changed);
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
    EXPECT_FALSE(g_art_scan.order_changed);
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
    EXPECT_TRUE(g_art_scan.order_changed);
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
    EXPECT_FALSE(g_art_scan.order_changed);
}

TEST_P(ExecuteScoreArticleTest, rescoringAppliesChangesToPersistedRules)
{
    std::ofstream{m_scores / "comp.lang.cpp"} << "25 subject: topic\n";

    const std::string output = score('\'', GetParam() ? "\n2" : "r\n", false);

    expect_scores({35, 35, 0});
    EXPECT_TRUE(g_art_scan.order_changed);
    EXPECT_THAT(output, testing::HasSubstr("Rescoring articles..."));
    EXPECT_EQ("25 subject: topic\n", score_file());
}

TEST_P(ExecuteScoreArticleTest, continuesScoringUnscoredArticles)
{
    g_cache.ptr(ArticleNum{2})->m_score_flags = SFLAG_NONE;
    g_cache.ptr(ArticleNum{3})->m_score_flags = SFLAG_NONE;
    g_sc_fill_max = ArticleNum{1};

    const std::string output = score('\'', GetParam() ? "\n5" : "f\n", false);

    expect_scores({10, 10, 0});
    EXPECT_TRUE(g_art_scan.order_changed);
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
    g_cache.ptr(ArticleNum{2})->m_score_flags = SFLAG_NONE;
    g_cache.ptr(ArticleNum{3})->m_score_flags = SFLAG_NONE;
    g_sc_fill_max = ArticleNum{1};

    const std::string output = score('\'', "f\n");

    EXPECT_TRUE(g_cache.scored(ArticleNum{1}));
    EXPECT_FALSE(g_cache.scored(ArticleNum{2}));
    EXPECT_FALSE(g_cache.scored(ArticleNum{3}));
    EXPECT_EQ(ArticleNum{1}, g_sc_fill_max);
    EXPECT_FALSE(g_art_scan.order_changed);
    EXPECT_THAT(output, testing::HasSubstr("Scoring more articles..."));
}

TEST_F(ScoreArticleTest, anUnwritableScoreFileReturnsToReadingWithoutApplyingTheRule)
{
    fs::create_directory(m_scores / "comp.lang.cpp");

    const std::string output = score('"', "\" 25 subject: Topic\n");

    EXPECT_THAT(output, testing::HasSubstr("Could not open (for append) file " +
                                           (m_scores / "comp.lang.cpp").generic_string()));
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_art_scan.order_changed);
    EXPECT_EQ(GLOBAL_RULES, score_file("global"));
}

TEST_F(ScoreArticleTest, rescoringClearsAuthorMatchesFromRemovedRules)
{
    sf_append("! 5 from: root@example.test");
    g_nav.target.art->m_score_flags = SFLAG_NONE;
    ASSERT_EQ(15, sc_score_art(g_nav.target.num, true));
    ASSERT_NE(0, g_nav.target.art->m_score_flags & SFLAG_AUTHOR);
    std::ofstream{m_scores / "comp.lang.cpp"} << "newauthor=7\n";

    score('\'', "r\n", false);

    expect_scores({17, 17, 7});
    EXPECT_EQ(0, g_nav.target.art->m_score_flags & SFLAG_AUTHOR);
}

TEST_F(ScoreArticleTest, interruptedRescoringInvalidatesOldScoresForLaterOnDemandScoring)
{
#ifndef PENDING
    GTEST_SKIP() << "this build does not defer scoring for pending input";
#endif
    std::ofstream{m_scores / "comp.lang.cpp"} << "25 subject: topic\n";

    score('\'', "r\n");

    EXPECT_FALSE(g_cache.scored(g_nav.target.num));
    EXPECT_EQ(35, sc_score_art(g_nav.target.num, true));
    EXPECT_TRUE(g_cache.scored(g_nav.target.num));
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
    EXPECT_TRUE(g_cache.scored(g_nav.target.num));
    EXPECT_EQ(10, g_nav.target.art->m_score);
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
    EXPECT_TRUE(g_art_scan.order_changed);
    EXPECT_EQ(EDITED_RULE, score_file());
}

TEST_P(FailedEditScoreArticleTest, aFailedEditorLeavesTheRulesAndScoresUnchanged)
{
    edit_score("e\n");

    EXPECT_EQ("# Keep these rules\n", score_file());
    expect_scores({10, 10, 0});
    EXPECT_FALSE(g_art_scan.order_changed);
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
    const ArticlePosition position = g_art_input.pos;
    art_close();
    ASSERT_TRUE(fs::remove(m_spool / "1"));
    g_pushback.push('n');
    testing::internal::CaptureStdout();

    const ArticleSwitchResult result = art_switch(std::string{command, FINISH_CMD});

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(AS_ASK, result);
    const std::string message =
        g_verbose ? command == 'C' ? "Can't cancel an empty article." : "Can't supersede an empty article."
                  : "Empty article.";
    EXPECT_THAT(output, testing::HasSubstr(message));
    EXPECT_EQ(g_nav.current.num, g_nav.target.num);
    EXPECT_EQ(g_nav.current.art, g_nav.target.art);
    EXPECT_EQ(position, g_art_input.pos);
    EXPECT_EQ(2, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(0, g_nav.target.art->m_flags & (AF_EXISTS | AF_UNREAD | AF_SEL));
    EXPECT_TRUE(g_sel.selected_only);
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
    g_cache.ptr(ArticleNum{3})->m_flags &= ~AF_UNREAD;
    g_newsgroup_ptr->m_to_read--;
    g_cache.ptr(ArticleNum{2})->select_article(AUTO_KILL_NONE);
    g_sel.selected_only = GetParam();
    ASSERT_EQ(1, g_selected_count);

    const std::string output = display_tree();

    SCOPED_TRACE(output);
    EXPECT_NE(std::string::npos, output.find("Threading the group. "));
    EXPECT_NE(std::string::npos, output.find("[1] Topic\n[2] Branch\n"));
    const std::string current_marker = GetParam() ? "<1>" : "[1]";
    EXPECT_NE(std::string::npos, output.find("<so>" + current_marker + "<se>+-[<so>1<se>]"));
    EXPECT_NE(std::string::npos, output.find("\\-(2)"));
    EXPECT_EQ(g_nav.current.art, g_cache.ptr(ArticleNum{2})->m_parent);
    EXPECT_EQ(g_nav.current.art, g_cache.ptr(ArticleNum{3})->m_parent);
    EXPECT_EQ(g_cache.ptr(ArticleNum{2}), g_nav.current.art->m_child1);
    EXPECT_EQ(g_cache.ptr(ArticleNum{3}), g_cache.ptr(ArticleNum{2})->m_sibling);
    EXPECT_NE(AF_NONE, g_cache.ptr(ArticleNum{2})->m_flags & AF_SEL);
    EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{3})->m_flags & AF_UNREAD);
    EXPECT_EQ(1, g_selected_subj_cnt);
    EXPECT_EQ(ArticleNum{1}, g_cache.first_cached);
    EXPECT_EQ(ArticleNum{3}, g_cache.last_cached);
    EXPECT_TRUE(g_cache.cached_all_in_range);
    for (const auto &[number, article] : g_cache.article_list)
    {
        if (number > g_last_art)
        {
            break;
        }
        SCOPED_TRACE(number.value_of());
        EXPECT_NE(AF_NONE, article.m_flags & AF_THREADED);
        EXPECT_EQ(g_nav.current.art, article.m_subj->m_thread);
    }
}

INSTANTIATE_TEST_SUITE_P(ReadingScope, SelectedOnDemandThreadArticleTest, testing::Bool());

TEST_F(OnDemandThreadArticleTest, threadsCurrentHeaderWithoutEagerCaching)
{
    open_group(false);
    ASSERT_EQ(AF_NONE, g_cache.ptr(ArticleNum{2})->m_flags & AF_CACHED);

    const std::string output = display_tree();

    SCOPED_TRACE(output);
    EXPECT_NE(std::string::npos, output.find("Threading the group. "));
    EXPECT_NE(std::string::npos, output.find("[1] Topic\n\n  <so>[1]<se>\n"));
    EXPECT_NE(AF_NONE, g_nav.current.art->m_flags & AF_THREADED);
    EXPECT_EQ(g_nav.current.art, g_nav.current.art->m_subj->m_thread);
    EXPECT_EQ(nullptr, g_nav.current.art->m_child1);
    EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{2})->m_flags & (AF_CACHED | AF_THREADED));
    EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{3})->m_flags & (AF_CACHED | AF_THREADED));
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
    ASSERT_TRUE(g_ng_order.move(&g_newsgroup_data[2], NewsgroupNum{1}));
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
    ASSERT_TRUE(g_ng_order.move(&g_newsgroup_data[2], NewsgroupNum{1}));

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
    ASSERT_TRUE(g_ng_order.move(&g_newsgroup_data[2], NewsgroupNum{0}));

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

TEST_P(SuccessiveLocalArticlesTest, readingTheLocalSpoolToCompletionKeepsArticleStateConsistent)
{
    errno = 0;

    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::nullopt);
    const std::string       output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(3, m_states.size());
    for (int index = 0; index < 3; ++index)
    {
        SCOPED_TRACE(index);
        const LocalArticleReadingState &state = m_states[index];
        const ArticleNum                number{index + 1};
        const ArticleNum                recent{index == 0 ? 4 : index};
        EXPECT_EQ((ArticleLocation{number, g_cache.find(number)}), state.article);
        EXPECT_EQ(state.article, state.current);
        EXPECT_EQ((ArticleLocation{recent, index == 0 ? nullptr : g_cache.find(recent)}), state.recent);
        EXPECT_EQ(ArticleUnread{2 - index}, state.unread);
        EXPECT_EQ((std::array<bool, 3>{true, index >= 1, index >= 2}), state.read);
    }
    const std::size_t root = output.find("Root first line\nRoot next line");
    const std::size_t reply = output.find("Reply body");
    const std::size_t branch = output.find("Branch body");
    ASSERT_NE(std::string::npos, root);
    ASSERT_NE(std::string::npos, reply);
    ASSERT_NE(std::string::npos, branch);
    EXPECT_LT(root, reply);
    EXPECT_LT(reply, branch);
    EXPECT_EQ(std::string::npos, output.find("Root first line", root + 1));
    EXPECT_EQ(std::string::npos, output.find("Reply body", reply + 1));
    EXPECT_EQ(std::string::npos, output.find("Branch body", branch + 1));
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(ArticleNum{4}, g_nav.target.num);
    EXPECT_EQ(nullptr, g_nav.target.art);
    EXPECT_EQ(ArticleNum{4}, g_nav.current.num);
    EXPECT_EQ(nullptr, g_nav.current.art);
    EXPECT_EQ(ArticleNum{3}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(ArticleNum{3}), g_nav.recent.art);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_NEWSGROUP_LIST, g_mode);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(NextAndSpace, SuccessiveLocalArticlesTest, testing::Values('n', ' '));

TEST_P(LocalArticleTraversalTest, forwardReadingSkipsReadAndMissingArticlesInTheChosenOrder)
{
    const std::vector<int> expected = reading_order();

    const DoNewsgroupResult result = run_reading({" ", " ", " ", " ", " "});

    EXPECT_EQ(NG_NORM, result);
    expect_articles(expected);
    EXPECT_EQ((std::vector<ArticleUnread>{4, 3, 2, 1, 0}), m_unread);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-7", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{6})->m_flags & AF_EXISTS);
}

TEST_P(LocalArticleTraversalTest, previousUnreadReturnsToAnArticleLeftUnread)
{
    const int second = reading_order()[1];

    const DoNewsgroupResult result = run_reading({"m", " ", "p", "Q"});

    EXPECT_EQ(NG_ASK, result);
    EXPECT_EQ((std::vector<int>{1, 1, second, 1}), m_articles);
    EXPECT_EQ((std::vector<ArticleUnread>{4, 5, 4, 3}), m_unread);
    EXPECT_TRUE(g_cache.was_read(ArticleNum{1}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{second}));
    EXPECT_EQ(ArticleUnread{3}, g_newsgroup_ptr->m_to_read);
}

TEST_P(LocalArticleTraversalTest, selectedOnlyReadingLeavesOtherUnreadArticlesUntouched)
{
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);
    g_cache.ptr(ArticleNum{7})->select_article(AUTO_KILL_NONE);

    const DoNewsgroupResult result = run_reading({" ", "Q"});

    EXPECT_EQ(NG_ASK, result);
    expect_articles({3, 7});
    EXPECT_EQ((std::vector<ArticleUnread>{4, 3}), m_unread);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{1}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{4}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{7}));
    EXPECT_EQ(ArticleUnread{3}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-0,3,5-7", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalArticleTraversalTest, previousUnreadSkipsReadAndMissingArticles)
{
    const int previous = GetParam() == LocalReadingOrder::Numeric ? 4 : GetParam() == LocalReadingOrder::Date ? 3 : 2;

    const DoNewsgroupResult result = run_reading({"p", "Q"}, "7");

    EXPECT_EQ(NG_ASK, result);
    expect_articles({7, previous});
    EXPECT_EQ((std::vector<ArticleUnread>{4, 3}), m_unread);
    EXPECT_TRUE(g_cache.was_read(ArticleNum{5}));
    EXPECT_EQ(AF_NONE, g_cache.ptr(ArticleNum{6})->m_flags & AF_EXISTS);
}

TEST_P(LocalArticleTraversalTest, previousUnreadAtTheFirstArticleKeepsTheCurrentArticle)
{
    const DoNewsgroupResult result = run_reading({"p", "Q"});

    EXPECT_EQ(NG_ASK, result);
    EXPECT_EQ((std::vector<int>{1, 1}), m_articles);
    EXPECT_EQ((std::vector<ArticleUnread>{4, 4}), m_unread);
    EXPECT_THAT(m_output, testing::HasSubstr("No previous unread articles"));
    EXPECT_EQ(ArticleNum{1}, g_nav.current.num);
    EXPECT_EQ(g_cache.find(g_nav.current.num), g_nav.current.art);
}

TEST_P(LocalArticleTraversalTest, firstUnreadReturnsFromAnExplicitReadArticle)
{
    const DoNewsgroupResult result = run_reading({"^", "Q"}, "5");

    EXPECT_EQ(NG_ASK, result);
    expect_articles({5, 1});
    EXPECT_EQ((std::vector<ArticleUnread>{5, 4}), m_unread);
    EXPECT_EQ(ArticleNum{5}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
}

TEST_P(LocalArticleTraversalTest, anExplicitReadTargetBypassesSelectedOnlyFiltering)
{
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);

    const DoNewsgroupResult result = run_reading({"Q"}, "5");

    EXPECT_EQ(NG_ASK, result);
    expect_articles({5});
    EXPECT_EQ((std::vector<ArticleUnread>{5}), m_unread);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(ArticleUnread{5}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-0,5-6", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalArticleTraversalTest, anExplicitUnselectedUnreadTargetIsRead)
{
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);

    const DoNewsgroupResult result = run_reading({"Q"}, "2");

    EXPECT_EQ(NG_ASK, result);
    expect_articles({2});
    EXPECT_EQ((std::vector<ArticleUnread>{4}), m_unread);
    EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(ArticleUnread{4}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-0,2,5-6", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalArticleTraversalTest, previousArticleIncludesReadArticlesInTheChosenOrder)
{
    const int second = reading_order()[1];
    const int previous = GetParam() == LocalReadingOrder::Subject ? 5 : 1;

    const DoNewsgroupResult result = run_reading({" ", "P", "Q"});

    EXPECT_EQ(NG_ASK, result);
    expect_articles({1, second, previous});
    EXPECT_EQ((std::vector<ArticleUnread>{4, 3, 3}), m_unread);
    EXPECT_EQ(ArticleNum{second}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
}

TEST_P(LocalArticleTraversalTest, nextArticleCanRereadAnAlreadyReadArticle)
{
    const int second = reading_order()[1];
    const int next = GetParam() == LocalReadingOrder::Subject ? 5 : second;

    const DoNewsgroupResult result = run_reading({" ", "1\n", "N", "Q"});

    EXPECT_EQ(NG_ASK, result);
    expect_articles({1, second, 1, next});
    EXPECT_EQ((std::vector<ArticleUnread>{4, 3, 3, 3}), m_unread);
    EXPECT_EQ(ArticleUnread{3}, g_newsgroup_ptr->m_to_read);
}

TEST_P(LocalArticleTraversalTest, minusRereadsTheMostRecentlyDisplayedArticle)
{
    const int second = reading_order()[1];

    const DoNewsgroupResult result = run_reading({" ", "-", "Q"});

    EXPECT_EQ(NG_ASK, result);
    expect_articles({1, second, 1});
    EXPECT_EQ((std::vector<ArticleUnread>{4, 3, 3}), m_unread);
    EXPECT_EQ(ArticleNum{second}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
}

TEST_P(LocalArticleTraversalTest, restartingTheCurrentArticleDoesNotMarkItReadTwice)
{
    const DoNewsgroupResult result = run_reading({"\022", "Q"});

    EXPECT_EQ(NG_ASK, result);
    expect_articles({1, 1});
    EXPECT_EQ((std::vector<ArticleUnread>{4, 4}), m_unread);
    EXPECT_EQ(ArticleUnread{4}, g_newsgroup_ptr->m_to_read);
}

INSTANTIATE_TEST_SUITE_P(ReadingOrder, LocalArticleTraversalTest,
                         testing::Values(LocalReadingOrder::Numeric, LocalReadingOrder::Date,
                                         LocalReadingOrder::Subject, LocalReadingOrder::Thread),
                         [](const testing::TestParamInfo<LocalReadingOrder> &info)
                         {
                             switch (info.param)
                             {
                             case LocalReadingOrder::Numeric:
                                 return "Numeric";
                             case LocalReadingOrder::Date:
                                 return "Date";
                             case LocalReadingOrder::Subject:
                                 return "Subject";
                             case LocalReadingOrder::Thread:
                                 return "Thread";
                             }
                             return "Unknown";
                         });

TEST_P(EmptyNewsgroupEndTest, entryOffersAnEndPromptWithoutDisplayingAnArticle)
{
    const DoNewsgroupResult result = run_reading({"Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(1, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ(article_after(g_last_art), m_states[0].article);
    EXPECT_EQ(TR_NONE, m_states[0].unread);
    EXPECT_EQ(TR_NONE, m_states[0].selected);
    EXPECT_EQ("npq", m_states[0].default_command);
    expect_end_heading();
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("still unread")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("body")));
    EXPECT_EQ(nullptr, g_nav.current.art);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
}

TEST_P(EmptyNewsgroupEndTest, theDefaultCommandAtTheInitialEndPromptLeavesTheGroup)
{
    const DoNewsgroupResult result = run_reading({" "});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(1, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ("npq", m_states[0].default_command);
    expect_end_heading();
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("body")));
}

TEST_P(ForcedNewsgroupEndCountTest, jumpingToTheEndDisplaysTheCurrentUnreadCount)
{
    const ArticleUnread unread = std::get<2>(GetParam()).second;

    const DoNewsgroupResult result = run_reading({"Q"}, "$");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(1, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ(ArticleNum{4}, m_states[0].article);
    EXPECT_EQ(unread, m_states[0].unread);
    EXPECT_EQ("npq", m_states[0].default_command);
    expect_end_heading();
    if (unread)
    {
        const std::string count =
            "(" + std::to_string(unread) + (unread == 1 ? " article still unread)" : " articles still unread)");
        EXPECT_THAT(m_output, testing::HasSubstr(count));
    }
    else
    {
        EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("still unread")));
    }
    EXPECT_EQ(unread, g_newsgroup_ptr->m_to_read);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("body")));
}

TEST_P(NewsgroupEndReadingTest, finishingTheLastUnreadArticleLeavesWithoutAnotherInputRequest)
{
    set_read_range("1-2");

    const DoNewsgroupResult result = run_reading({"n"});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(1, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[0].mode);
    EXPECT_EQ(ArticleNum{3}, m_states[0].article);
    EXPECT_EQ(TR_NONE, m_states[0].unread);
    EXPECT_EQ("npq", m_states[0].default_command);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(ArticleNum{4}, g_nav.current.num);
    EXPECT_EQ(nullptr, g_nav.current.art);
    EXPECT_EQ(ArticleNum{3}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("What next? [npq]")));
}

TEST_P(NewsgroupEndReadingTest, anExplicitEndJumpStillPromptsAfterTheLastUnreadArticle)
{
    set_read_range("1-2");

    const DoNewsgroupResult result = run_reading({"$", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[0].mode);
    EXPECT_EQ(ArticleNum{3}, m_states[0].article);
    EXPECT_EQ(MM_ARTICLE_END, m_states[1].mode);
    EXPECT_EQ(ArticleNum{4}, m_states[1].article);
    EXPECT_EQ(TR_NONE, m_states[1].unread);
    EXPECT_EQ("npq", m_states[1].default_command);
    expect_end_heading();
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(ArticleNum{3}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
}

TEST_P(NewsgroupEndReadingTest, nextAtAForcedEndPromptReadsTheFirstUnreadArticle)
{
    const DoNewsgroupResult result = run_reading({"n", "Q"}, "$");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ(ArticleUnread{2}, m_states[0].unread);
    EXPECT_EQ(MM_ARTICLE, m_states[1].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[1].article);
    EXPECT_EQ(ArticleUnread{1}, m_states[1].unread);
    expect_end_heading();
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Branch body")));
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupEndReadingTest, nextArticleAtAnAllReadEndPromptRereadsTheFirstArticle)
{
    set_read_range("1-3");

    const DoNewsgroupResult result = run_reading({"N", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ(MM_ARTICLE, m_states[1].mode);
    EXPECT_EQ(ArticleNum{1}, m_states[1].article);
    EXPECT_EQ(TR_NONE, m_states[1].unread);
    expect_end_heading();
    EXPECT_THAT(m_output, testing::HasSubstr("Root first line\nRoot next line"));
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupEndReadingTest, aForcedEndPromptSplitsSelectedAndUnselectedUnreadCounts)
{
    select_reply();

    const DoNewsgroupResult result = run_reading({"Q"}, "$");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(1, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    EXPECT_EQ(ArticleUnread{2}, m_states[0].unread);
    EXPECT_EQ(ArticleUnread{1}, m_states[0].selected);
    EXPECT_EQ("npq", m_states[0].default_command);
    expect_end_heading();
    EXPECT_THAT(m_output, testing::HasSubstr("(1 + 1 articles still unread)"));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
}

TEST_P(NewsgroupEndReadingTest, exhaustingSelectionsOffersTheRemainingUnreadArticlesInTheSelector)
{
    select_reply();

    const DoNewsgroupResult result = run_reading({"n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[0].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(ArticleUnread{1}, m_states[0].unread);
    EXPECT_EQ(TR_NONE, m_states[0].selected);
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[1].mode);
    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}}), m_states[1].selector_articles);
    EXPECT_EQ(ArticleUnread{1}, m_states[1].unread);
    EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("End of comp.lang.cpp")));
}

TEST_P(NewsgroupEndReadingTest, acceptingTheExhaustedSelectionPageContinuesReadingTheRemainingArticle)
{
    select_reply();

    const DoNewsgroupResult result = run_reading({"n", "Z", "n"});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(3, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[0].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[1].mode);
    EXPECT_EQ((std::vector<ArticleNum>{ArticleNum{3}}), m_states[1].selector_articles);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_EQ(TR_NONE, m_states[2].unread);
    EXPECT_THAT(m_output, testing::HasSubstr("Reply body"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupEndReadingTest, cancellingTheExhaustedSelectionPageReturnsToTheLastArticlePrompt)
{
    select_reply();

    const DoNewsgroupResult result = run_reading({"n", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3, m_states.size());
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[1].mode);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[2].article);
    EXPECT_EQ(ArticleUnread{1}, m_states[2].unread);
    EXPECT_EQ("npq", m_states[2].default_command);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Branch body")));
}

INSTANTIATE_TEST_SUITE_P(PresentationAndSpool, EmptyNewsgroupEndTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(PresentationAndReadRange, ForcedNewsgroupEndCountTest,
                         testing::Combine(testing::Bool(), testing::Bool(),
                                          testing::Values(ForcedEndReadRange{"", 3}, ForcedEndReadRange{"1", 2},
                                                          ForcedEndReadRange{"1-2", 1}, ForcedEndReadRange{"1-3", 0})));

INSTANTIATE_TEST_SUITE_P(Presentation, NewsgroupEndReadingTest, testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(EmptyArticleSelectorReadingTest, returningFromAnEmptySelectorRestoresTheEndPrompt)
{
    const DoNewsgroupResult result = run_reading({"+", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    expect_empty(1);
    expect_returned_context(2);
    EXPECT_EQ(TR_NONE, m_states[2].unread);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged selector body")));
}

TEST_P(EmptyArticleSelectorReadingTest, switchingScopeRebuildsAnEmptyUnreadPopulationWithoutStaleRows)
{
    const bool no_files = std::get<2>(GetParam());

    const DoNewsgroupResult result = run_reading({"+", "U", "U", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5U, m_states.size());
    expect_empty(1);
    EXPECT_EQ(MM_THREAD_SELECTOR, m_states[2].mode);
    EXPECT_TRUE(m_states[2].selector_rereading);
    EXPECT_EQ(AGF_DEL_SEL, m_states[2].selector_mask);
    EXPECT_EQ(no_files ? 0 : 11, m_cache[2].total);
    if (no_files)
    {
        expect_empty(2, true);
    }
    expect_empty(3);
    expect_returned_context(4);
    EXPECT_EQ(TR_NONE, m_states[4].unread);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged selector body")));
}

INSTANTIATE_TEST_SUITE_P(ModeScreenAndEmptySpool, EmptyArticleSelectorReadingTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool(),
                                          testing::Bool()));

TEST_P(EmptyReadArticleSelectorReadingTest, anEmptyReadSelectorReturnsToThePartlyReadArticleAndAllowsNextReading)
{
    const DoNewsgroupResult result = run_reading({"q", "U+", "+", "n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5U, m_states.size());
    EXPECT_EQ(MM_PAGER, m_states[0].mode);
    EXPECT_EQ(MM_ARTICLE, m_states[1].mode);
    EXPECT_EQ(ArticleNum{1}, m_states[1].article);
    EXPECT_TRUE(m_read[1].empty());
    expect_empty(2, true);
    EXPECT_EQ(ArticleUnread{11}, m_states[2].unread);
    expect_returned_context(3, 1);
    EXPECT_TRUE(m_read[3].empty());
    EXPECT_EQ(ArticleNum{2}, m_states[4].article);
    EXPECT_EQ((std::vector<int>{2}), m_read[4]);
    expect_bodies({2});
    EXPECT_THAT(m_output, testing::HasSubstr("Partial selector body line 1"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Partial selector body END")));
    EXPECT_EQ(" 1-0,2", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, EmptyReadArticleSelectorReadingTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool()));

TEST_P(InterruptedArticleSelectorReadingTest, interruptedEntryRestoresTheCallerAndExistingSelections)
{
    const char result = interrupt_entry();

    EXPECT_EQ('+', result);
    expect_interrupted_return();
    expect_bodies({});
    EXPECT_EQ(" 1-4", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(ArticleUnread{7}, g_newsgroup_ptr->m_to_read);
}

TEST_P(InterruptedArticleSelectorReadingTest, retryingAnInterruptedEntryBuildsACompleteUsableSelector)
{
    const auto &[input, erase_screen, selected_only, rereading] = GetParam();
    const int                      first_unread = rereading && input.mode != SM_ARTICLE && !selected_only ? 7 : 5;
    const std::vector<std::string> commands =
        rereading ? std::vector<std::string>{"U+", "+", "^", "Q"} : std::vector<std::string>{"+", "+", "^", "Q"};
    const std::vector<int> page = rereading                  ? std::vector<int>{1, 2}
                                  : input.mode == SM_ARTICLE ? std::vector<int>{5, 6}
                                                             : std::vector<int>{5, 10, 6, 11};

    ASSERT_EQ('+', interrupt_entry());
    expect_interrupted_return();
    g_in_ng = false;
    set_mode(GM_READ, MM_NEWSGROUP_LIST);

    const DoNewsgroupResult result = run_reading(commands, "4");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(4U, m_states.size());
    expect_page(1, page);
    EXPECT_EQ(ArticleNum{11}, m_cache[1].last_cached);
    if (rereading)
    {
        EXPECT_TRUE(m_cache[1].all_cached);
    }
    EXPECT_EQ(rereading, m_states[1].selector_rereading);
    EXPECT_EQ(rereading ? AGF_DEL_SEL : AGF_SEL, m_states[1].selector_mask);
    expect_returned_context(2);
    EXPECT_EQ(selected_only, std::find(m_selected[2].begin(), m_selected[2].end(), 5) != m_selected[2].end());
    EXPECT_EQ(ArticleUnread{static_cast<long>(m_selected[2].size())}, m_states[2].selected);
    EXPECT_EQ(ArticleNum{first_unread}, m_states[3].article);
    EXPECT_EQ(selected_only, m_states[3].selected_only);
    expect_bodies({4, first_unread});
    EXPECT_EQ(first_unread == 7 ? " 1-4,7" : " 1-5", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeScreenSelectionAndReadScope, InterruptedArticleSelectorReadingTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool(),
                                          testing::Bool(), testing::Bool()));

TEST_P(InterruptedSelectorRestartReadingTest, interruptedReadScopeLoadingRetainsTheUnreadPopulationAndChoices)
{
    const DoNewsgroupResult result = run_reading({"+", "a", "U", "+", "^", "Q"}, "4");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(6U, m_states.size());
    EXPECT_EQ(1, m_interruptions);
    expect_page(3, m_unread_page);
    EXPECT_FALSE(m_states[3].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[3].selector_mask);
    EXPECT_FALSE(m_cache[3].all_cached);
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4}), m_read[3]);
    expect_selected(3, m_delta);
    EXPECT_EQ(ArticleUnread{static_cast<long>(m_delta.size())}, m_states[3].selected);
    expect_returned_context(4);
    EXPECT_EQ(ArticleNum{5}, m_states[5].article);
    EXPECT_FALSE(m_states[5].selected_only);
    expect_bodies({4, 5});
    EXPECT_EQ(" 1-5", g_newsgroup_ptr->rc_numbers());
}

TEST_P(InterruptedSelectorRestartReadingTest, retryingTheScopeChangeCanReviveAndReadAnArticleWithExistingChoices)
{
    const bool               grouped = std::get<0>(GetParam()).mode != SM_ARTICLE;
    const std::vector<int>   batch = grouped ? std::vector<int>{2, 5, 10} : std::vector<int>{2, 5};
    std::vector<std::string> commands{"+", "a", "U", "U", "b", "Z"};
    append_reading(commands, batch);
    std::vector<int> bodies{4};
    bodies.insert(bodies.end(), batch.begin(), batch.end());

    const DoNewsgroupResult result = run_reading(commands, "4");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    EXPECT_EQ(1, m_interruptions);
    EXPECT_FALSE(m_states[3].selector_rereading);
    expect_selected(3, m_delta);
    expect_page(4, {1, 2});
    EXPECT_TRUE(m_states[4].selector_rereading);
    EXPECT_EQ(AGF_DEL_SEL, m_states[4].selector_mask);
    EXPECT_TRUE(m_cache[4].all_cached);
    EXPECT_EQ(ArticleNum{11}, m_cache[4].last_cached);
    expect_selected(4, m_delta);
    EXPECT_EQ(ArticleUnread{1}, m_states[5].selected);
    EXPECT_EQ(ArticleNum{2}, m_states[6].article);
    EXPECT_TRUE(m_states[6].selected_only);
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4}), m_read[6]);
    expect_bodies(bodies);
    EXPECT_EQ(grouped ? " 1-5,10" : " 1-5", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, InterruptedSelectorRestartReadingTest,
                         testing::Combine(testing::Values(PAGED_ARTICLE_SELECTOR_CASES[0],
                                                          PAGED_ARTICLE_SELECTOR_CASES[1]),
                                          testing::Bool()));

TEST_P(ArticleSelectorCleanupReadingTest, acceptingCommitsPendingKillsBeforeReadingTheOffPageSelection)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const std::vector<int>          killed = completed_with({1}, m_gamma);
    std::vector<std::string>        commands{"+", "b", ">", "k", "Z"};
    append_reading(commands, m_beta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_marks(4, m_gamma);
    expect_read(4, {1});
    expect_selected(4, m_beta);
    expect_marks(5, {});
    expect_reading(m_beta, true, killed);
    expect_saved(input.batch_range, completed_with(killed, m_beta));
}

TEST_P(ArticleSelectorCleanupReadingTest, returningCommitsPendingKillsAndRestoresOrdinaryReading)
{
    const bool             threaded = std::get<0>(GetParam()).mode == SM_THREAD;
    const std::vector<int> killed = completed_with({1}, m_gamma);

    const DoNewsgroupResult result = run_reading({"+", "b", ">", "k", "+", "^", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(7U, m_states.size());
    expect_marks(4, m_gamma);
    expect_read(4, {1});
    expect_returned_context(5);
    expect_marks(5, {});
    expect_read(5, killed);
    expect_selected(5, m_beta);
    EXPECT_EQ(ArticleNum{2}, m_states[6].article);
    EXPECT_FALSE(m_states[6].selected_only);
    expect_bodies({1, 2});
    expect_saved(threaded ? " 1-2,4,9" : " 1-2,4", completed_with(killed, {2}));
}

TEST_P(ArticleSelectorCleanupReadingTest, unmarkingBeforeAcceptanceCancelsThePendingKill)
{
    const bool               grouped = std::get<0>(GetParam()).mode != SM_ARTICLE;
    std::vector<std::string> commands{"+", "b", ">", "k", std::string(1, Ctl('g')) + "a", "m", "Z"};
    append_reading(commands, m_beta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_marks(4, m_gamma);
    expect_marks(6, {});
    expect_read(6, {1});
    expect_selected(6, m_beta);
    expect_reading(m_beta, true);
    expect_saved(grouped ? " 1-1,3,8" : " 1-1,3", completed_with({1}, m_beta));
}

TEST_P(ArticleSelectorCleanupReadingTest, changingScopeCommitsKillsBeforeBuildingTheReadPopulation)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const std::vector<int>          killed = completed_with({1}, m_gamma);
    std::vector<std::string>        commands{"+", "b", ">", "k", "U", "U", "Z"};
    append_reading(commands, m_beta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_marks(4, m_gamma);
    expect_read(4, {1});
    EXPECT_TRUE(m_states[5].selector_rereading);
    expect_read(5, killed);
    expect_page(5, killed);
    expect_marks(5, {});
    EXPECT_FALSE(m_states[6].selector_rereading);
    expect_selected(6, m_beta);
    expect_reading(m_beta, true, killed);
    expect_saved(input.batch_range, completed_with(killed, m_beta));
}

TEST_P(ArticleSelectorCleanupReadingTest, acceptedDelayedArticlesRemainReadUntilTheGroupIsLeft)
{
    const bool               grouped = std::get<0>(GetParam()).mode != SM_ARTICLE;
    const std::vector<int>   delayed_read = completed_with({1}, m_gamma);
    std::vector<std::string> commands{"+", "b", ">", "M", "Z"};
    append_reading(commands, m_beta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_marks(4, m_gamma, m_gamma);
    expect_read(4, {1});
    expect_marks(5, {}, m_gamma);
    expect_reading(m_beta, true, delayed_read);
    expect_saved(grouped ? " 1-1,3,8" : " 1-1,3", completed_with({1}, m_beta));
}

TEST_P(ArticleSelectorCleanupReadingTest, returningToTheArticleKeepsDelayedRestorationPendingUntilGroupExit)
{
    const DoNewsgroupResult result = run_reading({"+", "b", ">", "M", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(6U, m_states.size());
    expect_marks(4, m_gamma, m_gamma);
    expect_read(4, {1});
    expect_returned_context(5);
    expect_marks(5, {}, m_gamma);
    expect_read(5, completed_with({1}, m_gamma));
    expect_selected(5, m_beta);
    expect_bodies({1});
    expect_saved(" 1-1", {1});
}

TEST_P(ArticleSelectorCleanupReadingTest, yankingPendingDelayedItemsClearsTheirKillsAndJoinsExistingSelections)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    std::vector<std::string>        commands{"+", "b", ">", "M", "Y", "Z"};
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_marks(4, m_gamma, m_gamma);
    expect_marks(5, {});
    expect_read(5, {1});
    expect_page(5, input.first_page);
    expect_selected(5, input.batch);
    expect_reading(input.batch, true);
    expect_saved(input.batch_range, completed_with({1}, input.batch));
}

TEST_P(ArticleSelectorCleanupReadingTest, groupBulkReadCommitsUnselectedArticlesBeforeReadingTheSelectedBatch)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    std::vector<int>                killed{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    for (int selected : input.batch)
    {
        killed.erase(std::find(killed.begin(), killed.end(), selected));
    }
    std::vector<std::string> commands = selection_commands();
    commands.push_back("X");
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_read(6, {1});
    expect_selected(6, input.batch);
    expect_marks(7, {});
    expect_reading(input.batch, true, killed);
    expect_saved(" 1-11", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
}

TEST_P(ArticleSelectorCleanupReadingTest, pageBulkReadPreservesUnselectedArticlesOnOtherPages)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const char                     *range = input.mode == SM_ARTICLE  ? " 1-1,3-5"
                                            : input.mode == SM_THREAD ? " 1-1,3-5,8-10"
                                                                      : " 1-1,3-5,8,10";
    const std::vector<int>          killed = completed_with({1}, m_delta);
    std::vector<std::string>        commands = selection_commands();
    commands.push_back("D");
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_read(6, {1});
    expect_selected(6, input.batch);
    expect_reading(input.batch, true, killed);
    expect_saved(range, completed_with(killed, input.batch));
}

TEST_P(ArticleSelectorCleanupReadingTest, pageBulkReadWithoutSelectionsDisplaysTheNextPageBeforeAccepting)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const char *range = input.mode == SM_ARTICLE ? " 1-4" : input.mode == SM_THREAD ? " 1-4,7-9" : " 1-4,7-8";
    const std::vector<int>   killed = completed_with({1}, input.first_page);
    std::vector<std::string> commands{"+", "D", "\n"};
    append_reading(commands, m_gamma);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_read(1, {1});
    expect_page(2, input.second_page);
    expect_read(2, killed);
    expect_selected(2, {});
    expect_reading(m_gamma, true, killed);
    expect_saved(range, completed_with(killed, m_gamma));
}

TEST_P(ArticleSelectorCleanupReadingTest, junkingTheSelectedBatchRebuildsSurvivorsBeforeTheNextSelection)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const char                     *range = input.mode == SM_ARTICLE  ? " 1-1,3-5"
                                            : input.mode == SM_THREAD ? " 1-1,3-5,8-10"
                                                                      : " 1-1,3-5,8,10";
    const std::vector<int>          killed = completed_with({1}, input.batch);
    std::vector<std::string>        commands = selection_commands();
    commands.insert(commands.end(), {"J", "a", "Z"});
    append_reading(commands, m_delta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_read(6, {1});
    expect_page(7, input.mode == SM_ARTICLE ? std::vector<int>{5, 6} : std::vector<int>{5, 10, 6, 11});
    expect_read(7, killed);
    expect_selected(7, {});
    expect_marks(7, {});
    expect_reading(m_delta, true, killed);
    expect_saved(range, completed_with(killed, m_delta));
}

TEST_P(ArticleSelectorCleanupReadingTest, decliningCatchupRetainsSelectionsAndPendingKillsUntilAcceptance)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const std::vector<int>          killed = completed_with({1}, m_gamma);
    std::vector<std::string>        commands{"+", "b", ">", "k", "c", "n", "Z"};
    append_reading(commands, m_beta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    EXPECT_EQ(MM_CONFIRM_CATCH_UP_PROMPT, m_states[5].mode);
    expect_marks(6, m_gamma);
    expect_read(6, {1});
    expect_selected(6, m_beta);
    expect_reading(m_beta, true, killed);
    expect_saved(input.batch_range, completed_with(killed, m_beta));
}

TEST_P(ArticleSelectorCleanupReadingTest, fullCatchupLeavesTheGroupWithAllArticlesRead)
{
    std::ofstream{m_output_dir / "comp/lang/cpp/2", std::ios::binary}
        << "From: Reader <reader@example.test>\nSubject: Alpha\n"
           "Date: 21 Oct 2026 10:00:00 GMT\nMessage-ID: <page2@example.test>\n"
           "Xref: news.example.test comp.lang.cpp:2 comp.previous:3\n\nPaged selector body 2 END\n";

    const DoNewsgroupResult result = run_reading({"+", "b", ">", "a", "c", "y"});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(6U, m_states.size());
    EXPECT_EQ(MM_CONFIRM_CATCH_UP_PROMPT, m_states[5].mode);
    expect_read(5, {1});
    expect_bodies({1});
    expect_saved(" 1-11", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
}

TEST_P(ArticleSelectorCleanupReadingTest, partialCatchupRebuildsTheUnreadTailBeforeReadingAChosenArticle)
{
    const std::vector<int> caught_up{1, 2, 3, 4, 5, 6, 7, 8, 9};

    const DoNewsgroupResult result = run_reading({"+", "b", ">", "a", "c", "#", "2\n", "a", "Z", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(10U, m_states.size());
    EXPECT_EQ(MM_CONFIRM_CATCH_UP_PROMPT, m_states[5].mode);
    EXPECT_EQ(MM_CONFIRM_CATCH_UP_PROMPT, m_states[6].mode);
    expect_page(7, {10, 11});
    expect_read(7, caught_up);
    expect_selected(7, {});
    expect_marks(7, {});
    expect_reading({10}, true, caught_up);
    expect_saved(" 1-10", {1, 2, 3, 4, 5, 6, 7, 8, 9, 10});
}

TEST_P(ArticleSelectorCleanupReadingTest, inlineJunkCommitsTheSelectedBatchBeforeReturningToOrdinaryReading)
{
    const PagedArticleSelectorCase &input = std::get<0>(GetParam());
    const char            *range = input.mode == SM_ARTICLE ? " 1-4" : input.mode == SM_THREAD ? " 1-4,8-9" : " 1-4,8";
    const std::vector<int> killed = completed_with({1}, input.batch);

    const DoNewsgroupResult result = run_reading({"+", "b", ">", "a", ":j\n", "+", "^", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(8U, m_states.size());
    expect_read(4, {1});
    expect_selected(4, input.batch);
    expect_read(5, killed);
    expect_selected(5, {});
    expect_marks(5, {});
    expect_returned_context(6);
    EXPECT_EQ(ArticleNum{2}, m_states[7].article);
    EXPECT_FALSE(m_states[7].selected_only);
    expect_bodies({1, 2});
    expect_saved(range, completed_with(killed, {2}));
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, ArticleSelectorCleanupReadingTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool()));

TEST_P(ArticleSelectorSubjectOrderReadingTest, changingSubjectOrderReadsTheRetainedOffPageSelectionsInThatOrder)
{
    const auto &[input, reverse, erase_screen] = GetParam();
    const std::vector<int>  &batch = reverse ? input.reverse_batch : input.forward_batch;
    std::vector<std::string> commands{"+", "b", ">", "a", "O", reverse ? "S" : "s", "Z"};
    append_reading(commands, batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    EXPECT_EQ(MM_Q, m_states[5].mode);
    expect_view(6, input.mode, SS_STRING, reverse ? -1 : 1, reverse ? input.reverse_page : input.forward_page,
                input.forward_batch);
    EXPECT_NE(std::string::npos, m_output.find("Order by Date"));
    expect_reading(batch, true);
    EXPECT_EQ(input.read_range, g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeDirectionAndScreen, ArticleSelectorSubjectOrderReadingTest,
                         testing::Combine(testing::ValuesIn(SELECTOR_SUBJECT_ORDER_CASES), testing::Bool(),
                                          testing::Bool()));

TEST_P(ArticleSelectorPresentationReadingTest, reversingTwiceRebuildsFromTheFirstPageAndRetainsSelections)
{
    const auto &[input, erase_screen] = GetParam();
    const SelectionSortMode  sort = input.mode == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    const std::vector<int>   reversed = input.mode == SM_ARTICLE  ? std::vector<int>{11, 10}
                                        : input.mode == SM_THREAD ? std::vector<int>{6, 11, 5, 10}
                                                                  : std::vector<int>{9, 6, 11};
    std::vector<std::string> commands{"+", "b", ">", "a", "R", "R", "Z"};
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_view(5, input.mode, sort, -1, reversed, input.batch);
    expect_view(6, input.mode, sort, 1, input.first_page, input.batch);
    expect_reading(input.batch, true);
    EXPECT_EQ(input.batch_range, g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorPresentationReadingTest, cancellingTheOrderPromptKeepsTheCurrentPageAndSelections)
{
    const auto &[input, erase_screen] = GetParam();
    const SelectionSortMode  sort = input.mode == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    const int                prior = static_cast<int>(input.first_page.size());
    std::vector<std::string> commands{"+", "b", ">", "a", "O", "q", "Z"};
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    EXPECT_EQ(MM_Q, m_states[5].mode);
    expect_view(6, input.mode, sort, 1, input.second_page, input.batch, prior);
    expect_reading(input.batch, true);
    EXPECT_EQ(input.batch_range, g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorPresentationReadingTest, cyclingDisplayStylesKeepsThePageAnchorAndOffPageSelections)
{
    const auto &[input, erase_screen] = GetParam();
    const SelectionSortMode  sort = input.mode == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    const int                prior = static_cast<int>(input.first_page.size());
    std::vector<std::string> commands{"+", "b", ">", "a", "L", "L", "L", "L", "Z"};
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_view(5, input.mode, sort, 1, input.second_page, input.batch, prior);
    expect_view(6, input.mode, sort, 1, input.second_page, input.batch, prior);
    expect_view(7, input.mode, sort, 1, input.second_page, input.batch, prior);
    expect_view(8, input.mode, sort, 1, input.second_page, input.batch, prior);
    EXPECT_EQ('l', m_presentation[5].style);
    EXPECT_EQ('m', m_presentation[6].style);
    EXPECT_EQ('d', m_presentation[7].style);
    EXPECT_EQ('s', m_presentation[8].style);
    EXPECT_NE(std::string::npos, m_output.find("(long display style)"));
    EXPECT_NE(std::string::npos, m_output.find("(medium display style)"));
    EXPECT_NE(std::string::npos, m_output.find("(date display style)"));
    EXPECT_NE(std::string::npos, m_output.find("(short display style)"));
    EXPECT_TRUE(std::regex_search(m_output, std::regex{"Reader[^\\r\\n]*Gamma"}));
    EXPECT_TRUE(std::regex_search(m_output, std::regex{"Oct[^\\r\\n]*Gamma"}));
    expect_reading(input.batch, true);
    EXPECT_EQ(input.batch_range, g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, ArticleSelectorPresentationReadingTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool()));

TEST_P(ArticleSelectorGroupingReadingTest, changingGroupingPreservesPartialSelectionsAndUsesTheNewReadingOrder)
{
    const SelectorGroupingReadingCase &input = std::get<0>(GetParam());
    const SelectionSortMode            sort = input.target == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    std::vector<std::string>           commands{"+", "b", ">", "a", "S", std::string(1, input.response), "Z"};
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    EXPECT_EQ(MM_SELECTOR_ORDER_PROMPT, m_states[5].mode);
    expect_view(6, input.target, sort, 1, input.prompt_page, m_initial->batch, input.prompt_prior);
    EXPECT_EQ(input.initial == SM_THREAD || input.target == SM_THREAD, m_states[6].selector_threaded);
    EXPECT_NE(std::string::npos, m_output.find("Selector mode:  Threads, Subjects, Articles?"));
    expect_reading(input.batch, true);
    EXPECT_EQ(m_initial->batch_range, g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorGroupingReadingTest, returningAfterChangingGroupingRestoresTheArticleAndAllowsOrdinaryReading)
{
    const SelectorGroupingReadingCase &input = std::get<0>(GetParam());

    const DoNewsgroupResult result =
        run_reading({"+", "b", ">", "a", "S", std::string(1, input.response), "+", "^", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(9U, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[7].mode);
    EXPECT_EQ(m_states[0].article, m_states[7].article);
    EXPECT_EQ(m_states[0].current, m_states[7].current);
    EXPECT_EQ(m_states[0].recent, m_states[7].recent);
    EXPECT_FALSE(m_states[7].selected_only);
    expect_selected(7, m_initial->batch);
    EXPECT_EQ((std::vector<int>{1}), m_read[7]);
    EXPECT_EQ(MM_ARTICLE, m_states[8].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[8].article);
    EXPECT_FALSE(m_states[8].selected_only);
    expect_bodies({1, 2});
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(TransitionAndScreen, ArticleSelectorGroupingReadingTest,
                         testing::Combine(testing::ValuesIn(SELECTOR_GROUPING_READING_CASES), testing::Bool()));

TEST_P(ArticleSelectorToggleGroupingReadingTest, togglingGroupingBackRetainsThePageAnchorAndOriginalSelections)
{
    const SelectorGroupingReadingCase &input = std::get<0>(GetParam());
    const SelectionSortMode            target_sort = input.target == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    const SelectionSortMode            initial_sort = input.initial == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    std::vector<std::string>           commands{"+", "b", ">", "a", "=", "=", "Z"};
    append_reading(commands, m_initial->batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_view(5, input.target, target_sort, 1, input.toggle_page, m_initial->batch, input.toggle_prior);
    expect_view(6, input.initial, initial_sort, 1, m_initial->second_page, m_initial->batch,
                static_cast<int>(m_initial->first_page.size()));
    expect_reading(m_initial->batch, true);
    EXPECT_EQ(m_initial->batch_range, g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorToggleGroupingReadingTest, eachGroupingRetainsItsOwnSortWhenReturningToThatView)
{
    const SelectorGroupingReadingCase &input = std::get<0>(GetParam());
    const auto                         order =
        std::find_if(std::begin(SELECTOR_SUBJECT_ORDER_CASES), std::end(SELECTOR_SUBJECT_ORDER_CASES),
                     [&input](const SelectorSubjectOrderCase &value) { return value.mode == input.initial; });
    const SelectionSortMode  target_sort = input.target == SM_ARTICLE ? SS_NATURAL : SS_DATE;
    std::vector<std::string> commands{"+", "b", ">", "a", "O", "D", "=", "=", "Z"};
    append_reading(commands, order->reverse_batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    EXPECT_EQ(input.initial, m_states[6].selector_mode);
    EXPECT_EQ(SS_DATE, m_presentation[6].sort);
    EXPECT_EQ(-1, m_presentation[6].direction);
    EXPECT_EQ(input.target, m_states[7].selector_mode);
    EXPECT_EQ(target_sort, m_presentation[7].sort);
    EXPECT_EQ(1, m_presentation[7].direction);
    EXPECT_EQ(input.initial, m_states[8].selector_mode);
    EXPECT_EQ(SS_DATE, m_presentation[8].sort);
    EXPECT_EQ(-1, m_presentation[8].direction);
    expect_selected(6, m_initial->batch);
    expect_selected(7, m_initial->batch);
    expect_selected(8, m_initial->batch);
    expect_reading(order->reverse_batch, true);
    EXPECT_EQ(m_initial->batch_range, g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(
    TransitionAndScreen, ArticleSelectorToggleGroupingReadingTest,
    testing::Combine(testing::Values(SELECTOR_GROUPING_READING_CASES[0], SELECTOR_GROUPING_READING_CASES[1],
                                     SELECTOR_GROUPING_READING_CASES[2], SELECTOR_GROUPING_READING_CASES[3]),
                     testing::Bool()));

TEST_P(ArticleSelectorScopeReadingTest, repeatedScopeChangesRetainUnreadSelectionsAndRestoreTheCaller)
{
    const DoNewsgroupResult result = run_reading({"+", "a", "U", "U", "U", "U", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(8U, m_states.size());
    expect_scope(1, false, m_initial_read, {});
    expect_scope(2, false, m_initial_read, m_delta);
    expect_scope(3, true, m_initial_read, m_delta);
    expect_scope(4, false, m_initial_read, m_delta);
    expect_scope(5, true, m_initial_read, m_delta);
    expect_scope(6, false, m_initial_read, m_delta);
    expect_returned_context(7);
    EXPECT_EQ(m_initial_read, m_read[7]);
    expect_selected(7, m_delta);
    expect_bodies({4});
    EXPECT_EQ(" 1-4,7", g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorScopeReadingTest, existingSelectedOnlyReadingSurvivesScopeChanges)
{
    fs::current_path(m_output_dir);
    ASSERT_EQ(1, access_newsgroup());
    if (m_grouped)
    {
        g_cache.ptr(ArticleNum{5})->select_articles_subject(AUTO_KILL_NONE);
    }
    else
    {
        g_cache.ptr(ArticleNum{5})->select_article(AUTO_KILL_NONE);
    }
    g_in_ng = false;
    g_mode = MM_NEWSGROUP_LIST;

    const DoNewsgroupResult result = run_reading({"+", "U", "U", "+", "Q"}, "4");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5U, m_states.size());
    EXPECT_TRUE(m_states[0].selected_only);
    expect_scope(1, false, m_initial_read, m_delta);
    expect_scope(2, true, m_initial_read, m_delta);
    expect_scope(3, false, m_initial_read, m_delta);
    expect_returned_context(4);
    expect_selected(4, m_delta);
    expect_bodies({4});
    EXPECT_EQ(" 1-4,7", g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorScopeReadingTest, revivalsJoinExistingUnreadSelectionsAcrossRepeatedScopeChanges)
{
    std::vector<std::string> commands{"+", "a", "U", "b", "U", "U", "U", "Z"};
    append_reading(commands, m_batch);
    std::vector<int> bodies{4};
    bodies.insert(bodies.end(), m_batch.begin(), m_batch.end());

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_scope(3, true, m_initial_read, m_delta);
    expect_scope(4, true, m_initial_read, m_delta, m_alpha);
    expect_scope(5, false, m_revived_read, m_combined);
    expect_scope(6, true, m_revived_read, m_combined);
    expect_scope(7, false, m_revived_read, m_combined);
    EXPECT_EQ(ArticleNum{2}, m_states[8].article);
    EXPECT_TRUE(m_states[8].selected_only);
    EXPECT_EQ((m_grouped ? std::vector<int>{1, 2, 3, 4, 5, 7, 10} : std::vector<int>{1, 2, 3, 4, 5, 7}), m_read.back());
    expect_bodies(bodies);
    EXPECT_EQ(m_grouped ? " 1-5,7,10" : " 1-5,7", g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorScopeReadingTest, deselectingReadItemsBeforeSwitchingBackDoesNotReviveThem)
{
    std::vector<std::string> commands{"+", "a", "U", "b", "b", "U", "Z"};
    append_reading(commands, m_delta);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_scope(4, true, m_initial_read, m_delta, m_alpha);
    expect_scope(5, true, m_initial_read, m_delta);
    expect_scope(6, false, m_initial_read, m_delta);
    EXPECT_EQ(ArticleNum{5}, m_states[7].article);
    expect_bodies(m_grouped ? std::vector<int>{4, 5, 10} : std::vector<int>{4, 5});
    EXPECT_EQ(m_grouped ? " 1-5,7,10" : " 1-5,7", g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorScopeReadingTest, returningAfterARevivalRestoresTheCurrentArticleAndKeepsTheMarks)
{
    const DoNewsgroupResult result = run_reading({"+", "a", "U", "b", "U", "+", "^", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(8U, m_states.size());
    expect_scope(5, false, m_revived_read, m_combined);
    expect_returned_context(6);
    EXPECT_EQ(m_revived_read, m_read[6]);
    expect_selected(6, m_combined);
    EXPECT_EQ(ArticleNum{2}, m_states[7].article);
    expect_bodies({4, 2});
    EXPECT_EQ(m_grouped ? " 1-4" : " 1-4,7", g_newsgroup_ptr->rc_numbers());
}

TEST_P(ArticleSelectorScopeReadingTest, revivingTheCurrentArticlePreservesNormalNavigationAfterReturning)
{
    const bool             threaded = std::get<0>(GetParam()).mode == SM_THREAD;
    const std::vector<int> selected = m_grouped ? std::vector<int>{4, 5, 10} : std::vector<int>{4, 5};

    const DoNewsgroupResult result = run_reading({"+", "a", "U", ">", "b", "U", "+", "n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(9U, m_states.size());
    expect_scope(5, true, m_initial_read, m_delta, {4});
    expect_scope(6, false, {1, 2, 3, 7}, selected);
    expect_returned_context(7);
    EXPECT_EQ((std::vector<int>{1, 2, 3, 7}), m_read[7]);
    EXPECT_EQ(ArticleNum{threaded ? 9 : 5}, m_states[8].article);
    expect_bodies({4, threaded ? 9 : 5});
    EXPECT_EQ(threaded ? " 1-3,7,9" : " 1-3,5,7", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, ArticleSelectorScopeReadingTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool()));

TEST_P(PagedArticleSelectorAcceptTest, acceptsOffPageSelectionsAndReadsOnlyTheirArticlesInOrder)
{
    const auto &[input, erase_screen, accept] = GetParam();
    std::vector<std::string> commands = selection_commands();
    commands.emplace_back(1, accept);
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_page(1, input.first_page);
    expect_page(3, input.second_page);
    expect_page(5, input.first_page);
    expect_page(6, input.second_page);
    expect_selected(4, input.batch);
    expect_selected(5, input.batch);
    expect_selected(6, input.batch);
    expect_reading(input.batch, true);
    EXPECT_EQ(input.batch_range, g_newsgroup_ptr->rc_numbers());
}

TEST_P(PagedArticleSelectorAcceptTest, acceptanceWithoutSelectionsUsesTheDocumentedCurrentItemOrUnreadScope)
{
    const auto &[input, erase_screen, accept] = GetParam();
    const bool             current_item = accept == '\n';
    const std::vector<int> expected =
        current_item ? (input.mode == SM_THREAD ? std::vector<int>{4, 9} : std::vector<int>{4}) : input.all_unread;
    std::vector<std::string> commands{"+", ">", std::string(1, accept)};
    append_reading(commands, expected);
    const std::string_view range = !current_item ? " 1-11" : input.mode == SM_THREAD ? " 1-1,4,9" : " 1-1,4";

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_page(1, input.first_page);
    expect_page(2, input.second_page);
    EXPECT_TRUE(m_selected[2].empty());
    expect_reading(expected, current_item);
    EXPECT_EQ(range, g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeScreenAndAcceptance, PagedArticleSelectorAcceptTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool(),
                                          testing::Values('Z', '\t', '\n')));

TEST_P(PagedArticleSelectorDefaultTest, spaceAdvancesPagesAndThenAcceptsTheOffPageSelections)
{
    const auto &[input, erase_screen] = GetParam();
    std::vector<std::string> commands = selection_commands();
    commands.insert(commands.end(), input.pages - 1, " ");
    append_reading(commands, input.batch);

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_page(1, input.first_page);
    expect_page(3, input.second_page);
    expect_selected(4 + input.pages, input.batch);
    expect_reading(input.batch, true);
    EXPECT_EQ(input.batch_range, g_newsgroup_ptr->rc_numbers());
}

TEST_P(PagedArticleSelectorDefaultTest, exhaustingTheBatchRebuildsTheSelectorWithTheRemainingUnreadArticles)
{
    const auto &[input, erase_screen] = GetParam();
    std::vector<std::string> commands = selection_commands();
    commands.push_back("Z");
    append_reading(commands, input.batch, true);
    commands.back() = "^";
    commands.push_back("Q");

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(commands.size(), m_states.size());
    expect_reading(input.batch, true);
    expect_page(m_states.size() - 1, input.remaining_page);
    EXPECT_TRUE(m_selected.back().empty());
    EXPECT_EQ(ArticleUnread{10 - static_cast<long>(input.batch.size())}, m_states.back().unread);
    EXPECT_EQ(input.batch_range, g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, PagedArticleSelectorDefaultTest,
                         testing::Combine(testing::ValuesIn(PAGED_ARTICLE_SELECTOR_CASES), testing::Bool()));

TEST_P(NewsgroupSelectorShortcutReadingTest, theRequestedViewSelectsAnArticleAndReturnsToReading)
{
    const SelectorEntryReadingCase &entry = std::get<0>(GetParam());

    const DoNewsgroupResult result = run_reading({entry.command, "b", "Z", "Q"});

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(4U, m_states.size());
    EXPECT_EQ(ArticleNum{1}, m_states[0].article);
    expect_selector(1, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_FALSE(m_states[1].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[1].selector_mask);
    EXPECT_EQ(entry.threaded || std::string_view{entry.command} == "_t", m_states[1].selector_threaded);
    expect_selector(2, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[2].selected);
    EXPECT_EQ((std::array<bool, 3>{true, false, false}), m_states[2].read);
    EXPECT_EQ(MM_ARTICLE, m_states[3].mode);
    EXPECT_EQ(ArticleNum{3}, m_states[3].article);
    EXPECT_EQ(ArticleNum{3}, m_states[3].current);
    EXPECT_TRUE(m_states[3].selected_only);
    EXPECT_FALSE(m_states[3].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[3].selector_mask);
    EXPECT_EQ((std::array<bool, 3>{true, false, true}), m_states[3].read);
    EXPECT_EQ(ArticleUnread{1}, m_states[3].unread);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(
    EntryAndScreen, NewsgroupSelectorShortcutReadingTest,
    testing::Combine(
        testing::Values(SelectorEntryReadingCase{"ArticleOverridesSubject", "_a", SM_SUBJECT, SM_ARTICLE, false},
                        SelectorEntryReadingCase{"SubjectOverridesArticle", "_s", SM_ARTICLE, SM_SUBJECT, false},
                        SelectorEntryReadingCase{"ThreadBuildsAnUnthreadedGroup", "_t", SM_ARTICLE, SM_THREAD, false},
                        SelectorEntryReadingCase{"ThreadOverridesSubject", "_t", SM_SUBJECT, SM_THREAD, true},
                        SelectorEntryReadingCase{"ConditionalThreadFallsBack", "_T", SM_ARTICLE, SM_SUBJECT, false},
                        SelectorEntryReadingCase{"ConditionalThreadUsesThreads", "_T", SM_SUBJECT, SM_THREAD, true},
                        SelectorEntryReadingCase{"DefaultThreadFallsBack", "+", SM_THREAD, SM_SUBJECT, false}),
        testing::Bool()));

TEST_P(NewsgroupReadSelectorEntryTest, selectedReadItemsBecomeUnreadAndReturnToTheReader)
{
    const auto [mode, erase_screen, at_end] = GetParam();
    const bool                       grouped = mode != SM_ARTICLE;
    const std::optional<std::string> start = at_end ? std::optional<std::string>{"$"} : std::nullopt;

    const DoNewsgroupResult result = run_reading({"U+", "a", "Z", "Q"}, start);

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(4U, m_states.size());
    EXPECT_EQ(at_end ? MM_ARTICLE_END : MM_ARTICLE, m_states[0].mode);
    expect_selector(1, {ArticleNum{1}, ArticleNum{2}});
    EXPECT_TRUE(m_states[1].selector_rereading);
    EXPECT_EQ(AGF_DEL_SEL, m_states[1].selector_mask);
    EXPECT_EQ(ArticleUnread{1}, m_states[1].unread);
    expect_selector(2, {ArticleNum{1}, ArticleNum{2}});
    EXPECT_EQ(ArticleUnread{grouped ? 2 : 1}, m_states[2].selected);
    EXPECT_EQ((std::array<bool, 3>{true, true, false}), m_states[2].read);
    EXPECT_EQ(MM_ARTICLE, m_states[3].mode);
    EXPECT_EQ(ArticleNum{1}, m_states[3].article);
    EXPECT_EQ(ArticleNum{1}, m_states[3].current);
    EXPECT_TRUE(m_states[3].selected_only);
    EXPECT_FALSE(m_states[3].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[3].selector_mask);
    EXPECT_EQ((std::array<bool, 3>{true, !grouped, false}), m_states[3].read);
    EXPECT_EQ(ArticleUnread{grouped ? 2 : 1}, m_states[3].unread);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", at_end ? 0 : 1);
    expect_body_count("Branch body", 0);
    EXPECT_EQ(grouped ? " 1-1" : " 1-2", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeScreenAndEndPrompt, NewsgroupReadSelectorEntryTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Bool()));

TEST_P(NewsgroupSelectorReadingTest, acceptingTheReadSelectorWithoutSelectionsContinuesUnreadReading)
{
    set_read_range("1-2");

    const DoNewsgroupResult result = run_reading({"U+", "Z", "Q"}, "$");

    SCOPED_TRACE(m_output);
    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3U, m_states.size());
    expect_selector(1, {ArticleNum{1}, ArticleNum{2}});
    EXPECT_TRUE(m_states[1].selector_rereading);
    EXPECT_EQ(AGF_DEL_SEL, m_states[1].selector_mask);
    EXPECT_EQ(TR_NONE, m_states[1].selected);
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_FALSE(m_states[2].selected_only);
    EXPECT_FALSE(m_states[2].selector_rereading);
    EXPECT_EQ(AGF_SEL, m_states[2].selector_mask);
    EXPECT_EQ((std::array<bool, 3>{true, true, true}), m_states[2].read);
    EXPECT_EQ(TR_NONE, m_states[2].unread);
    expect_body_count("Root first line", 0);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, newSelectionsSurviveReturningToTheArticleAndReenteringTheSelector)
{
    set_read_range("");

    const DoNewsgroupResult result = run_reading({"+", "b", exit_command(), "+", "Y", "Z", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(7U, m_states.size());
    expect_selector(1, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[2].selected);
    expect_restored_context(0, 3);
    EXPECT_EQ(m_states[0].read, m_states[3].read);
    EXPECT_EQ(ArticleUnread{1}, m_states[3].selected);
    expect_selector(4, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[4].selected);
    EXPECT_EQ(m_states[4].selector_articles, m_states[5].selector_articles);
    EXPECT_THAT(m_output, testing::HasSubstr("No marked articles to yank back."));
    EXPECT_EQ(ArticleNum{3}, m_states[6].article);
    EXPECT_TRUE(m_states[6].selected_only);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, returningWithANewSelectionRestoresOrdinaryUnreadProgression)
{
    set_read_range("");

    const DoNewsgroupResult result = run_reading({"+", "b", exit_command(), "n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5U, m_states.size());
    expect_restored_context(0, 3);
    EXPECT_EQ(ArticleUnread{1}, m_states[3].selected);
    EXPECT_FALSE(m_states[3].selected_only);
    EXPECT_EQ(ArticleNum{2}, m_states[4].article);
    EXPECT_FALSE(m_states[4].selected_only);
    EXPECT_EQ((std::array<bool, 3>{true, true, false}), m_states[4].read);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 0);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, replacingAnExistingSelectionRetainsTheNewChoiceOnReentry)
{
    set_read_range("");
    select_reply();

    const DoNewsgroupResult result = run_reading({"+", "a", "b", exit_command(), "+", "Z", "Q"}, "1");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(7U, m_states.size());
    expect_selector(1, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[1].selected);
    EXPECT_EQ(TR_NONE, m_states[2].selected);
    EXPECT_EQ(ArticleUnread{1}, m_states[3].selected);
    expect_restored_context(0, 4);
    EXPECT_TRUE(m_states[4].selected_only);
    EXPECT_EQ(m_states[0].read, m_states[4].read);
    expect_selector(5, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[5].selected);
    EXPECT_EQ(ArticleNum{3}, m_states[6].article);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, removingTheLastSelectionAllowsTheAutomaticSelectorToResumeUnreadReading)
{
    select_reply();
    g_cache.ptr(ArticleNum{2})->deselect_article(AUTO_KILL_NONE);
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);

    const DoNewsgroupResult result = run_reading({"+", "a", exit_command(), "n", "Z", "Q"}, "2");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(6U, m_states.size());
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[1].selected);
    EXPECT_EQ(TR_NONE, m_states[2].selected);
    expect_restored_context(0, 3);
    EXPECT_EQ(TR_NONE, m_states[3].selected);
    expect_selector(4, {ArticleNum{3}});
    EXPECT_EQ(TR_NONE, m_states[4].selected);
    EXPECT_EQ(ArticleNum{3}, m_states[5].article);
    EXPECT_FALSE(m_states[5].selected_only);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, returningFromTheReadSelectorKeepsOrdinaryUnreadArticlesReachable)
{
    const DoNewsgroupResult result = run_reading({"U+", exit_command(), "n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(4U, m_states.size());
    expect_selector(1, {ArticleNum{1}, ArticleNum{2}});
    EXPECT_TRUE(m_states[1].selector_rereading);
    expect_restored_context(0, 2);
    EXPECT_EQ(m_states[0].read, m_states[2].read);
    EXPECT_EQ(ArticleNum{3}, m_states[3].article);
    EXPECT_EQ((std::array<bool, 3>{true, true, true}), m_states[3].read);
    expect_body_count("Root first line", 0);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, returningWithRevivedItemsKeepsTheCurrentArticleAndOtherUnreadArticles)
{
    const bool grouped = std::get<0>(GetParam()) != SM_ARTICLE;

    const DoNewsgroupResult result = run_reading({"U+", "a", exit_command(), "n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5U, m_states.size());
    expect_selector(1, {ArticleNum{1}, ArticleNum{2}});
    expect_restored_context(0, 3);
    EXPECT_EQ((std::array<bool, 3>{false, !grouped, false}), m_states[3].read);
    EXPECT_EQ(ArticleUnread{grouped ? 3 : 2}, m_states[3].unread);
    EXPECT_FALSE(m_states[3].selected_only);
    EXPECT_EQ(ArticleNum{3}, m_states[4].article);
    EXPECT_EQ((std::array<bool, 3>{false, !grouped, true}), m_states[4].read);
    expect_body_count("Root first line", 0);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(grouped ? " 1-0,3" : " 1-0,2-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorCancelReadingTest, revivedItemsRemainReachableThroughFirstUnreadAfterReturning)
{
    const bool grouped = std::get<0>(GetParam()) != SM_ARTICLE;

    const DoNewsgroupResult result = run_reading({"U+", "a", exit_command(), "^", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5U, m_states.size());
    expect_restored_context(0, 3);
    EXPECT_EQ(ArticleNum{1}, m_states[4].article);
    EXPECT_EQ((std::array<bool, 3>{true, !grouped, false}), m_states[4].read);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 0);
    EXPECT_EQ(grouped ? " 1-1" : " 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_F(NewsgroupSelectorReadingStateTest, returningFromTheReadSelectorPreservesReverseArticleReadingOrder)
{
    configure_selector(SM_ARTICLE, false);
    g_sel.set_selector(SM_ARTICLE, static_cast<SelectionSortMode>(-SS_NATURAL));
    set_read_range("");

    const DoNewsgroupResult result = run_reading({"+", "Z", "U+", "+", "n", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(6U, m_states.size());
    expect_selector(1, {ArticleNum{3}, ArticleNum{2}});
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    expect_selector(3, {ArticleNum{3}, ArticleNum{1}});
    EXPECT_EQ(ArticleNum{3}, m_states[4].article);
    EXPECT_EQ(ArticleNum{2}, m_states[5].article);
    expect_body_count("Root first line", 1);
    expect_body_count("Branch body", 1);
    expect_body_count("Reply body", 1);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ModeScreenAndReturnCommand, NewsgroupSelectorCancelReadingTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values('+', '\033')));

TEST_P(NewsgroupSelectorReadingExitTest, leavingAfterASelectionChangeSavesOnlyTheArticleAlreadyRead)
{
    const auto &[mode, erase_screen, exit] = GetParam();
    const auto [command, expected, group_result, index] = exit;
    set_read_range("");

    const InputNewsgroupResult result = run_from_group_prompt({"+", "b", std::string{command}});

    EXPECT_EQ(expected, result);
    EXPECT_EQ(group_result, g_newsgroup_exit_code);
    ASSERT_EQ(3U, m_states.size());
    EXPECT_EQ(ArticleNum{1}, m_states[0].article);
    expect_selector(1, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[2].selected);
    EXPECT_EQ(&g_newsgroup_data[index], g_newsgroup_ptr);
    EXPECT_EQ(" 1-1", g_newsgroup_data[1].rc_numbers());
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(TR_NONE, g_selected_count);
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 0);
}

TEST_P(NewsgroupSelectorReadingTest, cancellingExplicitSelectionRestoresTheCurrentArticlePrompt)
{
    const DoNewsgroupResult result = run_reading({"+", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[0].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(ArticleNum{2}, m_states[1].current);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[2].article);
    EXPECT_EQ(m_states[0].recent, m_states[2].recent);
    EXPECT_EQ(m_states[0].read, m_states[2].read);
    EXPECT_EQ(ArticleUnread{1}, m_states[2].unread);
    EXPECT_FALSE(m_states[2].selected_only);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 0);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorReadingTest, cancellationRetainsAnExistingSelectionAndSelectedOnlyReading)
{
    select_reply();
    g_cache.ptr(ArticleNum{2})->deselect_article(AUTO_KILL_NONE);
    g_cache.ptr(ArticleNum{3})->select_article(AUTO_KILL_NONE);

    const DoNewsgroupResult result = run_reading({"+", "+", "Q"}, "2");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3, m_states.size());
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{1}, m_states[1].selected);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[2].article);
    EXPECT_TRUE(m_states[2].selected_only);
    EXPECT_EQ(ArticleUnread{1}, m_states[2].selected);
    EXPECT_EQ(m_states[0].read, m_states[2].read);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 0);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorReadingTest, acceptingWithoutSelectionsContinuesWithTheRemainingUnreadArticle)
{
    const DoNewsgroupResult result = run_reading({"+", "Z", "n"});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(3, m_states.size());
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(TR_NONE, m_states[1].selected);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_EQ(TR_NONE, m_states[2].unread);
    EXPECT_FALSE(m_states[2].selected_only);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorReadingTest, acceptingTheChosenItemSkipsTheOtherUnreadArticle)
{
    set_read_range("");

    const DoNewsgroupResult result = run_reading({"+", "bZ", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3, m_states.size());
    EXPECT_EQ(ArticleNum{1}, m_states[0].article);
    expect_selector(1, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(ArticleUnread{2}, m_states[1].unread);
    EXPECT_EQ(MM_ARTICLE, m_states[2].mode);
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_TRUE(m_states[2].selected_only);
    EXPECT_EQ(ArticleUnread{1}, m_states[2].unread);
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    expect_body_count("Root first line", 1);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 1);
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorReadingTest, cancellingAtTheGroupEndRestoresTheEndPromptAndUnreadCount)
{
    const std::string_view default_command = std::get<0>(GetParam()) == SM_SUBJECT ? "^Nnpq" : "npq";

    const DoNewsgroupResult result = run_reading({"+", "+", "Q"}, "$");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(3, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[0].mode);
    expect_selector(1, {ArticleNum{2}, ArticleNum{3}});
    EXPECT_EQ(MM_ARTICLE_END, m_states[2].mode);
    EXPECT_EQ(ArticleNum{4}, m_states[2].article);
    EXPECT_EQ(m_states[0].read, m_states[2].read);
    EXPECT_EQ(ArticleUnread{2}, m_states[2].unread);
    EXPECT_EQ(default_command, m_states[2].default_command);
    expect_body_count("Reply body", 0);
    expect_body_count("Branch body", 0);
    EXPECT_EQ(" 1-1", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupSelectorReadingExitTest, leavingTheSelectorAfterReadingReachesTheRequestedGroupDestination)
{
    const auto &[mode, erase_screen, exit] = GetParam();
    const auto [command, expected, group_result, index] = exit;

    const InputNewsgroupResult result = run_from_group_prompt({"+", std::string{command}});

    EXPECT_EQ(expected, result);
    EXPECT_EQ(group_result, g_newsgroup_exit_code);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(MM_ARTICLE, m_states[0].mode);
    EXPECT_EQ(ArticleNum{2}, m_states[0].article);
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(ArticleNum{2}, m_states[1].current);
    EXPECT_EQ(&g_newsgroup_data[index], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_current_newsgroup);
    EXPECT_EQ(" 1-2", g_newsgroup_data[1].rc_numbers());
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[1].m_to_read);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 0);
}

INSTANTIATE_TEST_SUITE_P(ModeAndScreen, NewsgroupSelectorReadingTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ModeScreenAndDestination, NewsgroupSelectorReadingExitTest,
                         testing::Combine(testing::Values(SM_ARTICLE, SM_SUBJECT, SM_THREAD), testing::Bool(),
                                          testing::Values(SelectorReadingExit{'q', ING_NORM, NG_NEXT, 2},
                                                          SelectorReadingExit{'Q', ING_ASK, NG_ASK, 1},
                                                          SelectorReadingExit{'N', ING_NORM, NG_SEL_NEXT, 2},
                                                          SelectorReadingExit{'P', ING_NORM, NG_SEL_PRIOR, 0})));

TEST_P(LocalNewsgroupArrivalTest, reachingTheEndDiscoversAndReadsTheNewLocalArticles)
{
    const std::vector<std::string> commands(2 + m_arrival_count, "n");

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(2 + m_arrival_count, m_states.size());
    for (std::size_t index = 0; index < m_states.size(); ++index)
    {
        SCOPED_TRACE(index);
        EXPECT_EQ(MM_ARTICLE, m_states[index].mode);
        EXPECT_EQ(ArticleNum{2 + static_cast<long>(index)}, m_states[index].article);
        EXPECT_EQ(index < 2 ? ArticleNum{3} : ArticleNum{3 + m_arrival_count}, m_arrival_states[index].last);
        EXPECT_EQ(index < 2 ? ArticleUnread{1 - static_cast<int>(index)}
                            : ArticleUnread{m_arrival_count + 1 - static_cast<int>(index)},
                  m_states[index].unread);
    }
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 1);
    expect_arrival_bodies(1);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-" + std::to_string(3 + m_arrival_count), g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(ArticleNum{3 + m_arrival_count}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
}

TEST_P(LocalNewsgroupArrivalTest, aForcedEndJumpRetainsNewArrivalsAsUnreadAtTheEndPrompt)
{
    const DoNewsgroupResult result = run_reading({"$", "Q"}, "3");

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(ArticleNum{3}, m_states[0].article);
    EXPECT_EQ(ArticleNum{3}, m_arrival_states[0].last);
    EXPECT_EQ(MM_ARTICLE_END, m_states[1].mode);
    EXPECT_EQ(ArticleNum{4 + m_arrival_count}, m_states[1].article);
    EXPECT_EQ(ArticleNum{3 + m_arrival_count}, m_arrival_states[1].last);
    EXPECT_EQ(ArticleUnread{1 + m_arrival_count}, m_states[1].unread);
    EXPECT_EQ(ArticleNum{3}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(g_nav.recent.num), g_nav.recent.art);
    expect_arrival_bodies(0);
    for (int number = 4; number < 4 + m_arrival_count; ++number)
    {
        ASSERT_NE(nullptr, g_cache.find(ArticleNum{number}));
        EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_cache.find(ArticleNum{number})->m_flags & (AF_EXISTS | AF_UNREAD));
    }
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalNewsgroupArrivalTest, aRequestedGrowthRefreshKeepsTheCurrentArticleAndReadState)
{
    m_force_refresh = true;

    const DoNewsgroupResult result = run_reading({"\022", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(ArticleNum{3}, m_arrival_states[0].last);
    EXPECT_EQ(ArticleNum{3 + m_arrival_count}, m_arrival_states[1].last);
    EXPECT_EQ(ArticleNum{2}, m_states[1].article);
    EXPECT_EQ(m_states[0].current, m_states[1].current);
    EXPECT_EQ(m_states[0].recent, m_states[1].recent);
    EXPECT_EQ(m_states[0].read, m_states[1].read);
    EXPECT_EQ(ArticleUnread{1 + m_arrival_count}, m_states[1].unread);
    EXPECT_FALSE(g_force_grow);
    expect_body_count("Reply body", 2);
    expect_body_count("Branch body", 0);
    expect_arrival_bodies(0);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalNewsgroupArrivalTest, disablingAutomaticGrowthDefersNewArticlesUntilAnotherVisit)
{
    g_keep_the_group_static = 1;

    const DoNewsgroupResult result = run_reading({"n", "n"});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(2, m_states.size());
    EXPECT_EQ(ArticleNum{3}, g_last_art);
    EXPECT_EQ(ArticleNum{3}, g_newsgroup_ptr->m_ng_max);
    EXPECT_EQ(1, m_arrival_states[1].static_policy);
    EXPECT_EQ(nullptr, g_cache.find(ArticleNum{4}));
    EXPECT_TRUE(fs::exists(m_output_dir / "comp/lang/cpp/4"));
    expect_arrival_bodies(0);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalNewsgroupArrivalTest, overridingTheSelectorItemTemporarilyDefersNewArticles)
{
    const DoNewsgroupResult result = run_reading({"+", "#", "n"});

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(3, m_states.size());
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(ArticleNum{3}, m_states[2].article);
    EXPECT_EQ(2, m_arrival_states[2].static_policy);
    EXPECT_EQ(ArticleNum{3}, g_last_art);
    EXPECT_EQ(nullptr, g_cache.find(ArticleNum{4}));
    expect_arrival_bodies(0);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalNewsgroupArrivalTest, returningToTheSelectorReleasesTemporaryStaticPolicy)
{
    std::vector<std::string> commands{"+", "#", "+", "+", "n", "Z"};
    commands.insert(commands.end(), m_arrival_count, "n");
    std::vector<ArticleNum> arrivals;
    for (int number = 4; number < 4 + m_arrival_count; ++number)
    {
        arrivals.emplace_back(number);
    }

    const DoNewsgroupResult result = run_reading(commands);

    EXPECT_EQ(NG_NORM, result);
    ASSERT_EQ(6 + m_arrival_count, m_states.size());
    expect_selector(1, {ArticleNum{3}});
    EXPECT_EQ(2, m_arrival_states[2].static_policy);
    expect_selector(3, {});
    EXPECT_EQ(0, m_arrival_states[3].static_policy);
    EXPECT_EQ(ArticleNum{3}, m_states[4].article);
    expect_selector(5, arrivals);
    EXPECT_EQ(ArticleNum{4}, m_states[6].article);
    EXPECT_EQ(ArticleNum{3 + m_arrival_count}, m_arrival_states[6].last);
    EXPECT_EQ(0, g_keep_the_group_static);
    expect_body_count("Reply body", 1);
    expect_body_count("Branch body", 1);
    expect_arrival_bodies(1);
    EXPECT_EQ(" 1-" + std::to_string(3 + m_arrival_count), g_newsgroup_ptr->rc_numbers());
}

TEST_P(LocalNewsgroupArrivalTest, theSelectorReportsArrivalsOnceAcrossRepeatedEntries)
{
    std::vector<ArticleNum> unread{ArticleNum{3}};
    for (int number = 4; number < 4 + m_arrival_count; ++number)
    {
        unread.emplace_back(number);
    }
    const std::string message =
        std::to_string(m_arrival_count) + (m_arrival_count == 1 ? " new article arrived" : " new articles arrived");

    const DoNewsgroupResult result = run_reading({"$", "+", "+", "+", "Q"});

    EXPECT_EQ(NG_ASK, result);
    ASSERT_EQ(5, m_states.size());
    EXPECT_EQ(MM_ARTICLE_END, m_states[1].mode);
    EXPECT_EQ(m_arrival_count, m_arrival_states[1].added);
    expect_selector(2, unread);
    expect_selector(4, unread);
    EXPECT_EQ(0, m_arrival_states[2].added);
    EXPECT_EQ(0, m_arrival_states[4].added);
    EXPECT_EQ(0, g_added_articles);
    EXPECT_EQ(ArticleUnread{1 + m_arrival_count}, g_newsgroup_ptr->m_to_read);
    expect_body_count(message, 1);
    expect_arrival_bodies(0);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ThreadingVerbosityAndArrivals, LocalNewsgroupArrivalTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Values(1, 2)));

TEST_P(NewsgroupPagerReadingTest, completingTheSecondPageMarksTheArticleReadBeforeTheArticlePrompt)
{
    const DoNewsgroupResult result = run_reading(" Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{2}, true, ArticleUnread{1})));
    EXPECT_THAT(m_output, testing::HasSubstr("--MORE--("));
    EXPECT_THAT(m_output, testing::HasSubstr("Paged body 12\n"));
    std::size_t next_position{};
    for (int line = 1; line <= 12; ++line)
    {
        const std::string text = "Paged body " + std::to_string(line) + '\n';
        const std::size_t position = m_output.find(text, next_position);
        ASSERT_NE(std::string::npos, position) << line;
        next_position = position + text.size();
    }
    EXPECT_THAT(m_output, testing::HasSubstr("End of article 2"));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, displayingTheLastPageCompletesReadingAndMarksTheArticleRead)
{
    const DoNewsgroupResult result = run_reading("\005Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{2}, true, ArticleUnread{1})));
    EXPECT_THAT(m_output, testing::HasSubstr("Paged body 12\n"));
    EXPECT_THAT(m_output, testing::HasSubstr("End of article 2"));
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, quittingThePagerLeavesThePartialArticleUnreadAtTheArticlePrompt)
{
    const DoNewsgroupResult result = run_reading("qQ");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{2}, false, ArticleUnread{2})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged body 12\n")));
    EXPECT_THAT(m_output, testing::HasSubstr("End of article 2"));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_EQ(" 1-1", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, advancingAfterQuittingThePagerPreservesThePartialArticlesUnreadMark)
{
    const DoNewsgroupResult result = run_reading("qnQ");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{3}, true, ArticleUnread{1})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged body 12\n")));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body\n"));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, uppercaseQuitLeavesTheGroupDirectlyWithoutMarkingThePartialArticleRead)
{
    const DoNewsgroupResult result = run_reading("Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("End of article 2")));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_EQ(" 1-1", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, nextFromThePagerMarksThePartialArticleReadAndDisplaysTheNextArticle)
{
    const DoNewsgroupResult result = run_reading("nQ");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{3}, true, ArticleUnread{0})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged body 12\n")));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body\n"));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, junkFromThePagerMarksTheArticleReadAndReturnsToItsArticlePrompt)
{
    const DoNewsgroupResult result = run_reading("jQ");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{2}, true, ArticleUnread{1})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged body 12\n")));
    EXPECT_THAT(m_output, testing::HasSubstr("End of article 2"));
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, numericPagerCommandRetainsItsArgumentAndLeavesThePartialArticleUnread)
{
    const DoNewsgroupResult result = run_reading("3\nQ");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{2}, false, ArticleUnread{2}),
                                               testing::FieldsAre(MM_ARTICLE, ArticleNum{3}, true, ArticleUnread{1})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Paged body 12\n")));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body\n"));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{2}));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(" 1-1,3", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(ScrollingAndScreenClearing, NewsgroupPagerReadingTest, testing::Bool());

TEST_P(NewsgroupPagerReadingTest, anArticleExpiringDuringReadingIsSkippedWithoutAnExtraPrompt)
{
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        EXPECT_TRUE(fs::remove(m_output_dir / "comp/lang/cpp/3"));
        return reader(buffer, size);
    };

    const DoNewsgroupResult result = run_reading("n");

    EXPECT_EQ(NG_NORM, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{2})));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Branch body")));
    EXPECT_EQ(ArticleUnread{0}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_missing_count);
    EXPECT_EQ(ArticleNum{4}, g_nav.target.num);
    EXPECT_EQ(nullptr, g_nav.target.art);
    EXPECT_EQ(ArticleNum{2}, g_nav.recent.num);
    EXPECT_EQ(g_cache.find(ArticleNum{2}), g_nav.recent.art);
    EXPECT_FALSE(g_cache.find(ArticleNum{3})->m_flags & AF_EXISTS);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(NewsgroupPagerReadingTest, localGrowthRetainsAnAvailableNewArticle)
{
    const TerminalInputReader reader = g_terminal_input_reader;
    g_terminal_input_reader = [this, reader](char *buffer, int size)
    {
        EXPECT_TRUE(fs::copy_file(m_output_dir / "comp/lang/cpp/3", m_output_dir / "comp/lang/cpp/4"));
        grow_newsgroup(ArticleNum{4});
        errno = 0;
        return reader(buffer, size);
    };

    const DoNewsgroupResult result = run_reading("Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_THAT(m_states, testing::ElementsAre(testing::FieldsAre(MM_PAGER, ArticleNum{2}, false, ArticleUnread{3})));
    EXPECT_EQ(ArticleNum{4}, g_last_art);
    EXPECT_EQ(AF_EXISTS | AF_UNREAD, g_cache.find(ArticleNum{4})->m_flags & (AF_EXISTS | AF_UNREAD));
    EXPECT_EQ(ArticleUnread{3}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-1", g_newsgroup_ptr->rc_numbers());
}

TEST_F(LocalArticleFailureReadingTest, aHoleInTheLocalSpoolSkipsToTheNextReadableArticle)
{
    ASSERT_TRUE(fs::remove(m_output_dir / "comp/lang/cpp/2"));

    const DoNewsgroupResult result = run_reading("Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    EXPECT_EQ(ArticleNum{3}, g_nav.current.num);
    EXPECT_EQ(g_cache.find(ArticleNum{3}), g_nav.current.art);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_F(LocalArticleFailureReadingTest, anUnavailableExplicitArticleReportsTheFailureAndRetainsItsNumber)
{
    ASSERT_TRUE(fs::remove(m_output_dir / "comp/lang/cpp/2"));
    g_terminal_input_reader = [](char *buffer, int)
    {
        *buffer = 'Q';
        return 1;
    };

    const DoNewsgroupResult result = run_reading("", "2");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::HasSubstr("article is not available"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ(ArticleNum{2}, g_nav.current.num);
    EXPECT_EQ(g_cache.find(ArticleNum{2}), g_nav.current.art);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

TEST_F(LocalArticleFailureReadingTest, aSpoolWithNoRemainingFilesReachesTheGroupEndWithoutNegativeCounts)
{
    ASSERT_TRUE(fs::remove(m_output_dir / "comp/lang/cpp/2"));
    ASSERT_TRUE(fs::remove(m_output_dir / "comp/lang/cpp/3"));

    const DoNewsgroupResult result = run_reading("Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::HasSubstr("End of comp.lang.cpp"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Branch body")));
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(article_after(g_last_art), g_nav.current.num);
    EXPECT_EQ(nullptr, g_nav.current.art);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}

TEST_P(HeaderlessLocalArticleTest, anArticleWithoutASubjectCanBeReadWithoutLosingTheFollowingArticle)
{
    const auto &[contents, body] = GetParam();
    std::ofstream{m_output_dir / "comp/lang/cpp/2", std::ios::binary} << contents;

    const DoNewsgroupResult result = run_reading("Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::HasSubstr(body));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Reply body")));
    EXPECT_EQ("<NONE>", g_cache.find(ArticleNum{2})->get_cached_line_view(SUBJ_LINE, false));
    EXPECT_TRUE(g_cache.was_read(ArticleNum{2}));
    EXPECT_FALSE(g_cache.was_read(ArticleNum{3}));
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
}

INSTANTIATE_TEST_SUITE_P(
    EmptyFileEmptyHeaderAndMissingSubject, HeaderlessLocalArticleTest,
    testing::Values(HeaderlessLocalArticle{"", ""}, HeaderlessLocalArticle{"\nHeaderless body\n", "Headerless body"},
                    HeaderlessLocalArticle{"From: Reader <reader@example.test>\n\nNo subject body\n",
                                           "No subject body"}));

TEST_P(LocalArticleAccessFailureTest, aMissingGroupDirectoryReportsTheFailureAndPreservesReadRanges)
{
    g_verbose = GetParam();
    fs::rename(m_output_dir / "comp/lang/cpp", m_output_dir / "comp/lang/saved-cpp");

    const DoNewsgroupResult result = run_reading("");

    EXPECT_EQ(NG_ERROR, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "does not have a spool directory" : "No spool for"));
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1", g_newsgroup_ptr->rc_numbers());
}

TEST_F(LocalArticleFailureReadingTest, aRegularFileAtTheGroupDirectoryPathReturnsAnAccessError)
{
    const fs::path group = m_output_dir / "comp/lang/cpp";
    fs::rename(group, m_output_dir / "comp/lang/saved-cpp");
    std::ofstream{group} << "not a directory\n";
    fs::permissions(group, fs::perms::all);

    const DoNewsgroupResult result = run_reading("");

    EXPECT_EQ(NG_ERROR, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::HasSubstr("Can't chdir to directory comp/lang/cpp"));
    EXPECT_EQ(" 1", g_newsgroup_ptr->rc_numbers());
}

#ifdef _WIN32
TEST_P(LocalArticleAccessFailureTest, anUnreadableArticleReportsItsNumberAndContinuesToTheNextArticle)
{
    g_verbose = GetParam();
    const fs::path article = m_output_dir / "comp/lang/cpp/2";
    m_locked_file = _fsopen(article.string().c_str(), "rb", _SH_DENYRW);
    ASSERT_NE(nullptr, m_locked_file);

    const DoNewsgroupResult result = run_reading("Q");

    EXPECT_EQ(NG_ASK, result);
    EXPECT_FALSE(macro_pending());
    EXPECT_THAT(m_output, testing::HasSubstr(g_verbose ? "Article 2 exists but is unreadable" : "2 unreadable"));
    EXPECT_THAT(m_output, testing::HasSubstr("Branch body"));
    EXPECT_EQ(ArticleNum{3}, g_nav.current.num);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
}
#endif

INSTANTIATE_TEST_SUITE_P(TerseAndVerbose, LocalArticleAccessFailureTest, testing::Bool());

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
    EXPECT_FALSE(g_sel.rereading);
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

TEST_P(NewsgroupSelectorExitRequestTest, exitRequestsRetainSelectionsAndPendingChangesWithoutConsumingInput)
{
    const auto [command, rereading, at_end] = GetParam();
    prepare_exit(rereading, true, at_end);
    g_newsgroup_data[0].m_flags |= NF_DEL;
    g_newsgroup_data[2].m_flags |= NF_UNTHREADED;
    remember_pending_changes();
    NewsgroupData *const cursor = g_newsgroup_ptr;

    const DisplayState result = run_delegated(command, "Q");

    EXPECT_EQ(DS_QUIT, result);
    EXPECT_EQ(cursor, g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[0], g_recent_newsgroup);
    EXPECT_EQ(&m_multirc, g_multirc);
    EXPECT_EQ(&m_data_source, g_data_source);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_page(true, 1);
    expect_selector_context(rereading);
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(CommandReadScopeAndEndCursor, NewsgroupSelectorExitRequestTest,
                         testing::Combine(testing::Values(Ctl('n'), Ctl('p'), ';'), testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorSourceExitTest, sourceExitReturnsTheRequestedDirectionWithoutSwitchingSources)
{
    const auto [rereading, command] = GetParam();
    prepare_selector_session(rereading);
    const std::string input = std::string{rereading ? "U." : ""} + command + "Q";

    const char result = run_selector_session(input);

    EXPECT_EQ(command, result);
    EXPECT_EQ(&m_multirc, g_multirc);
    EXPECT_EQ(&m_data_source, g_data_source);
    EXPECT_EQ(&m_data_source, m_newsrc.data_source);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_selector_session_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndDirection, NewsgroupSelectorSourceExitTest,
                         testing::Combine(testing::Bool(), testing::Values(Ctl('n'), Ctl('p'))));

TEST_F(NewsgroupSelectorScanEntryTest, semicolonEntersTheSelectedGroupInScanModeAndReturnsToTheSelector)
{
    prepare_selector_session(false);

    const char result = run_selector_session(";qqQ");

    EXPECT_EQ('q', result);
    EXPECT_FALSE(g_sa_never_initialized);
    EXPECT_FALSE(g_sa_initialized);
    EXPECT_FALSE(g_in_ng);
    EXPECT_THAT(m_output, testing::HasSubstr("Entering comp.lang.cpp:"));
    EXPECT_THAT(m_output, testing::HasSubstr("Topic"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Root first line")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Entering comp.previous:")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Entering comp.next:")));
    for (int index = 0; index < 3; ++index)
    {
        const NewsgroupData &group = g_newsgroup_data[index];
        EXPECT_EQ(index == 1 ? " 1-1" : " 1-2", group.rc_numbers()) << index;
        EXPECT_EQ(ArticleUnread{index == 1 ? 2 : 1}, group.m_to_read) << index;
        EXPECT_EQ(':', group.m_subscribe_char) << index;
        EXPECT_EQ(NF_NONE, group.m_flags & (NF_SEL | NF_DEL_SEL | NF_DEL)) << index;
    }
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    expect_exit_files_unchanged();
    expect_selector_context();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorOnlineHelpTest, onlineHelpOpensTheNewsgroupSelectorDestinationAndRequestsARestart)
{
    const auto [rereading, command, leave] = GetParam();
    prepare_help(rereading);

    const DisplayState result = run_delegated(command, std::string{leave} + "Q");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_THAT(m_output, testing::HasSubstr("Extended Help"));
    EXPECT_THAT(m_output, testing::HasSubstr("Newsgroup Selector"));
    EXPECT_THAT(m_output, testing::HasSubstr("Local help topic"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Article Display/Selection Mode")));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Newsgroup Browse Mode")));
    expect_help_preserved(rereading, true);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeCommandAndExit, NewsgroupSelectorOnlineHelpTest,
                         testing::Combine(testing::Bool(), testing::Values('h', '?'), testing::Values('q', 'Q')));

TEST_P(NewsgroupSelectorFinishedHelpTest, completedKeyHelpRedisplaysAfterTheContinuationAnswer)
{
    const auto [rereading, answer] = GetParam();
    prepare_help(rereading, true);

    const DisplayState result = run_delegated('H', std::string{answer} + "Q");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_THAT(m_output, testing::HasSubstr("The Newsgroup Selector:"));
    EXPECT_THAT(m_output, testing::HasSubstr("Refresh the article counts in the selector"));
    EXPECT_THAT(m_output, testing::HasSubstr("Quit the selector and return to the rn-style group selector."));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Extended Help")));
    expect_help_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndAnswer, NewsgroupSelectorFinishedHelpTest,
                         testing::Combine(testing::Bool(), testing::Values(' ', '\n', 'q')));

TEST_P(NewsgroupSelectorHelpScopeTest, quittingPagedKeyHelpReturnsWithoutConsumingTheNextSelectorCommand)
{
    const bool rereading = GetParam();
    prepare_help(rereading);

    const DisplayState result = run_delegated('H', "qQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_THAT(m_output, testing::HasSubstr("The Newsgroup Selector:"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Refresh the article counts in the selector")));
    expect_help_preserved(rereading);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorHelpScopeTest, continuingPagedKeyHelpDisplaysTheRemainingCommandsBeforeRedisplay)
{
    const bool rereading = GetParam();
    prepare_help(rereading);
    g_tc_LINES = 24;

    const DisplayState result = run_delegated('H', "  Q");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_THAT(m_output, testing::HasSubstr("The Newsgroup Selector:"));
    EXPECT_THAT(m_output, testing::HasSubstr("Quit the selector and return to the rn-style group selector."));
    expect_help_preserved(rereading);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorHelpCommandTest, keyHelpReturnsASelectorCommandWithoutDiscardingQueuedInput)
{
    const auto [rereading, paged, command] = GetParam();
    prepare_help(rereading, !paged);

    const DisplayState result = run_delegated('H', std::string{command} + "Q");

    EXPECT_EQ(command == 'R' ? DS_DO_COMMAND : DS_DISPLAY, result);
    EXPECT_THAT(m_output, testing::HasSubstr("The Newsgroup Selector:"));
    expect_help_preserved(rereading);
    if (command != 'R')
    {
        ASSERT_TRUE(macro_pending());
        EXPECT_EQ(command, read_tty_char() & 0177);
    }
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopePaginationAndCommand, NewsgroupSelectorHelpCommandTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Values('R', 'a')));

TEST_P(NewsgroupSelectorHelpSessionTest, closingOnlineHelpRebuildsTheOriginalReadingScope)
{
    const auto [rereading, command] = GetParam();
    prepare_selector_session(rereading);
    const std::string input = std::string{rereading ? "U." : ""} + command + "qqQ";

    const char result = run_selector_session(input);

    EXPECT_EQ('q', result);
    const std::size_t help = m_output.find("Extended Help");
    ASSERT_NE(std::string::npos, help);
    EXPECT_NE(std::string::npos, m_output.find("comp.lang.cpp", help));
    EXPECT_THAT(m_output, testing::HasSubstr("Newsgroup Selector"));
    expect_selector_session_preserved(rereading);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorHelpSessionTest, theOnlineHelpContextItemDisplaysNewsgroupSelectorKeyHelp)
{
    const auto [rereading, command] = GetParam();
    prepare_selector_session(rereading);
    const std::string input = std::string{rereading ? "U." : ""} + command + "\rqqqQ";

    const char result = run_selector_session(input);

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("Extended Help"));
    EXPECT_THAT(m_output, testing::HasSubstr("The Newsgroup Selector:"));
    expect_selector_session_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCommand, NewsgroupSelectorHelpSessionTest,
                         testing::Combine(testing::Bool(), testing::Values('h', '?')));

TEST_P(NewsgroupSelectorHelpScopeTest, theSelectorDispatchesTheCommandReturnedFromKeyHelp)
{
    const bool rereading = GetParam();
    prepare_selector_session(rereading);
    g_tc_LINES = 200;

    const char result = run_selector_session(rereading ? "U.HRqQ" : "HRqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("The Newsgroup Selector:"));
    expect_selector_session_preserved(rereading, -1);
    ASSERT_EQ(3, g_ng_order.order.size());
    EXPECT_EQ(&g_newsgroup_data[0], g_ng_order.first());
    EXPECT_EQ(&g_newsgroup_data[1], g_ng_order.last());
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorHelpScopeTest, testing::Bool());

TEST_P(NewsgroupSelectorDisplayModeTest, displayCommandAdvancesTheConfiguredCycleWithoutChangingReadingState)
{
    const auto [rereading, choice] = GetParam();
    prepare_display(rereading, choice.modes, choice.before);
    load_descriptions();

    const DisplayState result = run_delegated('L', "Q");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(choice.modes, g_sel_grp_display_mode);
    EXPECT_EQ(choice.after, g_sel_grp_display_mode_index);
    EXPECT_EQ(choice.modes[choice.after], current_group_display_mode());
    EXPECT_EQ("(" + std::string{choice.style} + " display style)", g_msg);
    expect_display_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(
    ReadScopeAndConfiguredCycle, NewsgroupSelectorDisplayModeTest,
    testing::Combine(testing::Bool(),
                     testing::Values(GroupDisplayChoice{"slm", 0, 1, "long"}, GroupDisplayChoice{"slm", 1, 2, "medium"},
                                     GroupDisplayChoice{"slm", 2, 0, "short"}, GroupDisplayChoice{"ms", 0, 1, "short"},
                                     GroupDisplayChoice{"ms", 1, 0, "medium"}, GroupDisplayChoice{"s", 0, 0, "short"},
                                     GroupDisplayChoice{"m", 0, 0, "medium"}, GroupDisplayChoice{"l", 0, 0, "long"})));

TEST_P(NewsgroupSelectorDisplayLoadingTest, enteringADescriptionModeRequestsARestartBeforeLoadingDescriptions)
{
    const auto [rereading, mode] = GetParam();
    prepare_display(rereading, std::string{"s"} + mode);
    ASSERT_EQ(nullptr, m_data_source.m_desc_sf.m_hp);

    const DisplayState result = run_delegated('L', "Q");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(1, g_sel_grp_display_mode_index);
    EXPECT_EQ(mode, current_group_display_mode());
    EXPECT_EQ(nullptr, m_data_source.m_desc_sf.m_hp);
    EXPECT_EQ(mode == 'm' ? "(medium display style)" : "(long display style)", g_msg);
    expect_display_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndDescriptionMode, NewsgroupSelectorDisplayLoadingTest,
                         testing::Combine(testing::Bool(), testing::Values('m', 'l')));

TEST_P(NewsgroupSelectorDisplayScopeTest, returningToShortDisplayNeedsNoDescriptionCache)
{
    const bool rereading = GetParam();
    prepare_display(rereading, "ms");
    ASSERT_EQ(nullptr, m_data_source.m_desc_sf.m_hp);

    const DisplayState result = run_delegated('L', "Q");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ('s', current_group_display_mode());
    EXPECT_EQ("(short display style)", g_msg);
    EXPECT_EQ(nullptr, m_data_source.m_desc_sf.m_hp);
    expect_display_preserved(rereading);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDisplaySessionTest, descriptionDisplayResumesTheSelectorWithSelectionsAndReadRangesIntact)
{
    const auto [rereading, mode, cached] = GetParam();
    prepare_display_session(rereading, std::string{"s"} + mode);
    if (cached)
    {
        load_descriptions();
    }

    const char result = run_selector_session(rereading ? "U.LqQ" : "LqQ");

    EXPECT_EQ('q', result);
    EXPECT_EQ(mode, current_group_display_mode());
    ASSERT_NE(nullptr, m_data_source.m_desc_sf.m_hp);
    EXPECT_EQ("Cplusplus discussion\n", m_data_source.find_group_desc("comp.lang.cpp"));
    const std::size_t description = m_output.find("Cplusplus discussion");
    ASSERT_NE(std::string::npos, description);
    const std::size_t row_start = m_output.rfind('\n', description);
    const std::string row = m_output.substr(row_start == std::string::npos ? 0 : row_start + 1,
                                            row_start == std::string::npos ? description : description - row_start - 1);
    EXPECT_EQ(mode == 'l', row.find("comp.lang.cpp") != std::string::npos);
    EXPECT_NE(std::string::npos, m_output.find("Previous discussion"));
    EXPECT_NE(std::string::npos, m_output.find(mode == 'm' ? "(medium display style)" : "(long display style)"));
    expect_selector_session_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeDescriptionModeAndCache, NewsgroupSelectorDisplaySessionTest,
                         testing::Combine(testing::Bool(), testing::Values('m', 'l'), testing::Bool()));

TEST_P(NewsgroupSelectorDisplayScopeTest, unavailableDescriptionsFallBackToNamesAndKeepTheSelectorUsable)
{
    const bool rereading = GetParam();
    prepare_display_session(rereading, "sm");
    m_data_source.m_group_desc = (m_output_dir / "absent-newsgroups").generic_string();

    const char result = run_selector_session(rereading ? "U.LqQ" : "LqQ");

    EXPECT_EQ('q', result);
    EXPECT_EQ('m', current_group_display_mode());
    EXPECT_EQ(nullptr, m_data_source.m_desc_sf.m_hp);
    EXPECT_EQ(0, m_data_source.m_desc_sf.m_refetch_secs);
    EXPECT_NE(std::string::npos, m_output.find("comp.lang.cpp"));
    EXPECT_NE(std::string::npos, m_output.find("comp.previous"));
    expect_selector_session_preserved(rereading);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDisplayScopeTest, repeatedDisplayCommandsCycleBackToNamesWithoutChangingSelections)
{
    const bool rereading = GetParam();
    prepare_display_session(rereading);

    const char result = run_selector_session(rereading ? "U.LLLqQ" : "LLLqQ");

    EXPECT_EQ('q', result);
    EXPECT_EQ("slm", g_sel_grp_display_mode);
    EXPECT_EQ(0, g_sel_grp_display_mode_index);
    EXPECT_EQ('s', current_group_display_mode());
    const std::size_t long_style = m_output.find("(long display style)");
    const std::size_t medium_style = m_output.find("(medium display style)");
    const std::size_t short_style = m_output.find("(short display style)");
    ASSERT_NE(std::string::npos, long_style);
    ASSERT_NE(std::string::npos, medium_style);
    ASSERT_NE(std::string::npos, short_style);
    EXPECT_LT(long_style, medium_style);
    EXPECT_LT(medium_style, short_style);
    const std::string short_page = m_output.substr(medium_style, short_style - medium_style);
    EXPECT_EQ(std::string::npos, short_page.find("discussion"));
    EXPECT_NE(std::string::npos, short_page.find("comp.lang.cpp"));
    expect_selector_session_preserved(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorDisplayScopeTest, testing::Bool());

TEST_P(NewsgroupSelectorRefreshInvalidationTest, refreshInvalidatesCachedBoundsAndCountsOnlySubscribedCandidates)
{
    const auto [rereading, unread] = GetParam();
    prepare_refresh(rereading);
    NewsgroupData &group = g_newsgroup_data[0];
    group.m_to_read = unread;
    group.m_subscribe_char = unread == TR_UNSUB ? '!' : ':';
    group.m_flags &= ~NF_DEL;
    recount_groups();
    const std::string numbers{group.rc_numbers()};

    const DisplayState result = run_delegated('=', "Q");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(0, g_missing_count);
    EXPECT_EQ(NewsgroupNum{unread >= TR_NONE ? 3 : 2}, g_newsgroup_to_read);
    EXPECT_EQ(unread, group.m_to_read);
    EXPECT_EQ(unread == TR_UNSUB ? '!' : ':', group.m_subscribe_char);
    EXPECT_EQ(numbers, group.rc_numbers());
    for (const NewsgroupData &candidate : g_newsgroup_data)
    {
        EXPECT_EQ(ArticleNum{}, candidate.m_abs_first);
    }
    expect_selector_context(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndCachedCount, NewsgroupSelectorRefreshInvalidationTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(ArticleUnread{TR_JUNK}, ArticleUnread{TR_BOGUS},
                                                          ArticleUnread{TR_UNSUB}, ArticleUnread{0}, ArticleUnread{1},
                                                          ArticleUnread{2})));

TEST_P(NewsgroupSelectorRefreshCleanupTest, refreshAppliesPendingMarksBeforeInvalidatingCounts)
{
    const auto [rereading, exclusive] = GetParam();
    prepare_refresh(rereading);
    NewsgroupData &group = g_newsgroup_data[0];
    group.m_flags |= NF_DEL;
    if (rereading)
    {
        group.m_flags |= NF_DEL_SEL | NF_UNTHREADED;
    }
    g_sel.exclusive = exclusive;

    const DisplayState result = run_delegated('=', "Q");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(" 1-3", group.rc_numbers());
    EXPECT_EQ(TR_NONE, group.m_to_read);
    EXPECT_EQ(rereading ? NF_UNTHREADED : NF_NONE, group.m_flags & NF_UNTHREADED);
    EXPECT_EQ(rereading ? NF_SEL : NF_NONE, group.m_flags & NF_SEL);
    EXPECT_EQ(NF_SEL, g_newsgroup_data[1].m_flags & NF_SEL);
    EXPECT_EQ(exclusive, g_sel.exclusive);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    for (const NewsgroupData &candidate : g_newsgroup_data)
    {
        EXPECT_EQ(NF_NONE, candidate.m_flags & (NF_DEL | NF_DEL_SEL));
        EXPECT_EQ(ArticleNum{}, candidate.m_abs_first);
    }
    if (rereading)
    {
        expect_exit_files_unchanged();
    }
    else
    {
        EXPECT_EQ(newsrc_text(m_newsrc), read_newsrc(m_newsrc.name));
        EXPECT_EQ(m_original_newsrc, read_newsrc(m_newsrc.old_name));
    }
    expect_selector_context(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndDisplay, NewsgroupSelectorRefreshCleanupTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorRefreshRebuildTest, restartingAfterRefreshRecountsGroupsFromTheActiveList)
{
    const auto [rereading, new_articles] = GetParam();
    prepare_refresh(rereading);
    const int high = new_articles ? 5 : 3;
    replace_active_counts(g_data_sources.front(), high);

    const char result = run_selector_session(rereading ? "U=qQ" : "=qQ");

    EXPECT_EQ('q', result);
    EXPECT_EQ(NewsgroupNum{rereading && !new_articles ? 0 : 3}, g_newsgroup_to_read);
    EXPECT_EQ(rereading && new_articles ? 0 : 3, g_sel_page.total_obj_cnt);
    for (int index = 0; index < 3; ++index)
    {
        const NewsgroupData &group = g_newsgroup_data[index];
        EXPECT_EQ(ArticleUnread{high - (rereading ? 3 : index == 1 ? 1 : 2)}, group.m_to_read) << index;
        EXPECT_EQ(ArticleNum{1}, group.m_abs_first) << index;
        EXPECT_EQ(ArticleNum{high}, group.m_ng_max) << index;
        EXPECT_EQ(':', group.m_subscribe_char) << index;
        EXPECT_EQ(rereading ? " 1-3" : index == 1 ? " 1" : " 1-2", group.rc_numbers()) << index;
    }
    expect_exit_files_unchanged();
    expect_selector_context(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndNewArticles, NewsgroupSelectorRefreshRebuildTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_F(NewsgroupSelectorRefreshStateTest, repeatedRefreshesKeepReadAndUnsubscribedGroupsOutOfTheUnreadCount)
{
    prepare_refresh(false);
    NewsgroupData &read_group = g_newsgroup_data[0];
    read_group.m_rc_line.resize(read_group.m_num_offset);
    read_group.m_rc_line += " 1-3";
    read_group.m_to_read = TR_NONE;
    g_newsgroup_data[2].m_subscribe_char = '!';
    g_newsgroup_data[2].m_to_read = TR_UNSUB;
    m_newsrc.flags |= RF_RC_CHANGED;
    recount_groups();

    const char result = run_selector_session("==qQ");

    EXPECT_EQ('q', result);
    EXPECT_EQ(NewsgroupNum{1}, g_newsgroup_to_read);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(TR_NONE, read_group.m_to_read);
    EXPECT_EQ(TR_UNSUB, g_newsgroup_data[2].m_to_read);
    EXPECT_EQ('!', g_newsgroup_data[2].m_subscribe_char);
    EXPECT_EQ(" 1-2", g_newsgroup_data[2].rc_numbers());
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorRefreshSourceTest, refreshForcesOnlyEligibleActiveSources)
{
    const RefreshSourceState &state = GetParam();
    prepare_refresh(false);
    DataSource &source = g_data_sources.front();
    replace_active_counts(source, 5);
    source.m_flags = state.flags;
    source.m_act_sf.m_refetch_secs = state.refetch ? 3600 : 0;
    if (!state.cached_file)
    {
        std::fclose(source.m_act_sf.m_fp);
        source.m_act_sf.m_fp = nullptr;
    }

    const DisplayState result = run_delegated('=', "Q");

    EXPECT_EQ(DS_RESTART, result);
    expect_active_counts(source, state.refreshed ? 5 : 3);
    EXPECT_EQ(state.flags, source.m_flags);
    EXPECT_EQ(&source, g_data_source);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(SourceState, NewsgroupSelectorRefreshSourceTest,
                         testing::Values(RefreshSourceState{"open active", DF_OPEN | DF_ACTIVE, true, true, true},
                                         RefreshSourceState{"inactive", DF_OPEN, true, true, false},
                                         RefreshSourceState{"closed", DF_ACTIVE, true, true, false},
                                         RefreshSourceState{"unused", DF_NONE, true, true, false},
                                         RefreshSourceState{"disabled", DF_OPEN | DF_ACTIVE, false, true, false},
                                         RefreshSourceState{"no cached file", DF_OPEN | DF_ACTIVE, true, false,
                                                            false}));

TEST_F(NewsgroupSelectorRefreshStateTest, refreshRefetchesARecentRemoteActiveList)
{
    prepare_refresh(false);
    DataSource &source = g_data_sources.front();
    source.m_extra_name = source.m_news_id;
    source.m_news_id = "news.example.test";
    source.m_flags |= DF_REMOTE;
    const auto connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    g_nntp.link.connection = connection;
    g_nntp.link.flags = NNTP_NEW_CMD_OK;
    source.m_nntp_link = g_nntp.link;
    testing::InSequence sequence;
    EXPECT_CALL(*connection, write_line(testing::StrEq("LIST"), testing::_));
    EXPECT_CALL(*connection, read_line(testing::_)).WillOnce(testing::Return("215 active list follows"));
    for (const NewsgroupData &group : g_newsgroup_data)
    {
        EXPECT_CALL(*connection, read_line(testing::_))
            .WillOnce(testing::Return(std::string{group.rc_name()} + " 5 1 y"));
    }
    EXPECT_CALL(*connection, read_line(testing::_)).WillOnce(testing::Return("."));

    const DisplayState result = run_delegated('=', "Q");

    EXPECT_EQ(DS_RESTART, result);
    expect_active_counts(source, 5);
    EXPECT_EQ(3600, source.m_act_sf.m_refetch_secs);
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorRefreshStateTest, refreshUpdatesEveryOpenActiveSource)
{
    prepare_refresh(false);
    DataSource &other = add_refresh_source();
    replace_active_counts(g_data_sources.front(), 5);
    replace_active_counts(other, 7);

    const DisplayState result = run_delegated('=', "Q");

    EXPECT_EQ(DS_RESTART, result);
    expect_active_counts(g_data_sources.front(), 5);
    expect_active_counts(other, 7);
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorRefreshStateTest, aRefreshOfAnEmptyCatalogStillRefreshesTheActiveList)
{
    prepare_refresh(false);
    DataSource &source = g_data_sources.front();
    replace_active_counts(source, 5);
    g_newsgroup_data.clear();
    g_ng_order.order.clear();
    g_ng_order.first_newsgroup = g_ng_order.last_newsgroup = g_newsgroup_ptr = g_current_newsgroup =
        g_recent_newsgroup = nullptr;
    g_sel_page.page_ngd = g_sel_page.next_ngd = nullptr;
    g_newsgroup_count = g_newsgroup_to_read = NewsgroupNum{};

    const DisplayState result = run_delegated('=', "Q");

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_EQ(0, g_missing_count);
    EXPECT_EQ(NewsgroupNum{}, g_newsgroup_to_read);
    ASSERT_EQ(3, source.m_act_sf.m_lines.size());
    EXPECT_EQ("comp.lang.cpp 5 1 y\n", source.m_act_sf.m_lines[1]);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedQuitTest, quittingRetainsPendingChangesAndTheSelectorPosition)
{
    const auto [command, rereading, at_end] = GetParam();
    prepare_exit(rereading, true, at_end);
    NewsgroupData *const group = g_newsgroup_ptr;

    const DisplayState result = run_delegated('\\', std::string{command} + "Q");

    EXPECT_EQ(DS_QUIT, result);
    EXPECT_EQ(group, g_newsgroup_ptr);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_page(true, 1);
    expect_selector_context(rereading);
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(CommandReadScopeAndEndPrompt, NewsgroupSelectorDelegatedQuitTest,
                         testing::Combine(testing::Values('q', 'Q'), testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedAbandonExitTest, confirmedAbandonmentRequestsRestorationWithoutWritingTheNewsrc)
{
    const auto [rereading, answer] = GetParam();
    prepare_exit(rereading, true);

    const DisplayState result = run_delegated('x', std::string{answer} + "Q");

    EXPECT_EQ(DS_QUIT, result);
    EXPECT_TRUE(g_restore_old_newsrc);
    EXPECT_THAT(m_output, testing::HasSubstr("The abandoned changes are in"));
    expect_page(true, 1);
    expect_selector_context(rereading);
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndConfirmation, NewsgroupSelectorDelegatedAbandonExitTest,
                         testing::Combine(testing::Bool(), testing::Values('y', ' ', '\r', '\n')));

TEST_P(NewsgroupSelectorDelegatedDeclineExitTest, decliningAbandonmentKeepsTheSelectorAndPendingChanges)
{
    const auto [rereading, answer] = GetParam();
    prepare_exit(rereading, true);

    const DisplayState result = run_delegated('x', std::string{answer} + "\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_FALSE(g_restore_old_newsrc);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("The abandoned changes are in")));
    expect_page(true, 1);
    expect_selector_context(rereading);
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndRefusal, NewsgroupSelectorDelegatedDeclineExitTest,
                         testing::Combine(testing::Bool(), testing::Values('n', 'q')));

TEST_P(NewsgroupSelectorDelegatedErrorTest, anUnknownCommandRetainsThePageInputAndPendingChanges)
{
    const auto [rereading, later_page, at_end] = GetParam();
    prepare_exit(rereading, later_page, at_end);
    NewsgroupData *const group = g_newsgroup_ptr;

    const DisplayState result = run_delegated('@', "Q");

    EXPECT_EQ(DS_ERROR, result);
    EXPECT_EQ(group, g_newsgroup_ptr);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_page(later_page, 1);
    expect_selector_context(rereading);
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopePageAndEndPrompt, NewsgroupSelectorDelegatedErrorTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedQuitSessionTest, aDelegatedQuitReturnsTheGroupListQuitCommand)
{
    prepare_exit(false, false);
    const std::string input = "\\\\" + std::string{GetParam()} + "Q";

    const char result = run_selector_session(input);

    EXPECT_EQ('q', result);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_selector_context();
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Command, NewsgroupSelectorDelegatedQuitSessionTest, testing::Values('q', 'Q'));

TEST_F(NewsgroupSelectorDelegatedErrorSessionTest, anUnknownDelegatedCommandAllowsTheNextSelectorCommand)
{
    const char result = run_selector_session("\\\\@");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("help"));
    EXPECT_THAT(m_output, testing::HasSubstr("reverse group name order"));
    expect_selector_context(false, -1);
    expect_pending_changes();
    expect_exit_files_unchanged();
    EXPECT_FALSE(macro_pending());
    EXPECT_EQ('Q', read_tty_char() & 0177);
}

TEST_F(NewsgroupSelectorDelegatedExitStateTest, aDelegatedAbandonExitReturnsQuitWithRestorationPending)
{
    prepare_exit(false, false);

    const char result = run_selector_session("\\\\xyQ");

    EXPECT_EQ('q', result);
    EXPECT_TRUE(g_restore_old_newsrc);
    expect_selector_context();
    expect_pending_changes();
    expect_exit_files_unchanged();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedServerLossTest, serverLossRestartsOnlyWhenAnotherSourceRemains)
{
    const auto [rereading, later_page, remaining_source] = GetParam();
    prepare_server_loss(rereading, later_page, remaining_source);

    testing::internal::CaptureStderr();
    const DisplayState result = run_delegated('y', "  Q");
    m_diagnostic = testing::internal::GetCapturedStderr();

    EXPECT_EQ(remaining_source ? DS_RESTART : DS_QUIT, result);
    EXPECT_EQ(remaining_source ? &m_multirc : nullptr, g_multirc);
    EXPECT_EQ(remaining_source ? 2 : 0, g_newsgroup_data.size());
    EXPECT_EQ(nullptr, g_sel_page.page_ngd);
    if (remaining_source)
    {
        EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
        for (const NewsgroupData &group : g_newsgroup_data)
        {
            EXPECT_NE(&m_data_source, group.m_rc->data_source);
            EXPECT_EQ(rereading ? " 1-3" : (group.rc_name() == "comp.lang.cpp" ? " 1" : " 1-2"), group.rc_numbers());
        }
    }
    EXPECT_THAT(m_diagnostic, testing::HasSubstr("503 Server closed connection."));
    expect_saved_changes();
    expect_selector_modes(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopePageAndRemainingSource, NewsgroupSelectorDelegatedServerLossTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedServerLossTest, theSelectorRecoversOrQuitsAfterARealGroupAccessFailure)
{
    const auto [rereading, later_page, remaining_source] = GetParam();
    prepare_server_loss(rereading, later_page, remaining_source);
    const std::string input =
        std::string{rereading ? "U" : ""} + (later_page ? ">" : "") + "\\\\y  " + (remaining_source ? "qQ" : "Q");
    g_sel_page.page_ngd = nullptr;

    testing::internal::CaptureStderr();
    const char result = run_selector_session(input);
    m_diagnostic = testing::internal::GetCapturedStderr();

    EXPECT_EQ('q', result);
    EXPECT_EQ(remaining_source ? &m_multirc : nullptr, g_multirc);
    if (remaining_source)
    {
        EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
        EXPECT_EQ(0, g_sel_page.item_index);
        EXPECT_EQ(2, g_sel_page.total_obj_cnt);
        EXPECT_EQ(g_ng_order.first(), g_sel_page.page_ngd);
        EXPECT_EQ(0, g_selected_count);
    }
    EXPECT_THAT(m_diagnostic, testing::HasSubstr("503 Server closed connection."));
    expect_saved_changes();
    expect_selector_modes(rereading);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedEraseTest, erasingADelegatedCommandRecoversTheOriginalPageAndCursor)
{
    const auto [rereading, later_page, dirty, erase] = GetParam();
    prepare_recovery(rereading, later_page);
    if (dirty)
    {
        ASSERT_EQ(DS_DO_COMMAND, run_delegated('t', "\\"));
        ASSERT_FALSE(macro_pending());
    }
    const std::string input = std::string{erase} + (dirty ? "\nQ" : "Q");

    const DisplayState result = run_delegated('\\', input);

    EXPECT_EQ(dirty ? DS_DISPLAY : DS_ASK, result);
    expect_page(later_page, 1);
    expect_groups_preserved();
    expect_selector_context(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopePageScreenAndEraseKey, NewsgroupSelectorDelegatedEraseTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool(),
                                          testing::Values('\b', Ctl('u'))));

TEST_P(NewsgroupSelectorDelegatedPromptTest, returningToAVisibleGroupOnlyPausesWhenTheScreenHasScrolled)
{
    const auto [rereading, later_page, scrolled] = GetParam();
    prepare_recovery(rereading, later_page);
    g_term_scrolled = scrolled;

    const DisplayState result = run_delegated('p', scrolled ? "\nQ" : "Q");

    EXPECT_EQ(scrolled ? DS_DISPLAY : DS_ASK, result);
    expect_page(later_page, 0);
    EXPECT_EQ(&g_newsgroup_data[later_page ? 2 : 1], g_newsgroup_ptr);
    expect_groups_preserved();
    expect_selector_context(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopePageAndScrolling, NewsgroupSelectorDelegatedPromptTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedOutputTest, outputAcceptsAcknowledgmentOrPreservesTheFollowupCommand)
{
    const char followup = GetParam();
    prepare_recovery(false, true);
    const std::string input = std::string{followup} + "Q";

    const DisplayState result = run_delegated('t', input);

    EXPECT_EQ(followup == 'q' || followup == 'R' ? DS_DO_COMMAND : DS_DISPLAY, result);
    EXPECT_THAT(m_output, testing::HasSubstr("Not running in thread mode."));
    expect_page(true, 1);
    expect_groups_preserved();
    expect_selector_context();
    if (followup == 'a')
    {
        ASSERT_TRUE(macro_pending());
        EXPECT_EQ('a', read_tty_char() & 0177);
    }
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Followup, NewsgroupSelectorDelegatedOutputTest,
                         testing::Values('\n', '\r', ' ', 'q', 'R', 'a'));

TEST_P(NewsgroupSelectorDelegatedMessageTest, removingARestrictionReturnsItsStatusEvenWhenTheTargetChangesPages)
{
    const auto [later_page, scrolled, verbose] = GetParam();
    prepare_recovery(false, later_page);
    g_only.set_to_do("comp.*");
    g_verbose = verbose;
    g_term_scrolled = scrolled;

    const DisplayState result = run_delegated('o', "\nQ");

    EXPECT_EQ(DS_STATUS, result);
    EXPECT_EQ(verbose ? "Restriction comp.* removed." : "Exiting \"only\".", g_msg);
    EXPECT_EQ(0, g_only.max_to_do);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    expect_page(false, 0);
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(PageScrollingAndVerbosity, NewsgroupSelectorDelegatedMessageTest,
                         testing::Combine(testing::Bool(), testing::Bool(), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedRedisplayTest, anExplicitRedisplayClearsAnOldMessageAndReturnsToTheFirstGroup)
{
    const auto [rereading, scrolled] = GetParam();
    prepare_recovery(rereading, true);
    g_msg = "Old status";
    g_term_scrolled = scrolled;

    const DisplayState result = run_delegated('o', scrolled ? "\n\nQ" : "\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_TRUE(g_msg.empty());
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    expect_page(false, 0);
    expect_groups_preserved();
    expect_selector_context(rereading);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndScrolling, NewsgroupSelectorDelegatedRedisplayTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_F(NewsgroupSelectorDelegatedRecoveryTest, theSelectorShowsTheRestrictionRemovalMessageAfterChangingPages)
{
    prepare_recovery(false, false);
    g_only.set_to_do("comp.*");

    const char result = run_selector_session(">\\\\o\nqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("Exiting \"only\"."));
    expect_page(false, 0);
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorDelegatedRecoveryTest, aCommandEnteredAfterOutputIsDispatchedWithinTheSelector)
{
    prepare_selector();

    const char result = run_selector_session("\\\\tRqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("Not running in thread mode."));
    EXPECT_THAT(m_output, testing::HasSubstr("reverse group name order"));
    expect_groups_preserved();
    expect_selector_context(false, -1);
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorDelegatedRecoveryTest, aSelectionEnteredAfterOutputIsAppliedExactlyOnce)
{
    prepare_selector();

    const char result = run_selector_session("\\\\taqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("Not running in thread mode."));
    EXPECT_EQ(0, g_selected_count);
    EXPECT_EQ(NF_NONE, g_newsgroup_data[1].m_flags & NF_SEL);
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(" 1", g_newsgroup_data[1].rc_numbers());
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_selector_context();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedEntryTest, fallbackCommandsUseTheCurrentGroupOrEndPrompt)
{
    const auto [command, at_end] = GetParam();
    prepare_selector();
    if (at_end)
    {
        g_newsgroup_ptr = nullptr;
    }

    const DisplayState result = run_delegated(command, command == '\\' ? "v\nQ" : "\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_THAT(m_output, testing::HasSubstr(at_end ? "[*End*] Cmd: " : "[comp.lang.cpp] Cmd: "));
    EXPECT_THAT(m_output, testing::HasSubstr("trn"));
    expect_groups_preserved();
    expect_selector_context();
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(at_end ? nullptr : &g_newsgroup_data[1], g_newsgroup_ptr);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(CommandAndPosition, NewsgroupSelectorDelegatedEntryTest,
                         testing::Combine(testing::Values('v', '\\'), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedRetryTest, erasedCommandsKeepRequestingInputWithinTheSameDelegatedPrompt)
{
    const auto [rereading, erase] = GetParam();
    prepare_selector(rereading);
    const std::string input = std::string{erase} + "g" + std::string{erase} + "v\nQ";

    const DisplayState result = run_delegated('g', input);

    EXPECT_EQ(DS_DISPLAY, result);
    const std::size_t prompt = m_output.find("[comp.lang.cpp] Cmd: ");
    ASSERT_NE(std::string::npos, prompt);
    EXPECT_EQ(std::string::npos, m_output.find("[comp.lang.cpp] Cmd: ", prompt + 1));
    EXPECT_THAT(m_output, testing::HasSubstr("trn"));
    expect_groups_preserved();
    expect_selector_context(rereading);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndEditing, NewsgroupSelectorDelegatedRetryTest,
                         testing::Combine(testing::Bool(), testing::Values("\b", "unused\025")));

TEST_P(NewsgroupSelectorDelegatedReadingTest, returningFromReadingRestoresTheGroupSelectorContext)
{
    const DelegatedReadingEntry &entry = GetParam();
    prepare_selector();
    g_use_news_selector = 0;

    const DisplayState result = run_delegated(entry.command, entry.input);

    EXPECT_EQ(DS_RESTART, result);
    EXPECT_THAT(m_output, testing::HasSubstr(entry.content));
    expect_selector_context();
    EXPECT_FALSE(g_sel.exclusive);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(NF_SEL, g_newsgroup_data[1].m_flags & NF_SEL);
    EXPECT_EQ(NF_NONE, g_newsgroup_data[0].m_flags & NF_SEL);
    EXPECT_EQ(NF_NONE, g_newsgroup_data[2].m_flags & NF_SEL);
    EXPECT_EQ(entry.unread, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(SS_STRING, g_sel.newsgroup_sort);
    EXPECT_FALSE(g_in_ng);
    EXPECT_FALSE(g_doing_ng);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Entry, NewsgroupSelectorDelegatedReadingTest,
                         testing::Values(DelegatedReadingEntry{'y', "QQ", "Reply body", ArticleUnread{1}},
                                         DelegatedReadingEntry{'.', "3\nQQ", "Branch body", ArticleUnread{1}},
                                         DelegatedReadingEntry{'+', "QQ", "Reply", ArticleUnread{2}},
                                         DelegatedReadingEntry{'\\', "U+QQ", "Root", ArticleUnread{2}}));

TEST_F(NewsgroupSelectorDelegationStateTest, aSingleEscapeRunsAFallbackCommandWithinTheSelectorSession)
{
    prepare_selector();

    const char result = run_selector_session("\\g\bv\nqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("[comp.lang.cpp] Cmd: "));
    EXPECT_THAT(m_output, testing::HasSubstr("trn"));
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorDelegationStateTest, aSingleEscapeTriesTheSelectorCommandBeforeTheGroupListCommand)
{
    prepare_selector();

    const char result = run_selector_session("\\RqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("reverse group name order"));
    expect_groups_preserved();
    expect_selector_context(false, -1);
    EXPECT_EQ(-SS_STRING, g_sel.newsgroup_sort);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedOptionsTest, aForcedOptionCommandRestoresTheGroupCallbackAndDisplayScope)
{
    const bool rereading = GetParam();
    prepare_selector();
    if (rereading)
    {
        g_newsgroup_to_read = NewsgroupNum{};
        for (NewsgroupData &group : g_newsgroup_data)
        {
            group.m_to_read = TR_NONE;
            group.m_flags = NF_NONE;
            group.m_rc_line.resize(group.m_num_offset);
            group.m_rc_line += " 1-3";
        }
    }
    const std::string input = std::string{rereading ? "Ua" : ""} + "E\\\\&\nqRqQ";

    const char result = run_selector_session(input);

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("Cmd: "));
    EXPECT_THAT(m_output, testing::HasSubstr("an option to change"));
    expect_selector_context(rereading, -1);
    EXPECT_TRUE(g_sel.exclusive);
    EXPECT_EQ(-SS_STRING, g_sel.newsgroup_sort);
    EXPECT_EQ(1, g_sel_page.total_obj_cnt);
    EXPECT_EQ(NF_SEL, g_newsgroup_data[1].m_flags & (NF_SEL | NF_DEL_SEL));
    for (int index = 0; index < 3; ++index)
    {
        EXPECT_EQ(rereading ? ArticleUnread{0} : ArticleUnread{index == 1 ? 2 : 1}, g_newsgroup_data[index].m_to_read);
        EXPECT_EQ(rereading ? " 1-3" : (index == 1 ? " 1" : " 1-2"), g_newsgroup_data[index].rc_numbers());
    }
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorDelegatedOptionsTest, testing::Bool());

TEST_F(NewsgroupSelectorDelegationStateTest, returningFromANestedArticleSelectorRestoresGroupCommandDispatch)
{
    prepare_selector();

    const char result = run_selector_session("\\\\+QRqQ");

    EXPECT_EQ('q', result);
    EXPECT_THAT(m_output, testing::HasSubstr("[comp.lang.cpp] Cmd: "));
    EXPECT_THAT(m_output, testing::HasSubstr("Reply"));
    expect_selector_context(false, -1);
    EXPECT_EQ(-SS_STRING, g_sel.newsgroup_sort);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(NF_SEL, g_newsgroup_data[1].m_flags & NF_SEL);
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedNavigationTest, aVisibleTargetKeepsThePageAndMovesTheCursor)
{
    const DelegatedGroupNavigation &navigation = GetParam();
    prepare_selector();
    g_newsgroup_ptr = &g_newsgroup_data[navigation.start];
    g_sel_page.item_index = 2;
    const std::string input = std::string{navigation.input} + "Q";

    const DisplayState result = run_delegated('\\', input);

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[navigation.target], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(3, g_sel_page.page_item_cnt);
    EXPECT_EQ(navigation.row, g_sel_page.item_index);
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(
    Command, NewsgroupSelectorDelegatedNavigationTest,
    testing::Values(DelegatedGroupNavigation{"n", 1, 2, 1}, DelegatedGroupNavigation{"N", 1, 2, 1},
                    DelegatedGroupNavigation{"p", 2, 1, 0}, DelegatedGroupNavigation{"P", 2, 1, 0},
                    DelegatedGroupNavigation{"^", 0, 1, 0}, DelegatedGroupNavigation{"1", 0, 1, 0},
                    DelegatedGroupNavigation{"-", 2, 0, 2}, DelegatedGroupNavigation{"gcomp.next\n", 1, 2, 1},
                    DelegatedGroupNavigation{"g2\n", 1, 2, 1}, DelegatedGroupNavigation{"/next\n", 1, 2, 1},
                    DelegatedGroupNavigation{"?previous\n", 1, 0, 2}, DelegatedGroupNavigation{"p", 1, 1, 0}));

TEST_P(NewsgroupSelectorDelegatedPagedNavigationTest, anOffPageTargetRelocatesThePageAndResetsTheCursor)
{
    const DelegatedGroupNavigation &navigation = GetParam();
    locate_group(navigation.start);
    const std::string input = std::string{navigation.input} + "Q";

    const DisplayState result = run_delegated('\\', input);

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[navigation.target], g_newsgroup_ptr);
    EXPECT_EQ(g_newsgroup_ptr, g_sel_page.page_ngd);
    EXPECT_EQ(navigation.row, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Command, NewsgroupSelectorDelegatedPagedNavigationTest,
                         testing::Values(DelegatedGroupNavigation{"n", 1, 2, 1}, DelegatedGroupNavigation{"N", 2, 0, 2},
                                         DelegatedGroupNavigation{"p", 0, 2, 1}, DelegatedGroupNavigation{"P", 2, 1, 0},
                                         DelegatedGroupNavigation{"^", 0, 1, 0}, DelegatedGroupNavigation{"1", 0, 1, 0},
                                         DelegatedGroupNavigation{"-", 1, 0, 2},
                                         DelegatedGroupNavigation{"gcomp.previous\n", 1, 0, 2},
                                         DelegatedGroupNavigation{"g1\n", 0, 1, 0},
                                         DelegatedGroupNavigation{"/previous\n", 1, 0, 2},
                                         DelegatedGroupNavigation{"?lang.cpp\n", 0, 1, 0}));

TEST_F(NewsgroupSelectorDelegatedPageTest, lookingUpTheVisibleGroupOnALaterPageRetainsThatPage)
{
    locate_group(2);
    ASSERT_EQ(1, g_sel_page.prior_obj_cnt);

    const DisplayState result = run_delegated('g', "comp.next\nQ");

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ(g_newsgroup_ptr, g_sel_page.page_ngd);
    EXPECT_EQ(1, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(1, g_sel_page.page_item_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedEndTest, goingPastTheLastGroupReturnsToTheFirstPageWithNoCurrentGroup)
{
    if (GetParam())
    {
        ASSERT_TRUE(g_sel_page.next_page());
        g_newsgroup_ptr = g_sel_page.page_ngd;
    }

    const DisplayState result = run_delegated('\\', "$Q");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_sel_page.page_ngd);
    EXPECT_EQ(0, g_sel_page.prior_obj_cnt);
    EXPECT_EQ(0, g_sel_page.item_index);
    expect_groups_preserved();
    expect_selector_context();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Page, NewsgroupSelectorDelegatedEndTest, testing::Bool());

TEST_P(NewsgroupSelectorDelegatedReadLookupTest, explicitlyRequestedReadGroupsAreShownWithoutChangingReadState)
{
    const auto [command, start] = GetParam();
    g_newsgroup_ptr = start < 0 ? nullptr : &g_newsgroup_data[start];
    ASSERT_EQ(NF_NONE, g_newsgroup_data[0].m_flags & NF_INCLUDED);
    const std::string input = std::string{command} + "Q";

    const DisplayState result = run_delegated('\\', input);

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ(NF_INCLUDED, g_newsgroup_ptr->m_flags & NF_INCLUDED);
    EXPECT_EQ(NF_NONE, g_newsgroup_ptr->m_flags & NF_SEL);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(NF_SEL, g_newsgroup_data[1].m_flags & NF_SEL);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_selector_context();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(
    Command, NewsgroupSelectorDelegatedReadLookupTest,
    testing::Values(std::pair<std::string_view, int>{"N", 2}, std::pair<std::string_view, int>{"P", -1},
                    std::pair<std::string_view, int>{"-", 1}, std::pair<std::string_view, int>{"gcomp.previous\n", 1},
                    std::pair<std::string_view, int>{"g0\n", 1}, std::pair<std::string_view, int>{"/previous/r\n", 1},
                    std::pair<std::string_view, int>{"?previous?r\n", 1}));

TEST_F(NewsgroupSelectorDelegatedReadGroupTest, lookingUpAnAlreadySelectedReadGroupDoesNotCountItTwice)
{
    g_newsgroup_data[0].m_flags |= NF_SEL;
    init_pages(PRESERVE_PAGE);
    ASSERT_EQ(3, g_sel_page.total_obj_cnt);

    const DisplayState result = run_delegated('g', "comp.previous\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[0], g_newsgroup_ptr);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(3, g_sel_page.page_item_cnt);
    EXPECT_EQ(2, g_sel_page.item_index);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(NF_SEL, g_newsgroup_ptr->m_flags & NF_SEL);
    EXPECT_EQ(TR_NONE, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_selector_context();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedRestrictedLookupTest, lookupRetainsTheActiveRestrictionAndSelections)
{
    prepare_selector();
    g_only.set_to_do("comp.[ln]*");
    init_pages(PRESERVE_PAGE);
    const std::string input = std::string{GetParam()} + "Q";

    const DisplayState result = run_delegated('\\', input);

    EXPECT_EQ(DS_ASK, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ(&g_newsgroup_data[1], g_sel_page.page_ngd);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_sel_page.page_item_cnt);
    EXPECT_EQ(1, g_sel_page.item_index);
    EXPECT_EQ(NF_NONE, g_newsgroup_data[0].m_flags & NF_INCLUDED);
    EXPECT_EQ(1, g_only.max_to_do);
    EXPECT_EQ("comp.[ln]*", g_only.restriction(0));
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(NF_SEL, g_newsgroup_data[1].m_flags & NF_SEL);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_selector_context();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Command, NewsgroupSelectorDelegatedRestrictedLookupTest,
                         testing::Values("gcomp.next\n", "g2\n", "/next\n", "?next\n"));

TEST_P(NewsgroupSelectorDelegatedUnsubscribeTest, unsubscribeRemovesTheGroupAndRetainsOtherSelectionsAndPendingMarks)
{
    const auto [rereading, target, selected] = GetParam();
    prepare_population(rereading, target, selected);
    const std::string numbers{g_newsgroup_ptr->rc_numbers()};

    const DisplayState result = run_delegated('\\', "u\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ('!', g_newsgroup_data[target].m_subscribe_char);
    EXPECT_EQ(TR_UNSUB, g_newsgroup_data[target].m_to_read);
    EXPECT_EQ(numbers, g_newsgroup_data[target].rc_numbers());
    EXPECT_EQ(NF_NONE, g_newsgroup_data[target].m_flags & NF_INCLUDED);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 2}, g_newsgroup_to_read);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(target);
    expect_files_unchanged();
    expect_selector_context(rereading);
    expect_rebuilt_page();
    EXPECT_NE(&g_newsgroup_data[target], g_sel_page.items[0].u.np);
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAnchorAndSelection, NewsgroupSelectorDelegatedUnsubscribeTest,
                         testing::Combine(testing::Bool(), testing::Values(1, 2), testing::Bool()));

TEST_P(NewsgroupSelectorDelegatedSubscriptionTest, acceptingResubscriptionRebuildsEligibilityWithoutSelectingTheGroup)
{
    const bool rereading = GetParam();

    const DisplayState result = run_delegated('g', "comp.next\ny\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[2], g_newsgroup_ptr);
    EXPECT_EQ(':', g_newsgroup_ptr->m_subscribe_char);
    EXPECT_EQ(rereading ? ArticleUnread{0} : ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(rereading ? " 1-3" : " 1-2", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(NF_INCLUDED, g_newsgroup_ptr->m_flags & NF_INCLUDED);
    EXPECT_EQ(NF_NONE, g_newsgroup_ptr->m_flags & g_sel.mask);
    EXPECT_EQ(rereading ? NF_DEL : NF_NONE, g_newsgroup_ptr->m_flags & NF_DEL);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(2);
    expect_files_unchanged();
    expect_selector_context(rereading);
    expect_rebuilt_page();
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedSubscriptionTest, decliningResubscriptionRetainsThePopulationAndCurrentGroup)
{
    const bool rereading = GetParam();

    const DisplayState result = run_delegated('g', "comp.next\nn\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ('!', g_newsgroup_data[2].m_subscribe_char);
    EXPECT_EQ(TR_UNSUB, g_newsgroup_data[2].m_to_read);
    EXPECT_EQ(NF_NONE, g_newsgroup_data[2].m_flags & NF_INCLUDED);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 2}, g_newsgroup_to_read);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(2);
    expect_files_unchanged();
    expect_selector_context(rereading);
    expect_rebuilt_page();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScope, NewsgroupSelectorDelegatedSubscriptionTest, testing::Bool());

TEST_P(NewsgroupSelectorDelegatedCatchupTest, fullCatchupRebuildsThePopulationAndCheckpointsOnlyTheTargetReadRanges)
{
    const auto [rereading, target] = GetParam();
    prepare_population(rereading, target);

    const DisplayState result = run_delegated('\\', "cy\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(TR_NONE, g_newsgroup_data[target].m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_data[target].rc_numbers());
    EXPECT_EQ(':', g_newsgroup_data[target].m_subscribe_char);
    EXPECT_EQ(rereading ? NF_INCLUDED : NF_NONE, g_newsgroup_data[target].m_flags & NF_INCLUDED);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 2}, g_newsgroup_to_read);
    EXPECT_EQ(rereading ? 3 : 2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(target);
    expect_checkpoint(target, " 1-3");
    expect_selector_context(rereading);
    expect_rebuilt_page();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndAnchor, NewsgroupSelectorDelegatedCatchupTest,
                         testing::Values(std::pair<bool, int>{false, 1}, std::pair<bool, int>{false, 2},
                                         std::pair<bool, int>{true, 1}, std::pair<bool, int>{true, 2}));

TEST_P(NewsgroupSelectorDelegatedSelectedCatchupTest, catchingUpASelectedGroupKeepsItsSelectionAndVisibleRow)
{
    const int target = GetParam();
    prepare_population(false, target, true);

    const DisplayState result = run_delegated('\\', "cy\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(TR_NONE, g_newsgroup_data[target].m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_data[target].rc_numbers());
    EXPECT_EQ(NF_SEL | NF_INCLUDED, g_newsgroup_data[target].m_flags & (NF_SEL | NF_INCLUDED));
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(NewsgroupNum{2}, g_newsgroup_to_read);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(target);
    expect_checkpoint(target, " 1-3");
    expect_selector_context();
    expect_rebuilt_page();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(Anchor, NewsgroupSelectorDelegatedSelectedCatchupTest, testing::Values(1, 2));

TEST_P(NewsgroupSelectorDelegatedUnsubscribeTest, catchupAndUnsubscribeRemovesTheGroupRegardlessOfSelection)
{
    const auto [rereading, target, selected] = GetParam();
    prepare_population(rereading, target, selected);

    const DisplayState result = run_delegated('\\', "cu\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ('!', g_newsgroup_data[target].m_subscribe_char);
    EXPECT_EQ(TR_UNSUB, g_newsgroup_data[target].m_to_read);
    EXPECT_EQ(" 1-3", g_newsgroup_data[target].rc_numbers());
    EXPECT_EQ(NF_NONE, g_newsgroup_data[target].m_flags & NF_INCLUDED);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 2}, g_newsgroup_to_read);
    EXPECT_EQ(2, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(target);
    expect_checkpoint(target, " 1-3");
    expect_selector_context(rereading);
    expect_rebuilt_page();
    EXPECT_NE(&g_newsgroup_data[target], g_sel_page.items[0].u.np);
    expect_next_command('Q');
}

TEST_F(NewsgroupSelectorDelegatedPopulationTest, partialCatchupRetainsTheUnreadTailAndItsCurrentRow)
{
    prepare_population(false, 1);

    const DisplayState result = run_delegated('\\', "c1\n\nQ");

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(" 1-2", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(':', g_newsgroup_ptr->m_subscribe_char);
    EXPECT_EQ(NF_INCLUDED, g_newsgroup_ptr->m_flags & NF_INCLUDED);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(1, g_selected_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(1);
    expect_checkpoint(1, " 1-2");
    expect_selector_context();
    expect_rebuilt_page();
    EXPECT_EQ(g_newsgroup_ptr, g_sel_page.items[0].u.np);
    expect_next_command('Q');
}

TEST_P(NewsgroupSelectorDelegatedCatchupCancelTest, decliningCatchupKeepsReadRangesSelectionsAndPendingMarks)
{
    const auto [rereading, answer] = GetParam();
    prepare_population(rereading, 1, true);
    const std::string input = std::string{"c"} + answer + "\nQ";

    const DisplayState result = run_delegated('\\', input);

    EXPECT_EQ(DS_DISPLAY, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ(rereading ? ArticleUnread{0} : ArticleUnread{2}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(rereading ? " 1-3" : " 1", g_newsgroup_ptr->rc_numbers());
    EXPECT_EQ(static_cast<NewsgroupFlags>(g_sel.mask), g_newsgroup_ptr->m_flags & g_sel.mask);
    EXPECT_EQ(NewsgroupNum{rereading ? 0 : 3}, g_newsgroup_to_read);
    EXPECT_EQ(3, g_sel_page.total_obj_cnt);
    EXPECT_EQ(2, g_selected_count);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
    expect_other_groups_preserved(1);
    expect_files_unchanged();
    expect_selector_context(rereading);
    expect_rebuilt_page();
    expect_next_command('Q');
}

INSTANTIATE_TEST_SUITE_P(ReadScopeAndAnswer, NewsgroupSelectorDelegatedCatchupCancelTest,
                         testing::Values(std::pair<bool, char>{false, 'n'}, std::pair<bool, char>{false, 'q'},
                                         std::pair<bool, char>{true, 'n'}, std::pair<bool, char>{true, 'q'}));

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

TEST_P(InputNewsgroupQuitTest, normalQuitKeepsTheCurrentChanges)
{
    const auto &[command, at_end] = GetParam();
    set_exit_origin(at_end);
    NewsgroupData *original = g_newsgroup_ptr;

    const InputNewsgroupResult result = run_command(std::string{command});

    EXPECT_EQ(ING_QUIT, result);
    EXPECT_FALSE(g_restore_old_newsrc);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Confirm:")));
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_P(InputNewsgroupExitCancelTest, decliningAbandonExitKeepsReadingAtTheSamePosition)
{
    const auto &[answer, at_end] = GetParam();
    set_exit_origin(at_end);
    NewsgroupData *original = g_newsgroup_ptr;

    const InputNewsgroupResult result = run_command(std::string{"x"} + answer);

    EXPECT_EQ(ING_NORM, result);
    EXPECT_FALSE(g_restore_old_newsrc);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Confirm: exit and abandon newsrc changes?"));
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("The abandoned changes are in")));
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_P(InputNewsgroupExitConfirmTest, confirmingAbandonExitRequestsRestorationOnQuit)
{
    const auto &[answer, at_end] = GetParam();
    set_exit_origin(at_end);
    NewsgroupData *original = g_newsgroup_ptr;

    const InputNewsgroupResult result = run_command(std::string{"x"} + answer);

    EXPECT_EQ(ING_QUIT, result);
    EXPECT_TRUE(g_restore_old_newsrc);
    EXPECT_EQ(original, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("The abandoned changes are in newsrc.new."));
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_P(InputNewsgroupAbandonCancelTest, decliningGroupAbandonmentPreservesTheCurrentChanges)
{
    const auto &[answer, verbose] = GetParam();
    g_verbose = verbose;

    const InputNewsgroupResult result = run_command(std::string{"A"} + answer);

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_FALSE(g_restore_old_newsrc);
    EXPECT_THAT(m_output, testing::HasSubstr(verbose ? "Abandon changes to current newsgroup?" : "Abandon?"));
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_P(InputNewsgroupAbandonConfirmTest, confirmingGroupAbandonmentRestoresOnlyTheCurrentGroup)
{
    const auto &[answer, verbose] = GetParam();
    g_verbose = verbose;

    const InputNewsgroupResult result = run_command(std::string{"A"} + answer);

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_EQ("comp.bravo", g_newsgroup_name);
    EXPECT_EQ(" 1", g_newsgroup_data[1].rc_numbers());
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_data[1].m_to_read);
    EXPECT_EQ(" 1-2", g_newsgroup_data[0].rc_numbers());
    EXPECT_EQ(" 1-2", g_newsgroup_data[2].rc_numbers());
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[0].m_to_read);
    EXPECT_EQ(ArticleUnread{1}, g_newsgroup_data[2].m_to_read);
    EXPECT_EQ(NewsgroupNum{3}, g_newsgroup_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_files_unchanged();
}

TEST_P(InputNewsgroupAbandonPromptTest, helpReturnsToTheAbandonPromptWithoutDiscardingChanges)
{
    g_verbose = GetParam();

    const InputNewsgroupResult result = run_command("Ahn");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Type y or SP to abandon the changes to this group"));
    EXPECT_THAT(m_output, testing::HasSubstr("Type n to leave the group as it is."));
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_P(InputNewsgroupAbandonPromptTest, invalidInputAsksAgainWithoutDiscardingChanges)
{
    g_verbose = GetParam();
    prompt_answer("n");

    const InputNewsgroupResult result = run_command("A?");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::HasSubstr("Type h for help."));
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_P(InputNewsgroupAbandonPromptTest, refreshingThePromptDoesNotConfirmAbandonment)
{
    g_verbose = GetParam();

    const InputNewsgroupResult result = run_command("A\fn");

    EXPECT_EQ(ING_SPECIAL, result);
    EXPECT_EQ(&g_newsgroup_data[1], g_newsgroup_ptr);
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_changes_preserved();
    expect_files_unchanged();
}

TEST_F(InputNewsgroupConfirmationTest, abandoningAtTheEndPromptHasNoCurrentGroupToChange)
{
    set_exit_origin(true);

    const InputNewsgroupResult result = run_command("A");

    EXPECT_EQ(ING_NORM, result);
    EXPECT_EQ(nullptr, g_newsgroup_ptr);
    EXPECT_THAT(m_output, testing::Not(testing::HasSubstr("Abandon?")));
    EXPECT_FALSE(g_restore_old_newsrc);
    expect_changes_preserved();
    expect_files_unchanged();
}

INSTANTIATE_TEST_SUITE_P(CommandAndPosition, InputNewsgroupQuitTest,
                         testing::Combine(testing::Values('q', 'Q'), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(AnswerAndPosition, InputNewsgroupExitCancelTest,
                         testing::Combine(testing::Values('n', 'q', '\033'), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ExplicitOrDefaultAndPosition, InputNewsgroupExitConfirmTest,
                         testing::Combine(testing::Values('y', ' '), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(AnswerAndVerbosity, InputNewsgroupAbandonCancelTest,
                         testing::Combine(testing::Values('n', 'q'), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(ExplicitOrDefaultAndVerbosity, InputNewsgroupAbandonConfirmTest,
                         testing::Combine(testing::Values('y', ' '), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(TerseAndVerbose, InputNewsgroupAbandonPromptTest, testing::Bool());

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

    g_nntp.link.connection.reset();
    g_nntp.link.port_number = server.port();
    g_nntp.link.flags = NNTP_NEW_CMD_OK;
    ASSERT_EQ(1, nntp_connect("127.0.0.1", false));
    m_data_source.m_nntp_link = g_nntp.link;
    push_command(" qq");

    testing::internal::CaptureStdout();
    const DoNewsgroupResult result = do_newsgroup(std::optional<std::string>{});
    const std::string       output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(NG_NEXT, result);
    EXPECT_THAT(output, testing::HasSubstr(TRN_TEST_BODY));

    g_nntp.close(true);
    server.verify();
}
