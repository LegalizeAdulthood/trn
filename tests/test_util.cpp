// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/init.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/env-internal.h>
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
#include <tuple>

namespace
{

namespace fs = std::filesystem;

using GreySpaceCase = std::pair<std::string_view, bool>;

class GreySpaceTest : public testing::TestWithParam<GreySpaceCase>
{
protected:
    void SetUp() override;
    void TearDown() override;

    std::string m_old_input;
    std::string m_old_output;
    std::string m_text;
};

void GreySpaceTest::SetUp()
{
    m_old_input = input_charset_name();
    m_old_output = output_charset_name();
    utf_init(CHARSET_NAME_UTF8, CHARSET_NAME_UTF8);
    m_text = GetParam().first;
}

void GreySpaceTest::TearDown()
{
    utf_init(m_old_input, m_old_output);
}

TEST_P(GreySpaceTest, classifiesTheCharacter)
{
    const bool result = at_grey_space(m_text);

    EXPECT_EQ(GetParam().second, result);
}

INSTANTIATE_TEST_SUITE_P(WhitespaceControlAndNormal, GreySpaceTest,
                         testing::Values(GreySpaceCase{" ", true}, GreySpaceCase{"\001", true},
                                         GreySpaceCase{"A", false}));

using NeedleCase = std::tuple<std::string_view, std::string_view, bool>;

class SensitiveNeedleTest : public testing::TestWithParam<NeedleCase>
{
protected:
    void SetUp() override;

    std::string m_text;
    std::string m_needle;
};

void SensitiveNeedleTest::SetUp()
{
    m_text = std::get<0>(GetParam());
    m_needle = std::get<1>(GetParam());
}

using BerkeleyNameCase = std::pair<std::string_view, std::string_view>;

class BerkeleyNameTest : public testing::TestWithParam<BerkeleyNameCase>
{
protected:
    void SetUp() override
    {
        m_gecos = GetParam().first;
    }

    std::string m_gecos;
};

TEST(ShellWaitStatusTest, decodesExitStatus)
{
#ifdef USE_WIFSTAT
    EXPECT_EQ(42, shell_wait_status_exit_code(42 << 8));
#else
    EXPECT_EQ(42, shell_wait_status_exit_code(42));
#endif
}

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

#ifdef USE_WINDOWS_ENVIRONMENT_NAMES
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

using StringEscapeCase = std::tuple<std::string_view, std::string_view>;

class StringEscapeTest : public testing::TestWithParam<StringEscapeCase>
{
protected:
    void SetUp() override
    {
        m_text = std::get<0>(GetParam());
    }

