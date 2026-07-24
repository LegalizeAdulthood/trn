// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>

#include <config/common.h>
#include <trn/Article.h>
#include <trn/artio.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/head.h>
#include <trn/init.h>
#include <trn/ngdata.h>
#include <trn/nntp.h>

#include <test_config.h>

#include "MockNNTPConnection.h"

#include <boost/asio/error.hpp>

#include <gmock/gmock.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

using namespace testing;

using MockNNTPConnectionFactory =
    StrictMock<MockFunction<ConnectionPtr(const char *machine, int port, const char *service)>>;

class NNTPTest : public Test
{
public:
    ~NNTPTest() override = default;

protected:
    void SetUp() override
    {
        init_nntp();
        set_nntp_connection_factory(m_factory.AsStdFunction());
        m_connection = std::make_shared<StrictMock<MockNNTPConnection>>();
    }

    void TearDown() override
    {
        g_nntp_link.connection.reset();
        nntp_gets_clear_buffer();
        init_nntp();
    }

    void configure_factory_create(ConnectionPtr result)
    {
        EXPECT_CALL(m_factory, Call(StrEq(m_machine), _, StrEq("nntp"))).WillOnce(Return(std::move(result)));
    }

    const char *const                               m_machine{"news.gmane.io"};
    MockNNTPConnectionFactory                       m_factory;
    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
};

TEST_F(NNTPTest, server_init_connection_failed)
{
    configure_factory_create(nullptr);

    const int result = server_init(m_machine);

    EXPECT_EQ(-1, result);
}

TEST_F(NNTPTest, nntpConnectConnectionFailedReportsUnavailable)
{
    configure_factory_create(nullptr);

    testing::internal::CaptureStdout();
    const int         result = nntp_connect(m_machine, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(0, result);
    EXPECT_EQ("News server \"news.gmane.io\" is unavailable.\n", output);
}

class NNTPConnectedTest : public NNTPTest
{
public:
    ~NNTPConnectedTest() override = default;

protected:
    void SetUp() override
    {
        NNTPTest::SetUp();
        configure_factory_create(m_connection);
    }
};

TEST_F(NNTPConnectedTest, server_init_ok)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"));
    EXPECT_CALL(*m_connection, write_line(StrEq("MODE READER"), _)).Times(1);

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_POSTOK_VAL, result);
}

TEST_F(NNTPConnectedTest, server_init_posting_prohibited)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("201 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting prohibited)"))
        .WillOnce(Return("201 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting prohibited)"));
    EXPECT_CALL(*m_connection, write_line(StrEq("MODE READER"), _)).Times(1);

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_NOPOSTOK_VAL, result);
}

TEST_F(NNTPConnectedTest, server_init_temporarily_unavailable)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("400 news.gmane.io InterNetNews NNRP server INN 2.6.3 temporarily unavailable"));

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_GOODBYE_VAL, result);
}

TEST_F(NNTPConnectedTest, server_init_permanently_unavailable)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("502 news.gmane.io InterNetNews NNRP server INN 2.6.3 permanently unavailable"));

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_ACCESS_VAL, result);
}

TEST_F(NNTPConnectedTest, nntpConnectAccessDeniedReportsPermissionDenied)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("502 news.gmane.io InterNetNews NNRP server INN 2.6.3 permanently unavailable"));

    testing::internal::CaptureStdout();
    const int         result = nntp_connect(m_machine, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(-1, result);
    EXPECT_EQ("This machine does not have permission to use the news.gmane.io news server.\n\n", output);
}

TEST_F(NNTPConnectedTest, nntpConnectUnknownResponseReportsCode)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("600 unfamiliar response"));

    testing::internal::CaptureStdout();
    const int         result = nntp_connect(m_machine, false);
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_EQ(0, result);
    EXPECT_EQ("\nUnknown response code 600 from news.gmane.io.\n", output);
}

class NntpServerNameTest : public Test
{
protected:
    void SetUp() override
    {
        const TestInfo *test_info = UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();
        std::error_code error;
        fs::remove_all(m_output_dir, error);
        fs::create_directories(m_output_dir, error);
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    std::string file_ref_name(const fs::path &path) const
    {
        std::string name = path.generic_string();
        if (name.size() > 1 && name[1] == ':')
        {
            name.erase(0, 2);
        }
        return name;
    }

    fs::path m_output_dir;
};

TEST_F(NntpServerNameTest, returnsServerName)
{
    EXPECT_EQ("news.example.test", nntp_server_name("news.example.test"));
}

TEST_F(NntpServerNameTest, readsFirstNonCommentServerNameFromFile)
{
    const fs::path server_file = m_output_dir / "server-name";
    std::ofstream{server_file} << "# comment\n"
                               << "\n"
                               << "news.example.test\n"
                               << "ignored.example.test\n";
    const std::string name = file_ref_name(server_file);

    EXPECT_EQ("news.example.test", nntp_server_name(name));
}

TEST_F(NntpServerNameTest, returnsReferenceNameWhenFileIsMissing)
{
    const std::string name = file_ref_name(m_output_dir / "missing-server-name");

    EXPECT_EQ(name, nntp_server_name(name));
}

class NNTPGetStringTest : public Test
{
public:
    ~NNTPGetStringTest() override = default;

protected:
    void SetUp() override
    {
        Test::SetUp();
        nntp_gets_clear_buffer();
        m_connection = std::make_shared<StrictMock<MockNNTPConnection>>();
        m_saved_nntp_flags = g_nntp_link.flags;
        g_nntp_link.connection = m_connection;
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
    }

