// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <config/env.h>

#include <gtest/gtest.h>

#include <string_view>

namespace
{

constexpr std::string_view TEST_ENV_VAR{"TRN_TEST_CONFIG_ENV_VAR"};

void clear_test_env_var()
{
    unset_env_var(TEST_ENV_VAR);
}

class TestEnv : public testing::Test
{
protected:
    void SetUp() override
    {
        clear_test_env_var();
    }

    void TearDown() override
    {
        clear_test_env_var();
    }
};

} // namespace

TEST_F(TestEnv, missingVariableReturnsEmptyString)
{
    EXPECT_TRUE(get_env_var(TEST_ENV_VAR).empty());
}

TEST_F(TestEnv, missingVariableReturnsDefault)
{
    EXPECT_EQ("default", get_env_var(TEST_ENV_VAR, "default"));
}

TEST_F(TestEnv, emptyVariableReturnsDefault)
{
    set_env_var(TEST_ENV_VAR, "");

    EXPECT_EQ("default", get_env_var(TEST_ENV_VAR, "default"));
}

TEST_F(TestEnv, setVariableStoresValue)
{
    set_env_var(TEST_ENV_VAR, "fractal");

    EXPECT_EQ("fractal", get_env_var(TEST_ENV_VAR));
}

TEST_F(TestEnv, setVariableOverridesDefault)
{
    set_env_var(TEST_ENV_VAR, "fractal");

    EXPECT_EQ("fractal", get_env_var(TEST_ENV_VAR, "default"));
}

TEST_F(TestEnv, setVariableReplacesValue)
{
    set_env_var(TEST_ENV_VAR, "fractal");

    set_env_var(TEST_ENV_VAR, "spiral");

    EXPECT_EQ("spiral", get_env_var(TEST_ENV_VAR));
}

TEST_F(TestEnv, unsetVariableClearsValue)
{
    set_env_var(TEST_ENV_VAR, "fractal");

    unset_env_var(TEST_ENV_VAR);

    EXPECT_TRUE(get_env_var(TEST_ENV_VAR).empty());
}
