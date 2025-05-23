#include <gtest/gtest.h>

#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>

#include "mock_env.h"
#include "test_config.h"

#include <config/common.h>

#include <trn/ngdata.h>
#include <trn/addng.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artsrch.h>
#include <trn/backpage.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/init.h>
#include <trn/intrp.h>
#include <trn/kfile.h>
#include <trn/last.h>
#include <trn/mime.h>
#include <trn/ng.h>
#include <trn/ngsrch.h>
#include <trn/ngstuff.h>
#include <trn/only.h>
#include <trn/opt.h>
#include <trn/patchlevel.h>
#include <trn/rcln.h>
#include <trn/rcstuff.h>
#include <trn/respond.h>
#include <trn/rt-select.h>
#include <trn/rt-util.h>
#include <trn/rthread.h>
#include <trn/search.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/util.h>
#include <util/env-internal.h>
#include <util/util2.h>

constexpr int BUFFER_SIZE{4096};

using namespace testing;

namespace
{

using Environment = StrictMock<trn::testing::MockEnvironment>;

struct InterpolatorTest : Test
{
    ~InterpolatorTest() override = default;

protected:
    void SetUp() override;
    void TearDown() override;

    char *interpolate(char *pattern, const char *stoppers = "")
    {
        return do_interp(m_buffer.data(), BUFFER_SIZE, pattern, stoppers, nullptr);
    }

    std::string buffer() const
    {
        return m_buffer.data();
    }

    AssertionResult bufferIsEmpty() const
    {
        if (buffer().empty())
        {
            return AssertionSuccess();
        }

        return AssertionFailure() << "Contents: '" << buffer() << "'";
    }

