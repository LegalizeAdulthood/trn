// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>

#include <trn/file-contents.h>
#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

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

constexpr const char *RANGE_INPUT = "[test]\n"
                                    "Alpha Key = one\n"
                                    "Empty Key = # comment\n"
                                    "Unknown Key = ignored\n";
constexpr const char *MIXED_CASE_INPUT = "[test]\n"
                                         "MIXED CASE KEY = yes\n"
                                         "Unknown Key = ignored\n";

class ParsedIniSectionTest : public testing::Test
{
protected:
    void parse(std::string contents);

    IniDocument     m_document;
    const IniSchema m_schema{
        "test",
        {IniField::value(TEST_FIELD_ALPHA, "Alpha Key"), IniField::value(TEST_FIELD_BETA, "Beta Key"),
         IniField::value(TEST_FIELD_CONTINUED, "Continued Key"), IniField::value(TEST_FIELD_EMPTY, "Empty Key"),
         IniField::value(TEST_FIELD_ESCAPED, "Escaped Key"), IniField::value(TEST_FIELD_MIXED_CASE, "Mixed Case Key"),
         IniField::value(TEST_FIELD_QUOTED, "Quoted Key"), IniField::value(TEST_FIELD_TRAILING, "Trailing Key")}};
    IniSectionValues m_values;
    bool             m_parsed{};
    std::string      m_output;
};

void ParsedIniSectionTest::parse(std::string contents)
{
    m_document = IniDocument{std::move(contents), "test input"};
    const IniSection section = *m_document.begin();
    testing::internal::CaptureStdout();
    m_parsed = parse_ini_section(section, m_schema, m_values);
    m_output = testing::internal::GetCapturedStdout();
}

using ParsedIniValue = std::tuple<const char *, TestIniField, std::string_view>;

class ParsedIniValueTest : public ParsedIniSectionTest, public testing::WithParamInterface<ParsedIniValue>
{
};

class UnknownIniFieldTest : public ParsedIniSectionTest, public testing::WithParamInterface<const char *>
{
};

} // namespace

TEST(IniDocumentTest, iteratesSectionsWithRangeFor)
{
    IniDocument document{"# ignored\n"
                         "[first] enabled # trailing comment\n"
                         "Alpha Key = one\n"
                         "Wide   Key = spaced\n"
                         "\n"
                         "[second]\n"
                         "Gamma = three\n",
                         "test input"};

    std::vector<std::string> names;
    std::vector<std::string> conditions;
    std::vector<std::string> setting_names;
    std::vector<std::string> setting_values;
    for (const IniSection section : document)
    {
        names.emplace_back(section.name());
        conditions.emplace_back(section.condition());
        for (const IniSetting setting : section)
        {
            setting_names.emplace_back(setting.name());
            setting_values.emplace_back(setting.value());
        }
    }

    ASSERT_EQ(2, names.size());
    EXPECT_EQ("first", names[0]);
    EXPECT_EQ("enabled", conditions[0]);
    EXPECT_EQ("second", names[1]);
    EXPECT_EQ("", conditions[1]);

    ASSERT_EQ(3, setting_names.size());
    EXPECT_EQ("Alpha Key", setting_names[0]);
    EXPECT_EQ("Wide   Key", setting_names[1]);
    EXPECT_EQ("Gamma", setting_names[2]);
    EXPECT_EQ("one", setting_values[0]);
    EXPECT_EQ("spaced", setting_values[1]);
    EXPECT_EQ("three", setting_values[2]);
}

