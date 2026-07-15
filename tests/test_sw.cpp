// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/sw.h>

#include <trn/opt.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
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

struct SelectorRestorer
{
    SelectionMode     mode;
    SelectionMode     default_mode;
    SelectionMode     thread_mode;
    SelectionSortMode sort;
    SelectionSortMode article_sort;
    SelectionSortMode thread_sort;
    SelectionSortMode newsgroup_sort;
    const char       *mode_string;
    const char       *sort_string;
    int               direction;

    ~SelectorRestorer()
    {
        g_sel_mode = mode;
        g_sel_default_mode = default_mode;
        g_sel_thread_mode = thread_mode;
        g_sel_sort = sort;
        g_sel_art_sort = article_sort;
        g_sel_thread_sort = thread_sort;
        g_sel_newsgroup_sort = newsgroup_sort;
        g_sel_mode_string = mode_string;
        g_sel_sort_string = sort_string;
        g_sel_direction = direction;
    }
};

struct DisplayModeRestorer
{
    std::string group_mode;
    std::string article_mode;
    std::size_t group_mode_index;
    std::size_t article_mode_index;

    ~DisplayModeRestorer()
    {
        g_sel_grp_display_mode = group_mode;
        g_sel_art_display_mode = article_mode;
        g_sel_grp_display_mode_index = group_mode_index;
        g_sel_art_display_mode_index = article_mode_index;
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

TEST(SwitchTest, decodeSelectorModeAlsoSetsSelectorOrder)
{
    SelectorRestorer restore{g_sel_mode,        g_sel_default_mode, g_sel_thread_mode,    g_sel_sort,
                             g_sel_art_sort,    g_sel_thread_sort,  g_sel_newsgroup_sort, g_sel_mode_string,
                             g_sel_sort_string, g_sel_direction};

    decode_switch("-OaD");

    EXPECT_EQ(SM_ARTICLE, g_sel_default_mode);
    EXPECT_STREQ("reverse date", option_value(OI_NEWS_SEL_ORDER));
}

TEST(SwitchTest, selectorDisplayStyleOptionsReturnConfiguredOrder)
{
    DisplayModeRestorer restore{g_sel_grp_display_mode, g_sel_art_display_mode, g_sel_grp_display_mode_index,
                                g_sel_art_display_mode_index};

    set_option(OI_NEWSGROUP_SEL_STYLES, "mls");
    set_option(OI_NEWS_SEL_STYLES, "dslm");

    EXPECT_EQ('m', current_group_display_mode());
    EXPECT_EQ('d', current_article_display_mode());
    EXPECT_STREQ("mls", option_value(OI_NEWSGROUP_SEL_STYLES));
    EXPECT_STREQ("dslm", option_value(OI_NEWS_SEL_STYLES));
}
