#include <gmock/gmock.h>

#include <common.h>
#include <nntpclient.h>
#include <nntpinit.h>

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

TEST(NNTPTest, server_init_ok)
{
    const char *const machine{"news.gmane.io"};
    init_nntp();
    StrictMock<MockNNTPConnectionFactory> factory;
    set_nntp_connection_factory([&factory](const char *machine, int port, const char *service)
                                { return factory.create(machine, port, service); });
    std::shared_ptr<StrictMock<MockNNTPConnection>> connection = std::make_shared<StrictMock<MockNNTPConnection>>();
    EXPECT_CALL(factory, create(StrEq(machine), _, StrEq("nntp"))).WillOnce(Return(connection));
    EXPECT_CALL(*connection, read_line(_))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"));
    EXPECT_CALL(*connection, write_line(StrEq("MODE READER"), _)).Times(1);

    const int result = server_init(machine);

    EXPECT_EQ(NNTP_POSTOK_VAL, result);
}
