// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/univ.h>

#include <util/util2.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace
{

namespace fs = std::filesystem;

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
