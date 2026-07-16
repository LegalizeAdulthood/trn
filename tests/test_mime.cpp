// This software is copyrighted as detailed in the LICENSE file.
#include <trn/mime-internal.h>

#include <config/common.h>
#include <trn/artio.h>
#include <trn/terminal.h>
#include <trn/util.h>
#include <util/util2.h>

#include <test_mime.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
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

namespace
{

struct MimeExecTest : MimeTest
{
protected:
    void SetUp() override
    {
        MimeTest::SetUp();
        g_decode_filename = TRN_TEST_MIME_PDF_DECODE_FILE;
        m_mime_section.m_type_name = TRN_TEST_MIME_PDF_CONTENT_TYPE;
        m_type_params = TRN_TEST_MIME_PDF_SECTION_PARAMS;
        m_mime_section.m_type_params = mime_parse_params(m_type_params.data());
        g_mime_section = &m_mime_section;
    }
    void TearDown() override
    {
        g_mime_section = nullptr;
        g_decode_filename.clear();
        MimeTest::TearDown();
    }

    MimeSection m_mime_section{};
    std::string m_type_params;
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
