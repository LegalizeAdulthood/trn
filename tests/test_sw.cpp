// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/sw.h>

#include <config/env.h>
#include <trn/datasrc.h>
#include <trn/init.h>
#include <trn/ng.h>
#include <trn/opt.h>
#include <trn/rcstuff.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/terminal.h>
#include <trn/util.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

constexpr std::string_view VALUE_ENV_VAR{"TRN_TEST_SW_VALUE_ENV"};
constexpr std::string_view EMPTY_ENV_VAR{"TRN_TEST_SW_EMPTY_ENV"};
constexpr std::string_view EQUALS_ENV_VAR{"TRN_TEST_SW_EQUALS_ENV"};

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
    std::string_view  mode_string;
    std::string_view  sort_string;
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

struct SwitchFlagRestorer
{
    bool check_flag;
    bool unsafe_rc_saves;

    ~SwitchFlagRestorer()
    {
        g_check_flag = check_flag;
        g_unsafe_rc_saves = unsafe_rc_saves;
    }
};

class EnvironmentSwitchTest : public testing::Test
{
protected:
    void SetUp() override
    {
        unset_env_var(VALUE_ENV_VAR);
        unset_env_var(EMPTY_ENV_VAR);
        unset_env_var(EQUALS_ENV_VAR);
    }

    void TearDown() override
    {
        unset_env_var(VALUE_ENV_VAR);
        unset_env_var(EMPTY_ENV_VAR);
        unset_env_var(EQUALS_ENV_VAR);
    }
};

struct MouseButtonsRestorer
{
    MouseButtonList buttons;

    ~MouseButtonsRestorer()
    {
        g_univ_sel_btns = buttons;
    }
};

struct RefetchRestorer
{
    std::time_t refetch_secs;

    ~RefetchRestorer()
    {
        g_def_refetch_secs = refetch_secs;
    }
};

struct HeaderListRestorer
{
    std::array<HeaderTypeFlags, HEAD_LAST> flags;
    std::vector<UserHeaderType>            user_header_type;
    short                                  user_header_type_index[26];
    int                                    user_header_type_count;
    int                                    user_header_type_max;

    HeaderListRestorer()
    {
        for (int i = 0; i < HEAD_LAST; i++)
        {
            flags[static_cast<std::size_t>(i)] = g_header_type[i].flags;
        }
        user_header_type = g_user_header_type;
        std::memcpy(user_header_type_index, g_user_header_type_index, sizeof user_header_type_index);
        user_header_type_count = g_user_header_type_count;
        user_header_type_max = g_user_header_type_max;
    }

