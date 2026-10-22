// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSectionValues.h>

#include <gtest/gtest.h>

#include <optional>
#include <string>
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

class ResetSectionValuesTest : public testing::Test
{
protected:
    void SetUp() override;

    const IniSchema  m_schema{make_schema()};
    std::string      m_alpha{"alpha"};
    std::string      m_beta{"beta"};
    IniSectionValues m_values;
};

void ResetSectionValuesTest::SetUp()
{
    const IniField *alpha_field = m_schema.find("Alpha Key");
    ASSERT_NE(nullptr, alpha_field);
    ASSERT_TRUE(m_values.set(*alpha_field, m_alpha));
}

} // namespace

TEST(IniSectionValuesTest, storesOwnedValuesByFieldId)
{
    const IniSchema  schema = make_schema();
    std::string      alpha{"alpha"};
    IniSectionValues values;
    const IniField  *alpha_field = schema.find("Alpha Key");

    ASSERT_NE(nullptr, alpha_field);
    ASSERT_TRUE(values.set(*alpha_field, alpha));
    alpha[0] = 'z';

    const std::optional<std::string_view> value = values.value(TF_ALPHA);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"alpha"}, *value);
    EXPECT_NE(alpha.data(), value->data());
    EXPECT_TRUE(values.contains(TF_ALPHA));
    EXPECT_FALSE(values.contains(TF_BETA));
    EXPECT_FALSE(values.value(TF_BETA).has_value());
}

TEST_F(ResetSectionValuesTest, resetClearsValuesWithoutTouchingInputText)
{
    m_values.reset();

    EXPECT_EQ(std::string_view{"alpha"}, m_alpha);
    EXPECT_FALSE(m_values.value(TF_ALPHA).has_value());
    EXPECT_EQ(0U, m_values.size());
}

TEST_F(ResetSectionValuesTest, storesADifferentOwnedValueAfterReset)
{
    m_values.reset();
    const IniField *beta_field = m_schema.find("Beta Key");
    ASSERT_NE(nullptr, beta_field);

    const bool stored = m_values.set(*beta_field, m_beta);

    EXPECT_TRUE(stored);
    const std::optional<std::string_view> value = m_values.value(TF_BETA);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(std::string_view{"beta"}, *value);
    EXPECT_NE(m_beta.data(), value->data());
}

TEST(IniSectionValuesTest, displayGroupsAreNotValues)
{
    const IniSchema  schema = make_schema();
    IniSectionValues values;

    EXPECT_FALSE(values.set(schema.field(0), "ignored"));

    EXPECT_EQ(0U, values.size());
    EXPECT_FALSE(values.contains(schema.field(0).id()));
}
