// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include "test_generated_config.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace
{

std::string file_contents(const char *filename)
{
    std::ifstream file{filename, std::ios::binary};

    if (!file)
    {
        return {};
    }
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

void expect_file_contains(const char *filename, std::string_view expected)
{
    const std::string contents = file_contents(filename);

    ASSERT_FALSE(contents.empty()) << filename;
    EXPECT_NE(std::string::npos, contents.find(expected)) << filename;
}

void expect_file_not_contains(const char *filename, std::string_view expected)
{
    const std::string contents = file_contents(filename);

    ASSERT_FALSE(contents.empty()) << filename;
    EXPECT_EQ(std::string::npos, contents.find(expected)) << filename;
}

struct GeneratedArtifactCase
{
    const char *filename;
    bool        available{true};
};

void PrintTo(const GeneratedArtifactCase &value, std::ostream *output)
{
    *output << value.filename;
}

class GeneratedArtifactTest : public testing::TestWithParam<GeneratedArtifactCase>
{
protected:
    void SetUp() override;

    std::ifstream m_file;
};

void GeneratedArtifactTest::SetUp()
{
    if (!GetParam().available)
    {
        GTEST_SKIP() << "This artifact is not generated on this platform.";
    }
    m_file.open(GetParam().filename, std::ios::binary);
}

void expect_helper_has_no_metaconfig_residue(const char *filename)
{
    static constexpr std::string_view residues[] = {
        "@SHELL_START@",
        "@DIFF_PROGRAM@",
        "@ED_PROGRAM@",
        "@ISPELL_PROGRAM@",
        "@ISPELL_OPTIONS@",
        "@SPELL_PROGRAM@",
        "@PAGER@",
        "@NEWS_LIB@",
        "@PRIVATE_LIB@",
        "@BIN_DIR@",
        "@USE_INEWS@",
        "@DEFAULT_EDITOR@",
        "@NAME_TYPE@",
        "@ORG_NAME@",
        "@PNEWS_ORG_NAME@",
        "@POSTING_ORG_NAME@",
        "@LOCAL_DIST@",
        "@ORGANIZATION_DIST@",
        "@MULTISTATE_DIST@",
        "@CITY_DIST@",
        "@STATE_DIST@",
        "@COUNTRY_DIST@",
        "@CONTINENT_DIST@",
        "spitshell",
        "GROK",
        "NO!SUBS",
        "eunicefix",
        "nidump",
        "ypmatch",
        "${mailer-",
        "$test ",
        "$sed ",
        "$echo ",
        "$cat ",
        "$grep ",
        "$egrep ",
        "$cp ",
        "$rm ",
        "$sleep ",
        "$who ",
    };

    for (const std::string_view residue : residues)
    {
        expect_file_not_contains(filename, residue);
    }
}

struct GeneratedTextCase
{
    const char *filename;
    std::string expected;
    bool        available{true};
};

void PrintTo(const GeneratedTextCase &value, std::ostream *output)
{
    *output << value.expected.substr(0, 80);
}

class GeneratedDefinitionTest : public testing::TestWithParam<GeneratedTextCase>
{
protected:
    void SetUp() override;

    std::string m_contents;
};

void GeneratedDefinitionTest::SetUp()
{
    if (!GetParam().available)
    {
        GTEST_SKIP() << "This artifact is not generated on this platform.";
    }
    m_contents = file_contents(GetParam().filename);
    ASSERT_FALSE(m_contents.empty()) << GetParam().filename;
}

GeneratedTextCase config_define_case(const char *filename, std::string_view name, bool defined)
{
    const std::string expected =
        defined ? std::string{"#define "} + std::string{name} : std::string{"/* #undef "} + std::string{name} + " */";

    return {filename, expected};
}

void expect_config_define(const char *filename, std::string_view name, bool defined)
{
    expect_file_contains(filename, config_define_case(filename, name, defined).expected);
}

GeneratedTextCase config_string_case(const char *filename, std::string_view name, std::string_view value)
{
    const std::string literal = name == "NEWS_POSTER" || name == "MAIL_POSTER" || name == "FORWARD_POSTER"
                                    ? " R\"trn(" + std::string{value} + ")trn\""
                                    : " \"" + std::string{value} + "\"";
    const std::string expected = std::string{"#define "} + std::string{name} + literal;

    return {filename, expected};
}

void expect_config_string(const char *filename, std::string_view name, std::string_view value)
{
    expect_file_contains(filename, config_string_case(filename, name, value).expected);
}

GeneratedTextCase config_char_case(const char *filename, std::string_view name, std::string_view value)
{
    const std::string expected = std::string{"#define "} + std::string{name} + " '" + std::string{value} + "'";

    return {filename, expected};
}

void expect_news_source_defaults(const char *filename, bool has_local_spool, std::string_view server_name,
                                 std::string_view active, std::string_view group_desc, std::string_view subscriptions,
                                 std::string_view news_spool, std::string_view overview_dir,
                                 std::string_view overview_fmt)
{
    expect_config_string(filename, "SERVER_NAME", server_name);
    expect_config_define(filename, "HAS_LOCAL_SPOOL", has_local_spool);
    expect_config_string(filename, "ACTIVE", active);
    expect_config_string(filename, "GROUP_DESC", group_desc);
    expect_config_string(filename, "SUBSCRIPTIONS", subscriptions);
    expect_config_string(filename, "NEWS_SPOOL", news_spool);
    expect_config_string(filename, "OVERVIEW_DIR", overview_dir);
    expect_config_string(filename, "OVERVIEW_FMT", overview_fmt);
}

bool cmake_bool(std::string_view value)
{
    return value == "1" || value == "ON" || value == "TRUE";
}

bool windows_generated_helpers()
{
    return cmake_bool(TRN_TEST_GENERATED_WINDOWS_HELPERS);
}

using PostingCommandCase = std::pair<std::string_view, std::string_view>;

class WindowsPostingCommandTest : public testing::TestWithParam<PostingCommandCase>
{
protected:
    void SetUp() override;
};

void WindowsPostingCommandTest::SetUp()
{
    if (!windows_generated_helpers())
    {
        GTEST_SKIP() << "Windows posting defaults apply only on Windows.";
    }
}

bool generated_pnews_header()
{
    return std::string_view{TRN_TEST_GENERATED_PNEWS_HEADER_FILE}.empty() == false;
}

void expect_name_parsing_policy(const char *filename, std::string_view name_type)
{
    expect_config_define(filename, "PASS_NAMES", name_type == "bsd" || name_type == "usg");
    expect_config_define(filename, "BERKELEY_NAMES", name_type == "bsd");
}

} // namespace

TEST_P(GeneratedDefinitionTest, emitsTheExpectedDefinition)
{
    const std::size_t position = m_contents.find(GetParam().expected);

    EXPECT_NE(std::string::npos, position) << GetParam().filename;
}

INSTANTIATE_TEST_SUITE_P(
    InputAndSignalProbes, GeneratedDefinitionTest,
    testing::Values(
        config_define_case(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "I_SGTTY", cmake_bool(TRN_TEST_I_SGTTY)),
        config_define_case(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "HAS_RDCHK", cmake_bool(TRN_TEST_HAS_RDCHK)),
        config_define_case(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "HAS_SIGBLOCK", cmake_bool(TRN_TEST_HAS_SIGBLOCK)),
        config_define_case(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "HAS_SIGHOLD", cmake_bool(TRN_TEST_HAS_SIGHOLD))));

TEST(GeneratedConfigTest, waitStatusProbeMatchesPlatformCheck)
{
    expect_file_not_contains(TRN_TEST_WAIT_STATUS_PROBE_CONFIG, "UNION_WAIT");
    expect_config_define(TRN_TEST_WAIT_STATUS_PROBE_CONFIG, "USE_WIFSTAT", cmake_bool(TRN_TEST_USE_WIFSTAT));
}

TEST(GeneratedConfigTest, termlibProbeMatchesPlatformCheck)
{
    expect_config_define(TRN_TEST_TERMLIB_PROBE_CONFIG, "HAS_TERMLIB", cmake_bool(TRN_TEST_DEFAULT_HAS_TERMLIB));
}

INSTANTIATE_TEST_SUITE_P(
    PlatformPolicies, GeneratedDefinitionTest,
    testing::Values(config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "MSDOS", false),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "USE_CONIO_TERMINAL_INPUT",
                                       cmake_bool(TRN_TEST_USE_CONIO_TERMINAL_INPUT)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "USE_BUILTIN_TERMINAL_CAPABILITIES",
                                       cmake_bool(TRN_TEST_USE_BUILTIN_TERMINAL_CAPABILITIES)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "USE_SPAWN_SHELL",
                                       cmake_bool(TRN_TEST_USE_SPAWN_SHELL)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "USE_UNDERSCORE_SLEEP",
                                       cmake_bool(TRN_TEST_USE_UNDERSCORE_SLEEP)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "USE_WINDOWS_ENVIRONMENT_NAMES",
                                       cmake_bool(TRN_TEST_USE_WINDOWS_ENVIRONMENT_NAMES)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "USE_WINDOWS_FILENAME_RULES",
                                       cmake_bool(TRN_TEST_USE_WINDOWS_FILENAME_RULES)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "CHECK_NEWSRC_LOCK_PROCESS",
                                       cmake_bool(TRN_TEST_CHECK_NEWSRC_LOCK_PROCESS)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "SET_ARTICLE_TEMP_FILE_PERMISSIONS",
                                       cmake_bool(TRN_TEST_SET_ARTICLE_TEMP_FILE_PERMISSIONS)),
                    config_define_case(TRN_TEST_PLATFORM_POLICY_DEFAULT_CONFIG, "PRESERVE_NEWSRC_FILE_PERMISSIONS",
                                       cmake_bool(TRN_TEST_PRESERVE_NEWSRC_FILE_PERMISSIONS))));

