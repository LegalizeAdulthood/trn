// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/RcGroupConfig.h>

#include <trn/IniDocument.h>
#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <string_view>

namespace
{

class RcGroupConfigTest : public testing::Test
{
protected:
    RcGroupConfig parse(const char *text)
    {
        IniDocument document{text, "test input"};

        IniDocument::Iterator iterator = document.begin();
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

private:
    IniSectionValues m_values;
};

} // namespace

TEST_F(RcGroupConfigTest, schemaRecognizesGroupFields)
{
    const IniSchema &schema = RcGroupConfig::schema();

    ASSERT_NE(nullptr, schema.find("ID"));
    ASSERT_NE(nullptr, schema.find("Newsrc"));
    ASSERT_NE(nullptr, schema.find("Add Groups"));
    EXPECT_EQ(static_cast<int>(RcGroupConfigField::AddGroups), schema.find("Add Groups")->id());
}

TEST_F(RcGroupConfigTest, parsesValuesIntoNamedFields)
{
    const RcGroupConfig config = parse("[Group 1]\n"
                                       "ID = test-local\n"
                                       "Newsrc = /home/me/.newsrc\n"
                                       "Add Groups = manual\n");

    EXPECT_STREQ("test-local", config.id());
    EXPECT_STREQ("/home/me/.newsrc", config.newsrc());
    EXPECT_STREQ("manual", config.add_groups());
}

TEST_F(RcGroupConfigTest, missingValuesRemainNull)
{
    const RcGroupConfig config = parse("[Group 1]\nID = default\n");

    EXPECT_STREQ("default", config.id());
    EXPECT_EQ(nullptr, config.newsrc());
    EXPECT_EQ(nullptr, config.add_groups());
}
