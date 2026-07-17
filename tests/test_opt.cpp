// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/opt.h>

#include <file_contents.h>

#include <trn/OptionCatalog.h>
#include <trn/respond.h>
#include <trn/trn.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class CwdCheckTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_cwd = fs::current_path();
        m_old_home_dir = g_home_dir;
        m_old_priv_dir = g_priv_dir;
        m_old_verbose = g_verbose;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_root = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        m_home = m_root / "home";

        std::error_code error;
        fs::remove_all(m_root, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_home, error);
        ASSERT_FALSE(error) << error.message();
        fs::current_path(m_root, error);
        ASSERT_FALSE(error) << error.message();

        g_home_dir = m_home.generic_string();
        g_verbose = false;
    }

    void TearDown() override
    {
        std::error_code error;
        fs::current_path(m_old_cwd, error);
        fs::remove_all(m_root, error);

        g_home_dir = m_old_home_dir;
        g_priv_dir = m_old_priv_dir;
        g_verbose = m_old_verbose;
    }

    void expect_current_save_dir(const fs::path &path)
    {
        std::error_code error;
        EXPECT_TRUE(fs::equivalent(path, fs::current_path(), error));
        EXPECT_FALSE(error) << error.message();
        EXPECT_EQ(fs::current_path().generic_string(), g_priv_dir);
    }

    fs::path    m_old_cwd;
    fs::path    m_root;
    fs::path    m_home;
    std::string m_old_home_dir;
    std::string m_old_priv_dir;
    bool        m_old_verbose{};
};

class SaveOptionsTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_def_vals = g_option_def_vals;
        m_old_saved_vals = g_option_saved_vals;

        const OptionCatalog catalog;
        g_option_def_vals = OptionValueList(static_cast<std::size_t>(catalog.option_limit()));
        g_option_saved_vals = OptionValueList(static_cast<std::size_t>(catalog.option_limit()));

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_root = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_root, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_root, error);
        ASSERT_FALSE(error) << error.message();
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_root, error);

        g_option_def_vals = m_old_def_vals;
        g_option_saved_vals = m_old_saved_vals;
    }

    fs::path        m_root;
    OptionValueList m_old_def_vals;
    OptionValueList m_old_saved_vals;
};

} // namespace

TEST_F(CwdCheckTest, defaultsEmptySaveDirectoryToHomeNews)
{
    const fs::path save_dir = m_home / "News";
    g_priv_dir.clear();

    cwd_check();

    EXPECT_TRUE(fs::is_directory(save_dir));
    expect_current_save_dir(save_dir);
}

TEST_F(CwdCheckTest, expandsConfiguredSaveDirectoryBeforeCreatingIt)
{
    const fs::path save_dir = m_home / "Saved";
    g_priv_dir = "~/Saved";

    cwd_check();

    EXPECT_TRUE(fs::is_directory(save_dir));
    expect_current_save_dir(save_dir);
}

TEST_F(SaveOptionsTest, preservesNonOptionTextWhenReplacingOptionsSection)
{
    const fs::path path = m_root / "trnrc";
    std::ofstream{path} << "# before\n"
                        << "[environment]\n"
                        << "TERM = xterm\n"
                        << "\n"
                        << "[options]\n"
                        << "# old option comment\n"
                        << "Bogus Option = old\n"
                        << "# keep this note\n"
                        << "\n"
                        << "[extra]\n"
                        << "value = yes\n";

    save_options(path.string().c_str());

    const std::string output = file_contents(path);

    EXPECT_NE(std::string::npos, output.find("# before\n[environment]\nTERM = xterm\n\n[options]\n"));
    EXPECT_EQ(std::string::npos, output.find("# old option comment"));
    EXPECT_EQ(std::string::npos, output.find("Bogus Option = old"));
    EXPECT_NE(std::string::npos, output.find("# keep this note\n\n[extra]\nvalue = yes\n"));
}