    std::string m_text;
};

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

TEST_P(SensitiveNeedleTest, findsTheCaseSensitiveNeedle)
{
    const bool result = in_string(m_text, m_needle, true);

    EXPECT_EQ(std::get<2>(GetParam()), result);
}

INSTANTIATE_TEST_SUITE_P(MatchMismatchAndEmptyNeedle, SensitiveNeedleTest,
                         testing::Values(NeedleCase{"The quick brown fox", "quick", true},
                                         NeedleCase{"The quick brown fox", "Quick", false},
                                         NeedleCase{"text", "", true}, NeedleCase{"", "", false}));

TEST(InStringTest, findsCaseInsensitiveNeedle)
{
    const std::string text{"The quick brown fox"};

    EXPECT_TRUE(in_string(std::string_view{text}, "QUICK", false));
}

TEST_P(StringEscapeTest, decodesOneEscapeForm)
{
    const ParsedString parsed = parse_test_string(m_text);

    EXPECT_EQ(std::get<1>(GetParam()), parsed.text);
    EXPECT_TRUE(parsed.remaining.empty());
}

INSTANTIATE_TEST_SUITE_P(NewlineOctalAndHex, StringEscapeTest,
                         testing::Values(StringEscapeCase{R"(alpha\nbeta)", "alpha\nbeta"},
                                         StringEscapeCase{R"(\101)", "A"}, StringEscapeCase{R"(\x42)", "B"}));

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

TEST_P(BerkeleyNameTest, extractsTheFullName)
{
    const std::string result = passwd_full_name(m_gecos, "guest", PasswdNameFormat::Berkeley);

    EXPECT_EQ(GetParam().second, result);
}

INSTANTIATE_TEST_SUITE_P(DelimitersAndAmpersands, BerkeleyNameTest,
                         testing::Values(BerkeleyNameCase{"Mister Guest User,Room 42", "Mister Guest User"},
                                         BerkeleyNameCase{"Mister Guest User;Room 42", "Mister Guest User"},
                                         BerkeleyNameCase{"& User", "Guest User"},
                                         BerkeleyNameCase{R"(\& User)", "& User"}));

TEST(PasswdFullNameTest, usgKeepsNameBetweenHyphenAndParen)
{
    EXPECT_EQ("Mister Guest User", passwd_full_name("000-Mister Guest User(000)", "guest", PasswdNameFormat::Usg));
}

#ifndef PASS_NAMES
TEST_F(EnvInitTest, readsRealNameFromFullnameFile)
{
    const std::string home = m_home.generic_string();
    const std::string tmp = m_tmp.generic_string();
    const std::string trn = m_trn.generic_string();
    g_dot_dir = home;
    std::ofstream{file_exp(FULLNAMEFILE)} << "Casey Writer\n";
    g_dot_dir.clear();
    m_env.expect_env("HOME", home.c_str());
    m_env.expect_env("TMPDIR", tmp.c_str());
    expect_login_environment();
    m_env.expect_env("DOTDIR", home.c_str());
    m_env.expect_env("TRNDIR", trn.c_str());
    m_env.expect_env("NETSPEED", "5");

    (void) env_init(true);

    EXPECT_EQ("Casey Writer", g_real_name);
}
#endif

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

using DurationParseCase = std::pair<std::string_view, std::time_t>;

class DurationParseTest : public testing::TestWithParam<DurationParseCase>
{
protected:
    void SetUp() override
    {
        m_text = GetParam().first;
    }

    std::string m_text;
};

TEST_P(DurationParseTest, returnsTheSentinelValue)
{
    const std::time_t result = text_to_secs(m_text, 3660);

    EXPECT_EQ(GetParam().second, result);
}

INSTANTIATE_TEST_SUITE_P(Sentinels, DurationParseTest,
                         testing::Values(DurationParseCase{"", 0}, DurationParseCase{"never", 0},
                                         DurationParseCase{"missing", 2}, DurationParseCase{"yes", 3660}));

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

using DurationTextCase = std::pair<std::time_t, std::string_view>;

class DurationTextTest : public testing::TestWithParam<DurationTextCase>
{
};

TEST_P(DurationTextTest, formatsTheDuration)
{
    const std::string result = secs_to_text(GetParam().first);

    EXPECT_EQ(GetParam().second, result);
}

INSTANTIATE_TEST_SUITE_P(SentinelsAndCompositeIntervals, DurationTextTest,
                         testing::Values(DurationTextCase{0, "never"}, DurationTextCase{1, "never"},
                                         DurationTextCase{2, "missing"},
                                         DurationTextCase{93780, "1 day, 2 hours, 3 minutes"},
                                         DurationTextCase{176400, "2 days, 1 hour"}));

using NumericConditionCase = std::pair<std::string_view, bool>;

class NumericConditionTest : public testing::TestWithParam<NumericConditionCase>
{
protected:
    void SetUp() override
    {
        m_text = GetParam().first;
    }

    std::string m_text;
};

TEST_P(NumericConditionTest, evaluatesTheComparison)
{
    const bool result = check_ini_cond(m_text);

    EXPECT_EQ(GetParam().second, result);
}

INSTANTIATE_TEST_SUITE_P(OperatorsNegationAndLenience, NumericConditionTest,
                         testing::Values(NumericConditionCase{"5 > 3", true}, NumericConditionCase{"5 < 3", false},
                                         NumericConditionCase{"5 >= 5", true}, NumericConditionCase{"5!<3", true},
                                         NumericConditionCase{"5!>3", false}, NumericConditionCase{" 5 > 3junk", true},
                                         NumericConditionCase{"+5 > 3", true}));
