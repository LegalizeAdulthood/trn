// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <trn/intrp.h>

#include <config/common.h>
#include <trn/addng.h>
#include <trn/art.h>
#include <trn/artio.h>
#include <trn/artsrch.h>
#include <trn/artstate.h>
#include <trn/backpage.h>
#include <trn/bits.h>
#include <trn/cache.h>
#include <trn/charsubst.h>
#include <trn/color.h>
#include <trn/datasrc.h>
#include <trn/file-contents.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/kfile.h>
#include <trn/last.h>
#include <trn/mime.h>
#include <trn/ng.h>
#include <trn/ngdata.h>
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
#include <trn/rt-wumpus.h>
#include <trn/rthread.h>
#include <trn/search.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/utf.h>
#include <trn/util.h>
#include <util/env-internal.h>
#include <util/util2.h>

#include "MockNNTPConnection.h"
#include "support/YencFixture.h"
#include <mock_env.h>
#include <test_config.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <memory>
#include <numeric>
#include <sstream>
#include <string>
#include <system_error>
#include <tuple>
#include <vector>

using namespace testing;

namespace
{

using Environment = StrictMock<trn::testing::MockEnvironment>;
namespace fs = std::filesystem;

constexpr char s_path_separator =
#ifdef _WIN32
    ';';
#else
    ':';
#endif

void create_failing_inews(const fs::path &directory)
{
    const fs::path script = directory / "inews";
    std::ofstream{script} << "#!/bin/sh\nexit 1\n";
    std::error_code error;
    fs::permissions(script, fs::perms::owner_exec, fs::perm_options::add, error);

#ifdef _WIN32
    std::ofstream{directory / "inews.cmd"} << "@exit /b 1\n";
#endif
}

class TestOutputDirectory
{
public:
    void SetUp();
    void TearDown();

    std::string path() const
    {
        return m_path.generic_string();
    }

private:
    static std::string test_name();
    static std::string sanitize(std::string name);

    std::filesystem::path m_path;
};

struct InterpolatorTest : Test
{
    ~InterpolatorTest() override = default;

protected:
    void SetUp() override;
    void TearDown() override;

    std::string_view interpolate(std::string_view pattern, std::string_view stoppers = {})
    {
        std::string_view cursor{pattern};
        m_buffer = do_interp(cursor, stoppers, {});
        return cursor;
    }

    const char *interpolate(char *pattern, std::string_view stoppers = {})
    {
        const std::string_view input{pattern};
        const std::string_view cursor = interpolate(input, stoppers);
        return pattern + (input.size() - cursor.size());
    }

    std::string buffer() const
    {
        return m_buffer;
    }

    AssertionResult bufferIsEmpty() const
    {
        if (buffer().empty())
        {
            return AssertionSuccess();
        }

        return AssertionFailure() << "Contents: '" << buffer() << "'";
    }

    Environment                  m_env;
    TestOutputDirectory          m_output;
    TermCapScratchBuffer         m_tcbuf{};
    std::string                  m_buffer;
    long                         m_test_pid{6421};
};

void TestOutputDirectory::SetUp()
{
    const std::filesystem::path source{TRN_TEST_DOT_DIR};
    m_path = std::filesystem::path{TRN_TEST_DATA_DIR}.parent_path() / "test_runs" / sanitize(test_name());

    std::error_code error;
    std::filesystem::remove_all(m_path, error);
    std::filesystem::create_directories(m_path);
    std::filesystem::copy_file(source / "local-newsrc", m_path / "local-newsrc");
    std::filesystem::copy_file(source / "nntp-newsrc", m_path / "nntp-newsrc");
}

void TestOutputDirectory::TearDown()
{
    std::error_code error;
    std::filesystem::remove_all(m_path, error);
}

std::string TestOutputDirectory::test_name()
{
    const TestInfo *info = UnitTest::GetInstance()->current_test_info();
    if (info == nullptr)
    {
        return "unknown";
    }

    return std::string{info->test_suite_name()} + "_" + info->name();
}

std::string TestOutputDirectory::sanitize(std::string name)
{
    std::replace_if(name.begin(), name.end(), [](unsigned char ch) { return !std::isalnum(ch); }, '_');
    return name;
}

void InterpolatorTest::SetUp()
{
    Test::SetUp();
    m_output.SetUp();

    g_our_pid = m_test_pid;
    term_init();
    search_init();
    set_envars(m_env, m_output.path());
    m_env.expect_no_envars({"KILLGLOBAL", "KILLTHREADS", "MAILCAPS", "MIMECAPS", "NETSPEED", "NNTP_FORCE_AUTH",
                            "NNTPSERVER", "RNINIT", "RNMACRO", "RNRC"});
    env_init(true, trn::testing::set_name, trn::testing::set_host_name);
    trn::testing::reset_lib_dirs();
    head_init();
    char trn[]{"trn"};
    char *argv[]{trn};
    opt_init(1, argv, m_tcbuf);
    color_init();
    interp_init();
    cwd_check();
    term_set(m_tcbuf);
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
    m_output.TearDown();

    Test::TearDown();
}

} // namespace

TEST_F(InterpolatorTest, noEscapes)
{
    constexpr std::string_view pattern{"this string contains no escapes"};

    const std::string_view new_pattern = interpolate(pattern);

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("this string contains no escapes", buffer());
}

TEST(InterpBackslashTest, decodesHexEscape)
{
    std::string_view pattern{"x41!"};

    EXPECT_EQ('A', interp_backslash(pattern));
    EXPECT_EQ("!", pattern);
}

TEST(InterpBackslashTest, decodesLowercaseHexEscape)
{
    std::string_view pattern{"x7a."};

    EXPECT_EQ('z', interp_backslash(pattern));
    EXPECT_EQ(".", pattern);
}

TEST(InterpBackslashTest, leavesHexEscapeWithoutDigits)
{
    std::string_view pattern{"xq"};

    EXPECT_EQ('x', interp_backslash(pattern));
    EXPECT_EQ("q", pattern);
}

TEST_F(InterpolatorTest, stringDoInterpReturnsInterpolatedText)
{
    EXPECT_EQ("this string contains no escapes", do_interp("this string contains no escapes"));
}

TEST_F(InterpolatorTest, stringDoInterpExpandsEnvironmentVar)
{
    m_env.expect_env("FOO", "value");

    EXPECT_EQ("value", do_interp("%{FOO}"));
}

TEST_F(InterpolatorTest, regexpQuoteEscapesSpecials)
{
    m_env.expect_env("FOO", R"(^$.*[\/?%)");

    EXPECT_EQ(R"(\^\$\.\*\[\\\/\?\%)", do_interp(R"(%\{FOO})"));
}

TEST_F(InterpolatorTest, regexpQuoteCollapsesMultipleSpaces)
{
    m_env.expect_env("FOO", "a   b");

    EXPECT_EQ("a *b", do_interp(R"(%\{FOO})"));
}

TEST_F(InterpolatorTest, referenceCursorDoInterpStopsBeforeStopper)
{
    m_env.expect_env("FOO", "value");
    std::string_view pattern{"%{FOO}|tail"};

    EXPECT_EQ("value", do_interp(pattern, "|", ""));
    EXPECT_EQ("|tail", pattern);
}

TEST_F(InterpolatorTest, referenceCursorDoInterpUsesSearchCommand)
{
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;
    std::string_view pattern{"%/"};

    EXPECT_EQ("needle?", do_interp(pattern, "", "?"));
    EXPECT_TRUE(pattern.empty());
}

TEST_F(InterpolatorTest, stringInterpSearchUsesSearchCommand)
{
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;

    EXPECT_EQ("needle?", interp_search("%/", "?"));
}

TEST_F(InterpolatorTest, articleSearchPattern)
{
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;
    interpolate("%/");

    EXPECT_EQ("/needle/", buffer());
}

TEST_F(InterpolatorTest, articleSearchPatternForOneHeader)
{
    g_last_pat = "needle";
    g_art_do_read = true;
    g_art_how_much = ARTSCOPE_ONE_HDR;
    g_art_srch_hdr = SUBJ_LINE;
    interpolate("%/");

    EXPECT_EQ("/needle/rHsubject", buffer());
}

TEST_F(InterpolatorTest, firstStopCharacter)
{
    constexpr std::string_view pattern{"this string contains no escapes [but contains stop characters]"};

    const std::string_view new_pattern = interpolate(pattern, "[]");

    ASSERT_FALSE(new_pattern.empty());
    ASSERT_EQ('[', new_pattern.front());
    ASSERT_EQ(std::string_view{"this string contains no escapes "}.size(), pattern.size() - new_pattern.size());
    ASSERT_EQ("[but contains stop characters]", new_pattern);
    ASSERT_EQ("this string contains no escapes ", buffer());
}

TEST_F(InterpolatorTest, subsequentStopCharacter)
{
    constexpr std::string_view pattern{"this string contains no escapes [but contains stop characters]"};

    const std::string_view new_pattern = interpolate(pattern, "()[]");

    ASSERT_FALSE(new_pattern.empty());
    ASSERT_EQ('[', new_pattern.front());
    ASSERT_EQ(std::string_view{"this string contains no escapes "}.size(), pattern.size() - new_pattern.size());
    ASSERT_EQ("[but contains stop characters]", new_pattern);
    ASSERT_EQ("this string contains no escapes ", buffer());
}

TEST_F(InterpolatorTest, escapedStopCharacterDoesNotStop)
{
    constexpr std::string_view pattern{R"(this string contains an escaped \[ stop [but contains stop characters])"};

    const std::string_view new_pattern = interpolate(pattern, "[]");

    ASSERT_FALSE(new_pattern.empty());
    ASSERT_EQ('[', new_pattern.front());
    ASSERT_EQ(std::string_view{R"(this string contains an escaped \[ stop )"}.size(),
              pattern.size() - new_pattern.size());
    ASSERT_EQ("[but contains stop characters]", new_pattern);
    ASSERT_EQ("this string contains an escaped [ stop ", buffer());
}

TEST_F(InterpolatorTest, stopCharactersNotFoundReturnsEnd)
{
    constexpr std::string_view pattern{"this string contains no stop characters"};

    const std::string_view new_pattern = interpolate(pattern, "[]");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("this string contains no stop characters", buffer());
}

TEST_F(InterpolatorTest, tilde)
{
    const std::string_view new_pattern = interpolate("%~");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HOME_DIR, buffer());
}

TEST_F(InterpolatorTest, dotDir)
{
    const std::string_view new_pattern = interpolate("%.");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_DOT_DIR, buffer());
}

TEST_F(InterpolatorTest, processId)
{
    const std::string_view new_pattern = interpolate("%$");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(m_test_pid), buffer());
}

TEST_F(InterpolatorTest, environmentVarValue)
{
    m_env.expect_env("FOO", "value");

    const std::string_view new_pattern = interpolate("%{FOO}");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("value", buffer());
}

TEST_F(InterpolatorTest, environmentVarValueDefault)
{
    m_env.expect_no_envar("FOO");

    const std::string_view new_pattern = interpolate("%{FOO-not set}");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("not set", buffer());
}

TEST_F(InterpolatorTest, articleNumberOutsideNewsgroupIsEmpty)
{
    const std::string_view new_pattern = interpolate("%a");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, articleNumberInsideNewsgroup)
{
    g_in_ng = true;
    g_art = ArticleNum{TRN_TEST_ARTICLE_NUM};

    const std::string_view new_pattern = interpolate("%a");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(TRN_TEST_ARTICLE_NUM), buffer());
}

TEST_F(InterpolatorTest, articleNameOutsideNewsgroup)
{
    const std::string_view new_pattern = interpolate("%A");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, articleNameInsideLocalNewsgroupArticleClosed)
{
    g_in_ng = true;
    g_newsgroup_dir = TRN_TEST_NEWSGROUP_SUBDIR;
    g_last_art = ArticleNum{TRN_TEST_ARTICLE_NUM};
    g_art = ArticleNum{TRN_TEST_ARTICLE_NUM};

    const std::string_view new_pattern = interpolate("%A");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_ARTICLE_FILE, buffer());
}

TEST_F(InterpolatorTest, saveDestinationNotSet)
{
    const std::string_view new_pattern = interpolate("%b");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, saveDestinationSet)
{
    g_save_dest = "/tmp/frob";

    const std::string_view new_pattern = interpolate("%b");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("/tmp/frob", buffer());
}

TEST_F(InterpolatorTest, relativeNewsgroupDir)
{
    g_newsgroup_dir = "comp/arch";

    const std::string_view new_pattern = interpolate("%c");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("comp/arch", buffer());
}

TEST_F(InterpolatorTest, relativeNewsgroupDirNotSet)
{
    g_newsgroup_dir.clear();

    const std::string_view new_pattern = interpolate("%c");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsgroupNameNotSet)
{
    g_newsgroup_name.clear();

    const std::string_view new_pattern = interpolate("%C");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsgroupNameSet)
{
    g_newsgroup_name = "comp.arch";

    const std::string_view new_pattern = interpolate("%C");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("comp.arch", buffer());
}

TEST_F(InterpolatorTest, absoluteNewsgroupDirNotSet)
{
    g_newsgroup_dir.clear();

    const std::string_view new_pattern = interpolate("%d");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, oldDistributionLineNotInNewsgroup)
{
    g_in_ng = false;

    const std::string_view new_pattern = interpolate("%D");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, extractProgramNotSet)
{
    g_extract_prog.clear();

    const std::string_view new_pattern = interpolate("%e");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("-", buffer());
}

TEST_F(InterpolatorTest, extractProgramSet)
{
    g_extract_prog = "uudecode";

    const std::string_view new_pattern = interpolate("%e");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("uudecode", buffer());
}

TEST_F(InterpolatorTest, extractDestinationNotSet)
{
    g_extract_dest.clear();

    const std::string_view new_pattern = interpolate("%E");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, extractDestinationSet)
{
    g_extract_dest = "/home/users/foo";

    const std::string_view new_pattern = interpolate("%E");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("/home/users/foo", buffer());
}

TEST_F(InterpolatorTest, fromLineNotInNewsgroup)
{
    g_in_ng = false;

    const std::string_view new_pattern = interpolate("%f");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, followupNotInNewsgroup)
{
    g_in_ng = false;

    const std::string_view new_pattern = interpolate("%F");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, generalMode)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%g");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("p", buffer());
}

TEST_F(InterpolatorTest, headerFileName)
{
    const std::string_view new_pattern = interpolate("%h");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::string{TRN_TEST_DOT_DIR} + "/.rnhead." + std::to_string(m_test_pid), buffer());
}

TEST_F(InterpolatorTest, hostName)
{
    const std::string_view new_pattern = interpolate("%H");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_P_HOST_NAME, buffer());
}

TEST_F(InterpolatorTest, hostMatchName)
{
    ASSERT_EQ(host_match_name(TRN_TEST_P_HOST_NAME, POSTING_HOST_MATCH_SEGMENTS), g_host_name);
}

TEST(HostMatchNameTest, zeroUsesWholeHostName)
{
    EXPECT_EQ("client.news.example.org", host_match_name("client.news.example.org", 0));
}

TEST(HostMatchNameTest, oneUsesTwoHostWords)
{
    EXPECT_EQ("example.org", host_match_name("client.news.example.org", 1));
}

TEST(HostMatchNameTest, twoUsesTwoHostWords)
{
    EXPECT_EQ("example.org", host_match_name("client.news.example.org", 2));
}

TEST(HostMatchNameTest, largerCountUsesRequestedHostWords)
{
    EXPECT_EQ("news.example.org", host_match_name("client.news.example.org", 3));
}

TEST_F(InterpolatorTest, messageIdNotInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%i");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, indentString)
{
    const std::string_view new_pattern = interpolate("%I");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("'>'", buffer());
}

TEST_F(InterpolatorTest, doShellPassesQuotecharsFromIndentString)
{
#ifdef USE_SPAWN_SHELL
    EXPECT_EQ(0, do_shell(SH, "if not defined QUOTECHARS exit 1"));
    EXPECT_EQ(">", get_env_var("QUOTECHARS"));
#else
    EXPECT_EQ(0, do_shell(SH, "test \"$QUOTECHARS\" = '>'"));
#endif
}

TEST_F(InterpolatorTest, approximateBaudRate)
{
    const std::string_view new_pattern = interpolate("%j");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(g_just_a_sec*10), buffer());
}

#ifndef HAS_NEWS_ADMIN
TEST_F(InterpolatorTest, noNewsAdmin)
{
    const std::string_view new_pattern = interpolate("%l");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("???", buffer());
}
#endif

TEST_F(InterpolatorTest, loginName)
{
    const std::string_view new_pattern = interpolate("%L");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_LOGIN_NAME, buffer());
}

TEST_F(InterpolatorTest, minorMode)
{
    g_mode = MM_DELETE_BOGUS_NEWSGROUPS_PROMPT;

    const std::string_view new_pattern = interpolate("%m");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("D", buffer());
}

TEST_F(InterpolatorTest, markCount)
{
    g_dm_count = 96;

    const std::string_view new_pattern = interpolate("%M");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("96", buffer());
}

TEST_F(InterpolatorTest, newsgroupsLineNotInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%n");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, realName)
{
    m_env.expect_no_envar("NAME");

    const std::string_view new_pattern = interpolate("%N");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_REAL_NAME, buffer());
}

TEST_F(InterpolatorTest, realNameFromNAME)
{
    m_env.expect_env("NAME", "John Yeager");

    const std::string_view new_pattern = interpolate("%N");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("John Yeager", buffer());
}

TEST_F(InterpolatorTest, newsOrgFromConfiguration)
{
    m_env.expect_no_envar("NEWSORG");
    m_env.expect_no_envar("ORGANIZATION");
    // TODO: configure %X/organization contents

    const std::string_view new_pattern = interpolate("%o");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsOrgFromNEWSORG)
{
    m_env.expect_env("NEWSORG", TRN_TEST_ORGANIZATION);

    const std::string_view new_pattern = interpolate("%o");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromNEWSORGFile)
{
    m_env.expect_env("NEWSORG", TRN_TEST_ORGFILE);

    const std::string_view new_pattern = interpolate("%o");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGANIZATION)
{
    m_env.expect_no_envar("NEWSORG");
    m_env.expect_env("ORGANIZATION", TRN_TEST_ORGANIZATION);

    const std::string_view new_pattern = interpolate("%o");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, newsOrgFromORGANIZATIONFile)
{
    m_env.expect_no_envar("NEWSORG");
    m_env.expect_env("ORGANIZATION", TRN_TEST_ORGFILE);

    const std::string_view new_pattern = interpolate("%o");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_ORGANIZATION, buffer());
}

TEST_F(InterpolatorTest, originalDirectory)
{
    const std::string_view new_pattern = interpolate("%O");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(g_orig_dir, buffer());
}

TEST_F(InterpolatorTest, privateNewsDirectory)
{
    const std::string_view new_pattern = interpolate("%p");

    ASSERT_TRUE(new_pattern.empty());
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

    const std::string_view new_pattern = interpolate("%P");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newsSpoolDirectory)
{
    const std::string_view new_pattern = interpolate("%P");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(g_data_source->m_spool_dir, buffer());
}

TEST_F(InterpolatorTest, lastInputStringInitiallyEmpty)
{
    const std::string_view new_pattern = interpolate("%q");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, lastReferenceNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%r");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, newReferencesNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%R");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, strippedSubjectNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%s");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, subjectNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%S");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, toFromFromReplyToNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%t");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, toFromPathNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%T");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numUnreadArticlesNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%u");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numUnreadArticlesExceptCurrentNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%U");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numUnselectedArticlesExceptCurrentNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%v");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, patchLevel)
{
    const std::string_view new_pattern = interpolate("%V");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::string{PATCHLEVEL}, buffer());
}

TEST_F(InterpolatorTest, libDir)
{
    const std::string_view new_pattern = interpolate("%x");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_LIB_DIR, buffer());
}

TEST_F(InterpolatorTest, rnLibDir)
{
    const std::string_view new_pattern = interpolate("%X");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_RN_LIB_DIR, buffer());
}

TEST_F(InterpolatorTest, shortenedFromNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%y");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, tmpDir)
{
    const std::string_view new_pattern = interpolate("%Y");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(m_output.path(), buffer());
}

TEST_F(InterpolatorTest, articleSizeNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%z");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, numSelectedThreadsNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%Z");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, trailingPercentRemains)
{
    const std::string_view new_pattern = interpolate("%");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("%", buffer());
}

TEST_F(InterpolatorTest, unknownEscapeIsLiteral)
{
    const std::string_view new_pattern = interpolate("%!");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("!", buffer());
}

TEST_F(InterpolatorTest, unknownEscapePreservesMetabit)
{
    const std::string_view new_pattern = interpolate("^(%!^)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(0200 | '!', static_cast<unsigned char>(m_buffer[0]));
    ASSERT_EQ(std::size_t{1}, m_buffer.size());
}

TEST_F(InterpolatorTest, performCount)
{
    ValueSaver<int> saved(g_perform_count, 86);

    const std::string_view new_pattern = interpolate("%#");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(g_perform_count), buffer());
}

TEST_F(InterpolatorTest, modifiedPerformCountNotZero)
{
    const std::string_view new_pattern = interpolate("%^#");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_NE("0", buffer());
}

TEST_F(InterpolatorTest, consecutiveModifiedPerformCountIncreases)
{
    ValueSaver<int> saved(g_perform_count, 86);

    const std::string_view new_pattern = interpolate("%^#,%^#");
    std::istringstream str{buffer()};
    int value1;
    int value2;
    char comma;
    str >> value1 >> comma >> value2;

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(str.eof());
    ASSERT_EQ(',', comma);
    ASSERT_GT(value2, value1);
}

TEST_F(InterpolatorTest, loginNamecCapitalized)
{
    const std::string_view new_pattern = interpolate("%^L");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(!std::isupper(TRN_TEST_LOGIN_NAME[0]));
    ASSERT_EQ(static_cast<char>(std::toupper(TRN_TEST_LOGIN_NAME[0])), buffer()[0]);
}

