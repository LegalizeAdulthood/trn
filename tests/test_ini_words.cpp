// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/IniDocument.h>
#include <trn/IniSchema.h>
#include <trn/IniSectionValues.h>
#include <trn/util.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <string_view>

namespace
{

enum TestIniWord
{
    IW_ALPHA = 1,
    IW_BETA
};

struct IniWordsTest : testing::Test
{
    ~IniWordsTest() override
    {
        if (ini_values(m_words) != nullptr)
        {
            unprep_ini_words(m_words);
        }
    }

    char **parse(const char *text)
    {
        m_document = IniDocument{text, "test input"};
        m_buffer_size = std::strlen(text) + 2;

        IniDocument::Section section;
        EXPECT_TRUE(m_document.next_section(section));
        EXPECT_STREQ("test", section.name);
        EXPECT_STREQ("", section.condition);
        EXPECT_NE(nullptr, section.body);

        parse_ini_section(section.body, m_words);
        return ini_values(m_words);
    }

    const IniSectionValues &parse_section_values(const char *text)
    {
        m_section_values_document = IniDocument{text, "test input"};

        IniDocument::Section section;
        EXPECT_TRUE(m_section_values_document.next_section(section));
        EXPECT_STREQ("test", section.name);
        EXPECT_STREQ("", section.condition);
        EXPECT_NE(nullptr, section.body);

        parse_ini_section(section.body, m_schema, m_section_values);
        return m_section_values;
    }

    bool is_buffer_pointer(const char *ptr) const
    {
        return ptr >= m_document.data() && ptr < m_document.data() + m_buffer_size;
    }

    IniWords m_words[4]{
        {0, "TEST", nullptr}, {0, "Alpha Key", nullptr}, {0, "Beta Key", nullptr}, {0, nullptr, nullptr}};
    IniSchema        m_schema{"test", {IniField::value(IW_ALPHA, "Alpha Key"), IniField::value(IW_BETA, "Beta Key")}};
    IniSectionValues m_section_values;
    IniDocument      m_document;
    IniDocument      m_section_values_document;
    std::size_t      m_buffer_size{};
};

} // namespace

TEST_F(IniWordsTest, keyLookupIsCaseInsensitive)
{
    char **values = parse("[test]\nALPHA KEY = first\nbeta key = second\n");

    EXPECT_STREQ("first", values[IW_ALPHA]);
    EXPECT_STREQ("second", values[IW_BETA]);
}

TEST_F(IniWordsTest, unknownKeysAreReportedAndIgnored)
{
    testing::internal::CaptureStdout();

    char **values = parse("[test]\nUnknown Key = ignored\nAlpha Key = kept\n");

    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(std::string::npos, output.find("Unknown option: `unknown key'."));
    EXPECT_STREQ("kept", values[IW_ALPHA]);
    EXPECT_EQ(nullptr, values[IW_BETA]);
}

TEST_F(IniWordsTest, duplicateKeysKeepLastValue)
{
    char **values = parse("[test]\nAlpha Key = first\nalpha key = second\n");

    EXPECT_STREQ("second", values[IW_ALPHA]);
    EXPECT_EQ(nullptr, values[IW_BETA]);
}

TEST_F(IniWordsTest, prepClearsPriorValues)
{
    char **values = prep_ini_words(m_words);
    char   sentinel[]{"sentinel"};
    values[IW_ALPHA] = sentinel;
    values[IW_BETA] = sentinel + 1;

    char **cleared_values = prep_ini_words(m_words);

    EXPECT_EQ(values, cleared_values);
    EXPECT_EQ(nullptr, cleared_values[IW_ALPHA]);
    EXPECT_EQ(nullptr, cleared_values[IW_BETA]);
    EXPECT_STREQ("sentinel", sentinel);
}

TEST_F(IniWordsTest, parsedValuesAreBorrowedFromInputBuffer)
{
    char **values = parse("[test]\nAlpha Key = borrowed\n");

    ASSERT_NE(nullptr, values[IW_ALPHA]);
    EXPECT_TRUE(is_buffer_pointer(values[IW_ALPHA]));
    EXPECT_STREQ("borrowed", values[IW_ALPHA]);
}

TEST_F(IniWordsTest, sectionValuesMatchLegacyVals)
{
    constexpr char text[] = "[test]\nAlpha Key = first\nbeta key = second\n";

    char                  **values = parse(text);
    const IniSectionValues &section_values = parse_section_values(text);

    const auto alpha = section_values.value(IW_ALPHA);
    const auto beta = section_values.value(IW_BETA);

    ASSERT_TRUE(alpha.has_value());
    ASSERT_TRUE(beta.has_value());
    EXPECT_EQ(std::string_view{values[IW_ALPHA]}, *alpha);
    EXPECT_EQ(std::string_view{values[IW_BETA]}, *beta);
}

TEST_F(IniWordsTest, sectionValuesReportUnknownKeys)
{
    testing::internal::CaptureStdout();

    const IniSectionValues &values = parse_section_values("[test]\nUnknown Key = ignored\nAlpha Key = kept\n");

    const std::string output = testing::internal::GetCapturedStdout();
    const auto        alpha = values.value(IW_ALPHA);
    EXPECT_NE(std::string::npos, output.find("Unknown option: `unknown key'."));
    ASSERT_TRUE(alpha.has_value());
    EXPECT_EQ(std::string_view{"kept"}, *alpha);
    EXPECT_FALSE(values.contains(IW_BETA));
}
