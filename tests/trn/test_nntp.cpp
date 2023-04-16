#include <gmock/gmock.h>

#include <common.h>
#include <nntpclient.h>
#include <nntpinit.h>

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

class INNTPConnectionFactory
{
public:
    virtual ~INNTPConnectionFactory() = default;

    virtual ConnectionPtr create(const char *machine, int port, const char *service) = 0;
};

class MockNNTPConnectionFactory : public INNTPConnectionFactory
{
public:
    ~MockNNTPConnectionFactory() override = default;
    MOCK_METHOD(ConnectionPtr, create, (const char *machine, int port, const char *service), (override));
};

class NNTPTest : public Test
{
public:
    ~NNTPTest() override = default;

protected:
    void SetUp() override
    {
        init_nntp();
        set_nntp_connection_factory([this](const char *machine, int port, const char *service)
                                    { return m_factory.create(machine, port, service); });
        m_connection = std::make_shared<StrictMock<MockNNTPConnection>>();
    }

    void configure_factory_create(ConnectionPtr result)
    {
        EXPECT_CALL(m_factory, create(StrEq(m_machine), _, StrEq("nntp"))).WillOnce(Return(std::move(result)));
    }

    const char *const                               m_machine{"news.gmane.io"};
    StrictMock<MockNNTPConnectionFactory>           m_factory;
    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
};

TEST_F(NNTPTest, server_init_ok)
{
    configure_factory_create(m_connection);
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"));
    EXPECT_CALL(*m_connection, write_line(StrEq("MODE READER"), _)).Times(1);

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_POSTOK_VAL, result);
}

TEST_F(NNTPTest, server_init_connection_failed)
{
    configure_factory_create(nullptr);

    const int result = server_init(m_machine);

    EXPECT_EQ(-1, result);
}
