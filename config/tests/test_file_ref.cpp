// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/config2.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

TEST(FileRefTest, absolutePathIsAbsolute)
{
    const std::string path = fs::absolute("var/spool/news").string();

    EXPECT_TRUE(file_ref(path));
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
    constexpr std::string_view suffix{":ignored"};
    const std::string          absolute_path = fs::absolute("var/spool/news").string();
    const std::string          storage = absolute_path + std::string{suffix};
    const std::string_view     path{storage.data(), absolute_path.size()};

    EXPECT_TRUE(file_ref(path));
}
