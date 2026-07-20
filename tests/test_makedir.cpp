// This software is copyrighted as detailed in the LICENSE file.
#include <trn/util.h>

#include <test_makedir.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace
{

class TestMakeDir : public testing::Test
{
public:
    ~TestMakeDir() override = default;

protected:
    void SetUp() override;
    void TearDown() override;

    fs::path m_path{TEST_MAKEDIR_BASE};
};

void TestMakeDir::SetUp()
{
    Test::SetUp();
    ASSERT_TRUE(fs::exists(TEST_MAKEDIR_BASE));
}

void TestMakeDir::TearDown()
{
    Test::TearDown();
}

} // namespace

TEST_F(TestMakeDir, directoryExists)
{
    bool result{make_dir(m_path, MD_DIR)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(fs::path{TEST_MAKEDIR_BASE}));
}

TEST_F(TestMakeDir, fileDirectoryExists)
{
    fs::path file{TEST_MAKEDIR_BASE};
    file /= "file.txt";

    bool result{make_dir(file, MD_FILE)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(fs::exists(TEST_MAKEDIR_BASE));
    EXPECT_FALSE(exists(file));
}

TEST_F(TestMakeDir, createDirectory)
{
    fs::path dir{TEST_MAKEDIR_BASE};
    dir /= "create_dir";
    remove_all(dir);
    ASSERT_FALSE(exists(dir));

    bool result{make_dir(dir, MD_DIR)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
}

TEST_F(TestMakeDir, fileCreateDirectory)
{
    fs::path dir{TEST_MAKEDIR_BASE};
    dir /= "file_create_dir";
    remove_all(dir);
    fs::path file{dir / "file.txt"};
    ASSERT_FALSE(exists(dir));
    ASSERT_FALSE(exists(file));

    bool result{make_dir(file, MD_FILE)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
    EXPECT_FALSE(exists(file));
}


TEST_F(TestMakeDir, createSubDirectory)
{
    fs::path dir{TEST_MAKEDIR_BASE};
    dir /= "create_sub_dir";
    dir /= "sub_dir";
    remove_all(dir);
    ASSERT_FALSE(exists(dir));

    bool result{make_dir(dir, MD_DIR)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
}

TEST_F(TestMakeDir, fileCreateSubDirectory)
{
    fs::path dir{TEST_MAKEDIR_BASE};
    dir = dir / "file_create_sub_dir" / "sub_dir";
    remove_all(dir);
    fs::path file{dir / "file.txt"};
    ASSERT_FALSE(exists(dir));
    ASSERT_FALSE(exists(file));

    bool result{make_dir(file, MD_FILE)};

    EXPECT_FALSE(result);
    EXPECT_TRUE(exists(dir));
    EXPECT_FALSE(exists(file));
}
