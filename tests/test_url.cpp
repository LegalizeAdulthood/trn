// This software is copyrighted as detailed in the LICENSE file.
// Copyright (c) 2026, Richard Thomson

#include <trn/url.h>

#include <file_contents.h>

#include <test_config.h>

#include <boost/asio.hpp>
#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

namespace asio = boost::asio;
namespace fs = std::filesystem;

constexpr std::string_view HTTP_RESPONSE{"HTTP response"};

void serve_http_response(asio::ip::tcp::acceptor &acceptor, std::string &request)
{
    asio::ip::tcp::socket socket{acceptor.get_executor()};
    acceptor.accept(socket);
    asio::read_until(socket, asio::dynamic_buffer(request), "\r\n");
    asio::write(socket, asio::buffer(HTTP_RESPONSE.data(), HTTP_RESPONSE.size()));
}

class UrlFetchTest : public testing::Test
{
protected:
    void SetUp() override
    {
        const testing::TestInfo *test_info = testing::UnitTest::GetInstance()->current_test_info();
        m_output_dir = fs::path{TRN_TEST_TMP_DIR} / test_info->test_suite_name() / test_info->name();

        std::error_code error;
        fs::remove_all(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
        fs::create_directories(m_output_dir, error);
        ASSERT_FALSE(error) << error.message();
    }

    void TearDown() override
    {
        std::error_code error;
        fs::remove_all(m_output_dir, error);
    }

    fs::path m_output_dir;
};

} // namespace

TEST_F(UrlFetchTest, downloadsHttpResponse)
{
    asio::io_context        context;
    asio::ip::tcp::acceptor acceptor{context, {asio::ip::address_v4::loopback(), 0}};
    const unsigned short    port = acceptor.local_endpoint().port();
    std::string             request;
    std::future<void>       server =
        std::async(std::launch::async, serve_http_response, std::ref(acceptor), std::ref(request));

    const bool result =
        url_get("http://127.0.0.1:" + std::to_string(port) + "/resource", (m_output_dir / "download").string().c_str());
    server.get();

    EXPECT_TRUE(result);
    EXPECT_EQ("GET /resource\r\n", request);
    EXPECT_EQ(HTTP_RESPONSE, file_contents(m_output_dir / "download"));
}

TEST(UrlTest, rejectsEmptyUrl)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Empty URL -- ignoring.\n", output);
}

TEST(UrlTest, rejectsMissingScheme)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("example.com/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Incomplete URL: example.com/path\n", output);
}

TEST(UrlTest, rejectsMissingHost)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("http:/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("URL needs a hostname: http:/path\n", output);
}

TEST(UrlTest, rejectsUnterminatedAddressLiteral)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://[2001:db8::1/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Bad address literal: gopher://[2001:db8::1/path\n", output);
}

TEST(UrlTest, rejectsNonnumericPort)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://example.com:abc/path", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Bad URL (non-numeric portnum): gopher://example.com:abc/path\n", output);
}

TEST(UrlTest, rejectsMissingPath)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://example.com:70", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Bad URL (path does not start with /): gopher://example.com:70\n", output);
}

TEST(UrlTest, rejectsFtpPathWithoutFilename)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("ftp://example.com/", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("Error: URL:ftp path has no final filename.\n", output);
}

TEST(UrlTest, parsesUnsupportedScheme)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("gopher://example.com:70/resource", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("\nURL type gopher not supported (yet?)\n", output);
}

TEST(UrlTest, parsesHostlessNewsUrl)
{
    testing::internal::CaptureStdout();
    const bool        result = url_get("news:/comp.lang.apl", "unused");
    const std::string output = testing::internal::GetCapturedStdout();

    EXPECT_FALSE(result);
    EXPECT_EQ("\nURL type news not supported (yet?)\n", output);
}
