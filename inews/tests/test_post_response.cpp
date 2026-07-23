/* test_post_response.cpp
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <inews/post_response.h>

#include <gtest/gtest.h>

TEST(InewsPostResponseTest, parsesStatusCode)
{
    EXPECT_EQ(441, inews_post_response_code("441 posting failed\r\n"));
    EXPECT_EQ(500, inews_post_response_code("500 command failed"));
}

TEST(InewsPostResponseTest, treatsMissingStatusAsZero)
{
    EXPECT_EQ(0, inews_post_response_code(""));
    EXPECT_EQ(0, inews_post_response_code("posting failed"));
}

TEST(InewsPostResponseTest, extractsFailureMessage)
{
    EXPECT_EQ("posting failed", inews_post_failure_message("441 posting failed\r\n"));
}

TEST(InewsPostResponseTest, expandsEscapedNewlinesInFailureMessage)
{
    EXPECT_EQ("first\nsecond", inews_post_failure_message("441 first\\second\r\n"));
}
