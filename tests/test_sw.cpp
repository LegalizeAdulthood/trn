// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/sw.h>

#include <trn/terminal.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

struct ModeRestorer
{
    MinorMode mode;

    ~ModeRestorer()
    {
        g_mode = mode;
    }
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

TEST(SwitchTest, writeInitEnvironmentWritesSavedExports)
{
    ModeRestorer restore{g_mode};
    g_mode = MM_INITIALIZING;

    const fs::path  output_path{TRN_TEST_TMP_DIR "/switch-init-env.txt"};
    std::error_code error;
    fs::remove(output_path, error);

    for (int i = 0; i < 35; i++)
    {
        const std::string command{"-ETRN_SW_INIT_ENV_" + std::to_string(i) + "=value" + std::to_string(i)};
        decode_switch(command.c_str());
    }

    std::FILE *fp = std::fopen(output_path.string().c_str(), "w");
    ASSERT_NE(nullptr, fp);
    write_init_environment(fp);
    std::fclose(fp);

    const std::vector<std::string> lines = read_lines(output_path);
    ASSERT_EQ(35, lines.size());
    for (int i = 0; i < 35; i++)
    {
        EXPECT_EQ("TRN_SW_INIT_ENV_" + std::to_string(i) + "=value" + std::to_string(i), lines[i]);
    }
}
