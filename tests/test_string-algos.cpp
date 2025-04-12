#include <gtest/gtest.h>

#include <config/common.h>
#include <string-algos.h>

using namespace testing;

namespace {

struct StringAlgosTest : Test
{
protected:
    void configure_before_after(const char *before, const char *after)
    {
        strncpy(m_buffer, before, LBUFLEN);
        m_after = after;
    }

    char        m_buffer[LBUFLEN]{};
    const char *m_after{};
};

} // namespace

TEST_F(StringAlgosTest, skipEqNullPtr)
{
    const char *buffer{};

    const char *pos = skip_eq(buffer, ' ');

    ASSERT_EQ(nullptr, pos);
}

TEST_F(StringAlgosTest, skipEqNoChange)
{
    configure_before_after("No change.", "No change.");

    const char *pos = skip_eq(m_buffer, ' ');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipEqMiddle)
{
    configure_before_after("   This is a test.", "This is a test.");

    const char *pos = skip_eq(m_buffer, ' ');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipEqEnd)
{
    configure_before_after("       ", "");

    const char *pos = skip_eq(m_buffer, ' ');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNeNullPtr)
{
    const char *buffer{};

    const char *pos = skip_ne(buffer, ' ');

    ASSERT_EQ(nullptr, pos);
}

TEST_F(StringAlgosTest, skipNeNoChange)
{
    configure_before_after("No change.", "No change.");

    const char *pos = skip_ne(m_buffer, 'N');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNeMiddle)
{
    configure_before_after("   This is a test.", "This is a test.");

    const char *pos = skip_ne(m_buffer, 'T');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNeEnd)
{
    configure_before_after("This text contains no exclamation point.", "");

    const char *pos = skip_ne(m_buffer, '!');

    ASSERT_STREQ(m_after, pos);
}


TEST_F(StringAlgosTest, emptyNullPtr)
{
    ASSERT_TRUE(empty(nullptr));
}

TEST_F(StringAlgosTest, emptyNoChars)
{
    ASSERT_TRUE(empty(""));
}

TEST_F(StringAlgosTest, emptyChars)
{
    ASSERT_FALSE(empty("There be chars here!"));
}

TEST_F(StringAlgosTest, skipDigitsNullPtr)
{
    char *p{};

    ASSERT_EQ(nullptr, skip_digits(p));
}

TEST_F(StringAlgosTest, skipDigitsNoChange)
{
    configure_before_after("No change.", "No change.");

    const char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipDigitsMiddle)
{
    configure_before_after("1965 was a good year for television.", " was a good year for television.");

    const char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipDigitsEnd)
{
    configure_before_after("1965", "");

    const char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipConstDigitsEnd)
{
    configure_before_after("1965", "");
    const char *buffer = m_buffer;

    const char *pos = skip_digits(buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipMutableDigitsEnd)
{
    configure_before_after("1965", "");

    char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipSpaceNullPtr)
{
    const char *buffer{};

    ASSERT_EQ(nullptr, skip_space(buffer));
}

TEST_F(StringAlgosTest, skipSpaceNoChange)
{
    configure_before_after("No change.", "No change.");

    const char *pos = skip_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipSpaceMiddle)
{
    configure_before_after(" \t\f\v\r\nThere's plenty of space in here.", "There's plenty of space in here.");

    const char *pos = skip_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNonSpaceNullPtr)
{
    const char *buffer{};

    ASSERT_EQ(nullptr, skip_non_space(buffer));
}

TEST_F(StringAlgosTest, skipNonSpaceNoChange)
{
    configure_before_after(" No change.", " No change.");

    const char *pos = skip_non_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNonSpaceMiddle)
{
    configure_before_after("Hello, world!", " world!");

    const char *pos = skip_non_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipAlphaNullPtr)
{
    const char *buffer{};

    ASSERT_EQ(nullptr, skip_alpha(buffer));
}

TEST_F(StringAlgosTest, skipAlphaNoChange)
{
    configure_before_after("123 No change.", "123 No change.");

    const char *pos = skip_alpha(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipAlphaMiddle)
{
    configure_before_after("Hello, world!", ", world!");

    const char *pos = skip_alpha(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNonAlphaNullPtr)
{
    const char *buffer{};

    ASSERT_EQ(nullptr, skip_non_alpha(buffer));
}

TEST_F(StringAlgosTest, skipNonAlphaNoChange)
{
    configure_before_after("No change.", "No change.");

    const char *pos = skip_non_alpha(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipNonAlphaMiddle)
{
    configure_before_after("123 Hello, world!", "Hello, world!");

    const char *pos = skip_non_alpha(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipHorSpaceNullPtr)
{
    const char *buffer{};

    ASSERT_EQ(nullptr, skip_hor_space(buffer));
}

TEST_F(StringAlgosTest, skipHorSpaceNoChange)
{
    configure_before_after("No change.", "No change.");

    const char *pos = skip_hor_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipHorSpaceMiddleTab)
{
    configure_before_after("\t\t\tHello, world!", "Hello, world!");

    const char *pos = skip_hor_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, skipHorSpaceMiddleSpace)
{
    configure_before_after("   Hello, world!", "Hello, world!");

    const char *pos = skip_hor_space(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(StringAlgosTest, isHorSpaceNul)
{
    ASSERT_FALSE(is_hor_space('\0'));
}

TEST_F(StringAlgosTest, isHorSpaceSpaceTab)
{
    ASSERT_TRUE(is_hor_space(' '));
    ASSERT_TRUE(is_hor_space('\t'));
}

TEST_F(StringAlgosTest, isHorSpaceOtherWhiteSpace)
{
    ASSERT_FALSE(is_hor_space('\r'));
    ASSERT_FALSE(is_hor_space('\n'));
    ASSERT_FALSE(is_hor_space('\v'));
}

TEST_F(StringAlgosTest, isHorSpacePrintable)
{
    ASSERT_FALSE(is_hor_space('a'));
    ASSERT_FALSE(is_hor_space('!'));
    ASSERT_FALSE(is_hor_space('B'));
}
