#include <trn/util.h>

#include "test_makedir.h"

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>

namespace
{

class TestMakeDir : public testing::Test
{
public:
    ~TestMakeDir() override = default;

protected:
    void SetUp() override;
    void TearDown() override;

    void setup_buffer(const std::filesystem::path &path);

    std::filesystem::path m_path{TEST_MAKEDIR_BASE};
    char m_buffer[256]{};
};

void TestMakeDir::SetUp()
{
    Test::SetUp();
    ASSERT_TRUE(std::filesystem::exists(TEST_MAKEDIR_BASE));
}

void TestMakeDir::TearDown()
{
    Test::TearDown();
}

void TestMakeDir::setup_buffer(const std::filesystem::path &path)
{
    std::strcpy(m_buffer, path.string().c_str());
    for (char *backslash = std::strchr(m_buffer, '\\'); backslash != nullptr; backslash = std::strchr(m_buffer, '\\'))
    {
        *backslash = '/';
    }
}

} // namespace

TEST_F(TestMakeDir, directoryExists)
{
    setup_buffer(m_path);

    bool result{makedir(m_buffer, MD_DIR)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(std::filesystem::path{TEST_MAKEDIR_BASE}));
}

TEST_F(TestMakeDir, fileDirectoryExists)
{
    std::filesystem::path file{TEST_MAKEDIR_BASE};
    file /= "file.txt";
    setup_buffer(file);

    bool result{makedir(m_buffer, MD_FILE)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(std::filesystem::exists(TEST_MAKEDIR_BASE));
    EXPECT_FALSE(exists(file));
}

TEST_F(TestMakeDir, createDirectory)
{
    std::filesystem::path dir{TEST_MAKEDIR_BASE};
    dir /= "create_dir";
    remove_all(dir);
    ASSERT_FALSE(exists(dir));
    setup_buffer(dir);

    bool result{makedir(m_buffer, MD_DIR)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
}

TEST_F(TestMakeDir, fileCreateDirectory)
{
    std::filesystem::path dir{TEST_MAKEDIR_BASE};
    dir /= "file_create_dir";
    remove_all(dir);
    std::filesystem::path file{dir / "file.txt"};
    ASSERT_FALSE(exists(dir));
    ASSERT_FALSE(exists(file));
    setup_buffer(file);

    bool result{makedir(m_buffer, MD_FILE)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
    EXPECT_FALSE(exists(file));
}


TEST_F(TestMakeDir, createSubDirectory)
{
    std::filesystem::path dir{TEST_MAKEDIR_BASE};
    dir /= "create_sub_dir";
    dir /= "sub_dir";
    remove_all(dir);
    ASSERT_FALSE(exists(dir));
    setup_buffer(dir);

    bool result{makedir(m_buffer, MD_DIR)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
}

TEST_F(TestMakeDir, fileCreateSubDirectory)
{
    std::filesystem::path dir{TEST_MAKEDIR_BASE};
    dir = dir / "file_create_sub_dir" / "sub_dir";
    remove_all(dir);
    std::filesystem::path file{dir / "file.txt"};
    ASSERT_FALSE(exists(dir));
    ASSERT_FALSE(exists(file));
    setup_buffer(file);

    bool result{makedir(m_buffer, MD_FILE)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
    EXPECT_FALSE(exists(file));
}
