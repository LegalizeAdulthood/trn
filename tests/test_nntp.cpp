#include <gmock/gmock.h>

#include <config/common.h>
#include <nntp/nntpclient.h>
#include <nntp/nntpinit.h>

#include <boost/asio/error.hpp>

#include <utility>

using namespace testing;

class MockNNTPConnection : public INNTPConnection
{
public:
    ~MockNNTPConnection() override = default;

    MOCK_METHOD(std::string, read_line, (error_code &), (override));
    MOCK_METHOD(void, write_line, (const std::string &, error_code &), (override));
    MOCK_METHOD(void, write, (const char *, size_t, error_code &), (override));
    MOCK_METHOD(size_t, read, (char *, size_t, error_code &), (override));
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
        g_nntplink.connection = m_connection;
    }

    void TearDown() override
    {
        Test::TearDown();
        g_nntplink.connection.reset();
    }

    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
    boost::system::error_code                       m_ec;
};

TEST_F(NNTPGetStringTest, line_fits)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this fits")));
    char buffer[1024];

    const nntp_gets_result result = nntp_gets(buffer, sizeof(buffer));

    EXPECT_EQ(NGSR_FULL_LINE, result);
    EXPECT_EQ("this fits", std::string(buffer));
}

TEST_F(NNTPGetStringTest, partial_line)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this does not fit")));
    char buffer[5];

    const nntp_gets_result result = nntp_gets(buffer, sizeof(buffer));

    EXPECT_EQ(NGSR_PARTIAL_LINE, result);
    EXPECT_EQ("this", std::string(buffer));
}

TEST_F(NNTPGetStringTest, error)
{
    m_ec = boost::asio::error::eof;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this does not fit")));
    char buffer[1024]{"junk"};

    const nntp_gets_result result = nntp_gets(buffer, sizeof(buffer));

    EXPECT_EQ(NGSR_ERROR, result);
    EXPECT_EQ("junk", std::string(buffer));
}
