// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/decode.h>

#include <config/common.h>

#include <trn/Article.h>
#include <trn/artio.h>
#include <trn/artstate.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/file-contents.h>
#include <trn/head.h>
#include <trn/mime-internal.h>
#include <trn/mime.h>
#include <trn/Subject.h>
#include <trn/util.h>
#include <trn/yenc.h>
#include <util/env.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class DecodeSubjectTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_article_list = std::move(g_article_list);
        m_old_data_source = g_data_source;
        m_old_last_art = g_last_art;
        m_old_parsed_art = g_parsed_art;

        g_article_list.clear();
        g_data_source = &m_data_source;
        g_last_art = s_article_num;
        g_parsed_art = ArticleNum{};

        m_data_source.m_flags = DF_NONE;
    }

    void TearDown() override
    {
        g_article_list = std::move(m_old_article_list);
        g_data_source = m_old_data_source;
        g_last_art = m_old_last_art;
        g_parsed_art = m_old_parsed_art;
    }

    void cache_subject(std::string_view subject)
    {
        Article *article = article_ptr(s_article_num);
        article->m_flags |= AF_EXISTS;
        article->m_subj = &m_subject;
        m_subject.m_str = "Re: ";
        m_subject.m_str += subject;
    }

    static constexpr ArticleNum s_article_num{1};

    DataSource                    m_data_source{};
    Subject                       m_subject{};
    std::map<ArticleNum, Article> m_old_article_list;
    DataSource                   *m_old_data_source{};
    ArticleNum                    m_old_last_art{};
    ArticleNum                    m_old_parsed_art{};
};

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
        g_mime_getc_line = {};
        g_is_mime = false;
        g_no_wait_fork = true;

        m_section.m_filename = "payload_bin";
        m_section.m_type = APP_MIME;
        m_section.m_encoding = MENCODE_NONE;
        m_section.m_part = 1;
        m_section.m_total = 1;

        mime_set_executor(
            [this](std::string_view, std::string_view command)
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

    fs::path find_file_named(const char *name) const
    {
        for (const fs::directory_entry &entry : fs::recursive_directory_iterator(m_output_dir))
        {
            if (entry.is_regular_file() && entry.path().filename() == name)
            {
                return entry.path();
            }
        }
        return {};
    }

    void set_article_text(std::string_view text)
    {
        std::FILE *input = std::tmpfile();
        ASSERT_NE(nullptr, input);
        ASSERT_EQ(text.size(), std::fwrite(text.data(), 1, text.size(), input));
        std::rewind(input);

        if (m_input != nullptr)
        {
            std::fclose(m_input);
        }
        m_input = input;
        g_art_fp = m_input;
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
    std::string_view m_old_mime_getc_line;
    bool         m_old_is_mime{};
    bool         m_old_no_wait_fork{};
    std::string  m_old_decode_filename;
};

} // namespace

TEST_F(DecodeSubjectTest, extractsFilenameAndSlashPartTotal)
{
    cache_subject("archive.zip (2/5)");
    int part = 0;
    int total = 0;

    const std::string filename = decode_subject(s_article_num, &part, &total);

    EXPECT_EQ("archive.zip", filename);
    EXPECT_EQ(2, part);
    EXPECT_EQ(5, total);
}

TEST_F(DecodeSubjectTest, skipsRepostAndVolumeBeforeLaterDottedFilename)
{
    cache_subject("Repost: v1: plain words archive.tar.gz part 3 of 4");
    int part = 0;
    int total = 0;

    const std::string filename = decode_subject(s_article_num, &part, &total);

    EXPECT_EQ("archive.tar.gz", filename);
    EXPECT_EQ(3, part);
    EXPECT_EQ(4, total);
}

TEST_F(DecodeSubjectTest, returnsEmptyWhenPartExceedsTotal)
{
    cache_subject("archive.zip (7/3)");
    int part = 0;
    int total = 0;

    const std::string filename = decode_subject(s_article_num, &part, &total);

    EXPECT_TRUE(filename.empty());
    EXPECT_EQ(-1, part);
    EXPECT_EQ(0, total);
}

