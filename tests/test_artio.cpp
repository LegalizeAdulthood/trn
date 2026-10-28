// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/art-internal.h>
#include <trn/art.h>
#include <trn/artio.h>

#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/datasrc.h>
#include <trn/head.h>
#include <trn/IniSection.h>
#include <trn/mime-internal.h>
#include <trn/mime.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

class ArticleIoTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_art_fp = g_art_fp;
        m_old_raw_art_size = g_raw_art_size;
        m_old_art_size = g_art_size;
        m_old_do_hiding = g_do_hiding;
        m_old_is_mime = g_is_mime;
        m_old_mime_state = g_mime_state;
        m_old_mime_section = g_mime_section;
        m_old_multipart_separator = g_multipart_separator;
        m_old_data_source = g_data_source;
        m_old_past_header = g_header_type[PAST_HEADER];
        m_old_tc_cols = g_tc_COLS;
        m_old_word_wrap_offset = g_word_wrap_offset;
        m_old_input = input_charset_name();
        m_old_output = output_charset_name();
        m_old_subst = current_char_subst_mode();
        m_old_charsets = g_charsets;
        m_old_profile = g_unicode_substitutions;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        fs::create_directories(m_output_dir, error);

        g_art_fp = nullptr;
        head_init();
        art_io_init();
        g_data_source = &m_data_source;
        g_do_hiding = true;
        g_is_mime = false;
        g_mime_state = NOT_MIME;
        g_mime_section = m_old_mime_section;
        g_header_type[PAST_HEADER].min_pos = ArticlePosition{};
        g_tc_COLS = 30;
        g_word_wrap_offset = 8;
    }

    void TearDown() override
    {
        if (g_art_fp != nullptr)
        {
            std::fclose(g_art_fp);
            g_art_fp = nullptr;
        }
        art_io_final();
        reset_mime_section();
        head_final();

        g_art_fp = m_old_art_fp;
        g_raw_art_size = m_old_raw_art_size;
        g_art_size = m_old_art_size;
        g_do_hiding = m_old_do_hiding;
        g_is_mime = m_old_is_mime;
        g_mime_state = m_old_mime_state;
        g_mime_section = m_old_mime_section;
        g_multipart_separator = m_old_multipart_separator;
        g_data_source = m_old_data_source;
        g_header_type[PAST_HEADER] = m_old_past_header;
        g_tc_COLS = m_old_tc_cols;
        g_word_wrap_offset = m_old_word_wrap_offset;
        utf_init(m_old_input, m_old_output);
        g_charsets = m_old_charsets;
        g_unicode_substitutions = m_old_profile;
        set_char_subst_mode(m_old_subst);

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    void open_article_text(std::string_view text, const char *mode = "r")
    {
        const fs::path article_file = m_output_dir / "article";
        {
            std::ofstream output{article_file, std::ios::binary};
            output << text;
        }

        const std::string article_file_name = article_file.generic_string();
        g_art_fp = std::fopen(article_file_name.c_str(), mode);
        ASSERT_NE(nullptr, g_art_fp);
        g_raw_art_size = ArticlePosition{static_cast<long>(text.size())};
        g_art_size = g_raw_art_size;
        clear_art_buf();
    }

    void reset_mime_section()
    {
        while (g_mime_section != nullptr && g_mime_section != m_old_mime_section && g_mime_section != &m_mime_section)
        {
            MimeSection *previous = g_mime_section->m_prev;
            g_mime_section->mime_clear_struct();
            delete g_mime_section;
            g_mime_section = previous;
        }
        m_parent_mime_section.mime_clear_struct();
        m_mime_section.mime_clear_struct();
    }

    DataSource      m_data_source{};
    MimeSection     m_parent_mime_section{};
    MimeSection     m_mime_section{};
    fs::path        m_output_dir;
    std::FILE      *m_old_art_fp{};
    ArticlePosition m_old_raw_art_size{};
    ArticlePosition m_old_art_size{};
    bool            m_old_do_hiding{};
    bool            m_old_is_mime{};
    MimeState       m_old_mime_state{};
    MimeSection    *m_old_mime_section{};
    std::string     m_old_multipart_separator;
    DataSource     *m_old_data_source{};
    HeaderType      m_old_past_header{};
    int             m_old_tc_cols{};
    int             m_old_word_wrap_offset{};
    std::string_view m_old_input;
    std::string_view m_old_output;
    char             m_old_subst{};
    std::string                m_old_charsets;
    UnicodeSubstitutionProfile m_old_profile;
};

constexpr std::string_view ARTICLE_VIEW_HEADER{"Subject: example\n\n"};
const long                 ARTICLE_VIEW_HEADER_SIZE = static_cast<long>(ARTICLE_VIEW_HEADER.size());

struct ArticleBufferViewCase
{
    std::string_view name;
    bool             buffered_body;
    long             position;
    std::string_view expected;
};

const ArticleBufferViewCase ARTICLE_BUFFER_VIEW_CASES[]{
    {"HeaderStart", false, 0, ARTICLE_VIEW_HEADER},
    {"HeaderValue", false, 9, "example\n\n"},
    {"HeaderLastByte", false, ARTICLE_VIEW_HEADER_SIZE - 1, "\n"},
    {"BodyStart", true, ARTICLE_VIEW_HEADER_SIZE, "alpha\nbeta\n"},
    {"BodySecondLine", true, ARTICLE_VIEW_HEADER_SIZE + 6, "beta\n"},
    {"BodyEnd", true, ARTICLE_VIEW_HEADER_SIZE + 11, {}},
    {"AfterBodyEnd", true, ARTICLE_VIEW_HEADER_SIZE + 12, {}},
    {"BeforeArticle", true, -1, {}}};

void PrintTo(const ArticleBufferViewCase &value, std::ostream *output)
{
    *output << value.name;
}

class ArticleBufferViewTest : public ArticleIoTest, public testing::WithParamInterface<ArticleBufferViewCase>
{
protected:
    void SetUp() override;
    void TearDown() override;

private:
    std::string m_old_head_buf{g_head_buf};
};

void ArticleBufferViewTest::SetUp()
{
    ArticleIoTest::SetUp();
    g_head_buf = std::string{ARTICLE_VIEW_HEADER};
    const ArticlePosition body_start{ARTICLE_VIEW_HEADER_SIZE};
    g_header_type[PAST_HEADER].min_pos = body_start;
    if (GetParam().buffered_body)
    {
        ASSERT_NO_FATAL_FAILURE(open_article_text(g_head_buf + "alpha\nbeta\n"));
        ASSERT_EQ(0, seek_art(body_start));
        set_art_buf_seek(body_start);
        std::string line;
        ASSERT_TRUE(read_art_buf(line, false));
        ASSERT_TRUE(read_art_buf(line, false));
    }
}

void ArticleBufferViewTest::TearDown()
{
    ArticleIoTest::TearDown();
    g_head_buf = m_old_head_buf;
}

class CachedLatinArticleTest : public ArticleIoTest
{
protected:
    void SetUp() override;
};

void CachedLatinArticleTest::SetUp()
{
    ArticleIoTest::SetUp();
    m_mime_section.mime_parse_type("text/plain; charset=ISO-8859-1");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    ASSERT_NO_FATAL_FAILURE(open_article_text("\xE9\n"));
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ("\xC3\xA9\n", line);
    utf_init(CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_ASCII);
    ASSERT_EQ(0, seek_art_buf(ArticlePosition{}));
}

class CharsetIsolationArticleTest : public ArticleIoTest, public testing::WithParamInterface<std::string_view>
{
protected:
    void SetUp() override;
};

void CharsetIsolationArticleTest::SetUp()
{
    ArticleIoTest::SetUp();
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    m_mime_section.mime_parse_type("text/plain; charset=Latin1");
    ASSERT_NO_FATAL_FAILURE(open_article_text("\xE9\n"));
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ("\xC3\xA9\n", line);
    std::fclose(g_art_fp);
    g_art_fp = nullptr;
    m_mime_section.mime_parse_type(GetParam());
    ASSERT_NO_FATAL_FAILURE(open_article_text("\xE9\n"));
}

struct UnicodeProfileArticleCase
{
    std::string_view name;
    std::string_view type;
    std::string_view header;
    std::string_view body;
    std::string_view expected;
};

constexpr UnicodeProfileArticleCase UNICODE_PROFILE_ARTICLE_CASES[]{
    {"Utf8", "text/plain; charset=UTF-8",
     "=?UTF-8?Q?=F0=9F=98=8A=E2=80=AF=E2=80=94?=", "=F0=9F=98=8A=E2=80=AF=E2=80=94\n", ":) ---\n"},
    {"Windows1252", "text/plain; charset=windows-1252", "=?windows-1252?Q?=91a=92_=93b=94_=96_=97_=85_=A0x?=",
     "=91a=92 =93b=94 =96 =97 =85 =A0x\n", "`a' \"b\" -- --- ...  x\n"}};

void PrintTo(const UnicodeProfileArticleCase &value, std::ostream *output)
{
    *output << value.name;
}

class UnicodeProfileArticleTest : public ArticleIoTest, public testing::WithParamInterface<UnicodeProfileArticleCase>
{
protected:
    void SetUp() override;

    std::string m_presentation;
};

void UnicodeProfileArticleTest::SetUp()
{
    ArticleIoTest::SetUp();
    g_charsets = "up";
    reset_char_subst_mode();
    g_word_wrap_offset = -1;
    g_mime_section = &m_mime_section;
    g_is_mime = true;
    m_mime_section.mime_parse_type(GetParam().type);
    m_mime_section.m_encoding = MENCODE_QPRINT;
    g_mime_state = TEXT_MIME;
    ASSERT_NO_FATAL_FAILURE(open_article_text(GetParam().body));
    ASSERT_TRUE(read_art_buf(m_presentation, false));
    ASSERT_EQ(GetParam().expected, m_presentation);
    std::string line;
    ASSERT_FALSE(read_art_buf(line, false));
    ASSERT_EQ(0, seek_art_buf(ArticlePosition{}));
}

