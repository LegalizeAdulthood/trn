// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngstuff-internal.h>

#include <config/common.h>
#include <trn/rcstuff.h>
#include <trn/respond.h>
#include <trn/terminal.h>
#include <trn/trn.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class EscapadeTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_cwd = fs::current_path();
        m_old_priv_dir = g_priv_dir;
        m_old_save_dir = g_save_dir;
        m_old_bizarre = g_bizarre;
        m_old_check_flag = g_check_flag;
        std::copy_n(g_buf, m_old_buf.size(), m_old_buf.begin());
        std::copy_n(g_cmd_buf, m_old_cmd_buf.size(), m_old_cmd_buf.begin());

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_root = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_start = m_root / "start";
        m_private = m_root / "private";
        m_save = m_root / "save";

        std::error_code error;
        fs::remove_all(m_root, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_start, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_private, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_save, error);
        ASSERT_FALSE(error) << error.message();
        fs::current_path(m_start, error);
        ASSERT_FALSE(error) << error.message();

        g_priv_dir = m_private.generic_string();
        g_check_flag = false;
    }

    void TearDown() override
    {
        std::copy(m_old_buf.begin(), m_old_buf.end(), g_buf);
        std::copy(m_old_cmd_buf.begin(), m_old_cmd_buf.end(), g_cmd_buf);
        g_priv_dir = m_old_priv_dir;
        g_save_dir = m_old_save_dir;
        g_bizarre = m_old_bizarre;
        g_check_flag = m_old_check_flag;

        std::error_code error;
        fs::current_path(m_old_cwd, error);
        fs::remove_all(m_root, error);
    }

    void expect_equivalent(const fs::path &expected, const fs::path &actual)
    {
        std::error_code error;
        EXPECT_TRUE(fs::equivalent(expected, actual, error));
        EXPECT_FALSE(error) << error.message();
    }

    fs::path                           m_old_cwd;
    fs::path                           m_root;
    fs::path                           m_start;
    fs::path                           m_private;
    fs::path                           m_save;
    std::string                        m_old_priv_dir;
    std::string                        m_old_save_dir;
    std::array<char, LINE_BUF_LEN + 1> m_old_buf{};
    std::array<char, CMD_BUF_LEN>      m_old_cmd_buf{};
    bool                               m_old_bizarre{};
    bool                               m_old_check_flag{};
};

} // namespace

TEST_F(EscapadeTest, restoresCurrentDirectoryAfterShellCommand)
{
    bool              ran_shell{};
    fs::path          shell_cwd;
    const std::string command{"!echo cwd"};
    ASSERT_LT(command.size(), m_old_buf.size());
    command.copy(g_buf, command.size());
    g_buf[command.size()] = '\0';

    bool result = escapade_with_shell_runner(
        [&](const char *shell, const char *cmd)
        {
            EXPECT_EQ(nullptr, shell);
            EXPECT_STREQ("echo cwd", cmd);
            shell_cwd = fs::current_path();
            ran_shell = true;
            return 0;
        });

    EXPECT_FALSE(result);
    EXPECT_TRUE(ran_shell);
    expect_equivalent(m_private, shell_cwd);
    expect_equivalent(m_start, fs::current_path());
}

TEST_F(EscapadeTest, restoresCurrentDirectoryAfterSaveDirectorySwitch)
{
    const std::string command = "&-d" + m_save.generic_string();
    ASSERT_LT(command.size(), m_old_buf.size());
    command.copy(g_buf, command.size());
    g_buf[command.size()] = '\0';

    const bool result = switcheroo();

    EXPECT_FALSE(result);
    EXPECT_EQ(m_save.generic_string(), g_save_dir);
    expect_equivalent(m_save, g_priv_dir);
    expect_equivalent(m_start, fs::current_path());
}
