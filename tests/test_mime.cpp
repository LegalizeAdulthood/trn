// This software is copyrighted as detailed in the LICENSE file.
#include <trn/mime-internal.h>

#include <config/common.h>
#include <trn/util.h>
#include <util/util2.h>

#include <test_mime.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <string>

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
