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
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <variant>

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

bool no_input_pending()
{
    return false;
}

void reset_fake_visit_group()
{
    g_visited_group.clear();
    g_visit_count = 0;
}

UniversalItem make_universal_item(UniversalData data)
{
    UniversalItem item;
    item.m_flags = UF_NONE;
    item.m_state = UIS_NORMAL;
    item.m_score = 0;
    item.m_data = std::move(data);
    return item;
}

UniversalItem make_newsgroup_item(std::string_view group_name)
{
    UniversalItem item = make_universal_item(UniversalNewsgroup{});
    item.group().ng = group_name;
    return item;
}

UniversalItem make_virtual_group(std::string_view group_name)
{
    UniversalItem          item = make_universal_item(UniversalVirtualGroup{});
    UniversalVirtualGroup &vgroup = item.vgroup();
    vgroup.ng = group_name;
    vgroup.min_score = 0;
    vgroup.max_score = 0;
    vgroup.flags = UF_VG_NONE;
    return item;
}

UniversalItem make_numbered_article(std::string_view group_name)
{
    UniversalItem item = make_universal_item(UniversalVirtualArticle{});
    item.m_desc = "Article";
    UniversalVirtualArticle &article = item.article();
    article.ng = group_name;
    article.num = ArticleNum{1};
    return item;
}

UniversalItem make_undescribed_numbered_article(std::string_view group_name)
{
    UniversalItem         item = make_universal_item(UniversalVirtualArticle{});
    UniversalVirtualArticle &article = item.article();
    article.ng = group_name;
    article.num = ArticleNum{1};
    return item;
}

UniversalItem *append_universal_item(UniversalItem item)
{
    if (!item.m_num)
    {
        item.m_num = static_cast<UniversalItemIndex>(g_univ_items.size() + 1);
    }
    g_univ_items.push_back(std::move(item));
    return &g_univ_items.back();
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

    EXPECT_EQ(nullptr, univ_first_item());
    EXPECT_EQ("Empty", g_univ_title);
}

TEST_F(UnivTest, closeRemovesTemporaryFile)
{
    const fs::path  temp_dir = fs::path{TRN_TEST_TMP_DIR} / "UnivTest" / "closeRemovesTemporaryFile";
    std::error_code error;
    fs::remove_all(temp_dir, error);
    fs::create_directories(temp_dir, error);
    ASSERT_FALSE(error) << error.message();
    const fs::path temp_file = temp_dir / "univ.tmp";
    std::ofstream{temp_file} << "temporary\n";

    univ_mask_load("", "Temp");
    g_univ_tmp_file = temp_file.generic_string();

    univ_close();

    EXPECT_TRUE(g_univ_tmp_file.empty());
    EXPECT_FALSE(fs::exists(temp_file));
    fs::remove_all(temp_dir, error);
}

TEST_F(UnivTest, groupMaskExclusionMarksExistingGroup)
{
    add_test_newsgroup("alt.test");

    univ_mask_load("alt.test !alt.test", "Groups");

    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalNewsgroup>(item->m_data));
    EXPECT_EQ(UIS_DESELECTED, item->m_state);
    EXPECT_EQ("alt.test", item->group().ng);
    EXPECT_EQ(nullptr, univ_next_item(item));
}

TEST_F(UnivTest, groupMaskRestoresDeselectedGroup)
{
    add_test_newsgroup("alt.test");

    univ_mask_load("alt.test !alt.test alt.test", "Groups");

    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalNewsgroup>(item->m_data));
    EXPECT_EQ(UIS_NORMAL, item->m_state);
    EXPECT_EQ("alt.test", item->group().ng);
    EXPECT_EQ(nullptr, univ_next_item(item));
}

TEST_F(UnivTest, groupMaskWildcardSelectsMatchingGroups)
{
    g_newsgroup_data.reserve(3);
    add_test_newsgroup("alt.test");
    add_test_newsgroup("comp.test");
    add_test_newsgroup("alt.noise");

    univ_mask_load("alt.*", "Groups");

    UniversalItem *first = univ_first_item();
    ASSERT_NE(nullptr, first);
    EXPECT_TRUE(std::holds_alternative<UniversalNewsgroup>(first->m_data));
    EXPECT_EQ(UIS_NORMAL, first->m_state);
    EXPECT_EQ("alt.test", first->group().ng);

    UniversalItem *second = univ_next_item(first);
    ASSERT_NE(nullptr, second);
    EXPECT_TRUE(std::holds_alternative<UniversalNewsgroup>(second->m_data));
    EXPECT_EQ(UIS_NORMAL, second->m_state);
    EXPECT_EQ("alt.noise", second->group().ng);
    EXPECT_EQ(nullptr, univ_next_item(second));
}

