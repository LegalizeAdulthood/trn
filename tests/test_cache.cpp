/* test_cache.cpp - unit tests for the changed portions of cache.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/cache.h>

#include <config/common.h>

#include <trn/head.h>

#include <gtest/gtest.h>

#include <string>

using namespace testing;

TEST(DecodeHeaderTest, copiesPlainText)
{
    EXPECT_EQ("Plain header", decode_header("Plain header"));
}

TEST(DecodeHeaderTest, decodesBase64EncodedWord)
{
    EXPECT_EQ("Hello", decode_header("=?US-ASCII?B?SGVsbG8=?="));
}

TEST(DecodeHeaderTest, decodesQuotedPrintableEncodedWord)
{
    EXPECT_EQ("Hello World!", decode_header("=?US-ASCII?Q?Hello_World=21?="));
}

TEST(DecodeHeaderTest, removesNewlines)
{
    EXPECT_EQ("helloworld", decode_header("hello\nworld\n"));
}

TEST(DecodeHeaderTest, trimsTrailingSpaces)
{
    EXPECT_EQ("hello", decode_header("hello  "));
}

TEST(DecodeHeaderTest, normalizesControlCharacters)
{
    EXPECT_EQ("a b c", decode_header("a\tb\fc"));
}

TEST(ArticleCacheTest, setCachedLineParsesLineCount)
{
    Article article{};

    article.set_cached_line(LINES_LINE, "42junk");

    EXPECT_EQ(42, article.m_lines);
}

TEST(ArticleCacheTest, setCachedLineParsesByteCount)
{
    Article article{};

    article.set_cached_line(BYTES_LINE, "1234junk");

    EXPECT_EQ(1234, article.m_bytes);
}

class DectrlTest : public Test
{
protected:
    void configure_unchanged(const char *before)
    {
        m_buffer = before;
        m_expected = before;
    }
    void configure_before_expected(const char *before, const char *expected)
    {
        m_buffer = before;
        m_expected = expected;
    }

    std::string m_buffer;
    std::string m_expected;
};

TEST_F(DectrlTest, empty)
{
    dectrl(m_buffer);

    ASSERT_TRUE(m_buffer.empty());
}

TEST_F(DectrlTest, ascii_all_printable)
{
    configure_unchanged("This is a test.");

    dectrl(m_buffer);

    ASSERT_EQ(m_expected, m_buffer) << "dectrl changed an ASCII string with all-printable characters";
}

TEST_F(DectrlTest, ascii_some_nonprintable)
{
    configure_before_expected("This\tis\fa\177test.", "This is a test.");

    dectrl(m_buffer);

    ASSERT_EQ(m_expected, m_buffer) << "dectrl did not change an ASCII string with some non-printable characters";
}

TEST_F(DectrlTest, iso_8859_1)
{
    configure_unchanged("\302\253\302\240\240La Libert\303\251 guidant le peuple.\240\302\240\302\273");

    dectrl(m_buffer);

    ASSERT_EQ(m_expected, m_buffer) << "dectrl changed an ISO8859-1 string with all-printable characters";
}

TEST_F(DectrlTest, iso_8859_1_non_printable)
{
    configure_before_expected("\302\253\302\240\240La Libert\303\251 guidant\tle peuple.\240\302\240\302\273",
                              "\302\253\302\240\240La Libert\303\251 guidant le peuple.\240\302\240\302\273");

    dectrl(m_buffer);

    ASSERT_EQ(m_expected, m_buffer) << "dectrl did not change an ISO8859-1 string with non-printable characters";
}

TEST_F(DectrlTest, cjk_basic)
{
    configure_unchanged(
        "\345\257\247\345\214\226\351\243\233\347\201\260\357\274\214\344\270\215\344\275\234\346\265\256\345\241\265");

    dectrl(m_buffer);

    ASSERT_EQ(m_expected, m_buffer) << "dectrl changed a CJK string with all-printable characters";
}

TEST_F(DectrlTest, cjk_basic_non_printable)
{
    configure_before_expected("\345\257\247\345\214\226\351\243\233\347\201\260\357\274\214\t"
                              "\344\270\215\344\275\234\346\265\256\345\241\265",
                              "\345\257\247\345\214\226\351\243\233\347\201\260\357\274\214 "
                              "\344\270\215\344\275\234\346\265\256\345\241\265");

    dectrl(m_buffer);

    ASSERT_EQ(m_expected, m_buffer) << "dectrl did not change a CJK string with non-printable characters";
}
