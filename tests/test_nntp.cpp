// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson
#include <nntp/nntpauth.h>
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
#include <trn/yenc.h>

#include <test_config.h>

#include "MockNNTPConnection.h"
#include "support/MockNntpServer.h"
#include "support/NntpTestCertificate.h"
#include "support/YencFixture.h"

#include <boost/asio/error.hpp>

#include <gmock/gmock.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

namespace fs = std::filesystem;

using namespace testing;

class AdvertisedCapabilitySetTest : public Test
{
protected:
    void SetUp() override;

    NntpCapabilities m_capabilities;
};

void AdvertisedCapabilitySetTest::SetUp()
{
    for (const std::string_view line :
         {"VERSION 2 3", "READER", "POST", "IHAVE", "NEWNEWS", "LIST ACTIVE NEWSGROUPS OVERVIEW.FMT HEADERS",
          "OVER MSGID", "HDR", "MODE-READER", "STARTTLS", "AUTHINFO USER SASL", "SASL PLAIN SCRAM-SHA-256"})
    {
        m_capabilities.add(line);
    }
}

using CapabilityNameCase = std::pair<std::string_view, bool>;
using CapabilityArgumentCase = std::tuple<std::string_view, std::string_view, bool>;

class AdvertisedCapabilityNameTest : public AdvertisedCapabilitySetTest, public WithParamInterface<CapabilityNameCase>
{
};

class AdvertisedCapabilityArgumentTest : public AdvertisedCapabilitySetTest,
                                         public WithParamInterface<CapabilityArgumentCase>
{
};

TEST_P(AdvertisedCapabilityNameTest, findsOnlyAdvertisedNames)
{
    const CapabilityNameCase &input = GetParam();

    const bool present = m_capabilities.has(input.first);

    EXPECT_EQ(input.second, present);
}

INSTANTIATE_TEST_SUITE_P(AdvertisedAndMissing, AdvertisedCapabilityNameTest,
                         Values(CapabilityNameCase{"VERSION", true}, CapabilityNameCase{"READER", true},
                                CapabilityNameCase{"POST", true}, CapabilityNameCase{"IHAVE", true},
                                CapabilityNameCase{"NEWNEWS", true}, CapabilityNameCase{"LIST", true},
                                CapabilityNameCase{"OVER", true}, CapabilityNameCase{"HDR", true},
                                CapabilityNameCase{"MODE-READER", true}, CapabilityNameCase{"STARTTLS", true},
                                CapabilityNameCase{"AUTHINFO", true}, CapabilityNameCase{"SASL", true},
                                CapabilityNameCase{"XGTITLE", false}));

TEST_P(AdvertisedCapabilityArgumentTest, findsOnlyAdvertisedArguments)
{
    const auto &[name, argument, expected] = GetParam();

    const bool present = m_capabilities.has(name, argument);

    EXPECT_EQ(expected, present);
}

INSTANTIATE_TEST_SUITE_P(
    AdvertisedAndMissing, AdvertisedCapabilityArgumentTest,
    Values(CapabilityArgumentCase{"VERSION", "2", true}, CapabilityArgumentCase{"VERSION", "3", true},
           CapabilityArgumentCase{"LIST", "ACTIVE", true}, CapabilityArgumentCase{"LIST", "NEWSGROUPS", true},
           CapabilityArgumentCase{"LIST", "OVERVIEW.FMT", true}, CapabilityArgumentCase{"LIST", "HEADERS", true},
           CapabilityArgumentCase{"OVER", "MSGID", true}, CapabilityArgumentCase{"AUTHINFO", "USER", true},
           CapabilityArgumentCase{"AUTHINFO", "SASL", true}, CapabilityArgumentCase{"SASL", "SCRAM-SHA-256", true},
           CapabilityArgumentCase{"LIST", "SUBSCRIPTIONS", false}, CapabilityArgumentCase{"VERSION", "4", false},
           CapabilityArgumentCase{"XGTITLE", "comp.lang.apl", false}));

TEST_F(AdvertisedCapabilitySetTest, addingLinesDoesNotMarkTheSetAvailable)
{
    EXPECT_FALSE(m_capabilities.available);
}

class CapabilityLineTest : public Test
{
protected:
    void add_line(std::string_view line);

    std::string      m_line;
    NntpCapabilities m_capabilities;
};

void CapabilityLineTest::add_line(std::string_view line)
{
    m_line = line;
    m_capabilities.add(m_line);
}

using CapabilityLineNameCase = std::tuple<std::string_view, std::string_view, bool>;
using CapabilityLineArgumentCase = std::tuple<std::string_view, std::string_view, std::string_view, bool>;

class CapabilityLineNameTest : public CapabilityLineTest, public WithParamInterface<CapabilityLineNameCase>
{
protected:
    void SetUp() override
    {
        add_line(std::get<0>(GetParam()));
    }
};

class CapabilityLineArgumentTest : public CapabilityLineTest, public WithParamInterface<CapabilityLineArgumentCase>
{
protected:
    void SetUp() override
    {
        add_line(std::get<0>(GetParam()));
    }
};

constexpr const char *VARIANT_LIST_LINE = "lIsT\tactive  Newsgroups\tOVERVIEW.FMT\r\n";

TEST_P(CapabilityLineNameTest, recognizesOnlyAcceptedCapabilityNames)
{
    const auto &[line, name, expected] = GetParam();

    const bool present = m_capabilities.has(name);

    EXPECT_EQ(expected, present);
}

INSTANTIATE_TEST_SUITE_P(CaseVariants, CapabilityLineNameTest,
                         Values(CapabilityLineNameCase{VARIANT_LIST_LINE, "list", true}));

INSTANTIATE_TEST_SUITE_P(IgnoredLines, CapabilityLineNameTest,
                         Combine(Values("", "\r\n", ".", "#READER", "1READER", " READER", "\tREADER"),
                                 Values("READER", ""), Values(false)));

TEST_P(CapabilityLineArgumentTest, recognizesOnlyAcceptedCapabilityArguments)
{
    const auto &[line, name, argument, expected] = GetParam();

    const bool present = m_capabilities.has(name, argument);

    EXPECT_EQ(expected, present);
}

INSTANTIATE_TEST_SUITE_P(CaseAndWhitespaceVariants, CapabilityLineArgumentTest,
                         Values(CapabilityLineArgumentCase{VARIANT_LIST_LINE, "LIST", "ACTIVE", true},
                                CapabilityLineArgumentCase{VARIANT_LIST_LINE, "list", "newsgroups", true},
                                CapabilityLineArgumentCase{VARIANT_LIST_LINE, "LIST", "overview.fmt", true},
                                CapabilityLineArgumentCase{"oVeR\tmsgid\r\n", "over", "MSGID", true},
                                CapabilityLineArgumentCase{VARIANT_LIST_LINE, "LIST", "NEWS", false}));

INSTANTIATE_TEST_SUITE_P(PrivateExtensions, CapabilityLineArgumentTest,
                         Values(CapabilityLineArgumentCase{"XPRIVATE extension", "XPRIVATE", "extension", true}));

using MockNNTPConnectionFactory =
    StrictMock<MockFunction<ConnectionPtr(std::string_view machine, int port, std::string_view service)>>;

class NNTPTest : public Test
{
public:
    ~NNTPTest() override = default;

protected:
    void SetUp() override
    {
        init_nntp();
        m_saved_nntp_link = g_nntp_link;
        g_nntp_link = {};
        set_nntp_connection_factory(m_factory.AsStdFunction());
        m_connection = std::make_shared<StrictMock<MockNNTPConnection>>();
    }

