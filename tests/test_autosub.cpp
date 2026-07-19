// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/autosub.h>

#include <config/env.h>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace
{

constexpr std::string_view AUTOSUBSCRIBE_ENV{"AUTOSUBSCRIBE"};
constexpr std::string_view AUTOUNSUBSCRIBE_ENV{"AUTOUNSUBSCRIBE"};

class AutoSubscribeTest : public testing::Test
{
protected:
    void SetUp() override
    {
        m_old_auto_subscribe = get_env_var(AUTOSUBSCRIBE_ENV);
        m_old_auto_unsubscribe = get_env_var(AUTOUNSUBSCRIBE_ENV);
        unset_env_var(AUTOSUBSCRIBE_ENV);
        unset_env_var(AUTOUNSUBSCRIBE_ENV);
    }

    void TearDown() override
    {
        restore_env(AUTOSUBSCRIBE_ENV, m_old_auto_subscribe);
        restore_env(AUTOUNSUBSCRIBE_ENV, m_old_auto_unsubscribe);
    }

private:
    static void restore_env(std::string_view name, const std::string &value)
    {
        if (value.empty())
        {
            unset_env_var(name);
        }
        else
        {
            set_env_var(name, value);
        }
    }

    std::string m_old_auto_subscribe;
    std::string m_old_auto_unsubscribe;
};

} // namespace

TEST_F(AutoSubscribeTest, asksWhenNoPatternsMatch)
{
    EXPECT_EQ(ADDNEW_ASK, auto_subscribe("comp.lang.apl"));
}

TEST_F(AutoSubscribeTest, subscribesWhenSubscribePatternMatches)
{
    set_env_var(AUTOSUBSCRIBE_ENV, "comp.lang.*");

    EXPECT_EQ(ADDNEW_SUB, auto_subscribe("comp.lang.apl"));
}

TEST_F(AutoSubscribeTest, unsubscribesWhenUnsubscribePatternMatches)
{
    set_env_var(AUTOUNSUBSCRIBE_ENV, "comp.lang.*");

    EXPECT_EQ(ADDNEW_UNSUB, auto_subscribe("comp.lang.apl"));
}

TEST_F(AutoSubscribeTest, subscribePatternWinsOverUnsubscribePattern)
{
    set_env_var(AUTOSUBSCRIBE_ENV, "comp.lang.*");
    set_env_var(AUTOUNSUBSCRIBE_ENV, "comp.lang.*");

    EXPECT_EQ(ADDNEW_SUB, auto_subscribe("comp.lang.apl"));
}