struct UnicodeReplacementArticleCase
{
    std::string_view         name;
    std::string_view         body;
    std::string_view         header;
    std::vector<std::string> expected_lines;
};

const UnicodeReplacementArticleCase UNICODE_REPLACEMENT_ARTICLE_CASES[]{
    {"EscapedReplacement",
     "A\xE2\x80\x99"
     "B\nNext\n",
     "=?UTF-8?Q?A=E2=80=99B?=",
     {"Aleft\t\n", "right\\\"B\n", "Next\n"}},
    {"Deletion",
     "A\xE2\x80\x94"
     "B\nNext\n",
     "=?UTF-8?Q?A=E2=80=94B?=",
     {"AB\n", "Next\n"}},
    {"NonrecursiveReplacement",
     "A\xF0\x9F\x98\x8A"
     "B\nNext\n",
     "=?UTF-8?Q?A=F0=9F=98=8AB?=",
     {"A\xE2\x80\x94"
      "B\n",
      "Next\n"}},
    {"CombinedReplacements",
     "A\xE2\x80\x99"
     "B\xE2\x80\x94"
     "C\xF0\x9F\x98\x8A"
     "D\nNext\n",
     "=?UTF-8?Q?A=E2=80=99B=E2=80=94C=F0=9F=98=8AD?=",
     {"Aleft\t\n",
      "right\\\"BC\xE2\x80\x94"
      "D\n",
      "Next\n"}}};

void PrintTo(const UnicodeReplacementArticleCase &value, std::ostream *output)
{
    *output << value.name;
}

class UnicodeReplacementArticleTest : public ArticleIoTest,
                                      public testing::WithParamInterface<UnicodeReplacementArticleCase>
{
protected:
    void                     SetUp() override;
    std::vector<std::string> read_presentation();

    std::string m_presentation;
};

void UnicodeReplacementArticleTest::SetUp()
{
    ArticleIoTest::SetUp();
    g_charsets = "up";
    reset_char_subst_mode();
    ASSERT_TRUE(g_unicode_substitutions.configure(IniSection{
        "unicode substitutions", {}, "U+2019 = \"left\\t\\nright\\\\\\\"\"\nU+2014 = \"\"\nU+1F60A = \xE2\x80\x94\n"}));
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    g_word_wrap_offset = -1;
    ASSERT_NO_FATAL_FAILURE(open_article_text(GetParam().body));
    const std::vector<std::string> lines = read_presentation();
    ASSERT_EQ(GetParam().expected_lines, lines);
    for (const std::string &line : lines)
    {
        m_presentation += line;
    }
    ASSERT_EQ(0, seek_art_buf(ArticlePosition{}));
}

std::vector<std::string> UnicodeReplacementArticleTest::read_presentation()
{
    std::vector<std::string> lines;
    std::string              line;
    while (read_art_buf(line, false))
    {
        lines.push_back(line);
    }
    return lines;
}

std::string repeat_article_text(std::string_view text, int count)
{
    std::string result;
    for (int i = 0; i < count; ++i)
    {
        result += text;
    }
    return result;
}

struct LongArticleCase
{
    const char      *name;
    const char      *type;
    MimeEncoding     encoding;
    std::string_view prefix;
    std::string_view wire_fragment;
    std::string_view text_fragment;
    int              repetitions;
    std::string_view terminator;
    std::string_view following_wire;
    bool             following_cached;
};

const LongArticleCase LONG_ARTICLE_CASES[]{
    {"PlainParagraph", "text/plain; charset=UTF-8", MENCODE_NONE, "Hello\n", "A long paragraph of article text. ",
     "A long paragraph of article text. ", 1600, "\n", "Afterword\n", false},
    {"SoftLineBreaks", "text/plain; charset=UTF-8", MENCODE_QPRINT, "Hello\n",
     "Article=20text=20continues=20here.=20=\n", "Article text continues here. ", 1600, "\n", "Afterword\n", false},
    {"Base64Paragraph", "text/plain; charset=UTF-8", MENCODE_BASE64, "SGVsbG8K\n",
     "VXNlbmV0IGFydGljbGUuVXNlbmV0IGFydGljbGUuVXNlbmV0IGFydGljbGUu\n", "Usenet article.Usenet article.Usenet article.",
     1024, "CkFmdGVyd29yZAo=\n", "", true},
    {"ExpandingCharset", "text/plain; charset=ISO-8859-1", MENCODE_NONE, "Hello\n", "caf\xE9 ", "caf\xC3\xA9 ", 6000,
     "\n", "Afterword\n", false}};

void PrintTo(const LongArticleCase &value, std::ostream *output)
{
    *output << value.name;
}

class LongArticleBufferTest : public ArticleIoTest, public testing::WithParamInterface<LongArticleCase>
{
protected:
    void SetUp() override;
    void cache_body();

    std::string                       m_expected;
    ArticlePosition                   m_long_position;
    ArticlePosition                   m_following_position;
    ArticlePosition                   m_raw_after_long;
    static constexpr std::string_view FOLLOWING{"Afterword\n"};
};

void LongArticleBufferTest::SetUp()
{
    ArticleIoTest::SetUp();
    const LongArticleCase &value = GetParam();
    g_charsets = "p";
    reset_char_subst_mode();
    utf_init("UTF-8", "UTF-8");
    g_word_wrap_offset = -1;
    m_mime_section.mime_parse_type(value.type);
    m_mime_section.m_encoding = value.encoding;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    const std::string wire =
        repeat_article_text(value.wire_fragment, value.repetitions) + std::string(value.terminator);
    m_expected = repeat_article_text(value.text_fragment, value.repetitions) + '\n';
    const ArticlePosition body_start{ARTICLE_VIEW_HEADER_SIZE};
    g_header_type[PAST_HEADER].min_pos = body_start;
    ASSERT_NO_FATAL_FAILURE(open_article_text(
        std::string(ARTICLE_VIEW_HEADER) + std::string(value.prefix) + wire + std::string(value.following_wire), "rb"));
    ASSERT_EQ(0, seek_art(body_start));
    set_art_buf_seek(body_start);
    std::string prefix;
    ASSERT_TRUE(read_art_buf(prefix, false));
    ASSERT_EQ("Hello\n", prefix);
    m_long_position = art_buf_article_pos();
    m_following_position = m_long_position + ArticlePosition{static_cast<long>(m_expected.size())};
    m_raw_after_long = body_start + ArticlePosition{static_cast<long>(value.prefix.size() + wire.size())};
}

void LongArticleBufferTest::cache_body()
{
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ(m_expected, line);
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ(FOLLOWING, line);
}

TEST_P(LongArticleBufferTest, theRawReaderRetainsTheCompleteTextAcrossBufferGrowth)
{
    const char *const text = read_art_buf_raw(false);

    ASSERT_NE(nullptr, text);
    EXPECT_EQ(m_expected + (GetParam().following_cached ? std::string(FOLLOWING) : ""), text);
    EXPECT_EQ(m_following_position, art_buf_article_pos());
    EXPECT_EQ(m_raw_after_long, art_buf_seek());
}

TEST_P(LongArticleBufferTest, theLineReaderReturnsOneCompleteLogicalLineAcrossBufferGrowth)
{
    std::string line;

    const bool read = read_art_buf(line, false);

    EXPECT_TRUE(read);
    EXPECT_EQ(m_expected, line);
    EXPECT_EQ(m_following_position, art_buf_article_pos());
    EXPECT_EQ(m_raw_after_long, art_buf_seek());
    EXPECT_EQ(m_following_position + ArticlePosition{static_cast<long>(FOLLOWING.size())}, g_art_size);
}

TEST_P(LongArticleBufferTest, readingAfterTheLongLineReturnsTheFollowingLine)
{
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_EQ(m_expected, line);

    const bool read = read_art_buf(line, false);

    EXPECT_TRUE(read);
    EXPECT_EQ(FOLLOWING, line);
    EXPECT_EQ(g_art_size, art_buf_article_pos());
    EXPECT_TRUE(art_buf_at_end());
}

TEST_P(LongArticleBufferTest, aForwardSeekAcrossTheUncachedLongLineFindsTheFollowingLine)
{
    std::string line;

    const int  result = seek_art_buf(m_following_position);
    const bool read = read_art_buf(line, false);

    EXPECT_EQ(0, result);
    EXPECT_TRUE(read);
    EXPECT_EQ(FOLLOWING, line);
    EXPECT_EQ(g_art_size, art_buf_article_pos());
}

TEST_P(LongArticleBufferTest, aSavedLongLinePositionRemainsValidAfterTheBufferGrows)
{
    ASSERT_NO_FATAL_FAILURE(cache_body());
    const ArticlePosition source_position = tell_art();
    std::string           line;

    const int  result = seek_art_buf(m_long_position);
    const bool read = read_art_buf(line, false);

    EXPECT_EQ(0, result);
    EXPECT_TRUE(read);
    EXPECT_EQ(m_expected, line);
    EXPECT_EQ(m_following_position, art_buf_article_pos());
    EXPECT_EQ(source_position, tell_art());
}

TEST_P(LongArticleBufferTest, aPositionSavedBeforeBufferGrowthStillReadsTheCachedPrefix)
{
    ASSERT_NO_FATAL_FAILURE(cache_body());
    const ArticlePosition source_position = tell_art();
    std::string           line;

    const int  result = seek_art_buf(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE});
    const bool read = read_art_buf(line, false);

    EXPECT_EQ(0, result);
    EXPECT_TRUE(read);
    EXPECT_EQ("Hello\n", line);
    EXPECT_EQ(m_long_position, art_buf_article_pos());
    EXPECT_EQ(source_position, tell_art());
}

INSTANTIATE_TEST_SUITE_P(PlainAndDecodedText, LongArticleBufferTest, testing::ValuesIn(LONG_ARTICLE_CASES));

struct WrappedArticleCase
{
    const char              *name;
    std::string_view         body;
    int                      margin;
    std::vector<std::string> rows;
    const char              *type{"text/plain; charset=UTF-8"};
    MimeEncoding             encoding{MENCODE_NONE};
    bool                     substitute{};
};