TEST_F(DecodePieceDirectoryTest, createsUsesAndRemovesPieceDirectory)
{
    MimeCapEntry mime_cap;
    mime_cap.command = "viewer %s";
    testing::internal::CaptureStdout();

    const bool        result = decode_piece(&mime_cap, {});
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_EQ("Decoding payload_bin", output);
    EXPECT_EQ("viewer payload_bin", m_command);
    EXPECT_EQ("payload_bin", m_piece_dir.filename().generic_string());
    EXPECT_TRUE(m_output_existed);
    EXPECT_FALSE(fs::exists(m_piece_dir));
}

TEST_F(DecodePieceDirectoryTest, savesMultipartPieceInPieceDirectory)
{
    m_section.m_part = 1;
    m_section.m_total = 2;
    set_article_text("line one\nline two\n");

    testing::internal::CaptureStdout();
    const bool        result = decode_piece(nullptr, {});
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_EQ("Saving part 1 of 2 payload_bin", output);
    const fs::path piece = find_file_named("1");
    ASSERT_FALSE(piece.empty());
    EXPECT_EQ("line one\nline two\n", file_contents(piece));
}

TEST_F(DecodePieceDirectoryTest, completesMultipartDecodeUsingSavedTotal)
{
    m_section.m_part = 2;
    m_section.m_total = 0;
    set_article_text("second\nend payload\n");

    testing::internal::CaptureStdout();
    const bool        last_part_result = decode_piece(nullptr, {});
    const std::string last_part_output = testing::internal::GetCapturedStdout();

    ASSERT_TRUE(last_part_result);
    EXPECT_EQ("Saving part 2 payload_bin", last_part_output);
    const fs::path total_file = find_file_named("CT");
    ASSERT_FALSE(total_file.empty());
    EXPECT_EQ("2\n", file_contents(total_file));

    std::ofstream{total_file} << "2 extra\n";
    m_section.m_part = 1;
    set_article_text("first\n");
    std::error_code error;
    fs::current_path(m_output_dir, error);
    ASSERT_FALSE(error) << error.message();

    testing::internal::CaptureStdout();
    const bool        result = decode_piece(nullptr, {});
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_TRUE(result);
    EXPECT_EQ("Saving part 1 payload_binDecoding payload_bin", output);
    EXPECT_EQ("first\nsecond\nend payload\n", file_contents(m_output_dir / "payload_bin"));
    EXPECT_FALSE(fs::exists(total_file));
}

TEST(DecodeFixFilenameTest, usesUnixBasename)
{
    EXPECT_EQ("file_txt", decode_fix_filename("/tmp/path/file_txt"));
}

class YencExtractionTest : public DecodePieceDirectoryTest
{
protected:
    void SetUp() override
    {
        DecodePieceDirectoryTest::SetUp();
        fs::current_path(m_output_dir);
    }

    bool extract(std::string_view text)
    {
        const auto header = yenc_prescan(text.substr(0, text.find('\n')));
        EXPECT_TRUE(header);
        if (!header)
        {
            return false;
        }
        m_section.m_filename = header->filename;
        m_section.m_encoding = MENCODE_YENC;
        m_section.m_part = header->part;
        m_section.m_total = header->total;
        set_article_text(text);
        testing::internal::CaptureStdout();
        const bool result = decode_piece(nullptr, {});
        testing::internal::GetCapturedStdout();
        return result;
    }
};

TEST_F(YencExtractionTest, singlePartUsesPlatformFilenamePolicy)
{
    ASSERT_TRUE(extract("=ybegin line=128 size=3 name=../../safe name?.bin\n\x6D\x8B\x9E\n"
                        "=yend size=3 crc32=a6130548\n"));
    EXPECT_EQ("safename.bin", g_decode_filename);
    EXPECT_EQ("Cat", file_contents(m_output_dir / "safename.bin"));
}

