// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/bits.h>

#include <config/common.h>
#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/hash.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/rcstuff.h>
#include <trn/rthread.h>
#include <trn/trn.h>

#include <nntp/nntpclient.h>
#include <test_config.h>

#include "MockNNTPConnection.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{

namespace fs = std::filesystem;

using MockConnection = testing::StrictMock<MockNNTPConnection>;
using MockConnectionPtr = std::shared_ptr<MockConnection>;

int compare_newsgroup_name(std::string_view key, HashDatum data)
{
    const NewsgroupData *group = reinterpret_cast<NewsgroupData *>(data.dat_ptr);

    return key.compare(group->rc_name());
}

class BitsToRcTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_article_list = std::move(g_article_list);
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;

        g_article_list.clear();
        g_newsgroup_ptr = &m_group;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{7};

        m_newsrc.flags = RF_NONE;
        m_group.m_rc = &m_newsrc;
        set_rc_line("comp.lang.apl: old");
        m_group.m_to_read = ArticleUnread{};
    }

    void TearDown() override
    {
        g_article_list = std::move(m_old_article_list);
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
    }

    void add_article(long num, bool unread)
    {
        Article *article = article_ptr(ArticleNum{num});
        article->m_flags = AF_EXISTS;
        if (unread)
        {
            article->m_flags |= AF_UNREAD;
        }
    }

    void add_existing_articles(long first, long last)
    {
        for (long num = first; num <= last; ++num)
        {
            add_article(num, false);
        }
    }

    void expect_unread(long num, bool unread)
    {
        SCOPED_TRACE(num);
        EXPECT_EQ(unread, article_unread(ArticleNum{num}));
    }

    void set_rc_line(std::string line)
    {
        m_group.m_rc_line = std::move(line);
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.m_subscribe_char = m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)];
        m_group.hide_subscribe_char();
    }

    std::string visible_rc_line() const
    {
        std::string line = m_group.m_rc_line;
        line[static_cast<std::size_t>(m_group.m_num_offset - 1)] = m_group.m_subscribe_char;
        return line;
    }

    Newsrc                        m_newsrc{};
    NewsgroupData                 m_group{};
    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
};

class FindExistingArticlesTest : public testing::Test
{
protected:
    void SetUp() override
    {
        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();

        m_old_current_path = fs::current_path();
        m_old_article_list = std::move(g_article_list);
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_data_source = g_data_source;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_first_cached = g_first_cached;
        m_old_last_cached = g_last_cached;
        m_old_first_subject = g_first_subject;
        m_old_cached_all_in_range = g_cached_all_in_range;
        m_old_nntp_link = g_nntp_link;
        m_old_last_command = g_last_command;
        nntp_gets_clear_buffer();

        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error);
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error);
        fs::current_path(m_output_dir, error);
        ASSERT_FALSE(error);

        g_article_list.clear();
        g_newsgroup_ptr = &m_group;
        g_data_source = &m_data_source;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{5};
        g_first_cached = ArticleNum{1};
        g_last_cached = ArticleNum{};
        g_first_subject = nullptr;
        g_cached_all_in_range = false;
        g_nntp_link.connection.reset();
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
        g_last_command.clear();

        m_data_source.m_flags = DF_NONE;
        m_group.m_abs_first = ArticleNum{1};
        m_group.m_ng_max = ArticleNum{5};

        for (long num = 1; num <= 5; ++num)
        {
            article_ptr(ArticleNum{num})->m_flags = AF_EXISTS;
        }
    }

    void TearDown() override
    {
        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);

        g_article_list = std::move(m_old_article_list);
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_data_source = m_old_data_source;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_first_cached = m_old_first_cached;
        g_last_cached = m_old_last_cached;
        g_first_subject = m_old_first_subject;
        g_cached_all_in_range = m_old_cached_all_in_range;
        nntp_gets_clear_buffer();
        g_nntp_link = m_old_nntp_link;
        g_last_command = m_old_last_command;
    }

    void write_file(std::string_view name)
    {
        std::ofstream{m_output_dir / name} << "article\n";
    }

    bool article_exists(long num) const
    {
        return g_article_list.at(ArticleNum{num}).m_flags & AF_EXISTS;
    }

    void use_remote_connection()
    {
        m_data_source.m_flags = DF_REMOTE;
        m_connection = std::make_shared<MockConnection>();
        g_nntp_link.connection = m_connection;
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
    }

    fs::path                      m_old_current_path;
    fs::path                      m_output_dir;
    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    DataSource                   *m_old_data_source{};
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    ArticleNum                    m_old_first_cached{};
    ArticleNum                    m_old_last_cached{};
    Subject                      *m_old_first_subject{};
    bool                          m_old_cached_all_in_range{};
    NNTPLink                      m_old_nntp_link{};
    std::string                   m_old_last_command;
    DataSource                    m_data_source{};
    NewsgroupData                 m_group{};
    MockConnectionPtr             m_connection;
};

class XrefChaseTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_article_list = std::move(g_article_list);
        m_old_newsgroup_ptr = g_newsgroup_ptr;
        m_old_data_source = g_data_source;
        m_old_newsrc_hash = g_newsrc_hash;
        m_old_newsgroup_name = g_newsgroup_name;
        m_old_abs_first = g_abs_first;
        m_old_first_art = g_first_art;
        m_old_last_art = g_last_art;
        m_old_check_count = g_check_count;
        m_old_output_chase_phrase = g_output_chase_phrase;

        g_article_list.clear();
        g_newsrc_hash = hash_create(17, compare_newsgroup_name);
        g_data_source = &m_data_source;
        g_newsgroup_name = "comp.lang.apl";
        g_newsgroup_ptr = &m_current_group;
        g_abs_first = ArticleNum{1};
        g_first_art = ArticleNum{1};
        g_last_art = ArticleNum{10};
        g_check_count = 0;
        g_output_chase_phrase = false;

        m_newsrc.flags = RF_NONE;
        m_newsrc.data_source = &m_data_source;
        configure_group(m_current_group, "comp.lang.apl", "", ArticleUnread{1});
        configure_group(m_target_group, "comp.lang.cpp", "1-4", ArticleUnread{6});
        store_group(m_current_group);
        store_group(m_target_group);
    }

    void TearDown() override
    {
        chase_xrefs(false);
        hash_destroy(g_newsrc_hash);
        g_article_list = std::move(m_old_article_list);
        g_newsgroup_ptr = m_old_newsgroup_ptr;
        g_data_source = m_old_data_source;
        g_newsrc_hash = m_old_newsrc_hash;
        g_newsgroup_name = m_old_newsgroup_name;
        g_abs_first = m_old_abs_first;
        g_first_art = m_old_first_art;
        g_last_art = m_old_last_art;
        g_check_count = m_old_check_count;
        g_output_chase_phrase = m_old_output_chase_phrase;
    }

    void configure_group(NewsgroupData &group, std::string_view name, std::string_view numbers, ArticleUnread unread)
    {
        group = {};
        group.m_rc = &m_newsrc;
        group.m_rc_line.assign(name);
        group.m_rc_line += ": ";
        group.m_rc_line += numbers;
        group.m_num_offset = static_cast<int>(name.size()) + 1;
        group.m_subscribe_char = ':';
        group.m_abs_first = ArticleNum{1};
        group.m_ng_max = ArticleNum{10};
        group.m_to_read = unread;
        group.hide_subscribe_char();
    }

    void store_group(NewsgroupData &group)
    {
        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&group);
        data.dat_len = static_cast<unsigned>(group.rc_name().size());
        hash_store(g_newsrc_hash, group.rc_name(), data);
    }

    Article *add_xref_article()
    {
        Article *article = article_ptr(ArticleNum{1});
        article->m_flags = AF_EXISTS | AF_UNREAD;
        article->set_cached_line(XREF_LINE, "news.example comp.lang.apl:1 comp.lang.cpp:5");
        return article;
    }

    std::string visible_rc_line(const NewsgroupData &group) const
    {
        std::string line = group.m_rc_line;
        line[static_cast<std::size_t>(group.m_num_offset - 1)] = group.m_subscribe_char;
        return line;
    }

    std::map<ArticleNum, Article> m_old_article_list;
    NewsgroupData                *m_old_newsgroup_ptr{};
    DataSource                   *m_old_data_source{};
    HashTable                    *m_old_newsrc_hash{};
    std::string                   m_old_newsgroup_name;
    ArticleNum                    m_old_abs_first{};
    ArticleNum                    m_old_first_art{};
    ArticleNum                    m_old_last_art{};
    int                           m_old_check_count{};
    bool                          m_old_output_chase_phrase{};
    DataSource                    m_data_source{};
    Newsrc                        m_newsrc{};
    NewsgroupData                 m_current_group{};
    NewsgroupData                 m_target_group{};
};

} // namespace

TEST_F(BitsToRcTest, decodesReadRangesIntoUnreadArticleFlags)
{
    set_rc_line("comp.lang.apl: 2,4-5");
    add_existing_articles(1, 7);

    rc_to_bits();

    expect_unread(1, true);
    expect_unread(2, false);
    expect_unread(3, true);
    expect_unread(4, false);
    expect_unread(5, false);
    expect_unread(6, true);
    expect_unread(7, true);
    EXPECT_EQ(ArticleNum{1}, g_first_art);
    EXPECT_EQ(ArticleUnread{4}, m_group.m_to_read);
}

TEST_F(BitsToRcTest, decodesLeadingReadRangeIntoFirstUnreadArticle)
{
    set_rc_line("comp.lang.apl: 1-2,4");
    add_existing_articles(1, 7);

    rc_to_bits();

    expect_unread(1, false);
    expect_unread(2, false);
    expect_unread(3, true);
    expect_unread(4, false);
    expect_unread(5, true);
    expect_unread(6, true);
    expect_unread(7, true);
    EXPECT_EQ(ArticleNum{3}, g_first_art);
    EXPECT_EQ(ArticleUnread{4}, m_group.m_to_read);
}