void PrintTo(const WrappedArticleCase &value, std::ostream *output)
{
    *output << value.name;
}

class WrappedArticleTest : public ArticleIoTest, public testing::WithParamInterface<WrappedArticleCase>
{
protected:
    void                     SetUp() override;
    void                     TearDown() override;
    std::vector<std::string> read_rows();
    std::string              presentation() const;

private:
    char m_old_wrapped_nl{g_wrapped_nl};
};

void WrappedArticleTest::SetUp()
{
    ArticleIoTest::SetUp();
    const WrappedArticleCase &value = GetParam();
    g_charsets = value.substitute ? "up" : "p";
    reset_char_subst_mode();
    utf_init("UTF-8", "UTF-8");
    if (value.substitute)
    {
        ASSERT_TRUE(g_unicode_substitutions.configure(
            IniSection{"unicode substitutions", {}, "U+2019 = \xE2\x80\x94\nU+2014 = --\n"}));
    }
    g_word_wrap_offset = value.margin;
    g_wrapped_nl = WRAPPED_NL;
    g_mime_section = &m_mime_section;
    m_mime_section.mime_parse_type(value.type);
    m_mime_section.m_encoding = value.encoding;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    g_header_type[PAST_HEADER].min_pos = ArticlePosition{ARTICLE_VIEW_HEADER_SIZE};
    ASSERT_NO_FATAL_FAILURE(open_article_text(std::string(ARTICLE_VIEW_HEADER) + std::string(value.body), "rb"));
    ASSERT_EQ(0, seek_art(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE}));
    set_art_buf_seek(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE});
}

void WrappedArticleTest::TearDown()
{
    g_wrapped_nl = m_old_wrapped_nl;
    ArticleIoTest::TearDown();
}

std::vector<std::string> WrappedArticleTest::read_rows()
{
    std::vector<std::string> rows;
    std::string              line;
    while (read_art_buf(line, false))
    {
        rows.push_back(line);
    }
    return rows;
}

std::string WrappedArticleTest::presentation() const
{
    std::string result;
    for (const std::string &row : GetParam().rows)
    {
        result += row;
    }
    return result;
}

const WrappedArticleCase WRAPPED_ARTICLE_CASES[]{
    {"ShortLine", "alpha beta gamma\n", 8, {"alpha beta gamma\n"}},
    {"ExactWidth", "123456789012345678901234567890\n", 8, {"123456789012345678901234567890\n"}},
    {"BeforeMargin",
     "alpha beta gamma delta epsilon zeta eta\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta\n"}},
    {"AfterMargin", "abcdefghijklmnopqrstuvwxy tail words\n", 8, {"abcdefghijklmnopqrstuvwxy\003", "tail words\n"}},
    {"UnbrokenWord", "1234567890123456789012345678901234567890\n", 8, {"1234567890123456789012345678901234567890\n"}},
    {"OverwideWordAndText",
     "1234567890123456789012345678901234567890 alpha beta gamma delta epsilon zeta eta\n",
     8,
     {"1234567890123456789012345678901234567890 alpha beta\003", "gamma delta epsilon\003", "zeta eta\n"}},
    {"LeadingIndentation",
     "  alpha beta gamma delta epsilon zeta\n",
     8,
     {"  alpha beta gamma\003", "delta epsilon zeta\n"}},
    {"ContinuationSpaces",
     "alpha beta gamma delta     epsilon zeta eta\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta\n"}},
    {"ContinuationTabs",
     "alpha beta gamma delta \t \tepsilon zeta eta\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta\n"}},
    {"SeveralWraps",
     "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta theta\003", "iota kappa lambda mu\n"}},
    {"SeveralIndentedWraps",
     "alpha beta gamma delta     epsilon zeta eta theta    iota kappa lambda mu\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta theta\003", "iota kappa lambda mu\n"}},
    {"NoMargin", "alpha beta gamma delta epsilon zeta eta\n", 0, {"alpha beta gamma delta epsilon\003", "zeta eta\n"}},
    {"Disabled", "alpha beta gamma delta epsilon zeta eta\n", -1, {"alpha beta gamma delta epsilon zeta eta\n"}}};

TEST_P(WrappedArticleTest, preservesTheExpectedTextAndRowBoundaries)
{
    const std::vector<std::string> rows = read_rows();

    EXPECT_EQ(GetParam().rows, rows);
    EXPECT_EQ(presentation(), art_buf_view(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE}));
    EXPECT_EQ(g_art_size, art_buf_article_pos());
    EXPECT_EQ(static_cast<long>(presentation().size()), art_buf_len().value_of());
}

INSTANTIATE_TEST_SUITE_P(WordBoundariesAndIndentation, WrappedArticleTest, testing::ValuesIn(WRAPPED_ARTICLE_CASES));

const WrappedArticleCase WRAPPED_CACHE_CASES[]{
    {"OverwideWord",
     "1234567890123456789012345678901234567890 alpha beta gamma delta epsilon zeta eta\nNext paragraph\n",
     8,
     {"1234567890123456789012345678901234567890 alpha beta\003", "gamma delta epsilon\003", "zeta eta\n",
      "Next paragraph\n"}},
    {"Plain",
     "alpha beta gamma delta     epsilon zeta eta theta    iota kappa lambda mu\nNext paragraph\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta theta\003", "iota kappa lambda mu\n", "Next paragraph\n"}},
    {"QuotedPrintable",
     "alpha beta gamma delta =3D20 literal after wrap\nNext paragraph\n",
     8,
     {"alpha beta gamma delta\003", "=20 literal after wrap\n", "Next paragraph\n"},
     "text/plain; charset=UTF-8",
     MENCODE_QPRINT},
    {"Base64",
     "YWxwaGEgYmV0YSBnYW1tYSBkZWx0YSAgICAgZXBzaWxvbiB6ZXRhIGV0YSB0aGV0YSAgICBpb3RhIGthcHBhIGxhbWJkYSBtdQpOZXh0IHBhcmFnc"
     "mFwaAo=\n",
     8,
     {"alpha beta gamma delta\003", "epsilon zeta eta theta\003", "iota kappa lambda mu\n", "Next paragraph\n"},
     "text/plain; charset=UTF-8",
     MENCODE_BASE64},
    {"ConvertedCharset",
     "alpha beta gamma delta caf\xE9 continuation\nNext paragraph\n",
     8,
     {"alpha beta gamma delta\003", "caf\xC3\xA9 continuation\n", "Next paragraph\n"},
     "text/plain; charset=ISO-8859-1"},
    {"Substituted",
     "alpha beta gamma delta \xE2\x80\x99 quoted after wrap\nNext paragraph\n",
     8,
     {"alpha beta gamma delta\003", "\xE2\x80\x94 quoted after wrap\n", "Next paragraph\n"},
     "text/plain; charset=UTF-8",
     MENCODE_NONE,
     true}};

class WrappedArticleCacheTest : public WrappedArticleTest
{
};

TEST_P(WrappedArticleCacheTest, preservesTheFollowingParagraphWhenWrappingConvertedText)
{
    const std::vector<std::string> rows = read_rows();

    EXPECT_EQ(GetParam().rows, rows);
    EXPECT_EQ(presentation(), art_buf_view(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE}));
    EXPECT_EQ(g_art_size, art_buf_article_pos());
}

TEST_P(WrappedArticleCacheTest, aForwardSeekFindsTheWrappedContinuationInUncachedText)
{
    const ArticlePosition continuation{ARTICLE_VIEW_HEADER_SIZE + static_cast<long>(GetParam().rows.front().size())};
    const std::vector<std::string> expected{GetParam().rows.begin() + 1, GetParam().rows.end()};

    const int                      result = seek_art_buf(continuation);
    const std::vector<std::string> rows = read_rows();

    EXPECT_EQ(0, result);
    EXPECT_EQ(expected, rows);
    EXPECT_EQ(g_art_size, art_buf_article_pos());
}

TEST_P(WrappedArticleCacheTest, aCachedContinuationRereadsWithoutRepeatingDecodingOrSubstitution)
{
    ASSERT_EQ(GetParam().rows, read_rows());
    const ArticlePosition source_position = tell_art();
    const ArticlePosition continuation{ARTICLE_VIEW_HEADER_SIZE + static_cast<long>(GetParam().rows.front().size())};
    const std::vector<std::string> expected{GetParam().rows.begin() + 1, GetParam().rows.end()};

    const int                      result = seek_art_buf(continuation);
    const std::vector<std::string> rows = read_rows();

    EXPECT_EQ(0, result);
    EXPECT_EQ(expected, rows);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(presentation(), art_buf_view(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE}));
}

INSTANTIATE_TEST_SUITE_P(PlainDecodedAndSubstituted, WrappedArticleCacheTest, testing::ValuesIn(WRAPPED_CACHE_CASES));

std::string mime_part(std::string_view headers, std::string_view body)
{
    return "--part\n" + std::string(headers) + '\n' + std::string(body);
}

std::string mixed_mime_body(std::string_view headers, std::string_view body)
{
    return "Hidden preamble\n" + mime_part("Content-Type: text/plain\n", "First text\n") + mime_part(headers, body) +
           mime_part("Content-Type: text/plain\n", "Last text\n") + "--part--\n";
}

std::vector<std::string> mixed_mime_lines(std::string_view middle)
{
    return {"Part\n", "First text\n", "Part\n", std::string(middle), "Part\n", "Last text\n", "Part\n"};
}

struct MultipartArticleCase
{
    const char              *name;
    std::string              body;
    std::vector<std::string> lines;
    std::string_view         resume{"Last text\n"};
    const char              *type{"multipart/mixed; boundary=part"};
    int                      columns{100};
};

