#include <gtest/gtest.h>

#include <common.h>

using namespace testing;

namespace {

struct CommonTest : Test
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

TEST_F(CommonTest, skipEqNullPtr)
{
    const char *pos = skip_eq(nullptr, ' ');

    ASSERT_EQ(nullptr, pos);
}

TEST_F(CommonTest, skipEqMiddle)
{
    configure_before_after("   This is a test.", "This is a test.");

    const char *pos = skip_eq(m_buffer, ' ');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(CommonTest, skipEqEnd)
{
    configure_before_after("       ", "");

    const char *pos = skip_eq(m_buffer, ' ');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(CommonTest, skipNeNullPtr)
{
    const char *pos = skip_ne(nullptr, ' ');

    ASSERT_EQ(nullptr, pos);
}

TEST_F(CommonTest, skipNeMiddle)
{
    configure_before_after("   This is a test.", "This is a test.");

    const char *pos = skip_ne(m_buffer, 'T');

    ASSERT_STREQ(m_after, pos);
}

TEST_F(CommonTest, skipNeEnd)
{
    configure_before_after("This text contains no exclamation point.", "");

    const char *pos = skip_ne(m_buffer, '!');

    ASSERT_STREQ(m_after, pos);
}


TEST_F(CommonTest, emptyNullPtr)
{
    ASSERT_TRUE(empty(nullptr));
}

TEST_F(CommonTest, emptyNoChars)
{
    ASSERT_TRUE(empty(""));
}

TEST_F(CommonTest, emptyChars)
{
    ASSERT_FALSE(empty("There be chars here!"));
}

TEST_F(CommonTest, skipDigitsNullPtr)
{
    char *p = nullptr;

    ASSERT_EQ(nullptr, skip_digits(p));
}

TEST_F(CommonTest, skipDigitsMiddle)
{
    configure_before_after("1965 was a good year for television.", " was a good year for television.");

    const char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(CommonTest, skipDigitsEnd)
{
    configure_before_after("1965", "");

    const char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(CommonTest, skipConstDigitsEnd)
{
    configure_before_after("1965", "");
    const char *buffer = m_buffer;

    const char *pos = skip_digits(buffer);

    ASSERT_STREQ(m_after, pos);
}

TEST_F(CommonTest, skipMutableDigitsEnd)
{
    configure_before_after("1965", "");

    char *pos = skip_digits(m_buffer);

    ASSERT_STREQ(m_after, pos);
}
