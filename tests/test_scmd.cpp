// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scmd-internal.h>

#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/ngdata.h>
#include <trn/sadesc.h>
#include <trn/scan.h>
#include <trn/scanart.h>
#include <trn/score.h>
#include <trn/smisc.h>
#include <trn/Subject.h>

#include <gtest/gtest.h>

namespace
{

class ScanCommandTest : public testing::Test
{
protected:
    void SetUp() override
    {
        g_article_list.clear();
        g_char_subst = g_charsets.c_str();
        g_last_art = ArticleNum{1};
        g_s_cur_type = S_ART;
        g_s_status_cols = 3;
        g_sa_ents.assign(2, ScanArticleEntryData{});

        g_sa_ents[1].artnum = ArticleNum{1};
        Article *article = article_ptr(ArticleNum{1});
        article->m_from = m_author;
        article->m_flags = AF_EXISTS | AF_UNREAD;

        g_sc_initialized = false;
        g_sa_mode_desc_art_num = false;
        g_sa_mode_desc_author = true;
        g_sa_mode_desc_score = false;
        g_sa_mode_desc_thread_count = false;
        g_sa_mode_desc_subject = false;
        g_sa_mode_desc_summary = false;
        g_sa_mode_desc_keyw = false;
    }

    void TearDown() override
    {
        g_article_list.clear();
        g_sa_ents.clear();
        g_char_subst = nullptr;
        g_last_art = ArticleNum{};
        g_s_cur_type = S_NONE;
        g_s_status_cols = 0;
    }

    char                 m_author[64]{"Casey Mixed <case@example.com>"};
    Subject              m_subject{};
};

} // namespace

TEST_F(ScanCommandTest, descriptionSearchMatchesIgnoringCase)
{
    EXPECT_TRUE(scmd_match_description_for_test(1, "mixed"));
    EXPECT_TRUE(scmd_match_description_for_test(1, "CASEY"));
    EXPECT_FALSE(scmd_match_description_for_test(1, "missing"));
}

TEST_F(ScanCommandTest, subjectDescriptionShortensReplyPrefix)
{
    Article *article = article_ptr(ArticleNum{1});
    article->m_subj = &m_subject;
    article->m_flags |= AF_HAS_RE;
    m_subject.m_str = "Re: Replied Subject";

    EXPECT_EQ("> Replied Subject", std::string{sa_desc_subject(1)});
}

TEST_F(ScanCommandTest, articleStatusShowsUnreadSelectedAndMarked)
{
    sa_select1(1);
    sa_mark(1);

    EXPECT_EQ("+*x", sa_get_stat_chars(1, 1));
}

TEST_F(ScanCommandTest, articleStatusShowsReadArticle)
{
    article_ptr(ArticleNum{1})->m_flags = AF_EXISTS;

    EXPECT_EQ("-..", sa_get_stat_chars(1, 1));
}

TEST_F(ScanCommandTest, articleStatusUsesBlanksForUnknownStatusLine)
{
    EXPECT_EQ("   ", sa_get_stat_chars(1, 2));
}

TEST_F(ScanCommandTest, scanStatusHidesStatusWhenColumnWidthIsZero)
{
    g_s_status_cols = 0;

    EXPECT_EQ("", s_get_statchars(1, 1));
}
