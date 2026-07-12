// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/decode.h>

#include <gtest/gtest.h>

TEST(DecodeFixFilenameTest, usesUnixBasename)
{
    EXPECT_EQ("file_txt", decode_fix_filename("/tmp/path/file_txt"));
}

TEST(DecodeFixFilenameTest, usesWindowsBasename)
{
    EXPECT_EQ("file_txt", decode_fix_filename("C:\\tmp\\path\\file_txt"));
}

TEST(DecodeFixFilenameTest, filtersBadCharactersFromBasename)
{
    EXPECT_EQ("abc", decode_fix_filename("/tmp/a b;c"));
}

TEST(DecodeFixFilenameTest, fallsBackForEmptyBasename)
{
    EXPECT_EQ("x", decode_fix_filename("/tmp/path/"));
}

TEST(DecodeFixFilenameTest, fallsBackForBadBasename)
{
    EXPECT_EQ("x", decode_fix_filename("/tmp/.."));
}