TEST_F(UnivTest, fileLoadCreatesGroupMaskItem)
{
    const std::string file_name = fs::path{TRN_TEST_UNIV_GROUP_MASK_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(file_name, "Top", {}));
    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalGroupMaskData>(item->m_data));
    EXPECT_EQ("Filter", item->m_desc);
    EXPECT_EQ("Filter", item->group_mask().title);
    EXPECT_EQ("alt.test !alt.noise", item->group_mask().mask_list);
}

TEST_F(UnivTest, fileLoadAllowsEscapedQuoteInDescription)
{
    const fs::path  temp_dir = fs::path{TRN_TEST_TMP_DIR} / "UnivTest" / "fileLoadAllowsEscapedQuoteInDescription";
    std::error_code error;
    fs::remove_all(temp_dir, error);
    fs::create_directories(temp_dir, error);
    ASSERT_FALSE(error) << error.message();
    const fs::path selector_file = temp_dir / "selector.univ";
    std::ofstream{selector_file} << "\"Filter \\\"Name\\\"\" alt.test !alt.noise\n";

    ASSERT_TRUE(univ_file_load(selector_file.generic_string(), "Top", {}));
    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalGroupMaskData>(item->m_data));
    EXPECT_EQ("Filter \"Name\"", item->m_desc);
    EXPECT_EQ("Filter \"Name\"", item->group_mask().title);
    EXPECT_EQ("alt.test !alt.noise", item->group_mask().mask_list);
    fs::remove_all(temp_dir, error);
}

TEST_F(UnivTest, fileLoadCreatesTextFileItem)
{
    const fs::path selector_path{TRN_TEST_UNIV_TEXT_FILE};
    const std::string file_name = selector_path.generic_string();
    const std::string help_name = (selector_path.parent_path() / "help.txt").generic_string();

    ASSERT_TRUE(univ_file_load(file_name, "Top", {}));
    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalTextFile>(item->m_data));
    EXPECT_EQ("Help", item->m_desc);
    EXPECT_EQ(fs::path{file_exp(help_name)}, item->text_file().fname);
}

TEST_F(UnivTest, fileLoadCreatesNumberedVirtualArticle)
{
    const fs::path  temp_dir = fs::path{TRN_TEST_TMP_DIR} / "UnivTest" / "fileLoadCreatesNumberedVirtualArticle";
    std::error_code error;
    fs::remove_all(temp_dir, error);
    fs::create_directories(temp_dir, error);
    ASSERT_FALSE(error) << error.message();
    const fs::path selector_file = temp_dir / "selector.univ";
    std::ofstream{selector_file} << "\"Article\" $v1 1500 news.admin\n";

    ASSERT_TRUE(univ_file_load(selector_file.generic_string(), "Top", {}));
    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalVirtualArticle>(item->m_data));
    EXPECT_EQ("Article", item->m_desc);
    EXPECT_EQ("news.admin", item->article().ng);
    EXPECT_EQ(ArticleNum{1500}, item->article().num);
    EXPECT_EQ(nullptr, univ_next_item(item));
    fs::remove_all(temp_dir, error);
}

TEST_F(UnivTest, fileLoadParsesVirtualGroupExtensionWithoutMatches)
{
    const fs::path temp_dir =
        fs::path{TRN_TEST_TMP_DIR} / "UnivTest" / "fileLoadParsesVirtualGroupExtensionWithoutMatches";
    std::error_code error;
    fs::remove_all(temp_dir, error);
    fs::create_directories(temp_dir, error);
    ASSERT_FALSE(error) << error.message();
    const fs::path selector_file = temp_dir / "selector.univ";
    std::ofstream{selector_file} << "\"Virtual\" $vg +5 no.match\n";

    ASSERT_TRUE(univ_file_load(selector_file.generic_string(), "Top", {}));
    EXPECT_EQ(nullptr, univ_first_item());
    fs::remove_all(temp_dir, error);
}

TEST_F(UnivTest, debugItemStoresStringPayload)
{
    UniversalItem item = make_universal_item(UniversalDebugData{});
    item.debug_string() = "debug item";
    UniversalItem *stored_item = append_universal_item(std::move(item));

    EXPECT_EQ("debug item", stored_item->debug_string());
}