TEST_F(InterpolatorTest, equalTriviallyTrue)
{
    const std::string_view new_pattern = interpolate("%(x=x?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, equalTriviallyFalse)
{
    const std::string_view new_pattern = interpolate("%(x=y?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, interpolatedTestEqualTrue)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%(%g=p?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, interpolatedTestEqualFalse)
{
    g_general_mode = GM_INIT;

    const std::string_view new_pattern = interpolate("%(%g=p?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, equalInterpolatedTrue)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%(x=x?%g:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("p", buffer());
}

TEST_F(InterpolatorTest, equalInterpolatedFalse)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%(x=y?true:%g)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("p", buffer());
}

TEST_F(InterpolatorTest, conditionalTrueBranchStopsBeforeOuterStopper)
{
    constexpr std::string_view pattern{"%(x=x?true:false)|tail"};

    const std::string_view new_pattern = interpolate(pattern, "|");
    const std::size_t stopper = pattern.find('|');

    ASSERT_FALSE(new_pattern.empty());
    ASSERT_EQ('|', new_pattern.front());
    ASSERT_EQ(stopper, pattern.size() - new_pattern.size());
    ASSERT_EQ("|tail", new_pattern);
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, conditionalFalseBranchStopsBeforeOuterStopper)
{
    constexpr std::string_view pattern{"%(x=y?true:false)|tail"};

    const std::string_view new_pattern = interpolate(pattern, "|");
    const std::size_t stopper = pattern.find('|');

    ASSERT_FALSE(new_pattern.empty());
    ASSERT_EQ('|', new_pattern.front());
    ASSERT_EQ(stopper, pattern.size() - new_pattern.size());
    ASSERT_EQ("|tail", new_pattern);
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, conditionalSkipsNestedUnusedBranch)
{
    constexpr std::string_view pattern{"%(x=y?%(a=a?wrong:wrong):right)|tail"};

    const std::string_view new_pattern = interpolate(pattern, "|");
    const std::size_t stopper = pattern.find('|');

    ASSERT_FALSE(new_pattern.empty());
    ASSERT_EQ('|', new_pattern.front());
    ASSERT_EQ(stopper, pattern.size() - new_pattern.size());
    ASSERT_EQ("|tail", new_pattern);
    ASSERT_EQ("right", buffer());
}

TEST_F(InterpolatorTest, skipInterpSkipsNestedConditional)
{
    const std::string_view pattern{"%(x=x?yes:%(a=a?skip:skip))|tail"};

    EXPECT_EQ(pattern.find('|'), skip_interp(pattern, "|"));
}

TEST_F(InterpolatorTest, skipInterpSkipsBacktickText)
{
    const std::string_view pattern{"%`printf |`|tail"};

    EXPECT_EQ(pattern.rfind('|'), skip_interp(pattern, "|"));
}

TEST_F(InterpolatorTest, skipInterpSkipsPromptText)
{
    const std::string_view pattern{"%\"prompt | text\"|tail"};

    EXPECT_EQ(pattern.rfind('|'), skip_interp(pattern, "|"));
}

TEST_F(InterpolatorTest, notEqualTriviallyTrue)
{
    const std::string_view new_pattern = interpolate("%(x!=y?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, notEqualTriviallyFalse)
{
    const std::string_view new_pattern = interpolate("%(x!=x?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("false", buffer());
}

TEST_F(InterpolatorTest, regexMatched)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%(%g=^p$?true:false)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, triviallyTrueNoElse)
{
    const std::string_view new_pattern = interpolate("%(x=x?true)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("true", buffer());
}

TEST_F(InterpolatorTest, triviallyFalseNoElseIsEmpty)
{
    const std::string_view new_pattern = interpolate("%(x=y?true)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, escapedPercent)
{
    const std::string_view new_pattern = interpolate(R"(\%g)");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("%g", buffer());
}

TEST_F(InterpolatorTest, headerFieldNotInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%[X-Boogie-Nights]");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorTest, homeDirectoryCapitalized)
{
    const std::string_view new_pattern = interpolate("%_~");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HOME_DIR_CAPITALIZED, buffer());
}

TEST_F(InterpolatorTest, spaceForShortLine)
{
    const std::string_view new_pattern = interpolate("%?");

    ASSERT_TRUE(new_pattern.empty());
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
    const std::string_view new_pattern = interpolate(TRN_TEST_79_SPACES "%?");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_79_SPACES "\n", buffer());
}

#define TRN_TEST_80_SPACES TRN_TEST_40_SPACES TRN_TEST_40_SPACES
TEST_F(InterpolatorTest, newlineForLongerThan79CharsLine)
{
    const std::string_view new_pattern = interpolate(TRN_TEST_80_SPACES "%?");

    ASSERT_TRUE(new_pattern.empty());
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
    const std::string_view new_pattern = interpolate(R"pat(%(Abracadabra=^\(Ab\(.*\)bra\)$?0=%0, 1=%1, 2=%2:false))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("0=Abracadabra, 1=Abracadabra, 2=racada", buffer());
}

TEST_F(InterpolatorTest, escapeSpecialsModifier)
{
    const std::string_view new_pattern = interpolate(R"pat(%(Regex .* and percent \%p specials.=^\(.*\)$?%\0))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(R"text(Regex \.\* and percent \%p specials\.)text", buffer());
}

TEST_F(InterpolatorTest, addressModifier)
{
    const std::string_view new_pattern = interpolate("%(" TRN_TEST_HEADER_FROM R"pat(=^\(.*\)$?%>0))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM_ADDRESS, buffer());
}

TEST_F(InterpolatorTest, addressModifierDecodesHeader)
{
    m_env.expect_env("FROM", "=?US-ASCII?B?Qm96byB0aGUgQ2xvd24=?= <bozo@clown-world.org>");

    const std::string_view new_pattern = interpolate("%>{FROM}");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM_ADDRESS, buffer());
}

TEST_F(InterpolatorTest, nameModifier)
{
    const std::string_view new_pattern = interpolate("%(" TRN_TEST_HEADER_FROM R"pat(=^\(.*\)$?%)0))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM_NAME, buffer());
}

TEST_F(InterpolatorTest, commentModifierDecodesHeader)
{
    m_env.expect_env("FROM", "bozo@clown-world.org (=?US-ASCII?B?Qm96byB0aGUgQ2xvd24=?=)");

    const std::string_view new_pattern = interpolate("%){FROM}");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM_NAME, buffer());
}

TEST_F(InterpolatorTest, nameModifierFromParenValue)
{
    const std::string_view new_pattern = interpolate(R"pat(%(\(Bob the Builder\) <bob@example.org>=^\(.*\)$?%)0))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("(Bob the Builder)", buffer());
}

TEST_F(InterpolatorTest, nameModifierFromQuotedValue)
{
    const std::string_view new_pattern = interpolate(R"pat(%("Bob the Builder" <bob@example.org>=^\(.*\)$?%)0))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("Bob the Builder", buffer());
}

TEST_F(InterpolatorTest, nameModifierFromQuotedValueStripsTrailingWhitespace)
{
    const std::string_view new_pattern =
        interpolate(R"pat(%("Bob the Builder    " <bob@example.org>=^\(.*\)$?%)0))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("Bob the Builder", buffer());
}

TEST_F(InterpolatorTest, tickModifier)
{
    const std::string_view new_pattern = interpolate(R"pat(%(Isn't this interesting?=^\(.*\)$?'%'0'))pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(R"text('Isn'\''t this interesting?')text", buffer());
}

TEST_F(InterpolatorTest, formatModifierLeftJustified)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%:-10g");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("p         ", buffer());
}

TEST_F(InterpolatorTest, formatModifierRightJustified)
{
    g_general_mode = GM_PROMPT;

    const std::string_view new_pattern = interpolate("%:10g");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("         p", buffer());
}

TEST_F(InterpolatorTest, bell)
{
    const std::string_view new_pattern = interpolate(R"pat(\a)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\a", buffer());
}

TEST_F(InterpolatorTest, backspace)
{
    const std::string_view new_pattern = interpolate(R"pat(\b)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\b", buffer());
}

TEST_F(InterpolatorTest, formFeed)
{
    const std::string_view new_pattern = interpolate(R"pat(\f)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\f", buffer());
}

TEST_F(InterpolatorTest, newline)
{
    const std::string_view new_pattern = interpolate(R"pat(\n)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\n", buffer());
}

TEST_F(InterpolatorTest, carriageReturn)
{
    const std::string_view new_pattern = interpolate(R"pat(\r)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\r", buffer());
}

TEST_F(InterpolatorTest, horizontalTab)
{
    const std::string_view new_pattern = interpolate(R"pat(\t)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\t", buffer());
}

TEST_F(InterpolatorTest, verticalTab)
{
    const std::string_view new_pattern = interpolate(R"pat(\v)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\v", buffer());
}

TEST_F(InterpolatorTest, octalEscape)
{
    const std::string_view new_pattern = interpolate(R"pat(\122)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\122", buffer());
}

TEST_F(InterpolatorTest, octalEscapeOutOfRangeDigits)
{
    const std::string_view new_pattern = interpolate(R"pat(\4189)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("!89", buffer());
}

TEST_F(InterpolatorTest, hexEscapeLowerCase)
{
    const std::string_view new_pattern = interpolate(R"pat(\x4a)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\x4a", buffer());
}

TEST_F(InterpolatorTest, hexEscapeUpperCase)
{
    const std::string_view new_pattern = interpolate(R"pat(\x4A)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\x4A", buffer());
}

TEST_F(InterpolatorTest, hexEscapeOutOfRangeDigits)
{
    const std::string_view new_pattern = interpolate(R"pat(\x4G)pat");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\x4G", buffer());
}

TEST_F(InterpolatorTest, caretEscapeUpperCase)
{
    const std::string_view new_pattern = interpolate("^G");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\a", buffer());
}

TEST_F(InterpolatorTest, caretEscapeLowerCase)
{
    const std::string_view new_pattern = interpolate("^g");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("\a", buffer());
}

namespace
{

class PushDir
{
public:
    PushDir() :
        m_old_dir{fs::current_path()}
    {
    }
    explicit PushDir(const fs::path &new_dir) :
        PushDir()
    {
        push(new_dir);
    }
    ~PushDir()
    {
        pop();
    }

    void push(const fs::path &new_dir)
    {
        fs::current_path(new_dir);
    }

    void pop()
    {
        if (!m_old_dir.empty())
        {
            std::error_code error;
            fs::current_path(m_old_dir, error);
            m_old_dir.clear();
        }
    }

private:
    fs::path m_old_dir;
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
    ASSERT_NE(nullptr, g_data_source);
    g_data_source->m_flags &= ~DF_REMOTE;
    g_data_source->m_spool_dir = TRN_TEST_LOCAL_SPOOL_DIR;
    g_newsgroup_dir = TRN_TEST_NEWSGROUP_SUBDIR;
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
    m_curdir.pop();
    InterpolatorTest::TearDown();
}

class ArticleHeaderPresentationTest : public InterpolatorNewsgroupTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void render_article();

    DoArticleResult m_result{DA_NORM};
    std::string     m_rendered;
    std::string     m_article_command;

private:
    int                                      m_old_mouse_bar_count{g_mouse_bar_cnt};
    decltype(g_header_type[FROM_LINE].flags) m_old_from_flags{g_header_type[FROM_LINE].flags};
    ArticleLine                              m_old_top_line{g_top_line};
    ArticleLine                              m_old_init_lines{g_init_lines};
    int                                      m_old_lines{g_tc_LINES};
    int                                      m_old_columns{g_tc_COLS};
    std::string_view                         m_old_input{input_charset_name()};
    std::string_view                         m_old_output{output_charset_name()};
    std::string                              m_old_charsets{g_charsets};
    char                                     m_old_subst{current_char_subst_mode()};
    std::string                              m_old_prompt{g_prompt};
    Article                                 *m_old_current_article{g_curr_artp};
    Article                                 *m_old_article{g_artp};
    bool                                     m_capturing{};
};

void ArticleHeaderPresentationTest::SetUp()
{
    InterpolatorNewsgroupTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    g_mouse_bar_cnt = 0;
    g_header_type[FROM_LINE].flags |= HT_MAGIC;
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));
}

void ArticleHeaderPresentationTest::TearDown()
{
    if (m_capturing)
    {
        (void) testing::internal::GetCapturedStdout();
        m_capturing = false;
    }
    InterpolatorNewsgroupTest::TearDown();
    g_mouse_bar_cnt = m_old_mouse_bar_count;
    g_header_type[FROM_LINE].flags = m_old_from_flags;
    g_top_line = m_old_top_line;
    g_init_lines = m_old_init_lines;
    g_tc_LINES = m_old_lines;
    g_tc_COLS = m_old_columns;
    utf_init(m_old_input, m_old_output);
    g_charsets = m_old_charsets;
    set_char_subst_mode(m_old_subst);
    g_prompt = m_old_prompt;
    g_curr_artp = m_old_current_article;
    g_artp = m_old_article;
}

void ArticleHeaderPresentationTest::render_article()
{
    testing::internal::CaptureStdout();
    m_capturing = true;
    m_result = do_article(m_article_command);
    m_rendered = testing::internal::GetCapturedStdout();
    m_capturing = false;
}

void write_article_body(const fs::path &article_file, std::string_view body, std::string_view extra_headers = {})
{
    std::ofstream{article_file} << "Path: " TRN_TEST_HEADER_PATH "\n"
                                << "From: " TRN_TEST_HEADER_FROM "\n"
                                << "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                << "Article: " << TRN_TEST_ARTICLE_NUM << "\n"
                                << "Subject: " TRN_TEST_HEADER_SUBJECT "\n"
                                << "Date: " TRN_TEST_HEADER_DATE "\n"
                                << "Message-Id: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                << extra_headers << "Lines: 1\n\n"
                                << body;
}

using PagerBodyPrefix = std::pair<long, std::string_view>;

struct PagerMovementCase
{
    const char                    *name;
    const char                    *commands;
    int                            screen_lines;
    bool                           erase_screen;
    bool                           erase_each_line;
    MarkingMode                    marking;
    MarkingAreas                   marking_areas;
    std::vector<std::vector<long>> pages;
    long                           next_line;
    long                           top_line;
    long                           body_lines{40};
    long                           header_lines{};
    long                           page_break_line{-1};
    std::vector<long>              highlighted_lines;
    bool                           can_insert_line{};
    bool                           can_home{true};
    int                            insertions{};
    int                            search_context{3};
    std::vector<PagerBodyPrefix>   line_prefixes;
    int                            not_found_count{};
    std::vector<std::size_t>       rotated_pages;
    bool                           expected_rotate{};
    char                           expected_subst{'p'};
    std::string_view               expected_text;
};

void PrintTo(const PagerMovementCase &value, std::ostream *output)
{
    *output << value.name;
}

PagerMovementCase with_line_insertion(PagerMovementCase scenario, int insertions = 1, bool can_home = true)
{
    scenario.can_insert_line = true;
    scenario.can_home = can_home;
    scenario.insertions = insertions;
    return scenario;
}

PagerMovementCase search_case(const char *name, const char *commands, std::vector<std::vector<long>> pages,
                              long next_line, long top_line, std::vector<long> highlighted_lines = {}, int context = 3,
                              int not_found_count = 0)
{
    PagerMovementCase scenario{name, commands, 10, false, false, STANDOUT, NONE, std::move(pages), next_line, top_line};
    scenario.highlighted_lines = std::move(highlighted_lines);
    scenario.search_context = context;
    scenario.not_found_count = not_found_count;
    return scenario;
}

PagerMovementCase with_body_prefix(PagerMovementCase scenario, long first, long last, std::string_view prefix)
{
    for (long line = first; line <= last; ++line)
    {
        scenario.line_prefixes.emplace_back(line, prefix);
    }
    return scenario;
}

PagerMovementCase with_rotated_pages(PagerMovementCase scenario, std::vector<std::size_t> pages, bool rotated)
{
    scenario.rotated_pages = std::move(pages);
    scenario.expected_rotate = rotated;
    return scenario;
}

PagerMovementCase with_character_conversion(PagerMovementCase scenario, char mode, std::string_view text)
{
    scenario.header_lines = 2;
    scenario.expected_subst = mode;
    scenario.expected_text = text;
    const long last_line = scenario.body_lines - 1;
    return with_body_prefix(std::move(scenario), 0, last_line, "\"a ");
}

class PagerArticleTest : public ArticleHeaderPresentationTest
{
protected:
    void             SetUp() override;
    void             TearDown() override;
    void             load_body(std::string_view body, std::string_view extra_headers = {});
    void             load_article(std::string_view headers, std::string_view body);
    void             read_pages(std::string_view commands);
    std::string_view page_text(std::size_t page) const;

    ArticlePosition m_body_start{};

private:
    fs::path                     m_previous_directory;
    ValueSaver<std::string>      m_group_dir{g_newsgroup_dir, g_newsgroup_dir};
    ValueSaver<ArticleNum>       m_current_article{g_curr_art, g_curr_art};
    ValueSaver<ArticleLine>      m_line{g_art_line_num, g_art_line_num};
    ValueSaver<ArticlePosition>  m_position{g_art_pos, g_art_pos};
    ValueSaver<HeaderLineType>   m_in_header{g_in_header, g_in_header};
    ValueSaver<bool>             m_do_fseek{g_do_fseek, g_do_fseek};
    ValueSaver<bool>             m_erase_screen{g_erase_screen, g_erase_screen};
    ValueSaver<bool>             m_erase_each_line{g_erase_each_line, g_erase_each_line};
    ValueSaver<MarkingMode>      m_marking{g_marking, g_marking};
    ValueSaver<MarkingAreas>     m_marking_areas{g_marking_areas, g_marking_areas};
    ValueSaver<ArticleLine>      m_highlight{g_highlight, g_highlight};
    ValueSaver<ArticlePosition>  m_inner_search{g_inner_search, g_inner_search};
    ValueSaver<ArticleLine>      m_inner_light{g_inner_light, g_inner_light};
    ValueSaver<char>             m_hide_everything{g_hide_everything, g_hide_everything};
    ValueSaver<std::string>      m_page_stop{g_page_stop, g_page_stop};
    ValueSaver<int>              m_check_count{g_check_count, g_check_count};
    ValueSaver<int>              m_search_context{g_g_line, g_g_line};
    ValueSaver<char>             m_erase_char{g_erase_char, g_erase_char};
    ValueSaver<bool>             m_rotate{g_rotate, g_rotate};
    ValueSaver<GeneralMode>      m_general_mode{g_general_mode, g_general_mode};
    ValueSaver<MinorMode>        m_mode{g_mode, g_mode};
    ValueSaver<std::string_view> m_clear{g_tc_CD, g_tc_CD};
    ValueSaver<std::string_view> m_home{g_tc_HO, g_tc_HO};
    ValueSaver<std::string_view> m_up{g_tc_UP, g_tc_UP};
    ValueSaver<std::string_view> m_standout{g_tc_SO, g_tc_SO};
    ValueSaver<std::string_view> m_end_standout{g_tc_SE, g_tc_SE};
    ValueSaver<std::string_view> m_insert_line{g_tc_IL, g_tc_IL};
};

void PagerArticleTest::SetUp()
{
    ArticleHeaderPresentationTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    g_charsets = "pt";
    set_char_subst_mode('p');
    g_rotate = false;
    m_previous_directory = fs::current_path();
    fs::current_path(m_output.path());
    g_newsgroup_dir = ".";
    g_art_line_num = ArticleLine{};
    g_top_line = ArticleLine{};
    g_in_header = PAST_HEADER;
    g_do_fseek = false;
    g_highlight = ArticleLine{-1};
    g_inner_search = ArticlePosition{};
    g_inner_light = ArticleLine{};
    g_hide_everything = 0;
    g_page_stop.clear();
    g_check_count = 0;
    g_erase_char = '\b';
    g_curr_art = g_art;
    g_tc_LINES = 10;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_erase_screen = false;
    g_erase_each_line = false;
    g_marking = NO_MARKING;
    g_marking_areas = NONE;
    g_tc_CD = "<clear>";
    g_tc_HO = "<home>";
    g_tc_UP = "<up>";
    g_tc_SO = "<mark>";
    g_tc_SE = "</mark>";
    g_tc_IL = "";
    set_mode(GM_READ, MM_ARTICLE);
    while (macro_pending())
    {
        (void) read_tty_char();
    }
}

void PagerArticleTest::load_body(std::string_view body, std::string_view extra_headers)
{
    load_article(std::string{"Subject: pager advancement\n"} + std::string{extra_headers}, body);
}

void PagerArticleTest::load_article(std::string_view headers, std::string_view body)
{
    art_close();
    {
        std::ofstream article{std::to_string(g_art.value_of())};
        article << headers << '\n';
        m_body_start = ArticlePosition{static_cast<long>(article.tellp())};
        article << body;
        ASSERT_TRUE(article.good());
    }
    g_parsed_art = ArticleNum{};
    ASSERT_TRUE(parse_header(g_art));
    mime_set_article();
    clear_art_buf();
    set_art_buf_seek(m_body_start);
    ASSERT_EQ(0, seek_art(m_body_start));
    g_art_pos = m_body_start;
    virtual_write(ArticleLine{}, m_body_start);
}

void PagerArticleTest::TearDown()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    back_page_final();
    if (!m_previous_directory.empty())
    {
        fs::current_path(m_previous_directory);
    }
    ArticleHeaderPresentationTest::TearDown();
}

void PagerArticleTest::read_pages(std::string_view commands)
{
    push_char('q');
    for (auto character = commands.rbegin(); character != commands.rend(); ++character)
    {
        push_char(*character);
    }
    render_article();
}

std::string_view PagerArticleTest::page_text(std::size_t page) const
{
    std::string_view text{m_rendered};
    for (std::size_t index = 0; index < page; ++index)
    {
        const std::size_t prompt = text.find("--MORE--(");
        EXPECT_NE(std::string_view::npos, prompt);
        if (prompt == std::string_view::npos)
        {
            return {};
        }
        const std::size_t end = text.find(')', prompt);
        EXPECT_NE(std::string_view::npos, end);
        if (end == std::string_view::npos)
        {
            return {};
        }
        text.remove_prefix(end + 1);
    }
    return text.substr(0, text.find("--MORE--("));
}

class PagerMovementArticleTest : public PagerArticleTest, public WithParamInterface<PagerMovementCase>
{
protected:
    void              SetUp() override;
    void              read_pages();
    std::vector<long> page_lines(std::size_t page, bool highlighted_only = false) const;
};

void PagerMovementArticleTest::SetUp()
{
    PagerArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerMovementCase &scenario = GetParam();
    std::ostringstream       body;
    for (long line = 0; line < scenario.body_lines; ++line)
    {
        if (line == scenario.page_break_line)
        {
            body << '\f';
        }
        for (const PagerBodyPrefix &prefix : scenario.line_prefixes)
        {
            if (prefix.first == line)
            {
                body << prefix.second;
            }
        }
        body << "Body " << std::setw(3) << std::setfill('0') << line << '\n';
    }
    load_body(body.str());
    g_g_line = scenario.search_context;
    g_tc_LINES = scenario.screen_lines;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_erase_screen = scenario.erase_screen;
    g_erase_each_line = scenario.erase_each_line;
    g_marking = scenario.marking;
    g_marking_areas = scenario.marking_areas;
    g_tc_HO = scenario.can_home ? "<home>" : "";
    g_tc_IL = scenario.can_insert_line ? "<insert>" : "";
}

void PagerMovementArticleTest::read_pages()
{
    PagerArticleTest::read_pages(GetParam().commands);
}

std::vector<long> PagerMovementArticleTest::page_lines(std::size_t page, bool highlighted_only) const
{
    const std::string_view          text = page_text(page);
    std::vector<long> lines;
    const std::vector<std::size_t> &rotated = GetParam().rotated_pages;
    const std::string_view          prefix =
        std::find(rotated.begin(), rotated.end(), page) == rotated.end() ? "Body " : "Obql ";
    for (std::size_t position = text.find(prefix); position != std::string_view::npos;
         position = text.find(prefix, position + prefix.size()))
    {
        const std::size_t line_start = text.rfind('\n', position);
        const std::size_t start = line_start == std::string_view::npos ? 0 : line_start + 1;
        if (!highlighted_only || text.substr(start, position - start).find("<mark>") != std::string_view::npos)
        {
            lines.push_back(std::stol(std::string{text.substr(position + prefix.size(), 3)}));
        }
    }
    return lines;
}

class PagerTextArticleTest : public PagerArticleTest
{
protected:
    void                     SetUp() override;
    std::vector<std::string> displayed_rows(std::size_t page) const;

private:
    ValueSaver<int>              m_wrap_offset{g_word_wrap_offset, g_word_wrap_offset};
    ValueSaver<bool>             m_auto_margin{g_tc_AM, g_tc_AM};
    ValueSaver<bool>             m_newline_glitch{g_tc_XN, g_tc_XN};
    ValueSaver<std::string_view> m_clear_line{g_tc_CE, g_tc_CE};
};

void PagerTextArticleTest::SetUp()
{
    PagerArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    g_word_wrap_offset = -1;
    g_tc_AM = false;
    g_tc_XN = false;
    g_tc_CE = "<eol>";
    utf_init("utf-8", "utf-8");
}

enum class PagerTerminalMode
{
    NORMAL,
    HARDWARE_TABS,
    CRLF,
    UNFILTERED,
    UNDERCHAR,
    UNDERLINE_GLITCH
};

struct PagerTerminalCase
{
    const char                           *name;
    std::string_view                      text;
    std::vector<std::vector<std::string>> pages;
    std::vector<long>                     offsets;
    PagerTerminalMode                     mode{PagerTerminalMode::NORMAL};
    std::string_view                      commands{" "};
    int                                   screen_lines{4};
    std::vector<std::size_t>              stop_rows;
    std::string_view                      tail{"Tail\nEnd\nLast\nMore\nDone\n"};
};

void PrintTo(const PagerTerminalCase &value, std::ostream *output)
{
    *output << value.name;
}

class PagerTerminalArticleTest : public PagerTextArticleTest, public WithParamInterface<PagerTerminalCase>
{
protected:
    void SetUp() override;

    std::string m_source;

private:
    ValueSaver<bool>             m_hiding{g_do_hiding, g_do_hiding};
    ValueSaver<bool>             m_filter{g_dont_filter_control, g_dont_filter_control};
    ValueSaver<bool>             m_tabs{g_tc_GT, g_tc_GT};
    ValueSaver<bool>             m_glitch{g_tc_UG, g_tc_UG};
    ValueSaver<int>              m_fire{g_fire_is_out, g_fire_is_out};
    ValueSaver<std::string_view> m_underline_start{g_tc_US, g_tc_US};
    ValueSaver<std::string_view> m_underline_end{g_tc_UE, g_tc_UE};
    ValueSaver<std::string_view> m_underchar{g_tc_UC, g_tc_UC};
};

void PagerTerminalArticleTest::SetUp()
{
    PagerTextArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerTerminalCase &scenario = GetParam();
    g_tc_COLS = 8;
    g_tc_LINES = scenario.screen_lines;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_do_hiding = true;
    g_dont_filter_control = scenario.mode == PagerTerminalMode::UNFILTERED;
    g_tc_GT = scenario.mode == PagerTerminalMode::HARDWARE_TABS;
    g_tc_UG = scenario.mode == PagerTerminalMode::UNDERLINE_GLITCH;
    g_tc_US = scenario.mode == PagerTerminalMode::UNDERCHAR ? "" : "<ul>";
    g_tc_UE = scenario.mode == PagerTerminalMode::UNDERCHAR ? "" : "</ul>";
    g_tc_UC = scenario.mode == PagerTerminalMode::UNDERCHAR ? "<uc>" : "";
    std::string body = "One\nTwo\n" + std::string{scenario.text} + std::string{scenario.tail};
    std::string headers;
    if (scenario.mode == PagerTerminalMode::CRLF)
    {
        // Decode CRLF in the message body on text-mode and binary-mode hosts.
        headers = "MIME-Version: 1.0\nContent-Type: text/plain\nContent-Transfer-Encoding: quoted-printable\n";
        for (std::size_t position = body.find('\r'); position != std::string::npos;
             position = body.find('\r', position + 3))
        {
            body.replace(position, 1, "=0D");
        }
    }
    load_body(body, headers);
    std::ifstream source{std::to_string(g_art.value_of()), std::ios::binary};
    ASSERT_TRUE(source.is_open());
    m_source.assign(std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{});
}

struct PagerWrappingCase
{
    const char              *name;
    std::string_view         body;
    std::vector<std::string> rows;
    int                      columns;
    int                      wrap_offset{-1};
};

void PrintTo(const PagerWrappingCase &value, std::ostream *output)
{
    *output << value.name;
}

using PagerWrappingParam = std::tuple<PagerWrappingCase, bool>;

class PagerWrappingArticleTest : public PagerTextArticleTest, public WithParamInterface<PagerWrappingParam>
{
protected:
    void SetUp() override;
};

void PagerWrappingArticleTest::SetUp()
{
    PagerTextArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerWrappingCase &scenario = std::get<0>(GetParam());
    g_tc_LINES = 4;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_tc_COLS = scenario.columns;
    g_word_wrap_offset = scenario.wrap_offset;
    g_erase_screen = std::get<1>(GetParam());
    load_body(scenario.body, "Content-Type: text/plain; charset=UTF-8\n");
}

std::vector<std::string> PagerTextArticleTest::displayed_rows(std::size_t page) const
{
    std::string text{page_text(page)};
    for (const std::string_view marker : {"<clear>", "<home>", "<up>", "<eol>", "<mark>", "</mark>"})
    {
        for (std::size_t position = text.find(marker); position != std::string::npos; position = text.find(marker))
        {
            text.erase(position, marker.size());
        }
    }
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    text.erase(std::remove(text.begin(), text.end(), '\b'), text.end());
    const std::size_t start = text.find_first_not_of(' ');
    text.erase(0, start == std::string::npos ? text.size() : start);
    std::istringstream       input{text};
    std::vector<std::string> rows;
    for (std::string row; std::getline(input, row);)
    {
        if (row.find_first_not_of(' ') != std::string::npos)
        {
            rows.push_back(std::move(row));
        }
    }
    return rows;
}

class PagerFilteringArticleTest : public PagerTextArticleTest
{
protected:
    void            SetUp() override;
    void            set_filters(std::string_view hidden, std::string_view stop = {});
    void            load_filtered_body(std::string body, std::string_view headers = {});
    ArticlePosition position_of(std::string_view text) const;
    void            expect_position(long displayed_lines, std::string_view next_line);

    std::string m_body;

private:
    std::string                 m_raw_article;
    ValueSaver<bool>            m_hiding{g_do_hiding, g_do_hiding};
    ValueSaver<std::string>     m_hidden{g_hide_line, g_hide_line};
    ValueSaver<std::string>     m_first_line{g_first_line, g_first_line};
    ValueSaver<CompiledRegex>   m_hide_regex{g_hide_compex, g_hide_compex};
    ValueSaver<CompiledRegex>   m_stop_regex{g_page_compex, g_page_compex};
    ValueSaver<HeaderTypeFlags> m_subject_flags{g_header_type[SUBJ_LINE].flags, g_header_type[SUBJ_LINE].flags};
};

void PagerFilteringArticleTest::SetUp()
{
    PagerTextArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    g_do_hiding = true;
    g_hide_line.clear();
    g_first_line = "Pager title";
    g_header_type[SUBJ_LINE].flags &= ~HT_MAGIC;
    g_tc_LINES = 5;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_tc_COLS = 80;
}

void PagerFilteringArticleTest::set_filters(std::string_view hidden, std::string_view stop)
{
    g_hide_line = hidden;
    g_page_stop = stop;
    if (!hidden.empty())
    {
        ASSERT_TRUE(g_hide_compex.compile(hidden, true, true).empty());
    }
    if (!stop.empty())
    {
        ASSERT_TRUE(g_page_compex.compile(stop, true, true).empty());
    }
}

void PagerFilteringArticleTest::load_filtered_body(std::string body, std::string_view headers)
{
    m_body = std::move(body);
    load_body(m_body, headers);
    std::ifstream source{std::to_string(g_art.value_of()), std::ios::binary};
    ASSERT_TRUE(source.is_open());
    std::ostringstream raw;
    raw << source.rdbuf();
    m_raw_article = raw.str();
}

ArticlePosition PagerFilteringArticleTest::position_of(std::string_view text) const
{
    const std::string &source = g_do_hiding ? m_body : m_raw_article;
    std::size_t        offset = source.find(std::string{text} + (g_do_hiding ? "\n" : "\r\n"));
    if (offset == std::string::npos)
    {
        offset = source.find(text);
    }
    EXPECT_NE(std::string::npos, offset);
    return (g_do_hiding ? m_body_start : ArticlePosition{}) + ArticlePosition{static_cast<long>(offset)};
}

void PagerFilteringArticleTest::expect_position(long displayed_lines, std::string_view next_line)
{
    EXPECT_EQ(position_of(next_line), g_art_pos);
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_EQ(ArticleLine{displayed_lines}, g_art_line_num);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
}

std::string numbered_pager_body(long first = 0, long end = 12)
{
    std::ostringstream body;
    for (long line = first; line < end; ++line)
    {
        body << "Body " << std::setw(3) << std::setfill('0') << line << '\n';
    }
    return body.str();
}

struct PagerEndCase
{
    const char                           *name;
    std::string                           body;
    std::vector<std::vector<std::string>> pages;
    std::string_view                      hidden;
    std::string_view                      headers;
    int                                   columns{80};
    int                                   wrap_offset{-1};
    bool                                  hiding{true};
};

void PrintTo(const PagerEndCase &value, std::ostream *output)
{
    *output << value.name;
}

std::vector<std::string> numbered_pager_rows(long first, long end)
{
    std::istringstream       body{numbered_pager_body(first, end)};
    std::vector<std::string> rows;
    for (std::string row; std::getline(body, row);)
    {
        rows.push_back(std::move(row));
    }
    return rows;
}

using PagerEndParam = std::tuple<PagerEndCase, bool>;

class PagerEndArticleTest : public PagerFilteringArticleTest, public WithParamInterface<PagerEndParam>
{
protected:
    void SetUp() override;
    void read_to_end();

    std::vector<std::vector<std::string>> m_pages;
};

void PagerEndArticleTest::SetUp()
{
    PagerFilteringArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerEndCase &scenario = std::get<0>(GetParam());
    g_erase_screen = std::get<1>(GetParam());
    g_tc_COLS = scenario.columns;
    g_word_wrap_offset = scenario.wrap_offset;
    g_do_hiding = scenario.hiding;
    set_filters(scenario.hidden);
    load_filtered_body(scenario.body, scenario.headers);
    m_pages = scenario.pages;
    if (g_erase_screen)
    {
        for (std::size_t page = 1; page < m_pages.size(); ++page)
        {
            m_pages[page].insert(m_pages[page].begin(), m_pages[page - 1].back());
        }
    }
}

void PagerEndArticleTest::read_to_end()
{
    read_pages(std::string(m_pages.size() - 1, ' '));
}

struct PagerHiddenHeaderCase
{
    const char      *name;
    std::string_view header;
    HeaderLineType   type;
    long             lines;
};

void PrintTo(const PagerHiddenHeaderCase &value, std::ostream *output)
{
    *output << value.name;
}

using PagerHiddenHeaderParam = std::tuple<PagerHiddenHeaderCase, bool>;

class PagerHiddenHeaderArticleTest : public PagerFilteringArticleTest, public WithParamInterface<PagerHiddenHeaderParam>
{
protected:
    void SetUp() override;

    std::string m_headers;

private:
    ValueSaver<HeaderTypeFlags> m_from_flags{g_header_type[FROM_LINE].flags, g_header_type[FROM_LINE].flags};
    ValueSaver<HeaderTypeFlags> m_path_flags{g_header_type[PATH_LINE].flags, g_header_type[PATH_LINE].flags};
    ValueSaver<HeaderTypeFlags> m_unknown_flags{g_header_type[SOME_LINE].flags, g_header_type[SOME_LINE].flags};
};

void PagerHiddenHeaderArticleTest::SetUp()
{
    PagerFilteringArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerHiddenHeaderCase &scenario = std::get<0>(GetParam());
    g_header_type[scenario.type].flags |= HT_HIDE;
    g_header_type[scenario.type].flags &= ~HT_MAGIC;
    g_do_hiding = std::get<1>(GetParam());
    g_tc_LINES = 8;
    g_init_lines = ArticleLine{g_tc_LINES};
    load_filtered_body(numbered_pager_body(), scenario.header);
    m_headers = g_head_buf;
    g_top_line = ArticleLine{-1};
}

struct PagerFormattedHeaderCase
{
    const char              *name;
    HeaderLineType           type;
    std::string_view         header;
    std::vector<std::string> formatted;
    std::vector<std::string> raw;
    bool                     magic{true};
    bool                     known_date{true};
    int                      columns{80};
    std::size_t              first_rows{1};
};

void PrintTo(const PagerFormattedHeaderCase &value, std::ostream *output)
{
    *output << value.name;
}

using PagerFormattedHeaderParam = std::tuple<PagerFormattedHeaderCase, bool, bool>;

class PagerFormattedHeaderArticleTest : public PagerFilteringArticleTest,
                                        public WithParamInterface<PagerFormattedHeaderParam>
{
protected:
    void SetUp() override;
    void read_headers();

    std::vector<std::vector<std::string>>                              m_pages;
    std::string                                                        m_headers;
    std::string                                                        m_subject;
    std::string                                                        m_from;
    std::string                                                        m_source;
    std::array<std::pair<ArticlePosition, ArticlePosition>, HEAD_LAST> m_header_spans;
    long                                                               m_next_body_row{};
    long                                                               m_header_rows{};

private:
    ValueSaver<HeaderTypeFlags>  m_from_flags{g_header_type[FROM_LINE].flags, g_header_type[FROM_LINE].flags};
    ValueSaver<HeaderTypeFlags>  m_date_flags{g_header_type[DATE_LINE].flags, g_header_type[DATE_LINE].flags};
    ValueSaver<HeaderTypeFlags>  m_group_flags{g_header_type[NEWSGROUPS_LINE].flags,
                                               g_header_type[NEWSGROUPS_LINE].flags};
    ValueSaver<HeaderTypeFlags>  m_unknown_flags{g_header_type[SOME_LINE].flags, g_header_type[SOME_LINE].flags};
    ValueSaver<std::string>      m_group_name{g_newsgroup_name, g_newsgroup_name};
    ValueSaver<bool>             m_threaded{g_threaded_group, g_threaded_group};
    ValueSaver<std::string_view> m_underline_start{g_tc_US, g_tc_US};
    ValueSaver<std::string_view> m_underline_end{g_tc_UE, g_tc_UE};
    ValueSaver<std::string_view> m_underchar{g_tc_UC, g_tc_UC};
};

void PagerFormattedHeaderArticleTest::SetUp()
{
    PagerFilteringArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerFormattedHeaderCase &scenario = std::get<0>(GetParam());
    g_do_hiding = std::get<1>(GetParam());
    g_tc_LINES = 6;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_tc_COLS = scenario.columns;
    g_newsgroup_name = "list.current";
    g_threaded_group = false;
    g_tc_US = "<mark>";
    g_tc_UE = "</mark>";
    g_tc_UC = "";
    g_header_type[SOME_LINE].flags &= ~(HT_HIDE | HT_MAGIC);
    g_header_type[scenario.type].flags &= ~(HT_HIDE | HT_MAGIC);
    if (scenario.magic)
    {
        g_header_type[scenario.type].flags |= HT_MAGIC;
    }
    m_env.expect_env("LOCALTIMEFMT", "local date");
    std::string              headers;
    std::vector<std::string> prefix{"Pager title"};
    if (scenario.type != SUBJ_LINE)
    {
        headers = "Subject: Header pagination\n";
        prefix.emplace_back("Subject: Header pagination");
    }
    const long padding = scenario.type == SUBJ_LINE ? 3 : 2;
    for (long row = 0; row < padding; ++row)
    {
        const std::string line = "X-Pad: " + std::to_string(row);
        headers += line + '\n';
        prefix.push_back(line);
    }
    headers += scenario.header;
    load_article(headers, numbered_pager_body());
    // The overview cache describes the same private article as its header.
    g_artp->m_flags &= ~AF_HAS_RE;
    g_artp->set_subj_line(fetch_lines(g_art, SUBJ_LINE));
    g_artp->set_cached_line(FROM_LINE, fetch_lines(g_art, FROM_LINE));
    g_artp->m_date = scenario.known_date ? 1070514367 : -1;
    g_top_line = ArticleLine{-1};
    m_headers = g_head_buf;
    m_subject = g_artp->get_cached_line_view(SUBJ_LINE, false);
    m_from = g_artp->get_cached_line_view(FROM_LINE, false);
    for (std::size_t i = 0; i < m_header_spans.size(); ++i)
    {
        m_header_spans[i] = {g_header_type[i].min_pos, g_header_type[i].max_pos};
    }
    std::ifstream source{std::to_string(g_art.value_of()), std::ios::binary};
    ASSERT_TRUE(source.is_open());
    m_source.assign(std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{});
    const std::vector<std::string> &shown = g_do_hiding ? scenario.formatted : scenario.raw;
    const std::size_t               first = std::min(scenario.first_rows, shown.size());
    prefix.insert(prefix.end(), shown.begin(), shown.begin() + first);
    std::vector<std::string> remaining{shown.begin() + first, shown.end()};
    m_next_body_row = shown.empty() ? 4 : 3 - static_cast<long>(remaining.size());
    const std::vector<std::string> body = numbered_pager_rows(0, m_next_body_row);
    remaining.insert(remaining.end(), body.begin(), body.end());
    m_header_rows = 4 + static_cast<long>(shown.size());
    m_pages = {std::move(prefix)};
    if (std::get<2>(GetParam()))
    {
        m_pages.push_back(m_pages.front());
    }
    m_pages.push_back(std::move(remaining));
}

void PagerFormattedHeaderArticleTest::read_headers()
{
    read_pages(std::get<2>(GetParam()) ? "\f " : " ");
}

struct PagerFilteredBodyCase
{
    const char                           *name;
    std::string                           body;
    std::string_view                      hidden;
    std::string_view                      stop;
    std::string_view                      commands;
    std::vector<std::vector<std::string>> pages;
    std::string_view                      next_line;
    std::vector<long>                     stop_rows;
    bool                                  word_wrap{};
    bool                                  hiding{true};
};

void PrintTo(const PagerFilteredBodyCase &value, std::ostream *output)
{
    *output << value.name;
}

class PagerFilteredBodyArticleTest : public PagerFilteringArticleTest, public WithParamInterface<PagerFilteredBodyCase>
{
protected:
    void SetUp() override;
};

void PagerFilteredBodyArticleTest::SetUp()
{
    PagerFilteringArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerFilteredBodyCase &scenario = GetParam();
    g_do_hiding = scenario.hiding;
    if (scenario.word_wrap)
    {
        g_tc_COLS = 32;
        g_word_wrap_offset = 8;
    }
    set_filters(scenario.hidden, scenario.stop);
    load_filtered_body(scenario.body);
}

enum class PagerResumeText
{
    UNBROKEN,
    WORDS,
    UTF8
};

struct PagerResumePage
{
    long first;
    long count;
    bool headers{};
};

struct PagerResumeCase
{
    const char                  *name;
    std::string_view             commands;
    std::vector<PagerResumePage> pages;
    long                         next_row;
    bool                         starts_in_header{};
    bool                         erase_screen{};
};

void PrintTo(const PagerResumeCase &value, std::ostream *output)
{
    *output << value.name;
}

using PagerResumeParam = std::tuple<PagerResumeCase, PagerResumeText>;

class PagerResumeArticleTest : public PagerTextArticleTest, public WithParamInterface<PagerResumeParam>
{
protected:
    void SetUp() override;

    std::vector<std::string> m_rows;
    std::vector<long>        m_offsets;

private:
    ValueSaver<std::string> m_first_line{g_first_line, g_first_line};
};

void PagerResumeArticleTest::SetUp()
{
    PagerTextArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerResumeCase &scenario = std::get<0>(GetParam());
    const PagerResumeText  text = std::get<1>(GetParam());
    g_tc_LINES = 6;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_tc_COLS = 32;
    g_erase_screen = scenario.erase_screen;
    g_first_line = "Pager title";
    std::string_view suffix;
    switch (text)
    {
    case PagerResumeText::UNBROKEN:
        suffix = "abcdefghijklmnopqrstuvw";
        break;

    case PagerResumeText::WORDS:
        suffix = "alpha beta end";
        g_word_wrap_offset = 8;
        break;

    case PagerResumeText::UTF8:
        suffix = "abcdefghijklmnopqrs\345\244\251e\314\201f";
        break;
    }
    std::string body;
    for (long row = 0; row < 18; ++row)
    {
        std::ostringstream label;
        label << "Body " << std::setw(3) << std::setfill('0') << row << ' ' << suffix;
        const std::string line = label.str();
        m_offsets.push_back(static_cast<long>(body.size()));
        m_rows.push_back(line);
        body += line;
        if (text == PagerResumeText::WORDS && row != 17)
        {
            body += ' ';
        }
    }
    body += "\nEnd\n";
    load_body(body, "Content-Type: text/plain; charset=UTF-8\n");
    if (scenario.starts_in_header)
    {
        g_top_line = ArticleLine{-1};
    }
}

struct PagerSearchPage
{
    long first;
    long count;
    long highlighted{-1};
};

struct PagerSearchCase
{
    const char                  *name;
    std::string_view             commands;
    std::vector<PagerSearchPage> pages;
    long                         next_row;
};

void PrintTo(const PagerSearchCase &value, std::ostream *output)
{
    *output << value.name;
}

struct PagerSearchText
{
    const char      *name;
    std::string_view suffix;
    std::string_view separator;
    int              wrap_offset{-1};
};

void PrintTo(const PagerSearchText &value, std::ostream *output)
{
    *output << value.name;
}

using PagerSearchParam = std::tuple<PagerSearchCase, PagerSearchText, MarkingMode>;

std::string pager_search_name(const TestParamInfo<PagerSearchParam> &info)
{
    return std::string{std::get<0>(info.param).name} + std::get<1>(info.param).name +
           (std::get<2>(info.param) == STANDOUT ? "Standout" : "Underline");
}

class PagerSearchArticleTest : public PagerTextArticleTest, public WithParamInterface<PagerSearchParam>
{
protected:
    void                     SetUp() override;
    std::vector<std::string> highlighted_rows(std::size_t page) const;

    std::vector<std::string> m_rows;
    std::vector<long>        m_offsets;

private:
    ValueSaver<std::string_view> m_underline_start{g_tc_US, "<mark>"};
    ValueSaver<std::string_view> m_underline_end{g_tc_UE, "</mark>"};
    ValueSaver<std::string_view> m_underchar{g_tc_UC, ""};
};

void PagerSearchArticleTest::SetUp()
{
    PagerTextArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerSearchText &text = std::get<1>(GetParam());
    g_tc_LINES = 6;
    g_init_lines = ArticleLine{g_tc_LINES};
    g_tc_COLS = 32;
    g_g_line = 1;
    g_marking = std::get<2>(GetParam());
    g_tc_US = "<mark>";
    g_tc_UE = "</mark>";
    g_tc_UC = "";
    g_word_wrap_offset = text.wrap_offset;
    std::string body;
    for (long row = 0; row < 30; ++row)
    {
        std::ostringstream label;
        label << "Body " << std::setw(3) << std::setfill('0') << row << ' ' << text.suffix;
        m_offsets.push_back(static_cast<long>(body.size()));
        m_rows.push_back(label.str());
        body += m_rows.back();
        body += text.separator;
    }
    body += "\nEnd\n";
    load_body(body, "Content-Type: text/plain; charset=UTF-8\n");
}

std::vector<std::string> PagerSearchArticleTest::highlighted_rows(std::size_t page) const
{
    const std::string_view   text = page_text(page);
    std::vector<std::string> rows;
    for (std::size_t start = text.find("<mark>"); start != std::string_view::npos; start = text.find("<mark>", start))
    {
        start += 6;
        const std::size_t end = text.find("</mark>", start);
        if (end == std::string_view::npos)
        {
            break;
        }
        std::string row{text.substr(start, end - start)};
        row.erase(std::remove(row.begin(), row.end(), '\r'), row.end());
        row.erase(std::remove(row.begin(), row.end(), '\n'), row.end());
        rows.push_back(std::move(row));
        start = end + 7;
    }
    return rows;
}

struct PagerExitCase
{
    const char     *name;
    std::string     input;
    std::string     command;
    ArticleFlags    initial_flags{AF_UNREAD};
    ArticleFlags    expected_flags{AF_UNREAD};
    int             checkpoints{};
    DoArticleResult result{DA_RAISE};
    bool            reread{};
    const char     *pending{"q"};

    std::vector<PagerResumePage> pages;
    long                         next_row{4};
    std::string_view             message;
};

void PrintTo(const PagerExitCase &value, std::ostream *output)
{
    *output << value.name;
}

PagerExitCase raised_pager_command(const char *name, const char *input, bool mark_read = false)
{
    return {name,
            input,
            std::string{input[0], static_cast<char>(FINISH_CMD)},
            AF_UNREAD,
            mark_read ? AF_NONE : AF_UNREAD,
            mark_read ? 1 : 0};
}

PagerExitCase after_pager_interaction(PagerExitCase scenario, std::string_view commands,
                                      std::vector<PagerResumePage> pages, long next_row, std::string_view message = {})
{
    scenario.input = std::string{commands} + scenario.input;
    scenario.pages = std::move(pages);
    scenario.next_row = next_row;
    scenario.message = message;
    return scenario;
}

class PagerExitArticleTest : public PagerTextArticleTest, public WithParamInterface<PagerExitCase>
{
protected:
    void        SetUp() override;
    void        TearDown() override;
    std::string pending_input();

    ArticleUnread m_unread_before{};

private:
    ValueSaver<ArticleNum>    m_current_article{g_curr_art, g_curr_art};
    ValueSaver<char>          m_erase_char{g_erase_char, g_erase_char};
    ValueSaver<char>          m_kill_char{g_kill_char, g_kill_char};
    ValueSaver<bool>          m_reread{g_reread, g_reread};
    ValueSaver<int>           m_checkpoints{g_check_count, g_check_count};
    ValueSaver<ArticleUnread> m_selected{g_selected_count, g_selected_count};
    ValueSaver<AddGroupFlags> m_selection_mask{g_sel_mask, g_sel_mask};
    ValueSaver<GeneralMode>   m_general_mode{g_general_mode, g_general_mode};
    ValueSaver<MinorMode>     m_mode{g_mode, g_mode};
};

void PagerExitArticleTest::SetUp()
{
    PagerTextArticleTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    const PagerExitCase &scenario = GetParam();
    if (scenario.pages.empty())
    {
        g_top_line = ArticleLine{-1};
        g_init_lines = ArticleLine{2};
        g_tc_LINES = 4;
    }
    else
    {
        load_body(numbered_pager_body(0, 12));
        g_tc_LINES = 5;
        g_init_lines = ArticleLine{g_tc_LINES};
    }
    if (scenario.initial_flags & AF_UNREAD)
    {
        g_artp->one_more();
    }
    else
    {
        g_artp->one_less();
    }
    g_artp->m_flags = (g_artp->m_flags & ~AF_SEL) | (scenario.initial_flags & AF_SEL);
    g_sel_mask = static_cast<AddGroupFlags>(AF_SEL);
    g_selected_count = ArticleUnread{(scenario.initial_flags & AF_SEL) ? 1 : 0};
    m_unread_before = g_newsgroup_ptr->m_to_read;
    g_curr_art = g_art;
    g_erase_char = '\b';
    g_kill_char = '\025';
    g_check_count = 0;
    g_reread = true;
    m_article_command = "stale command";
    set_mode(GM_READ, MM_ARTICLE);
    (void) pending_input();
    push_char('q');
    const std::string_view input{scenario.input};
    for (auto character = input.rbegin(); character != input.rend(); ++character)
    {
        push_char(*character);
    }
}

void PagerExitArticleTest::TearDown()
{
    (void) pending_input();
    PagerTextArticleTest::TearDown();
}

std::string PagerExitArticleTest::pending_input()
{
    std::string input;
    while (macro_pending())
    {
        input.push_back(static_cast<char>(read_tty_char()));
    }
    return input;
}

struct PagerTreeCase
{
    const char *name;
    bool        threaded;
    bool        quoted_printable;
    const char *commands;
    long        next_line;
    int         displays{1};
};

void PrintTo(const PagerTreeCase &value, std::ostream *output)
{
    *output << value.name;
}

class PagerTreeArticleTest : public ArticleHeaderPresentationTest, public WithParamInterface<PagerTreeCase>
{
protected:
    void              SetUp() override;
    void              TearDown() override;
    void              read_with_tree();
    std::vector<long> displayed_body_lines() const;

    ArticlePosition              m_body_start{};
    std::vector<ArticlePosition> m_raw_positions;

private:
    PushDir                      m_private_group;
    ValueSaver<std::string>      m_group_dir{g_newsgroup_dir, g_newsgroup_dir};
    ValueSaver<ArticleNum>       m_current_article{g_curr_art, g_curr_art};
    ValueSaver<ArticleNum>       m_abs_first{g_abs_first, g_abs_first};
    ValueSaver<bool>             m_threaded{g_threaded_group, g_threaded_group};
    ValueSaver<bool>             m_thread_always{g_thread_always, g_thread_always};
    ValueSaver<int>              m_tree_lines{g_max_tree_lines, g_max_tree_lines};
    ValueSaver<GeneralMode>      m_general_mode{g_general_mode, g_general_mode};
    ValueSaver<MinorMode>        m_mode{g_mode, g_mode};
    ValueSaver<std::string_view> m_standout{g_tc_SO, g_tc_SO};
    ValueSaver<std::string_view> m_end_standout{g_tc_SE, g_tc_SE};
};

void PagerTreeArticleTest::SetUp()
{
    ArticleHeaderPresentationTest::SetUp();
    ASSERT_FALSE(HasFatalFailure());
    art_close();
    close_cache();
    m_private_group.push(m_output.path());
    g_newsgroup_dir = ".";
    g_data_source->m_flags &= ~DF_TRY_OVERVIEW;
    g_abs_first = g_art;
    g_last_art = article_after(g_art);
    g_newsgroup_ptr->m_abs_first = g_abs_first;
    g_newsgroup_ptr->m_ng_max = g_last_art;
    g_newsgroup_ptr->m_rc_line.resize(g_newsgroup_ptr->m_num_offset);
    g_newsgroup_ptr->m_rc_line += "1-" + std::to_string(article_before(g_abs_first).value_of());
    g_threaded_group = GetParam().threaded;
    g_thread_always = true;
    g_max_tree_lines = 0;
    {
        std::ofstream article{std::to_string(g_art.value_of())};
        article << "Subject: Pager thread\nMessage-ID: <pager-root@example.test>\n";
        if (GetParam().quoted_printable)
        {
            article << "MIME-Version: 1.0\nContent-Type: text/plain; charset=utf-8\n"
                       "Content-Transfer-Encoding: quoted-printable\n";
        }
        article << '\n';
        m_body_start = ArticlePosition{static_cast<long>(article.tellp())};
        m_raw_positions.push_back(m_body_start);
        for (long line = 0; line < 40; ++line)
        {
            article << (GetParam().quoted_printable ? "=42ody " : "Body ") << std::setw(3) << std::setfill('0') << line
                    << '\n';
            m_raw_positions.emplace_back(static_cast<long>(article.tellp()));
        }
        ASSERT_TRUE(article.good());
        std::ofstream reply{std::to_string(g_last_art.value_of())};
        reply << "Subject: Re: Pager thread\nMessage-ID: <pager-reply@example.test>\n"
                 "References: <pager-root@example.test>\n\nOther article\n";
        ASSERT_TRUE(reply.good());
    }
    build_cache();
    g_artp = g_curr_artp = article_ptr(g_art);
    g_curr_art = g_art;
    ASSERT_TRUE(parse_header(g_art));
    ASSERT_TRUE(g_artp->m_flags & AF_UNREAD);
    ASSERT_TRUE(article_ptr(g_last_art)->m_flags & AF_UNREAD);
    ASSERT_EQ(ArticleUnread{2}, g_newsgroup_ptr->m_to_read);
    mime_set_article();
    clear_art_buf();
    set_art_buf_seek(m_body_start);
    ASSERT_EQ(0, seek_art(m_body_start));
    g_art_pos = m_body_start;
    g_art_line_num = ArticleLine{};
    g_top_line = ArticleLine{};
    g_in_header = PAST_HEADER;
    g_do_fseek = false;
    g_tc_LINES = 10;
    g_init_lines = ArticleLine{10};
    g_tc_SO = "<mark>";
    g_tc_SE = "</mark>";
    virtual_write(ArticleLine{}, m_body_start);
    set_mode(GM_READ, MM_ARTICLE);
    while (macro_pending())
    {
        (void) read_tty_char();
    }
}

void PagerTreeArticleTest::TearDown()
{
    while (macro_pending())
    {
        (void) read_tty_char();
    }
    art_close();
    close_cache();
    back_page_final();
    m_private_group.pop();
    ArticleHeaderPresentationTest::TearDown();
}

void PagerTreeArticleTest::read_with_tree()
{
    push_char('q');
    const std::string_view commands{GetParam().commands};
    for (auto character = commands.rbegin(); character != commands.rend(); ++character)
    {
        push_char(*character);
    }
    render_article();
}

std::vector<long> PagerTreeArticleTest::displayed_body_lines() const
{
    std::vector<long> lines;
    for (std::size_t position = m_rendered.find("Body "); position != std::string::npos;
         position = m_rendered.find("Body ", position + 5))
    {
        lines.push_back(std::stol(m_rendered.substr(position + 5, 3)));
    }
    return lines;
}

} // namespace

TEST_F(InterpolatorNewsgroupTest, absoluteNewsgroupDirSet)
{
    g_newsgroup_dir = TRN_TEST_NEWSGROUP_SUBDIR;

    const std::string_view new_pattern = interpolate("%d");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_NEWSGROUP_DIR, buffer());
}

TEST_F(InterpolatorNewsgroupTest, pipeSaveRetainsExpandedDestination)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("PIPESAVER", "");
    const std::string command{"s | pipe destination"};

    const SaveResult result = save_article(command);

    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_EQ("pipe destination", g_save_dest);
}

TEST_F(InterpolatorNewsgroupTest, normalSaveWritesArticleToRelativeDestination)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    ValueSaver<bool>        normal_always(g_norm_always, true);
    m_env.expect_no_envar("SAVENAME");
    m_env.expect_env("SAVEDIR", m_output.path().c_str());
    m_env.expect_no_envar("NORMSAVER");
    const std::string command{"s saved-article"};

    testing::internal::CaptureStdout();
    const SaveResult  result = save_article(command);
    const std::string output = testing::internal::GetCapturedStdout();

    const fs::path saved_article = fs::path{m_output.path()} / "saved-article";
    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_EQ(saved_article.generic_string(), g_save_dest);
    EXPECT_EQ("Saved to file " + saved_article.generic_string(), output);
    EXPECT_THAT(file_contents(saved_article), HasSubstr(TRN_TEST_BODY));
}

#ifdef _WIN32
TEST_F(InterpolatorNewsgroupTest, normalSaveUsesWindowsNormSaverOverride)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    ValueSaver<bool>        normal_always(g_norm_always, true);
    const std::string       saver =
        std::string{"call \"" TRN_TEST_WINDOWS_NORM_SAVER "\""} + " \"%A\" \"%P\" \"%c\" %a %B \"%C\" \"%b\"";
    m_env.expect_no_envar("SAVENAME");
    m_env.expect_env("SAVEDIR", m_output.path().c_str());
    m_env.expect_env("NORMSAVER", saver.c_str());
    m_env.expect_no_envars({"NNTPSERVER", "NNTP_FORCE_AUTH", "NEWSACTIVE", "NEWSDESCRIPTIONS", "QUOTECHARS"});

    testing::internal::CaptureStdout();
    const SaveResult result = save_article("s saved via helper");
    testing::internal::GetCapturedStdout();

    const fs::path saved_article = fs::path{m_output.path()} / "saved via helper";
    EXPECT_EQ(SAVE_DONE, result);
    ASSERT_TRUE(fs::exists(saved_article));
    EXPECT_THAT(file_contents(saved_article), HasSubstr(TRN_TEST_BODY));
}
TEST_F(InterpolatorNewsgroupTest, mailboxSaveUsesWindowsMboxSaverOverride)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    ValueSaver<bool>        mailbox_always(g_mbox_always, true);
    const std::string       saver =
        std::string{"call \"" TRN_TEST_WINDOWS_MBOX_SAVER "\""} +
        " \"%A\" \"%P\" \"%c\" %a %B \"%C\" \"%b\" \"From supplied@example.test Thu Jan 1 00:00:00 1970\"";
    m_env.expect_no_envar("SAVENAME");
    m_env.expect_env("SAVEDIR", m_output.path().c_str());
    m_env.expect_env("MBOXSAVER", saver.c_str());
    m_env.expect_no_envars({"NNTPSERVER", "NNTP_FORCE_AUTH", "NEWSACTIVE", "NEWSDESCRIPTIONS", "QUOTECHARS"});

    testing::internal::CaptureStdout();
    const SaveResult result = save_article("s saved mailbox");
    testing::internal::GetCapturedStdout();

    const fs::path saved_article = fs::path{m_output.path()} / "saved mailbox";
    EXPECT_EQ(SAVE_DONE, result);
    ASSERT_TRUE(fs::exists(saved_article));
    EXPECT_THAT(file_contents(saved_article), HasSubstr(TRN_TEST_BODY));
    const std::string prefix =
        MBOX_CHAR == '\001' ? "\001\001\001\001\n" : "From supplied@example.test Thu Jan 1 00:00:00 1970\n";
    EXPECT_THAT(file_contents(saved_article), testing::StartsWith(prefix));
}
#endif

TEST_F(InterpolatorNewsgroupTest, normalSaveCanRejectMailboxPrompt)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_no_envar("SAVENAME");
    m_env.expect_env("SAVEDIR", m_output.path().c_str());
    m_env.expect_no_envar("NORMSAVER");
    const std::string command{"s prompted-save"};
    push_string("n\n", 0200);

    testing::internal::CaptureStdout();
    const SaveResult  result = save_article(command);
    const std::string output = testing::internal::GetCapturedStdout();

    const fs::path saved_article = fs::path{m_output.path()} / "prompted-save";
    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_THAT(output, HasSubstr("use mailbox format? [ynq] "));
    EXPECT_THAT(output, HasSubstr("Saved to file " + saved_article.generic_string()));
    EXPECT_THAT(file_contents(saved_article), HasSubstr(TRN_TEST_BODY));
}

TEST_F(InterpolatorNewsgroupTest, interactiveSaveAbortRecordsArticleReason)
{
    std::string command{"s"};
    command.push_back(FINISH_CMD);
    push_char('\b');

    testing::internal::CaptureStdout();
    const SaveResult result = save_article(command);
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(SAVE_ABORT, result);
    EXPECT_EQ("Save aborted at article " + std::to_string(TRN_TEST_ARTICLE_NUM), g_msg);
}

TEST_F(InterpolatorNewsgroupTest, performPropagatesInteractiveSaveAbort)
{
    std::string command{"s"};
    command.push_back(FINISH_CMD);
    push_char('\b');

    testing::internal::CaptureStdout();
    const int result = perform(command, 0);
    testing::internal::GetCapturedStdout();

    EXPECT_LT(result, 0);
    EXPECT_EQ("Save aborted at article " + std::to_string(TRN_TEST_ARTICLE_NUM), g_msg);
}

TEST_F(InterpolatorNewsgroupTest, mailboxSaveQuotesFromBodyLine)
{
    const std::string output_path = m_output.path();
    const fs::path    article_file = fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM);
    std::ofstream{article_file} << "Path: " TRN_TEST_HEADER_PATH "\n"
                                << "From: " TRN_TEST_HEADER_FROM "\n"
                                << "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                << "Subject: " TRN_TEST_HEADER_SUBJECT "\n"
                                << "Date: " TRN_TEST_HEADER_DATE "\n"
                                << "Message-Id: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                << "\n"
                                << "From body sender\n"
                                << "plain body\n";
    PushDir                 output_dir{output_path};
    ValueSaver<std::string> private_dir(g_priv_dir, output_path);
    ValueSaver<bool>        mailbox_always(g_mbox_always, true);
    m_env.expect_no_envar("SAVENAME");
    m_env.expect_env("SAVEDIR", output_path.c_str());
    m_env.expect_no_envar("MBOXSAVER");
    const std::string command{"s saved-mailbox"};

    testing::internal::CaptureStdout();
    const SaveResult  result = save_article(command);
    const std::string output = testing::internal::GetCapturedStdout();

    const fs::path saved_article = fs::path{output_path} / "saved-mailbox";
    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_EQ(saved_article.generic_string(), g_save_dest);
    EXPECT_EQ("Saved to mailbox " + saved_article.generic_string(), output);
    EXPECT_THAT(file_contents(saved_article), HasSubstr("\n>From body sender\n"));
}

TEST_F(InterpolatorNewsgroupTest, extractCreatesRelativeDestinationDirectory)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_no_envar("SAVEDIR");
    const std::string command{"e extracted"};

    testing::internal::CaptureStdout();
    const SaveResult  result = save_article(command);
    const std::string output = testing::internal::GetCapturedStdout();

    const fs::path expected = fs::path{m_output.path()} / "extracted";
    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_EQ(expected.generic_string(), g_extract_dest);
    EXPECT_TRUE(fs::is_directory(expected));
    EXPECT_EQ("Unable to determine type of file.\n", output);
}

TEST_F(InterpolatorNewsgroupTest, extractRecognizesYencArticle)
{
    const std::string output_path = m_output.path();
    write_article_body(
        fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM),
        "Introduction\n=ybegin line=128 size=3 name=cat.bin\n\x6D\x8B\x9E\n=yend size=3 crc32=a6130548\n");
    PushDir                 output_dir{output_path};
    ValueSaver<std::string> private_dir(g_priv_dir, output_path);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, ".");
    m_env.expect_no_envar("SAVEDIR");
    g_parsed_art = ArticleNum{};
    art_close();

    testing::internal::CaptureStdout();
    const SaveResult  result = save_article("e extracted");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_THAT(output, HasSubstr("Extracting yEnc file"));
    EXPECT_EQ("Cat", file_contents(fs::path{output_path} / "extracted" / "cat.bin"));
}

TEST_F(InterpolatorNewsgroupTest, extractPreservesAllYencBytes)
{
    const std::string output_path = m_output.path();
    write_article_body(fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM), YENC_ALL_BYTES);
    PushDir                 output_dir{output_path};
    ValueSaver<std::string> private_dir(g_priv_dir, output_path);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, ".");
    m_env.expect_no_envar("SAVEDIR");
    g_parsed_art = ArticleNum{};
    art_close();
    testing::internal::CaptureStdout();
    save_article("e extracted");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_THAT(output, Not(HasSubstr("Invalid")));
    EXPECT_THAT(output, HasSubstr("Extracting yEnc file"));
    std::ifstream     decoded{fs::path{output_path} / "extracted" / "all.bin", std::ios::binary};
    const std::string bytes{std::istreambuf_iterator<char>{decoded}, std::istreambuf_iterator<char>{}};
    ASSERT_EQ(256U, bytes.size()) << output;
    for (int i = 0; i < 256; ++i)
    {
        EXPECT_EQ(i, static_cast<unsigned char>(bytes[i]));
    }
}