const MultipartArticleCase MULTIPART_ARTICLE_CASES[]{
    {"MixedText", mixed_mime_body("Content-Type: text/html; charset=UTF-8\n", "<b>HTML text</b>\n"),
     mixed_mime_lines("HTML text \n")},
    {"Image",
     mixed_mime_body("Content-Type: image/png; name=picture.png\nContent-Transfer-Encoding: base64\n",
                     "SElEREVOX0lNQUdFX1BBWUxPQUQ=\n"),
     mixed_mime_lines("[Attachment type=image/png, name=picture.png]\n")},
    {"Audio",
     mixed_mime_body("Content-Type: audio/basic; name=sound.au\nContent-Transfer-Encoding: base64\n",
                     "SElEREVOX0FVRElPX1BBWUxPQUQ=\n"),
     mixed_mime_lines("[Attachment type=audio/basic, name=sound.au]\n")},
    {"Application",
     mixed_mime_body("Content-Type: application/pdf; name=old.pdf\n"
                     "Content-Disposition: attachment; filename=report.pdf\n",
                     "%PDF-HIDDEN\n"),
     mixed_mime_lines("[Attachment type=application/pdf, name=report.pdf]\n")},
    {"UnknownType",
     mixed_mime_body("Content-Type: application/x-trn-test\n", "HIDDEN PAYLOAD\n--part-not-a-boundary\n"),
     mixed_mime_lines("[Attachment type=application/x-trn-test, name=unknown]\n")},
    {"Vcard", mixed_mime_body("Content-Type: text/x-vcard; name=contact.vcf\n", "BEGIN:VCARD\nFN:Hidden\nEND:VCARD\n"),
     mixed_mime_lines("[Attachment type=text/x-vcard, name=contact.vcf]\n")},
    {"InlineImage",
     mixed_mime_body("Content-Type: image/png; name=inline.png\nContent-Disposition: inline\n",
                     "HIDDEN INLINE IMAGE\n"),
     mixed_mime_lines("[Attachment type=image/png, name=inline.png]\n")},
    {"NarrowDescription", mixed_mime_body("Content-Type: application/pdf; name=report.pdf\n", "%PDF-HIDDEN\n"),
     mixed_mime_lines("[Attachment type=application/pdf, n...]\n"), "Last text\n", "multipart/mixed; boundary=part",
     40},
    {"PlainAlternative",
     mime_part("Content-Type: text/plain\n", "Chosen plain\n") +
         mime_part("Content-Type: text/html\n", "<b>HIDDEN HTML ALTERNATIVE</b>\n") +
         mime_part("Content-Type: application/pdf\n", "%PDF-HIDDEN ALTERNATIVE\n") + "--part--\n",
     {"Part\n", "Chosen plain\n", "Part\n", "[Alternative: text/html]\n", "Part\n", "[Alternative: application/pdf]\n",
      "Part\n"},
     "Chosen plain\n",
     "multipart/alternative; boundary=part"},
    {"HtmlAlternative",
     mime_part("Content-Type: text/html\n", "<b>Chosen HTML</b>\n") +
         mime_part("Content-Type: text/plain\n", "HIDDEN PLAIN ALTERNATIVE\n") + "--part--\n",
     {"Part\n", "Chosen HTML \n", "Part\n", "[Alternative: text/plain]\n", "Part\n"},
     "Chosen HTML \n",
     "multipart/alternative; boundary=part"},
    {"DefaultTextType",
     mime_part("", "First text\n") + mime_part("Content-Type: text/plain\n", "Last text\n") + "--part--\n",
     {"Part\n", "First text\n", "Part\n", "Last text\n", "Part\n"}},
    {"UnknownMultipart", mixed_mime_body("Content-Type: text/plain\n", "Middle text\n"),
     mixed_mime_lines("Middle text\n"), "Last text\n", "multipart/x-trn-test; boundary=part"},
    {"ForwardedMessage",
     mixed_mime_body("Content-Type: message/rfc822\n", "From: Forwarded <sender@example.test>\n"
                                                       "Content-Type: text/plain\n\nForwarded text\n"),
     mixed_mime_lines("Forwarded text\n")}};

void PrintTo(const MultipartArticleCase &value, std::ostream *output)
{
    *output << value.name;
}

struct MimePresentation
{
    std::vector<std::string>     lines;
    std::vector<ArticlePosition> positions;
};

class MultipartArticleIoTest : public ArticleIoTest
{
protected:
    void             SetUp() override;
    void             TearDown() override;
    void             open_multipart(const MultipartArticleCase &article);
    MimePresentation read_presentation(bool view_inline = false);

private:
    bool m_old_multimedia{g_multimedia_mime};
};

void MultipartArticleIoTest::SetUp()
{
    ArticleIoTest::SetUp();
    g_charsets = "p";
    reset_char_subst_mode();
    utf_init("UTF-8", "UTF-8");
    g_word_wrap_offset = -1;
    g_multipart_separator = "Part";
    g_multimedia_mime = false;
}

void MultipartArticleIoTest::open_multipart(const MultipartArticleCase &article)
{
    g_tc_COLS = article.columns;
    m_mime_section.mime_parse_type(article.type);
    g_mime_section = &m_mime_section;
    g_mime_state = MULTIPART_MIME;
    g_is_mime = true;
    const ArticlePosition body_start{ARTICLE_VIEW_HEADER_SIZE};
    g_header_type[PAST_HEADER].min_pos = body_start;
    ASSERT_NO_FATAL_FAILURE(open_article_text(std::string(ARTICLE_VIEW_HEADER) + article.body, "rb"));
    ASSERT_EQ(0, seek_art(body_start));
    set_art_buf_seek(body_start);
}

void MultipartArticleIoTest::TearDown()
{
    g_multimedia_mime = m_old_multimedia;
    ArticleIoTest::TearDown();
}

MimePresentation MultipartArticleIoTest::read_presentation(bool view_inline)
{
    MimePresentation result;
    std::string      line;
    ArticlePosition  position = art_buf_article_pos();
    while (read_art_buf(line, view_inline))
    {
        result.lines.push_back(line);
        result.positions.push_back(position);
        position = art_buf_article_pos();
    }
    return result;
}

class MultipartArticleTest : public MultipartArticleIoTest, public testing::WithParamInterface<MultipartArticleCase>
{
protected:
    void SetUp() override;
};

void MultipartArticleTest::SetUp()
{
    MultipartArticleIoTest::SetUp();
    ASSERT_NO_FATAL_FAILURE(open_multipart(GetParam()));
}

TEST_P(MultipartArticleTest, presentsTextDescriptionsAndSeparatorsWithoutExposingHiddenPayloads)
{
    const MimePresentation result = read_presentation();

    EXPECT_EQ(GetParam().lines, result.lines);
    EXPECT_EQ(g_art_size, art_buf_article_pos());
    EXPECT_EQ(&m_mime_section, g_mime_section);
}

TEST_P(MultipartArticleTest, replaysTheCachedPresentationWithoutReparsingMimeParts)
{
    const MimePresentation cached = read_presentation();
    ASSERT_EQ(GetParam().lines, cached.lines);
    const ArticlePosition source_position = tell_art();
    const MimeState       state = g_mime_state;

    const int              seek = seek_art_buf(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE});
    const MimePresentation replayed = read_presentation();

    EXPECT_EQ(0, seek);
    EXPECT_EQ(cached.lines, replayed.lines);
    EXPECT_EQ(cached.positions, replayed.positions);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(state, g_mime_state);
    EXPECT_EQ(&m_mime_section, g_mime_section);
}

TEST_P(MultipartArticleTest, aSavedPositionResumesAtTheReadablePartAfterSkippedContent)
{
    const MimePresentation cached = read_presentation();
    ASSERT_EQ(GetParam().lines, cached.lines);
    const auto target = std::find(cached.lines.begin(), cached.lines.end(), GetParam().resume);
    ASSERT_NE(cached.lines.end(), target);
    const std::size_t              index = static_cast<std::size_t>(target - cached.lines.begin());
    const std::vector<std::string> expected{target, cached.lines.end()};
    const ArticlePosition          source_position = tell_art();

    const int              seek = seek_art_buf(cached.positions[index]);
    const MimePresentation resumed = read_presentation();

    EXPECT_EQ(0, seek);
    EXPECT_EQ(expected, resumed.lines);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(g_art_size, art_buf_article_pos());
}

INSTANTIATE_TEST_SUITE_P(MixedAlternativeAndAttachments, MultipartArticleTest,
                         testing::ValuesIn(MULTIPART_ARTICLE_CASES));

