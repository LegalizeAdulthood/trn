/* test_utf.cpp - unit tests for utf.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/utf.h>

#include <config/common.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

#include <ostream>
#include <string>
#include <string_view>

using namespace testing;

namespace
{

struct Utf8ScalarCase
{
    const char      *name;
    CodePoint        scalar;
    std::string_view bytes;
};

void PrintTo(const Utf8ScalarCase &value, std::ostream *out)
{
    *out << value.name;
}

constexpr Utf8ScalarCase UTF8_SCALARS[]{
    {"Nul", 0, {"\0", 1}},
    {"Ascii", 0x41, "A"},
    {"AsciiMaximum", 0x7F, "\x7F"},
    {"TwoByteMinimum", 0x80, "\xC2\x80"},
    {"TwoByteMaximum", 0x7FF, "\xDF\xBF"},
    {"ThreeByteMinimum", 0x800, "\xE0\xA0\x80"},
    {"Cjk", 0x4E00, "\xE4\xB8\x80"},
    {"BeforeSurrogates", 0xD7FF, "\xED\x9F\xBF"},
    {"AfterSurrogates", 0xE000, "\xEE\x80\x80"},
    {"Replacement", 0xFFFD, "\xEF\xBF\xBD"},
    {"Noncharacter", 0xFFFE, "\xEF\xBF\xBE"},
    {"ThreeByteMaximum", 0xFFFF, "\xEF\xBF\xBF"},
    {"FourByteMinimum", 0x10000, "\xF0\x90\x80\x80"},
    {"Emoji", 0x1F61A, "\xF0\x9F\x98\x9A"},
    {"ScalarMaximum", 0x10FFFF, "\xF4\x8F\xBF\xBF"},
};

struct InvalidUtf8Case
{
    const char      *name;
    std::string_view bytes;
};

void PrintTo(const InvalidUtf8Case &value, std::ostream *out)
{
    *out << value.name;
}

constexpr InvalidUtf8Case INVALID_UTF8[]{
    {"ContinuationMinimum", "\x80"},
    {"ContinuationMaximum", "\xBF"},
    {"OverlongNul", "\xC0\x80"},
    {"OverlongAscii", "\xC1\xBF"},
    {"OverlongThreeByteNul", "\xE0\x80\x80"},
    {"OverlongThreeByte", "\xE0\x9F\xBF"},
    {"OverlongFourByteNul", "\xF0\x80\x80\x80"},
    {"OverlongFourByte", "\xF0\x8F\xBF\xBF"},
    {"HighSurrogateMinimum", "\xED\xA0\x80"},
    {"HighSurrogateMaximum", "\xED\xAF\xBF"},
    {"LowSurrogateMinimum", "\xED\xB0\x80"},
    {"LowSurrogateMaximum", "\xED\xBF\xBF"},
    {"AboveScalarMaximum", "\xF4\x90\x80\x80"},
    {"InvalidFourByteLead", "\xF5\x80\x80\x80"},
    {"LegacyFiveByte", "\xF8\x88\x80\x80\x80"},
    {"LegacySixByte", "\xFC\x84\x80\x80\x80\x80"},
    {"InvalidLeadFe", "\xFE"},
    {"InvalidLeadFf", "\xFF"},
    {"TruncatedTwoByte", "\xC2"},
    {"TruncatedThreeByteLead", "\xE2"},
    {"TruncatedThreeByte", "\xE2\x82"},
    {"TruncatedFourByteLead", "\xF0"},
    {"TruncatedFourByteSecond", "\xF0\x9F"},
    {"TruncatedFourByteThird", "\xF0\x9F\x98"},
    {"InvalidTwoByteContinuation", "\xC2"
                                   "A"},
    {"InvalidThreeByteSecond", "\xE2"
                               "(\xA1"},
    {"InvalidThreeByteThird", "\xE2\x82"
                              "A"},
    {"InvalidFourByteSecond", "\xF0"
                              "(\x80\x80"},
    {"InvalidFourByteThird", "\xF0\x9F"
                             "A\x80"},
    {"InvalidFourByteFourth", "\xF0\x9F\x98"
                              "A"},
    {"Cesu8SurrogatePair", "\xED\xA0\xBD\xED\xB8\x9A"},
    {"NulContinuation", {"\xC2\0", 2}},
};

class Utf8ScalarTest : public Test
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string_view m_input;
    std::string_view m_output;
};

void Utf8ScalarTest::SetUp()
{
    m_input = input_charset_name();
    m_output = output_charset_name();
}

void Utf8ScalarTest::TearDown()
{
    utf_init(m_input, m_output);
}

class Utf8RoundTripTest : public Utf8ScalarTest, public WithParamInterface<Utf8ScalarCase>
{
};

TEST_P(Utf8RoundTripTest, decodesFirstScalar)
{
    const Utf8ScalarCase &value = GetParam();
    const std::string     text = std::string{value.bytes} + "suffix";

    const Utf8Scalar result = decode_utf8_scalar(text);

    EXPECT_EQ(value.scalar, result.code_point);
    EXPECT_EQ(value.bytes.size(), result.byte_count);
    EXPECT_EQ("suffix", std::string_view{text}.substr(result.byte_count));
}

TEST_P(Utf8RoundTripTest, encodesScalar)
{
    const Utf8ScalarCase &value = GetParam();

    EXPECT_EQ(value.bytes, encode_utf8_scalar(value.scalar));
}

INSTANTIATE_TEST_SUITE_P(ModernUtf8, Utf8RoundTripTest, ValuesIn(UTF8_SCALARS));

class Utf8MalformedTest : public Utf8ScalarTest, public WithParamInterface<InvalidUtf8Case>
{
};

TEST_P(Utf8MalformedTest, reportsErrorAndConsumesOneByte)
{
    const Utf8Scalar result = decode_utf8_scalar(GetParam().bytes);

    EXPECT_EQ(INVALID_CODE_POINT, result.code_point);
    EXPECT_EQ(1U, result.byte_count);
}

INSTANTIATE_TEST_SUITE_P(ModernUtf8, Utf8MalformedTest, ValuesIn(INVALID_UTF8));

TEST_F(Utf8ScalarTest, emptyInputMakesNoProgress)
{
    const Utf8Scalar result = decode_utf8_scalar({});

    EXPECT_EQ(INVALID_CODE_POINT, result.code_point);
    EXPECT_EQ(0U, result.byte_count);
}

TEST_F(Utf8ScalarTest, rejectsNonScalarsWhenEncoding)
{
    for (const CodePoint scalar : {0xD800UL, 0xDBFFUL, 0xDC00UL, 0xDFFFUL, 0x110000UL, 0x1FFFFFUL, 0x7FFFFFFFUL,
                                   static_cast<CodePoint>(INVALID_CODE_POINT)})
    {
        SCOPED_TRACE(scalar);
        EXPECT_TRUE(encode_utf8_scalar(scalar).empty());
    }
}

TEST_F(Utf8ScalarTest, operationsIgnoreGlobalCharsetState)
{
    for (const std::string_view charset : {CHARSET_NAME_ASCII, CHARSET_NAME_ISO8859_1, CHARSET_NAME_ISO8859_15,
                                           CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_UTF8})
    {
        SCOPED_TRACE(charset);
        utf_init(charset, charset);
        const std::string_view input = input_charset_name();
        const std::string_view output = output_charset_name();
        for (const Utf8ScalarCase &value : UTF8_SCALARS)
        {
            const Utf8Scalar result = decode_utf8_scalar(value.bytes);
            EXPECT_EQ(value.scalar, result.code_point);
            EXPECT_EQ(value.bytes.size(), result.byte_count);
            EXPECT_EQ(value.bytes, encode_utf8_scalar(value.scalar));
        }
        EXPECT_EQ(input, input_charset_name());
        EXPECT_EQ(output, output_charset_name());
    }
}

TEST_F(Utf8ScalarTest, malformedInputDoesNotConsumeFollowingAscii)
{
    const std::string text{"\xE2"
                           "(\xA1"};
    std::string_view  remaining = text;
    const Utf8Scalar  first = decode_utf8_scalar(remaining);
    ASSERT_EQ(1U, first.byte_count);
    remaining.remove_prefix(first.byte_count);
    const Utf8Scalar second = decode_utf8_scalar(remaining);
    EXPECT_EQ(static_cast<CodePoint>('('), second.code_point);
    ASSERT_EQ(1U, second.byte_count);
    remaining.remove_prefix(second.byte_count);
    const Utf8Scalar third = decode_utf8_scalar(remaining);
    EXPECT_EQ(INVALID_CODE_POINT, third.code_point);
    EXPECT_EQ(1U, third.byte_count);
    EXPECT_EQ("\xE2"
              "(\xA1",
              text);
}

TEST_F(Utf8ScalarTest, boundedChunksCanBeRetriedWithoutState)
{
    constexpr std::string_view text{"\xF0\x9F\x98\x9A"};
    for (std::size_t size = 1; size < text.size(); ++size)
    {
        const Utf8Scalar partial = decode_utf8_scalar(text.substr(0, size));
        EXPECT_EQ(INVALID_CODE_POINT, partial.code_point);
        EXPECT_EQ(1U, partial.byte_count);
    }
    const Utf8Scalar complete = decode_utf8_scalar(text);
    EXPECT_EQ(0x1F61AUL, complete.code_point);
    EXPECT_EQ(4U, complete.byte_count);
}

TEST_F(Utf8ScalarTest, headerPipelinePreservesRawBytesAndOrdersConversions)
{
    for (const std::string_view raw : {"=?ISO-8859-1?Q?=E9?=", "=?ISO-8859-1?B?6Q==?="})
    {
        const std::string source{raw};
        utf_init(CHARSET_NAME_ISO8859_1, CHARSET_NAME_UTF8);
        const std::string semantic = decode_header(source);
        ASSERT_EQ("\xC3\xA9", semantic);
        const std::string presentation = str_char_subst(semantic, 'p');
        EXPECT_EQ(semantic, presentation);
        const Utf8Scalar result = decode_utf8_scalar(presentation);
        EXPECT_EQ(0xE9UL, result.code_point);
        EXPECT_EQ(2U, result.byte_count);
        utf_init(CHARSET_NAME_UTF8, CHARSET_NAME_UTF8);
        EXPECT_EQ(1, visual_length_of(presentation));
        EXPECT_EQ(raw, source);
    }
}

} // namespace

TEST(UTFInitTest, charset_empty_utf8)
{
    ASSERT_EQ(CHARSET_UNKNOWN, utf_init("", "utf8"));
}

TEST(UTFInitTest, charset_ascii_utf8)
{
    ASSERT_EQ(CHARSET_ASCII, utf_init("ascii", "utf8"));
}

TEST(UTFInitTest, charset_utf8_utf8)
{
    ASSERT_EQ(CHARSET_UTF8, utf_init("utf8", "utf8"));
}

TEST(UTFInitTest, charset_us_us_dash_ascii_utf8)
{
    ASSERT_EQ(CHARSET_ASCII, utf_init("US-ASCII", "utf8"));
}

TEST(UTFInitTest, charset_utf_dash_8_utf8)
{
    ASSERT_EQ(CHARSET_UTF8, utf_init("UTF-8", "utf8"));
}

struct CharsetParam
{
    const char *value;
    const char *name;
};

constexpr CharsetParam charsets[] = {
    // clang-format off
    { "us-ascii",     CHARSET_NAME_ASCII },
    { "ascii",        CHARSET_NAME_ASCII },
    { "iso8859-1",    CHARSET_NAME_ISO8859_1 },
    { "WiNdOwS-1252", CHARSET_NAME_WINDOWS_1252 },
    { "utf8",         CHARSET_NAME_UTF8 },
    { "UTF-8",        CHARSET_NAME_UTF8 },

    { CHARSET_NAME_ASCII,        CHARSET_NAME_ASCII },
    { CHARSET_NAME_UTF8,         CHARSET_NAME_UTF8 },
    { CHARSET_NAME_ISO8859_1,    CHARSET_NAME_ISO8859_1 },
    { CHARSET_NAME_ISO8859_15,   CHARSET_NAME_ISO8859_15 },
    { CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_WINDOWS_1252 },
    // clang-format on
};

void PrintTo(const CharsetParam &value, std::ostream *str)
{
    *str << value.value << ", " << value.name;
}

class TestInputCharsetName : public TestWithParam<CharsetParam>
{
};

TEST_P(TestInputCharsetName, tag_from_name)
{
    const char *before = GetParam().value;
    const char *expected = GetParam().name;
    utf_init(before, CHARSET_NAME_UTF8);

    std::string_view after = input_charset_name();

    ASSERT_EQ(std::string_view{expected}, after);
}

INSTANTIATE_TEST_SUITE_P(UTFCharsetNames, TestInputCharsetName, ValuesIn(charsets));

class TestOutputCharsetName : public TestWithParam<CharsetParam>
{
};

TEST_P(TestOutputCharsetName, tag_from_name)
{
    const char *before = GetParam().value;
    const char *expected = GetParam().name;
    utf_init(CHARSET_NAME_UTF8, before);

    std::string_view after = output_charset_name();

    EXPECT_EQ(std::string_view{expected}, after);
}

INSTANTIATE_TEST_SUITE_P(UTFCharsetNames, TestOutputCharsetName, ValuesIn(charsets));

class CurrentCharSubstTest : public Test
{
protected:
    void SetUp() override
    {
        m_previous_charsets = g_charsets;
        m_previous_char_subst = current_char_subst_mode();
        m_previous_verbose = g_verbose;
    }

    void TearDown() override
    {
        g_charsets = m_previous_charsets;
        set_char_subst_mode(m_previous_char_subst);
        g_verbose = m_previous_verbose;
    }

    std::string m_previous_charsets;
    char m_previous_char_subst{};
    bool m_previous_verbose{};
};

TEST_F(CurrentCharSubstTest, showsVerboseMonoSubstitutionStatus)
{
    set_char_subst_mode('m');
    g_verbose = true;

    EXPECT_EQ("[ISO->USmono] ", current_char_subst());
}

TEST_F(CurrentCharSubstTest, showsTerseTexSubstitutionStatus)
{
    set_char_subst_mode('t');
    g_verbose = false;

    EXPECT_EQ("[T] ", current_char_subst());
}

TEST_F(CurrentCharSubstTest, returnsCurrentMode)
{
    set_char_subst_mode('a');

    EXPECT_EQ('a', current_char_subst_mode());
}

TEST_F(CurrentCharSubstTest, resetsToFirstConfiguredMode)
{
    set_char_subst_mode(g_charsets[2]);

    reset_char_subst_mode();

    EXPECT_EQ(g_charsets.front(), current_char_subst_mode());
}

TEST_F(CurrentCharSubstTest, cyclesToNextConfiguredMode)
{
    reset_char_subst_mode();

    next_char_subst_mode();

    EXPECT_EQ(g_charsets[1], current_char_subst_mode());
}

TEST_F(CurrentCharSubstTest, cyclesPastEndToFirstConfiguredMode)
{
    set_char_subst_mode(g_charsets.back());

    next_char_subst_mode();

    EXPECT_EQ(g_charsets.front(), current_char_subst_mode());
}

TEST_F(CurrentCharSubstTest, emptyCharsetsHaveNoCurrentMode)
{
    g_charsets = "";
    reset_char_subst_mode();

    EXPECT_EQ('\0', current_char_subst_mode());

    next_char_subst_mode();

    EXPECT_EQ('\0', current_char_subst_mode());
}

TEST_F(CurrentCharSubstTest, reassigningCharsetsNormalizesOutOfRangeMode)
{
    set_char_subst_mode(g_charsets.back());

    g_charsets = "pa";

    EXPECT_EQ(g_charsets.front(), current_char_subst_mode());
}

TEST(CharSubstTest, copiesThroughFirstNewline)
{
    EXPECT_EQ("first\n", str_char_subst("first\nsecond", 'p'));
}

TEST(CharSubstTest, transliteratesLatin1ToAscii)
{
    constexpr std::string_view input{"\xC3\x84\xC3\x96\xC3\x9C\xC3\x9F"};

    EXPECT_EQ("AeOeUess", str_char_subst(input, 'a'));
}

TEST(CharSubstTest, transliteratesLatin1ToMonospacedAscii)
{
    constexpr std::string_view input{"\xC3\x84\xC3\x96\xC3\x9C\xC3\x9F"};

    EXPECT_EQ("AOUs", str_char_subst(input, 'm'));
}

TEST(CharSubstTest, mapsUnicodeLatinSymbolsWithHistoricalTables)
{
    constexpr std::string_view input{"\xC2\xA0\xC2\xA3\xC2\xA9\xC2\xAE\xC2\xB1\xC2\xBC\xC3\xBF"};

    EXPECT_EQ(" (c)(R)+/- 1/4ij", str_char_subst(input, 'a'));
    EXPECT_EQ(" cRy", str_char_subst(input, 'm'));
}

TEST(CharSubstTest, passesUnmappedUnicodeThroughAllModes)
{
    constexpr std::string_view input{"\xC4\x80\xC2\x80\xE5\xA4\xA9"
                                     "e\xCC\x81\xF0\x9F\x98\x9A\xE2\x80\x99"};
    for (const char mode : {'\0', 'p', 'a', 'm', 't'})
    {
        SCOPED_TRACE(mode);
        EXPECT_EQ(input, str_char_subst(input, mode));
    }
}

TEST(CharSubstTest, identityPreservesEveryValidScalarByteForByte)
{
    std::string input;
    for (const Utf8ScalarCase &value : UTF8_SCALARS)
    {
        input += value.bytes;
    }
    EXPECT_EQ(input, str_char_subst(input, '\0'));
    EXPECT_EQ(input, str_char_subst(input, 'p'));
}

TEST(CharSubstTest, malformedBytesBecomeReplacementScalars)
{
    constexpr std::string_view input{"\xFF"
                                     "x\xC3\xE2\x80\x99"};
    constexpr std::string_view expected{"\xEF\xBF\xBD"
                                        "x\xEF\xBF\xBD\xE2\x80\x99"};
    for (const char mode : {'\0', 'p', 'a', 'm', 't'})
    {
        SCOPED_TRACE(mode);
        EXPECT_EQ(expected, str_char_subst(input, mode));
    }
}

TEST(CharSubstTest, asciiModesPreserveEmbeddedNul)
{
    const std::string_view input{"\xC3\x84\0\xC3\x9F", 5};
    EXPECT_EQ(std::string("Ae\0ss", 5), str_char_subst(input, 'a'));
    EXPECT_EQ(std::string("A\0s", 3), str_char_subst(input, 'm'));
}

TEST(CharSubstTest, allModesStopAfterFirstNewline)
{
    for (const char mode : {'\0', 'p', 'a', 'm', 't'})
    {
        SCOPED_TRACE(mode);
        EXPECT_EQ("first\n", str_char_subst("first\nsecond", mode));
    }
}

TEST(CharSubstTest, convertsHistoricalTexUmlautsToUtf8)
{
    EXPECT_EQ("\xC3\x84\xC3\x96\xC3\x9C\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F", str_char_subst(R"("A"O"U"a"o"u"s)", 't'));
    EXPECT_EQ("\xC3\xA4", str_char_subst(R"(\"a)", 't'));
}

TEST(CharSubstTest, texPreservesUnmappedSequencesAndTrailingPrefixes)
{
    EXPECT_EQ("\"\xE5\xA4\xA9\\xyz\"q\\", str_char_subst("\"\xE5\xA4\xA9\\xyz\"q\\", 't'));
    EXPECT_EQ("\"", str_char_subst("\"", 't'));
    EXPECT_EQ("a", str_char_subst("a", 't'));
}

TEST(CharSubstTest, texInstancesRetainTheirOwnPendingPrefix)
{
    CharSubstitution first{'t'};
    CharSubstitution second{'t'};
    CharSubstitution plain{'p'};

    EXPECT_EQ("", first.substitute("\""));
    EXPECT_EQ("a", second.substitute("a"));
    EXPECT_EQ("\"a", plain.substitute("\"a"));
    EXPECT_EQ("\xC3\xA4", first.substitute("a"));
    EXPECT_EQ("", first.finish());
    EXPECT_EQ("", second.finish());
}

TEST(CharSubstTest, texPrefixCanCrossCallsWithoutDuplicatingBackslashes)
{
    CharSubstitution subst{'t'};

    EXPECT_EQ("", subst.substitute("\\"));
    EXPECT_EQ("", subst.substitute("\""));
    EXPECT_EQ("\xC3\x84", subst.substitute("A"));
    EXPECT_EQ("\\xyz", subst.substitute("\\xyz"));
    EXPECT_EQ("\"\xF0\x9F\x98\x9A", subst.substitute("\"\xF0\x9F\x98\x9A"));
    EXPECT_EQ("", subst.finish());
}

TEST(CharSubstTest, finishingTexFlushesAndResetsPendingPrefix)
{
    CharSubstitution subst{'t'};

    EXPECT_EQ("", subst.substitute("\""));
    EXPECT_EQ("\"", subst.finish());
    EXPECT_EQ("", subst.finish());
    EXPECT_EQ("a", subst.substitute("a"));
    EXPECT_EQ("", subst.substitute("\\"));
    EXPECT_EQ("\\", subst.finish());
    EXPECT_EQ("a", subst.substitute("a"));
}

TEST(CharSubstTest, discardingTexInstanceDoesNotAffectNextArticle)
{
    {
        CharSubstitution previous{'t'};
        EXPECT_EQ("", previous.substitute("\""));
    }
    CharSubstitution next{'t'};

    EXPECT_EQ("a", next.substitute("a"));
    EXPECT_EQ("", next.finish());
}

TEST_F(Utf8ScalarTest, substitutionIgnoresGlobalCharsetState)
{
    for (const std::string_view charset :
         {CHARSET_NAME_ASCII, CHARSET_NAME_ISO8859_1, CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_UTF8})
    {
        utf_init(charset, charset);
        EXPECT_EQ("Ae\xE2\x80\x99", str_char_subst("\xC3\x84\xE2\x80\x99", 'a'));
        EXPECT_EQ(charset, input_charset_name());
        EXPECT_EQ(charset, output_charset_name());
    }
}

constexpr std::string_view ARBITRARY_ASCII{"a"};
constexpr std::string_view ARBITRARY_ISO8859D1_1{"\303\241"};
constexpr std::string_view ARBITRARY_ISO8859D1_2{"\303\246"};
constexpr std::string_view ARBITRARY_CJK_BASIC{"\345\244\251"};
constexpr std::string_view ASCII_SOH{"\001"};
constexpr std::string_view ASCII_DEL{"\177"};
constexpr std::string_view ASCII_SPACE{" "};
constexpr std::string_view ASCII_TILDE{"~"};

TEST(UTFByteLengthTest, length_at_empty_view)
{
    ASSERT_EQ(0, byte_length_at(std::string_view{}));
}

TEST(UTFByteLengthTest, length_at_ascii)
{
    ASSERT_EQ(1, byte_length_at(ARBITRARY_ASCII));
}

TEST(UTFByteLengthTest, length_at_iso8859_1)
{
    ASSERT_EQ(2, byte_length_at(ARBITRARY_ISO8859D1_1));
    ASSERT_EQ(2, byte_length_at(ARBITRARY_ISO8859D1_2));
}

TEST(UTFByteLengthTest, byte_length_at_cjk_basic)
{
    ASSERT_EQ(3, byte_length_at(ARBITRARY_CJK_BASIC));
}

TEST(UTFByteLengthTest, byte_length_at_bounded_cjk_view)
{
    ASSERT_EQ(3, byte_length_at(ARBITRARY_CJK_BASIC));
    ASSERT_EQ(1, byte_length_at(std::string_view{ARBITRARY_CJK_BASIC.data(), 2}));
}

TEST(UTFAtNormalCharacterTest, emptyView)
{
    ASSERT_FALSE(at_norm_char(std::string_view{}));
}

TEST(UTFAtNormalCharacterTest, SOH)
{
    ASSERT_FALSE(at_norm_char(ASCII_SOH));
}

TEST(UTFAtNormalCharacterTest, oneCharacterView)
{
    ASSERT_TRUE(at_norm_char(ASCII_SPACE.substr(0, 1)));
}

TEST(UTFAtNormalCharacterTest, space)
{
    ASSERT_TRUE(at_norm_char(ASCII_SPACE));
}

TEST(UTFAtNormalCharacterTest, tilde)
{
    ASSERT_TRUE(at_norm_char(ASCII_TILDE));
}

TEST(UTFAtNormalCharacterTest, DEL)
{
    ASSERT_FALSE(at_norm_char(ASCII_DEL));
}

TEST(UTFAtNormalCharacterTest, iso8859_1)
{
    ASSERT_TRUE(at_norm_char(ARBITRARY_ISO8859D1_1));
    ASSERT_TRUE(at_norm_char(ARBITRARY_ISO8859D1_2));
}

TEST(UTFAtNormalCharacterTest, cjk_basic)
{
    ASSERT_TRUE(at_norm_char(ARBITRARY_CJK_BASIC));
}

TEST(UTFVisualWidthTest, emptyView)
{
    ASSERT_EQ(0, visual_width_at(std::string_view{}));
}

TEST(UTFVisualWidthTest, ascii)
{
    ASSERT_EQ(1, visual_width_at(ARBITRARY_ASCII));
}

TEST(UTFVisualWidthTest, boundedCjkView)
{
    ASSERT_EQ(2, visual_width_at(ARBITRARY_CJK_BASIC));
    ASSERT_EQ(0, visual_width_at(std::string_view{ARBITRARY_CJK_BASIC.data(), 2}));
}

TEST(UTFVisualAdvanceWidthTest, emptyView)
{
    std::string_view text;

    ASSERT_EQ(0, put_char_adv(text, false));
    ASSERT_TRUE(text.empty());
}

TEST(UTFVisualAdvanceWidthTest, ascii)
{
    std::string_view text{ARBITRARY_ASCII};

    int retval = put_char_adv(text, true);

    ASSERT_EQ(1, retval) << "put_char_adv(" << ARBITRARY_ASCII << ")";
    ASSERT_TRUE(text.empty()) << "put_char_adv(" << ARBITRARY_ASCII << ")";
}

TEST(UTFVisualAdvanceWidthTest, iso8859_1)
{
    std::string_view text{ARBITRARY_ISO8859D1_1};

    int retval = put_char_adv(text, true);

    ASSERT_EQ(1, retval) << "put_char_adv(" << ARBITRARY_ISO8859D1_1 << ")";
    ASSERT_TRUE(text.empty()) << "put_char_adv(" << ARBITRARY_ISO8859D1_1 << ")";
}

TEST(UTFVisualAdvanceWidthTest, cjk_basic)
{
    std::string_view text{ARBITRARY_CJK_BASIC};

    int retval = put_char_adv(text, true);

    ASSERT_EQ(2, retval) << "put_char_adv(" << ARBITRARY_CJK_BASIC << ")";
    ASSERT_TRUE(text.empty()) << "put_char_adv(" << ARBITRARY_CJK_BASIC << ")";
}

TEST(UTFVisualAdvanceWidthTest, boundedCjkView)
{
    std::string_view text{ARBITRARY_CJK_BASIC};

    int retval = put_char_adv(text, true);

    ASSERT_EQ(2, retval) << "put_char_adv(" << ARBITRARY_CJK_BASIC << ")";
    ASSERT_TRUE(text.empty()) << "put_char_adv(" << ARBITRARY_CJK_BASIC << ")";
}

// code point decoding
TEST(UTFCodePointDecodingTest, emptyView)
{
    ASSERT_EQ(INVALID_CODE_POINT, code_point_at(std::string_view{}));
}

constexpr CodePoint ASCII_SPACE_CODE_POINT = 0x20;
constexpr CodePoint ASCII_5_CODE_POINT = 0x35;
constexpr CodePoint ISO8859D1_ETH_CODE_POINT = 0xF0;
constexpr CodePoint CJK_SHIN_CODE_POINT = 0x05E9;
constexpr CodePoint OY_CODE_POINT = 0x18B0;
constexpr CodePoint KISSING_FACE_WITH_CLOSED_EYES_CODE_POINT = 0x1F61A;

TEST(UTFCodePointDecodingTest, ascii_space)
{
    ASSERT_EQ(ASCII_SPACE_CODE_POINT, code_point_at(" "));
}

TEST(UTFCodePointDecodingTest, ascii_5)
{
    ASSERT_EQ(ASCII_5_CODE_POINT, code_point_at("5"));
}

TEST(UTFCodePointDecodingTest, eth)
{
    ASSERT_EQ(ISO8859D1_ETH_CODE_POINT, code_point_at("\303\260"));
}

TEST(UTFCodePointDecodingTest, shin)
{
    ASSERT_EQ(CJK_SHIN_CODE_POINT, code_point_at("\327\251"));
}

TEST(UTFCodePointDecodingTest, boundedShinView)
{
    const char text[]{'\327', '\251'};

    ASSERT_EQ(CJK_SHIN_CODE_POINT, code_point_at(std::string_view{text, sizeof text}));
    ASSERT_EQ(INVALID_CODE_POINT, code_point_at(std::string_view{text, 1}));
}

TEST(UTFCodePointDecodingTest, oy)
{
    ASSERT_EQ(OY_CODE_POINT, code_point_at("\341\242\260"));
}

TEST(UTFCodePointDecodingTest, kissing_face_with_closed_eyes)
{
    ASSERT_EQ(KISSING_FACE_WITH_CLOSED_EYES_CODE_POINT, code_point_at("\360\237\230\232"));
}

TEST(UTFVisualLengthTest, emptyView)
{
    ASSERT_EQ(0, visual_length_of(std::string_view{}));
}

TEST(UTFVisualLengthTest, ascii)
{
    ASSERT_EQ(3, visual_length_of("cat"));
}

TEST(UTFVisualLengthTest, iso_8859_1)
{
    ASSERT_EQ(7, visual_length_of("libert\303\251"));
    ASSERT_EQ(4, visual_length_of("b\303\251b\303\251"));
    ASSERT_EQ(4 /* combining acute */, visual_length_of("be\314\201be\314\201"));
    ASSERT_EQ(0 /* combining acute */, visual_length_of("\314\201"));
}