    Environment                   m_env;
    std::array<char, TCBUF_SIZE>  m_tcbuf{};
    std::array<char, BUFFER_SIZE> m_buffer{};
    long                          m_test_pid{6421};
};

void InterpolatorTest::SetUp()
{
    Test::SetUp();

    g_our_pid = m_test_pid;
    term_init();
    mp_init();
    search_init();
    set_envars(m_env);
    m_env.expect_no_envars({"KILLGLOBAL", "KILLTHREADS", "MAILCAPS", "MIMECAPS", "NETSPEED", "NNTP_FORCE_AUTH",
                            "NNTPSERVER", "RNINIT", "RNMACRO", "RNRC"});
    env_init(m_tcbuf.data(), true, trn::testing::set_name, trn::testing::set_host_name);
    trn::testing::reset_lib_dirs();
    head_init();
    char trn[]{"trn"};
    char *argv[]{trn};
    char *tcbuf = m_tcbuf.data();
    opt_init(1,argv,&tcbuf);
    color_init();
    interp_init(tcbuf, TCBUF_SIZE);
    cwd_check();
    term_set(tcbuf);
    last_init();
    univ_init();
    data_source_init();
    rcstuff_init();
    add_ng_init();
    art_init();
    art_io_init();
    art_search_init();
    back_page_init();
    bits_init();
    cache_init();
    help_init();
    kill_file_init();
    mime_init();
    ng_init();
    newsgroup_search_init();
    newsgroup_stuff_init();
    only_init();
    rcln_init();
    respond_init();
    trn_init();
    decode_init();
    thread_init();
    util_init();
    xmouse_init(argv[0]);
}

void InterpolatorTest::TearDown()
{
    g_general_mode = GM_INIT;
    g_mode = MM_INITIALIZING;
    g_dm_count = 0;
    g_last_art = ArticleNum{};
    g_art = ArticleNum{};
    g_in_ng = false;
    g_data_source = nullptr;

    util_final();
    mime_final();
    art_io_final();
    rcstuff_final();
    data_source_finalize();
    last_final();
    interp_final();
    opt_final();
    head_final();
    env_final();
    reset_tty();

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
    m_env.expect_env("FOO", "value");
    char pattern[]{"%{FOO}"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("value", buffer());
}

TEST_F(InterpolatorTest, environmentVarValueDefault)
{
    m_env.expect_no_envar("FOO");
    char pattern[]{"%{FOO-not set}"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("not set", buffer());
}

TEST_F(InterpolatorTest, articleNumberOutsideNewsgroupIsEmpty)
{
    char pattern[]{"%a"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, articleNumberInsideNewsgroup)
{
    char pattern[]{"%a"};
    g_in_ng = true;
    g_art = ArticleNum{TRN_TEST_ARTICLE_NUM};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(TRN_TEST_ARTICLE_NUM), buffer());
}

TEST_F(InterpolatorTest, articleNameOutsideNewsgroup)
{
    char pattern[]{"%A"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, articleNameInsideLocalNewsgroupArticleClosed)
{
    char pattern[]{"%A"};
    g_in_ng = true;
    g_newsgroup_dir = TRN_TEST_NEWSGROUP_SUBDIR;
    g_last_art = ArticleNum{TRN_TEST_ARTICLE_NUM};
    g_art = ArticleNum{TRN_TEST_ARTICLE_NUM};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ARTICLE_FILE, buffer());
}

TEST_F(InterpolatorTest, saveDestinationNotSet)
{
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, saveDestinationSet)
{
    g_save_dest = "/tmp/frob";
    char pattern[]{"%b"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("/tmp/frob", buffer());
}

TEST_F(InterpolatorTest, relativeNewsgroupDir)
{
    g_newsgroup_dir = "comp/arch";
    char pattern[]{"%c"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp/arch", buffer());
}

TEST_F(InterpolatorTest, relativeNewsgroupDirNotSet)
{
    g_newsgroup_dir.clear();
    char pattern[]{"%c"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsgroupNameNotSet)
{
    g_newsgroup_name.clear();
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsgroupNameSet)
{
    g_newsgroup_name = "comp.arch";
    char pattern[]{"%C"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("comp.arch", buffer());
}

TEST_F(InterpolatorTest, absoluteNewsgroupDirNotSet)
{
    g_newsgroup_dir.clear();
    char pattern[]{"%d"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, oldDistributionLineNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%D"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, extractProgramNotSet)
{
    g_extract_prog.clear();
    char pattern[]{"%e"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("-", buffer());
}

TEST_F(InterpolatorTest, extractProgramSet)
{
    g_extract_prog = "uudecode";
    char pattern[]{"%e"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("uudecode", buffer());
}

TEST_F(InterpolatorTest, extractDestinationNotSet)
{
    g_extract_dest.clear();
    char pattern[]{"%E"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, extractDestinationSet)
{
    g_extract_dest = "/home/users/foo";
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
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, followupNotInNewsgroup)
{
    g_in_ng = false;
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
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
    ASSERT_TRUE(bufferIsEmpty());
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
    g_dm_count = 96;

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("96", buffer());
}

TEST_F(InterpolatorTest, newsgroupsLineNotInNewsgroup)
{
    char pattern[]{"%n"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, realName)
{
    char pattern[]{"%N"};
    m_env.expect_no_envar("NAME");

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_REAL_NAME, buffer());
}

TEST_F(InterpolatorTest, realNameFromNAME)
{
    char pattern[]{"%N"};
    m_env.expect_env("NAME", "John Yeager");

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("John Yeager", buffer());
}

TEST_F(InterpolatorTest, newsOrgFromConfiguration)
{
    char pattern[]{"%o"};
    m_env.expect_no_envar("NEWSORG");
    m_env.expect_no_envar("ORGANIZATION");
    // TODO: configure %X/organization contents

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsOrgFromNEWSORG)
{
    char pattern[]{"%o"};
    m_env.expect_env("NEWSORG", TRN_TEST_ORGANIZATION);

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromNEWSORGFile)
{
    char pattern[]{"%o"};
    m_env.expect_env("NEWSORG", TRN_TEST_ORGFILE);

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGANIZATION)
{
    char pattern[]{"%o"};
    m_env.expect_no_envar("NEWSORG");
    m_env.expect_env("ORGANIZATION", TRN_TEST_ORGANIZATION);

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGANIZATIONFile)
{
    char pattern[]{"%o"};
    m_env.expect_no_envar("NEWSORG");
    m_env.expect_env("ORGANIZATION", TRN_TEST_ORGFILE);

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, originalDirectory)
{
    char pattern[]{"%O"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(g_orig_dir, buffer());
}

TEST_F(InterpolatorTest, privateNewsDirectory)
{
    char pattern[]{"%p"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(g_priv_dir, buffer());
}

namespace
{

template <typename T>
class ValueSaver
{
public:
    ValueSaver(T &var, T new_value) :
        m_var(var),
        m_old_value(var)
    {
        m_var = new_value;
    }
    ~ValueSaver()
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
    ValueSaver<DataSource *> datasrc(g_data_source, nullptr);
    char                   pattern[]{"%P"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsSpoolDirectory)
{
    char pattern[]{"%P"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(g_data_source->spool_dir, buffer());
}

TEST_F(InterpolatorTest, lastInputStringInitiallyEmpty)
{
    char pattern[]{"%q"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, lastReferenceNotInNewsgroupEmpty)
{
    char pattern[]{"%r"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newReferencesNotInNewsgroupEmpty)
{
    char pattern[]{"%R"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, strippedSubjectNotInNewsgroupEmpty)
{
    char pattern[]{"%s"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, subjectNotInNewsgroupEmpty)
{
    char pattern[]{"%S"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, toFromFromReplyToNotInNewsgroupEmpty)
{
    char pattern[]{"%t"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, toFromPathNotInNewsgroupEmpty)
{
    char pattern[]{"%T"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numUnreadArticlesNotInNewsgroupEmpty)
{
    char pattern[]{"%u"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numUnreadArticlesExceptCurrentNotInNewsgroupEmpty)
{
    char pattern[]{"%U"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numUnselectedArticlesExceptCurrentNotInNewsgroupEmpty)
{
    char pattern[]{"%v"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, patchLevel)
{
    char pattern[]{"%V"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::string{PATCHLEVEL}, buffer());
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
    ASSERT_TRUE(bufferIsEmpty());
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
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numSelectedThreadsNotInNewsgroupEmpty)
{
    char pattern[]{"%Z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
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
    ValueSaver<int> saved(g_perform_count, 86);
    char pattern[]{"%#"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(g_perform_count), buffer());
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
    ValueSaver<int> saved(g_perform_count, 86);
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
    ASSERT_TRUE(bufferIsEmpty());
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
    ASSERT_TRUE(bufferIsEmpty());
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

TEST_F(InterpolatorTest, regexCapture)
{
    char pattern[]{R"pat(%(Abracadabra=^\(Ab\(.*\)bra\)$?0=%0, 1=%1, 2=%2:false))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("0=Abracadabra, 1=Abracadabra, 2=racada", buffer());
}

TEST_F(InterpolatorTest, escapeSpecialsModifier)
{
    char pattern[]{R"pat(%(Regex .* and percent \%p specials.=^\(.*\)$?%\0))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(R"text(Regex \.\* and percent \%p specials\.)text", buffer());
}

TEST_F(InterpolatorTest, addressModifier)
{
    char pattern[]{"%(" TRN_TEST_HEADER_FROM R"pat(=^\(.*\)$?%>0))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FROM_ADDRESS, buffer());
}

TEST_F(InterpolatorTest, nameModifier)
{
    char pattern[]{"%(" TRN_TEST_HEADER_FROM R"pat(=^\(.*\)$?%)0))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FROM_NAME, buffer());
}

TEST_F(InterpolatorTest, nameModifierFromParenValue)
{
    char pattern[]{R"pat(%(\(Bob the Builder\) <bob@example.org>=^\(.*\)$?%)0))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("(Bob the Builder)", buffer());
}

TEST_F(InterpolatorTest, nameModifierFromQuotedValue)
{
    char pattern[]{R"pat(%("Bob the Builder" <bob@example.org>=^\(.*\)$?%)0))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Bob the Builder", buffer());
}

TEST_F(InterpolatorTest, nameModifierFromQuotedValueStripsTrailingWhitespace)
{
    char pattern[]{R"pat(%("Bob the Builder    " <bob@example.org>=^\(.*\)$?%)0))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("Bob the Builder", buffer());
}

TEST_F(InterpolatorTest, tickModifier)
{
    char pattern[]{R"pat(%(Isn't this interesting?=^\(.*\)$?'%'0'))pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(R"text('Isn'\''t this interesting?')text", buffer());
}

TEST_F(InterpolatorTest, formatModifierLeftJustified)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%:-10g"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("p         ", buffer());
}

TEST_F(InterpolatorTest, formatModifierRightJustified)
{
    g_general_mode = GM_PROMPT;
    char pattern[]{"%:10g"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("         p", buffer());
}

TEST_F(InterpolatorTest, bell)
{
    char pattern[]{R"pat(\a)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\a", buffer());
}

TEST_F(InterpolatorTest, backspace)
{
    char pattern[]{R"pat(\b)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\b", buffer());
}

TEST_F(InterpolatorTest, formFeed)
{
    char pattern[]{R"pat(\f)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\f", buffer());
}

TEST_F(InterpolatorTest, newline)
{
    char pattern[]{R"pat(\n)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\n", buffer());
}

TEST_F(InterpolatorTest, carriageReturn)
{
    char pattern[]{R"pat(\r)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\r", buffer());
}

TEST_F(InterpolatorTest, horizontalTab)
{
    char pattern[]{R"pat(\t)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\t", buffer());
}

TEST_F(InterpolatorTest, verticalTab)
{
    char pattern[]{R"pat(\v)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\v", buffer());
}

TEST_F(InterpolatorTest, octalEscape)
{
    char pattern[]{R"pat(\122)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\122", buffer());
}

TEST_F(InterpolatorTest, octalEscapeOutOfRangeDigits)
{
    char pattern[]{R"pat(\4189)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("!89", buffer());
}

TEST_F(InterpolatorTest, hexEscapeLowerCase)
{
    char pattern[]{R"pat(\x4a)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\x4a", buffer());
}

TEST_F(InterpolatorTest, hexEscapeUpperCase)
{
    char pattern[]{R"pat(\x4A)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\x4A", buffer());
}

TEST_F(InterpolatorTest, hexEscapeOutOfRangeDigits)
{
    char pattern[]{R"pat(\x4G)pat"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\x4G", buffer());
}

TEST_F(InterpolatorTest, caretEscapeUpperCase)
{
    char pattern[]{"^G"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\a", buffer());
}

TEST_F(InterpolatorTest, caretEscapeLowerCase)
{
    char pattern[]{"^g"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("\a", buffer());
}

namespace
{

class PushDir
{
public:
    PushDir() :
        m_old_dir{}
    {
        getcwd(m_old_dir, sizeof(m_old_dir));
    }
    PushDir(const char *new_dir) :
        PushDir()
    {
        chdir(new_dir);
    }
    ~PushDir()
    {
        if (m_old_dir[0] != '\0')
        {
            chdir(m_old_dir);
        }
    }

    void push(const char *new_dir)
    {
        chdir(new_dir);
    }

private:
    char m_old_dir[TCBUF_SIZE];
};

struct InterpolatorNewsgroupTest : InterpolatorTest
{
protected:
    void SetUp() override;
    void TearDown() override;

    PushDir m_curdir;
};

void InterpolatorNewsgroupTest::SetUp()
{
    InterpolatorTest::SetUp();
    g_in_ng = true;
    g_art = ArticleNum{TRN_TEST_ARTICLE_NUM};
    g_last_art = ArticleNum{TRN_TEST_NEWSGROUP_HIGH};
    g_newsgroup_ptr = g_first_newsgroup;
    m_curdir.push(TRN_TEST_NEWSGROUP_DIR);
    build_cache();
}

void InterpolatorNewsgroupTest::TearDown()
{
    art_close();
    close_cache();
    g_in_ng = false;
    g_art = ArticleNum{-1};
    g_last_art = ArticleNum{-1};
    g_newsgroup_ptr = nullptr;
    InterpolatorTest::TearDown();
}

}

TEST_F(InterpolatorNewsgroupTest, absoluteNewsgroupDirSet)
{
    g_newsgroup_dir = TRN_TEST_NEWSGROUP_SUBDIR;
    char pattern[]{"%d"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_NEWSGROUP_DIR, buffer());
}

TEST_F(InterpolatorNewsgroupTest, oldDistributionLineInNewsgroup)
{
    char pattern[]{"%D"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_DISTRIBUTION, buffer());
}

TEST_F(InterpolatorNewsgroupTest, fromLineInNewsgroupNoReplyTo)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};
    char pattern[]{"%f"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FROM, buffer());
}

TEST_F(InterpolatorNewsgroupTest, fromLineInNewsgroupWithReplyTo)
{
    char pattern[]{"%f"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_REPLY_TO, buffer());
}

TEST_F(InterpolatorNewsgroupTest, followupInNewsgroupWithFollowupToLine)
{
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FOLLOWUP_TO, buffer());
}

TEST_F(InterpolatorNewsgroupTest, followupInNewsgroupFromNewsgroupsLine)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};
    char pattern[]{"%F"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_NEWSGROUPS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, messageIdInNewsgroup)
{
    char pattern[]{"%i"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_MESSAGE_ID, buffer());
}

TEST_F(InterpolatorNewsgroupTest, newsgroupsLineInNewsgroup)
{
    char pattern[]{"%n"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_NEWSGROUPS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, lastReferenceInNewsgroup)
{
    char pattern[]{"%r"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_LAST_REFERENCE, buffer());
}

TEST(NormalizeReferencesTest, normalize)
{
    char buffer[] = TRN_TEST_HEADER_REFERENCES;

    normalize_refs(buffer);

    ASSERT_EQ(TRN_TEST_HEADER_REFERENCES, std::string{buffer});
}

TEST_F(InterpolatorNewsgroupTest, newReferencesInNewsgroup)
{
    char pattern[]{"%R"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_REFERENCES " " TRN_TEST_HEADER_MESSAGE_ID, buffer());
}

TEST_F(InterpolatorNewsgroupTest, strippedSubjectInNewsgroupNoArticleIsEmpty)
{
    g_art = ArticleNum{};
    char pattern[]{"%s"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorNewsgroupTest, strippedSubjectInNewsgroup)
{
    g_artp = article_ptr(g_art);
    char pattern[]{"%s"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, buffer());
}

TEST_F(InterpolatorNewsgroupTest, oneReStrippedSubjectInNewsgroup)
{
    g_artp = article_ptr(g_art);
    char pattern[]{"%S"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_ONE_RE_SUBJECT, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromReplyToInNewsgroup)
{
    char pattern[]{"%t"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_REPLY_TO_ADDRESS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromFromInNewsgroup)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};
    char pattern[]{"%t"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FROM_ADDRESS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromPathInNewsgroup)
{
    char pattern[]{"%T"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_PATH, buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnreadArticlesInNewsgroup)
{
    char pattern[]{"%u"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(TRN_TEST_NEWSGROUP_HIGH - TRN_TEST_NEWSGROUP_LOW + 1), buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnreadArticlesExceptCurrentInNewsgroup)
{
    char pattern[]{"%U"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(TRN_TEST_NEWSGROUP_HIGH - TRN_TEST_NEWSGROUP_LOW), buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnselectedArticlesExceptCurrentInNewsgroupEmpty)
{
    char pattern[]{"%v"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(std::to_string(TRN_TEST_NEWSGROUP_HIGH - TRN_TEST_NEWSGROUP_LOW), buffer());
}

TEST_F(InterpolatorNewsgroupTest, shortenedFromInNewsgroup)
{
    char pattern[]{"%y"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_FROM, buffer());
}

TEST_F(InterpolatorNewsgroupTest, articleSizeInNewsgroup)
{
    std::ostringstream str;
    str << std::setw(5) << TRN_TEST_ARTICLE_SIZE;
    char pattern[]{"%z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(str.str(), buffer());
}

TEST_F(InterpolatorNewsgroupTest, numSelectedThreadsInNewsgroupEmpty)
{
    ValueSaver<ArticleUnread> saver(g_selected_count, 66);
    char pattern[]{"%Z"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ("66", buffer());
}

TEST_F(InterpolatorNewsgroupTest, headerFieldInNewsgroup)
{
    char pattern[]{"%[X-Boogie-Nights]"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_EQ(TRN_TEST_HEADER_X_BOOGIE_NIGHTS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, missingHeaderFieldInNewsgroupIsEmpty)
{
    char pattern[]{"%[X-Missing-Header]"};

    const char *new_pattern = interpolate(pattern);

    ASSERT_EQ('\0', *new_pattern);
    ASSERT_TRUE(bufferIsEmpty());
}
