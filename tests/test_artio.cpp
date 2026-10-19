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
#include <trn/mime.h>
#include <trn/terminal.h>
#include <trn/utf.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

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

    void open_article_text(std::string_view text)
    {
        const fs::path article_file = m_output_dir / "article";
        {
            std::ofstream output{article_file, std::ios::binary};
            output << text;
        }

        const std::string article_file_name = article_file.generic_string();
        g_art_fp = std::fopen(article_file_name.c_str(), "r");
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

TEST_F(ArticleIoTest, viewReturnsHeaderBeforeBodyStart)
{
    g_head_buf = "Subject: example\n\n";
    g_header_type[PAST_HEADER].min_pos = ArticlePosition{static_cast<long>(g_head_buf.size())};

    EXPECT_EQ(g_head_buf, art_buf_view(ArticlePosition{}));
    EXPECT_EQ("example\n\n", art_buf_view(ArticlePosition{9}));
    EXPECT_EQ("\n", art_buf_view(g_header_type[PAST_HEADER].min_pos - ArticlePosition{1}));
}

TEST_F(ArticleIoTest, viewReturnsBufferedBodyAfterHeader)
{
    g_head_buf = "Subject: example\n\n";
    const ArticlePosition body_start{static_cast<long>(g_head_buf.size())};
    g_header_type[PAST_HEADER].min_pos = body_start;
    open_article_text(g_head_buf + "alpha\nbeta\n");
    ASSERT_EQ(0, seek_art(body_start));
    set_art_buf_seek(body_start);
    std::string line;
    ASSERT_TRUE(read_art_buf(line, false));
    ASSERT_TRUE(read_art_buf(line, false));

    EXPECT_EQ("alpha\nbeta\n", art_buf_view(body_start));
    EXPECT_EQ("beta\n", art_buf_view(body_start + ArticlePosition{6}));
    EXPECT_TRUE(art_buf_view(body_start + art_buf_len()).empty());
    EXPECT_TRUE(art_buf_view(body_start + art_buf_len() + ArticlePosition{1}).empty());
    EXPECT_TRUE(art_buf_view(ArticlePosition{-1}).empty());
}

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

TEST_F(ArticleIoTest, decodesLatinTextBeforeCachingAndKeepsRawBytes)
{
    m_mime_section.mime_parse_type("text/plain; charset=ISO-8859-1");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("\xE9\n");
    std::string line;

    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
    utf_init(CHARSET_NAME_WINDOWS_1252, CHARSET_NAME_ASCII);
    ASSERT_EQ(0, seek_art_buf(ArticlePosition{}));
    ASSERT_TRUE(read_art_buf(line, false));
    EXPECT_EQ("\xC3\xA9\n", line);
    std::rewind(g_art_fp);
    ASSERT_TRUE(read_art(line));
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

TEST_F(ArticleIoTest, unknownAndMissingCharsetsNeverInheritPreviousArticle)
{
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    for (const std::string_view type : {"text/plain; charset=Latin1", "text/plain; charset=unknown", "text/plain"})
    {
        m_mime_section.mime_parse_type(type);
        open_article_text("\xE9\n");
        std::string line;
        ASSERT_TRUE(read_art_buf(line, false));
        EXPECT_EQ(type == "text/plain; charset=Latin1" ? "\xC3\xA9\n" : "\xEF\xBF\xBD\n", line);
        std::fclose(g_art_fp);
        g_art_fp = nullptr;
    }
}

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

TEST_F(ArticleIoTest, unicodeProfileMatchesEncodedHeadersAndMimeBodies)
{
    g_charsets = "up";
    reset_char_subst_mode();
    g_word_wrap_offset = -1;
    g_mime_section = &m_mime_section;
    g_is_mime = true;
    struct EncodedCase
    {
        std::string_view type;
        std::string_view header;
        std::string_view body;
        std::string_view expected;
    };
    constexpr EncodedCase cases[]{
        {"text/plain; charset=UTF-8", "=?UTF-8?Q?=F0=9F=98=8A=E2=80=AF=E2=80=94?=", "=F0=9F=98=8A=E2=80=AF=E2=80=94\n",
         ":) ---\n"},
        {"text/plain; charset=windows-1252", "=?windows-1252?Q?=91a=92_=93b=94_=96_=97_=85_=A0x?=",
         "=91a=92 =93b=94 =96 =97 =85 =A0x\n", "`a' \"b\" -- --- ...  x\n"},
    };
    for (const EncodedCase &value : cases)
    {
        SCOPED_TRACE(value.type);
        m_mime_section.mime_parse_type(value.type);
        m_mime_section.m_encoding = MENCODE_QPRINT;
        g_mime_state = TEXT_MIME;
        open_article_text(value.body);
        std::string line;
        ASSERT_TRUE(read_art_buf(line, false));
        EXPECT_EQ(value.expected, line);
        EXPECT_EQ(str_char_subst(decode_header(value.header), 'u') + "\n", line);
        EXPECT_FALSE(read_art_buf(line, false));
        ASSERT_EQ(0, seek_art_buf(ArticlePosition{}));
        ASSERT_TRUE(read_art_buf(line, false));
        EXPECT_EQ(value.expected, line);
        std::rewind(g_art_fp);
        ASSERT_TRUE(read_art(line));
        EXPECT_EQ(value.body, line);
        std::fclose(g_art_fp);
        g_art_fp = nullptr;
    }
}

TEST_F(ArticleIoTest, unicodeReplacementEscapesAndDeletionSurviveCacheRereads)
{
    g_charsets = "up";
    reset_char_subst_mode();
    ASSERT_TRUE(g_unicode_substitutions.configure(IniSection{
        "unicode substitutions", {}, "U+2019 = \"left\\t\\nright\\\\\\\"\"\nU+2014 = \"\"\nU+1F60A = \xE2\x80\x94\n"}));
    m_mime_section.mime_parse_type("text/plain; charset=UTF-8");
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    g_word_wrap_offset = -1;
    open_article_text("A\xE2\x80\x99"
                      "B\xE2\x80\x94"
                      "C\xF0\x9F\x98\x8A"
                      "D\nNext\n");
    std::string line;

    for (int pass = 0; pass < 2; ++pass)
    {
        ASSERT_EQ(0, seek_art_buf(ArticlePosition{}));
        ASSERT_TRUE(read_art_buf(line, false));
        EXPECT_EQ("Aleft\t\n", line);
        ASSERT_TRUE(read_art_buf(line, false));
        EXPECT_EQ("right\\\"BC\xE2\x80\x94"
                  "D\n",
                  line);
        ASSERT_TRUE(read_art_buf(line, false));
        EXPECT_EQ("Next\n", line);
        EXPECT_FALSE(read_art_buf(line, false));
    }
    EXPECT_EQ("Aleft\t\nright\\\"BC\xE2\x80\x94"
              "D",
              str_char_subst(decode_header("=?UTF-8?Q?A=E2=80=99B=E2=80=94C=F0=9F=98=8AD?="), 'u'));
}

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
