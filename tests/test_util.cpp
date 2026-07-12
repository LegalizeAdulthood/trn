// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/init.h>
#include <trn/util.h>
#include <util/env.h>

#include <test_config.h>

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
        char       *name = temp_filename();
        std::string result{name};
        safe_free(name);
        return result;
    }

    std::string m_old_tmp_dir;
    long        m_old_pid{};
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
