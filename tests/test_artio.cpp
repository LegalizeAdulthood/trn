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

    DataSource      m_data_source{};
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
