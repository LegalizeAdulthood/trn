// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/ActiveRecordEditor.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace
{

class ActiveRecordEditorTest : public testing::Test
{
protected:
    std::string        m_text;
    ActiveRecordEditor m_editor{m_text, std::string_view{"comp.test"}.size()};
};

struct LowWaterCase
{
    const char *name;
    const char *cached;
    const char *source;
    const char *expected;
};

class LowWaterUpdateTest : public ActiveRecordEditorTest, public testing::WithParamInterface<LowWaterCase>
{
protected:
    void SetUp() override
    {
        m_text = GetParam().cached;
    }
};

TEST_P(LowWaterUpdateTest, changesOnlyTheExistingLowWaterField)
{
    m_editor.update_low(GetParam().source);

    EXPECT_EQ(GetParam().expected, m_text);
}

INSTANTIATE_TEST_SUITE_P(
    RecordLayout, LowWaterUpdateTest,
    testing::Values(
        LowWaterCase{"EqualWidth", "comp.test 00042 00007 y\n", "comp.test 00098 00023 m\n",
                     "comp.test 00042 00023 y\n"},
        LowWaterCase{"ZeroPadding", "comp.test 00042 00007 y\n", "comp.test 98 9 m\n", "comp.test 00042 00009 y\n"},
        LowWaterCase{"RightmostDigits", "comp.test 00042 00007 y\n", "comp.test 98 123456 m\n",
                     "comp.test 00042 23456 y\n"},
        LowWaterCase{"LiteralField", "comp.test 00042 00007 y\n", "comp.test 98 lo m\n", "comp.test 00042 000lo y\n"},
        LowWaterCase{"EmptySourceField", "comp.test 00042 00007 y\n", "comp.test 98  m\n", "comp.test 00042 00000 y\n"},
        LowWaterCase{"EmptyCachedField", "comp.test 00042  y\n", "comp.test 98 9 m\n", "comp.test 00042  y\n"},
        LowWaterCase{"UnterminatedRecord", "comp.test 00042 00007 y", "comp.test 98 9 m\n", "comp.test 00042 00009 y"},
        LowWaterCase{"NoSourceSeparator", "comp.test 00042 00007 y\n", "comp.test", "comp.test 00042 00007 y\n"},
        LowWaterCase{"LeadingSourceSeparator", "comp.test 00042 00007 y\n", " y\n", "comp.test 00042 00007 y\n"},
        LowWaterCase{"NoSourceLowSeparator", "comp.test 00042 00007 y\n", "comp.test y\n", "comp.test 00042 00007 y\n"},
        LowWaterCase{"NoCachedSeparator", "comp.test", "comp.test 98 9 m\n", "comp.test"},
        LowWaterCase{"LeadingCachedSeparator", " y\n", "comp.test 98 9 m\n", " y\n"},
        LowWaterCase{"NoCachedLowSeparator", "comp.test y\n", "comp.test 98 9 m\n", "comp.test y\n"}),
    [](const testing::TestParamInfo<LowWaterCase> &info) { return info.param.name; });

} // namespace