TEST_F(InterpolatorNewsgroupTest, extractRecognizesYencInMimeText)
{
    const std::string output_path = m_output.path();
    write_article_body(
        fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM),
        "Introduction\n=ybegin line=128 size=3 name=cat.bin\n\x6D\x8B\x9E\n=yend size=3 crc32=a6130548\n",
        "MIME-Version: 1.0\nContent-Type: text/plain\nContent-Transfer-Encoding: 8bit\n");
    PushDir                 output_dir{output_path};
    ValueSaver<std::string> private_dir(g_priv_dir, output_path);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, ".");
    m_env.expect_no_envar("SAVEDIR");
    g_parsed_art = ArticleNum{};
    art_close();
    testing::internal::CaptureStdout();
    save_article("e extracted");
    testing::internal::GetCapturedStdout();
    EXPECT_EQ("Cat", file_contents(fs::path{output_path} / "extracted" / "cat.bin"));
}

TEST_F(InterpolatorNewsgroupTest, extractKeepsUuencodeBehavior)
{
    const std::string output_path = m_output.path();
    write_article_body(fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM),
                       "begin 644 cat.bin\n#0V%T\n`\nend\n");
    PushDir                 output_dir{output_path};
    ValueSaver<std::string> private_dir(g_priv_dir, output_path);
    ValueSaver<std::string> temp_dir(g_tmp_dir, output_path);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, ".");
    m_env.expect_no_envar("SAVEDIR");
    g_parsed_art = ArticleNum{};
    art_close();
    testing::internal::CaptureStdout();
    save_article("e extracted");
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_THAT(output, HasSubstr("Extracting uuencoded file"));
    EXPECT_EQ("Cat", file_contents(fs::path{output_path} / "extracted" / "cat.bin"));
}

