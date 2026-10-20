#include <string_view>

#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;

class TestHttpServerWebsocketUpgrade: public ::testing::Test
{
    protected:
        TestHttpServerWebsocketUpgrade() { LoggerManager::stream(); }
        virtual ~TestHttpServerWebsocketUpgrade() { LoggerManager::clear_loggers(); }
};

namespace
{

std::string ws_upgrade(int port,
                       std::string_view path,
                       std::string_view protocol,
                       std::string_view headers = {},
                       std::string_view host = "localhost")
{
    SimpleWsClient client;
    if (client.connect(port) == false)
        return "no connection";
    return client.upgrade(path, protocol, headers, host);
}

} // namespace

TEST_F(TestHttpServerWebsocketUpgrade, test_websocket_upgrade_runs_http_filter)
{
    SimpleServerScope scope;
    TestFilter filter;
    filter.blocked_uri = "/blocked";
    scope.server.set_http_filter(&filter);
    scope.start(shard_port(3002));

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/blocked", "proto-two"), "");
    EXPECT_EQ(filter.filter_calls, 1);
    EXPECT_EQ(filter.last_uri, "/blocked");

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/open", "proto-two"), SimpleWsClient::switching_protocols);
    EXPECT_EQ(filter.filter_calls, 2);
    EXPECT_EQ(filter.last_uri, "/open");
    EXPECT_EQ(filter.last_client_ip, "127.0.0.1");
    ASSERT_TRUE(scope.server.wait_for_open());
    EXPECT_EQ(scope.server._nopen, 1);
}

TEST_F(TestHttpServerWebsocketUpgrade, test_websocket_upgrade_requires_authentication)
{
    SimpleServerScope scope;
    TestAuth auth;
    scope.server.set_authenticator(&auth);
    scope.start(shard_port(3002));

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two"), "");
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Authorization: Bearer wrong-token\r\n"), "");
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Authorization: Basic YWRtaW46d3Jvbmc=\r\n"), "");

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Authorization: Bearer my-secret-token-123\r\n"),
              SimpleWsClient::switching_protocols);
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Authorization: Basic YWRtaW46c2VjcmV0\r\n"),
              SimpleWsClient::switching_protocols);
}

TEST_F(TestHttpServerWebsocketUpgrade, test_websocket_upgrade_checks_origin)
{
    SimpleServerScope scope;
    ASSERT_TRUE(scope.server.add_websocket_origin("http://trusted.example"));
    scope.start(shard_port(3002));

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two"), SimpleWsClient::switching_protocols);
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: http://localhost\r\n"),
              SimpleWsClient::switching_protocols);
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: http://trusted.example\r\n"),
              SimpleWsClient::switching_protocols);
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: HTTP://Trusted.Example\r\n"),
              SimpleWsClient::switching_protocols);

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: http://evil.example\r\n"), "");
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: http://localhost:9\r\n"), "");
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: null\r\n"), "");
}

TEST_F(TestHttpServerWebsocketUpgrade, test_websocket_upgrade_same_origin_needs_a_literal_host)
{
    SimpleServerScope scope;
    scope.start(shard_port(3002));

    EXPECT_EQ(ws_upgrade(shard_port(3002),
                         "/",
                         "proto-two",
                         shard_url("Origin: http://127.0.0.1:3002\r\n"),
                         shard_url("127.0.0.1:3002")),
              SimpleWsClient::switching_protocols);
    EXPECT_EQ(ws_upgrade(shard_port(3002),
                         "/",
                         "proto-two",
                         shard_url("Origin: http://rebound.example:3002\r\n"),
                         shard_url("rebound.example:3002")),
              "");
}

TEST_F(TestHttpServerWebsocketUpgrade, test_websocket_upgrade_any_origin_when_allowed)
{
    SimpleServerScope scope;
    ASSERT_TRUE(scope.server.add_websocket_origin("*"));
    scope.start(shard_port(3002));

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two", "Origin: http://evil.example\r\n"),
              SimpleWsClient::switching_protocols);
}

TEST_F(TestHttpServerWebsocketUpgrade, test_websocket_upgrade_refuses_the_http_protocol)
{
    SimpleServerScope scope;
    scope.start(shard_port(3002));

    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", ""), "");
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "http-server"), "");
    EXPECT_EQ(ws_upgrade(shard_port(3002), "/", "proto-two"), SimpleWsClient::switching_protocols);
}

TEST_F(TestHttpServerWebsocketUpgrade, test_h2c_upgrade_is_refused)
{
    SimpleServerScope scope;
    TestFilter filter;
    filter.blocked_uri = "/blocked";
    scope.server.set_http_filter(&filter);
    scope.start(shard_port(3002));

    RawHttpClient client;
    ASSERT_TRUE(client.connect(shard_port(3002)));

    ASSERT_TRUE(client.send("GET /blocked HTTP/1.1\r\n"
                            "Host: localhost\r\n"
                            "Connection: Upgrade, HTTP2-Settings\r\n"
                            "Upgrade: h2c\r\n"
                            "HTTP2-Settings: AAMAAABkAAQAoAAAAAIAAAAA\r\n\r\n"));

    EXPECT_FALSE(client.read_response().has_value());
    EXPECT_TRUE(client.wait_closed());
}

} // namespace test
