// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/common.h>
#include <trn/datasrc.h>
#include <trn/hash.h>
#include <trn/ngdata.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>

#include <gtest/gtest.h>

#include <test_config.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

int compare_newsgroup_name(std::string_view key, HashDatum data)
{
    const NewsgroupData *group = reinterpret_cast<NewsgroupData *>(data.dat_ptr);

    return key.compare(group->rc_name());
}

class ExpiredArticleTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_newsrc.flags = RF_NONE;
        m_group.m_rc = &m_newsrc;
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.m_subscribe_char = ':';
    }

    void set_numbers(const std::string &numbers)
    {
        m_group.m_rc_line = "comp.lang.apl: " + numbers;
        m_group.hide_subscribe_char();
    }

    std::string visible_rc_line() const
    {
        std::string line = m_group.m_rc_line;
        line[static_cast<std::size_t>(m_group.m_num_offset - 1)] = m_group.m_subscribe_char;
        return line;
    }

    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
};

class SetToReadTest : public testing::Test
{
protected:
    void SetUp() override
    {
        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        m_old_newsgroup_min_to_read = g_newsgroup_min_to_read;
        m_old_to_read_quiet = g_to_read_quiet;
        g_newsgroup_min_to_read = TR_ONE;
        g_to_read_quiet = true;

        m_active_path = m_output_dir / "active";
        std::ofstream{m_active_path} << "comp.lang.apl 100 1 y\n";
        ASSERT_EQ(1, m_data_source.m_act_sf.open(m_active_path, "", ""));

        m_newsrc.flags = RF_NONE;
        m_newsrc.data_source = &m_data_source;
        m_group.m_rc = &m_newsrc;
        m_group.m_abs_first = ArticleNum{1};
        m_group.m_to_read = ArticleUnread{100};
        m_group.m_subscribe_char = ':';
    }

    void TearDown() override
    {
        m_data_source.m_act_sf.close();
        g_newsgroup_min_to_read = m_old_newsgroup_min_to_read;
        g_to_read_quiet = m_old_to_read_quiet;

        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    void set_numbers(const std::string &numbers)
    {
        m_group.m_rc_line = "comp.lang.apl: " + numbers;
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.hide_subscribe_char();
    }

    fs::path      m_output_dir;
    fs::path      m_active_path;
    ArticleUnread m_old_newsgroup_min_to_read{};
    bool          m_old_to_read_quiet{};
    DataSource    m_data_source{};
    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
};

class AddArtNumTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsrc_hash = g_newsrc_hash;
        g_newsrc_hash = hash_create(17, compare_newsgroup_name);

        m_newsrc.flags = RF_NONE;
        m_newsrc.data_source = &m_data_source;
        m_group.m_rc = &m_newsrc;
        m_group.m_abs_first = ArticleNum{1};
        m_group.m_ng_max = ArticleNum{20};
        m_group.m_to_read = ArticleUnread{10};
        m_group.m_subscribe_char = ':';
    }

    void TearDown() override
    {
        hash_destroy(g_newsrc_hash);
        g_newsrc_hash = m_old_newsrc_hash;
    }

    void set_numbers(const std::string &numbers)
    {
        m_group.m_rc_line = "comp.lang.apl: " + numbers;
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.hide_subscribe_char();

        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&m_group);
        data.dat_len = static_cast<unsigned>(m_group.rc_name().size());
        hash_store(g_newsrc_hash, m_group.rc_name(), data);
    }

    void add_article(ArticleNum art_num)
    {
        add_art_num(&m_data_source, art_num, "comp.lang.apl");
    }

#ifdef MCHASE
    void remove_article(ArticleNum art_num)
    {
        sub_art_num(&m_data_source, art_num, "comp.lang.apl");
    }
#endif

    std::string visible_rc_line() const
    {
        std::string line = m_group.m_rc_line;
        line[static_cast<std::size_t>(m_group.m_num_offset - 1)] = m_group.m_subscribe_char;
        return line;
    }

    HashTable    *m_old_newsrc_hash{};
    DataSource    m_data_source{};
    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
};

class WasReadGroupTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsrc_hash = g_newsrc_hash;
        g_newsrc_hash = hash_create(17, compare_newsgroup_name);

        m_newsrc.flags = RF_NONE;
        m_group.m_rc = &m_newsrc;
        m_group.m_ng_max = ArticleNum{20};
        m_group.m_to_read = ArticleUnread{10};
        m_group.m_subscribe_char = ':';
    }

    void TearDown() override
    {
        hash_destroy(g_newsrc_hash);
        g_newsrc_hash = m_old_newsrc_hash;
    }

    void set_numbers(const std::string &numbers)
    {
        m_group.m_rc_line = "comp.lang.apl: " + numbers;
        m_group.m_num_offset = static_cast<int>(std::string{"comp.lang.apl"}.size()) + 1;
        m_group.hide_subscribe_char();
        register_group();
    }

    void set_without_numbers()
    {
        m_group.m_rc_line = "comp.lang.apl";
        m_group.m_num_offset = 0;
        register_group();
    }

    void register_group()
    {
        HashDatum data{};
        data.dat_ptr = reinterpret_cast<char *>(&m_group);
        data.dat_len = static_cast<unsigned>(m_group.rc_name().size());
        hash_store(g_newsrc_hash, m_group.rc_name(), data);
    }

    HashTable    *m_old_newsrc_hash{};
    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
};

} // namespace

