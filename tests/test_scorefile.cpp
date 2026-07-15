// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile-internal.h>

#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/Subject.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <util/env.h>
#include <util/util2.h>

#include <config/common.h>
#include <test_config.h>

#include "mock_env.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{

std::string g_fetched_url;

namespace fs = std::filesystem;

constexpr ArticleNum TEST_ARTICLE_NUM{1};

bool fetch_score_url(std::string_view url, const char *outfile)
{
    g_fetched_url = std::string{url};

    std::ofstream output{outfile, std::ios::binary};
    output << "10 subject: remote\n";
    return output.good();
}

class ScoreFileTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_tmp_dir = g_tmp_dir;
        m_old_pid = g_our_pid;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_article_list = std::move(g_article_list);
        m_old_art = g_art;
        m_old_last_art = g_last_art;
        m_old_in_ng = g_in_ng;
        m_old_parsed_art = g_parsed_art;
        m_old_term_line = g_term_line;
        m_old_term_col = g_term_col;
        m_old_term_scrolled = g_term_scrolled;

        head_init();
        g_tmp_dir = TRN_TEST_TMP_DIR;
        g_our_pid = 1357;
        g_sf_num_entries = 0;
        g_sf_verbose = false;
        g_sf_score_verbose = 0;
        g_article_list.clear();
        Article *article = article_ptr(TEST_ARTICLE_NUM);
        article->m_flags = AF_EXISTS;
        article->m_subj = &m_subject;
        article->set_cached_line(FROM_LINE, "casey@news.example.test");
        m_subject.m_str = m_subject_text;
        g_art = TEST_ARTICLE_NUM;
        g_last_art = TEST_ARTICLE_NUM;
        g_in_ng = true;
        g_parsed_art = ArticleNum{};
    }

    void TearDown() override
    {
        sf_set_url_getter_for_test(nullptr);
        sf_clear_file_cache_for_test();
        sf_clean();
        article_ptr(TEST_ARTICLE_NUM)->clear_article();
        g_article_list.clear();
        g_article_list = std::move(m_old_article_list);
        head_final();
        g_sf_num_entries = 0;
        g_tmp_dir = m_old_tmp_dir;
        g_our_pid = m_old_pid;
        g_newsgroup_name = m_old_newsgroup_name;
        g_art = m_old_art;
        g_last_art = m_old_last_art;
        g_in_ng = m_old_in_ng;
        g_parsed_art = m_old_parsed_art;
        g_term_line = m_old_term_line;
        g_term_col = m_old_term_col;
        g_term_scrolled = m_old_term_scrolled;
    }

    std::vector<std::string> read_lines(const fs::path &path)
    {
        std::ifstream            input{path};
        std::vector<std::string> lines;
        std::string              line;
        while (std::getline(input, line))
        {
            lines.push_back(line);
        }
        return lines;
    }

    std::string                   m_old_tmp_dir;
    std::string                   m_old_newsgroup_name;
    std::map<ArticleNum, Article> m_old_article_list;
    ArticleNum                    m_old_art{};
    ArticleNum                    m_old_last_art{};
    bool                          m_old_in_ng{};
    ArticleNum                    m_old_parsed_art{};
    long        m_old_pid{};
    int         m_old_term_line{};
    int         m_old_term_col{};
    int         m_old_term_scrolled{};
    std::string m_long_subject;
    Subject     m_subject{};
    char        m_subject_text[64]{"Re: Compact Subject"};
};

} // namespace

TEST_F(ScoreFileTest, extraHeaderLookupIsCaseInsensitive)
{
    char header[]{"!header X-Custom-Score:"};
    sf_append(header);

    EXPECT_EQ(0, g_sf_num_entries);

    char rule[]{"!10 X-CUSTOM-SCORE: value"};
    sf_append(rule);

    EXPECT_EQ(1, g_sf_num_entries);
}

TEST_F(ScoreFileTest, includeUrlFetchesScoreFile)
{
    g_fetched_url.clear();
    sf_set_url_getter_for_test(fetch_score_url);

    char include[]{"!include URL:http://example.test/scores"};
    sf_append(include);

    EXPECT_EQ("http://example.test/scores", g_fetched_url);
    EXPECT_EQ(3, g_sf_num_entries);
}

TEST_F(ScoreFileTest, subjectScoringKeepsLineBufferCap)
{
    std::string subject_text(LINE_BUF_LEN + 80, 'a');
    subject_text.replace(LINE_BUF_LEN - 20, 6, "inside");
    subject_text.replace(LINE_BUF_LEN + 20, 7, "outside");
    m_long_subject = "Re: " + subject_text;
    m_subject.m_str = m_long_subject.data();

    char inside_rule[]{"!10 subject: inside"};
    sf_append(inside_rule);
    char outside_rule[]{"!20 subject: outside"};
    sf_append(outside_rule);

    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));
}

TEST_F(ScoreFileTest, appendFromShortcutWritesShortenedFromRule)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-append-from"};
    const fs::path    score_file{score_dir + "/comp.lang.apl"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());

    char line[]{"\" 10 F"};
    sf_append(line);

    EXPECT_EQ((std::vector<std::string>{"10 from: casey@*.example.test"}), read_lines(score_file));
}

TEST_F(ScoreFileTest, appendSubjectShortcutWritesSubjectRule)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-append-subject"};
    const fs::path    score_file{score_dir + "/comp.lang.apl"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());

    char line[]{"\" 10 S"};
    sf_append(line);

    EXPECT_EQ((std::vector<std::string>{"10 subject: compact subject"}), read_lines(score_file));
}

TEST_F(ScoreFileTest, editLocalFileBuildsExpandedEditorCommand)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-edit"};
    const std::string score_file{score_dir + "/comp.lang.apl"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());
    env.expect_no_envar("EDITOR");
    env.expect_env("VISUAL", ":");

    sf_edit_file("\"");

    EXPECT_STREQ((": " + score_file).c_str(), g_cmd_buf);
    EXPECT_TRUE(fs::exists(score_dir));
}