TEST(GeneratedConfigTest, activeTimesDefaultIsNone)
{
    expect_file_contains(TRN_TEST_ACTIVE_TIMES_DEFAULT_CONFIG, "#define ACTIVE_TIMES \"none\"");
}

TEST(GeneratedConfigTest, activeTimesConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_ACTIVE_TIMES_CONFIGURED_CONFIG, "#define ACTIVE_TIMES \"%x/active.times\"");
}

TEST(GeneratedConfigTest, newsSourceDetectedDefaultsAreEmitted)
{
    expect_news_source_defaults(TRN_TEST_NEWS_SOURCE_DEFAULT_CONFIG, cmake_bool(TRN_TEST_DEFAULT_HAS_LOCAL_SPOOL),
                                TRN_TEST_DEFAULT_SERVER_NAME, TRN_TEST_DEFAULT_ACTIVE, TRN_TEST_DEFAULT_GROUP_DESC,
                                TRN_TEST_DEFAULT_SUBSCRIPTIONS, TRN_TEST_DEFAULT_NEWS_SPOOL,
                                TRN_TEST_DEFAULT_OVERVIEW_DIR, TRN_TEST_DEFAULT_OVERVIEW_FMT);
}

TEST(GeneratedConfigTest, newsSourceNntpModeDefaultsAreEmitted)
{
    expect_news_source_defaults(TRN_TEST_NEWS_SOURCE_NNTP_CONFIG, false, "news.example.org", "remote", "none", "",
                                "none", "remote", "none");
}

TEST(GeneratedConfigTest, newsSourceLocalModeDefaultsAreEmitted)
{
    expect_news_source_defaults(TRN_TEST_NEWS_SOURCE_LOCAL_CONFIG, true, "local", "%x/active", "%x/newsgroups",
                                "%x/subscriptions", "/usr/spool/news", "/usr/spool/news", "%x/overview.fmt");
}

