#include <gtest/gtest.h>

#include <array>

#include <common.h>
#include <env-internal.h>
#include <init.h>
#include <util.h>
#include <util2.h>

namespace
{

struct InitTest : testing::Test
{
protected:
    void SetUp() override
    {
        const char *const vars[] = {"HOME=",     "LOGDIR=",    "TMPDIR=",   "TMP=",    "USER=",  "LOGNAME=",
                                    "USERNAME=", "HOMEDRIVE=", "HOMEPATH=", "DOTDIR=", "TRNDIR="};
        for (const char *name : vars)
        {
            putenv(name);
        }
    }

    void TearDown() override
    {
        safefree0(g_home_dir);
        safefree0(g_real_name);
        g_tmp_dir = nullptr;
        safefree0(g_login_name);
        safefree0(g_real_name);
        safefree0(g_local_host);
        safefree0(g_p_host_name);
        g_dot_dir = nullptr;
        safefree0(g_trn_dir);
        safefree0(g_lib);
    }

    std::array<char, TCBUF_SIZE> m_tcbuf{};
    std::function<bool(char *)>  m_failed_set_name_fn{[](char *) { return false; }};
    std::function<bool(char *)>  m_set_name_fn{[](char *)
                                                 {
                                                     g_login_name = savestr("bonzo");
                                                     g_real_name = savestr("Bonzo the Monkey");
                                                     return true;
                                                 }};
};

} // namespace

TEST_F(InitTest, homeDirFromHOME)
{
    const std::string home{"C:\\users\\bonzo"};
    putenv(("HOME=" + home).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(home, std::string{g_home_dir});
}

TEST_F(InitTest, homeDirFromLOGDIR)
{
    const std::string log_dir{"C:\\users\\bonzo"};
    putenv(("LOGDIR=" + log_dir).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(log_dir, std::string{g_home_dir});
}

TEST_F(InitTest, tempDirFromTMPDIR)
{
    const std::string tmp_dir{"C:\\tmp"};
    putenv(("TMPDIR=" + tmp_dir).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(tmp_dir, std::string{g_tmp_dir});
}

TEST_F(InitTest, tempDirFromTMP)
{
    const std::string tmp{"C:\\tmp"};
    putenv(("TMP=" + tmp).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(tmp, std::string{g_tmp_dir});
}

TEST_F(InitTest, tempDirFromDefault)
{
    const std::string default_value{"/tmp"};

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(default_value, std::string{g_tmp_dir});
}

TEST_F(InitTest, loginNameFromUSER)
{
    const std::string login_name{"bonzo"};
    putenv(("USER=" + login_name).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(login_name, std::string{g_login_name});
}

TEST_F(InitTest, loginNameFromLOGNAME)
{
    const std::string login_name{"bonzo"};
    putenv(("LOGNAME=" + login_name).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(login_name, std::string{g_login_name});
}

#ifdef MSDOS
TEST_F(InitTest, loginNameFromUSERNAME)
{
    const std::string login_name{"bonzo"};
    putenv(("USERNAME=" + login_name).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(login_name, std::string{g_login_name});
}

TEST_F(InitTest, homeDirFromHOMEDRIVEandHOMEPATH)
{
    const std::string home_drive{"C:"};
    const std::string home_path{"\\Users\\Bonzo"};
    putenv(("HOMEDRIVE=" + home_drive).c_str());
    putenv(("HOMEPATH=" + home_path).c_str());

    env_init(m_tcbuf.data(), true);

    ASSERT_EQ(home_drive + home_path, std::string{g_home_dir});
}
#endif

TEST_F(InitTest, namesFromFailedUserNameLookup)
{
    const bool fully_successful = env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_FALSE(fully_successful);
    ASSERT_TRUE(std::string(g_login_name).empty()) << "g_login_name = '" << g_login_name << '\'';
    ASSERT_TRUE(std::string(g_real_name).empty()) << "g_real_name = '" << g_real_name << '\'';
}

TEST_F(InitTest, namesFromSucessfulUserNameLookup)
{
    const std::string login_name{"bonzo"};
    const std::string real_name{"Bonzo the Monkey"};
    auto user_name_fn = [&](char *)
    {
        g_login_name = savestr(login_name.c_str());
        g_real_name = savestr(real_name.c_str());
        return true;
    };

    const bool fully_successful = env_init(m_tcbuf.data(), true, user_name_fn, m_failed_set_name_fn);

    ASSERT_FALSE(fully_successful);
    ASSERT_EQ(login_name, g_login_name);
    ASSERT_EQ(real_name, g_real_name);
}

TEST_F(InitTest, emptyHostNamesFromFailedHostFn)
{
    const bool fully_successful = env_init(m_tcbuf.data(), true, m_set_name_fn, m_failed_set_name_fn);

    ASSERT_FALSE(fully_successful);
    ASSERT_TRUE(std::string{g_local_host}.empty()) << "g_local_host = '" << g_local_host << '\'';
    ASSERT_TRUE(std::string{g_p_host_name}.empty()) << "g_p_host_name = '" << g_p_host_name << '\'';
}

TEST_F(InitTest, hostNamesFromSuccessfulHostFn)
{
    const std::string local_host{"fractal"};
    const std::string p_host_name{"news.gmane.io"};
    auto host_name_fn = [&](char *)
    {
        g_local_host = savestr(local_host.c_str());
        g_p_host_name = savestr(p_host_name.c_str());
        return true;
    };

    const bool fully_successful = env_init(m_tcbuf.data(), true, m_set_name_fn, host_name_fn);

    ASSERT_TRUE(fully_successful);
    ASSERT_EQ(local_host, std::string{g_local_host});
    ASSERT_EQ(p_host_name, std::string{g_p_host_name});
}

TEST_F(InitTest, homeDirFromInit2)
{
    env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_EQ(std::string{"/"}, g_home_dir);
}

TEST_F(InitTest, dotDirFromDOTDIR)
{
    const std::string dot_dir{"/home/users/bonzo"};
    putenv(("DOTDIR=" + dot_dir).c_str());

    env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_EQ(dot_dir, g_dot_dir);
}

TEST_F(InitTest, trnDirFromTRNDIR)
{
    const std::string trn_dir{"/home/users/bonzo/.trn"};
    putenv(("TRNDIR=" + trn_dir).c_str());

    env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_EQ(trn_dir, g_trn_dir);
}

TEST_F(InitTest, trnDirFromDefaultValue)
{
    // Default value is expanded from %.
    const std::string dot_dir{"/home/users/bonzo"};
    putenv(("DOTDIR=" + dot_dir).c_str());
    const std::string trn_dir{dot_dir + "/.trn"};

    env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_EQ(trn_dir, g_trn_dir);
}

TEST_F(InitTest, libDirFromConfiguration)
{
    const std::string lib_dir{NEWSLIB};

    env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_EQ(lib_dir, g_lib);
}

TEST_F(InitTest, rnLibDirFromConfiguration)
{
    const std::string rn_lib_dir{PRIVLIB};

    env_init(m_tcbuf.data(), true, m_failed_set_name_fn, m_failed_set_name_fn);

    ASSERT_EQ(rn_lib_dir, g_rn_lib);
}
