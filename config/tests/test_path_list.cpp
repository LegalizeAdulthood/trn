// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/path_list.h>

#include <config/config.h>

#include <gtest/gtest.h>

#include <string_view>
#include <vector>

TEST(PathListTest, splitsSelectedPathListSeparator)
{
#ifdef MSDOS
    const std::vector<std::string_view> expected{"one", "two", "three"};

    EXPECT_EQ(expected, split_path_list("one;two;;three;"));
#else
    const std::vector<std::string_view> expected{"one", "two", "three"};

    EXPECT_EQ(expected, split_path_list("one:two::three:"));
#endif
}

#ifdef MSDOS
TEST(PathListTest, keepsWindowsDriveLettersInPathItems)
{
    const std::vector<std::string_view> expected{"C:/one", "D:/two", R"(E:\three)"};

    EXPECT_EQ(expected, split_path_list(R"(C:/one;D:/two;E:\three)"));
}
#endif
