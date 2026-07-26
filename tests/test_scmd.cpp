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
#include <trn/terminal.h>

#include <gtest/gtest.h>

#include <string>

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
        m_old_s_ptr_page_line = g_s_ptr_page_line;
        m_old_s_bot_ent = g_s_bot_ent;
        m_old_s_ref_bot = g_s_ref_bot;
        m_old_s_bot_lines = g_s_bot_lines;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_bc = g_tc_BC;
        m_old_tc_ce = g_tc_CE;
        m_old_tc_cm = g_tc_CM;
        m_old_erase_char = g_erase_char;
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

        g_s_ptr_page_line = 0;
        g_s_bot_ent = 11;
        g_s_ref_bot = false;
        g_s_bot_lines = 1;
        g_tc_LINES = 24;
        g_tc_BC = "\b";
        g_tc_CE = "";
        g_tc_CM = "\033[%d;%dH";
        g_erase_char = '\b';
    }

    void TearDown() override
    {
        g_s_ptr_page_line = m_old_s_ptr_page_line;
        g_s_bot_ent = m_old_s_bot_ent;
        g_s_ref_bot = m_old_s_ref_bot;
        g_s_bot_lines = m_old_s_bot_lines;
        g_tc_LINES = m_old_tc_lines;
        g_tc_BC = m_old_tc_bc;
        g_tc_CE = m_old_tc_ce;
        g_tc_CM = m_old_tc_cm;
        g_erase_char = m_old_erase_char;
        g_article_list.clear();
        g_sa_ents.clear();
        g_char_subst = nullptr;
        g_last_art = ArticleNum{};
        g_s_cur_type = S_NONE;
        g_s_status_cols = 0;
    }

    char                 m_author[64]{"Casey Mixed <case@example.com>"};
    Subject              m_subject{};
    short                m_old_s_ptr_page_line{};
    long                 m_old_s_bot_ent{};
    bool                 m_old_s_ref_bot{};
    short                m_old_s_bot_lines{};
    int                  m_old_tc_lines{};
    const char          *m_old_tc_bc{};
    const char          *m_old_tc_ce{};
    const char          *m_old_tc_cm{};
    char                 m_old_erase_char{};
};

} // namespace

TEST_F(ScanCommandTest, descriptionSearchMatchesIgnoringCase)
{
    EXPECT_TRUE(scmd_match_description_for_test(1, "mixed"));
    EXPECT_TRUE(scmd_match_description_for_test(1, "CASEY"));
    EXPECT_FALSE(scmd_match_description_for_test(1, "missing"));
}

TEST_F(ScanCommandTest, jumpNumberReadsSecondDigit)
{
    push_char('2');

    testing::internal::CaptureStdout();
    scmd_jump_num_for_test('1');
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(11, g_s_ptr_page_line);
}

TEST_F(ScanCommandTest, jumpNumberPushesBackCommandAfterFirstDigit)
{
    push_char('x');

    testing::internal::CaptureStdout();
    scmd_jump_num_for_test('1');
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(0, g_s_ptr_page_line);
    const std::string command = get_cmd();
    ASSERT_FALSE(command.empty());
    EXPECT_EQ('x', command.front());
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