TEST(IniDocumentTest, parsesSectionValues)
{
    IniDocument     document{"[test]\nAlpha Key = one\nBeta Key = two\n", "test input"};
    const IniSchema schema{
        "test", {IniField::value(TEST_FIELD_ALPHA, "Alpha Key"), IniField::value(TEST_FIELD_BETA, "Beta Key")}};
    IniSectionValues values;

    const IniSection section = *document.begin();
    parse_ini_section(section, schema, values);

    const std::optional<std::string_view> alpha = values.value(TEST_FIELD_ALPHA);
    const std::optional<std::string_view> beta = values.value(TEST_FIELD_BETA);
    ASSERT_TRUE(alpha.has_value());
    ASSERT_TRUE(beta.has_value());
    EXPECT_EQ(std::string_view{"one"}, *alpha);
    EXPECT_EQ(std::string_view{"two"}, *beta);
}

TEST_P(ParsedIniValueTest, parsesTheConfiguredValue)
{
    const auto &[input, field, expected] = GetParam();

    parse(input);

    EXPECT_TRUE(m_parsed);
    EXPECT_EQ(1U, m_values.size());
    expect_value(m_values, field, expected);
}

INSTANTIATE_TEST_SUITE_P(RangeAndCase, ParsedIniValueTest,
                         testing::Values(ParsedIniValue{RANGE_INPUT, TEST_FIELD_ALPHA, "one"},
                                         ParsedIniValue{MIXED_CASE_INPUT, TEST_FIELD_MIXED_CASE, "yes"}));

INSTANTIATE_TEST_SUITE_P(
    Normalization, ParsedIniValueTest,
    testing::Values(
        ParsedIniValue{"[test]\nAlpha Key = one # comment\n", TEST_FIELD_ALPHA, "one"},
        ParsedIniValue{"[test]\nBeta Key = \"two # not comment\" # comment\n", TEST_FIELD_BETA, "two # not comment"},
        ParsedIniValue{"[test]\nEscaped Key = hello\\nthere\n", TEST_FIELD_ESCAPED, "hello\nthere"},
        ParsedIniValue{"[test]\nContinued Key = line\\\n continued\n", TEST_FIELD_CONTINUED, "line continued"},
        ParsedIniValue{"[test]\nTrailing Key = text     \n", TEST_FIELD_TRAILING, "text"},
        ParsedIniValue{"[test]\nQuoted Key = '  padded  '  # tail\n", TEST_FIELD_QUOTED, "  padded  "}));

TEST_F(ParsedIniSectionTest, omitsEmptyValues)
{
    parse(RANGE_INPUT);

    EXPECT_FALSE(m_values.contains(TEST_FIELD_EMPTY));
}

TEST_P(UnknownIniFieldTest, reportsUnknownFields)
{
    parse(GetParam());

    EXPECT_NE(std::string::npos, m_output.find("Unknown option: `Unknown Key'."));
}

INSTANTIATE_TEST_SUITE_P(RangeAndCase, UnknownIniFieldTest, testing::Values(RANGE_INPUT, MIXED_CASE_INPUT));

TEST(IniDocumentTest, rangeSectionWithoutValuesClearsValues)
{
    IniDocument     document{"[first]\nAlpha Key = one\n[second]\nEmpty Key = # comment\n", "test input"};
    const IniSchema schema{
        "test", {IniField::value(TEST_FIELD_ALPHA, "Alpha Key"), IniField::value(TEST_FIELD_EMPTY, "Empty Key")}};
    IniSectionValues values;

    IniDocument::Iterator iterator = document.begin();
    ASSERT_NE(document.end(), iterator);
    ASSERT_TRUE(parse_ini_section(*iterator, schema, values));
    EXPECT_TRUE(values.contains(TEST_FIELD_ALPHA));

    ++iterator;
    ASSERT_NE(document.end(), iterator);
    EXPECT_FALSE(parse_ini_section(*iterator, schema, values));
    EXPECT_EQ(0U, values.size());
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

    const IniSection section = *document.begin();
    EXPECT_EQ(std::string_view{"file"}, section.name());

    IniSection::Iterator setting = section.begin();
    ASSERT_NE(section.end(), setting);
    EXPECT_EQ(std::string_view{"Value"}, (*setting).name());
    EXPECT_EQ("text", (*setting).value());

    std::filesystem::remove(path, error);
}
