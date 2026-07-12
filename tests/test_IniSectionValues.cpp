// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSectionValues.h>

#include <gtest/gtest.h>

#include <optional>
#include <string_view>

namespace
{

enum TestField
{
    TF_ALPHA = 1,
    TF_BETA = 7
};

IniSchema make_schema()
{
    return IniSchema{"test",
                     {
                         IniField::group("Display Group"),
                         IniField::value(TF_ALPHA, "Alpha Key"),
                         IniField::value(TF_BETA, "Beta Key"),
                     }};
}

} // namespace

TEST(IniSectionValuesTest, storesBorrowedValuesByFieldId)
{
    const IniSchema  schema = make_schema();
    char             alpha[]{"alpha"};
    IniSectionValues values;
    const IniField  *alpha_field = schema.find("Alpha Key");

    ASSERT_NE(nullptr, alpha_field);
    ASSERT_TRUE(values.set(*alpha_field, alpha));

    const std::optional<std::string_view> value = values.value(TF_ALPHA);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"alpha"}, *value);
    EXPECT_EQ(alpha, value->data());
    EXPECT_EQ(alpha, values.c_str(TF_ALPHA));
    EXPECT_TRUE(values.contains(TF_ALPHA));
    EXPECT_FALSE(values.contains(TF_BETA));
    EXPECT_EQ(nullptr, values.c_str(TF_BETA));
}

TEST(IniSectionValuesTest, resetClearsValuesWithoutTouchingInputText)
{
    const IniSchema  schema = make_schema();
    char             alpha[]{"alpha"};
    char             beta[]{"beta"};
    IniSectionValues values;
    const IniField  *alpha_field = schema.find("Alpha Key");
    const IniField  *beta_field = schema.find("Beta Key");

    ASSERT_NE(nullptr, alpha_field);
    ASSERT_NE(nullptr, beta_field);
    ASSERT_TRUE(values.set(*alpha_field, alpha));
    values.reset();

    EXPECT_EQ(std::string_view{"alpha"}, std::string_view{alpha});
    EXPECT_FALSE(values.value(TF_ALPHA).has_value());
    EXPECT_EQ(nullptr, values.c_str(TF_ALPHA));
    EXPECT_EQ(0U, values.size());

    ASSERT_TRUE(values.set(*beta_field, beta));
    const std::optional<std::string_view> value = values.value(TF_BETA);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"beta"}, *value);
    EXPECT_EQ(beta, value->data());
}

TEST(IniSectionValuesTest, displayGroupsAreNotValues)
{
    const IniSchema  schema = make_schema();
    IniSectionValues values;

    EXPECT_FALSE(values.set(schema.field(0), "ignored"));

    EXPECT_EQ(0U, values.size());
    EXPECT_FALSE(values.contains(schema.field(0).id()));
}