TEST(UTFVisualLengthTest, cjk)
{
    ASSERT_EQ(4, visual_length_of("\350\211\257\345\277\203"));
}

class UTFInsertUnicodeAtTest : public Test
{
protected:
    void SetUp() override
    {
        utf_init(CHARSET_NAME_UTF8, CHARSET_NAME_UTF8);
    }
};

TEST_F(UTFInsertUnicodeAtTest, ascii)
{
    ASSERT_EQ(std::string{"d"}, insert_unicode_at(0x64));
}

TEST_F(UTFInsertUnicodeAtTest, iso_8859_1)
{
    ASSERT_EQ(std::string{"\303\244"}, insert_unicode_at(0xE4));
}

TEST_F(UTFInsertUnicodeAtTest, cjk_basic)
{
    ASSERT_EQ(std::string{"\344\270\200"}, insert_unicode_at(0x4E00));
}

TEST_F(UTFInsertUnicodeAtTest, kissing_face_with_closed_eyes)
{
    ASSERT_EQ(std::string{"\360\237\230\232"}, insert_unicode_at(KISSING_FACE_WITH_CLOSED_EYES_CODE_POINT));
}

// create copy of string converted to utf8

class CreateUTF8CopyTest : public Test
{
protected:
    void TearDown() override
    {
        utf_init(CHARSET_NAME_UTF8, CHARSET_NAME_UTF8);

        Test::TearDown();
    }

    std::string m_before;
    std::string m_after;
};

TEST_F(CreateUTF8CopyTest, emptyView)
{
    ASSERT_TRUE(create_utf8_copy(std::string_view{}).empty());
}

TEST_F(CreateUTF8CopyTest, ascii)
{
    m_before = "Lorem ipsum";

    m_after = create_utf8_copy(m_before);

    ASSERT_EQ(m_before, m_after) << "create_utf8_copy of ASCII string did not create an identical copy";
}

TEST_F(CreateUTF8CopyTest, iso8859_1)
{
    m_before = "Quoi, le biblioth\350que est ferm\351\240!";
    const char *expected = "Quoi, le biblioth\303\250que est ferm\303\251\302\240!";
    utf_init(CHARSET_NAME_ISO8859_1, CHARSET_NAME_UTF8);

    m_after = create_utf8_copy(m_before);

    ASSERT_EQ(expected, m_after) << "create_utf8_copy of ISO-8859-1 string did not create a corresponding UTF-8 copy";
}
