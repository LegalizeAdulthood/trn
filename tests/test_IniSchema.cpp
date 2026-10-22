// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSchema.h>

#include <gtest/gtest.h>

#include <string_view>
#include <tuple>

namespace
{

enum TestField
{
    TF_ALPHA = 1,
    TF_BETA = 2
};

IniSchema make_schema()
{
    return IniSchema{"test",
                     {
                         IniField::group("Display Group"),
                         IniField::value(TF_ALPHA, "Alpha Key", "alpha help"),
                         IniField::value(TF_BETA, "Beta Key"),
                     }};
}

class IniSchemaTest : public testing::Test
{
protected:
    const IniSchema m_schema{make_schema()};
};

using SchemaLookup = std::tuple<std::string_view, TestField, std::string_view, std::string_view>;

class SchemaLookupTest : public IniSchemaTest, public testing::WithParamInterface<SchemaLookup>
{
};

class MissingSchemaKeyTest : public IniSchemaTest, public testing::WithParamInterface<std::string_view>
{
};

} // namespace

TEST_P(SchemaLookupTest, findsTheFieldRegardlessOfKeyCase)
{
    const auto &[key, id, name, help] = GetParam();

    const IniField *field = m_schema.find(key);

    ASSERT_NE(nullptr, field);
    EXPECT_EQ(id, field->id());
    EXPECT_EQ(name, field->name());
    EXPECT_EQ(help, field->help());
}

INSTANTIATE_TEST_SUITE_P(CaseVariants, SchemaLookupTest,
                         testing::Values(SchemaLookup{"ALPHA KEY", TF_ALPHA, "Alpha Key", "alpha help"},
                                         SchemaLookup{"beta key", TF_BETA, "Beta Key", ""}));

TEST_P(MissingSchemaKeyTest, doesNotFindANonValueKey)
{
    const IniField *field = m_schema.find(GetParam());

    EXPECT_EQ(nullptr, field);
}

INSTANTIATE_TEST_SUITE_P(DisplayGroupAndAbsentKey, MissingSchemaKeyTest,
                         testing::Values("Display Group", "Missing Key"));

TEST(IniSchemaTest, fieldsPreserveDisplayOrder)
{
    const IniSchema schema = make_schema();

    ASSERT_EQ(3U, schema.size());
    EXPECT_EQ(std::string_view{"test"}, schema.section_name());
    EXPECT_TRUE(schema.field(0).is_group());
    EXPECT_EQ(std::string_view{"Display Group"}, schema.field(0).name());
    EXPECT_TRUE(schema.field(1).is_value());
    EXPECT_EQ(TF_ALPHA, schema.field(1).id());
    EXPECT_EQ(TF_BETA, schema.field(2).id());
}