TEST_F(InterpolatorNewsgroupTest, extractKeepsShellArchiveBehavior)
{
    const std::string output_path = m_output.path();
    write_article_body(fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM), "#!/bin/sh\n# archive\n");
    PushDir                 output_dir{output_path};
    ValueSaver<std::string> private_dir(g_priv_dir, output_path);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, ".");
    m_env.expect_no_envar("SAVEDIR");
    m_env.expect_env("SHARSAVER", ":");
    g_parsed_art = ArticleNum{};
    art_close();
    testing::internal::CaptureStdout();
    save_article("e extracted");
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_THAT(output, HasSubstr("Extracting shar"));
}

TEST_F(InterpolatorNewsgroupTest, extractUsesCustomCommand)
{
    ValueSaver<std::string> private_dir(g_priv_dir, m_output.path());
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_no_envar("SAVEDIR");
    m_env.expect_env("CUSTOMSAVER", ":");
    const std::string command{"e custom-extract | custom extractor --flag"};

    testing::internal::CaptureStdout();
    const SaveResult  result = save_article(command);
    const std::string output = testing::internal::GetCapturedStdout();

    const fs::path    expected = fs::path{m_output.path()} / "custom-extract";
    const std::string expected_output =
        "Extracting article into " + expected.generic_string() + " using custom extractor --flag\n";
    EXPECT_EQ(SAVE_DONE, result);
    EXPECT_EQ(expected.generic_string(), g_extract_dest);
    EXPECT_EQ("custom extractor --flag", g_extract_prog);
    EXPECT_TRUE(fs::is_directory(expected));
    EXPECT_EQ(expected_output, output);
}

TEST_F(InterpolatorNewsgroupTest, cancelArticleWritesInterpolatedHeader)
{
    const fs::path          head_file = fs::path{m_output.path()} / "cancel-head";
    const std::string       long_header_value(600, 'x');
    const std::string       cancel_header = "Newsgroups: %n\n"
                                            "Control: cancel %i\n"
                                            "X-Long: " +
                                            long_header_value + "\n\n";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    m_env.expect_env("FROM", TRN_TEST_HEADER_FROM);
    m_env.expect_env("CANCELHEADER", cancel_header.c_str());
    m_env.expect_env("CANCEL", "exit 0");

    testing::internal::CaptureStdout();
    cancel_article();
    const std::string output = testing::internal::GetCapturedStdout();

    const std::string expected_header = "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                        "Control: cancel " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                        "X-Long: " +
                                        long_header_value + "\n\n";
    EXPECT_THAT(output, HasSubstr("Canceling..."));
    EXPECT_EQ(expected_header, file_contents(head_file));
}

TEST_F(InterpolatorNewsgroupTest, supersedeArticleWritesInterpolatedHeaderAndBody)
{
    const fs::path          head_file = fs::path{m_output.path()} / "supersede-head";
    const std::string       long_header_value(600, 'x');
    const std::string       supersede_header = "Newsgroups: %n\n"
                                               "Supersedes: %i\n"
                                               "X-Long: " +
                                               long_header_value + "\n\n";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string> orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string> spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("FROM", TRN_TEST_HEADER_FROM);
    m_env.expect_env("SUPERSEDEHEADER", supersede_header.c_str());
    m_env.expect_env("NEWSPOSTER", "exit 0");

    supersede_article("Z");

    const std::string expected_header = "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                        "Supersedes: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                        "X-Long: " +
                                        long_header_value + "\n\n";
    const std::string written = file_contents(head_file);
    EXPECT_THAT(written, StartsWith(expected_header));
    EXPECT_THAT(written, HasSubstr(TRN_TEST_BODY));
}

TEST_F(InterpolatorNewsgroupTest, failedPostAppendsHeaderToDeadArticle)
{
    if (do_shell(SH, "exit 42") != 42)
    {
        GTEST_SKIP() << "configured shell cannot produce the NEWSPOSTER sentinel";
    }
    create_failing_inews(fs::path{m_output.path()});
    const char             *old_path = std::getenv("PATH");
    const std::string       path = m_output.path() + s_path_separator + (old_path == nullptr ? "" : old_path);
    const fs::path          head_file = fs::path{m_output.path()} / "failed-post-head";
    const fs::path          dead_article = fs::path{m_output.path()} / "dead.article";
    const std::string       existing_dead_article = "previous failed post\n";
    const std::string       long_header_value(CMD_BUF_LEN + 75, 'x');
    const std::string       supersede_header = "Newsgroups: %n\n"
                                               "X-Long: " +
                                               long_header_value + "\n";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string> orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string> dot_dir(g_dot_dir, m_output.path());
    ValueSaver<std::string> spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("PATH", path.c_str());
    m_env.expect_env("FROM", TRN_TEST_HEADER_FROM);
    m_env.expect_env("SUPERSEDEHEADER", supersede_header.c_str());
    m_env.expect_env("NEWSPOSTER", "exit 42");
    std::ofstream{dead_article} << existing_dead_article;

    testing::internal::CaptureStdout();
    supersede_article("z");
    const std::string output = testing::internal::GetCapturedStdout();

    const std::string expected_header = "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                        "X-Long: " +
                                        long_header_value + "\n";
    EXPECT_THAT(output, HasSubstr("Article appended to " + dead_article.generic_string()));
    EXPECT_EQ(existing_dead_article + expected_header, file_contents(dead_article));
}

TEST_F(InterpolatorNewsgroupTest, replyWritesInterpolatedHeaderAndQuotedBody)
{
    const fs::path           head_file = fs::path{m_output.path()} / "reply-head";
    const std::string        long_header_value(600, 'x');
    const std::string        mail_header = "To: tester@example.org\n"
                                           "In-Reply-To: %i\n"
                                           "X-Long: " +
                                           long_header_value + "\n\n";
    ValueSaver<std::string>  head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string>  orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string>  spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string>  group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    ValueSaver<std::string>  indent(g_indent_string, ">");
    reset_char_subst_mode();
    m_env.expect_env("MAILPOSTER", "exit 0 %h");
    m_env.expect_env("MAILHEADER", mail_header.c_str());
    m_env.expect_env("YOUSAID", "said %i:");

    reply("R");

    const std::string expected_header = "To: tester@example.org\n"
                                        "In-Reply-To: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                        "X-Long: " +
                                        long_header_value + "\n\n";
    const std::string written = file_contents(head_file);
    EXPECT_THAT(written, StartsWith(expected_header + "said " TRN_TEST_HEADER_MESSAGE_ID ":\n"));
    EXPECT_THAT(written, HasSubstr(">" TRN_TEST_BODY "\n"));
}

TEST_F(InterpolatorNewsgroupTest, replyDisplaysHeaderFileWhenMailerDoesNotAcceptHeader)
{
    const fs::path          head_file = fs::path{m_output.path()} / "reply-head";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string> orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string> spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("MAILPOSTER", "exit 0");
    m_env.expect_env("MAILHEADER", "To: tester@example.org\n\n");

    testing::internal::CaptureStdout();
    reply("r");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_THAT(output, HasSubstr("(Above lines saved in file " + head_file.generic_string() + ")"));
}

TEST_F(InterpolatorNewsgroupTest, forwardWritesInterpolatedHeaderMessageAndArticle)
{
    const fs::path          head_file = fs::path{m_output.path()} / "forward-head";
    const std::string       long_header_value(600, 'x');
    const std::string       forward_header = "Subject: forward %i\n"
                                             "X-Long: " +
                                             long_header_value + "\n\n";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string> orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string> spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("FORWARDPOSTER", "exit 0 %h");
    m_env.expect_env("FORWARDHEADER", forward_header.c_str());
    m_env.expect_env("FORWARDMSG", "Forwarded %i");
    m_env.expect_env("FORWARDMSGEND", "End %i");

    testing::internal::CaptureStdout();
    forward();
    const std::string output = testing::internal::GetCapturedStdout();

    const std::string expected_header = "Subject: forward " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                        "X-Long: " +
                                        long_header_value + "\n\n";
    const std::string written = file_contents(head_file);
    EXPECT_THAT(output, testing::Not(HasSubstr("- ")));
    EXPECT_THAT(written, StartsWith(expected_header + "Forwarded " TRN_TEST_HEADER_MESSAGE_ID "\n"));
    EXPECT_THAT(written, HasSubstr("Path: " TRN_TEST_HEADER_PATH "\n"));
    EXPECT_THAT(written, HasSubstr(TRN_TEST_BODY));
    EXPECT_THAT(written, HasSubstr("End " TRN_TEST_HEADER_MESSAGE_ID "\n"));
}

TEST_F(InterpolatorNewsgroupTest, forwardUsesMimeBoundaryFromInterpolatedHeader)
{
    const fs::path          head_file = fs::path{m_output.path()} / "forward-head";
    const std::string       forward_header = "Subject: MIME forward\n"
                                             "Content-Type: multipart/mixed; boundary=\"trn-boundary\"\n\n";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string> orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string> spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("FORWARDPOSTER", "exit 0 %h");
    m_env.expect_env("FORWARDHEADER", forward_header.c_str());
    m_env.expect_env("FORWARDMSG", "Forwarded %i");

    forward();

    const std::string written = file_contents(head_file);
    EXPECT_THAT(written, StartsWith(forward_header));
    EXPECT_THAT(written, HasSubstr("--trn-boundary\nContent-Type: text/plain\n\n"));
    EXPECT_THAT(written, HasSubstr("[Replace this with your comments.]\n\n"
                                   "--trn-boundary\n"
                                   "Content-Type: message/rfc822\n\n"
                                   "Path: " TRN_TEST_HEADER_PATH "\n"));
    EXPECT_THAT(written, HasSubstr("\n--trn-boundary--\n"));
}

namespace
{

class ModeratorPostingTest : public InterpolatorNewsgroupTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void compose(std::string_view header, std::string_view command = "F");
    void compose_without_remote_lookup(std::string_view policy);

    DataSource  m_source{};
    DataSource *m_saved_source{};
    std::string m_old_head;
    std::string m_old_orig_dir;
    fs::path    m_header;
};

void ModeratorPostingTest::SetUp()
{
    InterpolatorNewsgroupTest::SetUp();
    m_saved_source = g_data_source;
    m_old_head = g_head_name;
    m_old_orig_dir = g_orig_dir;
    m_header = fs::path{m_output.path()} / "moderator-head";
    g_head_name = m_header.generic_string();
    g_orig_dir = m_output.path();
    const fs::path active = fs::path{m_output.path()} / "moderator-active";
    std::ofstream{active} << TRN_TEST_NEWSGROUP " 626 623 y\n"
                                                "list.test 1 1 m\nlist.other 1 1 m\nopen.test 1 1 y\n";
    EXPECT_EQ(1, m_source.m_act_sf.open(active, "", ""));
    m_source.m_news_id = active.generic_string();
    m_source.m_spool_dir = TRN_TEST_LOCAL_SPOOL_DIR;
    m_source.m_moderators_file = (fs::path{m_output.path()} / "moderators").generic_string();
    std::ofstream{m_source.m_moderators_file} << "list.*:list@example.test\n*:fallback@example.test\n";
    g_data_source = &m_source;
    m_env.expect_env("NEWSPOSTER", "echo news > news-used");
    m_env.expect_env("MAILPOSTER", "echo mail > mail-used");
}

