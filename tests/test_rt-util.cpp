/* test_rt-util.cpp - unit tests for rt-util.cpp
 * vi: set sw=4 ts=8 ai sm noet :
 */
// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-util.h>

#include <config/common.h>
#include <trn/Article.h>
#include <trn/charsubst.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>
#include <trn/Subject.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <string_view>

using namespace testing;

constexpr int COMPRESSED_NAME_MAX{29};

static std::string extract_author_name(std::string text)
{
    return std::string{extract_name(text)};
}

TEST(ExtractNameTest, usesNameBeforeAngleAddress)
{
    EXPECT_EQ("Ross Ridge", extract_author_name("Ross Ridge <ross@example.com>"));
}

TEST(ExtractNameTest, usesNameFromComment)
{
    EXPECT_EQ("Ross Ridge", extract_author_name("ross@example.com (Ross Ridge)"));
}

TEST(ExtractNameTest, stripsQuotesFromDisplayName)
{
    EXPECT_EQ("Ross Ridge", extract_author_name("\"Ross Ridge\" <ross@example.com>"));
}

TEST(ExtractNameTest, returnsEmptyWhenNoDisplayNameExists)
{
    EXPECT_EQ("", extract_author_name("<ross@example.com>"));
}

class CompressNameTest : public Test
{
protected:
    void configure_before_expected(const char *before, const char *expected)
    {
        m_before = before;
        m_expected = expected;
    }

    std::string run_compress_name()
    {
        return compress_name(m_before, std::strlen(m_expected) + 1);
    }

    const char *m_before{};
    const char *m_expected{};
};

