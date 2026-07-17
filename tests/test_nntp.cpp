// This software is copyrighted as detailed in the LICENSE file.
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>

#include <config/common.h>
#include <trn/Article.h>
#include <trn/cache.h>
#include <trn/datasrc.h>
#include <trn/head.h>
#include <trn/ngdata.h>

#include <boost/asio/error.hpp>

#include <gmock/gmock.h>

#include <map>
#include <utility>

using namespace testing;

class MockNNTPConnection : public INNTPConnection
{
public:
    ~MockNNTPConnection() override = default;

    MOCK_METHOD(std::string, read_line, (error_code_t &), (override));
    MOCK_METHOD(void, write_line, (const std::string &, error_code_t &), (override));
    MOCK_METHOD(void, write, (const char *, size_t, error_code_t &), (override));
    MOCK_METHOD(size_t, read, (char *, size_t, error_code_t &), (override));
};

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
        g_nntp_link.connection = m_connection;
    }

    void TearDown() override
    {
        Test::TearDown();
        g_nntp_link.connection.reset();
    }

    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
    boost::system::error_code                       m_ec;
};

TEST_F(NNTPGetStringTest, line_fits)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this fits")));
    char buffer[1024];

    const NNTPGetsResult result = nntp_gets(buffer, sizeof(buffer));

    EXPECT_EQ(NGSR_FULL_LINE, result);
    EXPECT_EQ("this fits", std::string(buffer));
}

TEST_F(NNTPGetStringTest, getALineReturnsServerLine)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("server line")));

    const std::string line = nntp_get_a_line();

    EXPECT_EQ("server line", line);
}

TEST_F(NNTPGetStringTest, partial_line)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this does not fit")));
    char buffer[5];

    const NNTPGetsResult result = nntp_gets(buffer, sizeof(buffer));

    EXPECT_EQ(NGSR_PARTIAL_LINE, result);
    EXPECT_EQ("this", std::string(buffer));
}

TEST_F(NNTPGetStringTest, error)
{
    m_ec = boost::asio::error::eof;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this does not fit")));
    char buffer[1024]{"junk"};

    const NNTPGetsResult result = nntp_gets(buffer, sizeof(buffer));

    EXPECT_EQ(NGSR_ERROR, result);
    EXPECT_EQ("junk", std::string(buffer));
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
