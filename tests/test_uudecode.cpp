// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/uudecode.h>

#include <trn/file-contents.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class UudecodeTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_current_path = fs::current_path();
        m_old_decode_filename = g_decode_filename;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
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
    }

    void TearDown() override
    {
        uudecode(nullptr, DECODE_DONE);
        if (m_input != nullptr)
        {
            std::fclose(m_input);
        }
        g_decode_filename = m_old_decode_filename;

        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);
    }

    DecodeState decode_text(const char *text)
    {
        std::fputs(text, m_input);
        std::rewind(m_input);
        testing::internal::CaptureStdout();
        const DecodeState state = uudecode(m_input, DECODE_START);
        testing::internal::GetCapturedStdout();
        return state;
    }

    std::FILE  *m_input{};
    fs::path    m_output_dir;
    fs::path    m_old_current_path;
    std::string m_old_decode_filename;
};

} // namespace

TEST(UuePrescanTest, beginLineInitializesSinglePartDecode)
{
    std::string filename{"old.bin"};
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("begin 644 archive.bin", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_TRUE(filename.empty());
    EXPECT_EQ(1, part);
    EXPECT_EQ(0, total);
}

TEST(UuePrescanTest, sectionOfFileLineFindsFilenameAndPart)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("section 2 of 10 of file archive.bin trailing", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(0, total);
}

TEST(UuePrescanTest, postLineFindsFilenameAndPartCount)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("POST V1.0 archive.bin (Part 2/5)", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, fileLineFindsFilenameAndPartCount)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("File: archive.bin -- part 2 of 5 -- trailing", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, bracketedSectionLineFindsFilenameAndPartCount)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("[Section: 2/5 File: archive.bin trailing", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST(UuePrescanTest, fileNameHeaderStoresFilename)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("X-File-Name: archive.bin trailing", filename, &part, &total);

    EXPECT_EQ(0, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(-1, part);
    EXPECT_EQ(-1, total);
}

TEST(UuePrescanTest, partHeaderStoresPart)
{
    std::string filename;
    int         part = -1;
    int         total = -1;

    const int result = uue_prescan("X-Part: 2", filename, &part, &total);

    EXPECT_EQ(0, result);
    EXPECT_TRUE(filename.empty());
    EXPECT_EQ(2, part);
    EXPECT_EQ(-1, total);
}

TEST(UuePrescanTest, beginMarkerStartsKnownMultipartDecode)
{
    std::string filename{"archive.bin"};
    int         part = 2;
    int         total = 5;

    const int result = uue_prescan("BEGIN", filename, &part, &total);

    EXPECT_EQ(1, result);
    EXPECT_EQ("archive.bin", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST_F(UudecodeTest, decodesSavedShortLineBeforeEnd)
{
    const DecodeState state = decode_text("begin 644 cats_txt\n"
                                          "#0V%T\n"
                                          "`\n"
                                          "end\n");

    EXPECT_EQ(DECODE_MAYBE_DONE, state);
    EXPECT_EQ("Cat", file_contents(m_output_dir / "cats_txt"));
    EXPECT_EQ("cats_txt", g_decode_filename);
}

TEST_F(UudecodeTest, decodesLinesWithCarriageReturns)
{
    const DecodeState state = decode_text("begin 644 cats_txt\r\n"
                                          "#0V%T\r\n"
                                          "`\r\n"
                                          "end\r\n");

    EXPECT_EQ(DECODE_MAYBE_DONE, state);
    EXPECT_EQ("Cat", file_contents(m_output_dir / "cats_txt"));
    EXPECT_EQ("cats_txt", g_decode_filename);
}

TEST_F(UudecodeTest, returnsInactiveWhenDataIsInterruptedBeforeEnd)
{
    const DecodeState state = decode_text("begin 644 cats_txt\n"
                                          "#0V%T\n"
                                          "bogus\n");

    EXPECT_EQ(DECODE_INACTIVE, state);
    uudecode(nullptr, DECODE_DONE);
    EXPECT_TRUE(fs::exists(m_output_dir / "cats_txt"));
    EXPECT_TRUE(file_contents(m_output_dir / "cats_txt").empty());
}