TEST_F(YencExtractionTest, assemblesOutOfOrderMultipartData)
{
    ASSERT_TRUE(extract("=ybegin part=2 total=2 line=128 size=11 name=hello.bin\n=ypart begin=7 end=11\n"
                        "\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143 crc32=8bd69e52\n"));
    EXPECT_FALSE(fs::exists(m_output_dir / "hello.bin"));
    ASSERT_TRUE(extract("=ybegin part=1 total=2 line=128 size=11 name=hello.bin\n=ypart begin=1 end=6\n"
                        "r\x8F\x96\x96\x99J\n=yend size=6 part=1 pcrc32=ea2dfcc0\n"));
    EXPECT_EQ("Hello world", file_contents(m_output_dir / "hello.bin"));
    EXPECT_TRUE(find_file_named("1").empty());
    EXPECT_TRUE(find_file_named("2").empty());
}

TEST_F(YencExtractionTest, infersMissingTotalFromFinalRange)
{
    ASSERT_TRUE(extract("=ybegin part=2 line=128 size=11 name=hello.bin\n=ypart begin=7 end=11\n"
                        "\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143\n"));
    ASSERT_TRUE(extract("=ybegin part=1 line=128 size=11 name=hello.bin\n=ypart begin=1 end=6\n"
                        "r\x8F\x96\x96\x99J\n=yend size=6 part=1 pcrc32=ea2dfcc0\n"));
    EXPECT_EQ("Hello world", file_contents(m_output_dir / "hello.bin"));
}

TEST_F(YencExtractionTest, invalidCrcPreservesExistingOutput)
{
    std::ofstream{m_output_dir / "cat.bin"} << "existing";
    EXPECT_FALSE(extract("=ybegin line=128 size=3 name=cat.bin\n\x6D\x8B\x9E\n"
                         "=yend size=3 crc32=00000000\n"));
    EXPECT_EQ("existing", file_contents(m_output_dir / "cat.bin"));
    EXPECT_FALSE(g_msg.empty());
}

TEST_F(YencExtractionTest, invalidWholeCrcDoesNotPublishMultipartOutput)
{
    ASSERT_TRUE(extract("=ybegin part=2 total=2 line=128 size=11 name=hello.bin\n=ypart begin=7 end=11\n"
                        "\xA1\x99\x9C\x96\x8E\n=yend size=5 part=2 pcrc32=3a771143 crc32=00000000\n"));
    EXPECT_FALSE(extract("=ybegin part=1 total=2 line=128 size=11 name=hello.bin\n=ypart begin=1 end=6\n"
                         "r\x8F\x96\x96\x99J\n=yend size=6 part=1 pcrc32=ea2dfcc0\n"));
    EXPECT_FALSE(fs::exists(m_output_dir / "hello.bin"));
    EXPECT_FALSE(g_msg.empty());
}

TEST(DecodeFixFilenameTest, usesWindowsBasename)
{
    EXPECT_EQ("file_txt", decode_fix_filename("C:\\tmp\\path\\file_txt"));
}

TEST(DecodeFixFilenameTest, preservesFileExtension)
{
    EXPECT_EQ("file.txt", decode_fix_filename("file.txt"));
}

TEST(DecodeFixFilenameTest, filtersBadCharactersFromBasename)
{
    EXPECT_EQ("abc", decode_fix_filename("/tmp/a<b>c"));
}

TEST(DecodeFixFilenameTest, fallsBackForEmptyBasename)
{
    EXPECT_EQ("x", decode_fix_filename("/tmp/path/"));
}

TEST(DecodeFixFilenameTest, fallsBackForBadBasename)
{
    EXPECT_EQ("x", decode_fix_filename("/tmp/.."));
}

#ifdef USE_WINDOWS_FILENAME_RULES
TEST(DecodeFixFilenameTest, preservesWindowsLongFilenameCharacters)
{
    EXPECT_EQ("a!#$%&'()+,;=@[]^_`{}~.txt", decode_fix_filename("a!#$%&'()+,;=@[]^_`{}~.txt"));
}

TEST(DecodeFixFilenameTest, filtersWindowsForbiddenFilenameCharacters)
{
    EXPECT_EQ("ab", decode_fix_filename("a<>:\"|?* \tb"));
}

TEST(DecodeFixFilenameTest, stripsWindowsTrailingDotsAndSpaces)
{
    EXPECT_EQ("file", decode_fix_filename("file. "));
}

TEST(DecodeFixFilenameTest, rejectsWindowsDeviceNameWithExtension)
{
    EXPECT_EQ("x", decode_fix_filename("con.txt"));
}
#endif
