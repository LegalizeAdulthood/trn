#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include <config/common.h>
#include <util/env-internal.h>
#include <trn/init.h>
#include <trn/util.h>
#include <util/util2.h>

#include "mock_env.h"
#include "test_config.h"

using namespace testing;

namespace
{

using Environment = StrictMock<trn::testing::MockEnvironment>;

struct InitTest : Test
{
protected:
    void TearDown() override
    {
        safefree0(g_home_dir);
        g_tmp_dir.clear();
        g_login_name.clear();
        g_real_name.clear();
        safefree0(g_local_host);
        g_p_host_name.clear();
        g_dot_dir.clear();
        g_trn_dir.clear();
        g_lib.clear();
        g_rn_lib.clear();
    }

    void expect_env(const char *name, const char *value)
    {
        m_env.expect_env(name, value);
    }
    void expect_no_envars(std::initializer_list<const char *> envars)
    {
        m_env.expect_no_envars(envars);
    }

    Environment                  m_env;
    std::array<char, TCBUF_SIZE> m_tcbuf{};
    std::function<bool(char *)>  m_failed_fn{[](char *) { return false; }};
    std::function<bool(char *)>  m_succeeded_fn{[](char *) { return true; }};
    std::function<bool(char *)>  m_set_name_fn{[](char *)
                                              {
                                                  g_login_name = TRN_TEST_LOGIN_NAME;
                                                  g_real_name = TRN_TEST_REAL_NAME;
                                                  return true;
                                              }};
};

} // namespace