const MultipartArticleCase MULTIPART_ENDING_CASES[]{
    {"NestedThenText",
     mime_part("Content-Type: multipart/mixed; boundary=inner\n",
               "--inner\nContent-Type: text/plain\n\nInner text\n--inner--\n") +
         mime_part("Content-Type: text/plain\n", "Last text\n") + "--part--\nHidden epilogue\n",
     {"Part\n", "Part\n", "Inner text\n", "Part\n", "Part\n", "Last text\n", "Part\n"}},
    {"NestedAtEnd",
     mime_part("Content-Type: text/plain\n", "First text\n") +
         mime_part("Content-Type: multipart/mixed; boundary=inner\n",
                   "--inner\nContent-Type: text/plain\n\nInner text\n--inner--\n") +
         "--part--\n",
     {"Part\n", "First text\n", "Part\n", "Part\n", "Inner text\n", "Part\n", "Part\n"}},
    {"DeeplyNested",
     mime_part("Content-Type: multipart/mixed; boundary=inner\n",
               "--inner\nContent-Type: multipart/mixed; boundary=deep\n\n"
               "--deep\nContent-Type: text/plain\n\nDeep text\n--deep--\n--inner--\n") +
         "--part--\n",
     {"Part\n", "Part\n", "Part\n", "Deep text\n", "Part\n", "Part\n", "Part\n"}},
    {"TrailingApplication",
     mime_part("Content-Type: text/plain\n", "First text\n") +
         mime_part("Content-Type: application/pdf; name=report.pdf\n", "%PDF-HIDDEN\n") + "--part--\n",
     {"Part\n", "First text\n", "Part\n", "[Attachment type=application/pdf, name=report.pdf]\n", "Part\n"}},
    {"OnlyImage",
     mime_part("Content-Type: image/png; name=picture.png\n", "HIDDEN IMAGE\n") + "--part--\n",
     {"Part\n", "[Attachment type=image/png, name=picture.png]\n", "Part\n"}},
    {"Base64Tail",
     mime_part("Content-Type: text/plain\nContent-Transfer-Encoding: base64\n", "SGVsbG8=\n") + "--part--\n",
     {"Part\n", "Hello\n", "Part\n"}},
    {"Base64TailClosingAtEof",
     mime_part("Content-Type: text/plain\nContent-Transfer-Encoding: base64\n", "SGVsbG8=\n") + "--part--",
     {"Part\n", "Hello\n", "Part\n"}},
    {"Base64TailWithoutEnding",
     mime_part("Content-Type: text/plain\nContent-Transfer-Encoding: base64\n", "SGVsbG8="),
     {"Part\n", "Hello\n"}},
    {"TrailingAttachmentWithoutEnding",
     mime_part("Content-Type: text/plain\n", "First text\n") +
         mime_part("Content-Type: application/pdf; name=report.pdf\n", "%PDF-HIDDEN"),
     {"Part\n", "First text\n", "Part\n", "[Attachment type=application/pdf, name=report.pdf]\n"}},
    {"EmptyText", mime_part("Content-Type: text/plain\n", "") + "--part--\n", {"Part\n", "Part\n"}},
    {"ClosingBoundaryAtEof",
     mime_part("Content-Type: text/plain\n", "Last text\n") + "--part--",
     {"Part\n", "Last text\n", "Part\n"}},
    {"MissingOuterEnding", mime_part("Content-Type: text/plain\n", "Last text"), {"Part\n", "Last text\n"}},
    {"HtmlTailAtEof", mime_part("Content-Type: text/html\n", "<b>Last text</b>"), {"Part\n", "Last text \n"}},
    {"MissingInnerEndingBeforeSibling",
     mime_part("Content-Type: multipart/mixed; boundary=inner\n", "--inner\nContent-Type: text/plain\n\nInner text\n") +
         mime_part("Content-Type: text/plain\n", "Last text\n") + "--part--\n",
     {"Part\n", "Part\n", "Inner text\n", "Part\n", "Last text\n", "Part\n"}},
    {"MissingInnerEndingBeforeOuterEnding",
     mime_part("Content-Type: multipart/mixed; boundary=inner\n", "--inner\nContent-Type: text/plain\n\nInner text\n") +
         "--part--\n",
     {"Part\n", "Part\n", "Inner text\n", "Part\n"}}};

class MultipartEndingArticleTest : public MultipartArticleTest
{
};

TEST_P(MultipartEndingArticleTest, finishesWithTheExpectedTextSeparatorsAndPresentationSize)
{
    ArticlePosition expected_size{ARTICLE_VIEW_HEADER_SIZE};
    for (const std::string &line : GetParam().lines)
    {
        expected_size += ArticlePosition{static_cast<long>(line.size())};
        if (line == "Part\n" || line.front() == '[')
        {
            expected_size += ArticlePosition{1}; // Hidden presentation marker.
        }
    }

    const MimePresentation result = read_presentation();

    EXPECT_EQ(GetParam().lines, result.lines);
    EXPECT_EQ(expected_size, g_art_size);
    EXPECT_EQ(expected_size, art_buf_article_pos());
}

TEST_P(MultipartEndingArticleTest, repeatedEofDoesNotChangeTheCompletedPresentation)
{
    read_presentation();
    const ArticlePosition position = art_buf_article_pos();
    const ArticlePosition size = g_art_size;
    const ArticlePosition source_position = tell_art();
    const MimeState       state = g_mime_state;
    MimeSection *const    section = g_mime_section;
    std::string           first{"stale"};
    std::string           second{"stale"};

    const bool first_read = read_art_buf(first, false);
    const bool second_read = read_art_buf(second, false);

    EXPECT_FALSE(first_read);
    EXPECT_FALSE(second_read);
    EXPECT_TRUE(first.empty());
    EXPECT_TRUE(second.empty());
    EXPECT_EQ(position, art_buf_article_pos());
    EXPECT_EQ(size, g_art_size);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(state, g_mime_state);
    EXPECT_EQ(section, g_mime_section);
}

TEST_P(MultipartEndingArticleTest, replaysFinalTextAndSeparatorsFromTheCompletedCache)
{
    const MimePresentation cached = read_presentation();
    ASSERT_EQ(GetParam().lines, cached.lines);
    const ArticlePosition source_position = tell_art();
    const MimeState       state = g_mime_state;
    MimeSection *const    section = g_mime_section;

    const int              seek = seek_art_buf(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE});
    const MimePresentation replayed = read_presentation();

    EXPECT_EQ(0, seek);
    EXPECT_EQ(cached.lines, replayed.lines);
    EXPECT_EQ(cached.positions, replayed.positions);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(state, g_mime_state);
    EXPECT_EQ(section, g_mime_section);
}

INSTANTIATE_TEST_SUITE_P(NestingAttachmentsAndEof, MultipartEndingArticleTest,
                         testing::ValuesIn(MULTIPART_ENDING_CASES));

const MultipartArticleCase PIPELINE_MULTIPART_CASES[]{
    {"LegacyHtmlQuotedPrintable",
     mixed_mime_body("Content-Type: text/html; charset=windows-1252\nContent-Transfer-Encoding: quoted-printable\n",
                     "<b>Caf=E9 &amp; A=92B</b>\n"),
     {"Part\n", "First text\n", "Part\n", "Caf\xC3\xA9 & Aleft\n", "rightB \n", "Part\n", "Last text\n", "Part\n"},
     "rightB \n"},
    {"Utf8HtmlSoftLine",
     mixed_mime_body("Content-Type: text/html; charset=UTF-8\nContent-Transfer-Encoding: quoted-printable\n",
                     "<b>=\nA=E2=80=99B</b>\n"),
     {"Part\n", "First text\n", "Part\n", "Aleft\n", "rightB \n", "Part\n", "Last text\n", "Part\n"},
     "rightB \n"},
    {"Base64Tail",
     mixed_mime_body("Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: base64\n", "QeKAmUI=\n"),
     {"Part\n", "First text\n", "Part\n", "Aleft\n", "rightB\n", "Part\n", "Last text\n", "Part\n"},
     "rightB\n"},
    {"Base64Rows",
     mixed_mime_body("Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: base64\n",
                     "QeKAmUIKQ+KAlEQKRfCfmIpGCg==\n"),
     {"Part\n", "First text\n", "Part\n", "Aleft\n", "rightB\n", "CD\n",
      "E\xE2\x80\x94"
      "F\n",
      "Part\n", "Last text\n", "Part\n"},
     "rightB\n"},
    {"Base64HtmlRows",
     mixed_mime_body("Content-Type: text/html; charset=UTF-8\nContent-Transfer-Encoding: base64\n",
                     "PGI+Rmlyc3QKQeKAmUI8L2I+Cg==\n"),
     {"Part\n", "First text\n", "Part\n", "First Aleft\n", "rightB \n", "Part\n", "Last text\n", "Part\n"},
     "rightB \n"},
    {"SplitScalarAfterCachedRow",
     mixed_mime_body("Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: base64\n",
                     "cHJlZml4CkHi\ngJlCCg==\n"),
     {"Part\n", "First text\n", "Part\n", "prefix\n", "Aleft\n", "rightB\n", "Part\n", "Last text\n", "Part\n"},
     "rightB\n"},
    {"ChangingPartCharsets",
     mime_part("Content-Type: text/plain; charset=windows-1252\nContent-Transfer-Encoding: quoted-printable\n",
               "A=92B\n") +
         mime_part("Content-Type: text/plain; charset=ISO-8859-15\nContent-Transfer-Encoding: quoted-printable\n",
                   "=A4 caf=E9\n") +
         mime_part("Content-Type: text/html; charset=UTF-8\n", "<i>Tail \xF0\x9F\x98\x8A</i>\n") + "--part--\n",
     {"Part\n", "Aleft\n", "rightB\n", "Part\n", "\xE2\x82\xAC caf\xC3\xA9\n", "Part\n", "Tail \xE2\x80\x94 \n",
      "Part\n"},
     "rightB\n"},
    {"IncompleteScalarBeforeLegacyPart",
     mime_part("Content-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: base64\n", "b2sKQeI=\n") +
         mime_part("Content-Type: text/plain; charset=Latin1\nContent-Transfer-Encoding: quoted-printable\n",
                   "caf=E9\n") +
         "--part--\n",
     {"Part\n", "ok\n", "A?\n", "Part\n", "caf\xC3\xA9\n", "Part\n"},
     "caf\xC3\xA9\n"},
    {"MissingCharsetAfterLegacyPart",
     mime_part("Content-Type: text/plain; charset=Latin1\nContent-Transfer-Encoding: quoted-printable\n", "caf=E9\n") +
         mime_part("Content-Type: text/plain\n", "\xE9\n") +
         mime_part("Content-Type: text/plain; charset=UTF-8\n", "caf\xC3\xA9\n") + "--part--\n",
     {"Part\n", "caf\xC3\xA9\n", "Part\n", "?\n", "Part\n", "caf\xC3\xA9\n", "Part\n"},
     "?\n"},
    {"HtmlEntityAcrossSoftLine",
     mixed_mime_body("Content-Type: text/html; charset=UTF-8\nContent-Transfer-Encoding: quoted-printable\n",
                     "<b>A&#82=\n17;B &#x1f60a; end</b>\n"),
     {"Part\n", "First text\n", "Part\n", "Aleft\n", "rightB \xE2\x80\x94 end \n", "Part\n", "Last text\n", "Part\n"},
     "rightB \xE2\x80\x94 end \n"}};

class PipelineMultipartArticleTest : public MultipartArticleIoTest,
                                     public testing::WithParamInterface<MultipartArticleCase>
{
protected:
    void SetUp() override;

    std::vector<ArticlePosition> m_positions;
    std::vector<ArticlePosition> m_tail_positions;
    std::vector<std::string>     m_tail;
    ArticlePosition              m_resume_position{};
    ArticlePosition              m_expected_size{};
};

