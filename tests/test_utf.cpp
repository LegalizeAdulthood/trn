/* test_utf.cpp - unit tests for utf.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/utf.h>

#include <config/common.h>
#include <trn/charsubst.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

using namespace testing;

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
    constexpr std::string_view input{"\304\326\334\337"};

    EXPECT_EQ("AeOeUess", str_char_subst(input, 'a'));
}

TEST(CharSubstTest, transliteratesLatin1ToMonospacedAscii)
{
    constexpr std::string_view input{"\304\326\334\337"};

    EXPECT_EQ("AOUs", str_char_subst(input, 'm'));
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