TEST_F(BitsToRcTest, ignoresReversedReadRange)
{
    set_rc_line("comp.lang.apl: 4-2");
    add_existing_articles(1, 7);

    rc_to_bits();

    expect_unread(1, true);
    expect_unread(2, true);
    expect_unread(3, true);
    expect_unread(4, true);
    expect_unread(5, true);
    expect_unread(6, true);
    expect_unread(7, true);
    EXPECT_EQ(ArticleUnread{7}, m_group.m_to_read);
}

TEST_F(BitsToRcTest, setFirstArtSkipsLeadingReadRange)
{
    EXPECT_TRUE(set_first_art(" 1-2,4"));
    EXPECT_EQ(ArticleNum{3}, g_first_art);
}

TEST_F(BitsToRcTest, setFirstArtUsesAbsoluteFirstWithoutLeadingReadRange)
{
    g_abs_first = ArticleNum{4};

    EXPECT_FALSE(set_first_art("2,6"));
    EXPECT_EQ(ArticleNum{4}, g_first_art);
}

TEST_F(BitsToRcTest, reconstructsSubscribedLineFromReadRanges)
{
    add_article(1, false);
    add_article(2, false);
    add_article(3, true);
    add_article(4, false);
    add_article(5, false);
    add_article(6, true);
    add_article(7, true);

    bits_to_rc();

    EXPECT_EQ("comp.lang.apl: 1-2,4-5", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(ArticleUnread{3}, m_group.m_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(BitsToRcTest, reconstructsUnsubscribedLineAndKeepsItInvisible)
{
    m_group.m_subscribe_char = UNSUBSCRIBED_CHAR;

    add_article(1, false);
    add_article(2, false);
    add_article(3, true);
    add_article(4, false);
    add_article(5, false);
    add_article(6, true);
    add_article(7, true);

    bits_to_rc();

    EXPECT_EQ("comp.lang.apl! 1-2,4-5", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(TR_UNSUB, m_group.m_to_read);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(FindExistingArticlesTest, scansOnlyPlainNumericArticleFilenames)
{
    write_file("1");
    write_file("3");
    write_file("4.txt");
    write_file("5x");

    find_existing_articles();

    EXPECT_TRUE(article_exists(1));
    EXPECT_FALSE(article_exists(2));
    EXPECT_TRUE(article_exists(3));
    EXPECT_FALSE(article_exists(4));
    EXPECT_FALSE(article_exists(5));
    EXPECT_EQ(ArticleNum{1}, g_abs_first);
    EXPECT_EQ(ArticleNum{3}, g_last_art);
    EXPECT_EQ(ArticleNum{1}, m_group.m_abs_first);
    EXPECT_EQ(ArticleNum{3}, m_group.m_ng_max);
}

TEST_F(FindExistingArticlesTest, remoteListgroupMarksReturnedArticleNumbers)
{
    use_remote_connection();
    EXPECT_CALL(*m_connection, write_line(testing::StrEq("LISTGROUP"), testing::_));
    EXPECT_CALL(*m_connection, read_line(testing::_))
        .WillOnce(testing::Return("211 list of article numbers follows"))
        .WillOnce(testing::Return("0"))
        .WillOnce(testing::Return("2"))
        .WillOnce(testing::Return("4"))
        .WillOnce(testing::Return("."));

    find_existing_articles();

    EXPECT_FALSE(article_exists(1));
    EXPECT_TRUE(article_exists(2));
    EXPECT_FALSE(article_exists(3));
    EXPECT_TRUE(article_exists(4));
    EXPECT_FALSE(article_exists(5));
}

TEST_F(XrefChaseTest, markAsReadChasesXrefToOtherGroup)
{
    Article *article = add_xref_article();

    article->mark_as_read();
    EXPECT_TRUE(chase_xrefs(false));

    EXPECT_EQ("comp.lang.cpp: 1-5", visible_rc_line(m_target_group));
    EXPECT_EQ(ArticleUnread{5}, m_target_group.m_to_read);
}

#ifdef MCHASE
TEST_F(XrefChaseTest, unmarkAsReadChasesXrefToOtherGroup)
{
    configure_group(m_target_group, "comp.lang.cpp", "1-5", ArticleUnread{5});
    store_group(m_target_group);
    Article *article = add_xref_article();
    article->m_flags &= ~AF_UNREAD;

    article->unmark_as_read();
    EXPECT_TRUE(chase_xrefs(false));

    EXPECT_EQ("comp.lang.cpp: 1-4", visible_rc_line(m_target_group));
    EXPECT_EQ(ArticleUnread{6}, m_target_group.m_to_read);
}
#endif