    void TearDown() override
    {
        g_nntp_link.connection.reset();
        g_nntp_link = std::move(m_saved_nntp_link);
        nntp_gets_clear_buffer();
        init_nntp();
    }

    void configure_factory_create(ConnectionPtr result)
    {
        EXPECT_CALL(m_factory, Call(std::string_view{m_machine}, _, std::string_view{"nntp"}))
            .WillOnce(Return(std::move(result)));
    }

    void expect_reply(std::string_view command, const std::vector<std::string> &lines)
    {
        EXPECT_CALL(*m_connection, write_line(StrEq(std::string{command}), _));
        for (const std::string &line : lines)
        {
            EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return(line));
        }
    }

    NNTPLink                                        m_saved_nntp_link;
    const char *const                               m_machine{"news.gmane.io"};
    MockNNTPConnectionFactory                       m_factory;
    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
};

TEST_F(NNTPTest, server_init_connection_failed)
{
    configure_factory_create(nullptr);
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("STARTTLS");
    g_nntp_link.unsupported_commands.push_back("OVER");

    const int result = server_init(m_machine);

    EXPECT_EQ(-1, result);
    EXPECT_FALSE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
    EXPECT_TRUE(g_nntp_link.unsupported_commands.empty());
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

TEST_F(NNTPTest, connectsImplicitTlsBeforeGreetingAndOnReconnect)
{
    g_nntp_link.implicit_tls = true;
    g_nntp_link.allow_starttls = false;
    InSequence sequence;
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        EXPECT_CALL(m_factory, Call(std::string_view{m_machine}, 563, std::string_view{"nntps"}))
            .WillOnce(Return(m_connection));
        EXPECT_CALL(*m_connection, start_tls(_));
        EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 encrypted server ready"));
        expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    }

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        EXPECT_EQ(200, server_init(m_machine));
        EXPECT_EQ(NntpSecurity::ImplicitTls, g_nntp_link.security);
        nntp_close(false);
        EXPECT_EQ(NntpSecurity::Cleartext, g_nntp_link.security);
        EXPECT_TRUE(g_nntp_link.implicit_tls);
    }
}

TEST_F(NNTPTest, connectsImplicitTlsOnExplicitPortWithoutAnotherUpgrade)
{
    g_nntp_link.implicit_tls = true;
    g_nntp_link.port_number = 5563;
    InSequence sequence;
    EXPECT_CALL(m_factory, Call(std::string_view{m_machine}, 5563, std::string_view{"nntps"}))
        .WillOnce(Return(m_connection));
    EXPECT_CALL(*m_connection, start_tls(_));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 encrypted server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "MODE-READER", "STARTTLS", "."});
    expect_reply("MODE READER", {"201 encrypted reader ready"});
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "STARTTLS", "."});

    EXPECT_EQ(201, server_init(m_machine));
    EXPECT_EQ(NntpSecurity::ImplicitTls, g_nntp_link.security);
}

TEST_F(NNTPTest, closesImplicitTlsWhenHandshakeFailsBeforeGreeting)
{
    g_nntp_link.implicit_tls = true;
    EXPECT_CALL(m_factory, Call(std::string_view{m_machine}, 563, std::string_view{"nntps"}))
        .WillOnce(Return(m_connection));
    EXPECT_CALL(*m_connection, start_tls(_))
        .WillOnce([](error_code_t &ec) { ec = boost::asio::error::connection_reset; });

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_FALSE(g_nntp_link.connection);
    EXPECT_EQ(NntpSecurity::Cleartext, g_nntp_link.security);
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

TEST_F(NNTPConnectedTest, upgradesAdvertisedStartTlsBeforeReading)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "STARTTLS", "XOLD", "."});
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "OVER", "."});

    EXPECT_EQ(NNTP_POSTOK_VAL, server_init(m_machine));
    EXPECT_TRUE(g_nntp_link.capabilities.has("OVER"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("XOLD"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
}

TEST_F(NNTPConnectedTest, closesConnectionWhenStartTlsHandshakeFails)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "STARTTLS", "."});
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_))
        .WillOnce([](error_code_t &ec) { ec = boost::asio::error::connection_reset; });

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_FALSE(g_nntp_link.connection);
    EXPECT_FALSE(g_nntp_link.capabilities.available);
}

TEST_F(NNTPConnectedTest, closesConnectionWhenStartTlsIsUnavailable)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "STARTTLS", "."});
    expect_reply("STARTTLS", {"502 command unavailable"});

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_FALSE(g_nntp_link.connection);
}

TEST_F(NNTPConnectedTest, closesConnectionWhenStartTlsCannotNegotiate)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "STARTTLS", "."});
    expect_reply("STARTTLS", {"580 cannot initiate TLS negotiation"});

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_FALSE(g_nntp_link.connection);
}

TEST_F(NNTPConnectedTest, defersForcedAuthUntilAfterStartTlsDiscovery)
{
    DataSource source;
    source.m_auth_user = "reader";
    source.m_auth_pass = "secret";
    DataSource *const saved_source = g_data_source;
    g_data_source = &source;
    g_nntp_link.flags = NNTP_NEW_CMD_OK | NNTP_FORCE_AUTH_NOW;
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "STARTTLS", "."});
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});

    const int result = server_init(m_machine);
    g_data_source = saved_source;

    EXPECT_EQ(200, result);
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
    EXPECT_TRUE(g_nntp_link.flags & NNTP_FORCE_AUTH_NOW);
}

TEST_F(NNTPConnectedTest, handlesAuthChallengeDuringStartTlsDiscovery)
{
    DataSource source;
    source.m_auth_user = "reader";
    source.m_auth_pass = "secret";
    DataSource *const saved_source = g_data_source;
    g_data_source = &source;
    g_nntp_link.flags = NNTP_NEW_CMD_OK | NNTP_FORCE_AUTH_NOW;
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"480 authentication required"});
    expect_reply("AUTHINFO USER reader", {"381 password required"});
    expect_reply("AUTHINFO PASS secret", {"281 authentication accepted"});
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});

    const int result = server_init(m_machine);
    g_data_source = saved_source;

    EXPECT_EQ(200, result);
    EXPECT_FALSE(g_nntp_link.flags & NNTP_FORCE_AUTH_NOW);
}

TEST_F(NNTPConnectedTest, server_init_ok)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"))
        .WillOnce(Return("500 unknown command"))
        .WillOnce(Return("200 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting ok)"))
        .WillOnce(Return("500 unknown command"));
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _)).Times(2);
    EXPECT_CALL(*m_connection, write_line(StrEq("MODE READER"), _)).Times(1);

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_POSTOK_VAL, result);
    EXPECT_FALSE(g_nntp_link.capabilities.available);
}

TEST_F(NNTPConnectedTest, server_init_posting_prohibited)
{
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("201 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting prohibited)"))
        .WillOnce(Return("500 unknown command"))
        .WillOnce(Return("201 news.gmane.io InterNetNews NNRP server INN 2.6.3 ready (posting prohibited)"))
        .WillOnce(Return("500 unknown command"));
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _)).Times(2);
    EXPECT_CALL(*m_connection, write_line(StrEq("MODE READER"), _)).Times(1);

    const int result = server_init(m_machine);

    EXPECT_EQ(NNTP_NOPOSTOK_VAL, result);
}

