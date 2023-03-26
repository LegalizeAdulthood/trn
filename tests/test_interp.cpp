#include <gtest/gtest.h>

#include <array>

#include "common.h"
#include "intrp.h"

constexpr int BUFFER_SIZE{4096};

struct InterpolatorTest : public testing::Test
{
    std::array<char, BUFFER_SIZE> buffer{};
};

TEST_F(InterpolatorTest, noEscapes)
{
    char pattern[]{"this string contains no escapes"};

    const char *new_pattern = dointerp(buffer.data(), BUFFER_SIZE, pattern, "", nullptr);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{pattern}, std::string{buffer.data()});
}

TEST_F(InterpolatorTest, firstStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = dointerp(buffer.data(), BUFFER_SIZE, pattern, "[]", nullptr);

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ(std::string{"this string contains no escapes "}, std::string{buffer.data()});
}

TEST_F(InterpolatorTest, subsequentStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = dointerp(buffer.data(), BUFFER_SIZE, pattern, "()[]", nullptr);

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ(std::string{"this string contains no escapes "}, std::string{buffer.data()});
}
