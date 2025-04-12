#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <config/common.h>
#include <trn/mime-internal.h>

#include <trn/util.h>
#include <util/util2.h>

#include "test_mime.h"

using namespace testing;

namespace {

struct MimeTest : Test
{
    ~MimeTest() override = default;

protected:
    void SetUp() override
    {
        mime_init();
        mime_set_executor(m_exec.AsStdFunction());
        mime_ReadMimecap(TRN_TEST_MIMECAP_FILE);
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
    MIMECAP_ENTRY *cap = mime_FindMimecapEntry(TRN_TEST_MIME_IMAGE_GIF_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_STREQ(TRN_TEST_MIME_IMAGE_GIF_CONTENT_TYPE, cap->contenttype);
    ASSERT_STREQ(TRN_TEST_MIME_IMAGE_GIF_COMMAND, cap->command);
    ASSERT_EQ(nullptr, cap->testcommand);
    ASSERT_STREQ(TRN_TEST_MIME_IMAGE_GIF_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, imageWildcardWithLabel)
{
    MIMECAP_ENTRY *cap = mime_FindMimecapEntry(TRN_TEST_MIME_IMAGE_ANY_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_STREQ(TRN_TEST_MIME_IMAGE_ANY_CONTENT_TYPE, cap->contenttype);
    ASSERT_STREQ(TRN_TEST_MIME_IMAGE_ANY_COMMAND, cap->command);
    ASSERT_EQ(nullptr, cap->testcommand);
    ASSERT_STREQ(TRN_TEST_MIME_IMAGE_ANY_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, appleFileIgnoredParams)
{
    MIMECAP_ENTRY *cap = mime_FindMimecapEntry(TRN_TEST_MIME_APPLEFILE_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_STREQ(TRN_TEST_MIME_APPLEFILE_CONTENT_TYPE, cap->contenttype);
    ASSERT_STREQ(TRN_TEST_MIME_APPLEFILE_COMMAND, cap->command);
    ASSERT_EQ(nullptr, cap->testcommand);
    ASSERT_STREQ(TRN_TEST_MIME_APPLEFILE_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeTest, textPlainHasFlags)
{
    MIMECAP_ENTRY *cap = mime_FindMimecapEntry(TRN_TEST_MIME_TEXT_PLAIN_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_STREQ(TRN_TEST_MIME_TEXT_PLAIN_CONTENT_TYPE, cap->contenttype);
    ASSERT_STREQ(TRN_TEST_MIME_TEXT_PLAIN_COMMAND, cap->command);
    ASSERT_EQ(nullptr, cap->testcommand);
    ASSERT_STREQ(TRN_TEST_MIME_TEXT_PLAIN_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NEEDSTERMINAL | MCF_COPIOUSOUTPUT, cap->flags);
}

namespace {

struct MimeExecTest : MimeTest
{
protected:
    void SetUp() override
    {
        MimeTest::SetUp();
        g_decode_filename = safemalloc(1024);
        strcpy(g_decode_filename, TRN_TEST_MIME_PDF_DECODE_FILE);
        m_mime_section.type_name = savestr(TRN_TEST_MIME_PDF_CONTENT_TYPE);
        m_mime_section.type_params = mime_ParseParams(savestr(TRN_TEST_MIME_PDF_SECTION_PARAMS));
        g_mime_section = &m_mime_section;
    }
    void TearDown() override
    {
        free(m_mime_section.type_params);
        free(m_mime_section.type_name);
        g_mime_section = nullptr;
        safefree0(g_decode_filename);
        MimeTest::TearDown();
    }

    MIME_SECT m_mime_section{};
};

}

TEST_F(MimeExecTest, applicationPdfSuccessfulTestCommand)
{
    EXPECT_CALL(m_exec, Call(_, StrEq(TRN_TEST_MIME_PDF_TEST_EXEC_COMMAND))).WillOnce(Return(0));

    MIMECAP_ENTRY *cap = mime_FindMimecapEntry(TRN_TEST_MIME_PDF_CONTENT_TYPE, MCF_NONE);

    ASSERT_NE(nullptr, cap);
    ASSERT_STREQ(TRN_TEST_MIME_PDF_CONTENT_TYPE, cap->contenttype);
    ASSERT_STREQ(TRN_TEST_MIME_PDF_COMMAND, cap->command);
    ASSERT_STREQ(TRN_TEST_MIME_PDF_TEST_COMMAND, cap->testcommand);
    ASSERT_STREQ(TRN_TEST_MIME_PDF_DESCRIPTION, cap->description);
    ASSERT_EQ(MCF_NONE, cap->flags);
}

TEST_F(MimeExecTest, applicationPdfFailedTestCommand)
{
    EXPECT_CALL(m_exec, Call(_, _)).WillOnce(Return(1));

    MIMECAP_ENTRY *cap = mime_FindMimecapEntry(TRN_TEST_MIME_PDF_CONTENT_TYPE, MCF_NONE);

    ASSERT_EQ(nullptr, cap);
}