TEST_F(NNTPConnectedTest, refreshesCapabilitiesAfterReaderMode)
{
    g_nntp_link.allow_starttls = false;
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "MODE-READER", "STARTTLS", "."});
    expect_reply("MODE READER", {"201 reader mode, posting prohibited"});
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "READER", "OVER MSGID", "HDR", "."});

    EXPECT_EQ(NNTP_NOPOSTOK_VAL, server_init(m_machine));
    EXPECT_EQ("201 reader mode, posting prohibited", g_ser_line);
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_TRUE(g_nntp_link.capabilities.has("READER"));
    EXPECT_TRUE(g_nntp_link.capabilities.has("OVER", "MSGID"));
    EXPECT_TRUE(g_nntp_link.capabilities.has("HDR"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("MODE-READER"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
    EXPECT_TRUE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
}

TEST_F(NNTPConnectedTest, discoversCapabilitiesOnlyAvailableInReaderMode)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"500 unknown command"});
    expect_reply("MODE READER", {"200 reader ready"});
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "READER", "STARTTLS", "."});
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "READER", "."});

    EXPECT_EQ(NNTP_POSTOK_VAL, server_init(m_machine));
    EXPECT_EQ("200 reader ready", g_ser_line);
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
}

TEST_F(NNTPConnectedTest, clearsCapabilitiesWhenReaderModeDoesNotSupportDiscovery)
{
    g_nntp_link.allow_starttls = false;
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "MODE-READER", "STARTTLS", "."});
    expect_reply("MODE READER", {"200 reader ready"});
    expect_reply("CAPABILITIES", {"501 unsupported command"});

    EXPECT_EQ(NNTP_POSTOK_VAL, server_init(m_machine));
    EXPECT_FALSE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
}

TEST_F(NNTPConnectedTest, skipsReaderModeWhenAlreadyReading)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("201 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "READER", "HDR", "."});

    EXPECT_EQ(NNTP_NOPOSTOK_VAL, server_init(m_machine));
    EXPECT_EQ("201 server ready", g_ser_line);
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_TRUE(g_nntp_link.capabilities.has("HDR"));
}

TEST_F(NNTPConnectedTest, skipsUnadvertisedReaderModeOnTransitServer)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "IHAVE", "."});

    EXPECT_EQ(NNTP_POSTOK_VAL, server_init(m_machine));
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("READER"));
}

TEST_F(NNTPConnectedTest, preservesCapabilitiesWhenAdvertisedReaderModeIsUnsupported)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("201 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "MODE-READER", "HDR", "."});
    expect_reply("MODE READER", {"501 unsupported mode"});

    EXPECT_EQ(NNTP_NOPOSTOK_VAL, server_init(m_machine));
    EXPECT_TRUE(g_nntp_link.capabilities.has("HDR"));
}

TEST_F(NNTPConnectedTest, fallsBackWhenDiscoveryAndReaderModeAreUnsupported)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("201 server ready"));
    expect_reply("CAPABILITIES", {"500 unknown command"});
    expect_reply("MODE READER", {"500 unknown command"});

    EXPECT_EQ(NNTP_NOPOSTOK_VAL, server_init(m_machine));
    EXPECT_EQ("201 server ready", g_ser_line);
    EXPECT_FALSE(g_nntp_link.capabilities.available);
}

TEST_F(NNTPConnectedTest, failsWhenDiscoveryClosesConnection)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"400 server shutting down"});

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_EQ("400 server shutting down", g_ser_line);
    EXPECT_FALSE(g_nntp_link.connection);
    EXPECT_FALSE(g_nntp_link.capabilities.available);
}

TEST_F(NNTPConnectedTest, failsWhenReaderModeCapabilityRefreshClosesConnection)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "VERSION 2", "MODE-READER", "."});
    expect_reply("MODE READER", {"200 reader ready"});
    expect_reply("CAPABILITIES", {"502 service unavailable"});

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_FALSE(g_nntp_link.connection);
    EXPECT_FALSE(g_nntp_link.capabilities.available);
}

TEST_F(NNTPTest, discoversCapabilitiesFromMockServer)
{
    MockNntpServer server;
    server.expect_capabilities({"VERSION 2", "MODE-READER"});
    server.expect_mode_reader();
    server.expect_capabilities({"VERSION 2", "READER", "LIST ACTIVE NEWSGROUPS", "OVER", "HDR", "AUTHINFO USER"});
    server.expect_quit();
    server.start();
    init_nntp();
    g_nntp_link.port_number = server.port();

    ASSERT_EQ(1, nntp_connect("127.0.0.1", false));
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_TRUE(g_nntp_link.capabilities.has("LIST", "NEWSGROUPS"));
    EXPECT_TRUE(g_nntp_link.capabilities.has("OVER"));
    EXPECT_TRUE(g_nntp_link.capabilities.has("AUTHINFO", "USER"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("MODE-READER"));

    nntp_close(true);
    server.verify();
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
        return path.string();
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
        m_old_abs_first = g_abs_first;
        m_old_last_cached = g_last_cached;
        m_saved_nntp_link = g_nntp_link;
        g_nntp_link.connection = m_connection;
        g_nntp_link.flags = NNTP_NEW_CMD_OK;
        g_nntp_link.capabilities = {};
        g_nntp_link.unsupported_commands.clear();
    }

    void TearDown() override
    {
        Test::TearDown();
        g_nntp_link.connection.reset();
        g_nntp_link = std::move(m_saved_nntp_link);
        g_abs_first = m_old_abs_first;
        g_last_cached = m_old_last_cached;
    }

    std::shared_ptr<StrictMock<MockNNTPConnection>> m_connection;
    boost::system::error_code                       m_ec;
    ArticleNum                                      m_old_abs_first{};
    ArticleNum                                      m_old_last_cached{};
    NNTPLink                                        m_saved_nntp_link;
};

TEST_F(NNTPGetStringTest, retriesEncryptionRequiredCommandAfterStartTls)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("483 encryption required"));
    EXPECT_CALL(*m_connection, write_line(StrEq("STARTTLS"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("382 continue with TLS negotiation"));
    EXPECT_CALL(*m_connection, start_tls(_));
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("101 capabilities follow"))
        .WillOnce(Return("READER"))
        .WillOnce(Return("."));
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("211 1 1 1 comp.lang.apl"));

    ASSERT_EQ(1, nntp_command("GROUP comp.lang.apl"));
    EXPECT_EQ(1, nntp_check());
    EXPECT_EQ("211 1 1 1 comp.lang.apl", g_ser_line);
}

class RequiredTlsPolicyTest : public NNTPGetStringTest
{
protected:
    void SetUp() override;
};

void RequiredTlsPolicyTest::SetUp()
{
    NNTPGetStringTest::SetUp();
    g_nntp_link.allow_starttls = false;
    g_nntp_link.require_tls_for_auth = true;
    g_nntp_link.require_tls_for_posting = true;
}

class CleartextTlsCommandTest : public RequiredTlsPolicyTest, public WithParamInterface<const char *>
{
};

class ImplicitTlsPolicyTest : public RequiredTlsPolicyTest
{
protected:
    void SetUp() override;
};

void ImplicitTlsPolicyTest::SetUp()
{
    RequiredTlsPolicyTest::SetUp();
    g_nntp_link.security = NntpSecurity::ImplicitTls;
}

class ImplicitTlsCommandTest : public ImplicitTlsPolicyTest, public WithParamInterface<const char *>
{
};

using TlsHelper = std::pair<const char *, int (*)()>;

class ImplicitTlsHelperTest : public ImplicitTlsPolicyTest, public WithParamInterface<TlsHelper>
{
};

