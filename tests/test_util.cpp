// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/init.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/env.h>
#include <util/util2.h>

#include <config/common.h>
#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{

namespace fs = std::filesystem;

class TempFilenameTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_tmp_dir = g_tmp_dir;
        m_old_pid = g_our_pid;

        g_tmp_dir = TRN_TEST_TMP_DIR;
        g_our_pid = 2468;
    }

    void TearDown() override
    {
        g_tmp_dir = m_old_tmp_dir;
        g_our_pid = m_old_pid;
    }

    std::string take_temp_filename()
    {
        return temp_filename();
    }

    std::string m_old_tmp_dir;
    long        m_old_pid{};
};

class EditFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_term_scrolled = g_term_scrolled;
    }

    void TearDown() override
    {
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_term_scrolled = m_old_term_scrolled;
    }

    trn::testing::MockEnvironment m_env;
    int                           m_old_term_line{};
    int                           m_old_term_col{};
    int                           m_old_term_scrolled{};
};

class FileExpansionTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_home_dir = g_home_dir;
        m_old_dot_dir = g_dot_dir;
    }

    void TearDown() override
    {
        g_home_dir = m_old_home_dir;
        g_dot_dir = m_old_dot_dir;
    }

    std::string m_old_home_dir;
    std::string m_old_dot_dir;
};

class EnvInitTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_home_dir = g_home_dir;
        m_old_dot_dir = g_dot_dir;
        m_old_trn_dir = g_trn_dir;
        m_old_lib = g_lib;
        m_old_rn_lib = g_rn_lib;
        m_old_tmp_dir = g_tmp_dir;
        m_old_login_name = g_login_name;
        m_old_real_name = g_real_name;
        m_old_p_host_name = g_p_host_name;
        m_old_local_host = g_local_host;
        m_old_net_speed = g_net_speed;

        g_home_dir.clear();
        g_dot_dir.clear();
        g_trn_dir.clear();
        g_lib.clear();
        g_rn_lib.clear();
        g_tmp_dir.clear();
        g_login_name.clear();
        g_real_name.clear();
        g_p_host_name.clear();
        g_local_host.clear();
        g_net_speed = 20;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_root = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_home = m_root / "home";
        m_tmp = m_root / "tmp";
        m_trn = m_root / "trn";

        std::error_code error;
        fs::remove_all(m_root, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_home, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_tmp, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_trn, error);
        ASSERT_FALSE(error) << error.message();
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_root, error);

        g_home_dir = m_old_home_dir;
        g_dot_dir = m_old_dot_dir;
        g_trn_dir = m_old_trn_dir;
        g_lib = m_old_lib;
        g_rn_lib = m_old_rn_lib;
        g_tmp_dir = m_old_tmp_dir;
        g_login_name = m_old_login_name;
        g_real_name = m_old_real_name;
        g_p_host_name = m_old_p_host_name;
        g_local_host = m_old_local_host;
        g_net_speed = m_old_net_speed;
    }

    void expect_login_environment()
    {
        m_env.expect_no_envar("USER");
        m_env.expect_no_envar("LOGNAME");

#ifdef MSDOS
        m_env.expect_env("USERNAME", "casey");
#endif
    }

    trn::testing::MockEnvironment m_env;
    fs::path                      m_root;
    fs::path                      m_home;
    fs::path                      m_tmp;
    fs::path                      m_trn;
    std::string                   m_old_home_dir;
    std::string                   m_old_dot_dir;
    std::string                   m_old_trn_dir;
    std::string                   m_old_lib;
    std::string                   m_old_rn_lib;
    std::string                   m_old_tmp_dir;
    std::string                   m_old_login_name;
    std::string                   m_old_real_name;
    std::string                   m_old_p_host_name;
    std::string                   m_old_local_host;
    int                           m_old_net_speed{};
};

