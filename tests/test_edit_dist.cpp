// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/edit_dist.h>

#include <gtest/gtest.h>

#include <string>

TEST(EditDistanceTest, handlesEmptyInputs)
{
    EXPECT_EQ(0, edit_distn("", ""));
    EXPECT_EQ(3, edit_distn("", "abc"));
    EXPECT_EQ(3, edit_distn("abc", ""));
}

TEST(EditDistanceTest, handlesCommonEdits)
{
    EXPECT_EQ(0, edit_distn("comp.lang.c", "comp.lang.c"));
    EXPECT_EQ(2, edit_distn("comp.lang.c", "comp.lang.cpp"));
    EXPECT_EQ(1, edit_distn("comp.lang.c", "comp.lang.x"));
    EXPECT_EQ(1, edit_distn("ab", "ba"));
    EXPECT_EQ(3, edit_distn("kitten", "sitting"));
}

TEST(EditDistanceTest, handlesLongInputs)
{
    std::string from(600, 'a');
    std::string to{from};

    to[300] = 'b';

    EXPECT_EQ(1, edit_distn(from, to));
}