TEST(GeneratedConfigTest, newsSourceBothModeDefaultsAreEmitted)
{
    expect_news_source_defaults(TRN_TEST_NEWS_SOURCE_BOTH_CONFIG, true, "news.example.org", "remote", "none", "",
                                "none", "remote", "none");
}

TEST(GeneratedConfigTest, activeConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_ACTIVE_CONFIGURED_CONFIG, "#define ACTIVE \"%x/test-active\"");
}

TEST(GeneratedConfigTest, mailFileDefaultMatchesHistoricalFallback)
{
    expect_file_contains(TRN_TEST_MAIL_FILE_DEFAULT_CONFIG, "#define MAIL_FILE \"/usr/spool/mail/%L\"");
}

TEST(GeneratedConfigTest, mailFileConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_MAIL_FILE_CONFIGURED_CONFIG, "#define MAIL_FILE \"%./mailbox\"");
}

TEST(GeneratedConfigTest, mimecapDefaultMatchesPlatformPathList)
{
    expect_config_string(TRN_TEST_MIMECAP_DEFAULT_CONFIG, "MIMECAP", TRN_TEST_DEFAULT_MIMECAP);
}

TEST(GeneratedConfigTest, newsLibDefaultMatchesPlatformDefault)
{
    const std::string expected = std::string{"#define NEWS_LIB \""} + TRN_TEST_DEFAULT_NEWS_LIB + "\"";

    expect_file_contains(TRN_TEST_NEWS_LIB_DEFAULT_CONFIG, expected);
}

TEST(GeneratedConfigTest, newsLibConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_NEWS_LIB_CONFIGURED_CONFIG, "#define NEWS_LIB \"/var/lib/trn/news\"");
}

TEST(GeneratedConfigTest, groupDescConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_GROUP_DESC_CONFIGURED_CONFIG, "#define GROUP_DESC \"/var/lib/trn/newsgroups\"");
}

TEST(GeneratedConfigTest, subscriptionsConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_SUBSCRIPTIONS_CONFIGURED_CONFIG,
                         "#define SUBSCRIPTIONS \"/var/lib/trn/subscriptions\"");
}

TEST(GeneratedConfigTest, overviewDirConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_OVERVIEW_DIR_CONFIGURED_CONFIG, "#define OVERVIEW_DIR \"/var/spool/news/overview\"");
}

TEST(GeneratedConfigTest, overviewFmtConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_OVERVIEW_FMT_CONFIGURED_CONFIG,
                         "#define OVERVIEW_FMT \"/var/lib/trn/overview.fmt\"");
}

TEST(GeneratedConfigTest, postingHostMatchSegmentsDefaultMatchesHistoricalFallback)
{
    const std::string expected =
        std::string{"#define POSTING_HOST_MATCH_SEGMENTS "} + TRN_TEST_DEFAULT_POSTING_HOST_MATCH_SEGMENTS;

    expect_file_contains(TRN_TEST_POSTING_HOST_MATCH_SEGMENTS_DEFAULT_CONFIG, expected);
}

TEST(GeneratedConfigTest, postingHostMatchSegmentsConfiguredCountIsEmitted)
{
    expect_file_contains(TRN_TEST_POSTING_HOST_MATCH_SEGMENTS_CONFIGURED_CONFIG,
                         "#define POSTING_HOST_MATCH_SEGMENTS 4");
}

TEST(GeneratedConfigTest, nameParsingDefaultMatchesPlatformPolicy)
{
    expect_name_parsing_policy(TRN_TEST_NAME_PARSING_DEFAULT_CONFIG, TRN_TEST_DEFAULT_NAME_TYPE);
}

TEST(GeneratedConfigTest, nameParsingCanUseFullnameFile)
{
    expect_name_parsing_policy(TRN_TEST_NAME_PARSING_FULLNAME_FILE_CONFIG, "other");
}

TEST(GeneratedConfigTest, nameParsingCanUseUsgPasswdFormat)
{
    expect_name_parsing_policy(TRN_TEST_NAME_PARSING_USG_CONFIG, "usg");
}

TEST(GeneratedConfigTest, nameParsingCanUseBerkeleyPasswdFormat)
{
    expect_name_parsing_policy(TRN_TEST_NAME_PARSING_BERKELEY_CONFIG, "bsd");
}

INSTANTIATE_TEST_SUITE_P(AccountDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_ACCOUNT_NAME_FILES_DEFAULT_CONFIG, "PASSWORD_FILE",
                                                            TRN_TEST_DEFAULT_PASSWORD_FILE),
                                         GeneratedTextCase{TRN_TEST_ACCOUNT_NAME_FILES_DEFAULT_CONFIG,
                                                           std::string{"#define LOGIN_DIR_FIELD "} +
                                                               TRN_TEST_DEFAULT_LOGIN_DIR_FIELD},
                                         config_string_case(TRN_TEST_ACCOUNT_NAME_FILES_DEFAULT_CONFIG, "FULLNAMEFILE",
                                                            TRN_TEST_DEFAULT_FULLNAMEFILE)));

INSTANTIATE_TEST_SUITE_P(ConfiguredAccountFiles, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_ACCOUNT_NAME_FILES_CONFIGURED_CONFIG,
                                                            "PASSWORD_FILE", "/var/lib/trn/passwd"),
                                         GeneratedTextCase{TRN_TEST_ACCOUNT_NAME_FILES_CONFIGURED_CONFIG,
                                                           "#define LOGIN_DIR_FIELD 7"},
                                         config_string_case(TRN_TEST_ACCOUNT_NAME_FILES_CONFIGURED_CONFIG,
                                                            "FULLNAMEFILE", "%./profile-name")));

