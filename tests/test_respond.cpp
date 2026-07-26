// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/respond-internal.h>

#include <gtest/gtest.h>

#include <string_view>

TEST(ExtractOptionsTest, leavesPlainDestination)
{
    int part{};
    int total{};

    const std::string_view destination = respond_parse_extract_options_for_test(" extracted", part, total);

    EXPECT_EQ("extracted", destination);
    EXPECT_EQ(0, part);
    EXPECT_EQ(0, total);
}

TEST(ExtractOptionsTest, parsesPartAndTotal)
{
    int part{};
    int total{};

    const std::string_view destination = respond_parse_extract_options_for_test(" -2/5 extracted", part, total);

    EXPECT_EQ("extracted", destination);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(ExtractOptionsTest, usesPartAsTotalWithoutSlash)
{
    int part{};
    int total{};

    const std::string_view destination = respond_parse_extract_options_for_test(" -2 extracted", part, total);

    EXPECT_EQ(" extracted", destination);
    EXPECT_EQ(2, part);
    EXPECT_EQ(2, total);
}

TEST(ExtractOptionsTest, leavesNonNumericOptionInDestination)
{
    int part{};
    int total{};

    const std::string_view destination = respond_parse_extract_options_for_test(" -x extracted", part, total);

    EXPECT_EQ("-x extracted", destination);
    EXPECT_EQ(0, part);
    EXPECT_EQ(0, total);
}