    void TearDown() override
    {
        Test::TearDown();
        g_nntp_link.connection.reset();
        g_nntp_link.flags = m_saved_nntp_flags;
    }

    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
    boost::system::error_code                       m_ec;
    NNTPFlags                                       m_saved_nntp_flags{};
};

TEST_F(NNTPGetStringTest, getALineReturnsServerLine)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("server line")));

    const std::string line = nntp_get_a_line();

    EXPECT_EQ("server line", line);
}

TEST_F(NNTPGetStringTest, string_line_fits)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this fits")));
    std::string line{"junk"};

    const NNTPGetsResult result = nntp_gets(line, 1024);

    EXPECT_EQ(NGSR_FULL_LINE, result);
    EXPECT_EQ("this fits", line);
}

TEST_F(NNTPGetStringTest, string_partial_line_continues_from_saved_text)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this does not fit")));
    std::string line;

    EXPECT_EQ(NGSR_PARTIAL_LINE, nntp_gets(line, 5));
    EXPECT_EQ("this", line);
    EXPECT_EQ(NGSR_PARTIAL_LINE, nntp_gets(line, 5));
    EXPECT_EQ("does", line);
    EXPECT_EQ(NGSR_PARTIAL_LINE, nntp_gets(line, 5));
    EXPECT_EQ("not ", line);
    EXPECT_EQ(NGSR_FULL_LINE, nntp_gets(line, 5));
    EXPECT_EQ("it", line);
}

TEST_F(NNTPGetStringTest, string_error_leaves_line_unchanged)
{
    m_ec = boost::asio::error::eof;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this does not fit")));
    std::string line{"junk"};

    const NNTPGetsResult result = nntp_gets(line, 1024);

    EXPECT_EQ(NGSR_ERROR, result);
    EXPECT_EQ("junk", line);
}

TEST_F(NNTPGetStringTest, finishListDrainsUntilTerminator)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("comp.lang.apl 2 1 y"))
        .WillOnce(Return("comp.lang.c++ 2 1 y"))
        .WillOnce(Return("."));

    nntp_finish_list();
}

TEST_F(NNTPGetStringTest, finishListDrainsLongLinesUntilTerminator)
{
    const std::string long_line(static_cast<std::size_t>(NNTP_STRLEN) + 8, 'x');
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return(long_line)).WillOnce(Return("."));

    nntp_finish_list();
}

TEST_F(NNTPGetStringTest, finishListStopsOnReadError)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("comp.lang.apl 2 1 y"))
        .WillOnce(Invoke(
            [](error_code_t &ec)
            {
                ec = boost::asio::error::eof;
                return "ignored";
            }));

    nntp_finish_list();
}

TEST_F(NNTPGetStringTest, statFormatsArticleNumberCommand)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT 123"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 123 <message@example.test>"));

    EXPECT_EQ(1, nntp_stat(ArticleNum{123}));
}

TEST_F(NNTPGetStringTest, headerFormatsArticleNumberCommand)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("HEAD 123"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("221 123 <message@example.test> header follows"));

    EXPECT_EQ(1, nntp_header(ArticleNum{123}));
}

TEST_F(NNTPGetStringTest, newGroupsFormatsGmtTimestampCommand)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("NEWGROUPS 700101 000000 GMT"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("231 list of new newsgroups follows"));

    EXPECT_EQ(1, nntp_new_groups(std::time_t{}));
}

