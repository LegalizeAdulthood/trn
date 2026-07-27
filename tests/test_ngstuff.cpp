// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngstuff-internal.h>

#include <config/common.h>
#include <trn/cache.h>
#include <trn/intrp.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
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

void drain_macro_buffer()
{
    while (macro_pending())
    {
        char discarded{};
        read_tty(&discarded, 1);
    }
}

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
    bool                               m_old_bizarre{};
    bool                               m_old_check_flag{};
};

class NumNumTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_abs_first = g_abs_first;
        m_old_last_art = g_last_art;
        m_old_art = g_art;
        m_old_search_ahead = g_search_ahead;
        m_old_newsgroup_ptr = g_newsgroup_ptr;

        g_abs_first = ArticleNum{1};
        g_last_art = ArticleNum{10};
        g_art = ArticleNum{1};
        g_search_ahead = ArticleNum{};
        m_newsgroup.m_to_read = 10;
        g_newsgroup_ptr = &m_newsgroup;
    }

    void TearDown() override
    {
        g_abs_first = m_old_abs_first;
        g_last_art = m_old_last_art;
        g_art = m_old_art;
        g_search_ahead = m_old_search_ahead;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
    }

    ArticleNum     m_old_abs_first;
    ArticleNum     m_old_last_art;
    ArticleNum     m_old_art;
    ArticleNum     m_old_search_ahead;
    NewsgroupData *m_old_newsgroup_ptr{};
    NewsgroupData  m_newsgroup{};
};

class SwitcherooMacroTest : public testing::Test
{
protected:
    void SetUp() override
    {
        drain_macro_buffer();
        std::copy_n(g_buf, m_old_buf.size(), m_old_buf.begin());
    }

    void TearDown() override
    {
        drain_macro_buffer();
        std::copy(m_old_buf.begin(), m_old_buf.end(), g_buf);
    }

    std::array<char, LINE_BUF_LEN + 1> m_old_buf{};
};

class PerformExpansionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_one_command = g_one_command;
        m_old_perform_count = g_perform_count;
        m_old_msg = g_msg;
        std::copy_n(g_buf, m_old_buf.size(), m_old_buf.begin());
        drain_macro_buffer();

        g_one_command = false;
        g_perform_count = 0;
    }

    void TearDown() override
    {
        g_one_command = m_old_one_command;
        g_perform_count = m_old_perform_count;
        g_msg = m_old_msg;
        drain_macro_buffer();
        std::copy(m_old_buf.begin(), m_old_buf.end(), g_buf);
    }

    std::string                        m_old_msg;
    std::array<char, LINE_BUF_LEN + 1> m_old_buf{};
    bool                               m_old_one_command{};
    int                                m_old_perform_count{};
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
        [&](std::string_view shell, std::string_view cmd)
        {
            EXPECT_TRUE(shell.empty());
            EXPECT_EQ("echo cwd", cmd);
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

TEST_F(NumNumTest, selectsSingleArticle)
{
    const NumNumResult result = num_num("5");

    EXPECT_EQ(NN_REREAD, result);
    EXPECT_EQ(ArticleNum{5}, g_art);
}

TEST_F(NumNumTest, rejectsOpenEndedRange)
{
    testing::internal::CaptureStdout();
    const NumNumResult result = num_num("5-");
    const std::string  output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(NN_ASK, result);
    EXPECT_NE(std::string::npos, output.find("\nBad range\n"));
    EXPECT_EQ(ArticleNum{1}, g_art);
}

TEST_F(NumNumTest, rejectsOpenEndedRangeAfterComma)
{
    testing::internal::CaptureStdout();
    const NumNumResult result = num_num("5,7-");
    const std::string  output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(NN_ASK, result);
    EXPECT_NE(std::string::npos, output.find("\nBad range\n"));
}

TEST_F(SwitcherooMacroTest, definesMacroFromCommandText)
{
    const std::string command{"&&~ z"};
    ASSERT_LT(command.size(), m_old_buf.size());
    command.copy(g_buf, command.size());
    g_buf[command.size()] = '\0';

    const bool result = switcheroo();

    EXPECT_FALSE(result);
    push_char('~');
    const std::string expanded_command = get_cmd();
    EXPECT_EQ('z', expanded_command[0]);
    EXPECT_EQ(FINISH_CMD, expanded_command[1]);
}

TEST_F(PerformExpansionTest, expandsCommandBeforeContinuingAfterColon)
{
    const int result = perform("%Z:Z", 0);

    EXPECT_EQ(-1, result);
    EXPECT_EQ("Unknown command: Z", g_msg);
}

TEST_F(PerformExpansionTest, splitsSwitcherooCommandBeforeContinuingAfterColon)
{
    const int result = perform("&&~ \\::Z", 0);

    EXPECT_EQ(-1, result);
    EXPECT_EQ("Unknown command: Z", g_msg);
    push_char('~');
    const std::string expanded_command = get_cmd();
    EXPECT_EQ(':', expanded_command[0]);
    EXPECT_EQ(FINISH_CMD, expanded_command[1]);
}