TEST_P(CleartextTlsCommandTest, blocksTheCommandBeforeWriting)
{
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    const int result = nntp_command(GetParam());

    EXPECT_LT(result, 0);
}

INSTANTIATE_TEST_SUITE_P(AuthAndPosting, CleartextTlsCommandTest,
                         Values("AUTHINFO USER reader", "AUTHINFO PASS secret", "post"));

TEST_F(NNTPGetStringTest, restoresSelectedGroupAndArticleAfterStartTls)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("211 2 1 2 comp.lang.apl"));
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT 2"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 2 <two@example.test>"));
    EXPECT_CALL(*m_connection, write_line(StrEq("BODY"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("483 encryption required"));
    EXPECT_CALL(*m_connection, write_line(StrEq("STARTTLS"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("382 continue with TLS negotiation"));
    EXPECT_CALL(*m_connection, start_tls(_));
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("101 capabilities follow"))
        .WillOnce(Return("READER"))
        .WillOnce(Return("."));
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("211 2 1 2 comp.lang.apl"));
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT 2"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 2 <two@example.test>"));
    EXPECT_CALL(*m_connection, write_line(StrEq("BODY"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("222 2 <two@example.test>"));

    ASSERT_EQ(1, nntp_command("GROUP comp.lang.apl"));
    ASSERT_EQ(1, nntp_check());
    ASSERT_EQ(1, nntp_command("STAT 2"));
    ASSERT_EQ(1, nntp_check());
    ASSERT_EQ(1, nntp_command("BODY"));
    EXPECT_EQ(1, nntp_check());
    EXPECT_EQ(222, nntp_response_code(g_ser_line));
}

TEST_F(NNTPGetStringTest, protectsPostingWithStartTlsBeforeSendingPost)
{
    g_nntp_link.require_tls_for_posting = true;
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("STARTTLS");
    InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(StrEq("STARTTLS"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("382 continue with TLS negotiation"));
    EXPECT_CALL(*m_connection, start_tls(_));
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("101 capabilities follow"))
        .WillOnce(Return("POST"))
        .WillOnce(Return("."));
    EXPECT_CALL(*m_connection, write_line(StrEq("POST"), _));

    EXPECT_EQ(1, nntp_command("POST"));
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
}

TEST_F(NNTPGetStringTest, authTlsPolicyBlocksPasswordExchangeOnChallenge)
{
    DataSource source;
    source.m_auth_user = "reader";
    source.m_auth_pass = "secret";
    DataSource *const saved_source = g_data_source;
    g_data_source = &source;
    g_nntp_link.allow_starttls = false;
    g_nntp_link.require_tls_for_auth = true;
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("480 authentication required"));

    const int sent = nntp_command("GROUP comp.lang.apl");
    const int result = nntp_check();
    g_data_source = saved_source;

    EXPECT_EQ(1, sent);
    EXPECT_LT(result, 0);
    EXPECT_EQ(483, nntp_response_code(g_ser_line));
}

TEST_P(ImplicitTlsCommandTest, sendsTheCommandThroughImplicitTls)
{
    EXPECT_CALL(*m_connection, write_line(StrEq(GetParam()), _));

    const int result = nntp_command(GetParam());

    EXPECT_EQ(1, result);
}

INSTANTIATE_TEST_SUITE_P(AuthAndPosting, ImplicitTlsCommandTest,
                         Values("AUTHINFO USER reader", "AUTHINFO PASS secret", "POST"));

TEST_P(ImplicitTlsHelperTest, acceptsExistingImplicitTlsWithoutWriting)
{
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    const int result = GetParam().second();

    EXPECT_EQ(1, result);
}

INSTANTIATE_TEST_SUITE_P(RequireAndStartTls, ImplicitTlsHelperTest,
                         Values(TlsHelper{"require TLS", nntp_require_tls}, TlsHelper{"start TLS", nntp_start_tls}));

class NntpAuthTest : public NNTPTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void expect_authentication();

    DataSource  m_source;
    DataSource *m_saved_source{};
};

void NntpAuthTest::SetUp()
{
    NNTPTest::SetUp();
    m_saved_source = g_data_source;
    m_source.m_auth_user = "reader";
    m_source.m_auth_pass = "secret";
    g_data_source = &m_source;
    g_nntp_link.connection = m_connection;
    g_nntp_link.flags = NNTP_NEW_CMD_OK;
    g_nntp_link.allow_legacy_auth = false;
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("AUTHINFO USER");
    g_last_command = "GROUP comp.lang.apl";
}

void NntpAuthTest::TearDown()
{
    g_data_source = m_saved_source;
    NNTPTest::TearDown();
}

void NntpAuthTest::expect_authentication()
{
    expect_reply("AUTHINFO USER reader", {"381 password required"});
    expect_reply("AUTHINFO PASS secret", {"281 authentication accepted"});
}

TEST_F(NntpAuthTest, refreshesCapabilitiesAfterAdvertisedUserPass)
{
    g_nntp_link.unsupported_commands = {"OVER"};
    InSequence sequence;
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "OVER", "."});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
    EXPECT_TRUE(g_nntp_link.capabilities.has("OVER"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("AUTHINFO"));
    EXPECT_TRUE(g_nntp_link.unsupported_commands.empty());
}

TEST_F(NntpAuthTest, acceptsUserWithoutSendingPassword)
{
    m_source.m_auth_pass.clear();
    InSequence sequence;
    expect_reply("AUTHINFO USER reader", {"281 authentication accepted"});
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, rejectsUnadvertisedMethodsInStrictMode)
{
    g_nntp_link.capabilities = {};
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    EXPECT_LT(nntp_handle_auth_err(), 0);
}

TEST_F(NntpAuthTest, refusesEmptyAuthinfoEvenInCompatibilityMode)
{
    g_nntp_link.allow_legacy_auth = true;
    g_nntp_link.capabilities = {};
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("AUTHINFO");
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    EXPECT_LT(nntp_handle_auth_err(), 0);
}

TEST_F(NntpAuthTest, leavesSaslOnlyServersForFutureMechanisms)
{
    g_nntp_link.allow_legacy_auth = true;
    g_nntp_link.capabilities = {};
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("AUTHINFO SASL");
    g_nntp_link.capabilities.add("SASL PLAIN SCRAM-SHA-256");
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    EXPECT_LT(nntp_handle_auth_err(), 0);
}

