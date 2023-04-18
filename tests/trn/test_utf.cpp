/* test_utf.c - unit tests for utf.c
 * vi: set sw=4 ts=8 ai sm noet :
 */

#include <gtest/gtest.h>

#include <string.h>

#include "common.h"
#include "utf.h"

using namespace testing;

TEST(UTFInitTest, charset_null_utf8)
{
    ASSERT_EQ(CHARSET_UNKNOWN, utf_init(NULL, "utf8"));
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
    const char *name;
    const char *tag;
};

constexpr CharsetParam charsets[] = {
    // clang-format off
    { "us-ascii",     TAG_ASCII },
    { "ascii",        TAG_ASCII },
    { "iso8859-1",    TAG_ISO8859_1 },
    { "WiNdOwS-1252", TAG_WINDOWS_1252 },
    { "utf8",         TAG_UTF8 },
    { "UTF-8",        TAG_UTF8 },
    // clang-format on
};

class TestInputCharsetName : public TestWithParam<CharsetParam>
{
};

TEST_P(TestInputCharsetName, tag_from_name)
{
    const char *before = GetParam().name;
    const char *expected = GetParam().tag;
    utf_init(before, "utf8");

    const char *after = input_charset_name();

    ASSERT_STREQ(expected, after);
}

INSTANTIATE_TEST_SUITE_P(CharsetNames, TestInputCharsetName, ValuesIn(charsets));

class TestOutputCharsetName : public TestWithParam<CharsetParam>
{
};

TEST_P(TestOutputCharsetName, tag_from_name)
{
    const char *before = GetParam().name;
    const char *expected = GetParam().tag;
    utf_init("utf8", before);

    const char *after = output_charset_name();

    EXPECT_STREQ(expected, after);
}

INSTANTIATE_TEST_SUITE_P(CharsetNames, TestOutputCharsetName, ValuesIn(charsets));

constexpr const char *const ARBITRARY_ASCII{"a"};
constexpr const char *const ARBITRARY_ISO8859D1_1{"\303\241"};
constexpr const char *const ARBITRARY_ISO8859D1_2{"\303\246"};
constexpr const char *const ARBITRARY_CJK_BASIC{"\345\244\251"};
constexpr const char *const ASCII_SOH{"\001"};
constexpr const char *const ASCII_DEL{"\177"};
constexpr const char *const ASCII_SPACE{" "};
constexpr const char *const ASCII_TILDE{"~"};

TEST(ByteLengthTest, length_at_null)
{
    ASSERT_EQ(0, byte_length_at(nullptr));
}

TEST(ByteLengthTest, length_at_ascii)
{
    ASSERT_EQ(1, byte_length_at(ARBITRARY_ASCII));
}

TEST(ByteLengthTest, length_at_iso8859_1)
{
    ASSERT_EQ(2, byte_length_at(ARBITRARY_ISO8859D1_1));
    ASSERT_EQ(2, byte_length_at(ARBITRARY_ISO8859D1_2));
}

TEST(ByteLengthTest, byte_length_at_cjk_basic)
{
    ASSERT_EQ(3, byte_length_at(ARBITRARY_CJK_BASIC));
}

TEST(AtNormalCharacterTest, nullptr)
{
    ASSERT_FALSE(at_norm_char(nullptr));
}

TEST(AtNormalCharacterTest, SOH)
{
    ASSERT_FALSE(at_norm_char(ASCII_SOH));
}

TEST(AtNormalCharacterTest, space)
{
    ASSERT_TRUE(at_norm_char(ASCII_SPACE));
}

TEST(AtNormalCharacterTest, tilde)
{
    ASSERT_TRUE(at_norm_char(ASCII_TILDE));
}

TEST(AtNormalCharacterTest, DEL)
{
    ASSERT_FALSE(at_norm_char(ASCII_DEL));
}

TEST(AtNormalCharacterTest, iso8859_1)
{
    ASSERT_TRUE(at_norm_char(ARBITRARY_ISO8859D1_1));
    ASSERT_TRUE(at_norm_char(ARBITRARY_ISO8859D1_2));
}

TEST(AtNormalCharacterTest, cjk_basic)
{
    ASSERT_TRUE(at_norm_char(ARBITRARY_CJK_BASIC));
}

TEST(VisualAdvanceWidthTest, nullptr)
{
    ASSERT_EQ(0, put_char_adv(nullptr, false));
    ASSERT_EQ(0, put_char_adv(nullptr, true));
}

