// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/rcstuff.h>

#include <trn/datasrc.h>
#include <trn/ngdata.h>
#include <trn/rt-select.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

namespace fs = std::filesystem;

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

class NewsrcRotationTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_newsgroup_data = std::move(g_newsgroup_data);
        m_old_newsgroup_order = std::move(g_newsgroup_order);
        m_old_newsgroup_count = g_newsgroup_count;
        m_old_first_newsgroup = g_first_newsgroup;
        m_old_last_newsgroup = g_last_newsgroup;
        m_old_sel_sort = g_sel_sort;
        m_old_sel_newsgroup_sort = g_sel_newsgroup_sort;
        m_old_sel_direction = g_sel_direction;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_sel_sort = SS_NATURAL;
        g_sel_newsgroup_sort = SS_NATURAL;
        g_sel_direction = 1;
        g_newsgroup_data.clear();
        g_newsgroup_order.clear();
        g_newsgroup_count = NewsgroupNum{};
        g_first_newsgroup = nullptr;
        g_last_newsgroup = nullptr;
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_output_dir, error);

        g_newsgroup_data = std::move(m_old_newsgroup_data);
        g_newsgroup_order = std::move(m_old_newsgroup_order);
        g_newsgroup_count = m_old_newsgroup_count;
        g_first_newsgroup = m_old_first_newsgroup;
        g_last_newsgroup = m_old_last_newsgroup;
        g_sel_sort = m_old_sel_sort;
        g_sel_newsgroup_sort = m_old_sel_newsgroup_sort;
        g_sel_direction = m_old_sel_direction;
    }

    Newsrc make_newsrc()
    {
        Newsrc newsrc{};
        newsrc.data_source = &m_data_source;
        newsrc.name = (m_output_dir / "newsrc").generic_string();
        newsrc.old_name = (m_output_dir / "old-newsrc").generic_string();
        newsrc.new_name = (m_output_dir / "new-newsrc").generic_string();
        newsrc.info_name = (m_output_dir / "newsrc.info").generic_string();
        newsrc.flags = RF_ACTIVE | RF_RC_CHANGED;
        return newsrc;
    }

    void add_newsgroup(Newsrc &newsrc, std::string line)
    {
        NewsgroupData &group = g_newsgroup_data.emplace_back();
        group.m_rc = &newsrc;
        group.m_rc_line = std::move(line);
        append_newsgroup_order(&group);
        g_newsgroup_count = NewsgroupNum{static_cast<long>(g_newsgroup_data.size())};
    }

    fs::path                     m_output_dir;
    DataSource                   m_data_source{};
    std::vector<NewsgroupData>   m_old_newsgroup_data;
    std::vector<NewsgroupData *> m_old_newsgroup_order;
    NewsgroupNum                 m_old_newsgroup_count{};
    NewsgroupData               *m_old_first_newsgroup{};
    NewsgroupData               *m_old_last_newsgroup{};
    SelectionSortMode            m_old_sel_sort{};
    SelectionSortMode            m_old_sel_newsgroup_sort{};
    int                          m_old_sel_direction{};
};

} // namespace

TEST_F(NewsrcRotationTest, writeNewsrcsReplacesNewsrcWithNewFile)
{
    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;
    add_newsgroup(newsrc, "comp.lang.apl: 1-3");
    std::ofstream{newsrc.name} << "old contents\n";

    ASSERT_TRUE(write_newsrcs(&multirc));

    EXPECT_EQ((std::vector<std::string>{"comp.lang.apl: 1-3"}), read_lines(newsrc.name));
    EXPECT_FALSE(fs::exists(newsrc.new_name));
}

TEST_F(NewsrcRotationTest, getOldNewsrcsRestoresBackupFile)
{
    Newsrc  newsrc = make_newsrc();
    Multirc multirc{};
    multirc.m_first = &newsrc;
    std::ofstream{newsrc.name} << "current contents\n";
    std::ofstream{newsrc.old_name} << "old contents\n";
    std::ofstream{newsrc.new_name} << "temporary contents\n";

    get_old_newsrcs(&multirc);

    EXPECT_EQ((std::vector<std::string>{"old contents"}), read_lines(newsrc.name));
    EXPECT_EQ((std::vector<std::string>{"current contents"}), read_lines(newsrc.new_name));
    EXPECT_FALSE(fs::exists(newsrc.old_name));
}
