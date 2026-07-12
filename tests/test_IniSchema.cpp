// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSchema.h>

#include <gtest/gtest.h>

#include <string_view>

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

} // namespace

TEST(IniSchemaTest, lookupIsCaseInsensitive)
{
    const IniSchema schema = make_schema();

    const IniField *alpha = schema.find("ALPHA KEY");
    const IniField *beta = schema.find("beta key");

    ASSERT_NE(nullptr, alpha);
    ASSERT_NE(nullptr, beta);
    EXPECT_EQ(TF_ALPHA, alpha->id());
    EXPECT_EQ(std::string_view{"Alpha Key"}, alpha->name());
    EXPECT_EQ(std::string_view{"alpha help"}, alpha->help());
    EXPECT_EQ(TF_BETA, beta->id());
}

TEST(IniSchemaTest, displayGroupsAreNotLookupEntries)
{
    const IniSchema schema = make_schema();

    EXPECT_EQ(nullptr, schema.find("Display Group"));
    EXPECT_EQ(nullptr, schema.find("Missing Key"));
}

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
