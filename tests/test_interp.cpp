#include <gtest/gtest.h>

#include <array>

#include "common.h"
#include "datasrc.h"
#include "env.h"
#include "intrp.h"
#include "ng.h"

constexpr int BUFFER_SIZE{4096};

namespace
{

struct InterpolatorTest : testing::Test
{
protected:
    void SetUp() override
    {
        Test::SetUp();

        g_in_ng = false;
        g_art = 0;
    }

    void TearDown() override
    {
        datasrc_finalize();
    }

    char *interpolate(char *pattern, const char *stoppers = "")
    {
        return dointerp(m_buffer.data(), BUFFER_SIZE, pattern, stoppers, nullptr);
    }

    std::array<char, BUFFER_SIZE> m_buffer{};
};

} // namespace

TEST_F(InterpolatorTest, noEscapes)
{
    char pattern[]{"this string contains no escapes"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{pattern}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, firstStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = interpolate(pattern, "[]");

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ(std::string{"this string contains no escapes "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, subsequentStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = interpolate(pattern, "()[]");

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ(std::string{"this string contains no escapes "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNumberOutsideNewsgroup)
{
    char pattern[]{"Article %a"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNumberInsideNewsgroup)
{
    char pattern[]{"Article %a"};
    g_in_ng = true;
    g_art = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article 624"}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNameOutsideNewsgroup)
{
    char pattern[]{"Article %A"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNameInsideRemoteNewsgroupArticleClosed)
{
    char pattern[]{"Article %A"};
    g_in_ng = true;
    g_art = 624;
    g_tmp_dir = getenv("TEMP");
    datasrc_init();
    g_datasrc = datasrc_first();

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{m_buffer.data()});
}