TEST_F(UnivTest, itemIndexFindsStableListPosition)
{
    univ_mask_load("", "Index");
    UniversalItem first = make_universal_item(UniversalTextPlaceholder{});
    UniversalItem second = make_universal_item(UniversalDebugData{});
    first.m_num = 41;
    second.m_num = 42;
    append_universal_item(std::move(first));
    append_universal_item(std::move(second));

    EXPECT_EQ(UniversalItemIndex{}, univ_index(nullptr));
    EXPECT_EQ(41, univ_index(univ_item(41)));
    EXPECT_EQ(nullptr, univ_item(99));

    UniversalItem *second_item = univ_item(42);
    ASSERT_NE(nullptr, second_item);
    UniversalItems        items = univ_items(42);
    UniversalItemIterator iter = items.begin();
    ASSERT_NE(items.end(), iter);
    EXPECT_EQ(second_item, &*iter);
    ++iter;
    EXPECT_EQ(items.end(), iter);
}

TEST_F(UnivTest, virtualPassUsesInjectedVisitor)
{
    univ_mask_load("", "Virtual");
    UniversalItem           *expanded_group = append_universal_item(make_virtual_group("alt.test"));
    const UniversalItemIndex expanded_group_index = univ_index(expanded_group);
    UniversalItem           *kept_group = append_universal_item(make_newsgroup_item("alt.keep"));
    const UniversalItemIndex kept_group_index = univ_index(kept_group);
    UniversalItem           *kept_article = append_universal_item(make_numbered_article("alt.article"));
    const UniversalItemIndex kept_article_index = univ_index(kept_article);

    univ_virt_pass(fake_visit_group, no_input_pending);

    kept_group = univ_item(kept_group_index);
    kept_article = univ_item(kept_article_index);
    ASSERT_NE(nullptr, kept_group);
    ASSERT_NE(nullptr, kept_article);
    EXPECT_EQ(1, g_visit_count);
    EXPECT_EQ("alt.test", g_visited_group);
    EXPECT_EQ(nullptr, univ_item(expanded_group_index));
    EXPECT_TRUE(std::holds_alternative<UniversalNewsgroup>(kept_group->m_data));
    EXPECT_EQ(UIS_NORMAL, kept_group->m_state);
    EXPECT_EQ("alt.keep", kept_group->group().ng);
    EXPECT_TRUE(std::holds_alternative<UniversalVirtualArticle>(kept_article->m_data));
    EXPECT_EQ(UIS_NORMAL, kept_article->m_state);
    EXPECT_EQ("Article", kept_article->m_desc);
    EXPECT_EQ("alt.article", kept_article->article().ng);
    EXPECT_EQ(ArticleNum{1}, kept_article->article().num);
    EXPECT_FALSE(g_univ_ng_virt_flag);

    univ_virt_pass(fake_visit_group, no_input_pending);

    EXPECT_EQ(1, g_visit_count);
    EXPECT_FALSE(g_univ_ng_virt_flag);
}

TEST_F(UnivTest, virtualPassExpandsNumberedArticleWithoutDescription)
{
    univ_mask_load("", "Virtual");
    append_universal_item(make_undescribed_numbered_article("alt.article"));

    univ_virt_pass(fake_visit_group, no_input_pending);

    EXPECT_EQ(1, g_visit_count);
    EXPECT_EQ("alt.article", g_visited_group);
}

TEST_F(UnivTest, articleDescriptionFormatsScoreAuthorAndSubject)
{
    UniversalItem item = make_undescribed_numbered_article("alt.article");
    item.m_score = 7;

    EXPECT_EQ("[  7]     <No Author>  <No Subject>", std::string{item.univ_article_desc()});

    item.article().subj = "Re: abc\tdef";

    EXPECT_EQ("[  7]     <No Author>  >abc def", std::string{item.univ_article_desc()});
}

TEST_F(UnivTest, colonPathIsRelativeToCurrentUniversalFile)
{
    const std::string top_name = fs::path{TRN_TEST_UNIV_COLON_PATH_FILE}.generic_string();
    const std::string child_name = fs::path{TRN_TEST_UNIV_CHILD_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(top_name, "Top", {}));
    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalConfigFileData>(item->m_data));
    EXPECT_EQ("Child", item->m_desc);
    EXPECT_EQ(file_exp(child_name), item->config_file().fname);
    EXPECT_TRUE(item->config_file().label.empty());
}

TEST_F(UnivTest, colonPathLabelIsRelativeToCurrentUniversalFile)
{
    const std::string top_name = fs::path{TRN_TEST_UNIV_COLON_PATH_LABEL_FILE}.generic_string();
    const std::string child_name = fs::path{TRN_TEST_UNIV_CHILD_FILE}.generic_string();

    ASSERT_TRUE(univ_file_load(top_name, "Top", {}));
    UniversalItem *item = univ_first_item();
    ASSERT_NE(nullptr, item);
    EXPECT_TRUE(std::holds_alternative<UniversalConfigFileData>(item->m_data));
    EXPECT_EQ("Child", item->m_desc);
    EXPECT_EQ(file_exp(child_name), item->config_file().fname);
    EXPECT_EQ("chapter", item->config_file().label);
}
