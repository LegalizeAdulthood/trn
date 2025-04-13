/* test_utf.c - unit tests for rt-util.c
 * vi: set sw=4 ts=8 ai sm noet :
 */

#include <gtest/gtest.h>

#include "config/common.h"
#include "trn/rt-util.h"

#include "test_config.h"

#include <cstring>
#include <string.h>

using namespace testing;

enum
{
    NAME_MAX = 29
};

class CompressNameTest : public Test
{
protected:
    void configure_before_expected(const char *before, const char *expected)
    {
        m_before = before;
        m_buffer = strdup(m_before);
        m_expected = expected;
    }

    void TearDown() override
    {
        free(m_buffer);
    }

    char *run_compress_name()
    {
        return compress_name(m_buffer, std::strlen(m_expected) + 1);
    }

    const char *m_before{};
    char       *m_buffer{};
    const char *m_expected{};
};

TEST_F(CompressNameTest, dropTrailingJunkComma)
{
    configure_before_expected("Ross Douglas Ridge, The Great HTMU", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, dropTrailingJunkSemi)
{
    configure_before_expected("Ross Douglas Ridge; The Great HTMU", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, dropTrailingJunkAt)
{
    configure_before_expected("Ross Douglas Ridge @ The Great HTMU", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, dropTrailingJunkDashDash)
{
    configure_before_expected("Ross Douglas Ridge--The Great HTMU", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, dropTrailingJunkDashSpace)
{
    configure_before_expected("Ross Douglas Ridge- The Great HTMU", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, dropTrailingJunkOpenParen)
{
    configure_before_expected("Ross Douglas Ridge (The Great HTMU)", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, keepTrailingJunkDollaDolla)
{
    configure_before_expected("Ross Douglas Ridge $$ The Great HTMU", "Ross D R T G HTMU");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, middleInitial)
{
    configure_before_expected("Ross Douglas Ridge", "Ross D Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, dropMiddleName)
{
    configure_before_expected("Ross Douglas Ridge", "Ross Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, DISABLED_firstMiddleInitials)
{
    configure_before_expected("Ross Douglas Ridge", "R D Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, firstInitial)
{
    configure_before_expected("Ross Douglas Ridge", "R Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, DISABLED_truncated)
{
    configure_before_expected("Ross Douglas Ridge", "R Ridg");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, DISABLED_firstInitialDropped)
{
    configure_before_expected("R. Douglas Ridge", "Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, ddsDropped)
{
    configure_before_expected("Ross Douglas Ridge D.D.S.", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, DISABLED_quotedMiddleDropsQuotes)
{
    configure_before_expected(R"(Ross "Douglas" Ridge)", "Ross Douglas Ridge");

    char *result = run_compress_name();

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, SAIC)
{
    configure_before_expected("School of the Art Institute of Chicago", "School o t A I o Chicago");

    char *result = compress_name(m_buffer, NAME_MAX);

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST_F(CompressNameTest, PCS)
{
    configure_before_expected("IEEE Professional Communication Society", "IEEE P C Society");

    char *result = compress_name(m_buffer, NAME_MAX);

    EXPECT_EQ(result, m_buffer);
    EXPECT_STREQ(m_expected, m_buffer);
}

TEST(SubjectHasReTest, one)
{
    char *before = strdup("Re: followup");
    char *after{};
    const char *expected = "followup";

    const bool hasRe = subject_has_Re(before, &after);

    EXPECT_TRUE(hasRe);
    EXPECT_STREQ(expected, after);

    free(before);
}

TEST(SubjectHasReTest, noRePresent)
{
    char buffer[]{TRN_TEST_HEADER_STRIPPED_SUBJECT};
    char *interesting{};

    const bool hasRe = subject_has_Re(buffer, &interesting);

    ASSERT_FALSE(hasRe);
    ASSERT_STREQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, stripAllRe)
{
    char buffer[]{"Re: Re: Re: " TRN_TEST_HEADER_STRIPPED_SUBJECT};
    char *interesting{};

    const bool hasRe = subject_has_Re(buffer, &interesting);

    ASSERT_TRUE(hasRe);
    ASSERT_STREQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, stripRe3)
{
    char buffer[]{"Re^3: " TRN_TEST_HEADER_STRIPPED_SUBJECT};
    char *interesting{};

    const bool hasRe = subject_has_Re(buffer, &interesting);

    ASSERT_TRUE(hasRe);
    ASSERT_STREQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, stripOneRe)
{
    char buffer[]{"Re: Re: Re: " TRN_TEST_HEADER_STRIPPED_SUBJECT};
    char *interesting{};

    const bool hasRe = strip_one_Re(buffer, &interesting);

    ASSERT_TRUE(hasRe);
    ASSERT_STREQ("Re: Re: " TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}
