// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include "test_generated_config.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

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

void expect_file_exists(const char *filename)
{
    std::ifstream file{filename, std::ios::binary};

    EXPECT_TRUE(file.good()) << filename;
}

void expect_config_define(const char *filename, std::string_view name, bool defined)
{
    const std::string expected =
        defined ? std::string{"#define "} + std::string{name} : std::string{"/* #undef "} + std::string{name} + " */";

    expect_file_contains(filename, expected);
}

void expect_config_string(const char *filename, std::string_view name, std::string_view value)
{
    const std::string expected = std::string{"#define "} + std::string{name} + " \"" + std::string{value} + "\"";

    expect_file_contains(filename, expected);
}

void expect_config_char(const char *filename, std::string_view name, std::string_view value)
{
    ASSERT_EQ(1, value.size()) << name;
    const std::string expected = std::string{"#define "} + std::string{name} + " '" + std::string{value} + "'";

    expect_file_contains(filename, expected);
}

bool cmake_bool(std::string_view value)
{
    return value == "1" || value == "ON" || value == "TRUE";
}

} // namespace

TEST(GeneratedConfigTest, inputAndSignalProbesMatchPlatformChecks)
{
    expect_config_define(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "I_SGTTY", cmake_bool(TRN_TEST_I_SGTTY));
    expect_config_define(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "HAS_RDCHK", cmake_bool(TRN_TEST_HAS_RDCHK));
    expect_config_define(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "HAS_SIGBLOCK", cmake_bool(TRN_TEST_HAS_SIGBLOCK));
    expect_config_define(TRN_TEST_INPUT_SIGNAL_PROBES_CONFIG, "HAS_SIGHOLD", cmake_bool(TRN_TEST_HAS_SIGHOLD));
}

TEST(GeneratedConfigTest, waitStatusProbeMatchesPlatformCheck)
{
    expect_file_not_contains(TRN_TEST_WAIT_STATUS_PROBE_CONFIG, "UNION_WAIT");
    expect_config_define(TRN_TEST_WAIT_STATUS_PROBE_CONFIG, "USE_WIFSTAT", cmake_bool(TRN_TEST_USE_WIFSTAT));
}

TEST(GeneratedConfigTest, activeTimesDefaultIsNone)
{
    expect_file_contains(TRN_TEST_ACTIVE_TIMES_DEFAULT_CONFIG, "#define ACTIVE_TIMES \"none\"");
}

TEST(GeneratedConfigTest, activeTimesConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_ACTIVE_TIMES_CONFIGURED_CONFIG, "#define ACTIVE_TIMES \"%x/active.times\"");
}

TEST(GeneratedConfigTest, activeDefaultMatchesHistoricalFallback)
{
    const std::string expected = std::string{"#define ACTIVE \""} + TRN_TEST_DEFAULT_ACTIVE + "\"";

    expect_file_contains(TRN_TEST_ACTIVE_DEFAULT_CONFIG, expected);
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

TEST(GeneratedConfigTest, newsLibDefaultMatchesPlatformDefault)
{
    const std::string expected = std::string{"#define NEWS_LIB \""} + TRN_TEST_DEFAULT_NEWS_LIB + "\"";

    expect_file_contains(TRN_TEST_NEWS_LIB_DEFAULT_CONFIG, expected);
}

TEST(GeneratedConfigTest, newsLibConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_NEWS_LIB_CONFIGURED_CONFIG, "#define NEWS_LIB \"/var/lib/trn/news\"");
}

TEST(GeneratedConfigTest, groupDescDefaultMatchesHistoricalFallback)
{
    const std::string expected = std::string{"#define GROUP_DESC \""} + TRN_TEST_DEFAULT_GROUP_DESC + "\"";

    expect_file_contains(TRN_TEST_GROUP_DESC_DEFAULT_CONFIG, expected);
}

TEST(GeneratedConfigTest, groupDescConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_GROUP_DESC_CONFIGURED_CONFIG, "#define GROUP_DESC \"/var/lib/trn/newsgroups\"");
}

TEST(GeneratedConfigTest, subscriptionsDefaultMatchesHistoricalFallback)
{
    const std::string expected{std::string{"#define SUBSCRIPTIONS \""} + TRN_TEST_DEFAULT_SUBSCRIPTIONS + "\""};

    expect_file_contains(TRN_TEST_SUBSCRIPTIONS_DEFAULT_CONFIG, expected);
}