TEST_F(InitTest, homeDirFromHOME)
{
    const char *home{"C:\\users\\bonzo"};
    expect_env("HOME", home);
    expect_no_envars({"DOTDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true);

    ASSERT_STREQ(home, g_home_dir);
}

TEST_F(InitTest, homeDirFromLOGDIR)
{
    const char *log_dir{"C:\\users\\bonzo"};
    expect_env("LOGDIR", log_dir);
    expect_no_envars({"DOTDIR", "HOME", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true);

    ASSERT_STREQ(log_dir, g_home_dir);
}

TEST_F(InitTest, tempDirFromTMPDIR)
{
    const char *tmp_dir{"C:\\tmp"};
    expect_env("TMPDIR", tmp_dir);
    expect_no_envars(
        {"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(tmp_dir, g_tmp_dir);
}

TEST_F(InitTest, tempDirFromTMP)
{
    const char *tmp{"C:\\tmp"};
    expect_env("TMP", tmp);
    expect_no_envars(
        {"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMPDIR", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(tmp, g_tmp_dir);
}

TEST_F(InitTest, tempDirFromDefault)
{
    const char *default_value{"/tmp"};
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(default_value, g_tmp_dir);
}

TEST_F(InitTest, loginNameFromUSER)
{
    expect_env("USER", TRN_TEST_LOGIN_NAME);
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "NETSPEED", "TMP", "TMPDIR", "TRNDIR"});

    env_init(m_tcbuf.data(), true, m_succeeded_fn, m_failed_fn);

    ASSERT_EQ(TRN_TEST_LOGIN_NAME, g_login_name);
}

TEST_F(InitTest, loginNameFromLOGNAME)
{
    const char *login_name{TRN_TEST_LOGIN_NAME};
    expect_env("LOGNAME", login_name);
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER"});

    env_init(m_tcbuf.data(), true, m_succeeded_fn, m_failed_fn);

    ASSERT_EQ(login_name, g_login_name);
}

#ifdef MSDOS
TEST_F(InitTest, loginNameFromUSERNAME)
{
    const char *login_name{TRN_TEST_LOGIN_NAME};
    expect_env(USERNAME, login_name);
    expect_no_envars(
        {"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER"});

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(login_name, g_login_name);
}

TEST_F(InitTest, homeDirFromHOMEDRIVEandHOMEPATH)
{
    const char *home_drive{"C:"};
    const char *home_path{"\\Users\\Bonzo"};
    expect_env(HOMEDRIVE, home_drive);
    expect_env(HOMEPATH, home_path);
    expect_no_envars({"DOTDIR", "HOME", "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(std::string{home_drive} + home_path, g_home_dir);
}
#endif

TEST_F(InitTest, namesFromFailedUserNameLookup)
{
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    const bool fully_successful = env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_FALSE(fully_successful);
    ASSERT_TRUE(g_login_name.empty()) << "g_login_name = '" << g_login_name << '\'';
    ASSERT_TRUE(g_real_name.empty()) << "g_real_name = '" << g_real_name << '\'';
}

TEST_F(InitTest, namesFromSucessfulUserNameLookup)
{
    const char *login_name{TRN_TEST_LOGIN_NAME};
    const char *real_name{TRN_TEST_REAL_NAME};
    auto        user_name_fn = [&](char *)
    {
        g_login_name = login_name;
        g_real_name = real_name;
        return true;
    };
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    const bool fully_successful = env_init(m_tcbuf.data(), true, user_name_fn, m_failed_fn);

    ASSERT_FALSE(fully_successful);
    ASSERT_EQ(login_name, g_login_name);
    ASSERT_EQ(real_name, g_real_name);
}

TEST_F(InitTest, emptyHostNamesFromFailedHostFn)
{
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    const bool fully_successful = env_init(m_tcbuf.data(), true, m_set_name_fn, m_failed_fn);

    ASSERT_FALSE(fully_successful);
    ASSERT_TRUE(std::string{g_local_host}.empty()) << "g_local_host = '" << g_local_host << '\'';
    ASSERT_TRUE(g_p_host_name.empty()) << "g_p_host_name = '" << g_p_host_name << '\'';
}

TEST_F(InitTest, hostNamesFromSuccessfulHostFn)
{
    const char *local_host{"fractal"};
    const char *p_host_name{"news.gmane.io"};
    auto        host_name_fn = [&](char *)
    {
        g_local_host = savestr(local_host);
        g_p_host_name = p_host_name;
        return true;
    };
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    const bool fully_successful = env_init(m_tcbuf.data(), true, m_set_name_fn, host_name_fn);

    ASSERT_TRUE(fully_successful);
    ASSERT_EQ(local_host, std::string{g_local_host});
    ASSERT_EQ(p_host_name, g_p_host_name);
}

TEST_F(InitTest, homeDirFromInit2)
{
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_EQ(std::string{"/"}, g_home_dir);
}

TEST_F(InitTest, dotDirFromDOTDIR)
{
    const char *dot_dir{"/home/users/bonzo"};
    expect_env("DOTDIR", dot_dir);
    expect_no_envars(
        {"HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_EQ(dot_dir, g_dot_dir);
}

TEST_F(InitTest, trnDirFromTRNDIR)
{
    const char *trn_dir{"/home/users/bonzo/.trn"};
    expect_env("TRNDIR", trn_dir);
    expect_no_envars(
        {"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_EQ(trn_dir, g_trn_dir);
}

TEST_F(InitTest, trnDirFromDefaultValue)
{
    // Default value is expanded from %.
    const char *dot_dir{"/home/users/bonzo"};
    expect_env("DOTDIR", dot_dir);
    const std::string trn_dir{std::string{dot_dir} + "/.trn"};
    expect_no_envars(
        {"HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR", "USER", USERNAME});

    env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_EQ(trn_dir, g_trn_dir);
}

TEST_F(InitTest, libDirFromConfiguration)
{
    const char *lib_dir{NEWSLIB};
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_EQ(lib_dir, g_lib);
}

TEST_F(InitTest, rnLibDirFromConfiguration)
{
    const char *rn_lib_dir{PRIVLIB};
    expect_no_envars({"DOTDIR", "HOME", HOMEDRIVE, HOMEPATH, "LOGDIR", "LOGNAME", "NETSPEED", "TMP", "TMPDIR", "TRNDIR",
                      "USER", USERNAME});

    env_init(m_tcbuf.data(), true, m_failed_fn, m_failed_fn);

    ASSERT_EQ(rn_lib_dir, g_rn_lib);
}
