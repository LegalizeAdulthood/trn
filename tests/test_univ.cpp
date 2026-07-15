// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/univ.h>

#include <trn/ng.h>
#include <trn/ngdata.h>
#include <trn/util.h>
#include <util/util2.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <new>
#include <string>

namespace
{

namespace fs = std::filesystem;

std::string g_visited_group;
int         g_visit_count{};

void reset_test_newsgroups()
{
    g_newsgroup_data.clear();
    g_newsgroup_order.clear();
    g_newsgroup_count = NewsgroupNum{};
    g_newsgroup_to_read = NewsgroupNum{};
    g_first_newsgroup = nullptr;
    g_last_newsgroup = nullptr;
    g_newsgroup_ptr = nullptr;
    g_current_newsgroup = nullptr;
    g_recent_newsgroup = nullptr;
    g_start_here = nullptr;
    g_sel_page_np = nullptr;
    g_sel_next_np = nullptr;
}

NewsgroupData *add_test_newsgroup(std::string_view group_name)
{
    NewsgroupData &group = g_newsgroup_data.emplace_back();
    group.m_rc_line = group_name;
    group.m_to_read = 1;
    append_newsgroup_order(&group);
    g_newsgroup_count = NewsgroupNum{static_cast<int>(g_newsgroup_data.size())};
    g_newsgroup_to_read = g_newsgroup_count;
    return &group;
}

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

UniversalItem *make_universal_item(UniversalItemType type)
{
    UniversalItem *item = reinterpret_cast<UniversalItem *>(safe_malloc(sizeof(UniversalItem)));
    item->m_next = nullptr;
    item->m_prev = nullptr;
    item->m_num = 1;
    item->m_flags = UF_NONE;
    item->m_type = type;
    item->m_state = UIS_NORMAL;
    item->m_desc = nullptr;
    item->m_score = 0;
    if (type == UN_NEWSGROUP)
    {
        new (&item->m_data.group) UniversalNewsgroup{};
    }
    else if (type == UN_VGROUP)
    {
        new (&item->m_data.vgroup) UniversalVirtualGroup{};
    }
    else if (type == UN_ARTICLE)
    {
        new (&item->m_data.virt) UniversalVirtualData{};
    }
    else if (type == UN_CONFIG_FILE)
    {
        new (&item->m_data.cfile) UniversalConfigFileData{};
    }
    else if (type == UN_GROUP_MASK)
    {
        new (&item->m_data.gmask) UniversalGroupMaskData{};
    }
    else if (type == UN_TEXT_FILE)
    {
        new (&item->m_data.text_file) UniversalTextFile{};
    }
    return item;
}

UniversalItem *make_newsgroup_item(std::string_view group_name)
{
    UniversalItem *item = make_universal_item(UN_NEWSGROUP);
    item->group().ng = group_name;
    return item;
}

UniversalItem *make_virtual_group(std::string_view group_name)
{
    UniversalItem *item = make_universal_item(UN_VGROUP);
    UniversalVirtualGroup &vgroup = item->vgroup();
    vgroup.ng = group_name;
    vgroup.min_score = 0;
    vgroup.max_score = 0;
    vgroup.flags = UF_VG_NONE;
    return item;
}

UniversalItem *make_numbered_article(std::string_view group_name)
{
    UniversalItem *item = make_universal_item(UN_ARTICLE);
    item->m_desc = save_str("Article");
    UniversalVirtualData &article = item->article();
    article.ng = group_name;
    article.num = ArticleNum{1};
    return item;
}

void append_universal_item(UniversalItem *item)
{
    item->m_prev = g_last_univ;
    if (g_last_univ)
    {
        g_last_univ->m_next = item;
    }
    else
    {
        g_first_univ = item;
    }
    g_last_univ = item;
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
    reset_fake_visit_group();
    reset_test_newsgroups();
}

void UnivTest::TearDown()
{
    while (g_univ_level > 0)
    {
        univ_close();
    }
    reset_fake_visit_group();
    reset_test_newsgroups();
}

} // namespace

TEST_F(UnivTest, maskLoadAcceptsStringLiteral)
{
    univ_mask_load("", "Empty");

    EXPECT_EQ(nullptr, g_first_univ);
    EXPECT_EQ("Empty", g_univ_title);
}