class NNTPBodyTest : public NNTPGetStringTest
{
protected:
    void SetUp() override
    {
        NNTPGetStringTest::SetUp();
        head_init();

        m_old_art_fp = g_art_fp;
        m_old_current_path = fs::current_path();
        m_old_parsed_art = g_parsed_art;
        m_old_nntp_flags = g_nntp_link.flags;
        m_old_pid = g_our_pid;

        const TestInfo *test_info = UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::current_path(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();

        (void) nntp_art_name(ArticleNum{}, false);
        g_art_fp = nullptr;
        g_parsed_art = ArticleNum{};
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
        g_our_pid = 24680;
    }

    void TearDown() override
    {
        if (g_art_fp != nullptr)
        {
            std::fclose(g_art_fp);
            g_art_fp = nullptr;
        }
        (void) nntp_art_name(ArticleNum{}, false);

        g_art_fp = m_old_art_fp;
        g_parsed_art = m_old_parsed_art;
        g_nntp_link.flags = m_old_nntp_flags;
        g_our_pid = m_old_pid;
        head_final();

        std::error_code error;
        fs::current_path(m_old_current_path, error);
        fs::remove_all(m_output_dir, error);

        NNTPGetStringTest::TearDown();
    }

    std::FILE *m_old_art_fp{};
    fs::path   m_old_current_path;
    ArticleNum m_old_parsed_art{};
    NNTPFlags  m_old_nntp_flags{};
    long       m_old_pid{};
    fs::path   m_output_dir;
};

TEST_F(NNTPBodyTest, unstuffsDotPrefixedBodyLines)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("ARTICLE 7"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("220 7 <message@example.test> article follows"))
        .WillOnce(Return("plain"))
        .WillOnce(Return("..dot-stuffed"))
        .WillOnce(Return("."));

    nntp_body(ArticleNum{7});

    char line[NNTP_STRLEN]{};
    ASSERT_EQ(line, nntp_read_art(line, sizeof line));
    EXPECT_STREQ("plain\n", line);
    ASSERT_EQ(line, nntp_read_art(line, sizeof line));
    EXPECT_STREQ(".dot-stuffed\n", line);
    EXPECT_EQ(nullptr, nntp_read_art(line, sizeof line));
}

TEST_F(NNTPBodyTest, stopsBodyReadOnReadError)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("ARTICLE 7"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("220 7 <message@example.test> article follows"))
        .WillOnce(Invoke(
            [](error_code_t &ec)
            {
                ec = boost::asio::error::eof;
                return "ignored";
            }));

    nntp_body(ArticleNum{7});

    std::string line(NNTP_STRLEN, '\0');
    EXPECT_EQ(nullptr, nntp_read_art(line.data(), static_cast<int>(line.size())));
}

TEST_F(NNTPBodyTest, requestsBodyWhenArticleHeaderIsParsed)
{
    const std::string header{"Subject: cached\n\n"};
    ASSERT_NE(nullptr, g_head_buf);
    header.copy(g_head_buf, header.size());
    g_head_buf[header.size()] = '\0';
    g_parsed_art = ArticleNum{7};

    EXPECT_CALL(*m_connection, write_line(StrEq("BODY 7"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("222 7 <message@example.test> body follows"));

    nntp_body(ArticleNum{7});
}

class RemoteHeaderPrefetchTest : public NNTPGetStringTest
{
protected:
    void SetUp() override
    {
        NNTPGetStringTest::SetUp();

        m_old_data_source = g_data_source;
        m_old_article_list = std::move(g_article_list);
        m_old_parsed_art = g_parsed_art;
        m_old_last_art = g_last_art;
        m_old_nntp_flags = g_nntp_link.flags;

        g_data_source = &m_data_source;
        m_data_source.m_flags = DF_REMOTE;
        g_article_list.clear();
        article_ptr(ArticleNum{1})->m_flags = AF_EXISTS;
        g_parsed_art = ArticleNum{};
        g_last_art = ArticleNum{1};
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
    }

    void TearDown() override
    {
        g_nntp_link.flags = m_old_nntp_flags;
        g_last_art = m_old_last_art;
        g_parsed_art = m_old_parsed_art;
        g_article_list.clear();
        g_article_list = std::move(m_old_article_list);
        g_data_source = m_old_data_source;

        NNTPGetStringTest::TearDown();
    }

    DataSource                    m_data_source{};
    DataSource                   *m_old_data_source{};
    std::map<ArticleNum, Article> m_old_article_list;
    ArticleNum                    m_old_parsed_art{};
    ArticleNum                    m_old_last_art{};
    NNTPFlags                     m_old_nntp_flags{};
};

TEST_F(RemoteHeaderPrefetchTest, readsHeaderLineFromXhdr)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("XHDR from 1-1"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("221 Header follows"))
        .WillOnce(Return("1 Casey <casey@example.test>\r"))
        .WillOnce(Return("."));

    const std::string from = prefetch_lines_copy(ArticleNum{1}, FROM_LINE);

    EXPECT_EQ("Casey <casey@example.test>", from);
    EXPECT_EQ("Casey <casey@example.test>", article_ptr(ArticleNum{1})->get_cached_line_text(FROM_LINE, false));
}
