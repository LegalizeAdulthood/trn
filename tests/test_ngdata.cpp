// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/ngdata.h>

#include <trn/datasrc.h>
#include <trn/rcstuff.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class NewsgroupSizeTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_in_ng = g_in_ng;
        m_old_moderated = g_moderated;
        m_old_redirected = g_redirected;
        m_old_redirected_to = g_redirected_to;

        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        g_in_ng = false;
        g_moderated.clear();
        g_redirected = false;
        g_redirected_to.clear();

        m_newsrc.data_source = &m_source;
        m_group.m_rc = &m_newsrc;
        m_group.m_rc_line = "comp.lang.apl: ";
        m_group.m_num_offset = static_cast<int>(std::string_view{"comp.lang.apl"}.size()) + 1;
        m_group.m_subscribe_char = ':';
        m_group.m_ng_max = ArticleNum{5};
        m_group.m_abs_first = ArticleNum{};
    }

    void TearDown() override
    {
        m_source.m_act_sf.close();

        std::error_code error;
        fs::remove_all(m_output_dir, error);

        g_in_ng = m_old_in_ng;
        g_moderated = m_old_moderated;
        g_redirected = m_old_redirected;
        g_redirected_to = m_old_redirected_to;
    }

    void open_active_file(std::string_view active_line)
    {
        const fs::path active_path = m_output_dir / "active";
        std::ofstream{active_path} << active_line;
        m_source.m_flags = DF_OPEN;
        ASSERT_EQ(1, m_source.m_act_sf.open(active_path, "", ""));
    }

    fs::path      m_output_dir;
    DataSource    m_source{};
    Newsrc        m_newsrc{};
    NewsgroupData m_group{};
    bool          m_old_in_ng{};
    std::string   m_old_moderated;
    bool          m_old_redirected{};
    std::string   m_old_redirected_to;
};

} // namespace

TEST_F(NewsgroupSizeTest, parsesActiveFieldNumbersAndStatus)
{
    open_active_file("comp.lang.apl 0000000042 0000000007 x\n");

    const ArticleNum size = m_group.get_newsgroup_size();

    EXPECT_EQ(ArticleNum{42}, size);
    EXPECT_EQ(ArticleNum{42}, m_group.m_ng_max);
    EXPECT_EQ(ArticleNum{7}, m_group.m_abs_first);
    EXPECT_TRUE(g_redirected);
    EXPECT_TRUE(g_redirected_to.empty());
    EXPECT_EQ(" (DISABLED)", g_moderated);
}
