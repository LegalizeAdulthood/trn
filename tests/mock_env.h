#ifndef TRN_MOCK_ENV_H
#define TRN_MOCK_ENV_H

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include <common.h>
#include <env-internal.h>

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

    ::testing::StrictMock<::testing::MockFunction<char*(const char *)>> getter;

    void expect_env(const char *name, const char *value)
    {
        using namespace ::testing;
        EXPECT_CALL(getter, Call(StrEq(name))).WillOnce(Return(const_cast<char *>(value)));
    }
    void expect_no_envar(const char *name)
    {
        using namespace ::testing;
        EXPECT_CALL(getter, Call(StrEq(name))).WillOnce(Return(nullptr));
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