INSTANTIATE_TEST_SUITE_P(
    UserStateDefaults, GeneratedDefinitionTest,
    testing::Values(config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "TRNDIR", TRN_TEST_DEFAULT_TRNDIR),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "TRNMACRO", TRN_TEST_DEFAULT_TRNMACRO),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "RNMACRO", TRN_TEST_DEFAULT_RNMACRO),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "RCNAME", TRN_TEST_DEFAULT_RCNAME),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "LOCKNAME", TRN_TEST_DEFAULT_LOCKNAME),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "LASTNAME", TRN_TEST_DEFAULT_LASTNAME),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "SIGNATURE_FILE",
                                       TRN_TEST_DEFAULT_SIGNATURE_FILE),
                    config_string_case(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "NNTP_AUTH_FILE",
                                       TRN_TEST_DEFAULT_NNTP_AUTH_FILE)));

INSTANTIATE_TEST_SUITE_P(
    ConfiguredUserState, GeneratedDefinitionTest,
    testing::Values(
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "TRNDIR", "%./trn-test"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "TRNMACRO", "%+/test-macros"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "RNMACRO", "%./test-rnmac"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "RCNAME", "%./test-newsrc"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "LOCKNAME", "%./test-rnlock"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "LASTNAME", "%./test-rnlast"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "SIGNATURE_FILE", "%./test-signature"),
        config_string_case(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "NNTP_AUTH_FILE", "%./test-nntpauth")));

INSTANTIATE_TEST_SUITE_P(
    DataFileDefaults, GeneratedDefinitionTest,
    testing::Values(
        config_string_case(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "GLOBAL_INIT", TRN_TEST_DEFAULT_GLOBAL_INIT),
        config_string_case(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "DEFACCESS", TRN_TEST_DEFAULT_DEFACCESS),
        config_string_case(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "TRNACCESS", TRN_TEST_DEFAULT_TRNACCESS),
        config_string_case(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "NEWSNEWSNAME", TRN_TEST_DEFAULT_NEWSNEWSNAME)));

INSTANTIATE_TEST_SUITE_P(
    ConfiguredDataFiles, GeneratedDefinitionTest,
    testing::Values(config_string_case(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "GLOBAL_INIT", "%X/test-INIT"),
                    config_string_case(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "DEFACCESS", "%X/test-access.def"),
                    config_string_case(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "TRNACCESS", "%+/test-access"),
                    config_string_case(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "NEWSNEWSNAME",
                                       "%X/test-newsnews")));

TEST_P(GeneratedArtifactTest, generatesTheArtifact)
{
    EXPECT_TRUE(m_file.good()) << GetParam().filename;
}

INSTANTIATE_TEST_SUITE_P(DefaultDataFiles, GeneratedArtifactTest,
                         testing::Values(GeneratedArtifactCase{TRN_TEST_GENERATED_GLOBAL_INIT_FILE},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_DEFACCESS_FILE},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_NEWSNEWS_FILE}));

INSTANTIATE_TEST_SUITE_P(RuntimeHelpers, GeneratedArtifactTest,
                         testing::Values(GeneratedArtifactCase{TRN_TEST_GENERATED_MBOX_SAVER_FILE},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_NORM_SAVER_FILE},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_PNEWS_FILE},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_PNEWS_HEADER_FILE,
                                                               generated_pnews_header()},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_RNMAIL_FILE},
                                         GeneratedArtifactCase{TRN_TEST_GENERATED_SPELLER_FILE}));

INSTANTIATE_TEST_SUITE_P(ArticlePathDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_ARTICLE_PATH_TEMPLATES_DEFAULT_CONFIG,
                                                            "OV_FILE_NAME", TRN_TEST_DEFAULT_OV_FILE_NAME),
                                         config_string_case(TRN_TEST_ARTICLE_PATH_TEMPLATES_DEFAULT_CONFIG, "VARYNAME",
                                                            TRN_TEST_DEFAULT_VARYNAME),
                                         config_string_case(TRN_TEST_ARTICLE_PATH_TEMPLATES_DEFAULT_CONFIG, "HEADNAME",
                                                            TRN_TEST_DEFAULT_HEADNAME)));

INSTANTIATE_TEST_SUITE_P(ConfiguredArticlePaths, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_ARTICLE_PATH_TEMPLATES_CONFIGURED_CONFIG,
                                                            "OV_FILE_NAME", "/test-overview"),
                                         config_string_case(TRN_TEST_ARTICLE_PATH_TEMPLATES_CONFIGURED_CONFIG,
                                                            "VARYNAME", "%Y/test-vary.%$"),
                                         config_string_case(TRN_TEST_ARTICLE_PATH_TEMPLATES_CONFIGURED_CONFIG,
                                                            "HEADNAME", "%./test-head.%$")));

INSTANTIATE_TEST_SUITE_P(SavePathDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_SAVE_PATH_TEMPLATES_DEFAULT_CONFIG, "SAVEDIR",
                                                            TRN_TEST_DEFAULT_SAVEDIR),
                                         config_string_case(TRN_TEST_SAVE_PATH_TEMPLATES_DEFAULT_CONFIG, "SAVENAME",
                                                            TRN_TEST_DEFAULT_SAVENAME)));

TEST(GeneratedConfigTest, savePathTemplatesCanUseCategoryPolicy)
{
    expect_config_string(TRN_TEST_SAVE_PATH_TEMPLATES_CATEGORY_CONFIG, "SAVEDIR", "%p/%c");
    expect_config_string(TRN_TEST_SAVE_PATH_TEMPLATES_CATEGORY_CONFIG, "SAVENAME", "%a");
}

INSTANTIATE_TEST_SUITE_P(
    ConfiguredSavePaths, GeneratedDefinitionTest,
    testing::Values(config_string_case(TRN_TEST_SAVE_PATH_TEMPLATES_CONFIGURED_CONFIG, "SAVEDIR", "%p/test-save"),
                    config_string_case(TRN_TEST_SAVE_PATH_TEMPLATES_CONFIGURED_CONFIG, "SAVENAME", "test-%a")));

INSTANTIATE_TEST_SUITE_P(KillFileDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_KILL_FILE_PATHS_DEFAULT_CONFIG, "KILL_GLOBAL",
                                                            TRN_TEST_DEFAULT_KILL_GLOBAL),
                                         config_string_case(TRN_TEST_KILL_FILE_PATHS_DEFAULT_CONFIG, "KILL_LOCAL",
                                                            TRN_TEST_DEFAULT_KILL_LOCAL),
                                         config_string_case(TRN_TEST_KILL_FILE_PATHS_DEFAULT_CONFIG, "KILL_THREADS",
                                                            TRN_TEST_DEFAULT_KILL_THREADS)));

