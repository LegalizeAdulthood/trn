// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/datasrc.h>

#include <config/common.h>
#include <trn/hash.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

namespace fs = std::filesystem;

class SourceFileOwner
{
public:
    SourceFileOwner()
    {
        m_source.m_hp = hash_create(17, nullptr);
    }

    SourceFileOwner(const SourceFileOwner &) = delete;
    SourceFileOwner &operator=(const SourceFileOwner &) = delete;

    ~SourceFileOwner()
    {
        m_source.close();
    }

    SourceFile &get()
    {
        return m_source;
    }

private:
    SourceFile m_source{};
};

class SourceFileTest : public testing::Test
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
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    fs::path m_output_dir;
};

} // namespace

TEST(SourceFileAppendTest, storesNormalizedLineAndReturnsStoredStorage)
{
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();
    char            line[] = "comp.lang.c++     C++ language discussion";
    const int       key_len = static_cast<int>(std::strlen("comp.lang.c++"));

    const std::string_view stored_line = source_file.append(line, key_len);

    ASSERT_EQ(1U, source_file.m_lines.size());
    EXPECT_EQ(source_file.m_lines.back().data(), stored_line.data());
    EXPECT_EQ("comp.lang.c++ C++ language discussion\n", stored_line);
    EXPECT_EQ("comp.lang.c++ C++ language discussion\n", source_file.m_lines.back());
    EXPECT_EQ("C++ language discussion\n", std::string_view{stored_line.data() + key_len + 1});
}

TEST_F(SourceFileTest, openReadsLinesFromLocalFile)
{
    const fs::path source_path = m_output_dir / "active";
    std::ofstream{source_path} << "comp.lang.apl 0000000001 0000000001 y\n"
                               << "comp.lang.c++ 0000000002 0000000001 y\n";
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();

    const int result = source_file.open(source_path, "", nullptr);

    ASSERT_EQ(1, result);
    ASSERT_EQ(2U, source_file.m_lines.size());
    EXPECT_EQ("comp.lang.apl 0000000001 0000000001 y\n", source_file.m_lines[0]);
    EXPECT_EQ("comp.lang.c++ 0000000002 0000000001 y\n", source_file.m_lines[1]);
    EXPECT_EQ(0L, source_file.m_line_positions[0]);
}

TEST_F(SourceFileTest, endAppendUpdatesCachedFileTimestamp)
{
    const fs::path source_path = m_output_dir / "active";
    std::ofstream{source_path} << "comp.lang.apl 0000000001 0000000001 y\n";
    SourceFileOwner source_file_owner;
    SourceFile     &source_file = source_file_owner.get();
    source_file.m_fp = std::fopen(source_path.string().c_str(), "r+");
    ASSERT_NE(nullptr, source_file.m_fp);
    source_file.m_refetch_secs = DEFAULT_REFETCH_SECS;
    source_file.m_last_fetch = 946684800;

    source_file.end_append(source_path);

    struct stat file_stat;
    ASSERT_EQ(0, stat(source_path.string().c_str(), &file_stat));
    EXPECT_EQ(source_file.m_last_fetch, file_stat.st_mtime);
}

TEST_F(SourceFileTest, closeRemovesTemporaryRemoteFiles)
{
    const fs::path active_path = m_output_dir / "active";
    const fs::path group_desc_path = m_output_dir / "newsgroups";
    std::ofstream{active_path} << "comp.lang.apl 0000000001 0000000001 y\n";
    std::ofstream{group_desc_path} << "comp.lang.apl APL discussion\n";
    DataSource data_source{};
    data_source.m_flags = DF_REMOTE | DF_TMP_ACTIVE_FILE | DF_TMP_GROUP_DESC;
    data_source.m_extra_name = active_path.generic_string();
    data_source.m_group_desc = group_desc_path.generic_string();

    data_source.close();

    EXPECT_FALSE(fs::exists(active_path));
    EXPECT_FALSE(fs::exists(group_desc_path));
}

TEST_F(SourceFileTest, findGroupDescClearsMissingTemporaryGroupDescription)
{
    const fs::path group_desc_path = m_output_dir / "missing-newsgroups";
    DataSource     data_source{};
    data_source.m_flags = DF_TMP_GROUP_DESC;
    data_source.m_group_desc = group_desc_path.generic_string();

    EXPECT_STREQ("", data_source.find_group_desc("comp.lang.apl"));

    EXPECT_TRUE(data_source.m_group_desc.empty());
    EXPECT_FALSE(data_source.m_flags & DF_TMP_GROUP_DESC);
}
