// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/head.h>

#include <trn/art.h>
#include <trn/artio.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/ngdata.h>
#include <trn/Subject.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class HeaderLineTypeLookupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        head_init();
        g_head_buf.clear();
        g_header_type[CUSTOM_LINE].name.clear();
        g_header_type[CUSTOM_LINE].length = 0;
    }

    void TearDown() override
    {
        head_final();
    }
};

class HeaderParseTest : public testing::Test
{
protected:
    void SetUp() override
    {
        std::copy(std::begin(g_header_type), std::end(g_header_type), m_old_header_type.begin());
        m_old_article_list = std::move(g_article_list);
        m_old_data_source = g_data_source;
        m_old_last_art = g_last_art;
        m_old_parsed_art = g_parsed_art;
        m_old_threaded_group = g_threaded_group;
        m_old_art_fp = g_art_fp;
        m_old_open_art = g_open_art;
        m_old_art_buf = g_art_buf;
        m_old_art_buf_pos = g_art_buf_pos;
        m_old_art_buf_seek = g_art_buf_seek;
        m_old_art_buf_len = g_art_buf_len;
        m_old_raw_art_size = g_raw_art_size;
        m_old_art_size = g_art_size;
        m_old_current_path = fs::current_path();

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        fs::create_directories(m_output_dir, error);
        fs::current_path(m_output_dir, error);

        head_init();
        g_art_fp = nullptr;
        g_art_buf = nullptr;
        art_io_init();
        g_article_list.clear();
        g_data_source = &m_data_source;
        g_last_art = TEST_ARTICLE;
        g_parsed_art = ArticleNum{};
        g_threaded_group = false;

        Article *article = article_ptr(TEST_ARTICLE);
        article->m_flags = AF_EXISTS;
    }

    void TearDown() override
    {
        if (g_art_fp != nullptr)
        {
            std::fclose(g_art_fp);
            g_art_fp = nullptr;
        }
        art_io_final();
        head_final();

        g_article_list.clear();
        g_article_list = std::move(m_old_article_list);
        std::copy(m_old_header_type.begin(), m_old_header_type.end(), std::begin(g_header_type));
        g_data_source = m_old_data_source;
        g_last_art = m_old_last_art;
        g_parsed_art = m_old_parsed_art;
        g_threaded_group = m_old_threaded_group;
        g_art_fp = m_old_art_fp;
        g_open_art = m_old_open_art;
        g_art_buf = m_old_art_buf;
        g_art_buf_pos = m_old_art_buf_pos;
        g_art_buf_seek = m_old_art_buf_seek;
        g_art_buf_len = m_old_art_buf_len;
        g_raw_art_size = m_old_raw_art_size;
        g_art_size = m_old_art_size;

        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);
    }

    void write_article(const std::string &text)
    {
        std::ofstream output{"1"};
        output << text;
        output.close();
        g_raw_art_size = ArticlePosition{static_cast<long>(text.size())};
        g_art_size = g_raw_art_size;
    }

    static constexpr ArticleNum TEST_ARTICLE{1};

    DataSource                        m_data_source{};
    Subject                           m_subject{};
    std::map<ArticleNum, Article>     m_old_article_list;
    std::array<HeaderType, HEAD_LAST> m_old_header_type;
    DataSource                       *m_old_data_source{};
    ArticleNum                        m_old_last_art{};
    ArticleNum                        m_old_parsed_art{};
    bool                              m_old_threaded_group{};
    std::FILE                        *m_old_art_fp{};
    ArticleNum                        m_old_open_art{};
    char                             *m_old_art_buf{};
    ArticlePosition                   m_old_art_buf_pos{};
    ArticlePosition                   m_old_art_buf_seek{};
    ArticlePosition                   m_old_art_buf_len{};
    ArticlePosition                   m_old_raw_art_size{};
    ArticlePosition                   m_old_art_size{};
    fs::path                          m_old_current_path;
    fs::path                          m_output_dir;
};

} // namespace

TEST_F(HeaderLineTypeLookupTest, findsKnownHeaderIgnoringCase)
{
    EXPECT_EQ(SUBJ_LINE, set_line_type("Subject"));
    EXPECT_EQ(SUBJ_LINE, set_line_type("SUBJECT"));
}

TEST_F(HeaderLineTypeLookupTest, rejectsHeaderWithWhitespaceBeforeColon)
{
    EXPECT_EQ(SOME_LINE, set_line_type("Subject "));
}

TEST_F(HeaderLineTypeLookupTest, recordsCustomHeaderNameInLowerCase)
{
    EXPECT_EQ(CUSTOM_LINE, get_header_num("X-Strange-Thing"));
    EXPECT_EQ("x-strange-thing", g_header_type[CUSTOM_LINE].name);
}

TEST_F(HeaderParseTest, preservesHeaderOffsetsAcrossBufferGrowth)
{
    const std::string subject(LINE_BUF_LEN * 9, 'x');
    const std::string article = "From: writer@example.test\nSubject: " + subject + "\nX-Test: value\n\nbody\n";
    m_subject.m_str = "Re: " + subject;
    article_ptr(TEST_ARTICLE)->m_flags |= AF_CACHED;
    article_ptr(TEST_ARTICLE)->m_subj = &m_subject;
    write_article(article);

    ASSERT_TRUE(parse_header(TEST_ARTICLE));

    EXPECT_EQ(TEST_ARTICLE, g_parsed_art);
    EXPECT_EQ('F', g_head_buf.front());
    EXPECT_EQ("writer@example.test", fetch_lines(TEST_ARTICLE, FROM_LINE));
    EXPECT_EQ(subject, fetch_lines(TEST_ARTICLE, SUBJ_LINE));
}
