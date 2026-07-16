// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/url.h>

#include <gtest/gtest.h>

#include <string>

TEST(UrlTest, rejectsEmptyUrl)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Empty URL -- ignoring.\n", output);
}

TEST(UrlTest, rejectsMissingScheme)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("example.com/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Incomplete URL: example.com/path\n", output);
}

TEST(UrlTest, rejectsMissingHost)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("http:/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("URL needs a hostname: http:/path\n", output);
}

TEST(UrlTest, rejectsUnterminatedAddressLiteral)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://[2001:db8::1/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Bad address literal: gopher://[2001:db8::1/path\n", output);
}

TEST(UrlTest, rejectsNonnumericPort)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://example.com:abc/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Bad URL (non-numeric portnum): gopher://example.com:abc/path\n", output);
}

TEST(UrlTest, rejectsMissingPath)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://example.com:70", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Bad URL (path does not start with /): gopher://example.com:70\n", output);
}

TEST(UrlTest, parsesUnsupportedScheme)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://example.com:70/resource", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("\nURL type gopher not supported (yet?)\n", output);
}

TEST(UrlTest, parsesHostlessNewsUrl)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("news:/comp.lang.apl", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("\nURL type news not supported (yet?)\n", output);
}
