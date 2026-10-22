/* test_cache.cpp - unit tests for the changed portions of cache.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/cache.h>

#include <config/common.h>

#include <trn/head.h>
#include <trn/rt-util.h>
#include <trn/utf.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <tuple>
#include <utility>

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

class HeaderCharsetTest : public Test
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string_view m_input;
    std::string_view m_output;
};

void HeaderCharsetTest::SetUp()
{
    m_input = input_charset_name();
    m_output = output_charset_name();
}

void HeaderCharsetTest::TearDown()
{
    utf_init(m_input, m_output);
}

using HeaderDecodeCase = std::pair<std::string_view, std::string_view>;
using UnknownHeaderCharsetCase = std::tuple<std::string_view, HeaderDecodeCase>;

class UnknownHeaderCharsetTest : public HeaderCharsetTest, public WithParamInterface<UnknownHeaderCharsetCase>
{
};

class HeaderDecodeInputTest : public HeaderCharsetTest, public WithParamInterface<HeaderDecodeCase>
{
};

TEST_F(HeaderCharsetTest, eachWordUsesItsDeclaredCharset)
{
    utf_init(CHARSET_NAME_ASCII, CHARSET_NAME_ASCII);
    const std::string raw{"=?ISO-8859-1?Q?=E9?= =?windows-1252?B?kg==?= "
                          "=?ISO-8859-15?Q?=A4?= =?UTF-8?Q?=E5=A4=A9?="};

    EXPECT_EQ("\xC3\xA9\xE2\x80\x99\xE2\x82\xAC\xE5\xA4\xA9", decode_header(raw));
    EXPECT_EQ(CHARSET_NAME_ASCII, input_charset_name());
    EXPECT_EQ(CHARSET_NAME_ASCII, output_charset_name());
}

TEST_P(UnknownHeaderCharsetTest, unknownWordsNeverInheritPreviousDecoder)
{
    const auto &[charset, input] = GetParam();
    utf_init(charset, charset);

    const std::string decoded = decode_header(input.first);

    EXPECT_EQ(input.second, decoded);
    EXPECT_EQ(charset, input_charset_name());
    EXPECT_EQ(charset, output_charset_name());
}

INSTANTIATE_TEST_SUITE_P(PriorCharsetAndInput, UnknownHeaderCharsetTest,
                         Combine(Values(CHARSET_NAME_ISO8859_1, CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_UTF8),
                                 Values(HeaderDecodeCase{"=?unknown?Q?=E9?=", "\xEF\xBF\xBD"},
                                        HeaderDecodeCase{"=?Latin1?Q?=E9?= =?unknown?Q?=E9?=", "\xC3\xA9\xEF\xBF\xBD"},
                                        HeaderDecodeCase{"=?unknown?Q?=E2=80=99?=", "\xE2\x80\x99"})));

TEST_P(HeaderDecodeInputTest, decodesHeaderText)
{
    const HeaderDecodeCase &input = GetParam();

    const std::string decoded = decode_header(input.first);

    EXPECT_EQ(input.second, decoded);
}

INSTANTIATE_TEST_SUITE_P(MalformedAndLegacyBytes, HeaderDecodeInputTest,
                         Values(HeaderDecodeCase{"=?UTF-8?Q?=C0=AF?=", "\xEF\xBF\xBD\xEF\xBF\xBD"},
                                HeaderDecodeCase{"=?US-ASCII?Q?=FF?=", "\xEF\xBF\xBD"},
                                HeaderDecodeCase{"=?windows-1252?Q?=81?=", "\xEF\xBF\xBD"},
                                HeaderDecodeCase{"=?ISO-8859-1?Q?=AD?=", "\xC2\xAD"},
                                HeaderDecodeCase{"\xFF", "\xEF\xBF\xBD"}));

TEST_F(HeaderCharsetTest, incompleteWordRetainsLiteralTextAndState)
{
    utf_init(CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_ASCII);
    EXPECT_EQ("=?Latin1?Q?=E9", decode_header("=?Latin1?Q?=E9"));
    EXPECT_EQ(CHARSET_NAME_WINDOWS_1252, input_charset_name());
    EXPECT_EQ(CHARSET_NAME_ASCII, output_charset_name());
}

INSTANTIATE_TEST_SUITE_P(WhitespaceBoundaries, HeaderDecodeInputTest,
                         Values(HeaderDecodeCase{"=?US-ASCII?Q?Hello?= world", "Hello world"},
                                HeaderDecodeCase{"=?US-ASCII?Q?Hello?= \t=?US-ASCII?Q?world?=", "Helloworld"}));

TEST_F(HeaderCharsetTest, controlFilteringUsesUnicodeInsteadOfGlobalCharset)
{
    utf_init(CHARSET_NAME_ISO8859_1, CHARSET_NAME_ISO8859_1);
    std::string text{"\xC3\x84\xE2\x80\x99\t"};
    dectrl(text);
    EXPECT_EQ("\xC3\x84\xE2\x80\x99 ", text);
}

TEST_F(HeaderCharsetTest, nameCompressionUsesUtf8WithStaleLegacyCharset)
{
    utf_init(CHARSET_NAME_ISO8859_1, CHARSET_NAME_ASCII);

    EXPECT_EQ("\xC3\xA9", compress_from(decode_header("=?Latin1?Q?=E9?= <a@example.com>"), 1));
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