TEST_F(NntpAuthTest, permitsLegacyUserPassInCompatibilityMode)
{
    g_nntp_link.allow_legacy_auth = true;
    g_nntp_link.capabilities = {};
    InSequence sequence;
    expect_authentication();
    expect_reply("CAPABILITIES", {"500 unknown command"});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, sendsPasswordOnlyForPasswordRequiredReply)
{
    InSequence sequence;
    expect_reply("AUTHINFO USER reader", {"200 unrelated success"});

    EXPECT_LT(nntp_handle_auth_err(), 0);
    EXPECT_EQ(NntpAuthState::Unauthenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, requiresAuthenticationAcceptedAfterPassword)
{
    InSequence sequence;
    expect_reply("AUTHINFO USER reader", {"381 password required"});
    expect_reply("AUTHINFO PASS secret", {"381 invalid continuation"});

    EXPECT_LT(nntp_handle_auth_err(), 0);
    EXPECT_EQ(NntpAuthState::Unauthenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, doesNotRecursivelyAuthenticateAnAuthinfoChallenge)
{
    InSequence sequence;
    expect_reply("AUTHINFO USER reader", {"480 invalid authentication challenge"});

    EXPECT_LT(nntp_handle_auth_err(), 0);
}

TEST_F(NntpAuthTest, doesNotAuthenticateAgainAfterSuccess)
{
    g_nntp_link.auth_state = NntpAuthState::Authenticated;
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    EXPECT_LT(nntp_handle_auth_err(), 0);
}

TEST_F(NntpAuthTest, refusesCleartextWhenTlsIsRequired)
{
    g_nntp_link.require_tls_for_auth = true;
    g_nntp_link.allow_starttls = false;
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    EXPECT_LT(nntp_handle_auth_err(), 0);
    EXPECT_EQ(483, nntp_response_code(g_ser_line));
}

TEST_F(NntpAuthTest, closeClearsAuthenticationState)
{
    g_nntp_link.auth_state = NntpAuthState::Authenticated;

    nntp_close(false);

    EXPECT_EQ(NntpAuthState::Unauthenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, doesNotUpgradeAfterAuthentication)
{
    g_nntp_link.auth_state = NntpAuthState::Authenticated;
    g_nntp_link.capabilities.add("STARTTLS");
    EXPECT_CALL(*m_connection, write_line(_, _)).Times(0);

    EXPECT_LT(nntp_start_tls(), 0);
}

TEST_F(NntpAuthTest, selectsAuthMethodAfterRequiredTlsUpgrade)
{
    g_nntp_link.require_tls_for_auth = true;
    g_nntp_link.capabilities = {};
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("STARTTLS");
    g_nntp_link.capabilities.add("AUTHINFO SASL");
    InSequence sequence;
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "AUTHINFO USER", "."});
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, acceptsImplicitTlsForPasswordAuthentication)
{
    g_nntp_link.require_tls_for_auth = true;
    g_nntp_link.allow_starttls = false;
    g_nntp_link.security = NntpSecurity::ImplicitTls;
    InSequence sequence;
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, doesNotRepeatAuthenticationForAnotherChallenge)
{
    InSequence sequence;
    expect_reply("GROUP comp.lang.apl", {"480 authentication required"});
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    expect_reply("GROUP comp.lang.apl", {"480 still denied"});

    ASSERT_EQ(1, nntp_command("GROUP comp.lang.apl"));
    EXPECT_LE(nntp_check(), 0);
    EXPECT_EQ(480, nntp_response_code(g_ser_line));
}

TEST_F(NntpAuthTest, doesNotReplayReaderModeAfterAuthentication)
{
    configure_factory_create(m_connection);
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "MODE-READER", "AUTHINFO USER", "."});
    expect_reply("MODE READER", {"480 authentication required"});
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});

    EXPECT_EQ(200, server_init(m_machine));
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, skipsReaderModeAfterDiscoveryAuthentication)
{
    configure_factory_create(m_connection);
    g_nntp_link.allow_legacy_auth = true;
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"480 authentication required"});
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "MODE-READER", "."});

    EXPECT_EQ(200, server_init(m_machine));
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, restartsUserPassAfterPasswordRequiresTls)
{
    g_nntp_link.capabilities.add("STARTTLS");
    InSequence sequence;
    expect_reply("AUTHINFO USER reader", {"381 password required"});
    expect_reply("AUTHINFO PASS secret", {"483 encryption required"});
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "AUTHINFO USER", "."});
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
}

TEST_F(NntpAuthTest, reselectsMethodAfterAuthinfoRequiresTls)
{
    g_nntp_link.capabilities.add("STARTTLS");
    InSequence sequence;
    expect_reply("AUTHINFO USER reader", {"483 encryption required"});
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "AUTHINFO SASL", "SASL SCRAM-SHA-256", "."});

    EXPECT_LT(nntp_handle_auth_err(), 0);
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
    EXPECT_EQ(NntpAuthState::Unauthenticated, g_nntp_link.auth_state);
}

TEST_F(NntpAuthTest, failsStartupWhenPostAuthDiscoveryFailsInReaderMode)
{
    configure_factory_create(m_connection);
    InSequence sequence;
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 server ready"));
    expect_reply("CAPABILITIES", {"101 capabilities follow", "MODE-READER", "AUTHINFO USER", "."});
    expect_reply("MODE READER", {"480 authentication required"});
    expect_authentication();
    expect_reply("CAPABILITIES", {"502 discovery denied"});

    EXPECT_EQ(502, server_init(m_machine));
}

TEST_F(NntpAuthTest, authenticatesOnceWhenTlsDiscoveryNeedsCredentials)
{
    g_nntp_link.require_tls_for_auth = true;
    g_nntp_link.allow_legacy_auth = true;
    g_nntp_link.capabilities.add("STARTTLS");
    InSequence sequence;
    expect_reply("STARTTLS", {"382 continue with TLS negotiation"});
    EXPECT_CALL(*m_connection, start_tls(_));
    expect_reply("CAPABILITIES", {"480 authentication required"});
    expect_authentication();
    expect_reply("CAPABILITIES", {"101 capabilities follow", "READER", "."});
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));

    EXPECT_EQ(1, nntp_handle_auth_err());
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
    EXPECT_EQ(NntpAuthState::Authenticated, g_nntp_link.auth_state);
}

TEST_F(NNTPTest, startupClearsPreviousAuthenticationState)
{
    configure_factory_create(nullptr);
    g_nntp_link.auth_state = NntpAuthState::Authenticated;

    EXPECT_EQ(-1, server_init(m_machine));
    EXPECT_EQ(NntpAuthState::Unauthenticated, g_nntp_link.auth_state);
}

class NntpTlsLoopbackTest : public NNTPTest
{
protected:
    void SetUp() override;
    void TearDown() override;
    void set_ca_file(const std::string &value);

    fs::path    m_certificate;
    std::string m_old_ca_file;
};

void NntpTlsLoopbackTest::set_ca_file(const std::string &value)
{
#ifdef _WIN32
    ASSERT_EQ(0, _putenv_s("SSL_CERT_FILE", value.c_str()));
#else
    ASSERT_EQ(0, value.empty() ? unsetenv("SSL_CERT_FILE") : setenv("SSL_CERT_FILE", value.c_str(), 1));
#endif
}

void NntpTlsLoopbackTest::SetUp()
{
    NNTPTest::SetUp();
    init_nntp();
    const char *old_value = std::getenv("SSL_CERT_FILE");
    m_old_ca_file = old_value ? old_value : "";
    m_certificate = fs::path{TRN_TEST_TMP_DIR} / "NntpTlsLoopbackTest" /
                    testing::UnitTest::GetInstance()->current_test_info()->name() / "ca.pem";
    fs::create_directories(m_certificate.parent_path());
    std::ofstream{m_certificate} << NNTP_TEST_CERTIFICATE;
    set_ca_file(m_certificate.string());
}

void NntpTlsLoopbackTest::TearDown()
{
    nntp_close(false);
    set_ca_file(m_old_ca_file);
    std::error_code ec;
    fs::remove_all(m_certificate.parent_path(), ec);
    NNTPTest::TearDown();
}

