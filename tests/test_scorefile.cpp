// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/scorefile-internal.h>

#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/score.h>
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
std::string g_fetched_outfile;

namespace fs = std::filesystem;

constexpr ArticleNum TEST_ARTICLE_NUM{1};

bool fetch_score_url(std::string_view url, const fs::path &outfile)
{
    g_fetched_url = std::string{url};
    g_fetched_outfile = outfile.string();

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
        m_subject.m_str = "Re: Compact Subject";
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
};

} // namespace

TEST_F(ScoreFileTest, extraHeaderLookupIsCaseInsensitive)
{
    sf_append("!header X-Custom-Score:");

    EXPECT_EQ(0, g_sf_num_entries);

    sf_append("!10 X-CUSTOM-SCORE: value");

    EXPECT_EQ(1, g_sf_num_entries);
}

TEST_F(ScoreFileTest, extraHeaderScoringReadsParsedHeaderBuffer)
{
    sf_append("!header X-Custom-Score:");
    sf_append("!10 X-Custom-Score: magic value");
    g_parsed_art = TEST_ARTICLE_NUM;
    g_head_buf = "From: writer@example.test\n"
                 "X-Custom-Score:  Magic Value\n"
                 "Subject: Compact Subject\n"
                 "\n";

    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));
}

TEST_F(ScoreFileTest, fromWildcardMatchesBothPiecesInOrder)
{
    DataSource        data_source{};
    DataSource *const old_data_source = g_data_source;
    g_data_source = &data_source;

    sf_append("!10 from: casey@*.example.test");
    sf_append("!20 from: example*casey@");

    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));

    g_data_source = old_data_source;
}

TEST_F(ScoreFileTest, patternKeywordMatchesWithRegularExpression)
{
    sf_append("!10 pattern subject: compact.*subject");

    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));
}

TEST_F(ScoreFileTest, verboseScoringPrintsMatchedPatternRule)
{
    sf_append("!10 pattern subject: compact.*subject");
    g_sf_score_verbose = 1;

    testing::internal::CaptureStdout();
    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ("10 pattern subject: compact.*subject\n", output);
}

TEST_F(ScoreFileTest, killThresholdCommandAcceptsSpaceSeparator)
{
    sf_append("!killthreshold -11");

    EXPECT_EQ(1, g_sf_num_entries);
}

TEST_F(ScoreFileTest, newAuthorCommandAcceptsAssignmentSeparator)
{
    sf_append("!newauthor=7");

    EXPECT_EQ(1, g_sf_num_entries);
}

TEST_F(ScoreFileTest, saveScoresOffDisablesScoreSaving)
{
    const bool old_save_scores = g_sc_saves_cores;
    g_sc_saves_cores = true;
    sf_append("!savescores off");

    EXPECT_FALSE(g_sc_saves_cores);
    g_sc_saves_cores = old_save_scores;
}

TEST_F(ScoreFileTest, saveScoresOtherArgumentEnablesScoreSaving)
{
    const bool old_save_scores = g_sc_saves_cores;
    g_sc_saves_cores = false;
    sf_append("!savescores on");

    EXPECT_TRUE(g_sc_saves_cores);
    g_sc_saves_cores = old_save_scores;
}

TEST_F(ScoreFileTest, includeUrlFetchesScoreFile)
{
    g_fetched_url.clear();
    g_fetched_outfile.clear();
    sf_set_url_getter_for_test(fetch_score_url);

    sf_append("!include URL:http://example.test/scores");

    EXPECT_EQ("http://example.test/scores", g_fetched_url);
    EXPECT_FALSE(g_fetched_outfile.empty());
    EXPECT_FALSE(fs::exists(g_fetched_outfile));
    EXPECT_EQ(3, g_sf_num_entries);
}

TEST_F(ScoreFileTest, subjectScoringKeepsLineBufferCap)
{
    std::string subject_text(LINE_BUF_LEN + 80, 'a');
    subject_text.replace(LINE_BUF_LEN - 20, 6, "inside");
    subject_text.replace(LINE_BUF_LEN + 20, 7, "outside");
    m_long_subject = "Re: " + subject_text;
    m_subject.m_str = m_long_subject;

    sf_append("!10 subject: inside");
    sf_append("!20 subject: outside");

    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));
}

