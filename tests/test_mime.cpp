// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/mime-internal.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/head.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/util2.h>

#include <test_mime.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

using namespace testing;

namespace
{

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

    mime_parse_sub_header(m_input, nullptr);

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
