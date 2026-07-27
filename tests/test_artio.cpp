// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/art.h>
#include <trn/artio.h>

#include <trn/artstate.h>
#include <trn/datasrc.h>
#include <trn/head.h>
#include <trn/mime.h>
#include <trn/terminal.h>

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
        m_old_art_buf = g_art_buf;
        m_old_art_buf_pos = g_art_buf_pos;
        m_old_art_buf_seek = g_art_buf_seek;
        m_old_art_buf_len = g_art_buf_len;
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

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        fs::create_directories(m_output_dir, error);

        g_art_fp = nullptr;
        g_art_buf = nullptr;
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

        g_art_fp = m_old_art_fp;
        g_art_buf = m_old_art_buf;
        g_art_buf_pos = m_old_art_buf_pos;
        g_art_buf_seek = m_old_art_buf_seek;
        g_art_buf_len = m_old_art_buf_len;
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

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    void open_article_text(std::string_view text)
    {
        const fs::path article_file = m_output_dir / "article";
        {
            std::ofstream output{article_file};
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
        m_mime_section.mime_clear_struct();
    }

    DataSource      m_data_source{};
    MimeSection     m_mime_section{};
    fs::path        m_output_dir;
    std::FILE      *m_old_art_fp{};
    char           *m_old_art_buf{};
    ArticlePosition m_old_art_buf_pos{};
    ArticlePosition m_old_art_buf_seek{};
    ArticlePosition m_old_art_buf_len{};
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
};

} // namespace

TEST_F(ArticleIoTest, wordWrapCompactsIndentedContinuation)
{
    open_article_text("alpha beta gamma delta     epsilon zeta eta\n");

    char *first = read_art_buf(false);

    ASSERT_NE(nullptr, first);
    EXPECT_EQ("alpha beta gamma delta", std::string_view(first, 22));
    EXPECT_EQ(WRAPPED_NL, first[22]);
    EXPECT_EQ(23, g_art_buf_pos.value_of());

    char *second = read_art_buf(false);

    ASSERT_NE(nullptr, second);
    EXPECT_STREQ("epsilon zeta eta\n", second);
}

TEST_F(ArticleIoTest, readArtBufAddsNewlineToFinalLine)
{
    open_article_text("alpha");

    char *first = read_art_buf(false);

    ASSERT_NE(nullptr, first);
    EXPECT_STREQ("alpha\n", first);
    EXPECT_EQ(6, g_art_buf_pos.value_of());
    EXPECT_EQ(6, g_art_buf_len.value_of());

    EXPECT_EQ(nullptr, read_art_buf(false));
}

TEST_F(ArticleIoTest, readArtBufWithoutHidingReturnsArticleLine)
{
    g_do_hiding = false;
    open_article_text("alpha\nbeta\n");

    char *first = read_art_buf(false);

    ASSERT_NE(nullptr, first);
    EXPECT_STREQ("alpha\n", first);

    char *second = read_art_buf(false);

    ASSERT_NE(nullptr, second);
    EXPECT_STREQ("beta\n", second);
}

TEST_F(ArticleIoTest, readArtBufWithoutHidingReturnsLongArticleLine)
{
    g_do_hiding = false;
    const std::string line(static_cast<std::size_t>(LINE_BUF_LEN) + 20, 'x');
    open_article_text(line + "\n");

    char *first = read_art_buf(false);

    ASSERT_NE(nullptr, first);
    EXPECT_EQ(line + "\n", std::string{first});
}

TEST_F(ArticleIoTest, readArtBufDecodesBase64MimeText)
{
    m_mime_section.m_encoding = MENCODE_BASE64;
    g_mime_section = &m_mime_section;
    g_mime_state = TEXT_MIME;
    g_is_mime = true;
    open_article_text("SGVsbG8=\n");

    char *first = read_art_buf(false);

    ASSERT_NE(nullptr, first);
    EXPECT_STREQ("Hello\n", first);
    EXPECT_EQ(nullptr, read_art_buf(false));
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

    char *first = read_art_buf(false);

    ASSERT_NE(nullptr, first);
    EXPECT_EQ('\002', first[-1]);
    EXPECT_STREQ("part separator\n", first);
}
