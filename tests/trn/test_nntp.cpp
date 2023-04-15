#include <gmock/gmock.h>

#include <common.h>
#include <nntpclient.h>
#include <nntpinit.h>

using namespace testing;

class MockNNTPConnection : public INNTPConnection
{
public:
    MOCK_METHOD(std::string, readLine, (error_code &), (override));
    MOCK_METHOD(void, writeLine, (const std::string &, error_code &), (override));
    MOCK_METHOD(void, write, (const char *, size_t, error_code &), (override));
    MOCK_METHOD(size_t, read, (char *, size_t, error_code &), (override));
};

class MockNNTPConnectionFactory
{
public:
    MOCK_METHOD(ConnectionPtr, create, (const char *machine, int port, const char *service));
};

TEST( NNTPTest, basic )
{
    const char *const machine{"news.gmane.io"};
    init_nntp();
    StrictMock<MockNNTPConnectionFactory> factory;
    set_nntp_connection_factory([&factory](const char *machine, int port, const char *service)
                                { return factory.create(machine, port, service); });
    std::shared_ptr<StrictMock<MockNNTPConnection>> connection = std::make_shared<StrictMock<MockNNTPConnection>>();
    EXPECT_CALL(factory, create(StrEq(machine), _, StrEq("nntp"))).WillOnce(Return(connection));
    EXPECT_CALL(*connection, readLine(_))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"));
    EXPECT_CALL(*connection, writeLine(StrEq("MODE READER"), _)).Times(1);

    server_init(machine);
}
