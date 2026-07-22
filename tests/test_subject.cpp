// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/final.h>
#include <trn/kfile.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rthread.h>
#include <trn/Subject.h>
#include <trn/terminal.h>

#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class SubjectStorageTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_current_path = fs::current_path();
        m_old_article_list = std::move(g_article_list);
        m_old_data_source = g_data_source;
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_art = g_art;
        m_old_curr_artp = g_curr_artp;
        m_old_artp = g_artp;
        m_old_in_ng = g_in_ng;
        m_old_kf_state = g_kf_state;
        m_old_threaded_group = g_threaded_group;
        m_old_thread_always = g_thread_always;
        m_old_int_count = g_int_count;
        m_old_subj_line = g_subj_line;
        m_old_page_line = g_page_line;
        m_old_tc_lines = g_tc_LINES;
        m_old_tc_cols = g_tc_COLS;
        m_old_tc_am = g_tc_AM;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        std::ofstream{m_output_dir / "1"} << "article 1\n";
        std::ofstream{m_output_dir / "2"} << "article 2\n";
        fs::current_path(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_article_list.clear();
        g_data_source = &m_data_source;
        g_newsgroup_ptr = &m_group;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{2};
        g_art = ArticleNum{};
        g_curr_artp = nullptr;
        g_artp = nullptr;
        g_in_ng = false;
        g_kf_state = KFS_NONE;
        g_threaded_group = false;
        g_thread_always = false;
        g_int_count = 0;
        g_subj_line = std::string{};
        g_page_line = 1;
        g_tc_LINES = 200;
        g_tc_COLS = 200;
        g_tc_AM = false;

        m_data_source.m_flags = DF_NONE;
        m_group.m_rc_line = "comp.lang.apl: ";
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.m_abs_first = g_abs_first;
        m_group.m_ng_max = g_last_art;
        m_group.m_to_read = ArticleUnread{2};

        build_cache();
    }

    void TearDown() override
    {
        close_cache();

        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);

        g_article_list = std::move(m_old_article_list);
        g_data_source = m_old_data_source;
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_art = m_old_art;
        g_curr_artp = m_old_curr_artp;
        g_artp = m_old_artp;
        g_in_ng = m_old_in_ng;
        g_kf_state = m_old_kf_state;
        g_threaded_group = m_old_threaded_group;
        g_thread_always = m_old_thread_always;
        g_int_count = m_old_int_count;
        g_subj_line = m_old_subj_line;
        g_page_line = m_old_page_line;
        g_tc_LINES = m_old_tc_lines;
        g_tc_COLS = m_old_tc_cols;
        g_tc_AM = m_old_tc_am;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
    }

    DataSource                    m_data_source{};
    NewsgroupData                 m_group{};
    fs::path                      m_old_current_path;
    fs::path                      m_output_dir;
    std::map<ArticleNum, Article> m_old_article_list;
    DataSource                   *m_old_data_source{};
    NewsgroupData                *m_old_newsgroup_ptr{};
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    ArticleNum                    m_old_art{};
    Article                      *m_old_curr_artp{};
    Article                      *m_old_artp{};
    bool                          m_old_in_ng{};
    KillFileStateFlags            m_old_kf_state{KFS_NONE};
    bool                          m_old_threaded_group{};
    bool                          m_old_thread_always{};
    int                           m_old_int_count{};
    std::optional<std::string>    m_old_subj_line;
    int                           m_old_page_line{};
    int                           m_old_tc_lines{};
    int                           m_old_tc_cols{};
    bool                          m_old_tc_am{};
    int                           m_old_term_line{};
    int                           m_old_term_col{};
};

} // namespace

