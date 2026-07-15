// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/final.h>

#include <trn/init.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/nntp.h>
#include <trn/rcstuff.h>
#include <trn/terminal.h>
#include <util/env-internal.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class FinalizeTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_tmp_dir = g_tmp_dir;
        m_old_pid = g_our_pid;
        m_old_head_name = g_head_name;
        m_old_check_flag = g_check_flag;
        m_old_bizarre = g_bizarre;
        m_old_kf_state = g_kf_state;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_saved_score_file = m_output_dir / "saved-scores";

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_tmp_dir = m_output_dir.generic_string();
        g_our_pid = 97531;
        g_head_name = (m_output_dir / "head.tmp").generic_string();
        g_check_flag = true;
        g_bizarre = false;
        g_kf_state = KFS_NONE;
    }

    void TearDown() override
    {
        g_tmp_dir = m_old_tmp_dir;
        g_our_pid = m_old_pid;
        g_head_name = m_old_head_name;
        g_check_flag = m_old_check_flag;
        g_bizarre = m_old_bizarre;
        g_kf_state = m_old_kf_state;

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    fs::path nntp_temp_file(int index) const
    {
        return m_output_dir / nntp_tmp_name(index);
    }

    std::string        m_old_tmp_dir;
    long               m_old_pid{};
    std::string        m_old_head_name;
    bool               m_old_check_flag{};
    bool               m_old_bizarre{};
    KillFileStateFlags m_old_kf_state{KFS_NONE};
    fs::path           m_output_dir;
    fs::path           m_saved_score_file;
};

} // namespace

TEST_F(FinalizeTest, removesTemporaryFiles)
{
    for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
    {
        std::ofstream{nntp_temp_file(i)} << "article\n";
        ASSERT_TRUE(fs::exists(nntp_temp_file(i)));
    }
    std::ofstream{g_head_name} << "header\n";
    ASSERT_TRUE(fs::exists(g_head_name));

    const std::string saved_score_file = m_saved_score_file.generic_string();
    EXPECT_EXIT(
        {
            set_environment(
                [saved_score_file](const char *name) -> char *
                {
                    return std::strcmp(name, "SAVESCOREFILE") == 0 ? const_cast<char *>(saved_score_file.c_str())
                                                                   : nullptr;
                });
            finalize(0);
        },
        ::testing::ExitedWithCode(0), ".*");

    for (int i = 0; i < MAX_NNTP_ARTICLES; i++)
    {
        EXPECT_FALSE(fs::exists(nntp_temp_file(i)));
    }
    EXPECT_FALSE(fs::exists(g_head_name));
}