void ModeratorPostingTest::TearDown()
{
    g_data_source = m_saved_source;
    g_head_name = m_old_head;
    g_orig_dir = m_old_orig_dir;
    m_source.m_act_sf.close();
    InterpolatorNewsgroupTest::TearDown();
}

void ModeratorPostingTest::compose(std::string_view header, std::string_view command)
{
    const std::string text{header};
    m_env.expect_env("NEWSHEADER", text.c_str());
    m_env.expect_env("ATTRIBUTION", "Quoted article:");
    testing::internal::CaptureStdout();
    followup(command);
    (void) testing::internal::GetCapturedStdout();
}

void ModeratorPostingTest::compose_without_remote_lookup(std::string_view policy)
{
    const auto           connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    ValueSaver<NNTPLink> saved_link(g_nntp_link, NNTPLink{});
    g_nntp_link.connection = connection;
    g_nntp_link.flags = NNTP_NEW_CMD_OK;
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add(policy == "none" ? "LIST ACTIVE MODERATORS" : "LIST ACTIVE");
    m_source.m_flags |= DF_REMOTE | DF_USE_LIST_ACTIVE;
    m_source.m_moderators_file = policy;
    push_char('\n');

    compose("Newsgroups: list.test\nSubject: topic\n\n", "f");

    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "news-used"));
    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "mail-used"));
}

} // namespace

TEST_F(ModeratorPostingTest, routesFollowupThroughMailComposer)
{
    compose("Newsgroups: list.test\nSubject: topic\n\n");

    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "news-used"));
    EXPECT_THAT(file_contents(m_header),
                StartsWith("To: list@example.test\nNewsgroups: list.test\nSubject: topic\n\n"));
    EXPECT_THAT(file_contents(m_header), HasSubstr("Quoted article:\n"));
}

TEST_F(ModeratorPostingTest, routesNewTopicToModerator)
{
    push_char('\n');
    compose("Newsgroups: list.test\nSubject: new topic\n\n", "f");

    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_EQ("To: list@example.test\nNewsgroups: list.test\nSubject: new topic\n\n", file_contents(m_header));
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
}

TEST_F(ModeratorPostingTest, unmoderatedGroupsKeepNewsPosting)
{
    compose("Newsgroups: open.test\nSubject: topic\n\n");

    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "news-used"));
    EXPECT_THAT(file_contents(m_header), StartsWith("Newsgroups: open.test\n"));
}

TEST_F(ModeratorPostingTest, missingModeratorKeepsNewsPosting)
{
    m_source.m_moderators_file += ".missing";
    compose("Newsgroups: list.test\nSubject: topic\n\n");

    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "news-used"));
}

TEST_F(ModeratorPostingTest, approvedPostsKeepNewsPosting)
{
    compose("Newsgroups: list.test\nApproved: editor@example.test\n\n");

    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "news-used"));
}

TEST_F(ModeratorPostingTest, foldedCrosspostDeduplicatesRecipientsAndReplacesTo)
{
    compose("Newsgroups: list.test,\n list.other,open.test\nTo: previous@example.test\n"
            " ignored@example.test\nSubject: topic\n\n");

    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_THAT(file_contents(m_header),
                StartsWith("To: list@example.test\nNewsgroups: list.test,\n list.other,open.test\nSubject: topic\n\n"));
    EXPECT_THAT(file_contents(m_header), testing::Not(HasSubstr("previous@example.test")));
    EXPECT_THAT(file_contents(m_header), testing::Not(HasSubstr("ignored@example.test")));
}

TEST_F(ModeratorPostingTest, remoteListRoutesNewTopicThroughMailComposer)
{
    const auto           connection = std::make_shared<testing::StrictMock<MockNNTPConnection>>();
    ValueSaver<NNTPLink> saved_link(g_nntp_link, NNTPLink{});
    g_nntp_link.connection = connection;
    g_nntp_link.flags = NNTP_NEW_CMD_OK;
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("LIST ACTIVE MODERATORS");
    m_source.m_flags |= DF_REMOTE;
    m_source.m_moderators_file.clear();
    EXPECT_CALL(*connection, write_line(testing::StrEq("LIST MODERATORS"), testing::_));
    EXPECT_CALL(*connection, read_line(testing::_))
        .WillOnce(testing::Return("215 list follows"))
        .WillOnce(testing::Return("list.*:%s@remote.example.test"))
        .WillOnce(testing::Return("."));
    push_char('\n');

    compose("Newsgroups: list.test\nSubject: remote topic\n\n", "f");

    EXPECT_TRUE(fs::exists(fs::path{m_output.path()} / "mail-used"));
    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "news-used"));
    EXPECT_THAT(file_contents(m_header), StartsWith("To: list-test@remote.example.test\n"));
}

TEST_F(ModeratorPostingTest, mailComposerFailureDoesNotResubmitToNews)
{
    m_env.expect_env("MAILPOSTER", "exit 42");

    compose("Newsgroups: list.test\nSubject: topic\n\n");

    EXPECT_FALSE(fs::exists(fs::path{m_output.path()} / "news-used"));
    EXPECT_THAT(file_contents(m_header), StartsWith("To: list@example.test\n"));
}

TEST_F(ModeratorPostingTest, disabledRoutingDoesNotQueryActiveGroups)
{
    compose_without_remote_lookup("none");
}

TEST_F(ModeratorPostingTest, unadvertisedRoutingDoesNotQueryActiveGroups)
{
    compose_without_remote_lookup("remote");
}

TEST_F(InterpolatorNewsgroupTest, followupWritesInterpolatedHeaderAndQuotedBody)
{
    const fs::path           head_file = fs::path{m_output.path()} / "followup-head";
    const std::string        long_header_value(600, 'x');
    const std::string        news_header = "Newsgroups: %F\n"
                                           "References: %i\n"
                                           "X-Long: " +
                                           long_header_value + "\n\n";
    ValueSaver<std::string>  head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string>  orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string>  spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string>  group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    ValueSaver<std::string>  indent(g_indent_string, ">");
    reset_char_subst_mode();
    m_env.expect_env("NEWSPOSTER", "exit 0");
    m_env.expect_env("NEWSHEADER", news_header.c_str());
    m_env.expect_env("ATTRIBUTION", "In article %i:");

    testing::internal::CaptureStdout();
    followup("F");
    testing::internal::GetCapturedStdout();

    const std::string expected_header = "Newsgroups: " TRN_TEST_HEADER_FOLLOWUP_TO "\n"
                                        "References: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                        "X-Long: " +
                                        long_header_value + "\n\n";
    const std::string written = file_contents(head_file);
    EXPECT_THAT(written, StartsWith(expected_header + "In article " TRN_TEST_HEADER_MESSAGE_ID ":\n"));
    EXPECT_THAT(written, HasSubstr(">" TRN_TEST_BODY "\n"));
}

TEST_F(InterpolatorNewsgroupTest, followupPromptDefaultStartsNewTopic)
{
    const fs::path          head_file = fs::path{m_output.path()} / "followup-head";
    ValueSaver<std::string> head_name(g_head_name, head_file.generic_string());
    ValueSaver<std::string> orig_dir(g_orig_dir, m_output.path());
    ValueSaver<std::string> spool_dir(g_data_source->m_spool_dir, TRN_TEST_LOCAL_SPOOL_DIR);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, TRN_TEST_NEWSGROUP_SUBDIR);
    m_env.expect_env("NEWSPOSTER", "exit 0");
    m_env.expect_env("NEWSHEADER", "Message-Id: %i\n\n");
    push_char('\n');

    testing::internal::CaptureStdout();
    followup("f");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_THAT(output, HasSubstr("Are you starting an unrelated topic? [ynq]"));
    EXPECT_EQ("Message-Id: \n\n", file_contents(head_file));
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
}

TEST_F(ArticleHeaderPresentationTest, displaysFromNameWhilePreservingTheCachedHeader)
{
    render_article();

    EXPECT_EQ(DA_NORM, m_result);
    EXPECT_NE(std::string::npos, m_rendered.find("From: " TRN_TEST_HEADER_FROM_NAME));
    EXPECT_EQ(std::string::npos, m_rendered.find("From: " TRN_TEST_HEADER_FROM));
    EXPECT_EQ(TRN_TEST_HEADER_FROM, fetch_lines(g_art, FROM_LINE));
}

TEST_F(ArticleHeaderPresentationTest, constructsTheEndOfArticlePrompt)
{
    render_article();

    EXPECT_EQ(DA_NORM, m_result);
    EXPECT_EQ("%sEnd of article " + std::to_string(TRN_TEST_ARTICLE_NUM) + " (of " +
                  std::to_string(TRN_TEST_NEWSGROUP_HIGH) + ") %s-- what next? [%s]",
              g_prompt);
}

TEST_P(PagerEndArticleTest, completesAfterTheLastDisplayedPageWithoutConsumingTheNextCommand)
{
    read_to_end();

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_NORM, m_result);
    for (std::size_t page = 0; page < m_pages.size(); ++page)
    {
        EXPECT_EQ(m_pages[page], displayed_rows(page)) << "Page " << page;
    }
    std::size_t prompts = 0;
    for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
         position = m_rendered.find("--MORE--(", position + 1))
    {
        ++prompts;
    }
    EXPECT_EQ(m_pages.size() - 1, prompts);
    EXPECT_THAT(m_rendered, Not(HasSubstr("--MORE--(100%)")));
    EXPECT_EQ(g_art_size, g_art_pos);
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_TRUE(m_article_command.empty());
    ASSERT_TRUE(macro_pending());
    EXPECT_EQ('q', read_tty_char());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(
    ArticleEndings, PagerEndArticleTest,
    Combine(Values(PagerEndCase{"Empty", "", {{}}}, PagerEndCase{"OneLine", "Body 000\n", {{"Body 000"}}},
                   PagerEndCase{"Short", numbered_pager_body(0, 3), {numbered_pager_rows(0, 3)}},
                   PagerEndCase{"ExactFirstPage", numbered_pager_body(0, 4), {numbered_pager_rows(0, 4)}},
                   PagerEndCase{
                       "OneLineBeyondFirstPage", numbered_pager_body(0, 5), {numbered_pager_rows(0, 4), {"Body 004"}}},
                   PagerEndCase{"ExactSecondPage",
                                numbered_pager_body(0, 7),
                                {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7)}},
                   PagerEndCase{"PartialThirdPage",
                                numbered_pager_body(0, 9),
                                {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7), numbered_pager_rows(7, 9)}},
                   PagerEndCase{"ExactThirdPage",
                                numbered_pager_body(0, 10),
                                {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7), numbered_pager_rows(7, 10)}},
                   PagerEndCase{"UnterminatedShort", "Body 000", {{"Body 000"}}},
                   PagerEndCase{"UnterminatedExactFirstPage",
                                numbered_pager_body(0, 3) + "Body 003",
                                {numbered_pager_rows(0, 4)}},
                   PagerEndCase{"UnterminatedExactSecondPage",
                                numbered_pager_body(0, 6) + "Body 006",
                                {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7)}},
                   PagerEndCase{"UnterminatedPartialSecondPage",
                                numbered_pager_body(0, 4) + "Body 004",
                                {numbered_pager_rows(0, 4), {"Body 004"}}},
                   PagerEndCase{"AllHidden", "> hidden first\n> hidden last\n", {{}}, "^>..."},
                   PagerEndCase{"HiddenTailOnShortPage", "Body 000\n> hidden last\n", {{"Body 000"}}, "^>..."},
                   PagerEndCase{"HiddenTailAfterExactFirstPage",
                                numbered_pager_body(0, 4) + "> hidden first\n> hidden last\n",
                                {numbered_pager_rows(0, 4)},
                                "^>..."},
                   PagerEndCase{"HiddenTailAfterExactSecondPage",
                                numbered_pager_body(0, 7) + "> hidden last\n",
                                {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7)},
                                "^>..."},
                   PagerEndCase{"VisibleLineAfterHiddenTail",
                                numbered_pager_body(0, 4) + "> hidden first\n> hidden last\nBody 004\n",
                                {numbered_pager_rows(0, 4), {"Body 004"}},
                                "^>..."},
                   PagerEndCase{"WrappedHiddenTail",
                                numbered_pager_body(0, 4) + "> hidden " + std::string(100, 'x'),
                                {numbered_pager_rows(0, 4)},
                                "^>...",
                                "",
                                32,
                                8},
                   PagerEndCase{"ExactWidthUnterminated", std::string(32, 'x'), {{std::string(32, 'x')}}, "", "", 32},
                   PagerEndCase{"UnbrokenExactPageUnterminated",
                                std::string(128, 'x'),
                                {std::vector<std::string>(4, std::string(32, 'x'))},
                                "",
                                "",
                                32},
                   PagerEndCase{"UnbrokenBeyondPageUnterminated",
                                std::string(129, 'x'),
                                {std::vector<std::string>(4, std::string(32, 'x')), {"x"}},
                                "",
                                "",
                                32},
                   PagerEndCase{"WordWrappedUnterminated",
                                "Body 000 alpha beta end Body 001 alpha beta end Body 002 alpha beta end "
                                "Body 003 alpha beta end",
                                {{"Body 000 alpha beta end", "Body 001 alpha beta end", "Body 002 alpha beta end",
                                  "Body 003 alpha beta end"}},
                                "",
                                "",
                                32,
                                8},
                   PagerEndCase{"VisibleContinuationMatchesHidePattern",
                                "Body 000 alpha beta end Body 001 alpha beta end Body 002 alpha beta end "
                                "Body 003 alpha beta end > continuation end",
                                {{"Body 000 alpha beta end", "Body 001 alpha beta end", "Body 002 alpha beta end",
                                  "Body 003 alpha beta end"},
                                 {"> continuation end"}},
                                "^>...",
                                "",
                                32,
                                8},
                   PagerEndCase{"RawUnterminated", "Body 000", {{"Body 000"}}, "", "", 80, -1, false},
                   PagerEndCase{
                       "RawExactPage", numbered_pager_body(0, 4), {numbered_pager_rows(0, 4)}, "", "", 80, -1, false},
                   PagerEndCase{"RawUnterminatedSecondPage",
                                numbered_pager_body(0, 6) + "Body 006",
                                {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7)},
                                "",
                                "",
                                80,
                                -1,
                                false}),
            Bool()),
    [](const TestParamInfo<PagerEndParam> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Clearing" : "Scrolling"); });

INSTANTIATE_TEST_SUITE_P(
    FilteredEndings, PagerEndArticleTest,
    Combine(
        Values(
            PagerEndCase{"OnlySoftBreaks",
                         "=\n=\n",
                         {{}},
                         "",
                         "MIME-Version: 1.0\nContent-Type: text/plain\nContent-Transfer-Encoding: quoted-printable\n"},
            PagerEndCase{"SoftBreakTailOnShortPage",
                         "=42ody 000\n=\n=\n",
                         {{"Body 000"}},
                         "",
                         "MIME-Version: 1.0\nContent-Type: text/plain\nContent-Transfer-Encoding: quoted-printable\n"},
            PagerEndCase{"SoftBreakTailAfterExactPage",
                         numbered_pager_body(0, 4) + "=\n=\n",
                         {numbered_pager_rows(0, 4)},
                         "",
                         "MIME-Version: 1.0\nContent-Type: text/plain\nContent-Transfer-Encoding: quoted-printable\n"},
            PagerEndCase{"SoftBreakTailAfterSecondPage",
                         numbered_pager_body(0, 7) + "=\n=\n",
                         {numbered_pager_rows(0, 4), numbered_pager_rows(4, 7)},
                         "",
                         "MIME-Version: 1.0\nContent-Type: text/plain\nContent-Transfer-Encoding: quoted-printable\n"}),
        Bool()),
    [](const TestParamInfo<PagerEndParam> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Clearing" : "Scrolling"); });

TEST_P(PagerFormattedHeaderArticleTest, pagesThroughFormattedHeadersWithoutChangingCachedTextOrBodyOffsets)
{
    const std::time_t original_date = g_artp->m_date;

    read_headers();

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_TO_END, m_result);
    for (std::size_t page = 0; page < m_pages.size(); ++page)
    {
        EXPECT_EQ(m_pages[page], displayed_rows(page)) << "Page " << page;
    }
    std::size_t prompts = 0;
    for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
         position = m_rendered.find("--MORE--(", position + 1))
    {
        ++prompts;
    }
    EXPECT_EQ(m_pages.size(), prompts);
    EXPECT_EQ(m_headers, g_head_buf);
    EXPECT_EQ(m_subject, g_artp->get_cached_line_view(SUBJ_LINE, false));
    EXPECT_EQ(m_from, g_artp->get_cached_line_view(FROM_LINE, false));
    EXPECT_EQ(original_date, g_artp->m_date);
    for (std::size_t i = 0; i < m_header_spans.size(); ++i)
    {
        EXPECT_EQ(m_header_spans[i].first, g_header_type[i].min_pos) << "Header " << i;
        EXPECT_EQ(m_header_spans[i].second, g_header_type[i].max_pos) << "Header " << i;
    }
    EXPECT_EQ(m_body_start, g_header_type[PAST_HEADER].min_pos);
    EXPECT_EQ(ArticleLine{m_header_rows + m_next_body_row}, g_art_line_num);
    for (long row = 0; row <= m_next_body_row; ++row)
    {
        const std::string     text = numbered_pager_rows(row, row + 1).front();
        const ArticlePosition expected = g_do_hiding ? m_body_start + ArticlePosition{row * 9}
                                                     : ArticlePosition{static_cast<long>(m_source.find(text))};
        EXPECT_EQ(expected, virtual_read(ArticleLine{m_header_rows + row})) << "Body row " << row;
    }
    EXPECT_EQ(virtual_read(g_art_line_num), g_art_pos);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_TRUE(m_article_command.empty());
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(
    HeaderFormattingAndPaging, PagerFormattedHeaderArticleTest,
    Combine(Values(PagerFormattedHeaderCase{"FoldedSubject",
                                            SUBJ_LINE,
                                            "Subject: first\n continuation\n",
                                            {"Subject: first continuation"},
                                            {"Subject: first", "+        continuation"}},
                   PagerFormattedHeaderCase{"SubjectWithoutMagic",
                                            SUBJ_LINE,
                                            "Subject: first\n continuation\n",
                                            {"Subject: first", "+        continuation"},
                                            {"Subject: first", "+        continuation"},
                                            false},
                   PagerFormattedHeaderCase{"EncodedFoldedSubject",
                                            SUBJ_LINE,
                                            "Subject: =?us-ascii?q?first?=\n =?us-ascii?q?_continuation?=\n",
                                            {"Subject: first continuation"},
                                            {"Subject: =?us-ascii?q?first?=", "+        =?us-ascii?q?_continuation?="}},
                   PagerFormattedHeaderCase{"WrappedSubject",
                                            SUBJ_LINE,
                                            "Subject: one two three four five six seven eight\n",
                                            {"Subject: one two three four", "+        five six seven eight"},
                                            {"Subject: one two three four", "+        five six seven eight"},
                                            true,
                                            true,
                                            32,
                                            2},
                   PagerFormattedHeaderCase{"FoldedAuthorComment",
                                            FROM_LINE,
                                            "From: reader@example.test\n (Reader Name)\n",
                                            {"From: Reader Name"},
                                            {"From: reader@example.test", "+     (Reader Name)"}},
                   PagerFormattedHeaderCase{"ShortAuthorComment",
                                            FROM_LINE,
                                            "From: reader@example.test\n (R)\n",
                                            {"From: R"},
                                            {"From: reader@example.test", "+     (R)"}},
                   PagerFormattedHeaderCase{"FoldedAuthorAddress",
                                            FROM_LINE,
                                            "From: Reader Name\n <reader@example.test>\n",
                                            {"From: Reader Name"},
                                            {"From: Reader Name", "+     <reader@example.test>"}},
                   PagerFormattedHeaderCase{"PlainAuthorAddress",
                                            FROM_LINE,
                                            "From: reader@example.test\n",
                                            {"From: reader@example.test"},
                                            {"From: reader@example.test"}},
                   PagerFormattedHeaderCase{"AuthorWithoutMagic",
                                            FROM_LINE,
                                            "From: reader@example.test\n (Reader Name)\n",
                                            {"From: reader@example.test", "+     (Reader Name)"},
                                            {"From: reader@example.test", "+     (Reader Name)"},
                                            false},
                   PagerFormattedHeaderCase{"FoldedDate",
                                            DATE_LINE,
                                            "Date: Thu, 04 Dec 2003\n 05:06:07 +0000\n",
                                            {"Date: local date"},
                                            {"Date: Thu, 04 Dec 2003", "+     05:06:07 +0000"}},
                   PagerFormattedHeaderCase{"DateWithoutMagic",
                                            DATE_LINE,
                                            "Date: Thu, 04 Dec 2003\n 05:06:07 +0000\n",
                                            {"Date: Thu, 04 Dec 2003", "+     05:06:07 +0000"},
                                            {"Date: Thu, 04 Dec 2003", "+     05:06:07 +0000"},
                                            false},
                   PagerFormattedHeaderCase{"UnknownDate",
                                            DATE_LINE,
                                            "Date: unknown date\n",
                                            {"Date: unknown date"},
                                            {"Date: unknown date"},
                                            true,
                                            false},
                   PagerFormattedHeaderCase{"FoldedCurrentNewsgroup",
                                            NEWSGROUPS_LINE,
                                            "Newsgroups:\n list.current\n",
                                            {},
                                            {"Newsgroups:", "list.current"}},
                   PagerFormattedHeaderCase{"FoldedCrosspost",
                                            NEWSGROUPS_LINE,
                                            "Newsgroups: list.current,\n list.other\n",
                                            {"Newsgroups: list.current,", "+           list.other"},
                                            {"Newsgroups: list.current,", "+           list.other"}},
                   PagerFormattedHeaderCase{"NewsgroupWithoutMagic",
                                            NEWSGROUPS_LINE,
                                            "Newsgroups:\n list.current\n",
                                            {"Newsgroups:", "list.current"},
                                            {"Newsgroups:", "list.current"},
                                            false}),
            Bool(), Bool()),
    [](const TestParamInfo<PagerFormattedHeaderParam> &info)
    {
        return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Formatted" : "Raw") +
               (std::get<2>(info.param) ? "Refresh" : "Continue");
    });

TEST_P(PagerHiddenHeaderArticleTest, hiddenHeadersDoNotConsumeBodyRows)
{
    const PagerHiddenHeaderCase &scenario = std::get<0>(GetParam());
    const bool                   hiding = std::get<1>(GetParam());
    const long                   shown_headers = hiding ? 0 : scenario.lines;

    read_pages(" ");

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_TO_END, m_result);
    const long first_body_count = 4 - shown_headers;
    for (std::size_t page = 0; page < 2; ++page)
    {
        std::vector<std::string> expected;
        const long               first = page == 0 ? 0 : first_body_count;
        const long               count = page == 0 ? first_body_count : 6;
        for (long line = first; line < first + count; ++line)
        {
            const std::string text = numbered_pager_body(line, line + 1);
            expected.push_back(text.substr(0, text.size() - 1));
        }
        std::vector<std::string> actual = displayed_rows(page);
        if (page == 0)
        {
            while (!actual.empty() && actual.front().find("Body ") != 0)
            {
                actual.erase(actual.begin());
            }
        }
        EXPECT_EQ(expected, actual) << "Page " << page;
    }
    const std::string_view header_line = scenario.header.substr(0, scenario.header.find('\n'));
    EXPECT_EQ(!hiding, m_rendered.find(header_line) != std::string::npos);
    if (scenario.lines > 1)
    {
        const std::size_t      continuation = scenario.header.find('\n') + 1;
        const std::string_view continuation_line =
            scenario.header.substr(continuation, scenario.header.find('\n', continuation) - continuation);
        EXPECT_EQ(!hiding, m_rendered.find(continuation_line) != std::string::npos);
    }
    EXPECT_EQ(m_headers, g_head_buf);
    EXPECT_EQ(HT_HIDE, g_header_type[scenario.type].flags & HT_HIDE);
    const long        next_line = first_body_count + 6;
    const std::string next_text = numbered_pager_body(next_line, next_line + 1);
    expect_position(2 + shown_headers + next_line, std::string_view{next_text}.substr(0, next_text.size() - 1));
}

INSTANTIATE_TEST_SUITE_P(
    HeaderKindsAndDisplayModes, PagerHiddenHeaderArticleTest,
    Combine(Values(PagerHiddenHeaderCase{"FoldedFrom", "From: Reader <reader@example.test>\n via a gateway\n",
                                         FROM_LINE, 2},
                   PagerHiddenHeaderCase{"Path", "Path: first!second!last\n", PATH_LINE, 1},
                   PagerHiddenHeaderCase{"FoldedUnrecognized", "X-Trace: private\n more trace detail\n", SOME_LINE, 2}),
            Bool()),
    [](const TestParamInfo<PagerHiddenHeaderParam> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Hidden" : "Raw"); });

TEST_P(PagerFilteredBodyArticleTest, filteringAndStopsPreserveDisplayedRowsAndPosition)
{
    const PagerFilteredBodyCase &scenario = GetParam();

    read_pages(scenario.commands);

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_TO_END, m_result);
    long displayed = 0;
    for (std::size_t page = 0; page < scenario.pages.size(); ++page)
    {
        EXPECT_EQ(scenario.pages[page], displayed_rows(page)) << "Page " << page;
        for (const std::string &line : scenario.pages[page])
        {
            const ArticlePosition saved = virtual_read(ArticleLine{displayed});
            const bool            stop =
                std::find(scenario.stop_rows.begin(), scenario.stop_rows.end(), displayed) != scenario.stop_rows.end();
            EXPECT_EQ(stop, saved < 0) << "Row " << displayed;
            EXPECT_EQ(position_of(line), saved < 0 ? -saved : saved) << "Row " << displayed;
            ++displayed;
        }
    }
    std::size_t prompts = 0;
    for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
         position = m_rendered.find("--MORE--(", position + 1))
    {
        ++prompts;
    }
    EXPECT_EQ(scenario.pages.size(), prompts);
    expect_position(displayed, scenario.next_line);
}

INSTANTIATE_TEST_SUITE_P(
    HiddenLinesAndPageStops, PagerFilteredBodyArticleTest,
    Values(
        PagerFilteredBodyCase{"QuotedPrefix",
                              "> quoted first\n> quoted second\n>\n" + numbered_pager_body(),
                              "^>...",
                              "",
                              " ",
                              {{">", "Body 000", "Body 001", "Body 002"}, {"Body 003", "Body 004", "Body 005"}},
                              "Body 006"},
        PagerFilteredBodyCase{"ConsecutiveHiddenLines",
                              "Body 000\n> hidden alpha\n> hidden beta\n" + numbered_pager_body(1),
                              "^>...",
                              "",
                              " ",
                              {{"Body 000", "Body 001", "Body 002", "Body 003"}, {"Body 004", "Body 005", "Body 006"}},
                              "Body 007"},
        PagerFilteredBodyCase{"HiddenBlockAtPageBoundary",
                              numbered_pager_body(0, 4) + "> hidden alpha\n> hidden beta\n" + numbered_pager_body(4),
                              "^>...",
                              "",
                              " ",
                              {{"Body 000", "Body 001", "Body 002", "Body 003"}, {"Body 004", "Body 005", "Body 006"}},
                              "Body 007"},
        PagerFilteredBodyCase{"AlternativeHiddenPatterns",
                              "> quoted first\nSKIP removed\n>\n" + numbered_pager_body(),
                              "^>...\\|^SKIP",
                              "",
                              " ",
                              {{">", "Body 000", "Body 001", "Body 002"}, {"Body 003", "Body 004", "Body 005"}},
                              "Body 006"},
        PagerFilteredBodyCase{"WrappedHiddenLine",
                              "> quoted line alpha beta gamma delta epsilon zeta eta theta\n" + numbered_pager_body(),
                              "^>...",
                              "",
                              " ",
                              {{"Body 000", "Body 001", "Body 002", "Body 003"}, {"Body 004", "Body 005", "Body 006"}},
                              "Body 007",
                              {},
                              true},
        PagerFilteredBodyCase{"VisibleWrappedContinuation",
                              "Body first alpha beta hidden continuation text ends\n" + numbered_pager_body(),
                              "^hidden",
                              "",
                              " ",
                              {{"Body first alpha beta", "hidden continuation text", "ends", "Body 000"},
                               {"Body 001", "Body 002", "Body 003"}},
                              "Body 004",
                              {},
                              true},
        PagerFilteredBodyCase{
            "RawDisplayIgnoresHiding",
            "> quoted content\n" + numbered_pager_body(),
            "^>...",
            "",
            " ",
            {{"> quoted content", "Body 000", "Body 001", "Body 002"}, {"Body 003", "Body 004", "Body 005"}},
            "Body 006",
            {},
            false,
            false},
        PagerFilteredBodyCase{"SignatureStop",
                              "Body 000\n-- \nsig one\nsig two\n" + numbered_pager_body(1),
                              "",
                              "^-- $",
                              " ",
                              {{"Body 000", "-- "}, {"sig one", "sig two", "Body 001", "Body 002"}},
                              "Body 003",
                              {1}},
        PagerFilteredBodyCase{
            "AdjacentDigestStops",
            "-------- first\n-------- second\n" + numbered_pager_body(),
            "",
            "^--------",
            "  ",
            {{"-------- first"}, {"-------- second"}, {"Body 000", "Body 001", "Body 002", "Body 003"}},
            "Body 004",
            {0, 1}},
        PagerFilteredBodyCase{
            "StopAtPageBoundary",
            numbered_pager_body(0, 3) + "-- \n" + numbered_pager_body(3),
            "",
            "^-- $",
            " ",
            {{"Body 000", "Body 001", "Body 002", "-- "}, {"Body 003", "Body 004", "Body 005", "Body 006"}},
            "Body 007",
            {3}},
        PagerFilteredBodyCase{"StopOnFirstLine",
                              "-- \n" + numbered_pager_body(),
                              "",
                              "^-- $",
                              " ",
                              {{"-- "}, {"Body 000", "Body 001", "Body 002", "Body 003"}},
                              "Body 004",
                              {0}},
        PagerFilteredBodyCase{"NonmatchingStopPattern",
                              "Body 000\n--x\n" + numbered_pager_body(1),
                              "",
                              "^-- $",
                              " ",
                              {{"Body 000", "--x", "Body 001", "Body 002"}, {"Body 003", "Body 004", "Body 005"}},
                              "Body 006"},
        PagerFilteredBodyCase{
            "WrappedStopContinuesWithoutMatchingAgain",
            "-------- alpha beta gamma delta epsilon zeta eta theta iota kappa\n" + numbered_pager_body(),
            "",
            "^--------\\|^gamma",
            " ",
            {{"-------- alpha beta"}, {"gamma delta epsilon zeta", "eta theta iota kappa", "Body 000", "Body 001"}},
            "Body 002",
            {0},
            true},
        PagerFilteredBodyCase{"HiddenStopDoesNotEndThePage",
                              "Body 000\n> hidden stop\n" + numbered_pager_body(1),
                              "^>...",
                              "^>",
                              " ",
                              {{"Body 000", "Body 001", "Body 002", "Body 003"}, {"Body 004", "Body 005", "Body 006"}},
                              "Body 007"}),
    [](const TestParamInfo<PagerFilteredBodyCase> &info) { return info.param.name; });

TEST_P(PagerSearchArticleTest, displaysTheMatchingRowAndClearsItsHighlightBeforeContinuing)
{
    const PagerSearchCase &scenario = std::get<0>(GetParam());

    read_pages(scenario.commands);

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_TO_END, m_result);
    for (std::size_t page = 0; page < scenario.pages.size(); ++page)
    {
        SCOPED_TRACE(page);
        const PagerSearchPage         &expected = scenario.pages[page];
        const std::vector<std::string> rows{m_rows.begin() + expected.first,
                                            m_rows.begin() + expected.first + expected.count};
        std::vector<std::string>       actual = displayed_rows(page);
        while (!actual.empty() && actual.front().find("Body ") != 0)
        {
            actual.erase(actual.begin());
        }
        EXPECT_EQ(rows, actual);
        const std::vector<std::string> marked =
            expected.highlighted < 0 ? std::vector<std::string>{}
                                     : std::vector<std::string>{m_rows[static_cast<std::size_t>(expected.highlighted)]};
        EXPECT_EQ(marked, highlighted_rows(page));
    }
    EXPECT_EQ(ArticleLine{scenario.next_row}, g_art_line_num);
    EXPECT_EQ(m_body_start + ArticlePosition{m_offsets[static_cast<std::size_t>(scenario.next_row)]}, g_art_pos);
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_EQ(ArticleLine{-1}, g_highlight);
    EXPECT_EQ(ArticlePosition{}, g_inner_search);
    EXPECT_FALSE(g_hide_everything);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_TRUE(m_article_command.empty());
}

