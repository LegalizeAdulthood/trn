#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <string>

#include "test_config.h"

#include <common.h>

#include <art.h>
#include <artio.h>
#include <artsrch.h>
#include <backpage.h>
#include <bits.h>
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
#include <patchlevel.h>
#include <rcstuff.h>
#include <respond.h>
#include <rt-util.h>
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
    cwd_check();
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
    perform_status_init(0);
}

void InterpolatorTest::TearDown()
{
    g_general_mode = GM_INIT;
    g_mode = MM_INITIALIZING;
    g_dmcount = 0;
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

TEST_F(InterpolatorTest, saveDestinationNotSet)
{
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
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
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, newsgroupNameNotSet)
{
    g_ngname = nullptr;
    g_ngnlen = 0;
    g_ngname_len = 0;
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
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
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, oldDistributionLineNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%D"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
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
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
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
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, followupNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, generalMode)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%g"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("p", buffer());
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
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
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
    g_mode = MM_DELETE_BOGUS_NEWSGROUPS_PROMPT;
    char pattern[]{"%m"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("D", buffer());
}

TEST_F(InterpolatorTest, markCount)
{
    char pattern[]{"%M"};
    g_dmcount = 96;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("96", buffer());
}

TEST_F(InterpolatorTest, newsgroupsLineNotInNewsgroup)
{
    char pattern[]{"%n"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, realName)
{
    char pattern[]{"%N"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_REAL_NAME, buffer());
}

TEST_F(InterpolatorTest, realNameFromNAME)
{
    char pattern[]{"%N"};
    putenv("NAME=John Yeager");

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("John Yeager", buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGNAME)
{
    char pattern[]{"%o"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, newsOrgFromNEWSORG)
{
    char pattern[]{"%o"};
    putenv((std::string{"NEWSORG="} + TRN_TEST_ORGANIZATION).c_str());

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromNEWSORGFile)
{
    char pattern[]{"%o"};
    putenv((std::string{"NEWSORG="} + TRN_TEST_ORGFILE).c_str());

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGANIZATION)
{
    char pattern[]{"%o"};
    putenv((std::string{"ORGANIZATION="} + TRN_TEST_ORGANIZATION).c_str());

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGANIZATIONFile)
{
    char pattern[]{"%o"};
    putenv((std::string{"ORGANIZATION="} + TRN_TEST_ORGFILE).c_str());

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, originalDirectory)
{
    char pattern[]{"%O"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(g_origdir, buffer());
}

TEST_F(InterpolatorTest, privateNewsDirectory)
{
    char pattern[]{"%p"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(g_privdir, buffer());
}

namespace {

template <typename T>
class value_saver
{
public:
    value_saver(T &var, T new_value) :
        m_var(var),
        m_old_value(var)
    {
        m_var = new_value;
    }
    ~value_saver()
    {
        m_var = m_old_value;
    }

private:
    T &m_var;
    T m_old_value;
};

} // namespace

TEST_F(InterpolatorTest, newsSpoolDirectoryNoDataSource)
{
    value_saver<DATASRC *> datasrc(g_datasrc, nullptr);
    char                   pattern[]{"%P"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, newsSpoolDirectory)
{
    char pattern[]{"%P"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(g_datasrc->spool_dir, buffer());
}

TEST_F(InterpolatorTest, lastInputStringInitiallyEmpty)
{
    char pattern[]{"%q"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, lastReferenceNotInNewsgroupEmpty)
{
    char pattern[]{"%r"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, newReferencesNotInNewsgroupEmpty)
{
    char pattern[]{"%R"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, strippedSubjectNotInNewsgroupEmpty)
{
    char pattern[]{"%s"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, subjectNotInNewsgroupEmpty)
{
    char pattern[]{"%S"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, toFromFromReplyToNotInNewsgroupEmpty)
{
    char pattern[]{"%t"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, toFromPathNotInNewsgroupEmpty)
{
    char pattern[]{"%T"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, numUnreadArticlesNotInNewsgroupEmpty)
{
    char pattern[]{"%u"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, numUnreadArticlesExceptCurrentNotInNewsgroupEmpty)
{
    char pattern[]{"%U"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, numUnselectedArticlesExceptCurrentNotInNewsgroupEmpty)
{
    char pattern[]{"%v"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, patchLevel)
{
    char pattern[]{"%V"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{PATCHLEVEL}, buffer());
}

TEST_F(InterpolatorTest, threadDirNotInNewsgroup)
{
    char pattern[]{"%W"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
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

TEST_F(InterpolatorTest, shortenedFromNotInNewsgroupEmpty)
{
    char pattern[]{"%y"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, tmpDir)
{
    char pattern[]{"%Y"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_TMP_DIR, buffer());
}

TEST_F(InterpolatorTest, articleSizeNotInNewsgroupEmpty)
{
    char pattern[]{"%z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, numSelectedThreadsNotInNewsgroupEmpty)
{
    char pattern[]{"%Z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, trailingPercentRemains)
{
    char pattern[]{"%"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("%", buffer());
}

TEST_F(InterpolatorTest, performCount)
{
    value_saver<int> saved(g_perform_cnt, 86);
    char pattern[]{"%#"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(g_perform_cnt), buffer());
}

TEST_F(InterpolatorTest, modifiedPerformCountNotZero)
{
    char pattern[]{"%^#"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_NE("0", buffer());
}

TEST_F(InterpolatorTest, consecutiveModifiedPerformCountIncreases)
{
    value_saver<int> saved(g_perform_cnt, 86);
    char pattern[]{"%^#,%^#"};

    const char *new_pattern = interpolate(pattern);
    std::istringstream str{buffer()};
    int value1;
    int value2;
    char comma;
    str >> value1 >> comma >> value2;

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(str.eof());
    ASSERT_EQ(',', comma);
    ASSERT_GT(value2, value1);
}

TEST_F(InterpolatorTest, loginNamecCapitalized)
{
    char pattern[]{"%^L"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(!std::isupper(TRN_TEST_LOGIN_NAME[0]));
    ASSERT_EQ(static_cast<char>(std::toupper(TRN_TEST_LOGIN_NAME[0])), buffer()[0]);
}

TEST_F(InterpolatorTest, equalTriviallyTrue)
{
    char pattern[]{"%(x=x?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, equalTriviallyFalse)
{
    char pattern[]{"%(x=y?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, interpolatedTestEqualTrue)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%(%g=p?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, interpolatedTestEqualFalse)
{
    g_general_mode = GM_INIT;
    char pattern[]{"%(%g=p?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, equalInterpolatedTrue)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%(x=x?%g:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("p", buffer());
}

TEST_F(InterpolatorTest, equalInterpolatedFalse)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%(x=y?true:%g)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("p", buffer());
}

TEST_F(InterpolatorTest, notEqualTriviallyTrue)
{
    char pattern[]{"%(x!=y?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, notEqualTriviallyFalse)
{
    char pattern[]{"%(x!=x?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, regexMatched)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%(%g=^p$?true:false)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, triviallyTrueNoElse)
{
    char pattern[]{"%(x=x?true)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, triviallyFalseNoElseIsEmpty)
{
    char pattern[]{"%(x=y?true)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, escapedPercent)
{
    char pattern[]{R"(\%g)"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("%g", buffer());
}

TEST_F(InterpolatorTest, headerFieldNotInNewsgroupEmpty)
{
    char pattern[]{"%[X-Boogie-Nights]"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}

TEST_F(InterpolatorTest, homeDirectoryCapitalized)
{
    char pattern[]{"%_~"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HOME_DIR_CAPITALIZED, buffer());
}

TEST_F(InterpolatorTest, spaceForShortLine)
{
    char pattern[]{"%?"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(" ", buffer());
}

#define TRN_TEST_01_SPACES " "
#define TRN_TEST_02_SPACES TRN_TEST_01_SPACES TRN_TEST_01_SPACES
#define TRN_TEST_04_SPACES TRN_TEST_02_SPACES TRN_TEST_02_SPACES
#define TRN_TEST_08_SPACES TRN_TEST_04_SPACES TRN_TEST_04_SPACES
#define TRN_TEST_09_SPACES TRN_TEST_08_SPACES TRN_TEST_01_SPACES
#define TRN_TEST_10_SPACES TRN_TEST_09_SPACES TRN_TEST_01_SPACES
#define TRN_TEST_20_SPACES TRN_TEST_10_SPACES TRN_TEST_10_SPACES
#define TRN_TEST_40_SPACES TRN_TEST_20_SPACES TRN_TEST_20_SPACES
#define TRN_TEST_70_SPACES TRN_TEST_40_SPACES TRN_TEST_20_SPACES TRN_TEST_10_SPACES
#define TRN_TEST_79_SPACES TRN_TEST_70_SPACES TRN_TEST_09_SPACES
TEST_F(InterpolatorTest, newlineFor79CharsLine)
{
    char pattern[]{TRN_TEST_79_SPACES "%?"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_79_SPACES "\n", buffer());
}

#define TRN_TEST_80_SPACES TRN_TEST_40_SPACES TRN_TEST_40_SPACES
TEST_F(InterpolatorTest, newlineForLongerThan79CharsLine)
{
    char pattern[]{TRN_TEST_80_SPACES "%?"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_80_SPACES "\n", buffer());
}
#undef TRN_TEST_80_SPACES
#undef TRN_TEST_79_SPACES
#undef TRN_TEST_70_SPACES
#undef TRN_TEST_40_SPACES
#undef TRN_TEST_20_SPACES
#undef TRN_TEST_10_SPACES
#undef TRN_TEST_09_SPACES
#undef TRN_TEST_08_SPACES
#undef TRN_TEST_04_SPACES
#undef TRN_TEST_02_SPACES
#undef TRN_TEST_01_SPACES

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

TEST_F(InterpolatorNewsgroupTest, newsgroupsLineInNewsgroup)
{
    char pattern[]{"%n"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp.arch", buffer());
}

TEST_F(InterpolatorNewsgroupTest, lastReferenceInNewsgroup)
{
    char pattern[]{"%r"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("<reference1@flagrant.example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, newReferencesInNewsgroup)
{
    char pattern[]{"%R"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("<reference1@flagrant.example.org> <goink1@poster.example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, strippedSubjectInNewsgroup)
{
    char pattern[]{"%s"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("The best red nose", buffer());
}

TEST_F(InterpolatorNewsgroupTest, subjectInNewsgroup)
{
    char pattern[]{"%S"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Re: The best red nose", buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromReplyToInNewsgroup)
{
    char pattern[]{"%t"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Cyrus Longworth <clongworth@example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromFromInNewsgroup)
{
    char pattern[]{"%t"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Bozo the Clown <bozo@example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromPathInNewsgroup)
{
    char pattern[]{"%T"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Bozo the Clown <bozo@example.org>", buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnreadArticlesInNewsgroup)
{
    char pattern[]{"%u"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("3", buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnreadArticlesExceptCurrentInNewsgroup)
{
    char pattern[]{"%U"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("2", buffer());
}

TEST_F(InterpolatorTest, numUnselectedArticlesExceptCurrentInNewsgroupEmpty)
{
    char pattern[]{"%v"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("2", buffer());
}

TEST_F(InterpolatorTest, threadDirInNewsgroup)
{
    char pattern[]{"%W"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_THREAD_DIR, buffer());
}

TEST_F(InterpolatorTest, shortenedFromInNewsgroup)
{
    char pattern[]{"%y"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FROM, buffer());
}

TEST_F(InterpolatorTest, articleSizeInNewsgroup)
{
    char pattern[]{"%z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ARTICLE_SIZE, buffer());
}

TEST_F(InterpolatorTest, numSelectedThreadsInNewsgroupEmpty)
{
    char pattern[]{"%Z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("3", buffer());
}

TEST_F(InterpolatorTest, headerFieldInNewsgroup)
{
    char pattern[]{"%[X-Boogie-Nights]"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_X_BOOGIE_NIGHTS, buffer());
}

TEST_F(InterpolatorTest, missingHeaderFieldInNewsgroupIsEmpty)
{
    char pattern[]{"%[X-Missing-Header]"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(buffer().empty()) << "Contents: '" << buffer() << "'";
}
#endif