TEST_F(UnivTest, groupMaskExclusionMarksExistingGroup)
{
    add_test_newsgroup("alt.test");

    univ_mask_load("alt.test !alt.test", "Groups");

    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_NEWSGROUP, g_first_univ->m_type);
    EXPECT_EQ(UIS_DESELECTED, g_first_univ->m_state);
    EXPECT_EQ("alt.test", g_first_univ->group().ng);
    EXPECT_EQ(nullptr, g_first_univ->m_next);
}

TEST_F(UnivTest, groupMaskRestoresDeselectedGroup)
{
    add_test_newsgroup("alt.test");

    univ_mask_load("alt.test !alt.test alt.test", "Groups");

    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_NEWSGROUP, g_first_univ->m_type);
    EXPECT_EQ(UIS_NORMAL, g_first_univ->m_state);
    EXPECT_EQ("alt.test", g_first_univ->group().ng);
    EXPECT_EQ(nullptr, g_first_univ->m_next);
}

TEST_F(UnivTest, fileLoadCreatesGroupMaskItem)
{
    const std::string file_name = fs::path{TRN_TEST_UNIV_GROUP_MASK_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(file_name.c_str(), "Top", nullptr));
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_GROUP_MASK, g_first_univ->m_type);
    EXPECT_STREQ("Filter", g_first_univ->m_desc);
    EXPECT_EQ("Filter", g_first_univ->group_mask().title);
    EXPECT_EQ("alt.test !alt.noise", g_first_univ->group_mask().mask_list);
}

TEST_F(UnivTest, fileLoadCreatesTextFileItem)
{
    const fs::path selector_path{TRN_TEST_UNIV_TEXT_FILE};
    const std::string file_name = selector_path.generic_string();
    const std::string help_name = (selector_path.parent_path() / "help.txt").generic_string();

    ASSERT_TRUE(univ_file_load(file_name.c_str(), "Top", nullptr));
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_TEXT_FILE, g_first_univ->m_type);
    EXPECT_STREQ("Help", g_first_univ->m_desc);
    EXPECT_EQ(file_exp(help_name), g_first_univ->text_file().fname);
}

TEST_F(UnivTest, virtualPassUsesInjectedVisitor)
{
    univ_mask_load("", "Virtual");
    UniversalItem *expanded_group = make_virtual_group("alt.test");
    UniversalItem *kept_group = make_newsgroup_item("alt.keep");
    UniversalItem *kept_article = make_numbered_article("alt.article");
    append_universal_item(expanded_group);
    append_universal_item(kept_group);
    append_universal_item(kept_article);

    univ_virt_pass(fake_visit_group);

    EXPECT_EQ(1, g_visit_count);
    EXPECT_EQ("alt.test", g_visited_group);
    EXPECT_EQ(UN_VGROUP, expanded_group->m_type);
    EXPECT_EQ(UIS_DELETED, expanded_group->m_state);
    EXPECT_EQ(UN_NEWSGROUP, kept_group->m_type);
    EXPECT_EQ(UIS_NORMAL, kept_group->m_state);
    EXPECT_EQ("alt.keep", kept_group->group().ng);
    EXPECT_EQ(UN_ARTICLE, kept_article->m_type);
    EXPECT_EQ(UIS_NORMAL, kept_article->m_state);
    EXPECT_STREQ("Article", kept_article->m_desc);
    EXPECT_EQ("alt.article", kept_article->article().ng);
    EXPECT_EQ(ArticleNum{1}, kept_article->article().num);
    EXPECT_FALSE(g_univ_ng_virt_flag);

    univ_virt_pass(fake_visit_group);

    EXPECT_EQ(1, g_visit_count);
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
    EXPECT_EQ(file_exp(child_name), g_first_univ->config_file().fname);
    EXPECT_TRUE(g_first_univ->config_file().label.empty());
}

TEST_F(UnivTest, colonPathLabelIsRelativeToCurrentUniversalFile)
{
    const std::string top_name = fs::path{TRN_TEST_UNIV_COLON_PATH_LABEL_FILE}.generic_string();
    const std::string child_name = fs::path{TRN_TEST_UNIV_CHILD_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(top_name.c_str(), "Top", nullptr));
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_CONFIG_FILE, g_first_univ->m_type);
    EXPECT_STREQ("Child", g_first_univ->m_desc);
    EXPECT_EQ(file_exp(child_name), g_first_univ->config_file().fname);
    EXPECT_EQ("chapter", g_first_univ->config_file().label);
}