INSTANTIATE_TEST_SUITE_P(ConfiguredKillFiles, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_KILL_FILE_PATHS_CONFIGURED_CONFIG, "KILL_GLOBAL",
                                                            "%p/test-global-kill"),
                                         config_string_case(TRN_TEST_KILL_FILE_PATHS_CONFIGURED_CONFIG, "KILL_LOCAL",
                                                            "%p/%c/test-local-kill"),
                                         config_string_case(TRN_TEST_KILL_FILE_PATHS_CONFIGURED_CONFIG, "KILL_THREADS",
                                                            "%+/test-thread-kill")));

INSTANTIATE_TEST_SUITE_P(SelectionDefaults, GeneratedDefinitionTest,
                         testing::Values(config_char_case(TRN_TEST_SELECTION_POLICY_DEFAULT_CONFIG, "UNSUBSCRIBED_CHAR",
                                                          TRN_TEST_DEFAULT_UNSUBSCRIBED_CHAR),
                                         config_string_case(TRN_TEST_SELECTION_POLICY_DEFAULT_CONFIG, "SELECTION_CHARS",
                                                            TRN_TEST_DEFAULT_SELECTION_CHARS)));

INSTANTIATE_TEST_SUITE_P(
    ConfiguredSelection, GeneratedDefinitionTest,
    testing::Values(config_char_case(TRN_TEST_SELECTION_POLICY_CONFIGURED_CONFIG, "UNSUBSCRIBED_CHAR", "?"),
                    config_string_case(TRN_TEST_SELECTION_POLICY_CONFIGURED_CONFIG, "SELECTION_CHARS", "xyz789")));

TEST(GeneratedConfigTest, localTimeFormatDefaultMatchesHistoricalFallback)
{
    expect_config_string(TRN_TEST_LOCAL_TIME_FORMAT_DEFAULT_CONFIG, "LOCALTIME_FMT", TRN_TEST_DEFAULT_LOCALTIME_FMT);
}

TEST(GeneratedConfigTest, localTimeFormatConfiguredValueIsEmitted)
{
    expect_config_string(TRN_TEST_LOCAL_TIME_FORMAT_CONFIGURED_CONFIG, "LOCALTIME_FMT", "test %Y");
}

INSTANTIATE_TEST_SUITE_P(
    DefaultFeatures, GeneratedDefinitionTest,
    testing::Values(
        config_define_case(TRN_TEST_FEATURE_POLICY_DEFAULT_CONFIG, "MAIL_CALL", cmake_bool(TRN_TEST_DEFAULT_MAIL_CALL)),
        config_define_case(TRN_TEST_FEATURE_POLICY_DEFAULT_CONFIG, "NO_FIREWORKS",
                           cmake_bool(TRN_TEST_DEFAULT_NO_FIREWORKS)),
        config_define_case(TRN_TEST_FEATURE_POLICY_DEFAULT_CONFIG, "TILDE_NAME",
                           cmake_bool(TRN_TEST_DEFAULT_TILDE_NAME)),
        config_define_case(TRN_TEST_FEATURE_POLICY_DEFAULT_CONFIG, "VALIDATE_XREF_SITE",
                           cmake_bool(TRN_TEST_DEFAULT_VALIDATE_XREF_SITE)),
        config_define_case(TRN_TEST_FEATURE_POLICY_DEFAULT_CONFIG, "USE_FTP", cmake_bool(TRN_TEST_DEFAULT_USE_FTP)),
        config_define_case(TRN_TEST_FEATURE_POLICY_DEFAULT_CONFIG, "REPLYTO_POSTER_CHECKING",
                           cmake_bool(TRN_TEST_DEFAULT_REPLYTO_POSTER_CHECKING))));

INSTANTIATE_TEST_SUITE_P(
    ConfiguredFeatures, GeneratedDefinitionTest,
    testing::Values(config_define_case(TRN_TEST_FEATURE_POLICY_CONFIGURED_CONFIG, "MAIL_CALL", false),
                    config_define_case(TRN_TEST_FEATURE_POLICY_CONFIGURED_CONFIG, "NO_FIREWORKS", false),
                    config_define_case(TRN_TEST_FEATURE_POLICY_CONFIGURED_CONFIG, "TILDE_NAME", false),
                    config_define_case(TRN_TEST_FEATURE_POLICY_CONFIGURED_CONFIG, "VALIDATE_XREF_SITE", true),
                    config_define_case(TRN_TEST_FEATURE_POLICY_CONFIGURED_CONFIG, "USE_FTP", false),
                    config_define_case(TRN_TEST_FEATURE_POLICY_CONFIGURED_CONFIG, "REPLYTO_POSTER_CHECKING", true)));

INSTANTIATE_TEST_SUITE_P(ShellCommandDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG, "SH",
                                                            TRN_TEST_DEFAULT_SH),
                                         config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG,
                                                            "PIPE_SAVER", TRN_TEST_DEFAULT_PIPE_SAVER),
                                         config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG,
                                                            "SHAR_SAVER", TRN_TEST_DEFAULT_SHAR_SAVER),
                                         config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG,
                                                            "CUSTOM_SAVER", TRN_TEST_DEFAULT_CUSTOM_SAVER)));

INSTANTIATE_TEST_SUITE_P(
    ConfiguredShellCommands, GeneratedDefinitionTest,
    testing::Values(
        config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "SH", "/test/sh"),
        config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "PIPE_SAVER", "test-pipe %A %b"),
        config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "SHAR_SAVER", "test-shar %A"),
        config_string_case(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "CUSTOM_SAVER", "test-custom %A %e")));

