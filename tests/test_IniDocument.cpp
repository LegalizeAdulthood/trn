// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>

#include <file_contents.h>

#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

enum TestIniField
{
    TEST_FIELD_ALPHA = 1,
    TEST_FIELD_BETA
};

std::filesystem::path test_file_path()
{
    return std::filesystem::temp_directory_path() / "trn-IniDocumentTest.ini";
}

} // namespace

TEST(IniDocumentTest, iteratesTokenizedSections)
{
    IniDocument document{"# ignored\n"
                         "[first] enabled\n"
                         "Alpha Key = one\n"
                         "Beta Key = two\n"
                         "\n"
                         "[second]\n"
                         "Gamma = three\n",
                         "test input"};

    IniDocument::Section section;
    ASSERT_TRUE(document.next_section(section));
    EXPECT_STREQ("first", section.name);
    EXPECT_STREQ("enabled", section.condition);
    EXPECT_TRUE(section.has_condition());
    EXPECT_STREQ("Alpha Key", section.body);
    EXPECT_STREQ("one", section.body + std::strlen(section.body) + 1);

    ASSERT_TRUE(document.next_section(section));
    EXPECT_STREQ("second", section.name);
    EXPECT_STREQ("", section.condition);
    EXPECT_FALSE(section.has_condition());
    EXPECT_STREQ("Gamma", section.body);
    EXPECT_STREQ("three", section.body + std::strlen(section.body) + 1);

    EXPECT_FALSE(document.next_section(section));
}

TEST(IniDocumentTest, parsesSectionValuesFromSectionBody)
{
    IniDocument     document{"[test]\nAlpha Key = one\nBeta Key = two\n", "test input"};
    const IniSchema schema{
        "test", {IniField::value(TEST_FIELD_ALPHA, "Alpha Key"), IniField::value(TEST_FIELD_BETA, "Beta Key")}};
    IniSectionValues values;

    IniDocument::Section section;
    ASSERT_TRUE(document.next_section(section));
    parse_ini_section(section.body, schema, values);

    const auto alpha = values.value(TEST_FIELD_ALPHA);
    const auto beta = values.value(TEST_FIELD_BETA);
    ASSERT_TRUE(alpha.has_value());
    ASSERT_TRUE(beta.has_value());
    EXPECT_EQ(std::string_view{"one"}, *alpha);
    EXPECT_EQ(std::string_view{"two"}, *beta);
}

TEST(IniDocumentTest, parsesFileContents)
{
    const std::filesystem::path path = test_file_path();
    std::error_code             error;
    std::filesystem::remove(path, error);

    std::ofstream output{path};
    ASSERT_TRUE(output.good());
    output << "[file]\nValue = text\n";
    output.close();

    IniDocument document{file_contents(path), "test file"};

    IniDocument::Section section;
    ASSERT_TRUE(document.next_section(section));
    EXPECT_STREQ("file", section.name);
    EXPECT_STREQ("Value", section.body);

    std::filesystem::remove(path, error);
}