TEST_F(CompressNameTest, dropTrailingJunkComma)
{
    configure_before_expected("Ross Douglas Ridge, The Great HTMU", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, dropTrailingJunkSemi)
{
    configure_before_expected("Ross Douglas Ridge; The Great HTMU", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, dropTrailingJunkAt)
{
    configure_before_expected("Ross Douglas Ridge @ The Great HTMU", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, dropTrailingJunkDashDash)
{
    configure_before_expected("Ross Douglas Ridge--The Great HTMU", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, dropTrailingJunkDashSpace)
{
    configure_before_expected("Ross Douglas Ridge- The Great HTMU", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, dropTrailingJunkOpenParen)
{
    configure_before_expected("Ross Douglas Ridge (The Great HTMU)", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, keepTrailingJunkDollaDolla)
{
    configure_before_expected("Ross Douglas Ridge $$ The Great HTMU", "Ross D R T G HTMU");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, middleInitial)
{
    configure_before_expected("Ross Douglas Ridge", "Ross D Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, dropMiddleName)
{
    configure_before_expected("Ross Douglas Ridge", "Ross Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, DISABLED_firstMiddleInitials)
{
    configure_before_expected("Ross Douglas Ridge", "R D Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, firstInitial)
{
    configure_before_expected("Ross Douglas Ridge", "R Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, DISABLED_truncated)
{
    configure_before_expected("Ross Douglas Ridge", "R Ridg");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, DISABLED_firstInitialDropped)
{
    configure_before_expected("R. Douglas Ridge", "Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, ddsDropped)
{
    configure_before_expected("Ross Douglas Ridge D.D.S.", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, DISABLED_quotedMiddleDropsQuotes)
{
    configure_before_expected(R"(Ross "Douglas" Ridge)", "Ross Douglas Ridge");

    const std::string result = run_compress_name();

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, SAIC)
{
    configure_before_expected("School of the Art Institute of Chicago", "School o t A I o Chicago");

    const std::string result = compress_name(m_before, COMPRESSED_NAME_MAX);

    EXPECT_EQ(m_expected, result);
}

TEST_F(CompressNameTest, PCS)
{
    configure_before_expected("IEEE Professional Communication Society", "IEEE P C Society");

    const std::string result = compress_name(m_before, COMPRESSED_NAME_MAX);

    EXPECT_EQ(m_expected, result);
}

class CompressFromTest : public Test
{
protected:
    void SetUp() override
    {
        m_previous_char_subst = g_char_subst;
        g_char_subst = g_charsets.c_str();
    }

    void TearDown() override
    {
        g_char_subst = m_previous_char_subst;
    }

    const char *m_previous_char_subst{};
};

static std::string padded(std::string text, std::size_t width)
{
    if (text.size() < width)
    {
        text.append(width - text.size(), ' ');
    }
    return text;
}

TEST_F(CompressFromTest, usesAngleAddressDisplayName)
{
    EXPECT_EQ(padded("Ross Ridge", 16), compress_from("Ross Ridge <ross@example.com>", 16));
}

TEST_F(CompressFromTest, usesCommentDisplayName)
{
    EXPECT_EQ(padded("Ross Ridge", 16), compress_from("ross@example.com (Ross Ridge)", 16));
}

TEST_F(CompressFromTest, stripsAngleBracketsFromAddress)
{
    EXPECT_EQ("ross@example.com", compress_from("<ross@example.com>", 16));
}

TEST_F(CompressFromTest, padsShortAddress)
{
    EXPECT_EQ(padded("r@example.com", 16), compress_from("r@example.com", 16));
}

TEST_F(CompressFromTest, usesNoNameForEmptyInput)
{
    EXPECT_EQ(std::string{"NO NAME"} + std::string(8, ' '), compress_from("", 8));
}

TEST_F(CompressFromTest, truncatesPlainAddressFromStart)
{
    EXPECT_EQ("really.lon", compress_from("really.long.local@example.com", 10));
}

TEST_F(CompressFromTest, keepsBangPathUserWhenItFits)
{
    EXPECT_EQ("username", compress_from("host!username@example.com", 8));
}

TEST_F(CompressFromTest, returnsEmptyForNonPositiveWidth)
{
    EXPECT_EQ("", compress_from("Ross Ridge <ross@example.com>", 0));
}

class CompressSubjectTest : public Test
{
protected:
    void SetUp() override
    {
        m_previous_char_subst = g_char_subst;
        m_previous_threaded_group = g_threaded_group;
        m_previous_sel_rereading = g_sel_rereading;
        m_previous_unbroken_subjects = g_unbroken_subjects;

        g_char_subst = g_charsets.c_str();
        g_threaded_group = false;
        g_sel_rereading = false;
        g_unbroken_subjects = false;

        m_subject.m_str = "    Plain subject";
        m_subject.m_articles = &m_article;
        m_subject.m_thread = &m_article;
        m_article.m_subj = &m_subject;
        m_article.m_flags = AF_UNREAD;
    }

    void TearDown() override
    {
        g_char_subst = m_previous_char_subst;
        g_threaded_group = m_previous_threaded_group;
        g_sel_rereading = m_previous_sel_rereading;
        g_unbroken_subjects = m_previous_unbroken_subjects;
    }

    std::string compress(const Article *article, int width)
    {
        return compress_subj(article, width);
    }

    Subject     m_subject{};
    Article     m_article{};
    const char *m_previous_char_subst{};
    bool        m_previous_threaded_group{};
    bool        m_previous_sel_rereading{};
    bool        m_previous_unbroken_subjects{};
};

TEST_F(CompressSubjectTest, returnsMissingForNullArticle)
{
    EXPECT_EQ("<MISSING>", compress(nullptr, 80));
}

TEST_F(CompressSubjectTest, returnsStrippedSubject)
{
    EXPECT_EQ("Plain subject", compress(&m_article, 80));
}

TEST_F(CompressSubjectTest, marksNonFirstArticle)
{
    Article reply{};
    reply.m_subj = &m_subject;
    reply.m_flags = AF_UNREAD;

    EXPECT_EQ(">Plain subject", compress(&reply, 80));
}

TEST_F(CompressSubjectTest, removesWasSubject)
{
    m_subject.m_str = "    Plain subject (was: old subject)";

    EXPECT_EQ("Plain subject ", compress(&m_article, 80));
}

TEST_F(CompressSubjectTest, truncatesLongSubject)
{
    m_subject.m_str = "    LongSubjectValue";

    EXPECT_EQ("LongSubj", compress(&m_article, 8));
}

TEST(SubjectHasReTest, one)
{
    constexpr std::string_view before{"Re: followup"};
    std::string_view           after;

    const bool hasRe = subject_has_re(before, after);

    EXPECT_TRUE(hasRe);
    EXPECT_EQ("followup", after);
}

TEST(SubjectHasReTest, noRePresent)
{
    constexpr std::string_view subject{TRN_TEST_HEADER_STRIPPED_SUBJECT};
    std::string_view           interesting;

    const bool hasRe = subject_has_re(subject, interesting);

    ASSERT_FALSE(hasRe);
    ASSERT_EQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, skipsLeadingWhitespaceWithoutReplyPrefix)
{
    constexpr std::string_view subject{" \t" TRN_TEST_HEADER_STRIPPED_SUBJECT};
    std::string_view           interesting;

    const bool hasRe = subject_has_re(subject, interesting);

    EXPECT_FALSE(hasRe);
    EXPECT_EQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, stripAllRe)
{
    constexpr std::string_view subject{"Re: Re: Re: " TRN_TEST_HEADER_STRIPPED_SUBJECT};
    std::string_view           interesting;

    const bool hasRe = subject_has_re(subject, interesting);

    ASSERT_TRUE(hasRe);
    ASSERT_EQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, stripRe3)
{
    constexpr std::string_view subject{"Re^3: " TRN_TEST_HEADER_STRIPPED_SUBJECT};
    std::string_view           interesting;

    const bool hasRe = subject_has_re(subject, interesting);

    ASSERT_TRUE(hasRe);
    ASSERT_EQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}

TEST(SubjectHasReTest, stripOneRe)
{
    constexpr std::string_view subject{"Re: Re: Re: " TRN_TEST_HEADER_STRIPPED_SUBJECT};
    std::string_view           interesting;

    const bool hasRe = strip_one_re(subject, interesting);

    ASSERT_TRUE(hasRe);
    ASSERT_EQ("Re: Re: " TRN_TEST_HEADER_STRIPPED_SUBJECT, interesting);
}
