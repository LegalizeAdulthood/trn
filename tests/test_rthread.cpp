// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/cache.h>
#include <trn/ngdata.h>
#include <trn/rt-page.h>
#include <trn/rt-select.h>
#include <trn/rthread.h>

#include <gtest/gtest.h>

#include <ctime>
#include <map>
#include <utility>
#include <vector>

namespace
{

class ArticlePointerListTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_article_list = std::move(g_article_list);
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_obj_count = g_obj_count;
        m_old_sel_rereading = g_sel_rereading;
        m_old_sel_sort = g_sel_sort;
        m_old_sel_direction = g_sel_direction;
        m_old_sel_page_app = g_sel_page_app;
        m_old_art_ptr_list = std::move(g_art_ptr_list);
        m_old_art_ptr = g_art_ptr;

        g_article_list.clear();
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{3};
        g_obj_count = ArticleNum{3};
        g_sel_rereading = false;
        g_sel_sort = SS_DATE;
        g_sel_direction = 1;
        g_sel_page_app = reinterpret_cast<Article **>(1);
        g_art_ptr_list.clear();
        g_art_ptr = nullptr;
    }

    void TearDown() override
    {
        g_art_ptr_list = std::move(m_old_art_ptr_list);
        g_art_ptr = m_old_art_ptr;
        g_sel_page_app = m_old_sel_page_app;
        g_sel_direction = m_old_sel_direction;
        g_sel_sort = m_old_sel_sort;
        g_sel_rereading = m_old_sel_rereading;
        g_obj_count = m_old_obj_count;
        g_last_art = m_old_last_art;
        g_first_art = m_old_first_art;
        g_abs_first = m_old_abs_first;
        g_article_list = std::move(m_old_article_list);
    }

    void add_article(ArticleNum num, std::time_t date)
    {
        Article *article = article_ptr(num);
        article->m_flags = AF_EXISTS | AF_UNREAD;
        article->m_date = date;
    }

private:
    std::map<ArticleNum, Article> m_old_article_list;
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    ArticleNum                    m_old_obj_count{};
    bool                          m_old_sel_rereading{};
    SelectionSortMode             m_old_sel_sort{};
    int                           m_old_sel_direction{};
    Article                     **m_old_sel_page_app{};
    std::vector<Article *>        m_old_art_ptr_list;
    Article                     **m_old_art_ptr{};
};

} // namespace

TEST_F(ArticlePointerListTest, sortArticlesBuildsDateOrderedPointerList)
{
    add_article(ArticleNum{1}, 30);
    add_article(ArticleNum{2}, 10);
    add_article(ArticleNum{3}, 20);

    sort_articles();

    ASSERT_EQ(3U, g_art_ptr_list.size());
    EXPECT_EQ(ArticleNum{2}, g_art_ptr_list[0]->article_num());
    EXPECT_EQ(ArticleNum{3}, g_art_ptr_list[1]->article_num());
    EXPECT_EQ(ArticleNum{1}, g_art_ptr_list[2]->article_num());
    EXPECT_EQ(nullptr, g_sel_page_app);
}
