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
    }

    void TearDown() override
    {
        g_home_dir = m_old_home_dir;
    }

    std::string m_old_home_dir;
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
