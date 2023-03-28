#include <gtest/gtest.h>

#include "test_config.h"

#include <array>

#include <common.h>

#include <cache.h>
#include <datasrc.h>
#include <env-internal.h>
#include <init.h>
#include <intrp.h>
#include <ng.h>
#include <ngdata.h>
#include <util.h>
#include <util2.h>

constexpr int BUFFER_SIZE{4096};

namespace
{

struct InterpolatorTest : testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    char *interpolate(char *pattern, const char *stoppers = "")
    {
        return dointerp(m_buffer.data(), BUFFER_SIZE, pattern, stoppers, nullptr);
    }

    std::array<char, TCBUF_SIZE> m_tcbuf{};
    std::array<char, BUFFER_SIZE> m_buffer{};
};

void InterpolatorTest::SetUp()
{
    Test::SetUp();

    const char *const envars[] = {"HOME=",     "LOGDIR=",    "TMPDIR=",   "TMP=",    "USER=",  "LOGNAME=",
                                  "USERNAME=", "HOMEDRIVE=", "HOMEPATH=", "DOTDIR=", "TRNDIR="};
    for (const char *name : envars)
    {
        putenv(name);
    }
    env_init(
        m_tcbuf.data(), true,
        [](char *)
        {
            g_login_name = savestr("bonzo");
            g_real_name = savestr("Bonzo the Monkey");
            return true;
        },
        [](char *)
        {
            g_local_host = savestr("localhost");
            g_p_host_name = savestr("news.gmane.io");
            return true;
        });
    datasrc_init();
    g_datasrc = datasrc_first();
    build_cache();
}

void InterpolatorTest::TearDown()
{
    close_cache();
    g_lastart = 0;
    g_art = 0;
    g_in_ng = false;
    g_datasrc = nullptr;
    datasrc_finalize();
    g_datasrc_cnt = 0;

    safefree0(g_datasrc_list);
    safefree0(g_nntp_auth_file);
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

    Test::TearDown();
}

} // namespace

TEST_F(InterpolatorTest, noEscapes)
{
    char pattern[]{"this string contains no escapes"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{pattern}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, firstStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = interpolate(pattern, "[]");

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ(std::string{"this string contains no escapes "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, subsequentStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = interpolate(pattern, "()[]");

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ(std::string{"this string contains no escapes "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNumberOutsideNewsgroup)
{
    char pattern[]{"Article %a"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNumberInsideNewsgroup)
{
    char pattern[]{"Article %a"};
    g_in_ng = true;
    g_art = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article 624"}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNameOutsideNewsgroup)
{
    char pattern[]{"Article %A"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNameInsideRemoteNewsgroupArticleClosed)
{
    char pattern[]{"Article %A"};
    g_in_ng = true;
    g_lastart = 623;
    g_art = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article "}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, articleNameInsideRemoteNewsgroupArticleOpen)
{
    char pattern[]{"Article %A"};
    g_in_ng = true;
    g_art = 624;
    g_lastart = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article 624"}, std::string{m_buffer.data()});
}