TEST_F(NntpTlsLoopbackTest, readsArticleThroughStartTls)
{
    MockNntpServer server;
    server.expect_capabilities({"READER", "STARTTLS"});
    server.expect_starttls();
    server.expect_capabilities({"READER"});
    server.expect_group("comp.lang.apl", 1, 1, 1);
    server.expect_body(1, {"encrypted article", "second line"});
    server.expect_quit();
    server.start();
    g_nntp_link.port_number = server.port();

    ASSERT_EQ(1, nntp_connect("localhost", false));
    EXPECT_EQ(NntpSecurity::StartTls, g_nntp_link.security);
    ASSERT_EQ(1, nntp_command("GROUP comp.lang.apl"));
    ASSERT_EQ(1, nntp_check());
    ASSERT_EQ(1, nntp_command("BODY 1"));
    ASSERT_EQ(1, nntp_check());
    std::string line;
    ASSERT_EQ(NGSR_FULL_LINE, nntp_gets(line, NNTP_STRLEN));
    EXPECT_EQ("encrypted article", line);
    ASSERT_EQ(NGSR_FULL_LINE, nntp_gets(line, NNTP_STRLEN));
    EXPECT_EQ("second line", line);
    nntp_finish_list();
    nntp_close(true);
    server.verify();
}

TEST_F(NntpTlsLoopbackTest, rejectsUntrustedStartTlsCertificate)
{
    set_ca_file((m_certificate.parent_path() / "absent.pem").string());
    MockNntpServer server;
    server.expect_capabilities({"READER", "STARTTLS"});
    server.expect_starttls(true);
    server.start();
    g_nntp_link.port_number = server.port();

    EXPECT_EQ(0, nntp_connect("localhost", false));
    EXPECT_FALSE(g_nntp_link.connection);
    server.verify();
}

TEST_F(NntpTlsLoopbackTest, rejectsStartTlsCertificateForAnotherHost)
{
    MockNntpServer server;
    server.expect_capabilities({"READER", "STARTTLS"});
    server.expect_starttls(true);
    server.start();
    g_nntp_link.port_number = server.port();

    EXPECT_EQ(0, nntp_connect("127.0.0.1", false));
    EXPECT_FALSE(g_nntp_link.connection);
    server.verify();
}

TEST_F(NntpTlsLoopbackTest, readsArticleThroughImplicitTls)
{
    MockNntpServer server;
    server.use_implicit_tls();
    server.expect_capabilities({"READER"});
    server.expect_group("comp.lang.apl", 1, 1, 1);
    server.expect_body(1, {"encrypted article", "second line"});
    server.expect_quit();
    server.start();
    g_nntp_link.port_number = server.port();
    g_nntp_link.implicit_tls = true;
    g_nntp_link.allow_starttls = false;

    ASSERT_EQ(1, nntp_connect("localhost", false));
    EXPECT_EQ(NntpSecurity::ImplicitTls, g_nntp_link.security);
    ASSERT_EQ(1, nntp_command("GROUP comp.lang.apl"));
    ASSERT_EQ(1, nntp_check());
    ASSERT_EQ(1, nntp_command("BODY 1"));
    ASSERT_EQ(1, nntp_check());
    std::string line;
    ASSERT_EQ(NGSR_FULL_LINE, nntp_gets(line, NNTP_STRLEN));
    EXPECT_EQ("encrypted article", line);
    ASSERT_EQ(NGSR_FULL_LINE, nntp_gets(line, NNTP_STRLEN));
    EXPECT_EQ("second line", line);
    nntp_finish_list();
    nntp_close(true);
    server.verify();
}

TEST_F(NntpTlsLoopbackTest, rejectsUntrustedImplicitTlsCertificate)
{
    set_ca_file((m_certificate.parent_path() / "absent.pem").string());
    MockNntpServer server;
    server.use_implicit_tls(true);
    server.start();
    g_nntp_link.port_number = server.port();
    g_nntp_link.implicit_tls = true;

    EXPECT_EQ(0, nntp_connect("localhost", false));
    EXPECT_FALSE(g_nntp_link.connection);
    server.verify();
}

TEST_F(NntpTlsLoopbackTest, rejectsImplicitTlsCertificateForAnotherHost)
{
    MockNntpServer server;
    server.use_implicit_tls(true);
    server.start();
    g_nntp_link.port_number = server.port();
    g_nntp_link.implicit_tls = true;

    EXPECT_EQ(0, nntp_connect("127.0.0.1", false));
    EXPECT_FALSE(g_nntp_link.connection);
    server.verify();
}

struct NntpCommandCase
{
    const char *modern;
    const char *legacy;
    const char *capability;
    const char *response;
    const char *legacy_response;
    int (*request)();
};

class NntpCommandTest : public NNTPGetStringTest, public WithParamInterface<NntpCommandCase>
{
protected:
    void expect_status(std::string_view command, const char *response)
    {
        EXPECT_CALL(*m_connection, write_line(StrEq(std::string{command}), _));
        EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return(std::string{response}));
    }

    void expect_list(std::string_view command)
    {
        expect_status(command, command == GetParam().legacy ? GetParam().legacy_response : GetParam().response);
        EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("."));
    }
};

TEST_P(NntpCommandTest, prefersAdvertisedModernCommand)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add(GetParam().capability);
    InSequence sequence;
    expect_list(GetParam().modern);

    EXPECT_EQ(1, GetParam().request());
    EXPECT_FALSE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
    nntp_finish_list();
    EXPECT_TRUE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
}

TEST_P(NntpCommandTest, cachesModernRejectionAndUsesLegacyCommand)
{
    InSequence sequence;
    expect_status(GetParam().modern, "500 unknown command");
    expect_list(GetParam().legacy);
    expect_list(GetParam().legacy);

    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
}

TEST_P(NntpCommandTest, fallsBackWhenAdvertisedModernCommandIsRejected)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add(GetParam().capability);
    InSequence sequence;
    expect_status(GetParam().modern, "500 unknown command");
    expect_list(GetParam().legacy);
    expect_list(GetParam().legacy);

    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
}

TEST_P(NntpCommandTest, cachesRejectionOfBothCommands)
{
    InSequence sequence;
    expect_status(GetParam().modern, "500 unknown command");
    expect_status(GetParam().legacy, "500 unknown command");

    EXPECT_EQ(-1, GetParam().request());
    EXPECT_EQ(-1, GetParam().request());
}

TEST_P(NntpCommandTest, doesNotProbeUnadvertisedCommands)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("VERSION 2");

    EXPECT_EQ(-1, GetParam().request());
    EXPECT_EQ(-1, GetParam().request());
}

TEST_P(NntpCommandTest, usesAdvertisedLegacyExtension)
{
    g_nntp_link.capabilities.available = true;
    const std::string_view command{GetParam().legacy};
    g_nntp_link.capabilities.add(command.substr(0, command.find(' ')));
    InSequence sequence;
    expect_list(GetParam().legacy);

    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
}

TEST_P(NntpCommandTest, doesNotCacheTemporaryErrorsOrFallBack)
{
    InSequence sequence;
    expect_status(GetParam().modern, "403 temporary failure");
    expect_list(GetParam().modern);

    EXPECT_EQ(0, GetParam().request());
    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
}

TEST_P(NntpCommandTest, refreshClearsRejectedCommandState)
{
    InSequence sequence;
    expect_status(GetParam().modern, "500 unknown command");
    expect_status(GetParam().legacy, "500 unknown command");
    expect_status("CAPABILITIES", "101 capabilities follow");
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("VERSION 2"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return(std::string{GetParam().capability}));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("."));
    expect_list(GetParam().modern);

    EXPECT_EQ(-1, GetParam().request());
    EXPECT_EQ(1, nntp_refresh_capabilities());
    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
}

TEST_P(NntpCommandTest, closeClearsRejectedCommandState)
{
    InSequence sequence;
    expect_status(GetParam().modern, "500 unknown command");
    expect_status(GetParam().legacy, "500 unknown command");
    expect_list(GetParam().modern);

    EXPECT_EQ(-1, GetParam().request());
    nntp_close(false);
    g_nntp_link.connection = m_connection;
    EXPECT_EQ(1, GetParam().request());
    nntp_finish_list();
}

