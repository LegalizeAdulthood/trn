// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/env.h>
#include <tool/util3.h>
#include <util/env.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

std::string g_nntp_auth_file;

int nntp_handle_timeout()
{
    return 0;
}

namespace
{

constexpr std::string_view TEST_ENV_VAR{"TRN_TEST_TOOL_UTIL3_ENV_VAR"};

class ToolUtil3Test : public testing::Test
{
protected:
    void SetUp() override
    {
        unset_env_var(TEST_ENV_VAR);
        m_old_dot_dir = g_dot_dir;
    }

    void TearDown() override
    {
        g_dot_dir = m_old_dot_dir;
        unset_env_var(TEST_ENV_VAR);
    }

    std::string m_old_dot_dir;
};

} // namespace

TEST_F(ToolUtil3Test, passesLiteralTextThrough)
{
    EXPECT_EQ("literal", do_interp("literal"));
}

TEST_F(ToolUtil3Test, expandsDotDirectoryPrefix)
{
    g_dot_dir = "C:/Users/tester/.trn";

    EXPECT_EQ("C:/Users/tester/.trn/access", do_interp("%./access"));
}

TEST_F(ToolUtil3Test, expandsEnvironmentVariablePrefix)
{
    set_env_var(TEST_ENV_VAR, "C:/articles");

    EXPECT_EQ("C:/articles/current", do_interp("%{TRN_TEST_TOOL_UTIL3_ENV_VAR}/current"));
}
