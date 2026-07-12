// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/univ.h>

#include <util/util2.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{

std::filesystem::path test_root()
{
    return std::filesystem::temp_directory_path() / "trn-univ-test";
}

class UnivTest : public testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    std::filesystem::path m_root{test_root()};
};

void UnivTest::SetUp()
{
    while (g_univ_level > 0)
    {
        univ_close();
    }

    std::error_code error;
    std::filesystem::remove_all(m_root, error);
    std::filesystem::create_directories(m_root);
}

void UnivTest::TearDown()
{
    while (g_univ_level > 0)
    {
        univ_close();
    }

    std::error_code error;
    std::filesystem::remove_all(m_root, error);
}

} // namespace

TEST_F(UnivTest, colonPathIsRelativeToCurrentUniversalFile)
{
    const std::filesystem::path parent = m_root / "parent";
    const std::filesystem::path top = parent / "top.univ";
    const std::filesystem::path child = parent / "child.univ";
    std::filesystem::create_directories(parent);

    std::ofstream output{top, std::ios::binary};
    ASSERT_TRUE(output.good());
    output << "\"Child\" :child.univ\n";
    output.close();

    const std::string top_name = top.generic_string();
    const std::string child_name = child.generic_string();

    ASSERT_TRUE(univ_file_load(top_name.c_str(), "Top", nullptr));
    ASSERT_NE(nullptr, g_first_univ);
    EXPECT_EQ(UN_CONFIG_FILE, g_first_univ->m_type);
    EXPECT_STREQ("Child", g_first_univ->m_desc);
    EXPECT_EQ(file_exp(child_name), g_first_univ->m_data.cfile.fname);
    EXPECT_EQ(nullptr, g_first_univ->m_data.cfile.label);
}