INSTANTIATE_TEST_SUITE_P(
    SearchRedisplay, PagerSearchArticleTest,
    Combine(Values(PagerSearchCase{"CurrentPage", "gBody 002\n", {{0, 5}, {1, 5, 2}}, 6},
                   PagerSearchCase{"BeyondPage", "gBody 008\n", {{0, 5}, {7, 5, 8}}, 12},
                   PagerSearchCase{"RepeatSearch", "gBody 0[01]8\nG", {{0, 5}, {7, 5, 8}, {17, 5, 18}}, 22},
                   PagerSearchCase{"ContinueOneLine", "gBody 008\n\n", {{0, 5}, {7, 5, 8}, {12, 1}}, 13},
                   PagerSearchCase{"ContinueNextPage", "gBody 008\n ", {{0, 5}, {7, 5, 8}, {11, 5, 11}}, 16},
                   PagerSearchCase{"RefreshMatch", "gBody 008\n\f", {{0, 5}, {7, 5, 8}, {7, 5}}, 12},
                   PagerSearchCase{"BackThenForward", "gBody 008\nb ", {{0, 5}, {7, 5, 8}, {3, 5}, {7, 5, 7}}, 12},
                   PagerSearchCase{
                       "SearchAgainAfterBackingUp", "gBody 008\nbG", {{0, 5}, {7, 5, 8}, {3, 5}, {7, 5, 8}}, 12}),
            Values(PagerSearchText{"Newlines", "abcdefghijklmnopqrstuvw", "\n"},
                   PagerSearchText{"Unbroken", "abcdefghijklmnopqrstuvw", ""},
                   PagerSearchText{"Words", "alpha beta end", " ", 8},
                   PagerSearchText{"Utf8", "abcdefghijklmnopqrs\345\244\251e\314\201f", ""}),
            Values(STANDOUT, UNDERLINE)),
    pager_search_name);

INSTANTIATE_TEST_SUITE_P(
    MatchesAcrossDisplayRows, PagerSearchArticleTest,
    Combine(Values(PagerSearchCase{"StartsAtLastColumn", "g[wf]Body 008\n", {{0, 5}, {6, 5, 7}}, 11},
                   PagerSearchCase{"SpansTwoRows", "gBody 007.*Body 008\n", {{0, 5}, {6, 5, 7}}, 11}),
            Values(PagerSearchText{"Unbroken", "abcdefghijklmnopqrstuvw", ""},
                   PagerSearchText{"Utf8", "abcdefghijklmnopqrs\345\244\251e\314\201f", ""}),
            Values(STANDOUT, UNDERLINE)),
    pager_search_name);

TEST_P(PagerResumeArticleTest, redisplaysSavedRowsAndContinuesForward)
{
    const PagerResumeCase &scenario = std::get<0>(GetParam());

    read_pages(scenario.commands);

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_TO_END, m_result);
    bool visited_headers = false;
    for (std::size_t page = 0; page < scenario.pages.size(); ++page)
    {
        const PagerResumePage         &expected_page = scenario.pages[page];
        const std::vector<std::string> expected{m_rows.begin() + expected_page.first,
                                                m_rows.begin() + expected_page.first + expected_page.count};
        std::vector<std::string>       actual = displayed_rows(page);
        if (expected_page.headers)
        {
            while (!actual.empty() && actual.front().find("Body ") != 0)
            {
                actual.erase(actual.begin());
            }
        }
        EXPECT_EQ(expected, actual) << "Page " << page;
        EXPECT_EQ(expected_page.headers, page_text(page).find("Pager title") != std::string_view::npos);
        EXPECT_EQ(expected_page.headers, page_text(page).find("Subject:") != std::string_view::npos);
        visited_headers |= expected_page.headers;
    }
    std::size_t prompts = 0;
    for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
         position = m_rendered.find("--MORE--(", position + 1))
    {
        ++prompts;
    }
    EXPECT_EQ(scenario.pages.size(), prompts);
    const long header_lines = visited_headers ? 2 : 0;
    for (long row = 0; row <= scenario.next_row; ++row)
    {
        EXPECT_EQ(m_body_start + ArticlePosition{m_offsets[static_cast<std::size_t>(row)]},
                  virtual_read(ArticleLine{header_lines + row}))
            << "Row " << row;
    }
    EXPECT_EQ(ArticleLine{header_lines + scenario.next_row}, g_art_line_num);
    EXPECT_EQ(m_body_start + ArticlePosition{m_offsets[static_cast<std::size_t>(scenario.next_row)]}, g_art_pos);
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_TRUE(m_article_command.empty());
}

INSTANTIATE_TEST_SUITE_P(
    SavedPositions, PagerResumeArticleTest,
    Combine(Values(PagerResumeCase{"RefreshFirstBodyPage", "\f ", {{0, 5}, {0, 5}, {5, 4}}, 9},
                   PagerResumeCase{"RefreshContinuation", " \f ", {{0, 5}, {5, 4}, {4, 5}, {9, 4}}, 13},
                   PagerResumeCase{"RepeatedRefresh", " \fl ", {{0, 5}, {5, 4}, {4, 5}, {4, 5}, {9, 4}}, 13},
                   PagerResumeCase{"BackThenForward", "  b ", {{0, 5}, {5, 4}, {9, 4}, {4, 5}, {9, 4}}, 13},
                   PagerResumeCase{
                       "BackRefreshThenForward", "  b\f ", {{0, 5}, {5, 4}, {9, 4}, {4, 5}, {4, 5}, {9, 4}}, 13},
                   PagerResumeCase{"ClearingBackRefreshThenForward",
                                   "  b\f ",
                                   {{0, 5}, {4, 5}, {8, 5}, {4, 5}, {4, 5}, {8, 5}},
                                   13,
                                   false,
                                   true},
                   PagerResumeCase{
                       "BackToHeadersThenForward", "b\f  ", {{0, 5}, {0, 2, true}, {0, 2, true}, {2, 4}, {6, 4}}, 10},
                   PagerResumeCase{
                       "RefreshHeadersThenForward", "\f  ", {{0, 2, true}, {0, 2, true}, {2, 4}, {6, 4}}, 10, true},
                   PagerResumeCase{"CrossHeaderBoundaryAndReturn",
                                   " b\f ",
                                   {{0, 2, true}, {2, 4}, {0, 2, true}, {0, 2, true}, {2, 4}},
                                   6,
                                   true},
                   PagerResumeCase{"ClearingCrossHeaderBoundaryAndReturn",
                                   " b\f ",
                                   {{0, 2, true}, {1, 5}, {0, 2, true}, {0, 2, true}, {1, 5}},
                                   6,
                                   true,
                                   true}),
            Values(PagerResumeText::UNBROKEN, PagerResumeText::WORDS, PagerResumeText::UTF8)),
    [](const TestParamInfo<PagerResumeParam> &info)
    {
        const char *text = std::get<1>(info.param) == PagerResumeText::UNBROKEN ? "Unbroken"
                           : std::get<1>(info.param) == PagerResumeText::WORDS  ? "Words"
                                                                                : "Utf8";
        return std::string{std::get<0>(info.param).name} + text;
    });

TEST_P(PagerWrappingArticleTest, readsWrappedTextAcrossPagesWithoutLosingItsPosition)
{
    const PagerWrappingCase &scenario = std::get<0>(GetParam());
    const bool               overlap = std::get<1>(GetParam());

    read_pages(std::string(scenario.rows.size(), ' '));

    SCOPED_TRACE(m_rendered);
    EXPECT_EQ(DA_NORM, m_result);
    std::size_t page = 0;
    for (std::size_t first = 0; first < scenario.rows.size();)
    {
        const std::size_t count = std::min<std::size_t>(page == 0 || overlap ? 3 : 2, scenario.rows.size() - first);
        const std::vector<std::string> expected{scenario.rows.begin() + first, scenario.rows.begin() + first + count};
        EXPECT_EQ(expected, displayed_rows(page)) << "Page " << page;
        ++page;
        first += count;
        if (overlap && first < scenario.rows.size())
        {
            --first;
        }
    }
    std::size_t prompts = 0;
    for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
         position = m_rendered.find("--MORE--(", position + 1))
    {
        ++prompts;
    }
    EXPECT_EQ(page - 1, prompts);
    std::size_t source_position = 0;
    for (std::size_t row = 0; row < scenario.rows.size(); ++row)
    {
        const std::size_t offset = scenario.body.find(scenario.rows[row], source_position);
        ASSERT_NE(std::string_view::npos, offset);
        source_position = offset + scenario.rows[row].size();
        EXPECT_EQ(m_body_start + ArticlePosition{static_cast<long>(offset)},
                  virtual_read(ArticleLine{static_cast<long>(row)}))
            << "Row " << row;
    }
    EXPECT_EQ(ArticleLine{static_cast<long>(scenario.rows.size())}, g_art_line_num);
    EXPECT_EQ(m_body_start + ArticlePosition{static_cast<long>(scenario.body.size())}, g_art_pos);
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_EQ(g_art_pos, art_buf_article_pos());
}

INSTANTIATE_TEST_SUITE_P(
    TextAndTerminalModes, PagerWrappingArticleTest,
    Combine(Values(PagerWrappingCase{"Words",
                                     "one two three four five six seven eight nine ten eleven twelve thirteen "
                                     "fourteen fifteen sixteen seventeen eighteen nineteen twenty\nTail\n",
                                     {"one two three four five", "six seven eight nine ten", "eleven twelve thirteen",
                                      "fourteen fifteen sixteen", "seventeen eighteen", "nineteen twenty", "Tail"},
                                     32,
                                     8},
                   PagerWrappingCase{"UnbrokenLine",
                                     "0123456789abcdefghijklmnopqrstuvABCDEFGHIJKLMNOPQRSTUVWXYZ012345"
                                     "6789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZabcdefgh"
                                     "ijklmnopqrstuvwxyzABCDEFGHIJKLMN\nTail\n",
                                     {"0123456789abcdefghijklmnopqrstuv", "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345",
                                      "6789abcdefghijklmnopqrstuvwxyzAB", "CDEFGHIJKLMNOPQRSTUVWXYZabcdefgh",
                                      "ijklmnopqrstuvwxyzABCDEFGHIJKLMN", "Tail"},
                                     32,
                                     8},
                   PagerWrappingCase{"ExactWidthEndings",
                                     "abcdefghijklmnop\nABCDEFGHIJKLMNOP\n0123456789abcdef\nGHIJKLMNOPQRSTUV\n"
                                     "wxyz0123456789AB\nCDEFGHIJKLMNOPQR\nTail\n",
                                     {"abcdefghijklmnop", "ABCDEFGHIJKLMNOP", "0123456789abcdef", "GHIJKLMNOPQRSTUV",
                                      "wxyz0123456789AB", "CDEFGHIJKLMNOPQR", "Tail"},
                                     16},
                   PagerWrappingCase{"Utf8SingleColumn",
                                     "ab\303\251cd\316\251efgh\303\261ij\316\262klmn\303\266"
                                     "op\316\263qrst\303\274uv\316\264wxyz\nTail\n",
                                     {"ab\303\251cd\316\251ef", "gh\303\261ij\316\262kl", "mn\303\266op\316\263qr",
                                      "st\303\274uv\316\264wx", "yz", "Tail"},
                                     8},
                   PagerWrappingCase{"Utf8WideAtMargin",
                                     "abcdefg\345\244\251hijklmnopqrst\345\234\260uvwxyz\nTail\n",
                                     {"abcdefg", "\345\244\251hijklm", "nopqrst", "\345\234\260uvwxyz", "Tail"},
                                     8},
                   PagerWrappingCase{"Utf8CombiningAtMargin",
                                     "abcdefge\314\201HIJKLMNOPQRSTUVe\314\21012345678\nTail\n",
                                     {"abcdefge\314\201", "HIJKLMNO", "PQRSTUVe\314\210", "12345678", "Tail"},
                                     8}),
            Bool()),
    [](const TestParamInfo<PagerWrappingParam> &info)
    { return std::string{std::get<0>(info.param).name} + (std::get<1>(info.param) ? "Clearing" : "Scrolling"); });

TEST_P(PagerTerminalArticleTest, preservesColumnsStopsAndAttributesAcrossThePageBoundary)
{
    const PagerTerminalCase &scenario = GetParam();

    read_pages(scenario.commands);

    SCOPED_TRACE(m_rendered);
    const bool at_end = scenario.tail.empty();
    EXPECT_EQ(at_end ? DA_NORM : DA_TO_END, m_result);
    for (std::size_t page = 0; page < scenario.pages.size(); ++page)
    {
        std::vector<std::string> expected = scenario.pages[page];
        if (page == 0)
        {
            expected.insert(expected.begin(), {"One", "Two"});
        }
        EXPECT_EQ(expected, displayed_rows(page)) << "Page " << page;
        const std::string_view text = page_text(page);
        EXPECT_EQ(at_end ? std::string_view::npos : text.size() - std::string_view{"<mark>"}.size(),
                  text.find("<mark>"))
            << "Unexpected standout in body on page " << page;
        if (const std::size_t start = text.rfind("<ul>"); start != std::string_view::npos)
        {
            const std::size_t end = text.rfind("</ul>");
            EXPECT_NE(std::string_view::npos, end) << "Page " << page;
            EXPECT_GT(end, start) << "Page " << page;
        }
    }
    std::size_t prompts = 0;
    for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
         position = m_rendered.find("--MORE--(", position + 1))
    {
        ++prompts;
    }
    EXPECT_EQ(scenario.pages.size() - (at_end ? 1 : 0), prompts);
    EXPECT_EQ(m_body_start, virtual_read(ArticleLine{}));
    EXPECT_EQ(m_body_start + ArticlePosition{4}, virtual_read(ArticleLine{1}));
    for (std::size_t row = 0; row < scenario.offsets.size(); ++row)
    {
        const ArticlePosition expected = m_body_start + ArticlePosition{8 + scenario.offsets[row]};
        const bool            stop =
            std::find(scenario.stop_rows.begin(), scenario.stop_rows.end(), row) != scenario.stop_rows.end();
        EXPECT_EQ(stop ? -expected : expected, virtual_read(ArticleLine{static_cast<long>(row + 2)})) << "Row " << row;
    }
    EXPECT_EQ(ArticleLine{static_cast<long>(scenario.offsets.size() + 1)}, g_art_line_num);
    EXPECT_EQ(m_body_start + ArticlePosition{8 + scenario.offsets.back()}, g_art_pos);
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_GE(art_buf_article_pos(), g_art_pos);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_TRUE(m_article_command.empty());
    EXPECT_EQ(at_end, macro_pending());
    std::ifstream source{std::to_string(g_art.value_of()), std::ios::binary};
    ASSERT_TRUE(source.is_open());
    EXPECT_EQ(m_source, (std::string{std::istreambuf_iterator<char>{source}, std::istreambuf_iterator<char>{}}));
}

