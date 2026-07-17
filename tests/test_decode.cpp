// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/decode.h>

#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/datasrc.h>
#include <trn/mime-internal.h>
#include <trn/mime.h>
#include <trn/util.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class DecodePieceDirectoryTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_current_path = fs::current_path();
        m_old_tmp_dir = g_tmp_dir;
        m_old_login_name = g_login_name;
        m_old_data_source = g_data_source;
        m_old_art_fp = g_art_fp;
        m_old_mime_section = g_mime_section;
        m_old_mime_getc_line = g_mime_getc_line;
        m_old_is_mime = g_is_mime;
        m_old_no_wait_fork = g_no_wait_fork;
        m_old_decode_filename = g_decode_filename;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        m_input = std::tmpfile();
        ASSERT_NE(nullptr, m_input);

        g_tmp_dir = m_output_dir.generic_string();
        g_login_name = "decode-user";
        g_data_source = &m_data_source;
        g_art_fp = m_input;
        g_mime_section = &m_section;
        g_mime_getc_line = nullptr;
        g_is_mime = false;
        g_no_wait_fork = true;

        m_section.m_filename = "payload_bin";
        m_section.m_type = APP_MIME;
        m_section.m_encoding = MENCODE_NONE;
        m_section.m_part = 1;
        m_section.m_total = 1;

        mime_set_executor(
            [this](const char *, const char *command)
            {
                m_command = command;
                m_piece_dir = fs::current_path();
                m_output_existed = fs::exists(m_piece_dir / "payload_bin");
                return 0;
            });
    }

    void TearDown() override
    {
        mime_set_executor(do_shell);
        if (m_input != nullptr)
        {
            std::fclose(m_input);
        }
        g_art_fp = m_old_art_fp;
        g_mime_section = m_old_mime_section;
        g_mime_getc_line = m_old_mime_getc_line;
        g_is_mime = m_old_is_mime;
        g_no_wait_fork = m_old_no_wait_fork;
        g_decode_filename = m_old_decode_filename;
        g_tmp_dir = m_old_tmp_dir;
        g_login_name = m_old_login_name;
        g_data_source = m_old_data_source;

        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);
    }

    DataSource  m_data_source{};
    MimeSection m_section{};
    std::FILE  *m_input{};
    fs::path    m_output_dir;
    fs::path    m_piece_dir;
    std::string m_command;
    bool        m_output_existed{};

    fs::path     m_old_current_path;
    std::string  m_old_tmp_dir;
    std::string  m_old_login_name;
    DataSource  *m_old_data_source{};
    std::FILE   *m_old_art_fp{};
    MimeSection *m_old_mime_section{};
    char        *m_old_mime_getc_line{};
    bool         m_old_is_mime{};
    bool         m_old_no_wait_fork{};
    std::string  m_old_decode_filename;
};

} // namespace

TEST_F(DecodePieceDirectoryTest, createsUsesAndRemovesPieceDirectory)
{
    MimeCapEntry mime_cap;
    mime_cap.command = "viewer %s";
    testing::internal::CaptureStdout();

    const bool        result = decode_piece(&mime_cap, nullptr);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_EQ("Decoding payload_bin", output);
    EXPECT_EQ("viewer payload_bin", m_command);
    EXPECT_EQ("payload_bin", m_piece_dir.filename().generic_string());
    EXPECT_TRUE(m_output_existed);
    EXPECT_FALSE(fs::exists(m_piece_dir));
}

TEST(DecodeFixFilenameTest, usesUnixBasename)
{
    EXPECT_EQ("file_txt", decode_fix_filename("/tmp/path/file_txt"));
}

TEST(DecodeFixFilenameTest, usesWindowsBasename)
{
    EXPECT_EQ("file_txt", decode_fix_filename("C:\\tmp\\path\\file_txt"));
}

TEST(DecodeFixFilenameTest, filtersBadCharactersFromBasename)
{
    EXPECT_EQ("abc", decode_fix_filename("/tmp/a b;c"));
}

TEST(DecodeFixFilenameTest, fallsBackForEmptyBasename)
{
    EXPECT_EQ("x", decode_fix_filename("/tmp/path/"));
}

TEST(DecodeFixFilenameTest, fallsBackForBadBasename)
{
    EXPECT_EQ("x", decode_fix_filename("/tmp/.."));
}