INSTANTIATE_TEST_SUITE_P(
    ModernCommands, NntpCommandTest,
    Values(NntpCommandCase{"OVER 1-5", "XOVER 1-5", "OVER", "224 overview follows", "224 overview follows",
                           [] { return nntp_overview("1-5"); }},
           NntpCommandCase{"HDR subject 1-5", "XHDR subject 1-5", "HDR", "225 headers follow", "221 headers follow",
                           [] { return nntp_header_values("subject", "1-5"); }},
           NntpCommandCase{"LIST NEWSGROUPS comp.lang.apl", "XGTITLE comp.lang.apl", "LIST ACTIVE NEWSGROUPS",
                           "215 descriptions follow", "282 descriptions follow",
                           [] { return nntp_group_description("comp.lang.apl"); }}),
    [](const TestParamInfo<NntpCommandCase> &info)
    {
        return std::string{info.index == 0 ? "Overview" : info.index == 1 ? "Headers" : "Descriptions"};
    });

TEST_F(NNTPGetStringTest, listVariantRejectionIsCached)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(StrEq("LIST NEWSGROUPS comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("501 unknown LIST variant"));
    EXPECT_CALL(*m_connection, write_line(StrEq("XGTITLE comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("282 descriptions follow"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("."));
    EXPECT_CALL(*m_connection, write_line(StrEq("XGTITLE comp.lang.c++"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("282 descriptions follow"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("."));

    EXPECT_EQ(1, nntp_group_description("comp.lang.apl"));
    nntp_finish_list();
    EXPECT_EQ(1, nntp_group_description("comp.lang.c++"));
    nntp_finish_list();
}

TEST_F(NNTPGetStringTest, overviewSyntaxErrorDoesNotDisableCommand)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(StrEq("OVER 1-5"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("501 invalid range"));
    EXPECT_CALL(*m_connection, write_line(StrEq("OVER 1-6"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("224 overview follows"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("."));

    EXPECT_EQ(-1, nntp_overview("1-5"));
    EXPECT_EQ(1, nntp_overview("1-6"));
    nntp_finish_list();
}

TEST_F(NNTPGetStringTest, rejectionCacheHandlesDifferentCommandFamilies)
{
    InSequence sequence;
    for (const char *command : {"OVER 1-5", "XOVER 1-5", "HDR subject 1-5", "XHDR subject 1-5",
                                "LIST NEWSGROUPS comp.lang.apl", "XGTITLE comp.lang.apl"})
    {
        EXPECT_CALL(*m_connection, write_line(StrEq(command), _));
        EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("500 unknown command"));
    }

    EXPECT_EQ(-1, nntp_overview("1-5"));
    EXPECT_EQ(-1, nntp_header_values("subject", "1-5"));
    EXPECT_EQ(-1, nntp_group_description("comp.lang.apl"));
    EXPECT_EQ(-1, nntp_group_description("comp.lang.c++"));
    EXPECT_EQ(-1, nntp_header_values("from", "6-10"));
    EXPECT_EQ(-1, nntp_overview("6-10"));
}

TEST_F(NNTPGetStringTest, overviewFormatRejectionIsCached)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("LIST OVERVIEW.FMT"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("501 unknown LIST variant"));

    EXPECT_EQ(-1, nntp_overview_format());
    EXPECT_EQ(-1, nntp_overview_format());
}

TEST_F(NNTPGetStringTest, capabilitiesConsumeCompleteListBeforeNextCommand)
{
    InSequence sequence;
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("101 capabilities follow\r\n"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("VERSION 2\r\n"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("LIST ACTIVE NEWSGROUPS\r\n"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("OVER MSGID\r\n"));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return(".\r\n"));
    EXPECT_CALL(*m_connection, write_line(StrEq("DATE"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("111 20261019010000\r\n"));

    EXPECT_EQ(1, nntp_refresh_capabilities());
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_TRUE(g_nntp_link.capabilities.has("VERSION", "2"));
    EXPECT_TRUE(g_nntp_link.capabilities.has("LIST", "ACTIVE"));
    EXPECT_TRUE(g_nntp_link.capabilities.has("OVER", "MSGID"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("HDR"));
    EXPECT_TRUE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
    EXPECT_EQ(1, nntp_command("DATE"));
    EXPECT_EQ(1, nntp_check());
    EXPECT_EQ("111 20261019010000", g_ser_line);
}

TEST_F(NNTPGetStringTest, emptyCapabilityListIsAvailable)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("101 capabilities follow")).WillOnce(Return("."));

    EXPECT_EQ(1, nntp_refresh_capabilities());
    EXPECT_TRUE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("READER"));
}

TEST_F(NNTPGetStringTest, unsupportedCapabilitiesClearPreviousList)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("STARTTLS");
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("500 unknown command"));

    EXPECT_EQ(0, nntp_refresh_capabilities());
    EXPECT_FALSE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
    EXPECT_TRUE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
}

TEST_F(NNTPGetStringTest, truncatedCapabilitiesDiscardPartialListAndConnection)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("STARTTLS");
    EXPECT_CALL(*m_connection, write_line(StrEq("CAPABILITIES"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("101 capabilities follow"))
        .WillOnce(Return("READER"))
        .WillOnce(Invoke(
            [](error_code_t &ec)
            {
                ec = boost::asio::error::eof;
                return std::string{};
            }));

    EXPECT_EQ(-1, nntp_refresh_capabilities());
    EXPECT_FALSE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("STARTTLS"));
    EXPECT_FALSE(g_nntp_link.capabilities.has("READER"));
    EXPECT_FALSE(g_nntp_link.connection);
    EXPECT_TRUE(g_nntp_link.flags & NNTP_NEW_CMD_OK);
}

TEST_F(NNTPGetStringTest, closeDiscardsCapabilities)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("READER");

    nntp_close(false);

    EXPECT_FALSE(g_nntp_link.capabilities.available);
    EXPECT_FALSE(g_nntp_link.capabilities.has("READER"));
}

TEST_F(NNTPGetStringTest, getALineReturnsServerLine)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("server line")));

    const std::string line = nntp_get_a_line();

    EXPECT_EQ("server line", line);
}

TEST_F(NNTPGetStringTest, checkStoresStatusLineWithoutCrlf)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("200 ready\r\n"));

    EXPECT_EQ(1, nntp_check());

    EXPECT_EQ("200 ready", g_ser_line);
}

TEST_F(NNTPGetStringTest, checkStoresFullStatusLine)
{
    const std::string detail(static_cast<std::size_t>(NNTP_STRLEN) + 8, 'x');
    const std::string status_line{"200 " + detail};
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return(status_line));

    EXPECT_EQ(1, nntp_check());

    EXPECT_EQ(status_line, g_ser_line);
}

TEST_F(NNTPGetStringTest, string_line_fits)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this fits")));
    std::string line{"junk"};

    const NNTPGetsResult result = nntp_gets(line, 1024);

    EXPECT_EQ(NGSR_FULL_LINE, result);
    EXPECT_EQ("this fits", line);
}

TEST_F(NNTPGetStringTest, string_line_strips_crlf)
{
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(DoAll(SetArgReferee<0>(m_ec), Return("this fits\r\n")));
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

TEST_F(NNTPGetStringTest, statIdParsesArticleNumber)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT <message@example.test>"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 123 <message@example.test>"));

    EXPECT_EQ(ArticleNum{123}, nntp_stat_id("<message@example.test>"));
}