TEST(GeneratedConfigTest, subscriptionsConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_SUBSCRIPTIONS_CONFIGURED_CONFIG,
                         "#define SUBSCRIPTIONS \"/var/lib/trn/subscriptions\"");
}

TEST(GeneratedConfigTest, overviewDirDefaultUsesRemoteFallback)
{
    expect_file_contains(TRN_TEST_OVERVIEW_DIR_DEFAULT_CONFIG, "#define OVERVIEW_DIR    \"remote\"");
}

TEST(GeneratedConfigTest, overviewDirConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_OVERVIEW_DIR_CONFIGURED_CONFIG,
                         "#define OVERVIEW_DIR    \"/var/spool/news/overview\"");
}

TEST(GeneratedConfigTest, overviewFmtDefaultDisablesLocalFormatFile)
{
    expect_file_contains(TRN_TEST_OVERVIEW_FMT_DEFAULT_CONFIG, "#define OVERVIEW_FMT    \"none\"");
}

TEST(GeneratedConfigTest, overviewFmtConfiguredPathIsEmitted)
{
    expect_file_contains(TRN_TEST_OVERVIEW_FMT_CONFIGURED_CONFIG,
                         "#define OVERVIEW_FMT    \"/var/lib/trn/overview.fmt\"");
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
    expect_config_define(TRN_TEST_NAME_PARSING_DEFAULT_CONFIG, "PASS_NAMES", cmake_bool(TRN_TEST_DEFAULT_PASS_NAMES));
    expect_config_define(TRN_TEST_NAME_PARSING_DEFAULT_CONFIG, "BERKELEY_NAMES",
                         cmake_bool(TRN_TEST_DEFAULT_BERKELEY_NAMES));
}

TEST(GeneratedConfigTest, nameParsingCanUseFullnameFile)
{
    expect_config_define(TRN_TEST_NAME_PARSING_FULLNAME_FILE_CONFIG, "PASS_NAMES", false);
    expect_config_define(TRN_TEST_NAME_PARSING_FULLNAME_FILE_CONFIG, "BERKELEY_NAMES", false);
}

TEST(GeneratedConfigTest, nameParsingCanUseUsgPasswdFormat)
{
    expect_config_define(TRN_TEST_NAME_PARSING_USG_CONFIG, "PASS_NAMES", true);
    expect_config_define(TRN_TEST_NAME_PARSING_USG_CONFIG, "BERKELEY_NAMES", false);
}

TEST(GeneratedConfigTest, nameParsingCanUseBerkeleyPasswdFormat)
{
    expect_config_define(TRN_TEST_NAME_PARSING_BERKELEY_CONFIG, "PASS_NAMES", true);
    expect_config_define(TRN_TEST_NAME_PARSING_BERKELEY_CONFIG, "BERKELEY_NAMES", true);
}

TEST(GeneratedConfigTest, accountNameFilesDefaultMatchesHistoricalFallback)
{
    const std::string password_file = std::string{"#define PASSWORD_FILE \""} + TRN_TEST_DEFAULT_PASSWORD_FILE + "\"";
    const std::string login_dir_field = std::string{"#define LOGIN_DIR_FIELD "} + TRN_TEST_DEFAULT_LOGIN_DIR_FIELD;
    const std::string fullname_file = std::string{"#define FULLNAMEFILE \""} + TRN_TEST_DEFAULT_FULLNAMEFILE + "\"";

    expect_file_contains(TRN_TEST_ACCOUNT_NAME_FILES_DEFAULT_CONFIG, password_file);
    expect_file_contains(TRN_TEST_ACCOUNT_NAME_FILES_DEFAULT_CONFIG, login_dir_field);
    expect_file_contains(TRN_TEST_ACCOUNT_NAME_FILES_DEFAULT_CONFIG, fullname_file);
}

TEST(GeneratedConfigTest, accountNameFilesConfiguredValuesAreEmitted)
{
    expect_file_contains(TRN_TEST_ACCOUNT_NAME_FILES_CONFIGURED_CONFIG,
                         "#define PASSWORD_FILE \"/var/lib/trn/passwd\"");
    expect_file_contains(TRN_TEST_ACCOUNT_NAME_FILES_CONFIGURED_CONFIG, "#define LOGIN_DIR_FIELD 7");
    expect_file_contains(TRN_TEST_ACCOUNT_NAME_FILES_CONFIGURED_CONFIG, "#define FULLNAMEFILE \"%./profile-name\"");
}

