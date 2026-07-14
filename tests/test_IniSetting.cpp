// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSetting.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace
{

std::string setting_value(std::string_view name, std::string_view raw_value)
{
    const IniSetting setting{name, raw_value};
    return setting.value();
}

} // namespace

TEST(IniSettingTest, exposesName)
{
    const IniSetting setting{"Alpha Key", "one"};

    EXPECT_EQ(std::string_view{"Alpha Key"}, setting.name());
}

TEST(IniSettingTest, normalizesValueText)
{
    EXPECT_EQ("one", setting_value("Alpha Key", " one # comment"));
    EXPECT_EQ("two # not comment", setting_value("Beta Key", " \"two # not comment\" # comment"));
    EXPECT_EQ("hello\nthere", setting_value("Escaped Key", " hello\\nthere"));
    EXPECT_EQ("line continued", setting_value("Continued Key", " line\\\n continued"));
    EXPECT_EQ("", setting_value("Empty Key", " # comment"));
    EXPECT_EQ("text", setting_value("Trailing Key", " text     "));
    EXPECT_EQ("  padded  ", setting_value("Quoted Key", " '  padded  '  # tail"));
}

TEST(IniSettingTest, normalizesNumericEscapes)
{
    EXPECT_EQ("AA", setting_value("Octal Key", " \\101A"));
    EXPECT_EQ("A!", setting_value("Hex Key", " \\x41!"));
}

TEST(IniSettingTest, preservesTrailingBackslash)
{
    EXPECT_EQ("\\", setting_value("Slash Key", " \\"));
}
