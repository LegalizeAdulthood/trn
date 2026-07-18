// This software is copyrighted as detailed in the LICENSE file.
#include <trn/util.h>

#include <test_config.h>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

TEST(TestGetWorkingDirectory, returnsCurrentDirectory)
{
    const std::filesystem::path previous_dir = std::filesystem::current_path();
    std::filesystem::current_path(TRN_TEST_HOME_DIR);

    const std::string result = trn_getwd();

    EXPECT_EQ(TRN_TEST_HOME_DIR, result);
    std::filesystem::current_path(previous_dir);
}