TEST(GeneratedConfigTest, userStatePathsDefaultMatchHistoricalFallbacks)
{
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "TRNDIR", TRN_TEST_DEFAULT_TRNDIR);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "TRNMACRO", TRN_TEST_DEFAULT_TRNMACRO);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "RNMACRO", TRN_TEST_DEFAULT_RNMACRO);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "RCNAME", TRN_TEST_DEFAULT_RCNAME);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "LOCKNAME", TRN_TEST_DEFAULT_LOCKNAME);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "LASTNAME", TRN_TEST_DEFAULT_LASTNAME);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "SIGNATURE_FILE", TRN_TEST_DEFAULT_SIGNATURE_FILE);
    expect_config_string(TRN_TEST_USER_STATE_PATHS_DEFAULT_CONFIG, "NNTP_AUTH_FILE", TRN_TEST_DEFAULT_NNTP_AUTH_FILE);
}

TEST(GeneratedConfigTest, userStatePathsConfiguredValuesAreEmitted)
{
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "TRNDIR", "%./trn-test");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "TRNMACRO", "%+/test-macros");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "RNMACRO", "%./test-rnmac");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "RCNAME", "%./test-newsrc");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "LOCKNAME", "%./test-rnlock");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "LASTNAME", "%./test-rnlast");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "SIGNATURE_FILE", "%./test-signature");
    expect_config_string(TRN_TEST_USER_STATE_PATHS_CONFIGURED_CONFIG, "NNTP_AUTH_FILE", "%./test-nntpauth");
}

TEST(GeneratedConfigTest, dataFilePathsDefaultMatchHistoricalFallbacks)
{
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "GLOBAL_INIT", TRN_TEST_DEFAULT_GLOBAL_INIT);
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "DEFACCESS", TRN_TEST_DEFAULT_DEFACCESS);
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "TRNACCESS", TRN_TEST_DEFAULT_TRNACCESS);
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_DEFAULT_CONFIG, "NEWSNEWSNAME", TRN_TEST_DEFAULT_NEWSNEWSNAME);
}

TEST(GeneratedConfigTest, dataFilePathsConfiguredValuesAreEmitted)
{
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "GLOBAL_INIT", "%X/test-INIT");
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "DEFACCESS", "%X/test-access.def");
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "TRNACCESS", "%+/test-access");
    expect_config_string(TRN_TEST_DATA_FILE_PATHS_CONFIGURED_CONFIG, "NEWSNEWSNAME", "%X/test-newsnews");
}

TEST(GeneratedConfigTest, defaultDataFilesAreGenerated)
{
    expect_file_exists(TRN_TEST_GENERATED_GLOBAL_INIT_FILE);
    expect_file_exists(TRN_TEST_GENERATED_DEFACCESS_FILE);
    expect_file_exists(TRN_TEST_GENERATED_NEWSNEWS_FILE);
}

TEST(GeneratedConfigTest, articlePathTemplatesDefaultMatchHistoricalFallbacks)
{
    expect_config_string(TRN_TEST_ARTICLE_PATH_TEMPLATES_DEFAULT_CONFIG, "OV_FILE_NAME", TRN_TEST_DEFAULT_OV_FILE_NAME);
    expect_config_string(TRN_TEST_ARTICLE_PATH_TEMPLATES_DEFAULT_CONFIG, "VARYNAME", TRN_TEST_DEFAULT_VARYNAME);
    expect_config_string(TRN_TEST_ARTICLE_PATH_TEMPLATES_DEFAULT_CONFIG, "HEADNAME", TRN_TEST_DEFAULT_HEADNAME);
}

TEST(GeneratedConfigTest, articlePathTemplatesConfiguredValuesAreEmitted)
{
    expect_config_string(TRN_TEST_ARTICLE_PATH_TEMPLATES_CONFIGURED_CONFIG, "OV_FILE_NAME", "/test-overview");
    expect_config_string(TRN_TEST_ARTICLE_PATH_TEMPLATES_CONFIGURED_CONFIG, "VARYNAME", "%Y/test-vary.%$");
    expect_config_string(TRN_TEST_ARTICLE_PATH_TEMPLATES_CONFIGURED_CONFIG, "HEADNAME", "%./test-head.%$");
}

