// This software is copyrighted as detailed in the LICENSE file.
#include <trn/string-algos.h>

#include <config/common.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string_view>

using namespace testing;

namespace
{

struct StringAlgosTest : Test
{
protected:
    void configure_before_after(const char *before, const char *after)
    {
        std::strncpy(m_buffer, before, LINE_BUF_LEN);
        m_after = after;
    }

    char        m_buffer[LINE_BUF_LEN]{};
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

TEST_F(StringAlgosTest, skipEqStringView)
{
    std::string_view text{"   This is a test."};

    ASSERT_EQ(std::string_view{"This is a test."}, skip_eq(text, ' '));
}

TEST_F(StringAlgosTest, skipNeStringView)
{
    std::string_view text{"This is a test."};

    ASSERT_EQ(std::string_view{" is a test."}, skip_ne(text, ' '));
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

TEST_F(StringAlgosTest, skipDigitsStringView)
{
    std::string_view text{"1965 was a good year for television."};

    ASSERT_EQ(std::string_view{" was a good year for television."}, skip_digits(text));
}

TEST_F(StringAlgosTest, skipSpaceStringView)
{
    std::string_view text{" \t\f\v\r\nThere's plenty of space in here."};

    ASSERT_EQ(std::string_view{"There's plenty of space in here."}, skip_space(text));
}

TEST_F(StringAlgosTest, skipNonSpaceStringView)
{
    std::string_view text{"Hello, world!"};

    ASSERT_EQ(std::string_view{" world!"}, skip_non_space(text));
}

TEST_F(StringAlgosTest, skipAlphaStringView)
{
    std::string_view text{"Hello, world!"};

    ASSERT_EQ(std::string_view{", world!"}, skip_alpha(text));
}

TEST_F(StringAlgosTest, skipNonAlphaStringView)
{
    std::string_view text{"123 Hello, world!"};

    ASSERT_EQ(std::string_view{"Hello, world!"}, skip_non_alpha(text));
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

TEST_F(StringAlgosTest, skipHorSpaceStringView)
{
    std::string_view text{" \t\tHello, world!"};

    ASSERT_EQ(std::string_view{"Hello, world!"}, skip_hor_space(text));
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
