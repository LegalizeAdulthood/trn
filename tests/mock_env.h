// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_MOCK_ENV_H
#define TRN_MOCK_ENV_H

#include <config/common.h>
#include <config/env.h>
#include <util/env-internal.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <list>
#include <optional>
#include <string>

namespace trn
{
namespace testing
{

struct MockEnvironment
{
    MockEnvironment()
    {
        set_environment(getter.AsStdFunction());
    }
    virtual ~MockEnvironment()
    {
        set_environment(nullptr);
        for (const SavedEnvironmentValue &saved : m_saved_environment)
        {
            if (saved.value.has_value())
            {
                set_env_var(saved.name, *saved.value);
            }
            else
            {
                unset_env_var(saved.name);
            }
        }
    }

    ::testing::StrictMock<::testing::MockFunction<char *(const char *)>> getter;

    void expect_env(const char *name, const char *value)
    {
        using namespace ::testing;
        save_environment(name);
        set_env_var(name, value);
        m_values.emplace_back(value);
        EXPECT_CALL(getter, Call(StrEq(name)))
            .Times(AtMost(1))
            .WillRepeatedly(Return(const_cast<char *>(m_values.back().c_str())));
    }
    void expect_no_envar(const char *name)
    {
        using namespace ::testing;
        save_environment(name);
        unset_env_var(name);
        EXPECT_CALL(getter, Call(StrEq(name))).Times(AtMost(1)).WillRepeatedly(Return(nullptr));
    }
    void expect_env_repeatedly(const char *name, const char *value)
    {
        using namespace ::testing;
        save_environment(name);
        set_env_var(name, value);
        m_values.emplace_back(value);
        EXPECT_CALL(getter, Call(StrEq(name))).WillRepeatedly(Return(const_cast<char *>(m_values.back().c_str())));
    }
    void expect_no_envars(std::initializer_list<const char *> envars)
    {
        for (const char *envar : envars)
        {
            if (envar)
            {
                expect_no_envar(envar);
            }
        }
    }

private:
    struct SavedEnvironmentValue
    {
        std::string                name;
        std::optional<std::string> value;
    };

    void save_environment(const char *name)
    {
        if (name == nullptr)
        {
            return;
        }
        for (const SavedEnvironmentValue &saved : m_saved_environment)
        {
            if (saved.name == name)
            {
                return;
            }
        }
        if (const char *value = std::getenv(name))
        {
            m_saved_environment.push_back({name, std::string{value}});
        }
        else
        {
            m_saved_environment.push_back({name, std::nullopt});
        }
    }

    std::list<std::string> m_values;
    std::list<SavedEnvironmentValue> m_saved_environment;
};

} // namespace testing
} // namespace trn

#ifdef MSDOS
#define HOMEDRIVE "HOMEDRIVE"
#define HOMEPATH "HOMEPATH"
#define USERNAME "USERNAME"
#else
#define HOMEDRIVE nullptr
#define HOMEPATH nullptr
#define USERNAME nullptr
#endif

#endif
