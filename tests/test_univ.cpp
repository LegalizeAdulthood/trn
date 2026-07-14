// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/univ.h>

#include <trn/ng.h>
#include <trn/util.h>
#include <util/util2.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{

namespace fs = std::filesystem;

std::string g_visited_group;
int         g_visit_count{};

int fake_visit_group(const char *group_name)
{
    ++g_visit_count;
    g_visited_group = group_name ? group_name : "";
    return NG_NORM;
}

void reset_fake_visit_group()
{
    g_visited_group.clear();
    g_visit_count = 0;
}

UniversalItem *make_virtual_group(std::string_view group_name)
{
    UniversalItem *item = reinterpret_cast<UniversalItem *>(safe_malloc(sizeof(UniversalItem)));
    item->m_next = nullptr;
    item->m_prev = nullptr;
    item->m_num = 1;
    item->m_flags = UF_NONE;
    item->m_type = UN_VGROUP;
    item->m_desc = nullptr;
    item->m_score = 0;
    item->m_data.vgroup.ng = save_str(group_name);
    item->m_data.vgroup.min_score = 0;
    item->m_data.vgroup.max_score = 0;
    item->m_data.vgroup.flags = UF_VG_NONE;
    return item;
}

class UnivTest : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;
};

void UnivTest::SetUp()
{
    while (g_univ_level > 0)
    {
        univ_close();
    }
}

void UnivTest::TearDown()
{
    while (g_univ_level > 0)
    {
        univ_close();
    }
}

} // namespace

TEST_F(UnivTest, maskLoadAcceptsStringLiteral)
{
    univ_mask_load("", "Empty");

    EXPECT_EQ(nullptr, g_first_univ);
    EXPECT_EQ("Empty", g_univ_title);
}

TEST_F(UnivTest, virtualPassUsesInjectedVisitor)
{
    reset_fake_visit_group();
    univ_mask_load("", "Virtual");
    g_first_univ = make_virtual_group("alt.test");
    g_last_univ = g_first_univ;

    univ_virt_pass(fake_visit_group);

    EXPECT_EQ(1, g_visit_count);
    EXPECT_EQ("alt.test", g_visited_group);
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_DELETED, g_first_univ->m_type);
    EXPECT_FALSE(g_univ_ng_virt_flag);
}

TEST_F(UnivTest, colonPathIsRelativeToCurrentUniversalFile)
{
    const std::string top_name = fs::path{TRN_TEST_UNIV_COLON_PATH_FILE}.generic_string();
    const std::string child_name = fs::path{TRN_TEST_UNIV_CHILD_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(top_name.c_str(), "Top", nullptr));
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_CONFIG_FILE, g_first_univ->m_type);
    EXPECT_STREQ("Child", g_first_univ->m_desc);
    EXPECT_EQ(file_exp(child_name), g_first_univ->m_data.cfile.fname);
    EXPECT_EQ(nullptr, g_first_univ->m_data.cfile.label);
}

TEST_F(UnivTest, colonPathLabelIsRelativeToCurrentUniversalFile)
{
    const std::string top_name = fs::path{TRN_TEST_UNIV_COLON_PATH_LABEL_FILE}.generic_string();
    const std::string child_name = fs::path{TRN_TEST_UNIV_CHILD_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(top_name.c_str(), "Top", nullptr));
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_CONFIG_FILE, g_first_univ->m_type);
    EXPECT_STREQ("Child", g_first_univ->m_desc);
    EXPECT_EQ(file_exp(child_name), g_first_univ->m_data.cfile.fname);
    EXPECT_STREQ("chapter", g_first_univ->m_data.cfile.label);
}
