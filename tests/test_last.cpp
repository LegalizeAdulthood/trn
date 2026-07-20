// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/last.h>

#include <config/common.h>
#include <trn/init.h>
#include <trn/trn.h>
#include <util/env.h>
#include <util/util2.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class LastTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_dot_dir = fs::path{TRN_TEST_DATA_DIR}.parent_path() / "test_runs" /
                    (std::string{"LastTest_"} + testing::UnitTest::GetInstance()->current_test_info()->name());
        m_old_dot_dir = g_dot_dir;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_pid = g_our_pid;
        m_old_last_time = g_last_time;
        m_old_last_active_size = g_last_active_size;
        m_old_last_new_time = g_last_new_time;
        m_old_last_extra_num = g_last_extra_num;

        std::error_code error;
        fs::remove_all(m_dot_dir, error);
        fs::create_directories(m_dot_dir);
        g_dot_dir = m_dot_dir.string();
        last_init();
    }

    void TearDown() override
    {
        last_final();

        g_dot_dir = m_old_dot_dir;
        g_newsgroup_name = m_old_newsgroup_name;
        g_our_pid = m_old_pid;
        g_last_time = m_old_last_time;
        g_last_active_size = m_old_last_active_size;
        g_last_new_time = m_old_last_new_time;
        g_last_extra_num = m_old_last_extra_num;

        std::error_code error;
        fs::remove_all(m_dot_dir, error);
    }

    fs::path last_file() const
    {
        return file_exp(LASTNAME);
    }

    fs::path temp_file() const
    {
        fs::path path{last_file()};
        path += "." + std::to_string(g_our_pid);
        return path;
    }

    fs::path    m_dot_dir;
    std::string m_old_dot_dir;
    std::string m_old_newsgroup_name;
    long        m_old_pid{};
    long        m_old_last_time{};
    long        m_old_last_active_size{};
    long        m_old_last_new_time{};
    long        m_old_last_extra_num{};
};

std::vector<std::string> read_lines(const fs::path &path)
{
    std::ifstream            input{path};
    std::vector<std::string> lines;
    std::string              line;
    while (std::getline(input, line))
    {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

TEST_F(LastTest, writeLastRecordsCurrentState)
{
    g_our_pid = 2468;
    g_newsgroup_name = "comp.lang.apl";
    g_last_time = std::numeric_limits<long>::max();
    g_last_active_size = 1234;
    g_last_new_time = 5678;
    g_last_extra_num = 9;

    write_last();

    EXPECT_EQ((std::vector<std::string>{
                  g_newsgroup_name,
                  std::to_string(g_last_time),
                  std::to_string(g_last_active_size),
                  std::to_string(g_last_new_time),
                  std::to_string(g_last_extra_num),
              }),
              read_lines(last_file()));
    EXPECT_FALSE(fs::exists(temp_file()));
}

TEST_F(LastTest, readLastLoadsCurrentState)
{
    std::ofstream{last_file()} << "comp.lang.apl\n10\n20\n30\n40\n";
    g_last_newsgroup_name = "comp.lang.c++";
    g_last_time = 99;
    g_last_active_size = 0;
    g_last_new_time = 0;
    g_last_extra_num = 0;

    read_last();

    EXPECT_EQ("comp.lang.apl", g_last_newsgroup_name);
    EXPECT_EQ(99, g_last_time);
    EXPECT_EQ(20, g_last_active_size);
    EXPECT_EQ(30, g_last_new_time);
    EXPECT_EQ(40, g_last_extra_num);
}
