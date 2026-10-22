// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSetting.h>

#include <gtest/gtest.h>

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

namespace
{

std::string setting_value(std::string_view name, std::string_view raw_value)
{
    const IniSetting setting{name, raw_value};
    return setting.value();
}

struct SettingNormalizationCase
{
    const char *name;
    const char *raw_value;
    const char *expected;
};

void PrintTo(const SettingNormalizationCase &value, std::ostream *stream)
{
    *stream << value.name;
}

class SettingNormalizationTest : public testing::TestWithParam<SettingNormalizationCase>
{
protected:
    void SetUp() override;

    std::string               m_source;
    std::optional<IniSetting> m_setting;
};

void SettingNormalizationTest::SetUp()
{
    m_source = GetParam().raw_value;
    m_setting.emplace(GetParam().name, m_source);
}

} // namespace

TEST(IniSettingTest, exposesName)
{
    const IniSetting setting{"Alpha Key", "one"};

    EXPECT_EQ(std::string_view{"Alpha Key"}, setting.name());
}

TEST_P(SettingNormalizationTest, normalizesTheSettingValue)
{
    const std::string value = m_setting->value();

    EXPECT_EQ(GetParam().expected, value);
}

INSTANTIATE_TEST_SUITE_P(
    ValueText, SettingNormalizationTest,
    testing::Values(SettingNormalizationCase{"Alpha Key", " one # comment", "one"},
                    SettingNormalizationCase{"Beta Key", " \"two # not comment\" # comment", "two # not comment"},
                    SettingNormalizationCase{"Escaped Key", " hello\\nthere", "hello\nthere"},
                    SettingNormalizationCase{"Continued Key", " line\\\n continued", "line continued"},
                    SettingNormalizationCase{"Empty Key", " # comment", ""},
                    SettingNormalizationCase{"Trailing Key", " text     ", "text"},
                    SettingNormalizationCase{"Quoted Key", " '  padded  '  # tail", "  padded  "}));

INSTANTIATE_TEST_SUITE_P(NumericEscapes, SettingNormalizationTest,
                         testing::Values(SettingNormalizationCase{"Octal Key", " \\101A", "AA"},
                                         SettingNormalizationCase{"Hex Key", " \\x41!", "A!"}));

TEST(IniSettingTest, preservesTrailingBackslash)
{
    EXPECT_EQ("\\", setting_value("Slash Key", " \\"));
}