INSTANTIATE_TEST_SUITE_P(VerifierDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_VERIFICATION_COMMANDS_DEFAULT_CONFIG,
                                                            "VERIFY_RIPEM", TRN_TEST_DEFAULT_VERIFY_RIPEM),
                                         config_string_case(TRN_TEST_VERIFICATION_COMMANDS_DEFAULT_CONFIG, "VERIFY_PGP",
                                                            TRN_TEST_DEFAULT_VERIFY_PGP)));

INSTANTIATE_TEST_SUITE_P(
    DisabledVerifiers, GeneratedDefinitionTest,
    testing::Values(config_string_case(TRN_TEST_VERIFICATION_COMMANDS_DISABLED_CONFIG, "VERIFY_RIPEM", ""),
                    config_string_case(TRN_TEST_VERIFICATION_COMMANDS_DISABLED_CONFIG, "VERIFY_PGP", "")));

INSTANTIATE_TEST_SUITE_P(ConfiguredVerifiers, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_VERIFICATION_COMMANDS_CONFIGURED_CONFIG,
                                                            "VERIFY_RIPEM", "test-ripem %A"),
                                         config_string_case(TRN_TEST_VERIFICATION_COMMANDS_CONFIGURED_CONFIG,
                                                            "VERIFY_PGP", "test-pgp %A")));

TEST_P(WindowsPostingCommandTest, usesTheWindowsLauncherDefault)
{
    EXPECT_EQ(GetParam().second, GetParam().first);
}

INSTANTIATE_TEST_SUITE_P(NewsAndMail, WindowsPostingCommandTest,
                         testing::Values(PostingCommandCase{TRN_TEST_DEFAULT_NEWS_POSTER, "Pnews.cmd -h \"%h\""},
                                         PostingCommandCase{TRN_TEST_DEFAULT_MAIL_POSTER, "Rnmail.cmd -h \"%h\""}));

INSTANTIATE_TEST_SUITE_P(PostingHelperDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_POSTING_HELPER_PATHS_DEFAULT_CONFIG, "CALL_INEWS",
                                                            TRN_TEST_DEFAULT_CALL_INEWS),
                                         config_string_case(TRN_TEST_POSTING_HELPER_PATHS_DEFAULT_CONFIG, "NEWS_POSTER",
                                                            TRN_TEST_DEFAULT_NEWS_POSTER),
                                         config_string_case(TRN_TEST_POSTING_HELPER_PATHS_DEFAULT_CONFIG, "MAIL_POSTER",
                                                            TRN_TEST_DEFAULT_MAIL_POSTER)));

INSTANTIATE_TEST_SUITE_P(ConfiguredPostingHelpers, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_POSTING_HELPER_PATHS_CONFIGURED_CONFIG,
                                                            "CALL_INEWS", "/test/bin/inews -h <%h"),
                                         config_string_case(TRN_TEST_POSTING_HELPER_PATHS_CONFIGURED_CONFIG,
                                                            "NEWS_POSTER", "/test/bin/Pnews -h %h"),
                                         config_string_case(TRN_TEST_POSTING_HELPER_PATHS_CONFIGURED_CONFIG,
                                                            "MAIL_POSTER", "/test/bin/Rnmail -h %h")));

INSTANTIATE_TEST_SUITE_P(
    InstalledPostingHelpers, GeneratedDefinitionTest,
    testing::Values(
        GeneratedTextCase{TRN_TEST_GENERATED_PNEWS_FILE, std::string{"artcheck="} + TRN_TEST_BIN_DIR + "/trn-artchk",
                          !windows_generated_helpers()},
        GeneratedTextCase{TRN_TEST_GENERATED_PNEWS_FILE, std::string{"nntplist="} + TRN_TEST_BIN_DIR + "/nntplist",
                          !windows_generated_helpers()},
        GeneratedTextCase{TRN_TEST_GENERATED_PNEWS_FILE, std::string{"inews=\""} + TRN_TEST_BIN_DIR + "/inews\"",
                          !windows_generated_helpers()},
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "FORWARD_POSTER",
                           windows_generated_helpers()
                               ? std::string{"call \""} + TRN_TEST_BIN_DIR + "/Rnmail.cmd\" -h \"%h\""
                               : std::string{TRN_TEST_BIN_DIR} + "/Rnmail -h %h")));

TEST(GeneratedConfigTest, windowsMailHelperInvokesPowerShell)
{
    if (!windows_generated_helpers())
    {
        GTEST_SKIP() << "Windows launchers are only generated on Windows.";
    }

    expect_file_contains(TRN_TEST_GENERATED_RNMAIL_FILE, "Rnmail.ps1");
}

TEST(GeneratedConfigTest, generatedPnewsHeaderContainsPostingFields)
{
    if (!generated_pnews_header())
    {
        GTEST_SKIP() << "Pnews.header is folded into the Windows helper strategy.";
    }

    expect_file_contains(TRN_TEST_GENERATED_PNEWS_HEADER_FILE, "Newsgroups: $ng");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_HEADER_FILE, "Subject: $title");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_HEADER_FILE, "Distribution: $dist");
}

TEST(GeneratedConfigTest, generatedPnewsSendFilterReplacesOldUserAgent)
{
    if (windows_generated_helpers())
    {
        GTEST_SKIP() << "Windows Pnews behavior is covered by posting workflow tests.";
    }

    const std::string user_agent = std::string{"user_agent=\"trn/"} + TRN_TEST_PROJECT_VERSION + "\"";

    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, user_agent);
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "/^User-Agent:/d;");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "User-Agent: '\"$user_agent\"'");
}

TEST(GeneratedConfigTest, generatedPnewsSendFilterAddsMimeHeaders)
{
    if (windows_generated_helpers())
    {
        GTEST_SKIP() << "Windows Pnews behavior is covered by posting workflow tests.";
    }

    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "Mime-Version: 1.0");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "Content-Type: text/plain; charset=\"'\"$charset\"'\"");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "Content-Transfer-Encoding: '\"$transfer_encoding\"'");
}

