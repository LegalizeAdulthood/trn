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

#include <ctime>
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

    void expect_basic_environment(const char *net_speed)
    {
        const std::string home = m_home.generic_string();
        const std::string tmp = m_tmp.generic_string();
        const std::string trn = m_trn.generic_string();
        m_env.expect_env("HOME", home.c_str());
        m_env.expect_env("TMPDIR", tmp.c_str());
        expect_login_environment();
        m_env.expect_env("DOTDIR", home.c_str());
        m_env.expect_env("TRNDIR", trn.c_str());
        m_env.expect_env("NETSPEED", net_speed);
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

struct ParsedString
{
    std::string text;
    std::string remaining;
};

ParsedString parse_test_string(std::string text)
{
    std::string      output;
    std::string_view input{text};

    parse_string(output, input);
    return {output, std::string{input}};
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

TEST(InStringTest, findsCaseSensitiveNeedle)
{
    const std::string text{"The quick brown fox"};

    EXPECT_TRUE(in_string(std::string_view{text}, "quick", true));
    EXPECT_FALSE(in_string(std::string_view{text}, "Quick", true));
}

TEST(InStringTest, findsCaseInsensitiveNeedle)
{
    const std::string text{"The quick brown fox"};

    EXPECT_TRUE(in_string(std::string_view{text}, "QUICK", false));
}

TEST(InStringTest, findsEmptyNeedleOnlyInNonEmptyText)
{
    EXPECT_TRUE(in_string(std::string_view{"text"}, "", true));
    EXPECT_FALSE(in_string(std::string_view{}, "", true));
}

TEST(ParseStringTest, decodesBackslashEscapes)
{
    const ParsedString parsed = parse_test_string(R"(alpha\nbeta \101\x42)");

    EXPECT_EQ("alpha\nbeta AB", parsed.text);
    EXPECT_TRUE(parsed.remaining.empty());
}

TEST(ParseStringTest, trimsAfterClosingQuoteBeforeComment)
{
    const ParsedString parsed = parse_test_string("  \"quoted value\" # comment\nnext");

    EXPECT_EQ("quoted value", parsed.text);
    EXPECT_EQ("\nnext", parsed.remaining);
}

TEST(ParseStringTest, preservesTrailingBackslash)
{
    const ParsedString parsed = parse_test_string(R"(value\)");

    EXPECT_EQ(R"(value\)", parsed.text);
    EXPECT_TRUE(parsed.remaining.empty());
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

TEST_F(EnvInitTest, readsNumericNetSpeed)
{
    expect_basic_environment("42");

    (void) env_init(true);

    EXPECT_EQ(42, g_net_speed);
}

TEST_F(EnvInitTest, readsFastNetSpeedShortcut)
{
    expect_basic_environment("fast");

    (void) env_init(true);

    EXPECT_EQ(10, g_net_speed);
}

TEST_F(EnvInitTest, readsSlowNetSpeedShortcut)
{
    expect_basic_environment("slow");

    (void) env_init(true);

    EXPECT_EQ(1, g_net_speed);
}

TEST_F(EnvInitTest, clampsInvalidNetSpeed)
{
    expect_basic_environment("not-a-number");

    (void) env_init(true);

    EXPECT_EQ(1, g_net_speed);
}

TEST(TextToSecsTest, returnsSentinelValues)
{
    constexpr std::time_t default_secs{3660};

    EXPECT_EQ(0, text_to_secs("", default_secs));
    EXPECT_EQ(0, text_to_secs("never", default_secs));
    EXPECT_EQ(2, text_to_secs("missing", default_secs));
    EXPECT_EQ(default_secs, text_to_secs("yes", default_secs));
}

TEST(TextToSecsTest, parsesMinutesByDefault)
{
    EXPECT_EQ(300, text_to_secs("5", 999));
}

TEST(TextToSecsTest, parsesCompositeIntervals)
{
    EXPECT_EQ(93780, text_to_secs("1d, 2h 3m", 999));
}

TEST(TextToSecsTest, parsesSeparatedNumberAndUnit)
{
    EXPECT_EQ(7200, text_to_secs("2 h", 999));
}

TEST(TextToSecsTest, stopsAtNonNumericText)
{
    EXPECT_EQ(7200, text_to_secs("2h bananas", 999));
}

TEST(TextToSecsTest, rejectsUnknownUnits)
{
    EXPECT_EQ(0, text_to_secs("2 bananas", 999));
}

TEST(TextToSecsTest, respectsStringViewExtent)
{
    constexpr std::string_view text{"1h2m", 2};

    EXPECT_EQ(3600, text_to_secs(text, 999));
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