bool ends_with(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

TEST_F(TempFilenameTest, returnsUniqueNameInTempDirectory)
{
    const std::string first{take_temp_filename()};
    const std::string second{take_temp_filename()};

    EXPECT_EQ(fs::path{TRN_TEST_TMP_DIR}, fs::path{first}.parent_path());
    EXPECT_NE(first, second);

    const std::string filename{fs::path{first}.filename().string()};
    EXPECT_EQ(0U, filename.find("trn"));
    EXPECT_TRUE(ends_with(filename, ".2468"));
}

TEST_F(EditFileTest, doesNotCopyEditorCommandToGlobalScratchBuffer)
{
    const std::string filename{TRN_TEST_TMP_DIR "/edit-target"};

    m_env.expect_no_envar("EDITOR");
    m_env.expect_env("VISUAL", ":");
    g_cmd_buf[0] = '#';
    g_cmd_buf[1] = '\0';

    EXPECT_EQ(0, edit_file(filename.c_str()));
    EXPECT_STREQ("#", g_cmd_buf);
}

TEST_F(FileExpansionTest, expandsHomeDirectory)
{
    g_home_dir = "C:/Users/tester";

    EXPECT_EQ("C:/Users/tester/News", file_exp("~/News"));
}

TEST_F(FileExpansionTest, expandsEnvironmentVariable)
{
    trn::testing::MockEnvironment env;
    env.expect_env("ARTICLE", "C:/articles");

    EXPECT_EQ("C:/articles/current", file_exp("$ARTICLE/current"));
}

TEST_F(FileExpansionTest, expandsDotDirectory)
{
    g_dot_dir = "C:/Users/tester/.trn";

    EXPECT_EQ("C:/Users/tester/.trn/access", file_exp("%./access"));
}

TEST_F(EnvInitTest, readsRealNameFromFullnameFile)
{
    std::ofstream{m_home / ".fullname"} << "Casey Writer\n";
    std::ofstream{m_home / "fullname"} << "Casey Writer\n";

    const std::string home = m_home.generic_string();
    const std::string tmp = m_tmp.generic_string();
    const std::string trn = m_trn.generic_string();
    m_env.expect_env("HOME", home.c_str());
    m_env.expect_env("TMPDIR", tmp.c_str());
    expect_login_environment();
    m_env.expect_env("DOTDIR", home.c_str());
    m_env.expect_env("TRNDIR", trn.c_str());
    m_env.expect_env("NETSPEED", "5");

    (void) env_init(true);

    EXPECT_EQ("Casey Writer", g_real_name);
}

TEST_F(EnvInitTest, usesConfiguredPostingHostNameDefault)
{
    std::ofstream{m_home / ".fullname"} << "Casey Writer\n";
    std::ofstream{m_home / "fullname"} << "Casey Writer\n";

    const std::string home = m_home.generic_string();
    const std::string tmp = m_tmp.generic_string();
    const std::string trn = m_trn.generic_string();
    m_env.expect_env("HOME", home.c_str());
    m_env.expect_env("TMPDIR", tmp.c_str());
    expect_login_environment();
    m_env.expect_env("DOTDIR", home.c_str());
    m_env.expect_env("TRNDIR", trn.c_str());
    m_env.expect_env("NETSPEED", "5");

    (void) env_init(true);

    EXPECT_EQ(std::string{POSTING_HOSTNAME} + ".UNKNOWN.HOST", g_p_host_name);
}

TEST(SecsToTextTest, returnsSentinelText)
{
    EXPECT_EQ(std::string{"never"}, secs_to_text(0));
    EXPECT_EQ(std::string{"never"}, secs_to_text(1));
    EXPECT_EQ(std::string{"missing"}, secs_to_text(2));
}

TEST(SecsToTextTest, formatsCompositeIntervals)
{
    EXPECT_EQ(std::string{"1 day, 2 hours, 3 minutes"}, secs_to_text(93780));
    EXPECT_EQ(std::string{"2 days, 1 hour"}, secs_to_text(176400));
}
