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
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

enum TestIniField
{
    TEST_FIELD_ALPHA = 1,
    TEST_FIELD_BETA,
    TEST_FIELD_CONTINUED,
    TEST_FIELD_EMPTY,
    TEST_FIELD_ESCAPED,
    TEST_FIELD_MIXED_CASE,
    TEST_FIELD_QUOTED,
    TEST_FIELD_TRAILING
};

std::filesystem::path test_file_path()
{
    return std::filesystem::temp_directory_path() / "trn-IniDocumentTest.ini";
}

void expect_value(const IniSectionValues &values, int field_id, std::string_view expected)
{
    const std::optional<std::string_view> actual = values.value(field_id);
    ASSERT_TRUE(actual.has_value());
    EXPECT_EQ(expected, *actual);
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

TEST(IniDocumentTest, normalizesParsedValues)
{
    IniDocument     document{"[test]\n"
                             "Alpha Key = one # comment\n"
                             "Beta Key = \"two # not comment\" # comment\n"
                             "Escaped Key = hello\\nthere\n"
                             "Continued Key = line\\\n"
                             " continued\n"
                             "Empty Key = # comment\n"
                             "Trailing Key = text     \n"
                             "Quoted Key = '  padded  '  # tail\n",
                             "test input"};
    const IniSchema schema{
        "test",
        {IniField::value(TEST_FIELD_ALPHA, "Alpha Key"), IniField::value(TEST_FIELD_BETA, "Beta Key"),
         IniField::value(TEST_FIELD_CONTINUED, "Continued Key"), IniField::value(TEST_FIELD_EMPTY, "Empty Key"),
         IniField::value(TEST_FIELD_ESCAPED, "Escaped Key"), IniField::value(TEST_FIELD_QUOTED, "Quoted Key"),
         IniField::value(TEST_FIELD_TRAILING, "Trailing Key")}};
    IniSectionValues values;

    IniDocument::Section section;
    ASSERT_TRUE(document.next_section(section));
    parse_ini_section(section.body, schema, values);

    EXPECT_EQ(6, values.size());
    expect_value(values, TEST_FIELD_ALPHA, "one");
    expect_value(values, TEST_FIELD_BETA, "two # not comment");
    expect_value(values, TEST_FIELD_CONTINUED, "line continued");
    EXPECT_FALSE(values.contains(TEST_FIELD_EMPTY));
    expect_value(values, TEST_FIELD_ESCAPED, "hello\nthere");
    expect_value(values, TEST_FIELD_QUOTED, "  padded  ");
    expect_value(values, TEST_FIELD_TRAILING, "text");
}

TEST(IniDocumentTest, matchesSchemaNamesCaseInsensitivelyAndReportsUnknownFields)
{
    IniDocument      document{"[test]\n"
                              "MIXED CASE KEY = yes\n"
                              "Unknown Key = ignored\n",
                              "test input"};
    const IniSchema  schema{"test", {IniField::value(TEST_FIELD_MIXED_CASE, "Mixed Case Key")}};
    IniSectionValues values;

    IniDocument::Section section;
    ASSERT_TRUE(document.next_section(section));

    testing::internal::CaptureStdout();
    parse_ini_section(section.body, schema, values);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(1, values.size());
    expect_value(values, TEST_FIELD_MIXED_CASE, "yes");
    EXPECT_NE(std::string::npos, output.find("Unknown option: `unknown key'."));
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
