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

    std::string buffer() const
    {
        return m_buffer.data();
    }

    std::array<char, TCBUF_SIZE>  m_tcbuf{};
    std::array<char, BUFFER_SIZE> m_buffer{};
    MULTIRC                      *m_multirc{};
    long                          m_test_pid{6421};
};

void InterpolatorTest::SetUp()
{
    Test::SetUp();

    g_our_pid = m_test_pid;
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
    ASSERT_EQ(pattern, buffer());
}

TEST_F(InterpolatorTest, firstStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = interpolate(pattern, "[]");

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ("this string contains no escapes ", buffer());
}

TEST_F(InterpolatorTest, subsequentStopCharacter)
{
    char pattern[]{"this string contains no escapes [but contains stop characters]"};

    const char *new_pattern = interpolate(pattern, "()[]");

    ASSERT_EQ('[', *new_pattern);
    ASSERT_EQ("this string contains no escapes ", buffer());
}

TEST_F(InterpolatorTest, articleNumberOutsideNewsgroup)
{
    char pattern[]{"Article %a"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Article ", buffer());
}

TEST_F(InterpolatorTest, articleNumberInsideNewsgroup)
{
    char pattern[]{"Article %a"};
    g_in_ng = true;
    g_art = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Article 624", buffer());
}

TEST_F(InterpolatorTest, articleNameOutsideNewsgroup)
{
    char pattern[]{"Article %A"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Article ", buffer());
}

TEST_F(InterpolatorTest, articleNameInsideRemoteNewsgroupArticleClosed)
{
    char pattern[]{"Article %A"};
    g_in_ng = true;
    g_lastart = 623;
    g_art = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Article ", buffer());
}

TEST_F(InterpolatorTest, tilde)
{
    char pattern[]{"%~"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HOME_DIR, buffer());
}

TEST_F(InterpolatorTest, dotDir)
{
    char pattern[]{"%."};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_DOT_DIR, buffer());
}

TEST_F(InterpolatorTest, processId)
{
    char pattern[]{"%$"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(m_test_pid), buffer());
}

TEST_F(InterpolatorTest, environmentVarValue)
{
    putenv("FOO=value");
    char pattern[]{"%{FOO}"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("value", buffer());

    putenv("FOO=");
}

TEST_F(InterpolatorTest, environmentVarValueDefault)
{
    putenv("FOO=");
    char pattern[]{"%{FOO-not set}"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("not set", buffer());
}

TEST_F(InterpolatorTest, libDir)
{
    char pattern[]{"%x"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_LIB_DIR, buffer());
}

TEST_F(InterpolatorTest, rnLibDir)
{
    char pattern[]{"%X"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_RN_LIB_DIR, buffer());
}

TEST_F(InterpolatorTest, saveDestinationNotSet)
{
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, saveDestinationSet)
{
    g_savedest = "/tmp/frob";
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("/tmp/frob", buffer());
}

TEST_F(InterpolatorTest, relativeNewsgroupDir)
{
    g_ngdir = "comp/arch";
    char pattern[]{"%c"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp/arch", buffer());
}

TEST_F(InterpolatorTest, relativeNewsgroupDirNotSet)
{
    g_ngdir.clear();
    char pattern[]{"%c"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, newsgroupNameNotSet)
{
    g_ngname = nullptr;
    g_ngnlen = 0;
    g_ngname_len = 0;
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, newsgroupNameSet)
{
    g_ngname = savestr("comp.arch");
    g_ngnlen = strlen(g_ngname);
    g_ngname_len = strlen(g_ngname);
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp.arch", buffer());
}

TEST_F(InterpolatorTest, absoluteNewsgroupDirNotSet)
{
    g_ngdir.clear();
    char pattern[]{"%d"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, oldDistributionLineNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%D"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, extractProgramNotSet)
{
    g_extractprog.clear();
    char pattern[]{"%e"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("-", buffer());
}

TEST_F(InterpolatorTest, extractProgramSet)
{
    g_extractprog = "uudecode";
    char pattern[]{"%e"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("uudecode", buffer());
}

TEST_F(InterpolatorTest, extractDestinationNotSet)
{
    g_extractdest.clear();
    char pattern[]{"%E"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, extractDestinationSet)
{
    g_extractdest = "/home/users/foo";
    char pattern[]{"%E"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("/home/users/foo", buffer());
}

TEST_F(InterpolatorTest, fromLineNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%f"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, followupNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    const std::string buffer{m_buffer.data()};
    ASSERT_TRUE(buffer.empty()) << "Contents: '" << buffer << "'";
}

TEST_F(InterpolatorTest, generalMode)
{
    g_general_mode = GM_INIT;
    char pattern[]{"%g"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("I", buffer());
}

TEST_F(InterpolatorTest, headerFileName)
{
    char pattern[]{"%h"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{TRN_TEST_DOT_DIR} + "/.rnhead." + std::to_string(m_test_pid), buffer());
}

TEST_F(InterpolatorTest, hostName)
{
    char pattern[]{"%H"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_P_HOST_NAME, buffer());
}

TEST_F(InterpolatorTest, messageIdNotInNewsgroup)
{
    char pattern[]{"%i"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty());
}

TEST_F(InterpolatorTest, indentString)
{
    char pattern[]{"%I"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("'>'", buffer());
}

TEST_F(InterpolatorTest, approximateBaudRate)
{
    char pattern[]{"%j"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(g_just_a_sec*10), buffer());
}

#ifndef HAS_NEWS_ADMIN
TEST_F(InterpolatorTest, noNewsAdmin)
{
    char pattern[]{"%l"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("???", buffer());
}
#endif

TEST_F(InterpolatorTest, loginName)
{
    char pattern[]{"%L"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_LOGIN_NAME, buffer());
}

TEST_F(InterpolatorTest, minorMode)
{
    char pattern[]{"%m"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("i", buffer());
}

#ifdef TEST_ACTIVE_NEWSGROUP
namespace {

struct InterpolatorNewsgroupTest : InterpolatorTest
{
protected:
    void SetUp() override;
    void TearDown() override;
};

void InterpolatorNewsgroupTest::SetUp()
{
    InterpolatorTest::SetUp();
    g_in_ng = true;
    // TODO: figure out how to set active newsgroup
    // TODO: figure out how to properly set active DATASRC
    // TODO: figure out how to fake up an article
}

void InterpolatorNewsgroupTest::TearDown()
{
    g_in_ng = false;
    InterpolatorTest::TearDown();
}

}

TEST_F(InterpolatorNewsgroupTest, articleNameInsideRemoteNewsgroupArticleOpen)
{
    build_cache();
    char pattern[]{"Article %A"};
    g_art = 624;
    g_lastart = 624;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Article 624", buffer());

    close_cache();
}

TEST_F(InterpolatorNewsgroupTest, absoluteNewsgroupDirSet)
{
    g_ngdir = "comp/arch";
    char pattern[]{"%d"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("/usr/spool/news/comp/arch", buffer());
}

TEST_F(InterpolatorNewsgroupTest, oldDistributionLineInNewsgroup)
{
    char pattern[]{"%D"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("na", buffer());
}

TEST_F(InterpolatorNewsgroupTest, fromLineInNewsgroupNoReplyTo)
{
    char pattern[]{"%f"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("John Yeager <jyeager@example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, fromLineInNewsgroupWithReplyTo)
{
    char pattern[]{"%f"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Cyrus Longworth <clongworth@example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, followupInNewsgroupWithFollowupToLine)
{
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("alt.flame", buffer());
}

TEST_F(InterpolatorNewsgroupTest, followupInNewsgroupFromNewsgroupsLine)
{
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp.arch", buffer());
}

TEST_F(InterpolatorNewsgroupTest, messageIdInNewsgroup)
{
    char pattern[]{"%i"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("<hippityhop@flagrant.example.org>", buffer());
}
#endif