INSTANTIATE_TEST_SUITE_P(
    TextAndTerminalBoundaries, PagerTerminalArticleTest,
    Values(
        PagerTerminalCase{"ExpandedTabAtMargin", "abcdefg\tXYZ\n", {{"abcdefg "}, {"XYZ", "Tail"}}, {0, 8, 12, 17}},
        PagerTerminalCase{"HardwareTabAtMargin",
                          "abcdefg\tXYZ\n",
                          {{"abcdefg\t"}, {"XYZ", "Tail"}},
                          {0, 8, 12, 17},
                          PagerTerminalMode::HARDWARE_TABS},
        PagerTerminalCase{"TabToTheNextStop", "ab\tXYZ\n", {{"ab      "}, {"XYZ", "Tail"}}, {0, 3, 7, 12}},
        PagerTerminalCase{"CrLfBeforeMargin",
                          "1234567\r\nNEXT\r\n",
                          {{"1234567"}, {"NEXT", "Tail"}},
                          {0, 8, 13, 18},
                          PagerTerminalMode::CRLF},
        PagerTerminalCase{"CrLfAtMargin",
                          "12345678\r\nNEXT\r\n",
                          {{"12345678"}, {"NEXT", "Tail"}},
                          {0, 9, 14, 19},
                          PagerTerminalMode::CRLF},
        PagerTerminalCase{"LoneCarriageReturn", "ab\rcd\n", {{"ab^Mcd"}, {"Tail", "End"}}, {0, 6, 11, 15}},
        PagerTerminalCase{"ControlNeedsTwoColumns", "1234567\001Z\n", {{"1234567"}, {"^AZ", "Tail"}}, {0, 7, 10, 15}},
        PagerTerminalCase{"ControlFitsAtMargin", "123456\007Z\n", {{"123456^G"}, {"Z", "Tail"}}, {0, 7, 9, 14}},
        PagerTerminalCase{"DeleteIsFiltered", "ab\177cd\n", {{"ab^?cd"}, {"Tail", "End"}}, {0, 6, 11, 15}},
        PagerTerminalCase{"EscapeIsFiltered", "ab\033cd\n", {{"ab^[cd"}, {"Tail", "End"}}, {0, 6, 11, 15}},
        PagerTerminalCase{"ControlsRemainRawWithJ",
                          "1234567\001Z\n",
                          {{"1234567\001"}, {"Z", "Tail"}},
                          {0, 8, 10, 15},
                          PagerTerminalMode::UNFILTERED},
        PagerTerminalCase{"BellRemainsRawWithJ",
                          "1234567\007Z\n",
                          {{"1234567\007"}, {"Z", "Tail"}},
                          {0, 8, 10, 15},
                          PagerTerminalMode::UNFILTERED},
        PagerTerminalCase{"LiteralControlBIsNotAMimeSeparator",
                          "1234567\002Z\n",
                          {{"1234567\002"}, {"Z", "Tail"}},
                          {0, 8, 10, 15},
                          PagerTerminalMode::UNFILTERED},
        PagerTerminalCase{
            "FormFeedStopsThePage", "\fbreak\n", {{"^Lbreak"}}, {0, 7}, PagerTerminalMode::NORMAL, "", 6, {0}},
        PagerTerminalCase{"ContinueAfterFormFeed",
                          "\fbreak\n",
                          {{"^Lbreak"}, {"Tail", "End", "Last", "More", "Done"}},
                          {0, 7, 12, 16, 21, 26, 31},
                          PagerTerminalMode::NORMAL,
                          " ",
                          6,
                          {0},
                          "Tail\nEnd\nLast\nMore\nDone\nStay\n"},
        PagerTerminalCase{"InlineFormFeedDoesNotStop",
                          "ab\fcd\n",
                          {{"ab^Lcd", "Tail", "End"}},
                          {0, 6, 11, 15},
                          PagerTerminalMode::NORMAL,
                          "",
                          6},
        PagerTerminalCase{"FormFeedNeedsTwoColumns", "1234567\fZ\n", {{"1234567"}, {"^LZ", "Tail"}}, {0, 7, 10, 15}},
        PagerTerminalCase{"FormFeedFitsAtMargin", "123456\fZ\n", {{"123456^L"}, {"Z", "Tail"}}, {0, 7, 9, 14}},
        PagerTerminalCase{"UnderlineRunEndsBeforePlainText",
                          " _\bA_\bB cd\n",
                          {{" <ul>AB</ul> cd"}, {"Tail", "End"}},
                          {0, 11, 16, 20}},
        PagerTerminalCase{
            "LiteralUnderscoreEndsUnderline", "_\bA_B\n", {{"<ul>A</ul>_B"}, {"Tail", "End"}}, {0, 6, 11, 15}},
        PagerTerminalCase{"UnderlineEndsAtNewline", " _\bA_\bB\n", {{" <ul>AB</ul>"}, {"Tail", "End"}}, {0, 8, 13, 17}},
        PagerTerminalCase{"UnderlineAcrossPageBoundary",
                          "_\ba_\bb_\bc_\bd_\be_\bf_\bg_\bh_\bi_\bj\n",
                          {{"<ul>abcdefgh</ul>"}, {"<ul>ij</ul>", "Tail"}},
                          {0, 24, 31, 36}},
        PagerTerminalCase{"QuitDuringUnderlinedText",
                          "_\ba_\bb_\bc_\bd_\be_\bf_\bg_\bh_\bi_\bj\n",
                          {{"<ul>abcdefgh</ul>"}},
                          {0, 24},
                          PagerTerminalMode::NORMAL,
                          ""},
        PagerTerminalCase{"UnderlineAtExactWidthEof",
                          "_\ba_\bb_\bc_\bd_\be_\bf_\bg_\bh\n",
                          {{"<ul>abcdefgh</ul>"}},
                          {0, 25},
                          PagerTerminalMode::NORMAL,
                          "",
                          4,
                          {},
                          ""},
        PagerTerminalCase{"PerCharacterUnderline",
                          " _\bA_\bB cd\n",
                          {{" A<uc>B<uc> cd"}, {"Tail", "End"}},
                          {0, 11, 16, 20},
                          PagerTerminalMode::UNDERCHAR},
        PagerTerminalCase{"PerCharacterUnderlineAcrossPageBoundary",
                          "_\ba_\bb_\bc_\bd_\be_\bf_\bg_\bh_\bi_\bj\n",
                          {{"a<uc>b<uc>c<uc>d<uc>e<uc>f<uc>g<uc>h<uc>"}, {"i<uc>j<uc>", "Tail"}},
                          {0, 24, 31, 36},
                          PagerTerminalMode::UNDERCHAR},
        PagerTerminalCase{"UnderlineCookieConsumesSpaces",
                          " _\bA_\bB cd\n",
                          {{" <ul>AB</ul>cd"}, {"Tail", "End"}},
                          {0, 11, 16, 20},
                          PagerTerminalMode::UNDERLINE_GLITCH}),
    [](const TestParamInfo<PagerTerminalCase> &info) { return std::string{info.param.name}; });

TEST_P(PagerMovementArticleTest, displaysTheExpectedBodyLinesAndPosition)
{
    read_pages();

    SCOPED_TRACE(m_rendered);
    const PagerMovementCase &scenario = GetParam();
    const bool              at_end = scenario.next_line == scenario.body_lines;
    EXPECT_EQ(at_end ? DA_NORM : DA_TO_END, m_result);
    const long        first_page_lines = scenario.page_break_line < 0
                                             ? scenario.screen_lines - 1
                                             : std::min<long>(scenario.screen_lines - 1, scenario.page_break_line + 1);
    std::vector<long> first_page(static_cast<std::size_t>(first_page_lines));
    std::iota(first_page.begin(), first_page.end(), 0L);
    EXPECT_EQ(first_page, page_lines(0));
    for (std::size_t index = 0; index < scenario.pages.size(); ++index)
    {
        EXPECT_EQ(scenario.pages[index], page_lines(index + 1)) << "Page " << index + 1;
    }
    EXPECT_EQ(scenario.highlighted_lines, page_lines(scenario.pages.size(), true));
    if (!scenario.expected_text.empty())
    {
        EXPECT_THAT(page_text(scenario.pages.size()), HasSubstr(scenario.expected_text));
    }
    EXPECT_EQ(scenario.expected_rotate, g_rotate);
    EXPECT_EQ(scenario.expected_subst, current_char_subst_mode());
    int insertions = 0;
    for (std::size_t position = m_rendered.find("<insert>"); position != std::string::npos;
         position = m_rendered.find("<insert>", position + 8))
    {
        ++insertions;
    }
    EXPECT_EQ(scenario.insertions, insertions);
    int not_found_count = 0;
    for (std::size_t position = m_rendered.find("(Not found)"); position != std::string::npos;
         position = m_rendered.find("(Not found)", position + 11))
    {
        ++not_found_count;
    }
    EXPECT_EQ(scenario.not_found_count, not_found_count);
    EXPECT_EQ(ArticleLine{scenario.next_line + scenario.header_lines}, g_art_line_num);
    EXPECT_EQ(ArticleLine{scenario.top_line}, g_top_line);
    const long form_feeds = scenario.page_break_line >= 0 && scenario.page_break_line < scenario.next_line ? 1 : 0;
    long       prefix_bytes = 0;
    for (const PagerBodyPrefix &prefix : scenario.line_prefixes)
    {
        if (prefix.first < scenario.next_line)
        {
            prefix_bytes += static_cast<long>(prefix.second.size());
        }
    }
    EXPECT_EQ(m_body_start + ArticlePosition{9 * scenario.next_line + form_feeds + prefix_bytes}, g_art_pos);
    EXPECT_EQ(g_art_pos, art_buf_article_pos());
    const ArticlePosition saved_position = virtual_read(g_art_line_num);
    EXPECT_EQ(g_art_pos, saved_position < 0 ? -saved_position : saved_position);
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_EQ(ArticlePosition{}, g_inner_search);
    EXPECT_EQ(0, g_hide_everything);
    EXPECT_EQ(at_end, macro_pending());
}

INSTANTIATE_TEST_SUITE_P(
    CommandsAndDisplayModes, PagerMovementArticleTest,
    Values(
        PagerMovementCase{
            "ScrollingSpace", " ", 10, false, false, NO_MARKING, NONE, {{9, 10, 11, 12, 13, 14, 15, 16}}, 17, 8},
        PagerMovementCase{
            "ScrollingY", "y", 10, false, false, NO_MARKING, NONE, {{9, 10, 11, 12, 13, 14, 15, 16}}, 17, 8},
        PagerMovementCase{
            "ClearedPage", " ", 10, true, false, NO_MARKING, NONE, {{8, 9, 10, 11, 12, 13, 14, 15, 16}}, 17, 8},
        PagerMovementCase{
            "HomeAndErase", " ", 10, true, true, NO_MARKING, NONE, {{8, 9, 10, 11, 12, 13, 14, 15, 16}}, 17, 8},
        PagerMovementCase{"MarkedFullPage",
                          " ",
                          10,
                          false,
                          false,
                          STANDOUT,
                          NONE,
                          {{8, 9, 10, 11, 12, 13, 14, 15, 16}},
                          17,
                          8,
                          40,
                          0,
                          -1,
                          {8}},
        PagerMovementCase{"HalfPageD", "d", 10, false, false, NO_MARKING, NONE, {{9, 10, 11, 12}}, 13, 4},
        PagerMovementCase{"HalfPageControlD", "\004", 10, false, false, NO_MARKING, NONE, {{9, 10, 11, 12}}, 13, 4},
        PagerMovementCase{"MarkedHalfPage",
                          "d",
                          10,
                          false,
                          false,
                          STANDOUT,
                          HALF_PAGE_MARKING,
                          {{8, 9, 10, 11, 12}},
                          13,
                          4,
                          40,
                          0,
                          -1,
                          {8}},
        PagerMovementCase{
            "HalfPageMarkingDisabledD", "d", 10, false, false, STANDOUT, BACK_PAGE_MARKING, {{9, 10, 11, 12}}, 13, 4},
        PagerMovementCase{"HalfPageMarkingDisabledControlD",
                          "\004",
                          10,
                          false,
                          false,
                          STANDOUT,
                          BACK_PAGE_MARKING,
                          {{9, 10, 11, 12}},
                          13,
                          4},
        PagerMovementCase{"OddHeightHalfPage", "d", 9, false, false, NO_MARKING, NONE, {{8, 9, 10}}, 11, 3},
        PagerMovementCase{"RepeatedOddHeightHalfPages",
                          "dd",
                          9,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{8, 9, 10}, {11, 12, 13, 14}},
                          15,
                          7},
        PagerMovementCase{"SingleLineNewline", "\n", 10, false, false, NO_MARKING, NONE, {{9}}, 10, 1},
        PagerMovementCase{"SingleLineReturn", "\r", 10, false, false, NO_MARKING, NONE, {{9}}, 10, 1},
        PagerMovementCase{"FinalPartialPage", " ", 10, false, false, NO_MARKING, NONE, {{9, 10, 11, 12}}, 13, 4, 13}),
    [](const TestParamInfo<PagerMovementCase> &info) { return info.param.name; });

INSTANTIATE_TEST_SUITE_P(
    BackwardPaging, PagerMovementArticleTest,
    Values(PagerMovementCase{"PreviousPageB",
                             " b",
                             10,
                             false,
                             false,
                             NO_MARKING,
                             NONE,
                             {{9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
                             9,
                             0},
           PagerMovementCase{"PreviousPageControlB",
                             " \002",
                             10,
                             false,
                             false,
                             NO_MARKING,
                             NONE,
                             {{9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
                             9,
                             0},
           PagerMovementCase{"PreviousPageWithLineErasing",
                             " b",
                             10,
                             true,
                             true,
                             NO_MARKING,
                             NONE,
                             {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
                             9,
                             0},
           PagerMovementCase{"PreviousPageMarking",
                             " b",
                             10,
                             false,
                             false,
                             STANDOUT,
                             BACK_PAGE_MARKING,
                             {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
                             9,
                             0,
                             40,
                             0,
                             -1,
                             {8}},
           PagerMovementCase{"PreviousPageMarkingDisabled",
                             " b",
                             10,
                             false,
                             false,
                             STANDOUT,
                             HALF_PAGE_MARKING,
                             {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5, 6, 7, 8}},
                             9,
                             0},
           PagerMovementCase{"PreviousPageAfterHalfPage",
                             " db",
                             10,
                             false,
                             false,
                             NO_MARKING,
                             NONE,
                             {{9, 10, 11, 12, 13, 14, 15, 16}, {17, 18, 19, 20}, {4, 5, 6, 7, 8, 9, 10, 11, 12}},
                             13,
                             4},
           PagerMovementCase{"BeginningB", "b", 10, false, false, NO_MARKING, NONE, {{0, 1, 2, 3, 4, 5}}, 6, -1, 40, 2},
           PagerMovementCase{
               "BeginningControlB", "\002", 10, false, false, NO_MARKING, NONE, {{0, 1, 2, 3, 4, 5}}, 6, -1, 40, 2},
           PagerMovementCase{"RepeatedBackAtBeginning",
                             "bb",
                             10,
                             false,
                             false,
                             NO_MARKING,
                             NONE,
                             {{0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5}},
                             6,
                             -1,
                             40,
                             2},
           PagerMovementCase{"RepeatedBackPages",
                             "  bb",
                             10,
                             false,
                             false,
                             NO_MARKING,
                             NONE,
                             {{9, 10, 11, 12, 13, 14, 15, 16},
                              {17, 18, 19, 20, 21, 22, 23, 24},
                              {8, 9, 10, 11, 12, 13, 14, 15, 16},
                              {0, 1, 2, 3, 4, 5, 6, 7, 8}},
                             9,
                             0},
           PagerMovementCase{
               "ForwardAfterBack",
               " b ",
               10,
               false,
               false,
               NO_MARKING,
               NONE,
               {{9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5, 6, 7, 8}, {9, 10, 11, 12, 13, 14, 15, 16}},
               17,
               8},
           PagerMovementCase{"BackStopsAfterFormFeed",
                             "  b",
                             10,
                             false,
                             false,
                             NO_MARKING,
                             NONE,
                             {{8, 9, 10, 11, 12, 13, 14, 15, 16},
                              {17, 18, 19, 20, 21, 22, 23, 24},
                              {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                             17,
                             8,
                             40,
                             0,
                             7}),
    [](const TestParamInfo<PagerMovementCase> &info) { return info.param.name; });

INSTANTIATE_TEST_SUITE_P(BackwardSingleLine, PagerMovementArticleTest,
                         Values(with_line_insertion(PagerMovementCase{"InsertPreviousLine",
                                                                      " B",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{9, 10, 11, 12, 13, 14, 15, 16}, {7}},
                                                                      16,
                                                                      7}),
                                with_line_insertion(PagerMovementCase{"RepeatedInsertion",
                                                                      " BB",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{9, 10, 11, 12, 13, 14, 15, 16}, {7}, {6}},
                                                                      15,
                                                                      6},
                                                    2),
                                with_line_insertion(PagerMovementCase{
                                    "ForwardAfterInsertion",
                                    " B ",
                                    10,
                                    false,
                                    false,
                                    NO_MARKING,
                                    NONE,
                                    {{9, 10, 11, 12, 13, 14, 15, 16}, {7}, {16, 17, 18, 19, 20, 21, 22, 23}},
                                    24,
                                    15}),
                                with_line_insertion(PagerMovementCase{"LineBackDoesNotAddPageMarking",
                                                                      " B",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      STANDOUT,
                                                                      BACK_PAGE_MARKING,
                                                                      {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {7}},
                                                                      16,
                                                                      7}),
                                PagerMovementCase{"MissingInsertRedrawsThePage",
                                                  " B",
                                                  10,
                                                  false,
                                                  false,
                                                  NO_MARKING,
                                                  NONE,
                                                  {{9, 10, 11, 12, 13, 14, 15, 16}, {7, 8, 9, 10, 11, 12, 13, 14, 15}},
                                                  16,
                                                  7},
                                with_line_insertion(PagerMovementCase{"MissingHomeRedrawsThePage",
                                                                      " B",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{9, 10, 11, 12, 13, 14, 15, 16},
                                                                       {7, 8, 9, 10, 11, 12, 13, 14, 15}},
                                                                      16,
                                                                      7},
                                                    0, false),
                                with_line_insertion(PagerMovementCase{"BeginningIgnoresLineBack",
                                                                      "bB",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{0, 1, 2, 3, 4, 5}, {}},
                                                                      6,
                                                                      -1,
                                                                      40,
                                                                      2},
                                                    0),
                                with_line_insertion(PagerMovementCase{"ReachingTheHeaderRedrawsTheFirstPage",
                                                                      "B",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{0, 1, 2, 3, 4, 5}},
                                                                      6,
                                                                      -1,
                                                                      40,
                                                                      2}),
                                with_line_insertion(PagerMovementCase{"RepeatedInsertionCrossesAFormFeed",
                                                                      " BB",
                                                                      10,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {7}, {6}},
                                                                      15,
                                                                      6,
                                                                      40,
                                                                      0,
                                                                      7},
                                                    2),
                                with_line_insertion(PagerMovementCase{"FormFeedAtTheRestoredBottomPosition",
                                                                      "  B",
                                                                      5,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{4, 5, 6}, {7}, {3}},
                                                                      7,
                                                                      3,
                                                                      40,
                                                                      0,
                                                                      7}),
                                with_line_insertion(PagerMovementCase{"FormFeedBeforeTheRestoredBottomPosition",
                                                                      " \nB",
                                                                      6,
                                                                      false,
                                                                      false,
                                                                      NO_MARKING,
                                                                      NONE,
                                                                      {{5, 6, 7}, {8}, {3}},
                                                                      8,
                                                                      3,
                                                                      40,
                                                                      0,
                                                                      7})),
                         [](const TestParamInfo<PagerMovementCase> &info) { return info.param.name; });

INSTANTIATE_TEST_SUITE_P(
    InArticleSearch, PagerMovementArticleTest,
    Values(search_case("MatchOnTheCurrentPage", "g006\n", {{3, 4, 5, 6, 7, 8, 9, 10, 11}}, 12, 3, {6}),
           search_case("MatchAtTheCurrentBottom", "g008\n", {{5, 6, 7, 8, 9, 10, 11, 12, 13}}, 14, 5, {8}),
           search_case("MatchBeyondTheCurrentPage", "g012\n", {{9, 10, 11, 12, 13, 14, 15, 16, 17}}, 18, 9, {12}),
           search_case("CaseInsensitiveRegularExpression", "g ^b.*01[2]$\n", {{9, 10, 11, 12, 13, 14, 15, 16, 17}}, 18,
                       9, {12}),
           search_case("RepeatedSearchFindsTheNextMatch", "g0[12]2\nG",
                       {{9, 10, 11, 12, 13, 14, 15, 16, 17}, {19, 20, 21, 22, 23, 24, 25, 26, 27}}, 28, 19, {22}),
           search_case("RepeatedSearchPastTheLastMatch", "g032\nG", {{29, 30, 31, 32, 33, 34, 35, 36, 37}, {}}, 38, 29,
                       {}, 3, 1),
           search_case("MissingMatchPreservesPosition", "gmissing\n", {{}}, 9, 0, {}, 3, 1),
           search_case("ForwardPagingAfterAMissingMatch", "gmissing\n ", {{}, {8, 9, 10, 11, 12, 13, 14, 15, 16}}, 17,
                       8, {8}, 3, 1),
           search_case("MalformedPatternPreservesPosition", "g[\n", {{}}, 9, 0),
           search_case("ErasingTheSearchCommandCancelsIt", "g\b", {{}}, 9, 0),
           search_case("ZeroContextPutsTheMatchAtTheTop", "g012\n", {{12, 13, 14, 15, 16, 17, 18, 19, 20}}, 21, 12,
                       {12}, 0),
           search_case("ExcessiveContextIsClamped", "g012\n", {{4, 5, 6, 7, 8, 9, 10, 11, 12}}, 13, 4, {12}, 100),
           search_case("NegativeContextIsClamped", "g012\n", {{4, 5, 6, 7, 8, 9, 10, 11, 12}}, 13, 4, {12}, -1),
           with_body_prefix(search_case("DigestSubjectSearch", "\007", {{9, 10, 11, 12, 13, 14, 15, 16, 17}}, 18, 9,
                                        {12}),
                            12, 12, "Subject: "),
           with_body_prefix(with_body_prefix(search_case("RepeatedDigestSubjectSearch", "\007G",
                                                         {{9, 10, 11, 12, 13, 14, 15, 16, 17},
                                                          {21, 22, 23, 24, 25, 26, 27, 28, 29}},
                                                         30, 21, {24}),
                                             12, 12, "Subject: "),
                            24, 24, "Subject: "),
           with_body_prefix(search_case("TabSkipsTheQuotedBlock", "\t", {{9, 10, 11, 12, 13, 14, 15, 16, 17}}, 18, 9,
                                        {12}),
                            0, 11, "> "),
           search_case("TabWithoutAChangedPrefixPreservesPosition", "\t", {{}}, 9, 0, {}, 3, 1)),
    [](const TestParamInfo<PagerMovementCase> &info) { return info.param.name; });

