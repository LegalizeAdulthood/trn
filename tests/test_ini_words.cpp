// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/util.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>

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
        std::strncpy(m_buffer, text, sizeof m_buffer);
        m_buffer[sizeof m_buffer - 1] = '\0';

        prep_ini_data(m_buffer, "test input");

        char *section{};
        char *condition{};
        char *section_body = next_ini_section(m_buffer, &section, &condition);
        EXPECT_STREQ("test", section);
        EXPECT_STREQ("", condition);
        EXPECT_NE(nullptr, section_body);

        parse_ini_section(section_body, m_words);
        return ini_values(m_words);
    }

    bool is_buffer_pointer(const char *ptr) const
    {
        return ptr >= m_buffer && ptr < m_buffer + sizeof m_buffer;
    }

    IniWords m_words[4]{
        {0, "TEST", nullptr}, {0, "Alpha Key", nullptr}, {0, "Beta Key", nullptr}, {0, nullptr, nullptr}};
    char m_buffer[512]{};
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
