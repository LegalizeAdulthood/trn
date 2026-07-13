// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/rt-ov.h>

#include <config/common.h>
#include <test_config.h>
#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/list.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rt-util.h>
#include <trn/trn.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

constexpr ArticleNum TEST_ARTICLE_NUM{1};

class OverviewTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_data_source = g_data_source;
        m_old_article_list = g_article_list;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_first_cached = g_first_cached;
        m_old_last_cached = g_last_cached;
        m_old_cached_all_in_range = g_cached_all_in_range;
        m_old_verbose = g_verbose;
        m_old_spin_todo = g_spin_todo;
        m_old_spin_estimate = g_spin_estimate;
        m_old_int_count = g_int_count;
        m_old_curr_artp = g_curr_artp;
        m_old_sentinel_art_ptr = g_sentinel_art_ptr;

        std::error_code error;
        fs::remove_all(m_overview_dir, error);
        m_overview_dir_text = m_overview_dir.generic_string();

        g_data_source = &m_data_source;
        m_data_source.m_over_dir = m_overview_dir_text.data();
        for (int i = 0; i < OV_MAX_FIELDS; ++i)
        {
            m_data_source.m_field_num[i] = static_cast<OverviewFieldNum>(i);
            m_data_source.m_field_flags[i] = FF_HAS_FIELD;
        }

        g_article_list = new_list(TEST_ARTICLE_NUM.value_of(), TEST_ARTICLE_NUM.value_of(), sizeof(Article), 1,
                                  LF_ZERO_MEM, nullptr);
        Article *article = article_ptr(TEST_ARTICLE_NUM);
        article->m_num = TEST_ARTICLE_NUM;
        article->m_flags = AF_EXISTS;

        g_newsgroup_name = "comp.lang.apl";
        g_abs_first = TEST_ARTICLE_NUM;
        g_first_art = TEST_ARTICLE_NUM;
        g_last_art = TEST_ARTICLE_NUM;
        g_first_cached = TEST_ARTICLE_NUM;
        g_last_cached = ArticleNum{};
        g_cached_all_in_range = false;
        g_verbose = false;
        g_spin_todo = 1;
        g_spin_estimate = 1;
        g_int_count = 0;
        g_curr_artp = nullptr;
        g_sentinel_art_ptr = nullptr;
    }

    void TearDown() override
    {
        ov_close();
        delete_list(g_article_list);
        g_data_source = m_old_data_source;
        g_article_list = m_old_article_list;
        g_newsgroup_name = m_old_newsgroup_name;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_first_cached = m_old_first_cached;
        g_last_cached = m_old_last_cached;
        g_cached_all_in_range = m_old_cached_all_in_range;
        g_verbose = m_old_verbose;
        g_spin_todo = m_old_spin_todo;
        g_spin_estimate = m_old_spin_estimate;
        g_int_count = m_old_int_count;
        g_curr_artp = m_old_curr_artp;
        g_sentinel_art_ptr = m_old_sentinel_art_ptr;

        std::error_code error;
        fs::remove_all(m_overview_dir, error);
    }

    fs::path overview_file() const
    {
        return fs::path{m_overview_dir_text + "/comp/lang/apl" OV_FILE_NAME};
    }

    DataSource  m_data_source{};
    fs::path    m_overview_dir{TRN_TEST_TMP_DIR "/overview-local"};
    std::string m_overview_dir_text;

    DataSource *m_old_data_source{};
    List       *m_old_article_list{};
    std::string m_old_newsgroup_name;
    ArticleNum  m_old_abs_first{};
    ArticleNum  m_old_first_art{};
    ArticleNum  m_old_last_art{};
    ArticleNum  m_old_first_cached{};
    ArticleNum  m_old_last_cached{};
    bool        m_old_cached_all_in_range{};
    bool        m_old_verbose{};
    long        m_old_spin_todo{};
    long        m_old_spin_estimate{};
    char        m_old_int_count{};
    Article    *m_old_curr_artp{};
    Article    *m_old_sentinel_art_ptr{};
};

} // namespace

TEST_F(OverviewTest, localOverviewPathUsesGroupDirectory)
{
    const fs::path file{overview_file()};
    fs::create_directories(file.parent_path());
    std::ofstream{file};

    EXPECT_TRUE(ov_data(TEST_ARTICLE_NUM, TEST_ARTICLE_NUM, false));
    EXPECT_NE(nullptr, m_data_source.m_ov_in);
}
