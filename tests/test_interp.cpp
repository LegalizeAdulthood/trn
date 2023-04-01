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
#include <respond.h>
#include <search.h>
#include <term.h>
#include <trn.h>
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

    std::array<char, TCBUF_SIZE>  m_tcbuf{};
    std::array<char, BUFFER_SIZE> m_buffer{};
    MULTIRC                      *m_multirc{};
};

void InterpolatorTest::SetUp()
{
    Test::SetUp();

    term_init();
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
    term_set(tcbuf);
    last_init();
    univ_init();
    datasrc_init();
    m_multirc = rcstuff_init_data();
    art_init();
    artio_init();
    artsrch_init();
    cache_init();
    kfile_init();
    mime_init();
    ng_init();
    ngsrch_init();
    respond_init();
    trn_init();
    util_init();
    g_datasrc = datasrc_first();
}

void InterpolatorTest::TearDown()
{
    g_lastart = 0;
    g_art = 0;
    g_in_ng = false;
    g_datasrc = nullptr;

    util_final();
    mime_final();
    artio_final();
    rcstuff_final();
    datasrc_finalize();
    last_final();
    intrp_final();
    opt_final();
    head_final();
    env_final();
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

TEST_F(InterpolatorTest, DISABLED_articleNameInsideRemoteNewsgroupArticleOpen)
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

TEST_F(InterpolatorTest, tilde)
{
    char pattern[]{"%~"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{TRN_TEST_HOME_DIR}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, dotDir)
{
    char pattern[]{"%."};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{TRN_TEST_DOT_DIR}, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, processId)
{
    g_our_pid = 8642;
    char pattern[]{"%$"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("8642", std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, environmentVarValue)
{
    putenv("FOO=value");
    char pattern[]{"%{FOO}"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("value", std::string{m_buffer.data()});

    putenv("FOO=");
}

TEST_F(InterpolatorTest, environmentVarValueDefault)
{
    putenv("FOO=");
    char pattern[]{"%{FOO-not set}"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("not set", std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, libDir)
{
    char pattern[]{"%x"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_LIB_DIR, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, rnLibDir)
{
    char pattern[]{"%X"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_RN_LIB_DIR, std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, saveDestinationNotSet)
{
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(std::string{m_buffer.data()}.empty());
}

TEST_F(InterpolatorTest, saveDestinationSet)
{
    g_savedest = "/tmp/frob";
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("/tmp/frob", std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, relativeNewsgroupDir)
{
    g_ngdir = "comp/arch";
    char pattern[]{"%c"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp/arch", std::string{m_buffer.data()});
}

TEST_F(InterpolatorTest, newsgroupNameNotSet)
{
    g_ngname = nullptr;
    g_ngnlen = 0;
    g_ngname_len = 0;
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(std::string{m_buffer.data()}.empty());
}

TEST_F(InterpolatorTest, newsgroupNameSet)
{
    g_ngname = savestr("comp.arch");
    g_ngnlen = strlen(g_ngname);
    g_ngname_len = strlen(g_ngname);
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp.arch", std::string{m_buffer.data()});
}
