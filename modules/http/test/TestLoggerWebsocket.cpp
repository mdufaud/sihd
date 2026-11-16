#include <optional>
#include <string>

#include <gtest/gtest.h>

#include <sihd/http/LoggerWebsocket.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/LoggerFilter.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;

class TestLoggerWebsocket: public ::testing::Test
{
    protected:
        TestLoggerWebsocket()
        {
            LoggerManager::stream();
            EXPECT_TRUE(sink.set_pattern("{level} {source} {msg}"));
            EXPECT_TRUE(sink.serve(scope.server, "logs"));
            scope.start(3002);
        }

        virtual ~TestLoggerWebsocket()
        {
            scope.stop();
            LoggerManager::clear_loggers();
        }

        // the logger counts a client before its switching protocols answer leaves
        static void connect(SimpleWsClient & client, int port_key, const char *protocol)
        {
            ASSERT_TRUE(client.connect(loopback_port(port_key)));
            ASSERT_TRUE(client.handshake(protocol));
        }

        void connect(SimpleWsClient & client) { connect(client, 3002, "logs"); }

        void log(std::string_view msg) { sink.log(LogInfo("test::ws", LogLevel::info), msg); }

        LoggerWebsocket sink {"ws-logs"};
        ServerScope scope;
};

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_client_receives_lines)
{
    SimpleWsClient client;
    this->connect(client);

    this->log("hello");
    this->log("world");

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("INFO test::ws hello"));
    EXPECT_EQ(client.recv_text(), std::optional<std::string>("INFO test::ws world"));
    EXPECT_EQ(sink.connection_count(), 1u);
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_every_client_receives_every_line)
{
    SimpleWsClient first;
    SimpleWsClient second;
    this->connect(first);
    this->connect(second);

    this->log("one");
    this->log("two");

    EXPECT_EQ(first.recv_text(), std::optional<std::string>("INFO test::ws one"));
    EXPECT_EQ(first.recv_text(), std::optional<std::string>("INFO test::ws two"));
    EXPECT_EQ(second.recv_text(), std::optional<std::string>("INFO test::ws one"));
    EXPECT_EQ(second.recv_text(), std::optional<std::string>("INFO test::ws two"));
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_late_client_gets_new_lines_only)
{
    SimpleWsClient early;
    this->connect(early);
    this->log("before");
    ASSERT_EQ(early.recv_text(), std::optional<std::string>("INFO test::ws before"));

    SimpleWsClient late;
    this->connect(late);
    this->log("after");

    EXPECT_EQ(late.recv_text(), std::optional<std::string>("INFO test::ws after"));
    EXPECT_EQ(early.recv_text(), std::optional<std::string>("INFO test::ws after"));
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_ignores_inbound_messages)
{
    SimpleWsClient client;
    this->connect(client);

    ASSERT_TRUE(client.send_text("ignored"));
    this->log("only line");

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("INFO test::ws only line"));
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_server_stop_releases_clients)
{
    SimpleWsClient client;
    this->connect(client);

    scope.stop();

    EXPECT_EQ(sink.connection_count(), 0u);
    this->log("after the server stopped");
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_installed_sink_pushes_manager_records)
{
    auto *installed = new LoggerWebsocket("installed");
    ASSERT_TRUE(installed->set_conf_str("pattern", "{level} {source} {msg}").has_value());
    LoggerFilter::Options options;
    options.source_regex = "^sihd::.*";
    installed->add_filter(new LoggerFilter(options));
    LoggerManager::add(installed);
    ServerScope installed_scope;
    ASSERT_TRUE(installed->serve(installed_scope.server, "installed"));
    installed_scope.start(3003);
    SimpleWsClient client;
    connect(client, 3003, "installed");

    LoggerManager::log(LogInfo("test::ws", LogLevel::notice), "through the manager");

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("NOTICE test::ws through the manager"));
    installed_scope.stop();
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_unattached_is_silent)
{
    LoggerWebsocket unattached("unattached");

    unattached.log(LogInfo("test::ws", LogLevel::info), "dropped");

    EXPECT_EQ(unattached.connection_count(), 0u);
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_full_queue_warns_once)
{
    LoggerWebsocket bounded("bounded");
    ServerScope bounded_scope;
    ASSERT_TRUE(bounded_scope.server.set_websocket_max_pending(1));
    ASSERT_TRUE(bounded.serve(bounded_scope.server, "bounded"));
    bounded_scope.start(3003);
    SimpleWsClient client;
    connect(client, 3003, "bounded");

    testing::internal::CaptureStderr();
    for (int i = 0; i < 200; ++i)
        bounded.log(LogInfo("test::ws", LogLevel::info), "burst");
    const std::string warned = testing::internal::GetCapturedStderr();

    EXPECT_EQ(warned, "bounded: a 'bounded' connection queue is full, dropping messages\n");
    bounded_scope.stop();
}

TEST_F(TestLoggerWebsocket, test_loggerwebsocket_pattern)
{
    LoggerWebsocket configured("configured");

    EXPECT_TRUE(configured.set_pattern(""));
    EXPECT_FALSE(configured.set_pattern("{nope}"));
}

} // namespace test