TEST_F(NNTPGetStringTest, statIdReturnsZeroForMalformedArticleNumber)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT <message@example.test>"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 <message@example.test>"));

    EXPECT_EQ(ArticleNum{}, nntp_stat_id("<message@example.test>"));
}

TEST_F(NNTPGetStringTest, findRealArticleParsesNextArticleNumber)
{
    InSequence sequence;
    g_abs_first = ArticleNum{1};
    g_last_cached = ArticleNum{100};
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT 100"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 100 <cached@example.test>"));
    EXPECT_CALL(*m_connection, write_line(StrEq("NEXT"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 123 <next@example.test>"));

    EXPECT_EQ(ArticleNum{123}, nntp_find_real_art(ArticleNum{100}));
}

TEST_F(NNTPGetStringTest, findRealArticleReturnsZeroForMalformedNextArticleNumber)
{
    InSequence sequence;
    g_abs_first = ArticleNum{1};
    g_last_cached = ArticleNum{100};
    EXPECT_CALL(*m_connection, write_line(StrEq("STAT 100"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 100 <cached@example.test>"));
    EXPECT_CALL(*m_connection, write_line(StrEq("NEXT"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("223 <next@example.test>"));

    EXPECT_EQ(ArticleNum{}, nntp_find_real_art(ArticleNum{100}));
}

TEST_F(NNTPGetStringTest, headerFormatsArticleNumberCommand)
{
    EXPECT_CALL(*m_connection, write_line(StrEq("HEAD 123"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("221 123 <message@example.test> header follows"));

    EXPECT_EQ(1, nntp_header(ArticleNum{123}));
}

TEST_F(NNTPGetStringTest, groupParsesArticleRange)
{
    NewsgroupData group{};
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.lang.apl"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("211 3 10 12 comp.lang.apl"));

    EXPECT_EQ(1, nntp_group("comp.lang.apl", &group));
    EXPECT_EQ(ArticleNum{10}, group.m_abs_first);
    EXPECT_EQ(ArticleNum{12}, group.m_ng_max);
}

TEST_F(NNTPGetStringTest, groupEmptyUsesArticleAfterPreviousMaximum)
{
    NewsgroupData group{};
    group.m_ng_max = ArticleNum{123};
    EXPECT_CALL(*m_connection, write_line(StrEq("GROUP comp.empty"), _));
    EXPECT_CALL(*m_connection, read_line(_)).WillOnce(Return("211 0 0 0 comp.empty"));

    EXPECT_EQ(1, nntp_group("comp.empty", &group));
    EXPECT_EQ(ArticleNum{124}, group.m_abs_first);
    EXPECT_EQ(ArticleNum{123}, group.m_ng_max);
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

    EXPECT_EQ("plain\n", nntp_read_art(NNTP_STRLEN));
    EXPECT_EQ(".dot-stuffed\n", nntp_read_art(NNTP_STRLEN));
    EXPECT_TRUE(nntp_read_art(NNTP_STRLEN).empty());
}

TEST_F(NNTPBodyTest, preservesYencBytesWhenRereadingCachedArticle)
{
    std::vector<std::string> lines{"220 7 <message@example.test> article follows", "Subject: binary", ""};
    std::string_view         text = YENC_ALL_BYTES;
    while (!text.empty())
    {
        const std::size_t end = text.find('\n');
        lines.emplace_back(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
    lines.emplace_back(".");
    EXPECT_CALL(*m_connection, write_line(StrEq("ARTICLE 7"), _));
    testing::internal::TypedExpectation<std::string(error_code_t &)> &reads = EXPECT_CALL(*m_connection, read_line(_));
    for (const std::string &line : lines)
    {
        reads.WillOnce(Return(line));
    }
    nntp_body(ArticleNum{7});
    for (int pass = 0; pass < 2; ++pass)
    {
        SCOPED_TRACE(pass);
        std::string article;
        for (std::string chunk = nntp_read_art(NNTP_STRLEN); !chunk.empty(); chunk = nntp_read_art(NNTP_STRLEN))
        {
            article += chunk;
        }
        const std::optional<YencPart> decoded = yenc_decode(article);
        ASSERT_TRUE(decoded);
        ASSERT_EQ(256U, decoded->data.size());
        for (int i = 0; i < 256; ++i)
        {
            EXPECT_EQ(i, static_cast<unsigned char>(decoded->data[i]));
        }
        ASSERT_EQ(0, nntp_seek_art(ArticlePosition{}));
    }
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

    EXPECT_TRUE(nntp_read_art(NNTP_STRLEN).empty());
}

TEST_F(NNTPBodyTest, requestsBodyWhenArticleHeaderIsParsed)
{
    const std::string header{"Subject: cached\n\n"};
    g_head_buf = header;
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
    EXPECT_CALL(*m_connection, write_line(StrEq("HDR from 1-1"), _));
    EXPECT_CALL(*m_connection, write_line(StrEq("XHDR from 1-1"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("500 unknown command"))
        .WillOnce(Return("221 Header follows"))
        .WillOnce(Return("1 Casey <casey@example.test>\r"))
        .WillOnce(Return("."));

    const std::string from = prefetch_lines_copy(ArticleNum{1}, FROM_LINE);

    EXPECT_EQ("Casey <casey@example.test>", from);
    EXPECT_EQ("Casey <casey@example.test>", article_ptr(ArticleNum{1})->get_cached_line_text(FROM_LINE, false));
}

TEST_F(RemoteHeaderPrefetchTest, readsHdrWithConfiguredBrokenXhdrPolicy)
{
    m_data_source.m_flags |= DF_XHDR_BROKEN;
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("HDR");
    EXPECT_CALL(*m_connection, write_line(StrEq("HDR from 1-1"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("225 headers follow"))
        .WillOnce(Return("1 Casey <casey@example.test>\r\n"))
        .WillOnce(Return("."));

    EXPECT_EQ("Casey <casey@example.test>", prefetch_lines_copy(ArticleNum{1}, FROM_LINE));
}

TEST_F(RemoteHeaderPrefetchTest, readsUncachedHeaderFromHdr)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("HDR");
    EXPECT_CALL(*m_connection, write_line(StrEq("HDR keywords 1"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("225 headers follow"))
        .WillOnce(Return("1 array programming\r\n"))
        .WillOnce(Return("."));

    EXPECT_EQ("array programming", prefetch_lines_copy(ArticleNum{1}, KEYW_LINE));
}

TEST_F(RemoteHeaderPrefetchTest, hdrEmptyValueDoesNotMarkArticleMissing)
{
    g_nntp_link.capabilities.available = true;
    g_nntp_link.capabilities.add("HDR");
    g_last_art = ArticleNum{2};
    article_ptr(ArticleNum{2})->m_flags = AF_EXISTS;
    EXPECT_CALL(*m_connection, write_line(StrEq("HDR from 1-2"), _));
    EXPECT_CALL(*m_connection, read_line(_))
        .WillOnce(Return("225 headers follow"))
        .WillOnce(Return("1\r\n"))
        .WillOnce(Return("2 Casey <casey@example.test>\r\n"))
        .WillOnce(Return("."));

    EXPECT_TRUE(prefetch_lines_copy(ArticleNum{1}, FROM_LINE).empty());
    EXPECT_TRUE(article_ptr(ArticleNum{1})->m_flags & AF_EXISTS);
    EXPECT_EQ("Casey <casey@example.test>", article_ptr(ArticleNum{2})->get_cached_line_text(FROM_LINE, false));
}
