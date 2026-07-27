// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/mime-internal.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/datasrc.h>
#include <trn/file-contents.h>
#include <trn/head.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/util2.h>

#include <test_config.h>
#include <test_mime.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

using namespace testing;

namespace
{

namespace fs = std::filesystem;

struct MimeTest : Test
{
    ~MimeTest() override = default;

protected:
    void SetUp() override
    {
        mime_init();
        mime_set_executor(m_exec.AsStdFunction());
        mime_read_mimecap(TRN_TEST_MIMECAP_FILE);
    }
    void TearDown() override
    {
        mime_final();
    }

    StrictMock<MockFunction<int(const char *shell, const char *cmd)>> m_exec;
};

} // namespace

TEST_F(MimeTest, imageGif)
{
    MimeCapEntry *cap = mime_find_mimecap_entry(TRN_TEST_MIME_IMAGE_GIF_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_EQ(TRN_TEST_MIME_IMAGE_GIF_CONTENT_TYPE, cap->content_type);
    ASSERT_EQ(TRN_TEST_MIME_IMAGE_GIF_COMMAND, cap->command);
    ASSERT_TRUE(cap->test_command.empty());
    ASSERT_EQ(TRN_TEST_MIME_IMAGE_GIF_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, escapedPercentBecomesInterpolatorLiteralPercent)
{
    MimeCapEntry *cap = mime_find_mimecap_entry("application/x-percent", MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_EQ("percent %%s", cap->command);
    ASSERT_TRUE(cap->test_command.empty());
    ASSERT_EQ("Escaped percent", cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, imageWildcardWithLabel)
{
    MimeCapEntry *cap = mime_find_mimecap_entry(TRN_TEST_MIME_IMAGE_ANY_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_EQ(TRN_TEST_MIME_IMAGE_ANY_CONTENT_TYPE, cap->content_type);
    ASSERT_EQ(TRN_TEST_MIME_IMAGE_ANY_COMMAND, cap->command);
    ASSERT_TRUE(cap->test_command.empty());
    ASSERT_EQ(TRN_TEST_MIME_IMAGE_ANY_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, appleFileIgnoredParams)
{
    MimeCapEntry *cap = mime_find_mimecap_entry(TRN_TEST_MIME_APPLEFILE_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_EQ(TRN_TEST_MIME_APPLEFILE_CONTENT_TYPE, cap->content_type);
    ASSERT_EQ(TRN_TEST_MIME_APPLEFILE_COMMAND, cap->command);
    ASSERT_TRUE(cap->test_command.empty());
    ASSERT_EQ(TRN_TEST_MIME_APPLEFILE_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, textPlainHasFlags)
{
    MimeCapEntry *cap = mime_find_mimecap_entry(TRN_TEST_MIME_TEXT_PLAIN_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_EQ(TRN_TEST_MIME_TEXT_PLAIN_CONTENT_TYPE, cap->content_type);
    ASSERT_EQ(TRN_TEST_MIME_TEXT_PLAIN_COMMAND, cap->command);
    ASSERT_TRUE(cap->test_command.empty());
    ASSERT_EQ(TRN_TEST_MIME_TEXT_PLAIN_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NEEDS_TERMINAL | MCF_COPIOUS_OUTPUT, cap->flags);
}

TEST(MimeParseParamsTest, returnsViewsWithoutModifyingInput)
{
    const std::string input{" (leading) multipart/mixed (type); "
                            "boundary = \"part\\\"-boundary\"; charset = utf-8"};

    const MimeParamViews parsed = mime_parse_params(input);

    EXPECT_EQ("multipart/mixed", parsed.value);
    EXPECT_THAT(parsed.params, ElementsAre("boundary = \"part\\\"-boundary\"", "charset = utf-8"));
}

TEST(MimeParseParamsTest, decodesEscapedQuotedValueWhenConsumed)
{
    const std::string input{"multipart/mixed; boundary = \"part\\\"-boundary\""};
    MimeSection       section{};

    section.mime_parse_type(input);

    ASSERT_TRUE(section.m_boundary);
    EXPECT_EQ("part\"-boundary", *section.m_boundary);
    section.mime_clear_struct();
}

TEST(MimeParseTypeTest, parsesMessagePartialParameters)
{
    MimeSection section{};

    section.mime_parse_type("message/partial; id=\"part-id\"; number=2; total=4");

    EXPECT_EQ(MESSAGE_MIME, section.m_type);
    ASSERT_TRUE(section.m_filename);
    EXPECT_EQ("part-id", *section.m_filename);
    EXPECT_EQ(2, section.m_part);
    EXPECT_EQ(4, section.m_total);
}

TEST(MimeParseTypeTest, preservesPrefixNumberParsing)
{
    MimeSection section{};

    section.mime_parse_type("message/partial; id=part-id; number=2x; total=4x");

    EXPECT_EQ(MESSAGE_MIME, section.m_type);
    EXPECT_EQ(2, section.m_part);
    EXPECT_EQ(4, section.m_total);
}

TEST(MimeDescriptionTest, describesTypeAndNormalizedFilename)
{
    MimeSection section{};
    section.m_type_name = "application/pdf";
    section.m_filename = "/tmp/report_pdf";

    const std::string description = section.mime_description();

    EXPECT_EQ("[Attachment type=application/pdf, name=report_pdf]\n", description);
}

namespace
{

MimeEncoding parse_encoding_for_test(std::string_view text)
{
    MimeSection section{};
    section.m_encoding = MENCODE_UNHANDLED;

    section.mime_parse_encoding(text);

    return section.m_encoding;
}

} // namespace

TEST(MimeParseEncodingTest, parsesNoEncodingValues)
{
    EXPECT_EQ(MENCODE_NONE, parse_encoding_for_test(""));
    EXPECT_EQ(MENCODE_NONE, parse_encoding_for_test(" \t"));
    EXPECT_EQ(MENCODE_NONE, parse_encoding_for_test("7bit"));
    EXPECT_EQ(MENCODE_NONE, parse_encoding_for_test("8BIT"));
    EXPECT_EQ(MENCODE_NONE, parse_encoding_for_test("binary"));
}

TEST(MimeParseEncodingTest, parsesTransferEncodings)
{
    EXPECT_EQ(MENCODE_QPRINT, parse_encoding_for_test("quoted-printable"));
    EXPECT_EQ(MENCODE_BASE64, parse_encoding_for_test("base64"));
    EXPECT_EQ(MENCODE_UUE, parse_encoding_for_test("x-uue"));
    EXPECT_EQ(MENCODE_UUE, parse_encoding_for_test("x-uuencode"));
}

TEST(MimeParseEncodingTest, permitsTokenTerminators)
{
    EXPECT_EQ(MENCODE_BASE64, parse_encoding_for_test("base64; charset=utf-8"));
    EXPECT_EQ(MENCODE_BASE64, parse_encoding_for_test("base64 (comment)"));
    EXPECT_EQ(MENCODE_BASE64, parse_encoding_for_test("base64 extra"));
}

TEST(MimeParseEncodingTest, rejectsUnknownEncodingsAndSuffixes)
{
    EXPECT_EQ(MENCODE_UNHANDLED, parse_encoding_for_test("rot13"));
    EXPECT_EQ(MENCODE_UNHANDLED, parse_encoding_for_test("base64x"));
}

namespace
{

struct MimeBoundaryTest : Test
{
protected:
    void SetUp() override
    {
        m_old_mime_section = g_mime_section;
        m_parent.m_boundary = "part-boundary";
        m_parent.m_boundary_len = static_cast<short>(m_parent.m_boundary->size());
        m_current.m_prev = &m_parent;
        g_mime_section = &m_current;
    }

    void TearDown() override
    {
        g_mime_section = m_old_mime_section;
    }

    int end_of_section(std::string_view line)
    {
        return mime_end_of_section(line);
    }

    MimeSection  m_parent{};
    MimeSection  m_current{};
    MimeSection *m_old_mime_section{};
};

} // namespace

TEST_F(MimeBoundaryTest, detectsBoundaryLine)
{
    EXPECT_EQ(1, end_of_section("--part-boundary\n"));
}

TEST_F(MimeBoundaryTest, detectsBoundaryAtEndOfString)
{
    EXPECT_EQ(1, end_of_section("--part-boundary"));
}

TEST_F(MimeBoundaryTest, detectsFinalBoundaryLine)
{
    EXPECT_EQ(2, end_of_section("--part-boundary--\n"));
}

TEST_F(MimeBoundaryTest, rejectsBoundaryPrefix)
{
    EXPECT_EQ(0, end_of_section("--part-boundary-extra\n"));
}

TEST_F(MimeBoundaryTest, rejectsBodyLine)
{
    EXPECT_EQ(0, end_of_section("body\n"));
}

namespace
{

struct MimeSubHeaderTest : Test
{
protected:
    void SetUp() override
    {
        m_old_mime_section = g_mime_section;
        m_old_mime_state = g_mime_state;
        head_init();
        g_mime_section = &m_mime_section;
        m_input = std::tmpfile();
        ASSERT_NE(nullptr, m_input);
    }

    void TearDown() override
    {
        if (m_input != nullptr)
        {
            std::fclose(m_input);
        }
        m_mime_section.mime_clear_struct();
        g_mime_section = m_old_mime_section;
        g_mime_state = m_old_mime_state;
        head_final();
    }

    MimeSection  m_mime_section{};
    MimeSection *m_old_mime_section{};
    MimeState    m_old_mime_state{};
    std::FILE   *m_input{};
};

} // namespace

TEST_F(MimeSubHeaderTest, parsesFoldedHeaders)
{
    constexpr std::string_view header{"Content-Type: multipart/mixed;\n"
                                      " boundary=\"part-boundary\"\n"
                                      "Content-Transfer-Encoding: base64\n"
                                      "Content-Disposition: inline;\n"
                                      " filename=\"part.bin\"\n"
                                      "\n"};
    ASSERT_EQ(header.size(), std::fwrite(header.data(), sizeof(char), header.size(), m_input));
    std::rewind(m_input);

    mime_parse_sub_header(m_input, {});

    EXPECT_EQ(MULTIPART_MIME, m_mime_section.m_type);
    EXPECT_EQ(MENCODE_BASE64, m_mime_section.m_encoding);
    EXPECT_EQ(MSF_INLINE, m_mime_section.m_flags);
    ASSERT_TRUE(m_mime_section.m_type_name);
    EXPECT_EQ("multipart/mixed", *m_mime_section.m_type_name);
    ASSERT_TRUE(m_mime_section.m_boundary);
    EXPECT_EQ("part-boundary", *m_mime_section.m_boundary);
    ASSERT_TRUE(m_mime_section.m_filename);
    EXPECT_EQ("part.bin", *m_mime_section.m_filename);
}

TEST_F(MimeSubHeaderTest, parsesSuppliedFirstHeaderLine)
{
    constexpr std::string_view header{"Content-Transfer-Encoding: base64\n"
                                      "\n"};
    ASSERT_EQ(header.size(), std::fwrite(header.data(), sizeof(char), header.size(), m_input));
    std::rewind(m_input);

    mime_parse_sub_header(m_input, "Content-Type: text/html\n");

    EXPECT_EQ(HTML_TEXT_MIME, m_mime_section.m_type);
    EXPECT_EQ(MENCODE_BASE64, m_mime_section.m_encoding);
    ASSERT_TRUE(m_mime_section.m_type_name);
    EXPECT_EQ("text/html", *m_mime_section.m_type_name);
}

TEST_F(MimeSubHeaderTest, parsesContentNameHeader)
{
    constexpr std::string_view header{"Content-Name: (legacy name) part.txt\n"
                                      "\n"};
    ASSERT_EQ(header.size(), std::fwrite(header.data(), sizeof(char), header.size(), m_input));
    std::rewind(m_input);

    mime_parse_sub_header(m_input, {});

    ASSERT_TRUE(m_mime_section.m_filename);
    EXPECT_EQ("part.txt", *m_mime_section.m_filename);
}

namespace
{

struct MimeExecTest : MimeTest
{
protected:
    void SetUp() override
    {
        MimeTest::SetUp();
        g_decode_filename = TRN_TEST_MIME_PDF_DECODE_FILE;
        m_mime_section.mime_parse_type(TRN_TEST_MIME_PDF_CONTENT_TYPE "; " TRN_TEST_MIME_PDF_SECTION_PARAMS);
        g_mime_section = &m_mime_section;
    }
    void TearDown() override
    {
        g_mime_section = nullptr;
        g_decode_filename.clear();
        MimeTest::TearDown();
    }

    MimeSection m_mime_section{};
};

}

TEST_F(MimeExecTest, applicationPdfSuccessfulTestCommand)
{
    EXPECT_CALL(m_exec, Call(_, StrEq(TRN_TEST_MIME_PDF_TEST_EXEC_COMMAND))).WillOnce(Return(0));

    MimeCapEntry *cap = mime_find_mimecap_entry(TRN_TEST_MIME_PDF_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_EQ(TRN_TEST_MIME_PDF_CONTENT_TYPE, cap->content_type);
    ASSERT_EQ(TRN_TEST_MIME_PDF_COMMAND, cap->command);
    ASSERT_EQ(TRN_TEST_MIME_PDF_TEST_COMMAND, cap->test_command);
    ASSERT_EQ(TRN_TEST_MIME_PDF_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeExecTest, applicationPdfFailedTestCommand)
{
    EXPECT_CALL(m_exec, Call(_, _)).WillOnce(Return(1));

    MimeCapEntry *cap = mime_find_mimecap_entry(TRN_TEST_MIME_PDF_CONTENT_TYPE, MCF_NONE);

    ASSERT_EQ(nullptr, cap);
}

TEST_F(MimeExecTest, rejectsUnsupportedNameEscape)
{
    EXPECT_CALL(m_exec, Call(_, _)).Times(0);

    EXPECT_EQ(-1, mime_exec("viewer %n"));
}

TEST_F(MimeExecTest, rejectsUnsupportedFileEscape)
{
    EXPECT_CALL(m_exec, Call(_, _)).Times(0);

    EXPECT_EQ(-1, mime_exec("viewer %F"));
}

TEST_F(MimeExecTest, rejectsUnterminatedParameterEscape)
{
    EXPECT_CALL(m_exec, Call(_, _)).Times(0);

    EXPECT_EQ(-1, mime_exec("viewer %{name"));
}

namespace
{

struct CatDecodeTest : MimeTest
{
protected:
    void SetUp() override
    {
        MimeTest::SetUp();
        m_old_current_path = fs::current_path();
        m_old_art_fp = g_art_fp;
        m_old_mime_section = g_mime_section;
        m_old_data_source = g_data_source;
        m_old_no_wait_fork = g_no_wait_fork;
        m_old_decode_filename = g_decode_filename;

        const TestInfo *test_info = UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::current_path(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        m_input = std::tmpfile();
        ASSERT_NE(nullptr, m_input);

        g_mime_section = &m_mime_section;
        g_data_source = &m_data_source;
        g_art_fp = nullptr;
        g_no_wait_fork = true;
        m_mime_section.m_filename = "cat-output.txt";
    }

    void TearDown() override
    {
        if (g_art_fp == m_input)
        {
            g_art_fp = nullptr;
        }
        if (m_input != nullptr)
        {
            std::fclose(m_input);
        }
        std::error_code error;
        fs::current_path(m_old_current_path, error);
        g_art_fp = m_old_art_fp;
        g_mime_section = m_old_mime_section;
        g_data_source = m_old_data_source;
        g_no_wait_fork = m_old_no_wait_fork;
        g_decode_filename = m_old_decode_filename;
        MimeTest::TearDown();
    }

    void write_input(std::string_view text)
    {
        ASSERT_EQ(text.size(), std::fwrite(text.data(), sizeof(char), text.size(), m_input));
        std::rewind(m_input);
    }

    fs::path     m_old_current_path;
    fs::path     m_output_dir;
    std::FILE   *m_old_art_fp{};
    MimeSection *m_old_mime_section{};
    DataSource  *m_old_data_source{};
    bool         m_old_no_wait_fork{};
    std::string  m_old_decode_filename;
    DataSource   m_data_source{};
    MimeSection  m_mime_section{};
    std::FILE   *m_input{};
};

} // namespace

TEST_F(CatDecodeTest, copiesFileInputToDecodedFile)
{
    write_input("first line\nsecond line\n");

    testing::internal::CaptureStdout();
    EXPECT_EQ(DECODE_MAYBE_DONE, cat_decode(m_input, DECODE_START));
    EXPECT_EQ(DECODE_DONE, cat_decode(m_input, DECODE_DONE));
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Decoding cat-output.txt", output);
    EXPECT_EQ("first line\nsecond line\n", file_contents(m_output_dir / "cat-output.txt"));
}

TEST_F(CatDecodeTest, copiesArticleInputToDecodedFileUntilBoundary)
{
    write_input("first line\n--part\nsecond line\n");
    g_art_fp = m_input;

    MimeSection parent;
    parent.m_boundary = "part";
    parent.m_boundary_len = 4;
    m_mime_section.m_prev = &parent;

    testing::internal::CaptureStdout();
    EXPECT_EQ(DECODE_MAYBE_DONE, cat_decode(nullptr, DECODE_START));
    EXPECT_EQ(DECODE_DONE, cat_decode(nullptr, DECODE_DONE));
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("Decoding cat-output.txt", output);
    EXPECT_EQ("first line\n", file_contents(m_output_dir / "cat-output.txt"));
}

namespace
{

struct HtmlFilterTest : Test
{
protected:
    void SetUp() override
    {
        m_old_mime_section = g_mime_section;
        m_old_art_buf = g_art_buf;
        m_old_cols = g_tc_COLS;
        m_old_word_wrap_offset = g_word_wrap_offset;

        g_mime_section = &m_mime_section;
        g_art_buf = m_output.data();
        g_tc_COLS = 200;
        g_word_wrap_offset = 0;
    }

    void TearDown() override
    {
        m_mime_section.mime_clear_struct();
        g_mime_section = m_old_mime_section;
        g_art_buf = m_old_art_buf;
        g_tc_COLS = m_old_cols;
        g_word_wrap_offset = m_old_word_wrap_offset;
    }

    std::string filter(std::string_view text)
    {
        const std::string input{text};
        const int         length = filter_html(m_output.data(), input.c_str());
        return {m_output.data(), static_cast<std::size_t>(length)};
    }

    std::string filter(std::string_view first, std::string_view second)
    {
        char       *output = m_output.data();
        std::string input{first};
        output += filter_html(output, input.c_str());
        input = std::string{second};
        output += filter_html(output, input.c_str());
        return {m_output.data(), static_cast<std::size_t>(output - m_output.data())};
    }

    MimeSection                    m_mime_section{};
    MimeSection                   *m_old_mime_section{};
    char                          *m_old_art_buf{};
    int                            m_old_cols{};
    int                            m_old_word_wrap_offset{};
    std::array<char, LINE_BUF_LEN> m_output{};
};

} // namespace

TEST_F(HtmlFilterTest, rendersLowerRomanListMarkers)
{
    EXPECT_EQ("List:\n\n   i. one\n  ii. two\n iii. three", //
              filter("List:<ol type=i><li>one<li>two<li>three</ol>"));
}

TEST_F(HtmlFilterTest, rendersUpperRomanListMarkers)
{
    EXPECT_EQ("List:\n\n   I. one\n  II. two\n III. three", //
              filter("List:<ol type=I><li>one<li>two<li>three</ol>"));
}

TEST_F(HtmlFilterTest, rendersBlockquoteCiteIndent)
{
    EXPECT_EQ("Intro\n\n> quoted", filter("Intro<blockquote type=cite>quoted</blockquote>"));
}

TEST_F(HtmlFilterTest, rendersBlockquoteBorderStyleIndent)
{
    EXPECT_EQ("Intro\n\n> quoted", filter("Intro<blockquote style=\"border-left: solid\">quoted</blockquote>"));
}

TEST_F(HtmlFilterTest, rendersUnorderedListCircleMarker)
{
    EXPECT_EQ("List:\n\n  o one", filter("List:<ul type=circle><li>one</ul>"));
}

TEST_F(HtmlFilterTest, rendersUnorderedListSquareMarker)
{
    EXPECT_EQ("List:\n\n  + one", filter("List:<ul type=square><li>one</ul>"));
}

TEST_F(HtmlFilterTest, treatsEmptyBlockquoteAttributesAsAbsent)
{
    EXPECT_EQ("Intro\n\n    quoted", filter("Intro<blockquote type= style=>quoted</blockquote>"));
}

TEST_F(HtmlFilterTest, treatsEmptyOrderedListTypeAsAbsent)
{
    EXPECT_EQ("List:\n\n 1. one", filter("List:<ol type=><li>one</ol>"));
}

TEST_F(HtmlFilterTest, rendersDecimalListMarkersPastTwoDigits)
{
    EXPECT_EQ("List:\n\n 1. one\n 2. two\n 3. three\n 4. four\n 5. five\n 6. six\n 7. seven\n 8. eight\n"
              " 9. nine\n10. ten",
              filter("List:<ol><li>one<li>two<li>three<li>four<li>five<li>six<li>seven<li>eight<li>nine<li>ten</ol>"));
}

TEST_F(HtmlFilterTest, rendersImageMarker)
{
    EXPECT_EQ("Before [Image] after", filter("Before <img src=x>after"));
}

TEST_F(HtmlFilterTest, rendersNamedEntities)
{
    EXPECT_EQ("Tom & Jerry <tag>", filter("Tom &amp; Jerry &lt;tag&gt;"));
}

TEST_F(HtmlFilterTest, keepsPartialTagNameAcrossCalls)
{
    EXPECT_EQ("List:\n\n 1. one", filter("List:<o", "l><li>one</ol>"));
}
