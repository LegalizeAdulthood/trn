// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/kfile.h>

#include <trn/cache.h>
#include <trn/ngdata.h>
#include <trn/terminal.h>

#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
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
        m_old_local_kfp = g_local_kfp;
        m_old_first_subject = g_first_subject;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_term_scrolled = g_term_scrolled;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);

        g_in_ng = false;
        g_kf_state = KFS_NONE;
        g_local_kfp = nullptr;
        g_first_subject = nullptr;
    }

    void TearDown() override
    {
        if (g_local_kfp != nullptr && g_local_kfp != m_old_local_kfp)
        {
            std::fclose(g_local_kfp);
        }

        g_in_ng = m_old_in_ng;
        g_kf_state = m_old_kf_state;
        g_local_kfp = m_old_local_kfp;
        g_first_subject = m_old_first_subject;
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
    std::FILE                    *m_old_local_kfp{};
    Subject                      *m_old_first_subject{};
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