void PipelineMultipartArticleTest::SetUp()
{
    MultipartArticleIoTest::SetUp();
    g_charsets = "up";
    reset_char_subst_mode();
    ASSERT_TRUE(g_unicode_substitutions.configure(IniSection{
        "unicode substitutions",
        {},
        "U+00E9 = \xC3\xA9\nU+2019 = \"left\\nright\"\nU+2014 = \"\"\nU+1F60A = \xE2\x80\x94\nU+FFFD = ?\n"}));
    utf_init(CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_ASCII);
    ASSERT_NO_FATAL_FAILURE(open_multipart(GetParam()));
    m_expected_size = ArticlePosition{ARTICLE_VIEW_HEADER_SIZE};
    for (const std::string &line : GetParam().lines)
    {
        m_positions.push_back(m_expected_size);
        m_expected_size += ArticlePosition{static_cast<long>(line.size())};
        if (line == "Part\n")
        {
            m_expected_size += ArticlePosition{1};
        }
    }
    const auto target = std::find(GetParam().lines.begin(), GetParam().lines.end(), GetParam().resume);
    ASSERT_NE(GetParam().lines.end(), target);
    const std::size_t index = static_cast<std::size_t>(target - GetParam().lines.begin());
    m_resume_position = m_positions[index];
    m_tail.assign(target, GetParam().lines.end());
    m_tail_positions.assign(m_positions.begin() + index, m_positions.end());
}

TEST_P(PipelineMultipartArticleTest, presentsConvertedAndSubstitutedTextAcrossPartBoundaries)
{
    const MimePresentation result = read_presentation();

    EXPECT_EQ(GetParam().lines, result.lines);
    EXPECT_EQ(m_positions, result.positions);
    EXPECT_EQ(m_expected_size, g_art_size);
    EXPECT_EQ(m_expected_size, art_buf_article_pos());
    EXPECT_EQ(&m_mime_section, g_mime_section);
    EXPECT_EQ(CHARSET_NAME_WINDOWS_1252, input_charset_name());
    EXPECT_EQ(CHARSET_NAME_ASCII, output_charset_name());
}

TEST_P(PipelineMultipartArticleTest, seeksForwardIntoUncachedTransformedText)
{
    const int              seek = seek_art_buf(m_resume_position);
    const MimePresentation result = read_presentation();

    EXPECT_EQ(0, seek);
    EXPECT_EQ(m_tail, result.lines);
    EXPECT_EQ(m_tail_positions, result.positions);
    EXPECT_EQ(m_expected_size, art_buf_article_pos());
}

TEST_P(PipelineMultipartArticleTest, rereadsASavedPositionWithoutRepeatingThePipeline)
{
    const MimePresentation cached = read_presentation();
    ASSERT_EQ(GetParam().lines, cached.lines);
    const ArticlePosition source_position = tell_art();
    const MimeState       state = g_mime_state;

    const int              seek = seek_art_buf(m_resume_position);
    const MimePresentation result = read_presentation();

    EXPECT_EQ(0, seek);
    EXPECT_EQ(m_tail, result.lines);
    EXPECT_EQ(m_tail_positions, result.positions);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(state, g_mime_state);
}

TEST_P(PipelineMultipartArticleTest, retainsTheOriginalEncodedArticleAfterPresentationReads)
{
    read_presentation();
    ASSERT_EQ(0, seek_art(ArticlePosition{}));
    std::string raw;
    std::string line;

    while (read_art(line))
    {
        raw += line;
    }

    EXPECT_EQ(std::string{ARTICLE_VIEW_HEADER} + GetParam().body, raw);
    EXPECT_EQ(ArticlePosition{static_cast<long>(raw.size())}, g_raw_art_size);
}

INSTANTIATE_TEST_SUITE_P(TransferCharsetHtmlAndSubstitution, PipelineMultipartArticleTest,
                         testing::ValuesIn(PIPELINE_MULTIPART_CASES));

enum class InlineHandler
{
    Viewer,
    Missing,
    NeedsTerminal,
    CopiousOutput,
    RejectedProbe,
    Fallback
};

struct InlineMimeCase
{
    const char   *name;
    const char   *encoding;
    std::string   payload;
    std::string   decoded;
    const char   *type{"image/x-trn-inline"};
    InlineHandler handler{InlineHandler::Viewer};
    bool          inline_disposition{true};
    bool          view_inline{true};
    bool          already_viewed{};
    int           viewer_status{};
    bool          decode_succeeds{true};
};

const InlineMimeCase INLINE_MIME_CASES[]{
    {"PaddedBase64", "base64", "SGVsbG8=\n", "Hello"},
    {"UnpaddedBase64", "base64", "SGVsbG8h\n", "Hello!"},
    {"QuotedPrintable", "quoted-printable", "First=20line\nSecond=20line\n", "First line\nSecond line\n"},
    {"Unencoded", "8bit", "First line\nSecond line\n", "First line\nSecond line\n"},
    {"Audio", "base64", "SGVsbG8=\n", "Hello", "audio/x-trn-inline"},
    {"MissingHandler", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::Missing},
    {"TerminalHandler", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::NeedsTerminal},
    {"CopiousHandler", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::CopiousOutput},
    {"RejectedProbe", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::RejectedProbe},
    {"FallbackHandler", "base64", "SGVsbG8=\n", "Hello", "image/x-trn-inline", InlineHandler::Fallback},
    {"AttachmentDisposition", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::Viewer, false},
    {"ViewingDisabled", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::Viewer, true, false},
    {"AlreadyViewed", "base64", "SGVsbG8=\n", "", "image/x-trn-inline", InlineHandler::Viewer, true, true, true},
    {"ViewerFailure", "base64", "SGVsbG8=\n", "Hello", "image/x-trn-inline", InlineHandler::Viewer, true, true, false,
     1},
    {"UnsupportedEncoding", "x-trn-unknown", "Hidden payload\n", "", "image/x-trn-inline", InlineHandler::Viewer, true,
     true, false, 0, false},
    {"TruncatedBase64", "base64", "SG\n", "", "image/x-trn-inline", InlineHandler::Viewer, true, true, false, 0,
     false}};

void PrintTo(const InlineMimeCase &value, std::ostream *output)
{
    *output << value.name;
}

class InlineMultipartArticleTest : public MultipartArticleIoTest, public testing::WithParamInterface<InlineMimeCase>
{
protected:
    void SetUp() override;
    void TearDown() override;
    int  execute_viewer(std::string_view command);
    bool attempts_viewing() const;
    int  expected_viewers() const;

    std::vector<std::string> m_expected;
    std::vector<std::string> m_commands;
    std::vector<std::string> m_decoded;
    fs::path                 m_viewer_directory;
    fs::path                 m_news_directory;
    bool                     m_viewer_no_wait{};
    int                      m_probes{};

private:
    fs::path         m_old_directory{fs::current_path()};
    std::string      m_old_tmp_dir{g_tmp_dir};
    std::string      m_old_login_name{g_login_name};
    std::string      m_old_newsgroup_dir{g_newsgroup_dir};
    std::string      m_old_decode_filename{g_decode_filename};
    std::string      m_old_art_line{g_art_line};
    std::string_view m_old_getc_line{g_mime_getc_line};
    ArticleLine      m_old_first_view{g_first_view};
    ArticleLine      m_old_art_line_num{g_art_line_num};
    int              m_old_term_line{g_term_line};
    int              m_old_term_col{g_term_col};
    int              m_old_fire_is_out{g_fire_is_out};
    bool             m_old_no_wait{g_no_wait_fork};
    bool             m_old_auto_view{g_auto_view_inline};
    std::string_view m_old_cr{g_tc_CR};
    std::string_view m_old_ce{g_tc_CE};
    std::string_view m_old_us{g_tc_US};
    std::string_view m_old_ue{g_tc_UE};
};

void InlineMultipartArticleTest::SetUp()
{
    MultipartArticleIoTest::SetUp();
    g_tmp_dir = m_output_dir.generic_string();
    g_login_name = "inline-reader";
    g_newsgroup_dir = "news";
    m_data_source.m_spool_dir = m_output_dir.generic_string();
    m_news_directory = m_output_dir / g_newsgroup_dir;
    fs::create_directories(m_news_directory);
    fs::current_path(m_news_directory);
    g_first_view = GetParam().already_viewed ? ArticleLine{7} : ArticleLine{};
    g_art_line_num = ArticleLine{7};
    g_term_line = 12;
    g_term_col = 0;
    g_no_wait_fork = false;
    g_auto_view_inline = false;
    g_mime_getc_line = {};
    g_tc_CR = "<CR>";
    g_tc_CE = "<CE>";
    g_tc_US = "<US>";
    g_tc_UE = "<UE>";
    mime_final();
    mime_set_executor([this](std::string_view, std::string_view command) { return execute_viewer(command); });
    const fs::path cap_file = m_output_dir / "mimecap";
    {
        std::ofstream output{cap_file};
        if (GetParam().handler != InlineHandler::Missing)
        {
            output << GetParam().type << "; viewer %s";
            switch (GetParam().handler)
            {
            case InlineHandler::NeedsTerminal:
                output << "; needsterminal";
                break;
            case InlineHandler::CopiousOutput:
                output << "; copiousoutput";
                break;
            case InlineHandler::RejectedProbe:
            case InlineHandler::Fallback:
                output << "; test=probe";
                break;
            default:
                break;
            }
            output << '\n';
            if (GetParam().handler == InlineHandler::Fallback)
            {
                output << GetParam().type << "; fallback %s\n";
            }
        }
    }
    mime_read_mimecap(cap_file.generic_string());
    const std::string disposition = GetParam().inline_disposition ? "inline" : "attachment";
    const std::string headers = std::string{"Content-Type: "} + GetParam().type +
                                "; name=inline.bin\nContent-Disposition: " + disposition +
                                "\nContent-Transfer-Encoding: " + GetParam().encoding + '\n';
    m_expected = mixed_mime_lines(std::string{"[Attachment type="} + GetParam().type + ", name=inline.bin]\n");
    ASSERT_NO_FATAL_FAILURE(open_multipart({"Inline", mixed_mime_body(headers, GetParam().payload), m_expected}));
}