TEST(GeneratedConfigTest, savePathTemplatesDefaultMatchCMakeSelection)
{
    expect_config_string(TRN_TEST_SAVE_PATH_TEMPLATES_DEFAULT_CONFIG, "SAVEDIR", TRN_TEST_DEFAULT_SAVEDIR);
    expect_config_string(TRN_TEST_SAVE_PATH_TEMPLATES_DEFAULT_CONFIG, "SAVENAME", TRN_TEST_DEFAULT_SAVENAME);
}

TEST(GeneratedConfigTest, savePathTemplatesConfiguredValuesAreEmitted)
{
    expect_config_string(TRN_TEST_SAVE_PATH_TEMPLATES_CONFIGURED_CONFIG, "SAVEDIR", "%p/test-save");
    expect_config_string(TRN_TEST_SAVE_PATH_TEMPLATES_CONFIGURED_CONFIG, "SAVENAME", "test-%a");
}

TEST(GeneratedConfigTest, selectionPolicyDefaultMatchesHistoricalFallbacks)
{
    expect_config_char(TRN_TEST_SELECTION_POLICY_DEFAULT_CONFIG, "UNSUBSCRIBED_CHAR",
                       TRN_TEST_DEFAULT_UNSUBSCRIBED_CHAR);
    expect_config_string(TRN_TEST_SELECTION_POLICY_DEFAULT_CONFIG, "SELECTION_CHARS", TRN_TEST_DEFAULT_SELECTION_CHARS);
}

TEST(GeneratedConfigTest, selectionPolicyConfiguredValuesAreEmitted)
{
    expect_config_char(TRN_TEST_SELECTION_POLICY_CONFIGURED_CONFIG, "UNSUBSCRIBED_CHAR", "?");
    expect_config_string(TRN_TEST_SELECTION_POLICY_CONFIGURED_CONFIG, "SELECTION_CHARS", "xyz789");
}

TEST(GeneratedConfigTest, shellCommandDefaultsMatchPlatformFallbacks)
{
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG, "SH", TRN_TEST_DEFAULT_SH);
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG, "PIPE_SAVER", TRN_TEST_DEFAULT_PIPE_SAVER);
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG, "SHAR_SAVER", TRN_TEST_DEFAULT_SHAR_SAVER);
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_DEFAULT_CONFIG, "CUSTOM_SAVER", TRN_TEST_DEFAULT_CUSTOM_SAVER);
}

TEST(GeneratedConfigTest, shellCommandConfiguredValuesAreEmitted)
{
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "SH", "/test/sh");
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "PIPE_SAVER", "test-pipe %A %b");
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "SHAR_SAVER", "test-shar %A");
    expect_config_string(TRN_TEST_SHELL_COMMAND_DEFAULTS_CONFIGURED_CONFIG, "CUSTOM_SAVER", "test-custom %A %e");
}

TEST(GeneratedConfigTest, verificationCommandDefaultsMatchToolDiscovery)
{
    expect_config_string(TRN_TEST_VERIFICATION_COMMANDS_DEFAULT_CONFIG, "VERIFY_RIPEM", TRN_TEST_DEFAULT_VERIFY_RIPEM);
    expect_config_string(TRN_TEST_VERIFICATION_COMMANDS_DEFAULT_CONFIG, "VERIFY_PGP", TRN_TEST_DEFAULT_VERIFY_PGP);
}

TEST(GeneratedConfigTest, verificationCommandDisabledDefaultsAreEmitted)
{
    expect_config_string(TRN_TEST_VERIFICATION_COMMANDS_DISABLED_CONFIG, "VERIFY_RIPEM", "");
    expect_config_string(TRN_TEST_VERIFICATION_COMMANDS_DISABLED_CONFIG, "VERIFY_PGP", "");
}

TEST(GeneratedConfigTest, verificationCommandConfiguredValuesAreEmitted)
{
    expect_config_string(TRN_TEST_VERIFICATION_COMMANDS_CONFIGURED_CONFIG, "VERIFY_RIPEM", "test-ripem %A");
    expect_config_string(TRN_TEST_VERIFICATION_COMMANDS_CONFIGURED_CONFIG, "VERIFY_PGP", "test-pgp %A");
}

TEST(GeneratedConfigTest, pnewsNameTypeDefaultMatchesNameParsingMode)
{
    const std::string expected = std::string{"nametype=\""} + TRN_TEST_DEFAULT_NAME_TYPE + "\"";

    expect_file_contains(TRN_TEST_PNEWS_NAME_TYPE_DEFAULT_SCRIPT, expected);
}
