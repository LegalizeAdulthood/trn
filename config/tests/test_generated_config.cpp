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

} // namespace

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
