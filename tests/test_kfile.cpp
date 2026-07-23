// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/kfile.h>

#include <trn/cache.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>
#include <trn/rt-process.h>
#include <trn/rthread.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <test_config.h>

#include "mock_env.h"

#include <file_contents.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class KillFileEditTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_in_ng = g_in_ng;
        m_old_kf_state = g_kf_state;
        m_old_kfs_thread_change_set = g_kfs_thread_change_set;
        m_old_kf_change_thread_cnt = g_kf_change_thread_cnt;
        m_old_local_kfp = g_local_kfp;
        m_old_msg_id_hash = g_msg_id_hash;
        m_old_article_list = g_article_list;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_force_last = g_force_last;
        m_old_general_mode = g_general_mode;
        m_old_mode = g_mode;
        m_old_first_subject = g_first_subject;
        m_old_verbose = g_verbose;
        m_old_novice_delays = g_novice_delays;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_term_scrolled = g_term_scrolled;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);

        g_in_ng = false;
        g_kf_state = KFS_NONE;
        g_kfs_thread_change_set = KFS_NONE;
        g_kf_change_thread_cnt = 0;
        g_local_kfp = nullptr;
        g_msg_id_hash = hash_create(17, msg_id_cmp);
        g_article_list.clear();
        g_newsgroup_ptr = nullptr;
        g_first_art = ArticleNum{};
        g_last_art = ArticleNum{};
        g_force_last = false;
        g_general_mode = GM_READ;
        g_mode = MM_NONE;
        g_first_subject = nullptr;
        g_verbose = false;
        g_novice_delays = false;
    }

    void TearDown() override
    {
        if (g_local_kfp != nullptr && g_local_kfp != m_old_local_kfp)
        {
            std::fclose(g_local_kfp);
        }
        if (g_msg_id_hash != nullptr && g_msg_id_hash != m_old_msg_id_hash)
        {
            hash_destroy(g_msg_id_hash);
        }

        g_in_ng = m_old_in_ng;
        g_kf_state = m_old_kf_state;
        g_kfs_thread_change_set = m_old_kfs_thread_change_set;
        g_kf_change_thread_cnt = m_old_kf_change_thread_cnt;
        g_local_kfp = m_old_local_kfp;
        g_msg_id_hash = m_old_msg_id_hash;
        g_article_list = m_old_article_list;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_force_last = m_old_force_last;
        g_general_mode = m_old_general_mode;
        g_mode = m_old_mode;
        g_first_subject = m_old_first_subject;
        g_verbose = m_old_verbose;
        g_novice_delays = m_old_novice_delays;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_term_scrolled = m_old_term_scrolled;

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    void expect_editor()
    {
        m_env.expect_no_envar("EDITOR");
        m_env.expect_env("VISUAL", ":");
    }

    trn::testing::MockEnvironment m_env;
    fs::path                      m_output_dir;
    bool                          m_old_in_ng{};
    KillFileStateFlags            m_old_kf_state{};
    KillFileStateFlags            m_old_kfs_thread_change_set{};
    int                           m_old_kf_change_thread_cnt{};
    std::FILE                    *m_old_local_kfp{};
    HashTable                    *m_old_msg_id_hash{};
    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    bool                          m_old_force_last{};
    GeneralMode                   m_old_general_mode{};
    MinorMode                     m_old_mode{};
    Subject                      *m_old_first_subject{};
    bool                          m_old_verbose{};
    bool                          m_old_novice_delays{};
    int                           m_old_term_line{};
    int                           m_old_term_col{};
    int                           m_old_term_scrolled{};
};

} // namespace

TEST_F(KillFileEditTest, createsLocalKillFileDirectory)
{
    const fs::path    kill_file = m_output_dir / "local" / "KILL";
    const std::string kill_file_name = kill_file.generic_string();

    g_in_ng = true;
    m_env.expect_env_repeatedly("KILLLOCAL", kill_file_name.c_str());
    expect_editor();

    edit_kill_file();

    EXPECT_TRUE(fs::exists(kill_file.parent_path()));
}

TEST_F(KillFileEditTest, createsGlobalKillFileDirectory)
{
    const fs::path    kill_file = m_output_dir / "global" / "KILL";
    const std::string kill_file_name = kill_file.generic_string();

    g_in_ng = false;
    m_env.expect_env_repeatedly("KILLGLOBAL", kill_file_name.c_str());
    expect_editor();

    edit_kill_file();

    EXPECT_TRUE(fs::exists(kill_file.parent_path()));
}

