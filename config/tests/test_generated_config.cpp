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
