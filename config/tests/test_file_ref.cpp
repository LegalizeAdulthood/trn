// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/config2.h>

#include <gtest/gtest.h>

#include <string_view>

TEST(FileRefTest, slashPathIsAbsolute)
{
    EXPECT_TRUE(file_ref("/var/spool/news"));
}

TEST(FileRefTest, relativePathIsNotAbsolute)
{
    EXPECT_FALSE(file_ref("var/spool/news"));
}

TEST(FileRefTest, emptyPathIsNotAbsolute)
{
    EXPECT_FALSE(file_ref(""));
}

TEST(FileRefTest, emptyStringViewIsNotAbsolute)
{
    EXPECT_FALSE(file_ref(std::string_view{}));
}

TEST(FileRefTest, acceptsStringView)
{
    constexpr std::string_view path{"/var/spool/news:ignored", 15};

    EXPECT_TRUE(file_ref(path));
}

#ifdef MSDOS
TEST(FileRefTest, drivePathIsAbsolute)
{
    EXPECT_TRUE(file_ref("c:/news"));
}

TEST(FileRefTest, acceptsDrivePathStringView)
{
    constexpr std::string_view path{"c:/news:ignored", 7};

    EXPECT_TRUE(file_ref(path));
}
#else
TEST(FileRefTest, drivePathIsNotAbsolute)
{
    EXPECT_FALSE(file_ref("c:/news"));
}
#endif