TEST(VisualAdvanceWidthTest, ascii)
{
    char sp0[80];
    strcpy(sp0, ARBITRARY_ASCII);
    char *sp = sp0;

    int retval = put_char_adv(&sp, true);

    ASSERT_EQ(1, retval) << "put_char_adv(" << sp0 << ")";
    ASSERT_EQ(1, sp - sp0) << "put_char_adv(" << sp0 << ")";
}

TEST(VisualAdvanceWidthTest, iso8859_1)
{
    char sp0[80];
    strcpy(sp0, ARBITRARY_ISO8859D1_1);
    char *sp = sp0;

    int retval = put_char_adv(&sp, true);

    ASSERT_EQ(1, retval) << "put_char_adv(" << sp0 << ")";
    ASSERT_EQ(2, sp - sp0) << "put_char_adv(" << sp0 << ")";
}

TEST(VisualAdvanceWidthTest, cjk_basic)
{
    char sp0[80];
    strcpy(sp0, ARBITRARY_CJK_BASIC);
    char *sp = sp0;

    int retval = put_char_adv(&sp, true);

    ASSERT_EQ(2, retval) << "put_char_adv(" << sp0 << ")";
    ASSERT_EQ(3, sp - sp0) << "put_char_adv(" << sp0 << ")";
}

/* code point decoding */
TEST(CodePointDecodingTest, nullptr)
{
    ASSERT_EQ(INVALID_CODE_POINT, code_point_at(nullptr));
}

constexpr CODE_POINT ASCII_SPACE_CODE_POINT = 0x20;
constexpr CODE_POINT ASCII_5_CODE_POINT = 0x35;
constexpr CODE_POINT ISO8859D1_ETH_CODE_POINT = 0xF0;
constexpr CODE_POINT CJK_SHIN_CODE_POINT = 0x05E9;
constexpr CODE_POINT OY_CODE_POINT = 0x18B0;
constexpr CODE_POINT KISSING_FACE_WITH_CLOSED_EYES_CODE_POINT = 0x1F61A;

TEST(CodePointDecodingTest, ascii_space)
{
    ASSERT_EQ(ASCII_SPACE_CODE_POINT, code_point_at(" "));
}

TEST(CodePointDecodingTest, ascii_5)
{
    ASSERT_EQ(ASCII_5_CODE_POINT, code_point_at("5"));
}

TEST(CodePointDecodingTest, eth)
{
    ASSERT_EQ(ISO8859D1_ETH_CODE_POINT, code_point_at("\303\260"));
}

TEST(CodePointDecodingTest, shin)
{
    ASSERT_EQ(CJK_SHIN_CODE_POINT, code_point_at("\327\251"));
}

TEST(CodePointDecodingTest, oy)
{
    ASSERT_EQ(OY_CODE_POINT, code_point_at("\341\242\260"));
}

TEST(CodePointDecodingTest, kissing_face_with_closed_eyes)
{
    ASSERT_EQ(KISSING_FACE_WITH_CLOSED_EYES_CODE_POINT, code_point_at("\360\237\230\232"));
}

TEST(VisualLengthTest, nullptr)
{
    ASSERT_EQ(0, visual_length_of(nullptr));
}

TEST(VisualLengthTest, ascii)
{
    ASSERT_EQ(3, visual_length_of("cat"));
}

TEST(VisualLengthTest, iso_8859_1)
{
    ASSERT_EQ(7, visual_length_of("libert\303\251"));
    ASSERT_EQ(4, visual_length_of("b\303\251b\303\251"));
    ASSERT_EQ(4 /* combining acute */, visual_length_of("be\314\201be\314\201"));
    ASSERT_EQ(0 /* combining acute */, visual_length_of("\314\201"));
}

TEST(VisualLengthTest, cjk)
{
    ASSERT_EQ(4, visual_length_of("\350\211\257\345\277\203"));
}

TEST(InsertUnicodeAtTest, null)
{
    ASSERT_EQ(0, insert_unicode_at(nullptr, 0x40));
}

TEST(InsertUnicodeAtTest, ascii)
{
    char buf[8]{};

    ASSERT_EQ(1, insert_unicode_at(buf, 0x64));

    ASSERT_EQ(std::string{"d"}, buf);
}

TEST(InsertUnicodeAtTest, iso_8859_1)
{
    char buf[8]{};

    ASSERT_EQ(2, insert_unicode_at(buf, 0xE4));

    ASSERT_EQ(std::string{"\303\244"}, buf);
}

TEST(InsertUnicodeAtTest, cjk_basic)
{
    char buf[8]{};

    ASSERT_EQ(3, insert_unicode_at(buf, 0x4E00));

    ASSERT_EQ(std::string{"\344\270\200"}, buf);
}

