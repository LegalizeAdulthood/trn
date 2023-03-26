#include <gtest/gtest.h>

#include <array>

#include "common.h"
#include "intrp.h"
#include "ng.h"

constexpr int BUFFER_SIZE{4096};

struct InterpolatorTest : public testing::Test
{
protected:
    void SetUp() override
    {
        Test::SetUp();

        g_in_ng = false;
        g_art = 0;
    }

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

TEST_F(InterpolatorTest, articleNumberOutsideNewsgroup)
{
    char pattern[]{"Article %a"};

    const char *new_pattern = dointerp(buffer.data(), BUFFER_SIZE, pattern, "", nullptr);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{buffer.data()});
}

TEST_F(InterpolatorTest, articleNumberInsideNewsgroup)
{
    char pattern[]{"Article %a"};
    g_in_ng = true;
    g_art = 624;

    const char *new_pattern = dointerp(buffer.data(), BUFFER_SIZE, pattern, "", nullptr);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article 624"}, std::string{buffer.data()});
}

TEST_F(InterpolatorTest, articleNameOutsideNewsgroup)
{
    char pattern[]{"Article %A"};

    const char *new_pattern = dointerp(buffer.data(), BUFFER_SIZE, pattern, "", nullptr);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{buffer.data()});
}