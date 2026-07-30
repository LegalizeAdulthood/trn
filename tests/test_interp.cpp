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
#include <trn/rthread.h>
#include <trn/search.h>
#include <trn/terminal.h>
#include <trn/trn.h>
#include <trn/univ.h>
#include <trn/util.h>
#include <util/env-internal.h>
#include <util/util2.h>

#include <mock_env.h>
#include <test_config.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>

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
    std::array<char, TCBUF_SIZE> m_tcbuf{};
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
    opt_init(1,argv,m_tcbuf.data());
    color_init();
    interp_init();
    cwd_check();
    term_set(m_tcbuf.data());
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
#if HOST_BITS != 0
    ASSERT_EQ("example.org", g_host_name);
#else
    ASSERT_EQ(TRN_TEST_P_HOST_NAME, g_host_name);
#endif
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
    EXPECT_EQ(0, do_shell(SH, "test \"$QUOTECHARS\" = '>'"));
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

void write_article_body(const fs::path &article_file, std::string_view body)
{
    std::ofstream{article_file} << "Path: " TRN_TEST_HEADER_PATH "\n"
                                << "From: " TRN_TEST_HEADER_FROM "\n"
                                << "Newsgroups: " TRN_TEST_HEADER_NEWSGROUPS "\n"
                                << "Article: " << TRN_TEST_ARTICLE_NUM << "\n"
                                << "Subject: " TRN_TEST_HEADER_SUBJECT "\n"
                                << "Date: " TRN_TEST_HEADER_DATE "\n"
                                << "Message-Id: " TRN_TEST_HEADER_MESSAGE_ID "\n"
                                << "Lines: 1\n\n"
                                << body;
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
    ValueSaver<const char *> char_subst(g_char_subst, g_charsets.c_str());
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
    EXPECT_THAT(output, HasSubstr("- "));
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
    ValueSaver<const char *> char_subst(g_char_subst, g_charsets.c_str());
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

TEST_F(InterpolatorNewsgroupTest, displaysFromNameInArticleHeader)
{
    ValueSaver<int> mouse_bar_count(g_mouse_bar_cnt, 0);
    g_header_type[FROM_LINE].flags |= HT_MAGIC;
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    g_char_subst = g_charsets.c_str();
    g_curr_artp = article_ptr(g_art);
    g_artp = g_curr_artp;
    m_env.expect_no_envar("LOCALTIMEFMT");
    ASSERT_TRUE(parse_header(g_art));

    testing::internal::CaptureStdout();
    const DoArticleResult result = do_article();
    const std::string     output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(DA_NORM, result);
    EXPECT_NE(std::string::npos, output.find("From: " TRN_TEST_HEADER_FROM_NAME));
    EXPECT_EQ(std::string::npos, output.find("From: " TRN_TEST_HEADER_FROM));
    EXPECT_EQ(TRN_TEST_HEADER_FROM, fetch_lines(g_art, FROM_LINE));
    EXPECT_EQ("%sEnd of article " + std::to_string(TRN_TEST_ARTICLE_NUM) + " (of " +
                  std::to_string(TRN_TEST_NEWSGROUP_HIGH) + ") %s-- what next? [%s]",
              g_prompt);
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
    g_char_subst = g_charsets.c_str();
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
    ValueSaver<const char *> standout_start(g_tc_SO, "<so>");
    ValueSaver<const char *> standout_end(g_tc_SE, "<se>");
    ValueSaver<std::string>  group_dir(g_newsgroup_dir, ".");
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    g_char_subst = g_charsets.c_str();
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
    g_char_subst = g_charsets.c_str();
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
    ValueSaver<const char *> underline_start(g_tc_US, "<ul>");
    ValueSaver<const char *> underline_end(g_tc_UE, "</ul>");
    ValueSaver<const char *> underchar(g_tc_UC, "");
    ValueSaver<bool>         underline_glitch(g_tc_UG, false);
    ValueSaver<std::string>  group_dir(g_newsgroup_dir, ".");
    ValueSaver<int>          mouse_bar_count(g_mouse_bar_cnt, 0);
    g_top_line = ArticleLine{-1};
    g_init_lines = ArticleLine{30000};
    g_tc_LINES = 30000;
    g_tc_COLS = 80;
    g_char_subst = g_charsets.c_str();
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
    g_char_subst = g_charsets.c_str();
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
    g_char_subst = g_charsets.c_str();
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
    g_char_subst = g_charsets.c_str();
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
