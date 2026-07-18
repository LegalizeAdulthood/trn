// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/color.h>

#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>

TEST(ColorAttributeTest, parsesForegroundAndBackgroundCapabilities)
{
    EXPECT_EXIT(
        {
            add_tc_string("fg default", "<fg-default>");
            add_tc_string("bg default", "<bg-default>");
            add_tc_string("fg red", "<fg-red>");
            add_tc_string("bg blue", "<bg-blue>");

            char value[]{"n red blue"};
            color_rc_attribute("notice", value);

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
