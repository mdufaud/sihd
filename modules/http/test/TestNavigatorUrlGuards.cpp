#include <gtest/gtest.h>

#include <sihd/http/Navigator.hpp>
#include <sihd/util/Logger.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;

class TestNavigatorUrlGuards: public ::testing::Test
{
    protected:
        TestNavigatorUrlGuards() { LoggerManager::stream(); }
        virtual ~TestNavigatorUrlGuards() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNavigatorUrlGuards, test_navigator_ssrf_guard_scheme_less_host)
{
    Navigator navigator;
    navigator.clear_proxy();
    navigator.set_ssrf_guard(true);

    const std::vector<std::string> urls {
        shard_url("localhost:3001/api/hello"),
        "metadata.internal:80/latest",
        shard_url("[::1]:3001/api/hello"),
        shard_url("http://[::ffff:127.0.0.1]:3001/api/hello"),
        shard_url("http://[::]:3001/api/hello"),
        "http://100.100.100.200/latest",
    };

    for (const std::string & url : urls)
    {
        auto resp = navigator.get(url);
        ASSERT_FALSE(resp.has_value()) << url;
        EXPECT_EQ(resp.error().message, std::string("SSRF guard blocked a request to a private host: ") + url);
    }
}

TEST_F(TestNavigatorUrlGuards, test_navigator_guards_private_hosts_by_default)
{
    Navigator navigator;
    navigator.clear_proxy();

    auto resp = navigator.get(shard_url("http://127.0.0.1:3001/api/hello"));
    ASSERT_FALSE(resp.has_value());
    EXPECT_EQ(resp.error().message,
              shard_url("SSRF guard blocked a request to a private host: http://127.0.0.1:3001/api/hello"));

    EXPECT_FALSE(navigator.ws_connect(shard_url("ws://127.0.0.1:3013"), "proto-two"));
    EXPECT_FALSE(navigator.ws_connect(shard_url("WS://[::ffff:127.0.0.1]:3013"), "proto-two"));
    EXPECT_FALSE(navigator.ws_connect("file:///etc/passwd", "proto-two"));
}

TEST_F(TestNavigatorUrlGuards, test_navigator_refuses_non_http_scheme)
{
    Navigator navigator;
    allow_local_targets(navigator);

    for (const char *url : {"file:///etc/passwd",
                            "file:/etc/passwd",
                            "FILE:///etc/passwd",
                            "gopher://127.0.0.1:6379/_PING",
                            "ftp://localhost/x"})
    {
        auto resp = navigator.get(url);
        ASSERT_FALSE(resp.has_value()) << url;
        EXPECT_EQ(resp.error().message, std::string("Navigator: only http and https URLs are allowed: ") + url);
    }
}

TEST_F(TestNavigatorUrlGuards, test_navigator_ws_connect_keeps_the_query_string)
{
    HeadCaptureServer capture;
    ASSERT_TRUE(capture.start());

    const std::string url = fmt::format("ws://localhost:{}/chat?room=1&token=a%20b#top", capture.port());

    Navigator navigator;
    allow_local_targets(navigator);
    EXPECT_FALSE(navigator.ws_connect(url, "proto-two"));

    const std::string & head = capture.finish();
    EXPECT_EQ(head.substr(0, head.find("\r\n")), "GET /chat?room=1&token=a%20b HTTP/1.1");
}

TEST_F(TestNavigatorUrlGuards, test_navigator_ws_connect_refuses_a_socks_proxy)
{
    Navigator navigator;
    allow_local_targets(navigator);
    navigator.set_proxy("127.0.0.1:1", ProxyType::Socks5);

    auto connected = navigator.ws_connect(shard_url("ws://localhost:3013"), "proto-two");

    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::not_supported);
    EXPECT_EQ(connected.error().message, "Navigator: WebSocket only supports an HTTP proxy");
}

} // namespace test