TEST_F(KillFileEditTest, appendLocalKillFileWritesConfiguredPath)
{
    const fs::path    kill_file = m_output_dir / "local-append" / "KILL";
    const std::string kill_file_name = kill_file.generic_string();
    fs::create_directories(kill_file.parent_path());
    std::ofstream{kill_file} << "/old/j";

    m_env.expect_env_repeatedly("KILLLOCAL", kill_file_name.c_str());

    testing::internal::CaptureStdout();
    kill_file_append("/new/j", KF_LOCAL);
    (void) testing::internal::GetCapturedStdout();

    EXPECT_EQ("/old/j\n/new/j\n", file_contents(kill_file));
    EXPECT_NE(nullptr, g_local_kfp);
    EXPECT_TRUE(g_kf_state & KFS_NORMAL_LINES);
}

TEST_F(KillFileEditTest, rewriteLocalKillFileWritesThreadCommand)
{
    const fs::path    kill_file = m_output_dir / "local-rewrite" / "KILL";
    const std::string kill_file_name = kill_file.generic_string();

    Newsrc newsrc{};
    newsrc.name = "news.example";
    NewsgroupData newsgroup{};
    newsgroup.m_rc = &newsrc;
    newsgroup.m_to_read = ArticleUnread{1};
    g_newsgroup_ptr = &newsgroup;
    g_last_art = ArticleNum{42};
    g_kf_state = KFS_LOCAL_CHANGES | KFS_THREAD_LINES;

    Article *article = article_ptr(ArticleNum{1});
    article->m_msg_id = "<case@example.com>";
    article->m_flags = AF_EXISTS;
    article->m_auto_flags = AUTO_SEL_THD;
    hash_store(g_msg_id_hash, article->msg_id_view(), {reinterpret_cast<char *>(article), 0});

    m_env.expect_env_repeatedly("KILLLOCAL", kill_file_name.c_str());

    testing::internal::CaptureStdout();
    kill_unwanted(ArticleNum{1}, "", false);
    (void) testing::internal::GetCapturedStdout();

    EXPECT_EQ("THRU news.example 42\n<case@example.com> T+\n", file_contents(kill_file));
}

TEST_F(KillFileEditTest, editLocalKillFileAppliesThreadCommand)
{
    const fs::path    kill_file = m_output_dir / "local-edit" / "KILL";
    const std::string kill_file_name = kill_file.generic_string();

    fs::create_directories(kill_file.parent_path());
    std::ofstream{kill_file} << "<case@example.com> T+\n";

    g_in_ng = true;
    Article *article = article_ptr(ArticleNum{1});
    article->m_msg_id = "<case@example.com>";
    hash_store(g_msg_id_hash, article->msg_id_view(), {reinterpret_cast<char *>(article), 0});

    m_env.expect_env_repeatedly("KILLLOCAL", kill_file_name.c_str());
    expect_editor();

    testing::internal::CaptureStdout();
    edit_kill_file();
    (void) testing::internal::GetCapturedStdout();

    EXPECT_TRUE((article->m_auto_flags & AUTO_SEL_THD) != 0);
}

TEST_F(KillFileEditTest, rewriteGlobalThreadKillFileWritesThreadCommand)
{
    const fs::path    kill_file = m_output_dir / "global-thread" / "KILLTHREADS";
    const std::string kill_file_name = kill_file.generic_string();
    const long        day_num = static_cast<long>(std::time(nullptr)) / 86400 - 10490;

    fs::create_directories(kill_file.parent_path());
    std::ofstream{kill_file} << "<case@example.com> + " << day_num << "\n";

    hash_destroy(g_msg_id_hash);
    g_msg_id_hash = nullptr;

    m_env.expect_env_repeatedly("KILLTHREADS", kill_file_name.c_str());
    kill_file_init();
    g_kf_change_thread_cnt = 1;
    g_kf_state |= KFS_THREAD_CHANGES;

    update_thread_kill_file();

    EXPECT_EQ("<case@example.com> + " + std::to_string(day_num) + "\n", file_contents(kill_file));
}
