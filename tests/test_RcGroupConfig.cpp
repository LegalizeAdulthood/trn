// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/RcGroupConfig.h>

#include <trn/IniDocument.h>
#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <ostream>
#include <string_view>
#include <tuple>

namespace
{

class RcGroupConfigTest : public testing::Test
{
protected:
    RcGroupConfig parse(const char *text);

private:
    IniSectionValues m_values;
};

RcGroupConfig RcGroupConfigTest::parse(const char *text)
{
    IniDocument document{text, "test input"};

    auto iterator = document.begin();
    if (iterator == document.end())
    {
        ADD_FAILURE() << "missing section";
        return {};
    }
    const IniSection &section = *iterator;
    EXPECT_EQ(std::string_view{"Group 1"}, section.name());
    EXPECT_TRUE(section.condition().empty());

    parse_ini_section(section, RcGroupConfig::schema(), m_values);
    return RcGroupConfig::from(m_values);
}

struct RcGroupFieldCase
{
    const char        *name;
    RcGroupConfigField id;
    std::string_view (RcGroupConfig::*getter)() const;
    const char *configured;
    const char *missing;
};

void PrintTo(const RcGroupFieldCase &value, std::ostream *stream)
{
    *stream << value.name;
}

const RcGroupFieldCase GROUP_FIELDS[]{
    {"ID", RcGroupConfigField::Id, &RcGroupConfig::id, "test-local", "default"},
    {"Newsrc", RcGroupConfigField::Newsrc, &RcGroupConfig::newsrc, "/home/me/.newsrc", ""},
    {"Add Groups", RcGroupConfigField::AddGroups, &RcGroupConfig::add_groups, "manual", ""}};

class RcGroupSchemaFieldTest : public RcGroupConfigTest, public testing::WithParamInterface<RcGroupFieldCase>
{
};

class ParsedRcGroupFieldTest : public RcGroupConfigTest,
                               public testing::WithParamInterface<std::tuple<bool, RcGroupFieldCase>>
{
protected:
    void SetUp() override;

    RcGroupConfig m_config;
};

void ParsedRcGroupFieldTest::SetUp()
{
    m_config = parse(std::get<0>(GetParam()) ? "[Group 1]\n"
                                               "ID = test-local\n"
                                               "Newsrc = /home/me/.newsrc\n"
                                               "Add Groups = manual\n"
                                             : "[Group 1]\nID = default\n");
}

} // namespace

TEST_P(RcGroupSchemaFieldTest, recognizesTheNamedGroupField)
{
    const IniField *field = RcGroupConfig::schema().find(GetParam().name);

    ASSERT_NE(nullptr, field);
    EXPECT_EQ(static_cast<int>(GetParam().id), field->id());
}

INSTANTIATE_TEST_SUITE_P(FieldNames, RcGroupSchemaFieldTest, testing::ValuesIn(GROUP_FIELDS));

TEST_P(ParsedRcGroupFieldTest, returnsTheConfiguredOrMissingFieldValue)
{
    const RcGroupFieldCase &field = std::get<1>(GetParam());

    const std::string_view value = (m_config.*field.getter)();

    EXPECT_EQ(std::get<0>(GetParam()) ? field.configured : field.missing, value);
}

INSTANTIATE_TEST_SUITE_P(CompleteAndDefaults, ParsedRcGroupFieldTest,
                         testing::Combine(testing::Bool(), testing::ValuesIn(GROUP_FIELDS)));
