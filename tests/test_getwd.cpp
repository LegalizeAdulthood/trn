#include <trn/util.h>

#include "test_config.h"

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>

TEST(TestGetWorkingDirectory, foo)
{
    std::filesystem::current_path(TRN_TEST_HOME_DIR);

    char buf[512];
    const char *result = trn_getwd(buf, sizeof buf);

    EXPECT_NE(nullptr, result);
    EXPECT_EQ(buf, result);
    EXPECT_STREQ(TRN_TEST_HOME_DIR, result);
}
