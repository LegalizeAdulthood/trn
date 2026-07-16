// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#ifndef TRN_MOCK_ENV_H
#define TRN_MOCK_ENV_H

#include <config/common.h>
#include <util/env-internal.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <list>
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
    }

    ::testing::StrictMock<::testing::MockFunction<char *(const char *)>> getter;

    void expect_env(const char *name, const char *value)
    {
        using namespace ::testing;
        m_values.emplace_back(value);
        EXPECT_CALL(getter, Call(StrEq(name))).WillOnce(Return(const_cast<char *>(m_values.back().c_str())));
    }
    void expect_no_envar(const char *name)
    {
        using namespace ::testing;
        EXPECT_CALL(getter, Call(StrEq(name))).WillOnce(Return(nullptr));
    }
    void expect_env_repeatedly(const char *name, const char *value)
    {
        using namespace ::testing;
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
    std::list<std::string> m_values;
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
