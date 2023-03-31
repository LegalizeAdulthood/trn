#include <gtest/gtest.h>

#include <array>

#include "test_config.h"

#include <common.h>

#include <art.h>
#include <artio.h>
#include <artsrch.h>
#include <backpage.h>
#include <cache.h>
#include <color.h>
#include <datasrc.h>
#include <env-internal.h>
#include <init.h>
#include <intrp.h>
#include <kfile.h>
#include <last.h>
#include <mime.h>
#include <ng.h>
#include <ngdata.h>
#include <ngsrch.h>
#include <opt.h>
#include <rcstuff.h>
#include <search.h>
#include <univ.h>
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

    mp_init();
    search_init();
    trn::testing::set_envars();
    env_init(m_tcbuf.data(), true, trn::testing::set_name, trn::testing::set_host_name);
    trn::testing::reset_lib_dirs();
    head_init();
    char trn[]{"trn"};
    char *argv[]{trn};
    char *tcbuf = m_tcbuf.data();
    opt_init(1,argv,&tcbuf);
    color_init();
    intrp_init(tcbuf, TCBUF_SIZE);
    last_init();
    univ_init();
    datasrc_init();
    rcstuff_init();
    art_init();
    artio_init();
    artsrch_init();
    backpage_init();
    cache_init();
    kfile_init();
    mime_init();
    ng_init();
    ngsrch_init();
    util_init();
    g_datasrc = datasrc_first();
}

void InterpolatorTest::TearDown()
{
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
    trn::testing::reset_envars();

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
    build_cache();
    char pattern[]{"Article %A"};
    g_in_ng = true;
    g_art = 624;
    g_lastart = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{"Article 624"}, std::string{m_buffer.data()});

    close_cache();
}
