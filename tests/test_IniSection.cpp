// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniSection.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

TEST(IniSectionTest, exposesSectionMetadata)
{
    const IniSection section{"options", "%{TERM}=xterm", ""};

    EXPECT_EQ(std::string_view{"options"}, section.name());
    EXPECT_EQ(std::string_view{"%{TERM}=xterm"}, section.condition());
    EXPECT_TRUE(section.has_condition());
}

TEST(IniSectionTest, detectsMissingCondition)
{
    const IniSection section{"options", "", ""};

    EXPECT_FALSE(section.has_condition());
}

TEST(IniSectionTest, iteratesSettings)
{
    const std::string body{"# ignored\n"
                           "Alpha Key = one # comment\n"
                           "No Equals\n"
                           "\n"
                           "Beta Key=\"two # not comment\"\n"
                           "Wide   Key = spaced\n"
                           "  Trailing Key  = text     \n"};
    const IniSection  section{"test", "", body};

    std::vector<std::string> names;
    std::vector<std::string> values;
    for (const IniSetting setting : section)
    {
        names.emplace_back(setting.name());
        values.emplace_back(setting.value());
    }

    ASSERT_EQ(4, names.size());
    EXPECT_EQ("Alpha Key", names[0]);
    EXPECT_EQ("Beta Key", names[1]);
    EXPECT_EQ("Wide   Key", names[2]);
    EXPECT_EQ("Trailing Key", names[3]);
    EXPECT_EQ("one", values[0]);
    EXPECT_EQ("two # not comment", values[1]);
    EXPECT_EQ("spaced", values[2]);
    EXPECT_EQ("text", values[3]);
}

TEST(IniSectionTest, continuesValuesAcrossLines)
{
    const std::string    body{"Continued Key = line\\\n"
                              " continued\n"
                              "Next Key = done\n"};
    const IniSection     section{"test", "", body};
    IniSection::Iterator iterator = section.begin();

    ASSERT_NE(section.end(), iterator);
    EXPECT_EQ(std::string_view{"Continued Key"}, (*iterator).name());
    EXPECT_EQ("line continued", (*iterator).value());

    ++iterator;
    ASSERT_NE(section.end(), iterator);
    EXPECT_EQ(std::string_view{"Next Key"}, (*iterator).name());
    EXPECT_EQ("done", (*iterator).value());
}

TEST(IniSectionTest, doesNotModifySectionBody)
{
    const std::string body{"Alpha Key = one\n"
                           "Beta Key = two\n"};
    const std::string copy{body};
    const IniSection  section{"test", "", body};

    for (const IniSetting setting : section)
    {
        static_cast<void>(setting.value());
    }

    EXPECT_EQ(copy, body);
}