    ~HeaderListRestorer()
    {
        for (int i = 0; i < HEAD_LAST; i++)
        {
            g_header_type[i].flags = flags[static_cast<std::size_t>(i)];
        }
        g_user_header_type = user_header_type;
        std::memcpy(g_user_header_type_index, user_header_type_index, sizeof user_header_type_index);
        g_user_header_type_count = user_header_type_count;
        g_user_header_type_max = user_header_type_max;
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

TEST(SwitchFileTest, readsSwitchesBeyondOldBufferCapacity)
{
    SwitchFlagRestorer restore{g_check_flag, g_unsafe_rc_saves};
    g_check_flag = false;
    g_unsafe_rc_saves = false;

    const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
    const fs::path           output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
    std::error_code          error;
    fs::remove_all(output_dir, error);
    fs::create_directories(output_dir, error);
    ASSERT_FALSE(error) << error.message();
    const fs::path switch_file = output_dir / "switches";
    std::ofstream  output{switch_file};
    ASSERT_TRUE(output);
    output << std::string(TCBUF_SIZE, ' ') << "# comment\n\"-c\"\n-U\n";
    output.close();

    sw_file(switch_file.string());

    EXPECT_TRUE(g_check_flag);
    EXPECT_TRUE(g_unsafe_rc_saves);
    fs::remove_all(output_dir, error);
}

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

TEST_F(EnvironmentSwitchTest, decodeEnvironmentSwitchSetsValue)
{
    std::string command{"-E="};
    command.append(VALUE_ENV_VAR);
    command += "=value";

    decode_switch(command.c_str());

    EXPECT_EQ("value", get_env_var(VALUE_ENV_VAR));
}

TEST_F(EnvironmentSwitchTest, decodeEnvironmentSwitchSetsEmptyValue)
{
    std::string command{"-E="};
    command.append(EMPTY_ENV_VAR);

    decode_switch(command.c_str());

    EXPECT_TRUE(get_env_var(EMPTY_ENV_VAR).empty());
}

TEST_F(EnvironmentSwitchTest, decodeEnvironmentSwitchPreservesEqualsInValue)
{
    std::string command{"-E="};
    command.append(EQUALS_ENV_VAR);
    command += "=left=right";

    decode_switch(command.c_str());

    EXPECT_EQ("left=right", get_env_var(EQUALS_ENV_VAR));
}

TEST(SwitchTest, decodeSelectorModeAlsoSetsSelectorOrder)
{
    SelectorRestorer restore{g_sel_mode,        g_sel_default_mode, g_sel_thread_mode,    g_sel_sort,
                             g_sel_art_sort,    g_sel_thread_sort,  g_sel_newsgroup_sort, g_sel_mode_string,
                             g_sel_sort_string, g_sel_direction};

    decode_switch("-OaD");

    EXPECT_EQ(SM_ARTICLE, g_sel_default_mode);
    EXPECT_EQ("articles", option_value(OI_NEWS_SEL_MODE));
    EXPECT_EQ("reverse date", option_value(OI_NEWS_SEL_ORDER));
}

TEST(SwitchTest, selectorDisplayStyleOptionsReturnConfiguredOrder)
{
    DisplayModeRestorer restore{g_sel_grp_display_mode, g_sel_art_display_mode, g_sel_grp_display_mode_index,
                                g_sel_art_display_mode_index};

    set_option(OI_NEWSGROUP_SEL_STYLES, "mls");
    set_option(OI_NEWS_SEL_STYLES, "dslm");

    EXPECT_EQ('m', current_group_display_mode());
    EXPECT_EQ('d', current_article_display_mode());
    EXPECT_EQ("mls", option_value(OI_NEWSGROUP_SEL_STYLES));
    EXPECT_EQ("dslm", option_value(OI_NEWS_SEL_STYLES));
}

TEST(OptionValueTest, expandsMouseButtonsWithoutChangingParsedStorage)
{
    MouseButtonsRestorer restore{g_univ_sel_btns};
    g_univ_sel_btns.clear();
    set_option(OI_UNIV_SEL_BTNS, "[Top]^ [Quit]q x");

    EXPECT_EQ("[Top]^ [Quit]q x", option_value(OI_UNIV_SEL_BTNS));
    EXPECT_EQ("[Top]^ [Quit]q x", option_value(OI_UNIV_SEL_BTNS));
}

TEST(OptionValueTest, formatsDefaultRefetchTime)
{
    RefetchRestorer restore{g_def_refetch_secs};
    g_def_refetch_secs = 93780;

    EXPECT_EQ("1 day, 2 hours, 3 minutes", option_value(OI_DEFAULT_REFETCH_TIME));
}

TEST(OptionValueTest, formatsHeaderHidingList)
{
    HeaderListRestorer restore;
    g_user_header_type_max = 3;
    g_user_header_type.assign(static_cast<std::size_t>(g_user_header_type_max), UserHeaderType{});
    g_user_header_type[0].name = "*";
    g_user_header_type[1].name = "x-hidden";
    g_user_header_type[1].flags = HT_HIDE;
    g_user_header_type[2].name = "x-visible";
    g_user_header_type_count = 3;

    EXPECT_EQ("x-hidden,!x-visible", option_value(OI_HEADER_HIDING));
}

TEST(OptionValueTest, formatsHeaderMagicList)
{
    HeaderListRestorer restore;
    g_header_type[DATE_LINE].flags &= ~HT_MAGIC;
    g_header_type[FROM_LINE].flags |= HT_MAGIC;

    EXPECT_EQ("!date,from", option_value(OI_HEADER_MAGIC));
}