INSTANTIATE_TEST_SUITE_P(
    RefreshCommands, PagerMovementArticleTest,
    Values(
        PagerMovementCase{"ControlLRedrawsTheCurrentPage",
                          " \f",
                          10,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                          17,
                          8},
        PagerMovementCase{"LowercaseLRedrawsTheCurrentPage",
                          " l",
                          10,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                          17,
                          8},
        PagerMovementCase{
            "RepeatedRefreshRetainsTheSamePage",
            " \fl",
            10,
            false,
            false,
            NO_MARKING,
            NONE,
            {{9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}},
            17,
            8},
        PagerMovementCase{
            "RefreshAtTheFirstBodyPage", "\f", 10, false, false, NO_MARKING, NONE, {{0, 1, 2, 3, 4, 5, 6, 7, 8}}, 9, 0},
        PagerMovementCase{"FirstArticlePageRefreshIncludesHeaders",
                          "b\f",
                          10,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5}},
                          6,
                          -1,
                          40,
                          2},
        PagerMovementCase{
            "PagingContinuesAfterRefresh",
            " \f ",
            10,
            false,
            false,
            NO_MARKING,
            NONE,
            {{9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}, {17, 18, 19, 20, 21, 22, 23, 24}},
            25,
            16},
        PagerMovementCase{"RefreshAfterGoingBack",
                          "  b\f",
                          10,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{9, 10, 11, 12, 13, 14, 15, 16},
                           {17, 18, 19, 20, 21, 22, 23, 24},
                           {8, 9, 10, 11, 12, 13, 14, 15, 16},
                           {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                          17,
                          8},
        PagerMovementCase{"RefreshOnClearingTerminal",
                          " \f",
                          10,
                          true,
                          true,
                          NO_MARKING,
                          NONE,
                          {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                          17,
                          8},
        PagerMovementCase{"RefreshAfterFormFeed",
                          " \f",
                          10,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                          17,
                          8,
                          40,
                          0,
                          7},
        PagerMovementCase{"RefreshFromPageStartingAtFormFeed",
                          " B\f",
                          10,
                          false,
                          false,
                          NO_MARKING,
                          NONE,
                          {{8, 9, 10, 11, 12, 13, 14, 15, 16}, {7}, {7}},
                          8,
                          7,
                          40,
                          0,
                          7},
        with_rotated_pages(PagerMovementCase{"Rot13RefreshKeepsTheCurrentPage",
                                             " X",
                                             10,
                                             false,
                                             false,
                                             NO_MARKING,
                                             NONE,
                                             {{9, 10, 11, 12, 13, 14, 15, 16}, {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                                             17,
                                             8},
                           {2}, true),
        with_rotated_pages(PagerMovementCase{"RefreshRetainsRot13Mode",
                                             " X\f",
                                             10,
                                             false,
                                             false,
                                             NO_MARKING,
                                             NONE,
                                             {{9, 10, 11, 12, 13, 14, 15, 16},
                                              {8, 9, 10, 11, 12, 13, 14, 15, 16},
                                              {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                                             17,
                                             8},
                           {2, 3}, true),
        with_rotated_pages(PagerMovementCase{"SecondRot13RefreshRestoresPlainText",
                                             " XX",
                                             10,
                                             false,
                                             false,
                                             NO_MARKING,
                                             NONE,
                                             {{9, 10, 11, 12, 13, 14, 15, 16},
                                              {8, 9, 10, 11, 12, 13, 14, 15, 16},
                                              {8, 9, 10, 11, 12, 13, 14, 15, 16}},
                                             17,
                                             8},
                           {2}, false),
        with_character_conversion(PagerMovementCase{"CharacterConversionRestartsTheArticle",
                                                    " _C",
                                                    10,
                                                    false,
                                                    false,
                                                    NO_MARKING,
                                                    NONE,
                                                    {{9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5}},
                                                    6,
                                                    -1},
                                  't', "\xC3\xA4 Body 005"),
        with_character_conversion(
            PagerMovementCase{"CharacterConversionRetainsModeOnRefresh",
                              " _C\f",
                              10,
                              false,
                              false,
                              NO_MARKING,
                              NONE,
                              {{9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5}},
                              6,
                              -1},
            't', "\xC3\xA4 Body 005"),
        with_character_conversion(
            PagerMovementCase{"CharacterConversionCyclesBackToPlain",
                              " _C_C",
                              10,
                              false,
                              false,
                              NO_MARKING,
                              NONE,
                              {{9, 10, 11, 12, 13, 14, 15, 16}, {0, 1, 2, 3, 4, 5}, {0, 1, 2, 3, 4, 5}},
                              6,
                              -1},
            'p', "\"a Body 005")),
    [](const TestParamInfo<PagerMovementCase> &info) { return info.param.name; });

TEST_P(PagerExitArticleTest, returnsTheCommandAndItsReadStateToTheArticleLoop)
{
    render_article();

    SCOPED_TRACE(m_rendered);
    const PagerExitCase &scenario = GetParam();
    EXPECT_EQ(scenario.result, m_result);
    EXPECT_EQ(scenario.command, m_article_command);
    EXPECT_EQ(scenario.pending, pending_input());
    EXPECT_EQ(scenario.expected_flags, g_artp->m_flags & (AF_UNREAD | AF_SEL));
    const int unread_change =
        ((scenario.initial_flags & AF_UNREAD) ? 1 : 0) - ((scenario.expected_flags & AF_UNREAD) ? 1 : 0);
    EXPECT_EQ(m_unread_before - ArticleUnread{unread_change}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(ArticleUnread{(scenario.expected_flags & AF_SEL) ? 1 : 0}, g_selected_count);
    EXPECT_EQ(scenario.checkpoints, g_check_count);
    EXPECT_EQ(scenario.reread, g_reread);
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_THAT(m_rendered, HasSubstr("--MORE--("));
    if (!scenario.message.empty())
    {
        EXPECT_THAT(m_rendered, HasSubstr(scenario.message));
    }
    if (!scenario.pages.empty())
    {
        for (std::size_t page = 0; page < scenario.pages.size(); ++page)
        {
            const PagerResumePage   &expected = scenario.pages[page];
            std::vector<std::string> rows = displayed_rows(page);
            rows.erase(
                std::remove_if(rows.begin(), rows.end(), [](const std::string &row) { return row.find("Body ") != 0; }),
                rows.end());
            EXPECT_EQ(numbered_pager_rows(expected.first, expected.first + expected.count), rows) << "Page " << page;
        }
        std::size_t prompts = 0;
        for (std::size_t position = m_rendered.find("--MORE--("); position != std::string::npos;
             position = m_rendered.find("--MORE--(", position + 1))
        {
            ++prompts;
        }
        EXPECT_EQ(scenario.pages.size() - (scenario.result == DA_NORM ? 1 : 0), prompts);
        EXPECT_EQ(ArticleLine{scenario.next_row}, g_art_line_num);
        EXPECT_EQ(ArticleLine{std::max(0L, scenario.next_row - 4)}, g_top_line);
        for (long row = 0; row <= scenario.next_row; ++row)
        {
            EXPECT_EQ(m_body_start + ArticlePosition{row * 9}, virtual_read(ArticleLine{row})) << "Row " << row;
        }
        EXPECT_EQ(virtual_read(g_art_line_num), g_art_pos);
        EXPECT_FALSE(g_do_fseek);
        EXPECT_FALSE(g_in_header);
    }
}

INSTANTIATE_TEST_SUITE_P(
    ArticleLevelCommands, PagerExitArticleTest,
    Values(raised_pager_command("NextUnread", "n", true), raised_pager_command("NextArticle", "N", true),
           raised_pager_command("NextSubject", "\016", true), raised_pager_command("PreviousUnread", "p"),
           raised_pager_command("PreviousArticle", "P"), raised_pager_command("PreviousSubject", "\020"),
           raised_pager_command("ReplyWithArticle", "R", true), raised_pager_command("Reply", "r"),
           raised_pager_command("ForwardWithArticle", "F", true), raised_pager_command("Followup", "f"),
           raised_pager_command("SaveWithHeaders", "s", true), raised_pager_command("SaveWithoutHeaders", "w", true),
           raised_pager_command("Extract", "e", true), raised_pager_command("Unsubscribe", "u", true),
           raised_pager_command("KillSubject", "k", true), raised_pager_command("KillThread", "J", true),
           raised_pager_command("SelectSubject", "a", true), raised_pager_command("ArticleSelector", "+"),
           raised_pager_command("MarkUnread", "m"), raised_pager_command("DelayUnread", "M"),
           raised_pager_command("Junk", "j"), raised_pager_command("GoToBeginning", "^"),
           raised_pager_command("QuitReading", "Q"),
           PagerExitCase{"AlreadyReadNext", "n", std::string{'n', static_cast<char>(FINISH_CMD)}, AF_NONE, AF_NONE, 1},
           PagerExitCase{"SelectedNext", "n", std::string{'n', static_cast<char>(FINISH_CMD)}, AF_UNREAD | AF_SEL,
                         AF_NONE, 1},
           PagerExitCase{"SelectedReply", "r", std::string{'r', static_cast<char>(FINISH_CMD)}, AF_UNREAD | AF_SEL,
                         AF_UNREAD | AF_SEL},
           PagerExitCase{"SaveLeavesItsArgumentsQueued", "s saved.article\n",
                         std::string{'s', static_cast<char>(FINISH_CMD)}, AF_UNREAD, AF_NONE, 1, DA_RAISE, false,
                         " saved.article\nq"},
           PagerExitCase{"SearchLeavesItsPatternQueued", "/needle\n", std::string{'/', static_cast<char>(FINISH_CMD)},
                         AF_UNREAD, AF_UNREAD, 0, DA_RAISE, false, "needle\nq"},
           PagerExitCase{"NumericJumpLeavesItsDigitsQueued", "123\n", std::string{'1', static_cast<char>(FINISH_CMD)},
                         AF_UNREAD, AF_UNREAD, 0, DA_RAISE, false, "23\nq"},
           PagerExitCase{"SelectorModeRetainsTheCompletedCommand", "_a", "_a"},
           PagerExitCase{"ThreadSelectorRetainsTheCompletedCommand", "_t", "_t"},
           PagerExitCase{"QuitOnlyThePager", "q", "", AF_UNREAD, AF_UNREAD, 0, DA_TO_END, true},
           PagerExitCase{"ErasingSelectorPrefixStaysInPager", "_\b", "", AF_UNREAD, AF_UNREAD, 0, DA_TO_END, true, ""}),
    [](const TestParamInfo<PagerExitCase> &info) { return info.param.name; });

INSTANTIATE_TEST_SUITE_P(
    PagerInteractions, PagerExitArticleTest,
    Values(after_pager_interaction(raised_pager_command("CanceledSearchThenNext", "n", true), "g\b", {{0, 4}, {0, 0}},
                                   4),
           after_pager_interaction(raised_pager_command("MalformedSearchThenReply", "r"), "g[\n", {{0, 4}, {0, 0}}, 4,
                                   "Missing ]"),
           after_pager_interaction(raised_pager_command("MissedSearchThenNext", "n", true), "gnosuchline\n",
                                   {{0, 4}, {0, 0}}, 4, "(Not found)"),
           after_pager_interaction(raised_pager_command("CanceledSelectorThenNext", "n", true), "_\b", {{0, 4}, {0, 0}},
                                   4),
           after_pager_interaction(PagerExitCase{"KilledSelectorThenSelect", "_a", "_a"}, "_\025", {{0, 4}, {0, 0}}, 4),
           after_pager_interaction(raised_pager_command("ContinueThenNext", "n", true), " ", {{0, 4}, {4, 3}}, 7),
           after_pager_interaction(PagerExitCase{"ContinueThenQuit", "q", "", AF_UNREAD, AF_UNREAD, 0, DA_TO_END, true},
                                   " ", {{0, 4}, {4, 3}}, 7),
           after_pager_interaction(PagerExitCase{"LineThenSelectedReply", "r",
                                                 std::string{'r', static_cast<char>(FINISH_CMD)}, AF_UNREAD | AF_SEL,
                                                 AF_UNREAD | AF_SEL},
                                   "\n", {{0, 4}, {4, 1}}, 5),
           after_pager_interaction(PagerExitCase{"RefreshThenRange", "3,5:s\n",
                                                 std::string{'3', static_cast<char>(FINISH_CMD)}, AF_UNREAD, AF_UNREAD,
                                                 0, DA_RAISE, false, ",5:s\nq"},
                                   "\f", {{0, 4}, {0, 4}}, 4),
           after_pager_interaction(PagerExitCase{"BackThenSearch", "/needle\n",
                                                 std::string{'/', static_cast<char>(FINISH_CMD)}, AF_UNREAD, AF_UNREAD,
                                                 0, DA_RAISE, false, "needle\nq"},
                                   " b", {{0, 4}, {4, 3}, {0, 4}}, 4),
           after_pager_interaction(PagerExitCase{"ContinueThenSelector", "_an", "_a", AF_UNREAD, AF_UNREAD, 0, DA_RAISE,
                                                 false, "nq"},
                                   " ", {{0, 4}, {4, 3}}, 7),
           after_pager_interaction(PagerExitCase{"MalformedSearchThenContinue", "q", "", AF_UNREAD, AF_UNREAD, 0,
                                                 DA_TO_END, true},
                                   "g[\n ", {{0, 4}, {0, 0}, {4, 3}}, 7, "Missing ]"),
           after_pager_interaction(raised_pager_command("HelpThenMarkUnread", "m"), "Hq", {{0, 4}, {0, 0}}, 4,
                                   "Paging commands:"),
           after_pager_interaction(PagerExitCase{"EofRetainsSelectionAndNextCommand", "   n", "", AF_UNREAD | AF_SEL,
                                                 AF_UNREAD | AF_SEL, 0, DA_NORM, true, "nq"},
                                   "", {{0, 4}, {4, 3}, {7, 3}, {10, 2}}, 12)),
    [](const TestParamInfo<PagerExitCase> &info) { return info.param.name; });

TEST_P(PagerTreeArticleTest, displaysTheThreadAndResumesTheSameArticleBody)
{
    read_with_tree();

    SCOPED_TRACE(m_rendered);
    const PagerTreeCase &scenario = GetParam();
    EXPECT_EQ(DA_TO_END, m_result);
    EXPECT_TRUE(m_article_command.empty());
    std::vector<long> expected(static_cast<std::size_t>(scenario.next_line));
    std::iota(expected.begin(), expected.end(), 0L);
    EXPECT_EQ(expected, displayed_body_lines());
    int displays = 0;
    for (std::size_t position = m_rendered.find("[1] Pager thread"); position != std::string::npos;
         position = m_rendered.find("[1] Pager thread", position + 16))
    {
        ++displays;
    }
    EXPECT_EQ(scenario.displays, displays);
    EXPECT_THAT(m_rendered, HasSubstr("<mark>[1]</mark>"));
    EXPECT_EQ(!scenario.threaded, m_rendered.find("Threading the group.") != std::string::npos);
    EXPECT_TRUE(g_threaded_group);
    EXPECT_TRUE(g_artp->m_flags & AF_THREADED);
    EXPECT_EQ(article_ptr(g_last_art), g_artp->m_child1);
    EXPECT_EQ(g_artp, article_ptr(g_last_art)->m_parent);
    EXPECT_TRUE(g_artp->m_flags & AF_UNREAD);
    EXPECT_TRUE(article_ptr(g_last_art)->m_flags & AF_UNREAD);
    EXPECT_EQ(ArticleUnread{2}, g_newsgroup_ptr->m_to_read);
    EXPECT_EQ(ArticleUnread{}, g_selected_count);
    EXPECT_EQ(ArticleNum{TRN_TEST_ARTICLE_NUM}, g_art);
    EXPECT_EQ(g_art, g_curr_art);
    EXPECT_EQ(g_art, g_open_art);
    EXPECT_EQ(g_art, g_parsed_art);
    EXPECT_EQ(article_ptr(g_art), g_artp);
    EXPECT_EQ(g_artp, g_curr_artp);
    EXPECT_EQ(ArticleLine{scenario.next_line}, g_art_line_num);
    EXPECT_EQ(m_body_start + ArticlePosition{9 * scenario.next_line}, g_art_pos);
    EXPECT_EQ(g_art_pos, art_buf_article_pos());
    EXPECT_EQ(g_art_pos, virtual_read(g_art_line_num));
    EXPECT_FALSE(art_buf_empty());
    EXPECT_EQ(m_raw_positions[static_cast<std::size_t>(scenario.next_line + 1)], tell_art());
    EXPECT_EQ(scenario.quoted_printable ? TEXT_MIME : NOT_MIME, g_mime_state);
    EXPECT_EQ(scenario.quoted_printable ? MENCODE_QPRINT : MENCODE_NONE, g_mime_section->m_encoding);
    EXPECT_EQ(scenario.quoted_printable, g_is_mime);
    EXPECT_TRUE(g_mime_section->m_decoder_pending.empty());
    EXPECT_EQ(GM_READ, g_general_mode);
    EXPECT_EQ(MM_ARTICLE, g_mode);
    EXPECT_FALSE(g_do_fseek);
    EXPECT_FALSE(g_in_header);
    EXPECT_FALSE(macro_pending());
}

INSTANTIATE_TEST_SUITE_P(ThreadingAndBodyEncoding, PagerTreeArticleTest,
                         Values(PagerTreeCase{"ThreadedPlainText", true, false, "t ", 17},
                                PagerTreeCase{"UnthreadedPlainText", false, false, "t ", 17},
                                PagerTreeCase{"ThreadedQuotedPrintable", true, true, "t ", 17},
                                PagerTreeCase{"UnthreadedQuotedPrintable", false, true, "t ", 17},
                                PagerTreeCase{"UnthreadedSingleLineContinuation", false, true, "t\n", 10},
                                PagerTreeCase{"RepeatedTreeDisplay", false, true, "tt ", 17, 2},
                                PagerTreeCase{"ThreadedTreeRetainsTheBufferedPage", true, true, "t", 9},
                                PagerTreeCase{"UnthreadedTreeRetainsTheBufferedPage", false, true, "t", 9}),
                         [](const TestParamInfo<PagerTreeCase> &info) { return info.param.name; });

TEST_F(InterpolatorNewsgroupTest, displaysDateUsingLocalTimeFormatOverride)
{
    ValueSaver<int> mouse_bar_count(g_mouse_bar_cnt, 0);
    g_header_type[DATE_LINE].flags |= HT_MAGIC;
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_env("LOCALTIMEFMT", "%Y");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_THAT(output, HasSubstr("Date: 2003\n"));
}

TEST_F(InterpolatorNewsgroupTest, hidesSingleNewsgroupHeaderInArticleHeader)
{
    ValueSaver<int>         mouse_bar_count(g_mouse_bar_cnt, 0);
    ValueSaver<std::string> newsgroup_name(g_newsgroup_name, TRN_TEST_NEWSGROUP);
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};
    g_header_type[NEWSGROUPS_LINE].flags |= HT_MAGIC;
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_THAT(output, HasSubstr(TRN_TEST_BODY));
    EXPECT_EQ(std::string::npos, output.find("Newsgroups: " TRN_TEST_NEWSGROUP));
}

TEST_F(InterpolatorNewsgroupTest, marksCitedArticleBodyLine)
{
    const std::string output_path = m_output.path();
    const fs::path    article_file = fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM);
    std::ofstream{article_file} << "Path: " TRN_TEST_HEADER_PATH "\n"
                                << "From: " TRN_TEST_HEADER_FROM "\n"
                                << "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                << "Article: " << TRN_TEST_ARTICLE_NUM << "\n"
                                << "Subject: " TRN_TEST_HEADER_SUBJECT "\n"
                                << "Date: " TRN_TEST_HEADER_DATE "\n"
                                << "Message-Id: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                << "Lines: 2\n\n"
                                << "> cited text\n"
                                << "plain text\n";
    struct CitedTextAttribute
    {
        CitedTextAttribute()
        {
            color_rc_attribute("cited text", "s");
        }
        ~CitedTextAttribute()
        {
            color_rc_attribute("cited text", "-");
            color_default();
        }
    } cited_text_attribute;
    PushDir                  output_dir{output_path};
    ValueSaver<int>          mouse_bar_count(g_mouse_bar_cnt, 0);
    ValueSaver<std::string_view> standout_start(g_tc_SO, "<so>");
    ValueSaver<std::string_view> standout_end(g_tc_SE, "<se>");
    ValueSaver<std::string>  group_dir(g_newsgroup_dir, ".");
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_THAT(output, HasSubstr("<so>> cited text\n<se>plain text\n"));
}

TEST_F(InterpolatorNewsgroupTest, rotatesArticleBodyText)
{
    const std::string output_path = m_output.path();
    const fs::path    article_file = fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM);
    write_article_body(article_file, "uryyb Jbeyq\n");
    PushDir                 output_dir{output_path};
    ValueSaver<bool>        rotate(g_rotate, true);
    ValueSaver<std::string> group_dir(g_newsgroup_dir, ".");
    ValueSaver<int>         mouse_bar_count(g_mouse_bar_cnt, 0);
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_THAT(output, HasSubstr("hello World\n"));
}

TEST_F(InterpolatorNewsgroupTest, rendersBackspaceUnderlineBodyText)
{
    const std::string output_path = m_output.path();
    const fs::path    article_file = fs::path{output_path} / std::to_string(TRN_TEST_ARTICLE_NUM);
    write_article_body(article_file, "_\bU text\n");
    PushDir                  output_dir{output_path};
    ValueSaver<std::string_view> underline_start(g_tc_US, "<ul>");
    ValueSaver<std::string_view> underline_end(g_tc_UE, "</ul>");
    ValueSaver<std::string_view> underchar(g_tc_UC, "");
    ValueSaver<bool>         underline_glitch(g_tc_UG, false);
    ValueSaver<std::string>  group_dir(g_newsgroup_dir, ".");
    ValueSaver<int>          mouse_bar_count(g_mouse_bar_cnt, 0);
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_THAT(output, HasSubstr("<ul>U</ul> text\n"));
}

TEST_F(InterpolatorNewsgroupTest, displaysInterpolatedFirstLine)
{
    ValueSaver<int>         mouse_bar_count(g_mouse_bar_cnt, 0);
    ValueSaver<std::string> first_line(g_first_line, std::string{"X-First: article %a"});
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_THAT(output, HasSubstr("X-First: article " + std::to_string(TRN_TEST_ARTICLE_NUM)));
}

TEST_F(InterpolatorNewsgroupTest, articleSearchCurrentSubjectCommandCompletes)
{
    g_artp = article_ptr(g_art);

    EXPECT_EQ(SRCH_DONE, art_search("k", false));
}

TEST_F(InterpolatorNewsgroupTest, articleSearchCurrentAuthorCommandCompletes)
{
    g_artp = article_ptr(g_art);

    EXPECT_EQ(SRCH_DONE, art_search("kf", false));
}

TEST_F(InterpolatorNewsgroupTest, pagerPromptInterpolatesMailCallWithPagerCommand)
{
    ValueSaver<int>         mouse_bar_count(g_mouse_bar_cnt, 0);
    ValueSaver<std::string> mail_call(g_mail_call, std::string{"<%/> "});
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{2};
    g_tc_LINES = 4;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    ASSERT_TRUE(parse_header(g_art));
    push_char('?');
    std::string article_command;

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article(article_command);
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_RAISE, result);
    ASSERT_EQ(2, article_command.size());
    EXPECT_EQ('?', article_command[0]);
    EXPECT_EQ(static_cast<char>(FINISH_CMD), article_command[1]);
    EXPECT_THAT(output, HasSubstr("<needle?> End of article "));
}

TEST_F(InterpolatorNewsgroupTest, pagerPromptSkipsMailCallForNextArticleCommand)
{
    ValueSaver<int>         mouse_bar_count(g_mouse_bar_cnt, 0);
    ValueSaver<std::string> mail_call(g_mail_call, std::string{"<%/> "});
    g_last_pat = "needle";
    g_art_do_read = false;
    g_art_how_much = ARTSCOPE_SUBJECT;
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{2};
    g_tc_LINES = 4;
    g_tc_COLS = 80;
    reset_char_subst_mode();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    ASSERT_TRUE(parse_header(g_art));
    push_char('n');
    std::string article_command;

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article(article_command);
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_RAISE, result);
    ASSERT_EQ(2, article_command.size());
    EXPECT_EQ('n', article_command[0]);
    EXPECT_EQ(static_cast<char>(FINISH_CMD), article_command[1]);
    EXPECT_EQ(std::string::npos, output.find("<needle?> End of article "));
}

TEST_F(InterpolatorNewsgroupTest, oldDistributionLineInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%D");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_DISTRIBUTION, buffer());
}

TEST_F(InterpolatorNewsgroupTest, fromLineInNewsgroupNoReplyTo)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};

    const std::string_view new_pattern = interpolate("%f");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM, buffer());
}

TEST_F(InterpolatorNewsgroupTest, fromLineInNewsgroupWithReplyTo)
{
    const std::string_view new_pattern = interpolate("%f");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_REPLY_TO, buffer());
}

TEST_F(InterpolatorNewsgroupTest, followupInNewsgroupWithFollowupToLine)
{
    const std::string_view new_pattern = interpolate("%F");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FOLLOWUP_TO, buffer());
}

TEST_F(InterpolatorNewsgroupTest, followupInNewsgroupFromNewsgroupsLine)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};

    const std::string_view new_pattern = interpolate("%F");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_NEWSGROUP, buffer());
}

TEST_F(InterpolatorNewsgroupTest, messageIdInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%i");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_MESSAGE_ID, buffer());
}

TEST_F(InterpolatorNewsgroupTest, messageIdAddsAngleBrackets)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_BARE_MESSAGE_ID_NUM};

    const std::string_view new_pattern = interpolate("%i");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::string{"<"} + TRN_TEST_HEADER_BARE_MESSAGE_ID + ">", buffer());
}

TEST_F(InterpolatorNewsgroupTest, newsgroupsLineInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%n");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_NEWSGROUPS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, lastReferenceInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%r");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_LAST_REFERENCE, buffer());
}

TEST_F(InterpolatorNewsgroupTest, lastReferenceInNewsgroupNoArticleIsEmpty)
{
    g_art = ArticleNum{};

    const std::string_view new_pattern = interpolate("%r");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST(NormalizeReferencesTest, normalize)
{
    ASSERT_EQ(TRN_TEST_HEADER_REFERENCES, normalize_refs(TRN_TEST_HEADER_REFERENCES));
}

TEST_F(InterpolatorNewsgroupTest, newReferencesInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%R");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_REFERENCES " " TRN_TEST_HEADER_MESSAGE_ID, buffer());
}

TEST_F(InterpolatorNewsgroupTest, newReferencesAddsMessageIdAngleBrackets)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_BARE_MESSAGE_ID_NUM};

    const std::string_view new_pattern = interpolate("%R");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::string{TRN_TEST_HEADER_REFERENCES} + " <" + TRN_TEST_HEADER_BARE_MESSAGE_ID + ">", buffer());
}

TEST_F(InterpolatorNewsgroupTest, strippedSubjectInNewsgroupNoArticleIsEmpty)
{
    g_art = ArticleNum{};

    const std::string_view new_pattern = interpolate("%s");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}

TEST_F(InterpolatorNewsgroupTest, strippedSubjectInNewsgroup)
{
    g_artp = article_ptr(g_art);

    const std::string_view new_pattern = interpolate("%s");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_STRIPPED_SUBJECT, buffer());
}

TEST_F(InterpolatorNewsgroupTest, strippedSubjectDropsNotesFileSuffix)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NOTES_FILE_SUBJECT_NUM};
    g_artp = article_ptr(g_art);

    const std::string_view new_pattern = interpolate("%s");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::string{TRN_TEST_HEADER_STRIPPED_SUBJECT} + " ", buffer());
}

TEST_F(InterpolatorNewsgroupTest, oneReStrippedSubjectInNewsgroup)
{
    g_artp = article_ptr(g_art);

    const std::string_view new_pattern = interpolate("%S");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_ONE_RE_SUBJECT, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromReplyToInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%t");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_REPLY_TO_ADDRESS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromFromInNewsgroup)
{
    g_art = ArticleNum{TRN_TEST_ARTICLE_NO_FALLBACKS_NUM};

    const std::string_view new_pattern = interpolate("%t");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM_ADDRESS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromPathInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%T");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_PATH, buffer());
}

TEST_F(InterpolatorNewsgroupTest, toFromPathTrimsPostingHostPrefix)
{
    g_p_host_name = "foo";

    const std::string_view new_pattern = interpolate("%T");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("bar!goink!not-for-mail", buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnreadArticlesInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%u");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(TRN_TEST_NEWSGROUP_HIGH - TRN_TEST_NEWSGROUP_LOW + 1), buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnreadArticlesExceptCurrentInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%U");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(TRN_TEST_NEWSGROUP_HIGH - TRN_TEST_NEWSGROUP_LOW), buffer());
}

TEST_F(InterpolatorNewsgroupTest, numUnselectedArticlesExceptCurrentInNewsgroupEmpty)
{
    const std::string_view new_pattern = interpolate("%v");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(std::to_string(TRN_TEST_NEWSGROUP_HIGH - TRN_TEST_NEWSGROUP_LOW), buffer());
}

TEST_F(InterpolatorNewsgroupTest, shortenedFromInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%y");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_FROM, buffer());
}

TEST_F(InterpolatorNewsgroupTest, shortenedFromShortensMultiPartDomain)
{
    article_ptr(g_art)->set_cached_line(FROM_LINE, "casey@host.news.example.test (Casey)");
    g_parsed_art = ArticleNum{};

    const std::string_view new_pattern = interpolate("%y");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("casey@*.example.test (Casey)", buffer());
}

TEST_F(InterpolatorNewsgroupTest, articleSizeInNewsgroup)
{
    std::ostringstream str;
    str << std::setw(5) << TRN_TEST_ARTICLE_SIZE;

    const std::string_view new_pattern = interpolate("%z");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(str.str(), buffer());
}

TEST_F(InterpolatorNewsgroupTest, numSelectedThreadsInNewsgroupEmpty)
{
    ValueSaver<ArticleUnread> saver(g_selected_count, 66);

    const std::string_view new_pattern = interpolate("%Z");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ("66", buffer());
}

TEST_F(InterpolatorNewsgroupTest, headerFieldInNewsgroup)
{
    const std::string_view new_pattern = interpolate("%[X-Boogie-Nights]");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_EQ(TRN_TEST_HEADER_X_BOOGIE_NIGHTS, buffer());
}

TEST_F(InterpolatorNewsgroupTest, missingHeaderFieldInNewsgroupIsEmpty)
{
    const std::string_view new_pattern = interpolate("%[X-Missing-Header]");

    ASSERT_TRUE(new_pattern.empty());
    ASSERT_TRUE(bufferIsEmpty());
}