TEST(GeneratedConfigTest, generatedPnewsTransferEncodingDefaultsAsciiBodiesTo7Bit)
{
    if (windows_generated_helpers())
    {
        GTEST_SKIP() << "Windows Pnews behavior is covered by posting workflow tests.";
    }

    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "set_transfer_encoding() {");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "transfer_encoding=7bit");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "set_transfer_encoding $tmpart");
}

TEST(GeneratedConfigTest, generatedPnewsTransferEncodingMarksNonAsciiBodiesAs8Bit)
{
    if (windows_generated_helpers())
    {
        GTEST_SKIP() << "Windows Pnews behavior is covered by posting workflow tests.";
    }

    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "LC_ALL=C tr -d '\\000-\\177'");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "LC_ALL=C grep .");
    expect_file_contains(TRN_TEST_GENERATED_PNEWS_FILE, "transfer_encoding=8bit");
}

namespace
{

constexpr std::string_view DEFAULT_MAIL_HEADER = "To: %t\\n"
                                                 "Subject: %(%i=^$?:Re: %S\\n"
                                                 "X-Newsgroups: %n\\n"
                                                 "In-Reply-To: %i)\\n"
                                                 "%(%{FROM}=^$?:From: %{FROM}\\n)"
                                                 "%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\\n)"
                                                 "%(%[references]=^$?:References: %[references]\\n)"
                                                 "Organization: %o\\n"
                                                 "Cc: \\n"
                                                 "Bcc: \\n"
                                                 "\\n";
constexpr std::string_view DEFAULT_NEWS_HEADER = "%(%[followup-to]=^$?:%(%[followup-to]=^%n$?:"
                                                 "X-ORIGINAL-NEWSGROUPS: %n\\n))"
                                                 "Newsgroups: %(%F=^$?%C:%F)\\n"
                                                 "Subject: %(%S=^$?%\\\"\\n\\nSubject: \\\":Re: %S)\\n"
                                                 "Summary: \\n"
                                                 "Expires: \\n"
                                                 "%(%R=^$?:References: %R\\n)"
                                                 "Sender: \\n"
                                                 "Followup-To: \\n"
                                                 "%(%{FROM}=^$?:From: %{FROM}\\n)"
                                                 "%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\\n)"
                                                 "Distribution: %(%i=^$?%\\\"Distribution: \\\":%D)\\n"
                                                 "Organization: %o\\n"
                                                 "Keywords: %[keywords]\\n"
                                                 "Cc: %(%F=poster?%t:%(%F!=@?:%F))\\n"
                                                 "\\n";
constexpr std::string_view DEFAULT_CANCEL_HEADER = "Newsgroups: %n\\n"
                                                   "Subject: cancel\\n"
                                                   "%(%{FROM}=^$?:From: %{FROM}\\n)"
                                                   "Control: cancel %i\\n"
                                                   "Distribution: %D\\n"
                                                   "\\n"
                                                   "%i was cancelled from within trn.\\n";
constexpr std::string_view DEFAULT_SUPERSEDE_HEADER = "Newsgroups: %n\\n"
                                                      "Subject: %[subject]\\n"
                                                      "%(%{FROM}=^$?:From: %{FROM}\\n)"
                                                      "Summary: %[summary]\\n"
                                                      "Expires: %[expires]\\n"
                                                      "References: %[references]\\n"
                                                      "From: %[from]\\n"
                                                      "Reply-To: %[reply-to]\\n"
                                                      "Supersedes: %i\\n"
                                                      "Sender: %[sender]\\n"
                                                      "Followup-To: %[followup-to]\\n"
                                                      "Distribution: %D\\n"
                                                      "Organization: %o\\n"
                                                      "Keywords: %[keywords]\\n"
                                                      "\\n";

} // namespace

INSTANTIATE_TEST_SUITE_P(PostingHeaderDefaults, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_DEFAULT_CONFIG,
                                                            "MAIL_HEADER", DEFAULT_MAIL_HEADER),
                                         config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_DEFAULT_CONFIG,
                                                            "NEWS_HEADER", DEFAULT_NEWS_HEADER),
                                         config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_DEFAULT_CONFIG,
                                                            "CANCEL_HEADER", DEFAULT_CANCEL_HEADER),
                                         config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_DEFAULT_CONFIG,
                                                            "SUPERSEDE_HEADER", DEFAULT_SUPERSEDE_HEADER)));

INSTANTIATE_TEST_SUITE_P(ConfiguredPostingHeaders, GeneratedDefinitionTest,
                         testing::Values(config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_CONFIGURED_CONFIG,
                                                            "MAIL_HEADER", "test-mail\\n"),
                                         config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_CONFIGURED_CONFIG,
                                                            "NEWS_HEADER", "test-news\\n"),
                                         config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_CONFIGURED_CONFIG,
                                                            "CANCEL_HEADER", "test-cancel\\n"),
                                         config_string_case(TRN_TEST_POSTING_HEADER_TEMPLATES_CONFIGURED_CONFIG,
                                                            "SUPERSEDE_HEADER", "test-supersede\\n")));

namespace
{

constexpr std::string_view DEFAULT_FORWARD_HEADER = "To: %\\\"\\n"
                                                    "\\n"
                                                    "To: \\\"\\n"
                                                    "Subject: %(%i=^$?:%[subject] (fwd\\\\)\\n"
                                                    "%(%{FROM}=^$?:From: %{FROM}\\n)"
                                                    "%(%{REPLYTO}=^$?:Reply-To: %{REPLYTO}\\n)"
                                                    "X-Newsgroups: %n\\n"
                                                    "In-Reply-To: %i)\\n"
                                                    "%(%[references]=^$?:References: %[references]\\n)"
                                                    "Organization: %o\\n"
                                                    "Mime-Version: 1.0\\n"
                                                    "Content-Type: multipart/mixed; boundary=\\\"=%$%^#=--\\\"\\n"
                                                    "Cc: \\n"
                                                    "Bcc: \\n"
                                                    "\\n";

} // namespace