void InlineMultipartArticleTest::TearDown()
{
    mime_final();
    mime_set_executor(do_shell);
    fs::current_path(m_old_directory);
    g_tmp_dir = m_old_tmp_dir;
    g_login_name = m_old_login_name;
    g_newsgroup_dir = m_old_newsgroup_dir;
    g_decode_filename = m_old_decode_filename;
    g_art_line = m_old_art_line;
    g_mime_getc_line = m_old_getc_line;
    g_first_view = m_old_first_view;
    g_art_line_num = m_old_art_line_num;
    g_term_line = m_old_term_line;
    g_term_col = m_old_term_col;
    g_fire_is_out = m_old_fire_is_out;
    g_no_wait_fork = m_old_no_wait;
    g_auto_view_inline = m_old_auto_view;
    g_tc_CR = m_old_cr;
    g_tc_CE = m_old_ce;
    g_tc_US = m_old_us;
    g_tc_UE = m_old_ue;
    MultipartArticleIoTest::TearDown();
}

int InlineMultipartArticleTest::execute_viewer(std::string_view command)
{
    if (command == "probe")
    {
        ++m_probes;
        return 1;
    }
    m_commands.emplace_back(command);
    m_viewer_directory = fs::current_path();
    m_viewer_no_wait = g_no_wait_fork;
    std::ifstream input{g_decode_filename, std::ios::binary};
    m_decoded.emplace_back(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
    g_term_line = 37;
    g_term_col = 11;
    return GetParam().viewer_status;
}

bool InlineMultipartArticleTest::attempts_viewing() const
{
    return GetParam().inline_disposition && GetParam().view_inline && !GetParam().already_viewed;
}

int InlineMultipartArticleTest::expected_viewers() const
{
    return attempts_viewing() && GetParam().decode_succeeds &&
           (GetParam().handler == InlineHandler::Viewer || GetParam().handler == InlineHandler::Fallback);
}

TEST_P(InlineMultipartArticleTest, decodesInlineContentAndContinuesToTheNextTextPart)
{
    const MimePresentation result = read_presentation(GetParam().view_inline);

    EXPECT_EQ(m_expected, result.lines);
    ASSERT_EQ(expected_viewers(), m_commands.size());
    if (expected_viewers())
    {
        EXPECT_EQ(std::vector<std::string>{GetParam().decoded}, m_decoded);
        EXPECT_EQ(GetParam().handler == InlineHandler::Fallback ? "fallback inline.bin" : "viewer inline.bin",
                  m_commands.front());
    }
    EXPECT_EQ(g_art_size, art_buf_article_pos());
}

TEST_P(InlineMultipartArticleTest, restoresReadingStateAndRemovesTheViewedFile)
{
    read_presentation(GetParam().view_inline);

    EXPECT_EQ(m_news_directory, fs::current_path());
    EXPECT_EQ(12, g_term_line);
    EXPECT_EQ(0, g_term_col);
    EXPECT_FALSE(g_no_wait_fork);
    if (expected_viewers())
    {
        EXPECT_TRUE(m_viewer_no_wait);
        EXPECT_NE(m_news_directory, m_viewer_directory);
        EXPECT_FALSE(fs::exists(m_viewer_directory / "inline.bin"));
        EXPECT_FALSE(fs::exists(m_viewer_directory));
    }
}

TEST_P(InlineMultipartArticleTest, cachedRereadingDoesNotRepeatTheViewerOrHandlerProbe)
{
    const MimePresentation         cached = read_presentation(GetParam().view_inline);
    const std::vector<std::string> commands = m_commands;
    const int                      probes = m_probes;
    const ArticlePosition          source_position = tell_art();
    const ArticleLine              first_view = g_first_view;

    const int              seek = seek_art_buf(ArticlePosition{ARTICLE_VIEW_HEADER_SIZE});
    const MimePresentation replayed = read_presentation(true);

    EXPECT_EQ(0, seek);
    EXPECT_EQ(cached.lines, replayed.lines);
    EXPECT_EQ(cached.positions, replayed.positions);
    EXPECT_EQ(commands, m_commands);
    EXPECT_EQ(probes, m_probes);
    EXPECT_EQ(source_position, tell_art());
    EXPECT_EQ(first_view, g_first_view);
}

INSTANTIATE_TEST_SUITE_P(HandlersEncodingAndReadingPolicy, InlineMultipartArticleTest,
                         testing::ValuesIn(INLINE_MIME_CASES));

class MultipleInlinePartsArticleTest : public InlineMultipartArticleTest
{
};

TEST_P(MultipleInlinePartsArticleTest, viewsEachInlinePartOnceAsReadingAdvances)
{
    std::fclose(g_art_fp);
    g_art_fp = nullptr;
    const std::string              headers = "Content-Type: image/x-trn-inline; name=inline.bin\n"
                                             "Content-Disposition: inline\nContent-Transfer-Encoding: base64\n";
    const std::string              body = mime_part("Content-Type: text/plain\n", "First text\n") +
                                          mime_part(headers, "SGVsbG8=\n") + mime_part(headers, "V29ybGQ=\n") +
                                          mime_part("Content-Type: text/plain\n", "Last text\n") + "--part--\n";
    const std::string              description = "[Attachment type=image/x-trn-inline, name=inline.bin]\n";
    const std::vector<std::string> expected{"Part\n",    "First text\n", "Part\n",      description, "Part\n",
                                            description, "Part\n",       "Last text\n", "Part\n"};
    ASSERT_NO_FATAL_FAILURE(open_multipart({"MultipleInline", body, expected}));
    std::vector<std::string> lines;
    std::string              line;

    while (read_art_buf(line, true))
    {
        lines.push_back(line);
        ++g_art_line_num;
    }

    EXPECT_EQ(expected, lines);
    EXPECT_EQ((std::vector<std::string>{"Hello", "World"}), m_decoded);
    EXPECT_EQ((std::vector<std::string>{"viewer inline.bin", "viewer inline.bin"}), m_commands);
    EXPECT_EQ(m_news_directory, fs::current_path());
    EXPECT_EQ(g_art_size, art_buf_article_pos());
}

INSTANTIATE_TEST_SUITE_P(ReadingProgress, MultipleInlinePartsArticleTest, testing::Values(INLINE_MIME_CASES[0]));

} // namespace