TEST(InsertUnicodeAtTest, kissing_face_with_closed_eyes)
{
    char buf[8]{};

    ASSERT_EQ(4, insert_unicode_at(buf, KISSING_FACE_WITH_CLOSED_EYES_CODE_POINT));

    ASSERT_EQ(std::string{"\360\237\230\232"}, buf);
}

/* create copy of string converted to utf8 */

class CreateUTF8CopyTest : public Test
{
protected:
    void TearDown() override
    {
        if (m_after)
            free(m_after);

        Test::TearDown();
    }

    char  m_before[80]{};
    char *m_after{};
};

TEST_F(CreateUTF8CopyTest, nullptr)
{
    ASSERT_EQ(nullptr, create_utf8_copy(nullptr));
}

TEST_F(CreateUTF8CopyTest, ascii)
{
    strcpy(m_before, "Lorem ipsum");

    m_after = create_utf8_copy(m_before);

    ASSERT_NE(nullptr, m_after) << "create_utf8_copy of ASCII string returned nullptr";
    ASSERT_NE(m_before, m_after) << "create_utf8_copy of ASCII string did not alloc a new string";
    ASSERT_STREQ(m_before, m_after) << "create_utf8_copy of ASCII string did not create an identical copy";
}

TEST_F(CreateUTF8CopyTest, iso8859_1)
{
    strcpy(m_before, "Quoi, le biblioth\350que est ferm\351\240!");
    const char *expected = "Quoi, le biblioth\303\250que est ferm\303\251\302\240!";
    utf_init("iso8859-1", "utf8");

    m_after = create_utf8_copy(m_before);

    ASSERT_NE(nullptr, m_after) << "create_utf8_copy of ISO-8859-1 string returned nullptr";
    ASSERT_NE(m_before, m_after) << "create_utf8_copy of ISO-8859-1 string did not alloc a new string";
    ASSERT_STREQ(expected, m_after)
        << "create_utf8_copy of ISO-8859-1 string did not create a corresponding UTF-8 copy";
}

/* terminate string at nth visual column instead of at the nth byte */
class TerminateStringAtVisualIndexTest : public Test
{
protected:
    void configure_before_after(const char *before, const char *after)
    {
        m_before = before;
        m_buffer = strdup(before);
        m_after = after;
    }

    void TearDown() override
    {
        if (m_buffer)
            free(m_buffer);
        Test::TearDown();
    }

    const char *m_before{};
    char       *m_buffer{};
    const char *m_after{};
};

TEST_F(TerminateStringAtVisualIndexTest, nullptr_doesnt_crash)
{
    terminate_string_at_visual_index(nullptr, 1);
}

TEST_F(TerminateStringAtVisualIndexTest, negative_index)
{
    configure_before_after("abcdefg", "");

    terminate_string_at_visual_index(m_buffer, -12345);

    ASSERT_STREQ(m_after, m_buffer) << "terminate_string_at_visual_index(-12345)";
}

TEST_F(TerminateStringAtVisualIndexTest, ascii)
{
    configure_before_after("abcdefg", "abcde");

    terminate_string_at_visual_index(m_buffer, 5);

    EXPECT_STREQ(m_after, m_buffer) <<  "terminate_string_at_visual_index(5)";
}

TEST_F(TerminateStringAtVisualIndexTest, iso8859_1)
{
    configure_before_after("\303\241\303\255\303\272\303\251\303\263\303\244\303\257\303\274\303\253\303\266",
                           "\303\241\303\255\303\272\303\251\303\263");

    terminate_string_at_visual_index(m_buffer, 5);

    EXPECT_STREQ(m_after, m_buffer) << "terminate_string_at_visual_index(5)";
}

TEST_F(TerminateStringAtVisualIndexTest, cjk_basic)
{
    configure_before_after(
        "\257\247\345\214\226\351\243\233\347\201\260\344\270\215\344\275\234\346\265\256\345\241\265",
        "\257\247\345\214\226\351\243\233\347\201\260");

    terminate_string_at_visual_index(m_buffer, 8);

    EXPECT_STREQ(m_after, m_buffer) << "terminate_string_at_visual_index(8)";
}

TEST_F(TerminateStringAtVisualIndexTest, cjk_basic_at_wrong_boundary)
{
    configure_before_after(
        "\345\257\247\345\214\226\351\243\233\347\201\260\344\270\215\344\275\234\346\265\256\345\241\265",
        "\345\257\247\345\214\226\351\243\233\347\201\260 ");

    terminate_string_at_visual_index(m_buffer, 9);

    EXPECT_STREQ(m_after, m_buffer) << "terminate_string_at_visual_index(9)";
}