INSTANTIATE_TEST_SUITE_P(
    ReplyAndForwardDefaults, GeneratedDefinitionTest,
    testing::Values(
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "YOU_SAID", "In article %i you write:"),
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "ATTRIBUTION",
                           "In article %i,%?%)f <%>f> wrote:"),
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "MAIL_POSTER",
                           TRN_TEST_DEFAULT_MAIL_POSTER),
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "FORWARD_POSTER",
                           TRN_TEST_DEFAULT_FORWARD_POSTER),
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "FORWARD_HEADER", DEFAULT_FORWARD_HEADER),
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "FORWARD_MSG",
                           "------- start of forwarded message -------"),
        config_string_case(TRN_TEST_REPLY_FORWARD_TEMPLATES_DEFAULT_CONFIG, "FORWARD_MSG_END",
                           "------- end of forwarded message -------")));

INSTANTIATE_TEST_SUITE_P(
    HelperToolDefaults, GeneratedDefinitionTest,
    testing::Values(
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, TRN_TEST_DEFAULT_SHELL_START},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT,
                          std::string{"pager=\"${PAGER-"} + TRN_TEST_DEFAULT_PAGER + "}\""},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT,
                          std::string{"ispell=\"${ISPELL_PROGRAM-"} + TRN_TEST_DEFAULT_ISPELL_PROGRAM + "}\""},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT,
                          std::string{"ispell_options=\"${ISPELL_OPTIONS-"} + TRN_TEST_DEFAULT_ISPELL_OPTIONS + "}\""},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, std::string{"diff="} + TRN_TEST_DEFAULT_DIFF_PROGRAM},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, std::string{"ed="} + TRN_TEST_DEFAULT_ED_PROGRAM},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT,
                          std::string{"spell=\"${SPELL_PROGRAM-"} + TRN_TEST_DEFAULT_SPELL_PROGRAM + "}\""}));

INSTANTIATE_TEST_SUITE_P(
    ConfiguredHelperTools, GeneratedDefinitionTest,
    testing::Values(
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "#!/test/sh"},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "pager=\"${PAGER-/test/bin/more}\""},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "ispell=\"${ISPELL_PROGRAM-/test/bin/ispell}\""},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "ispell_options=\"${ISPELL_OPTIONS--x -Tlatin1}\""},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "diff=/test/bin/diff"},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "ed=/test/bin/ed"},
        GeneratedTextCase{TRN_TEST_HELPER_TOOLS_CONFIGURED_SCRIPT, "spell=\"${SPELL_PROGRAM-/test/bin/spell}\""}));

TEST(GeneratedConfigTest, helperFilesDoNotContainOldToolVariables)
{
    expect_file_not_contains(TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, "@SHELL_START@");
    expect_file_not_contains(TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, "@DIFF_PROGRAM@");
    expect_file_not_contains(TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, "@ED_PROGRAM@");
    expect_file_not_contains(TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, "@ISPELL_PROGRAM@");
    expect_file_not_contains(TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, "@ISPELL_OPTIONS@");
    expect_file_not_contains(TRN_TEST_HELPER_TOOLS_DEFAULT_SCRIPT, "@SPELL_PROGRAM@");
    expect_file_not_contains(TRN_TEST_GENERATED_MBOX_SAVER_FILE, "$sed");
}

TEST(GeneratedConfigTest, runtimeHelperFilesDoNotContainMetaconfigResidue)
{
    expect_helper_has_no_metaconfig_residue(TRN_TEST_GENERATED_MBOX_SAVER_FILE);
    expect_helper_has_no_metaconfig_residue(TRN_TEST_GENERATED_NORM_SAVER_FILE);
    expect_helper_has_no_metaconfig_residue(TRN_TEST_GENERATED_PNEWS_FILE);
    if (generated_pnews_header())
    {
        expect_helper_has_no_metaconfig_residue(TRN_TEST_GENERATED_PNEWS_HEADER_FILE);
    }
    expect_helper_has_no_metaconfig_residue(TRN_TEST_GENERATED_RNMAIL_FILE);
    expect_helper_has_no_metaconfig_residue(TRN_TEST_GENERATED_SPELLER_FILE);
}

TEST(GeneratedConfigTest, pnewsDistributionDefaultsMatchHistoricalFallbacks)
{
    const std::string name_type = std::string{"nametype=\""} + TRN_TEST_DEFAULT_NAME_TYPE + "\"";
    const std::string local_dist = std::string{"loc=\""} + TRN_TEST_DEFAULT_LOCAL_DIST + "\"";
    const std::string organization_dist = std::string{"org=\""} + TRN_TEST_DEFAULT_ORGANIZATION_DIST + "\"";
    const std::string city_dist = std::string{"city=\""} + TRN_TEST_DEFAULT_CITY_DIST + "\"";
    const std::string state_dist = std::string{"state=\""} + TRN_TEST_DEFAULT_STATE_DIST + "\"";
    const std::string multistate_dist = std::string{"multistate=\""} + TRN_TEST_DEFAULT_MULTISTATE_DIST + "\"";
    const std::string country_dist = std::string{"cntry=\""} + TRN_TEST_DEFAULT_COUNTRY_DIST + "\"";
    const std::string continent_dist = std::string{"cont=\""} + TRN_TEST_DEFAULT_CONTINENT_DIST + "\"";

    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, name_type);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, local_dist);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, organization_dist);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, city_dist);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, state_dist);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, multistate_dist);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, country_dist);
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_DEFAULT_SCRIPT, continent_dist);
}

TEST(GeneratedConfigTest, pnewsDistributionConfiguredValuesAreEmitted)
{
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "nametype=\"bsd\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "loc=\"local-test\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "org=\"org-test\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "city=\"city-test\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "state=\"state-test\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "multistate=\"multi-test\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "cntry=\"country-test\"");
    expect_file_contains(TRN_TEST_PNEWS_DISTRIBUTION_CONFIGURED_SCRIPT, "cont=\"continent-test\"");
}