TEST_F(ScoreFileTest, replyScoreRecognizesReplyPrefix)
{
    const fs::path score_dir{TRN_TEST_TMP_DIR "/scorefile-reply"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    fs::create_directories(score_dir);
    std::ofstream{score_dir / "global"} << "reply 17\n";
    g_newsgroup_name = "comp.lang.apl";
    article_ptr(TEST_ARTICLE_NUM)->m_flags |= AF_HAS_RE;

    trn::testing::MockEnvironment env;
    const std::string             score_dir_name{score_dir.string()};
    env.expect_env("SCOREDIR", score_dir_name.c_str());

    sf_init();

    EXPECT_EQ(17, sf_score(TEST_ARTICLE_NUM));
}

TEST_F(ScoreFileTest, initReadsGlobalAndHierarchicalGroupScoreFiles)
{
    const fs::path score_dir{TRN_TEST_TMP_DIR "/scorefile-hierarchy"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    fs::create_directories(score_dir);
    std::ofstream{score_dir / "global"} << "1 subject: global\n";
    std::ofstream{score_dir / "comp"} << "2 subject: comp\n";
    std::ofstream{score_dir / "comp.lang"} << "3 subject: lang\n";
    std::ofstream{score_dir / "comp.lang.apl"} << "4 subject: apl\n";
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    const std::string             score_dir_name{score_dir.string()};
    env.expect_env("SCOREDIR", score_dir_name.c_str());

    sf_init();

    EXPECT_EQ(12, g_sf_num_entries);
}

TEST_F(ScoreFileTest, initReadsScorefileLinePastLegacyBufferLimit)
{
    const fs::path score_dir{TRN_TEST_TMP_DIR "/scorefile-long-line"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    fs::create_directories(score_dir);
    std::ofstream{score_dir / "global"} << "10 subject:" << std::string(LINE_BUF_LEN, ' ') << "compact subject\n";
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    const std::string             score_dir_name{score_dir.string()};
    env.expect_env("SCOREDIR", score_dir_name.c_str());

    sf_init();

    EXPECT_EQ(10, sf_score(TEST_ARTICLE_NUM));
}

TEST_F(ScoreFileTest, excludeRemovesIncludedLocalScoreFile)
{
    const fs::path score_dir{TRN_TEST_TMP_DIR "/scorefile-include-exclude"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    fs::create_directories(score_dir, error);
    std::ofstream{score_dir / "global"} << "!include child\n"
                                        << "!exclude child\n"
                                        << "10 subject: compact subject\n";
    std::ofstream{score_dir / "child"} << "20 subject: compact subject\n";
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    const std::string             score_dir_name{score_dir.string()};
    env.expect_env("SCOREDIR", score_dir_name.c_str());

    sf_init();

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

    sf_append(R"(" 10 F)");

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

    sf_append(R"(" 10 S)");

    EXPECT_EQ((std::vector<std::string>{"10 subject: compact subject"}), read_lines(score_file));
}

TEST_F(ScoreFileTest, appendMissingScoreUsesTypedScore)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-append-missing-score"};
    const fs::path    score_file{score_dir + "/comp.lang.apl"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());

    push_string("7\n", 0);

    testing::internal::CaptureStdout();
    sf_append(R"(" subject: compact subject)");
    testing::internal::GetCapturedStdout();

    EXPECT_EQ((std::vector<std::string>{"7 subject: compact subject"}), read_lines(score_file));
}

TEST_F(ScoreFileTest, appendAbbreviationWritesConfiguredFile)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-append-abbreviation"};
    const fs::path    score_file{score_dir + "/abbr-score"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    fs::create_directories(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());
    sf_init();

    sf_append("!file @ abbr-score");
    sf_append("@ 10 subject: abbreviated");

    EXPECT_EQ((std::vector<std::string>{"10 subject: abbreviated"}), read_lines(score_file));
}

TEST_F(ScoreFileTest, editLocalFileCreatesScoreDirectory)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-edit"};

    std::error_code error;
    fs::remove_all(score_dir, error);
    g_newsgroup_name = "comp.lang.apl";

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());
    env.expect_no_envar("EDITOR");
    env.expect_env("VISUAL", ":");

    sf_edit_file("\"");

    EXPECT_TRUE(fs::exists(score_dir));
}

TEST_F(ScoreFileTest, editGlobalFileCreatesScoreDirectory)
{
    const std::string score_dir{TRN_TEST_TMP_DIR "/scorefile-edit-global"};

    std::error_code error;
    fs::remove_all(score_dir, error);

    trn::testing::MockEnvironment env;
    env.expect_env("SCOREDIR", score_dir.c_str());
    env.expect_no_envar("EDITOR");
    env.expect_env("VISUAL", ":");

    sf_edit_file("*");

    EXPECT_TRUE(fs::exists(score_dir));
}