TEST_F(ExpiredArticleTest, extendsReadRangeToFirstAvailableArticle)
{
    set_numbers("2-20,25");

    m_group.check_expired(ArticleNum{10});

    EXPECT_EQ("comp.lang.apl: 1-20,25", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(ExpiredArticleTest, leavesExistingRangeStartingAtFirstArticle)
{
    set_numbers("1-20,25");

    m_group.check_expired(ArticleNum{10});

    EXPECT_EQ("comp.lang.apl: 1-20,25", visible_rc_line());
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(ExpiredArticleTest, extendsReadRangeWithoutUnreadSuffix)
{
    set_numbers("1-5");

    m_group.check_expired(ArticleNum{8});

    EXPECT_EQ("comp.lang.apl: 1-7", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(SetToReadTest, countsUnreadArticlesFromLongReadList)
{
    set_numbers("1,3,5,7,9,11,13,15,17,19,21,23,25,27,29,"
                "31,33,35,37,39,41,43,45,47,49,51,53,55,57,59");

    m_group.set_to_read(ST_STRICT);

    EXPECT_EQ(ArticleUnread{70}, m_group.m_to_read);
    EXPECT_EQ(ArticleNum{100}, m_group.m_ng_max);
    EXPECT_EQ(RF_NONE, m_newsrc.flags & RF_RC_CHANGED);
}

TEST_F(AddArtNumTest, insertsStandaloneArticleBeforeLaterRange)
{
    set_numbers("1,5-7");

    add_article(ArticleNum{3});

    EXPECT_EQ("comp.lang.apl: 1,3,5-7", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, extendsPriorRange)
{
    set_numbers("1-3,6");

    add_article(ArticleNum{4});

    EXPECT_EQ("comp.lang.apl: 1-4,6", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, mergesAdjacentSingletons)
{
    set_numbers("1,3");

    add_article(ArticleNum{2});

    EXPECT_EQ("comp.lang.apl: 1-3", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, bridgesAdjacentRanges)
{
    set_numbers("1-3,5-7");

    add_article(ArticleNum{4});

    EXPECT_EQ("comp.lang.apl: 1-7", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, appendsAfterLastReadRange)
{
    set_numbers("1-3");

    add_article(ArticleNum{5});

    EXPECT_EQ("comp.lang.apl: 1-3,5", visible_rc_line());
    EXPECT_EQ(ArticleUnread{9}, m_group.m_to_read);
}

#ifdef MCHASE
TEST_F(AddArtNumTest, removesTrailingSingleton)
{
    set_numbers("1-3,5");

    remove_article(ArticleNum{5});

    EXPECT_EQ("comp.lang.apl: 1-3", visible_rc_line());
    EXPECT_EQ(ArticleUnread{11}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, splitsRange)
{
    set_numbers("1-7");

    remove_article(ArticleNum{4});

    EXPECT_EQ("comp.lang.apl: 1-3,5-7", visible_rc_line());
    EXPECT_EQ(ArticleUnread{11}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, removesFirstArticleFromRange)
{
    set_numbers("1-7");

    remove_article(ArticleNum{1});

    EXPECT_EQ("comp.lang.apl: 2-7", visible_rc_line());
    EXPECT_EQ(ArticleUnread{11}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, removesLastArticleFromRange)
{
    set_numbers("1-7");

    remove_article(ArticleNum{7});

    EXPECT_EQ("comp.lang.apl: 1-6", visible_rc_line());
    EXPECT_EQ(ArticleUnread{11}, m_group.m_to_read);
}

TEST_F(AddArtNumTest, removesStandaloneFromMiddle)
{
    set_numbers("1,3,5");

    remove_article(ArticleNum{3});

    EXPECT_EQ("comp.lang.apl: 1,5", visible_rc_line());
    EXPECT_EQ(ArticleUnread{11}, m_group.m_to_read);
}
#endif

TEST_F(WasReadGroupTest, findsReadArticleInExplicitList)
{
    set_numbers("1,3,5");

    EXPECT_TRUE(was_read_group(ArticleNum{3}, "comp.lang.apl"));
}

TEST_F(WasReadGroupTest, findsReadArticleInRange)
{
    set_numbers("1-5,9");

    EXPECT_TRUE(was_read_group(ArticleNum{4}, "comp.lang.apl"));
}

TEST_F(WasReadGroupTest, reportsUnreadArticleBetweenRanges)
{
    set_numbers("1-3,7");

    EXPECT_FALSE(was_read_group(ArticleNum{5}, "comp.lang.apl"));
}

TEST_F(WasReadGroupTest, unknownGroupIsTreatedAsRead)
{
    EXPECT_TRUE(was_read_group(ArticleNum{5}, "comp.lang.apl"));
}

TEST_F(WasReadGroupTest, lineWithoutNumbersIsUnread)
{
    set_without_numbers();

    EXPECT_FALSE(was_read_group(ArticleNum{5}, "comp.lang.apl"));
}

TEST_F(ExpiredArticleTest, removesExpiredRangesAndKeepsUnreadSuffix)
{
    set_numbers("1-5,10-20");

    m_group.check_expired(ArticleNum{8});

    EXPECT_EQ("comp.lang.apl: 1-7,10-20", visible_rc_line());
    EXPECT_EQ('\0', m_group.m_rc_line[static_cast<std::size_t>(m_group.m_num_offset - 1)]);
    EXPECT_EQ(RF_RC_CHANGED, m_newsrc.flags & RF_RC_CHANGED);
}
