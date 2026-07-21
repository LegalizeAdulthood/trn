// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/color.h>

#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

namespace
{

void add_color_capabilities()
{
    add_tc_string("fg red", "<fg-red>");
    add_tc_string("bg blue", "<bg-blue>");
}

} // namespace

TEST(ColorAttributeTest, parsesForegroundAndBackgroundCapabilities)
{
    EXPECT_EXIT(
        {
            add_tc_string("fg default", "<fg-default>");
            add_tc_string("bg default", "<bg-default>");
            add_color_capabilities();

            color_rc_attribute("notice", "n red blue");

            testing::internal::CaptureStdout();
            color_init();
            (void) testing::internal::GetCapturedStdout();

            testing::internal::CaptureStdout();
            color_string(COLOR_NOTICE, "sample");
            const std::string output = testing::internal::GetCapturedStdout();
            if (output != "<fg-red><bg-blue>sample<fg-default><bg-default>")
            {
                std::exit(2);
            }
            std::exit(0);
        },
        ::testing::ExitedWithCode(0), ".*");
}

TEST(ColorAttributeTest, rejectsUnknownObject)
{
    EXPECT_EXIT(
        {
            color_rc_attribute("missing", "n red blue");
            std::exit(0);
        },
        ::testing::ExitedWithCode(1), "unknown object");
}

TEST(ColorAttributeTest, rejectsBadAttribute)
{
    EXPECT_EXIT(
        {
            color_rc_attribute("notice", "x red blue");
            std::exit(0);
        },
        ::testing::ExitedWithCode(1), "bad attribute");
}

TEST(ColorAttributeTest, rejectsMissingBackgroundColor)
{
    EXPECT_EXIT(
        {
            add_color_capabilities();

            color_rc_attribute("notice", "n red");
            std::exit(0);
        },
        ::testing::ExitedWithCode(1), "wrong number of parameters");
}

TEST(ColorAttributeTest, rejectsExtraColorParameter)
{
    EXPECT_EXIT(
        {
            add_color_capabilities();

            color_rc_attribute("notice", "n red blue extra");
            std::exit(0);
        },
        ::testing::ExitedWithCode(1), "wrong number of parameters");
}

TEST(ColorAttributeTest, rejectsUnknownForegroundColor)
{
    EXPECT_EXIT(
        {
            add_color_capabilities();

            color_rc_attribute("notice", "n chartreuse blue");
            std::exit(0);
        },
        ::testing::ExitedWithCode(1), "no color 'fg chartreuse'");
}

TEST(ColorAttributeTest, rejectsUnknownBackgroundColor)
{
    EXPECT_EXIT(
        {
            add_color_capabilities();

            color_rc_attribute("notice", "n red chartreuse");
            std::exit(0);
        },
        ::testing::ExitedWithCode(1), "no color 'bg chartreuse'");
}
