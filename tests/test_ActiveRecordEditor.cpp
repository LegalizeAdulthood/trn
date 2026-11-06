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

struct HighWaterCase
{
    const char *name;
    const char *text;
    long        expected;
};

class HighWaterReadTest : public ActiveRecordEditorTest, public testing::WithParamInterface<HighWaterCase>
{
protected:
    void SetUp() override
    {
        m_text = GetParam().text;
    }
};

TEST_P(HighWaterReadTest, readsTheHighWaterValueWithoutChangingTheRecord)
{
    const ArticleNum high = m_editor.high();

    EXPECT_EQ(ArticleNum{GetParam().expected}, high);
    EXPECT_EQ(GetParam().text, m_text);
}

INSTANTIATE_TEST_SUITE_P(NumericFields, HighWaterReadTest,
                         testing::Values(HighWaterCase{"Decimal", "comp.test 42 7 y\n", 42},
                                         HighWaterCase{"LeadingZeros", "comp.test 00042 00007 y\n", 42},
                                         HighWaterCase{"Zero", "comp.test 00000 7 y\n", 0},
                                         HighWaterCase{"NumericPrefix", "comp.test 42suffix 7 y\n", 42},
                                         HighWaterCase{"Negative", "comp.test -12 7 y\n", -12},
                                         HighWaterCase{"LeadingPlus", "comp.test +42 7 y\n", 0},
                                         HighWaterCase{"LeadingSpace", "comp.test  42 7 y\n", 0},
                                         HighWaterCase{"MissingField", "comp.test", 0},
                                         HighWaterCase{"EmptyField", "comp.test ", 0},
                                         HighWaterCase{"ShortRecord", "short", 0},
                                         HighWaterCase{"Nonnumeric", "comp.test missing 7 y\n", 0},
                                         HighWaterCase{"Overflow", "comp.test 999999999999999999999999999 7 y\n", 0},
                                         HighWaterCase{"Underflow", "comp.test -999999999999999999999999999 7 y\n", 0},
                                         HighWaterCase{"NameBoundaryByte", "comp.testX42 7 y\n", 42}),
                         [](const testing::TestParamInfo<HighWaterCase> &info) { return info.param.name; });

} // namespace