TEST_F(SubjectStorageTest, setSubjectLineStripsReplyPrefixAndSharesSubject)
{
    Article *first = article_ptr(ArticleNum{1});
    Article *second = article_ptr(ArticleNum{2});

    first->set_subj_line("Re: Shared Topic");
    second->set_subj_line("Shared Topic");

    ASSERT_NE(nullptr, first->m_subj);
    ASSERT_NE(nullptr, second->m_subj);
    EXPECT_EQ(first->m_subj, second->m_subj);
    EXPECT_EQ("Re: Shared Topic", first->get_cached_line_text(SUBJ_LINE, false));
    EXPECT_EQ("Shared Topic", second->get_cached_line_text(SUBJ_LINE, false));
    EXPECT_EQ(1, g_subject_count);
}

TEST_F(SubjectStorageTest, replacingSubjectUpdatesHashKey)
{
    Article *first = article_ptr(ArticleNum{1});
    Article *second = article_ptr(ArticleNum{2});

    first->set_subj_line("Original Topic");
    Subject *original = first->m_subj;

    first->set_subj_line("Replacement Topic");
    second->set_subj_line("Original Topic");

    ASSERT_NE(nullptr, first->m_subj);
    ASSERT_NE(nullptr, second->m_subj);
    EXPECT_EQ(original, first->m_subj);
    EXPECT_NE(first->m_subj, second->m_subj);
    EXPECT_EQ("Replacement Topic", first->get_cached_line_text(SUBJ_LINE, false));
    EXPECT_EQ("Original Topic", second->get_cached_line_text(SUBJ_LINE, false));
    EXPECT_EQ(2, g_subject_count);
}

TEST_F(SubjectStorageTest, outputSubjectPrintsArticleNumberAndSubject)
{
    Article *article = article_ptr(ArticleNum{1});
    article->set_subj_line("A Subject");

    testing::internal::CaptureStdout();
    const bool        stopped = output_subject(reinterpret_cast<char *>(article), 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(stopped);
    EXPECT_EQ("1     A Subject\n", output);
}

TEST_F(SubjectStorageTest, outputSubjectPrintsCustomSubjectLine)
{
    Article *article = article_ptr(ArticleNum{1});
    article->set_subj_line("A Subject");
    g_subj_line = std::string{"[%s]"};
    g_in_ng = true;
    g_artp = article;

    testing::internal::CaptureStdout();
    const bool        stopped = output_subject(reinterpret_cast<char *>(article), 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(stopped);
    EXPECT_EQ("1     [A Subject]\n", output);
}

TEST_F(SubjectStorageTest, outputSubjectReadsCustomSubjectLineFromEnvironment)
{
    trn::testing::MockEnvironment env;
    env.expect_env("SUBJLINE", "[%s]");
    Article *article = article_ptr(ArticleNum{1});
    article->set_subj_line("A Subject");
    g_subj_line = std::nullopt;
    g_in_ng = true;
    g_artp = article;

    testing::internal::CaptureStdout();
    const bool        stopped = output_subject(reinterpret_cast<char *>(article), 0);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(stopped);
    EXPECT_EQ("1     [A Subject]\n", output);
}

TEST_F(SubjectStorageTest, threadArticleBuildsParentChainFromReferences)
{
    Article *article = article_ptr(ArticleNum{1});
    article->m_date = 100;
    article->m_msg_id = "<child@example.com>";
    article->set_subj_line("A Subject");

    ASSERT_TRUE(article->valid_article());

    const std::string original_references{"<grand@example.com> <parent@example.com>"};
    std::string       references{original_references};

    article->thread_article(references);

    EXPECT_EQ(original_references, references);

    Article *parent = article->m_parent;
    ASSERT_NE(nullptr, parent);
    Article *grandparent = parent->m_parent;
    ASSERT_NE(nullptr, grandparent);

    EXPECT_EQ(parent, grandparent->m_child1);
    EXPECT_EQ(article, parent->m_child1);
    EXPECT_EQ(grandparent, article->m_subj->m_thread);
    ASSERT_TRUE(parent->m_msg_id);
    ASSERT_TRUE(grandparent->m_msg_id);
    EXPECT_EQ("<parent@example.com>", *parent->m_msg_id);
    EXPECT_EQ("<grand@example.com>", *grandparent->m_msg_id);
}