TEST_F(ArticleIoTest, wordWrapCompactsIndentedContinuation)
{
    open_article_text("alpha beta gamma delta     epsilon zeta eta\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    std::string expected_first{"alpha beta gamma delta"};
    expected_first += WRAPPED_NL;
    EXPECT_EQ(expected_first, first);
    EXPECT_EQ(23, art_buf_pos().value_of());

    std::string second;
    ASSERT_TRUE(read_art_buf(second, false));

    EXPECT_EQ("epsilon zeta eta\n", second);
}

TEST_F(ArticleIoTest, readArtBufAddsNewlineToFinalLine)
{
    open_article_text("alpha");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ("alpha\n", first);
    EXPECT_EQ(6, art_buf_pos().value_of());
    EXPECT_EQ(6, art_buf_len().value_of());

    EXPECT_FALSE(read_art_buf(first, false));
    EXPECT_TRUE(first.empty());
}

TEST_P(ArticleBufferViewTest, returnsTheExpectedViewAtThePosition)
{
    const std::string_view view = art_buf_view(ArticlePosition{GetParam().position});

    EXPECT_EQ(GetParam().expected, view);
}

INSTANTIATE_TEST_SUITE_P(HeaderAndBodyPosition, ArticleBufferViewTest, testing::ValuesIn(ARTICLE_BUFFER_VIEW_CASES),
                         [](const testing::TestParamInfo<ArticleBufferViewCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(ArticleIoTest, viewExcludesClearedBodyText)
{
    open_article_text("alpha\n");
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    clear_art_buf();

    EXPECT_TRUE(art_buf_view(ArticlePosition{1}).empty());
}

TEST_F(ArticleIoTest, readArtBufWithoutHidingReturnsArticleLine)
{
    g_do_hiding = false;
    open_article_text("alpha\nbeta\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ("alpha\n", first);

    std::string second;
    ASSERT_TRUE(read_art_buf(second, false));

    EXPECT_EQ("beta\n", second);
}

TEST_F(ArticleIoTest, readArtBufWithoutHidingReturnsLongArticleLine)
{
    g_do_hiding = false;
    const std::string line(static_cast<std::size_t>(LINE_BUF_LEN) + 20, 'x');
    open_article_text(line + "\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ(line + "\n", first);
}

TEST_F(ArticleIoTest, readArtBufDecodesBase64MimeText)
{
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("SGVsbG8=\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ("Hello\n", first);
    EXPECT_FALSE(read_art_buf(first, false));
    EXPECT_TRUE(first.empty());
}

TEST_F(ArticleIoTest, readArtBufDecodesQuotedPrintableMimeText)
{
    m_mime_section.m_encoding = MENCODE_QPRINT;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("Hello=20world=21\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ("Hello world!\n", first);
    EXPECT_FALSE(read_art_buf(first, false));
    EXPECT_TRUE(first.empty());
}

TEST_F(ArticleIoTest, readArtBufSuppressesMimeSubHeaderBetweenParts)
{
    m_mime_section.m_prev = &m_parent_mime_section;
    g_mime_section = &m_mime_section;
    g_mime_state = BETWEEN_MIME;
    g_is_mime = true;
    open_article_text("Content-Type: text/html\n\nbody\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ("body \n", first);
    EXPECT_EQ(HTML_TEXT_MIME, g_mime_section->m_type);
    EXPECT_EQ(HTML_TEXT_MIME, g_mime_state);
}

TEST_F(ArticleIoTest, multipartBoundaryOutputsSeparatorLine)
{
    m_mime_section.m_type = MULTIPART_MIME;
    m_mime_section.m_type_name = "multipart/mixed";
    m_mime_section.m_boundary = "part";
    m_mime_section.m_boundary_len = 4;
    g_mime_section = &m_mime_section;
    g_mime_state = MULTIPART_MIME;
    g_is_mime = true;
    g_multipart_separator = "part separator";
    open_article_text("--part\nContent-Type: text/plain\n\nbody\n");

    std::string first;
    ASSERT_TRUE(read_art_buf(first, false));

    EXPECT_EQ("part separator\n", first);
}

TEST_F(CachedLatinArticleTest, rereadsTheCachedPresentationAfterChangingGlobalCharsets)
{
    std::string line;

    const bool read = read_art_buf(line, false);

    ASSERT_TRUE(read);
    EXPECT_EQ("\xC3\xA9\n", line);
}

TEST_F(CachedLatinArticleTest, readsTheOriginalRawBytesAfterCachingThePresentation)
{
    std::string line;
    std::rewind(g_art_fp);

    const bool read = read_art(line);

    ASSERT_TRUE(read);
    EXPECT_EQ("\xE9\n", line);
}

TEST_F(ArticleIoTest, transferDecodesSplitUtf8BeforeCharsetConversion)
{
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_QPRINT;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("=C3=\n=A9\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
}

TEST_F(ArticleIoTest, base64LinesCanSplitAUnicodeScalar)
{
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("ww==\nqQo=\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
}

TEST_F(ArticleIoTest, convertsEveryCachedBase64LineOnce)
{
    m_mime_section.mime_parse_type("text/plain; charset=windows-1252");
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("6QqSCg==\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xE2\x80\x99\n", line);
    EXPECT_FALSE(read_art_buf(line, false));
}

TEST_P(CharsetIsolationArticleTest, doesNotInheritThePreviousArticleCharset)
{
    std::string line;

    const bool read = read_art_buf(line, false);

    ASSERT_TRUE(read);
    EXPECT_EQ("\xEF\xBF\xBD\n", line);
}

INSTANTIATE_TEST_SUITE_P(CharsetDeclaration, CharsetIsolationArticleTest,
                         testing::Values("text/plain; charset=unknown", "text/plain"),
                         [](const testing::TestParamInfo<std::string_view> &info)
                         { return info.param == "text/plain" ? "Missing" : "Unknown"; });

TEST_F(ArticleIoTest, htmlFilteringReceivesUtf8AfterTransferDecoding)
{
    m_mime_section.mime_parse_type("text/html; charset=Latin1");
    m_mime_section.m_encoding = MENCODE_QPRINT;
    g_mime_section = &m_mime_section;
    g_mime_state = HTML_TEXT_MIME;
    g_is_mime = true;
    open_article_text("<b>=E9</b>\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9 \n", line);
}

TEST_F(ArticleIoTest, bodySubstitutionFollowsCharsetConversion)
{
    set_char_subst_mode('a');
    m_mime_section.mime_parse_type("text/plain; charset=Latin1");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("\xC4\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("Ae\n", line);
}

TEST_F(ArticleIoTest, malformedUtf8IsReplacedBeforeCaching)
{
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("\xC0\xAF"
                      "x\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xEF\xBF\xBD\xEF\xBF\xBD"
              "x\n",
              line);
}

TEST_F(ArticleIoTest, parsingContentTypeDoesNotChangeGlobalCharset)
{
    utf_init(CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_ASCII);
    m_mime_section.mime_parse_type("text/plain; charset=Latin1");

    EXPECT_EQ(CHARSET_NAME_WINDOWS_1252, input_charset_name());
    EXPECT_EQ(CHARSET_NAME_ASCII, output_charset_name());
}

TEST_F(ArticleIoTest, base64PartialScalarSurvivesCachedNewline)
{
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("w6kKww==\nqQo=\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
    EXPECT_FALSE(read_art_buf(line, false));
}

TEST_F(ArticleIoTest, unfinishedScalarAtEofIsReplaced)
{
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("b2sKww==\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("ok\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xEF\xBF\xBD\n", line);
    EXPECT_FALSE(read_art_buf(line, false));
}

TEST_F(ArticleIoTest, unfinishedTextIsFlushedBeforeNextMimePart)
{
    m_parent_mime_section.m_boundary = "part";
    m_parent_mime_section.m_boundary_len = 4;
    m_mime_section.m_prev = &m_parent_mime_section;
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    g_multipart_separator = "part separator";
    open_article_text("b2sKww==\n--part\nContent-Type: text/plain; charset=unknown\n\n\xE9\n--part--\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("ok\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xEF\xBF\xBD\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("part separator\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xEF\xBF\xBD\n", line);
}

TEST_F(ArticleIoTest, texSubstitutionDoesNotReprocessCachedPartialLine)
{
    set_char_subst_mode('t');
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("b2sKIic=\nYQo=\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("ok\n", line);
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\"a\n", line);
}

TEST_F(ArticleIoTest, htmlScanningIgnoresLegacyCharsetState)
{
    m_mime_section.mime_parse_type("text/html; charset=UTF-8");
    utf_init(CHARSET_NAME_ISO8859_1, CHARSET_NAME_ASCII);
    g_mime_section = &m_mime_section;
    g_mime_state = HTML_TEXT_MIME;
    g_is_mime = true;
    open_article_text("<b>\xF0\x9F\x98\x9A</b>\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xF0\x9F\x98\x9A \n", line);
}

TEST_F(ArticleIoTest, unencodedTextUsesDeclaredCharsetWithoutMimeVersion)
{
    m_mime_section.mime_parse_type("text/plain; charset=Latin1");
    g_mime_section = &m_mime_section;
    g_mime_state = NOT_MIME;
    g_is_mime = false;
    open_article_text("\xE9\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
}

TEST_F(ArticleIoTest, unicodeExpansionGrowsPresentationCache)
{
    m_mime_section.mime_parse_type("text/plain; charset=Latin1");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    g_word_wrap_offset = -1;
    open_article_text(std::string(5000, '\xE9') + "\n");
    std::string expected;
    for (int i = 0; i < 5000; ++i)
    {
        expected += "\xC3\xA9";
    }
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ(expected + "\n", line);
}

TEST_F(ArticleIoTest, htmlFinalLineIsSubstitutedAfterFiltering)
{
    set_char_subst_mode('a');
    m_mime_section.mime_parse_type("text/html; charset=Latin1");
    g_mime_section = &m_mime_section;
    g_mime_state = HTML_TEXT_MIME;
    g_is_mime = true;
    open_article_text("<b>\xC4</b>\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("Ae \n", line);
}

TEST_P(UnicodeProfileArticleTest, presentsEncodedHeadersAndMimeBodiesIdentically)
{
    const std::string header = str_char_subst(decode_header(GetParam().header), 'u') + "\n";

    EXPECT_EQ(m_presentation, header);
}

TEST_P(UnicodeProfileArticleTest, rereadsTheCachedPresentationWithoutChangingIt)
{
    std::string line;

    const bool read = read_art_buf(line, false);

    ASSERT_TRUE(read);
    EXPECT_EQ(GetParam().expected, line);
}

TEST_P(UnicodeProfileArticleTest, readsTheOriginalRawArticleBytes)
{
    std::string line;
    std::rewind(g_art_fp);

    const bool read = read_art(line);

    ASSERT_TRUE(read);
    EXPECT_EQ(GetParam().body, line);
}

INSTANTIATE_TEST_SUITE_P(SourceCharset, UnicodeProfileArticleTest, testing::ValuesIn(UNICODE_PROFILE_ARTICLE_CASES),
                         [](const testing::TestParamInfo<UnicodeProfileArticleCase> &info)
                         { return std::string{info.param.name}; });

TEST_P(UnicodeReplacementArticleTest, preservesReplacementTextAndLineBoundariesOnCacheReread)
{
    const std::vector<std::string> lines = read_presentation();

    EXPECT_EQ(GetParam().expected_lines, lines);
}

TEST_P(UnicodeReplacementArticleTest, presentsHeaderAndBodyReplacementTextIdentically)
{
    const std::string header = str_char_subst(decode_header(GetParam().header), 'u') + "\nNext\n";

    EXPECT_EQ(m_presentation, header);
}

INSTANTIATE_TEST_SUITE_P(ReplacementBehavior, UnicodeReplacementArticleTest,
                         testing::ValuesIn(UNICODE_REPLACEMENT_ARTICLE_CASES),
                         [](const testing::TestParamInfo<UnicodeReplacementArticleCase> &info)
                         { return std::string{info.param.name}; });

TEST_F(ArticleIoTest, unicodeProfileMalformedTextMatchesHeaderAndBodyRecovery)
{
    g_charsets = "up";
    reset_char_subst_mode();
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    m_mime_section.m_encoding = MENCODE_QPRINT;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("=FF=E2=80=99\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xEF\xBF\xBD'\n", line);
    EXPECT_EQ(str_char_subst(decode_header("=?UTF-8?Q?=FF=E2=80=99?="), 'u') + "\n", line);
}

TEST_F(ArticleIoTest, changingSubstitutionModeInvalidatesPresentationCache)
{
    set_char_subst_mode('p');
    m_mime_section.mime_parse_type("text/plain; charset=Latin1");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("\xC4\n");
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_FALSE(art_buf_empty());
    const ArticleLine      old_top = g_top_line;
    const ArticleLine      old_line_num = g_art_line_num;
    const bool             old_seek = g_do_fseek;
    const std::string_view old_cr = g_tc_CR;
    g_tc_CR = "\r";
    g_top_line = ArticleLine{4};

    testing::internal::CaptureStdout();
    const PageSwitchResult result = page_switch("_C");
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(PS_NORM, result);
    EXPECT_EQ('a', current_char_subst_mode());
    EXPECT_TRUE(art_buf_empty());
    EXPECT_EQ(ArticleLine{-1}, g_top_line);
    g_top_line = old_top;
    g_art_line_num = old_line_num;
    g_do_fseek = old_seek;
    g_tc_CR = old_cr;
}
