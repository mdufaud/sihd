#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/http/AWebsocketHandler.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/Waitable.hpp>

#include "http_test_helpers.hpp"

namespace test
{

using namespace sihd::http;
using namespace sihd::util;

namespace
{

std::string outcome(const Result<void> & sent)
{
    return sent ? "sent" : sent.error().message;
}

struct Recorder
{
        void add(const std::string & call, AWebsocketHandler *opened = nullptr)
        {
            {
                auto lock = waitable.guard();
                calls.push_back(call);
                if (opened != nullptr)
                    handler = opened;
            }
            waitable.notify_all();
        }

        bool wait_call(const std::string & call)
        {
            return waitable.wait_for(Duration(time::sec(2)),
                                     [&] { return std::ranges::find(calls, call) != calls.end(); });
        }

        std::vector<std::string> snapshot()
        {
            auto lock = waitable.guard();
            return calls;
        }

        AWebsocketHandler *opened_handler()
        {
            auto lock = waitable.guard();
            return handler;
        }

        Waitable waitable;
        std::vector<std::string> calls;
        AWebsocketHandler *handler = nullptr;
};

class RecordingWebsocket: public AWebsocketHandler
{
    public:
        RecordingWebsocket(IWebsocketConnection & connection, Recorder & recorder):
            AWebsocketHandler(connection),
            _recorder(recorder)
        {
        }

        ~RecordingWebsocket() { _recorder.add("destroyed"); }

        bool on_auth(HttpRequest & request) override
        {
            _recorder.add("auth " + request.url());
            return true;
        }

        void on_open() override { _recorder.add("open", this); }

        void on_text(std::string_view text) override { _recorder.add(fmt::format("text {}", text)); }

        void on_binary(ArrByteView data) override { _recorder.add(fmt::format("binary {} bytes", data.size())); }

        void on_peer_close(uint16_t code, std::string_view) override
        {
            _recorder.add(fmt::format("peer_close {}", code));
        }

        void on_close() override { _recorder.add("close"); }

    protected:
        Recorder & _recorder;
};

class EchoWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_text(std::string_view text) override { (void)this->send(text); }

        void on_binary(ArrByteView data) override { (void)this->send_binary(data); }
};

class GreetingWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_open() override
        {
            (void)this->send("first");
            (void)this->send("second");
            RecordingWebsocket::on_open();
        }
};

class FloodWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_open() override
        {
            (void)this->send("kept");
            (void)this->send("kept too");
            const Result<void> third = this->send("dropped");
            _recorder.add(fmt::format("third: {}, dropped {}", outcome(third), this->dropped()));
            RecordingWebsocket::on_open();
        }
};

class ClosingWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_open() override
        {
            (void)this->send("welcome");
            this->close();
            _recorder.add(fmt::format("after close: {}", outcome(this->send("refused"))));
            RecordingWebsocket::on_open();
        }
};

class LateSenderWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_close() override
        {
            _recorder.add(
                fmt::format("on close: {}, connected {}", outcome(this->send("too late")), this->is_connected()));
        }
};

class RefusingWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        bool on_auth(HttpRequest & request) override
        {
            RecordingWebsocket::on_auth(request);
            return false;
        }
};

class UserWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_open() override
        {
            _recorder.add(fmt::format("user {}", this->request().auth_user()));
            RecordingWebsocket::on_open();
        }
};

class SessionCookieWebsocket: public UserWebsocket
{
    public:
        using UserWebsocket::UserWebsocket;

        bool on_auth(HttpRequest & request) override
        {
            _recorder.add(fmt::format("auth {} room={} session={} ip={}",
                                      request.url(),
                                      request.query_param("room").value_or(""),
                                      request.cookie("session").value_or(""),
                                      request.client_ip()));
            request.set_auth_user("alice");
            return true;
        }
};

class ThrowingWebsocket: public RecordingWebsocket
{
    public:
        using RecordingWebsocket::RecordingWebsocket;

        void on_text(std::string_view) override { throw std::runtime_error("handler failure"); }
};

class StoppingWebsocket: public RecordingWebsocket
{
    public:
        StoppingWebsocket(IWebsocketConnection & connection, Recorder & recorder, HttpServer & server):
            RecordingWebsocket(connection, recorder),
            _server(server)
        {
        }

        void on_close() override
        {
            _server.request_stop();
            RecordingWebsocket::on_close();
        }

    private:
        HttpServer & _server;
};

} // namespace

class TestWebsocketHandler: public ::testing::Test
{
    protected:
        TestWebsocketHandler() { LoggerManager::stream(); }

        virtual ~TestWebsocketHandler()
        {
            scope.stop();
            LoggerManager::clear_loggers();
        }

        template <typename T>
        void start()
        {
            ASSERT_TRUE(scope.server.add_websocket<T>("test", recorder));
            scope.start(3002);
        }

        void connect(SimpleWsClient & client, std::string_view path = "/", std::string_view headers = {})
        {
            ASSERT_TRUE(client.connect(loopback_port(3002)));
            ASSERT_EQ(client.upgrade(path, "test", headers), SimpleWsClient::switching_protocols);
            ASSERT_TRUE(recorder.wait_call("open"));
        }

        Recorder recorder;
        ServerScope scope;
};

TEST_F(TestWebsocketHandler, test_websockethandler_hooks_in_order)
{
    this->start<RecordingWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    ASSERT_TRUE(client.send_text("hi"));
    ASSERT_TRUE(recorder.wait_call("text hi"));
    client.send_close();

    ASSERT_TRUE(recorder.wait_call("destroyed"));
    const std::vector<std::string> expected {"auth /", "open", "text hi", "peer_close 1000", "close", "destroyed"};
    EXPECT_EQ(recorder.snapshot(), expected);
}

TEST_F(TestWebsocketHandler, test_websockethandler_echo_text)
{
    this->start<EchoWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    ASSERT_TRUE(client.send_text("hello"));

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("hello"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_echo_binary)
{
    this->start<EchoWebsocket>();
    SimpleWsClient client;
    this->connect(client);
    const std::string bytes("\x00\x01\xff", 3);

    // FIN + binary opcode
    ASSERT_TRUE(client.send_frame(0x82, bytes));

    EXPECT_EQ(client.recv_binary(), std::optional<std::string>(bytes));
}

TEST_F(TestWebsocketHandler, test_websockethandler_binary_reaches_on_binary)
{
    this->start<RecordingWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    // FIN + binary opcode
    ASSERT_TRUE(client.send_frame(0x82, std::string("\x00\x01\xff", 3)));

    EXPECT_TRUE(recorder.wait_call("binary 3 bytes"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_sends_in_order)
{
    this->start<GreetingWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("first"));
    EXPECT_EQ(client.recv_text(), std::optional<std::string>("second"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_send_from_another_thread)
{
    this->start<RecordingWebsocket>();
    SimpleWsClient client;
    this->connect(client);
    AWebsocketHandler *handler = recorder.opened_handler();
    ASSERT_NE(handler, nullptr);

    EXPECT_TRUE(handler->send("pushed").has_value());

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("pushed"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_burst_from_another_thread_arrives_whole)
{
    this->start<RecordingWebsocket>();
    SimpleWsClient client;
    this->connect(client);
    AWebsocketHandler *handler = recorder.opened_handler();
    ASSERT_NE(handler, nullptr);

    std::vector<std::optional<std::string>> expected;
    for (int i = 0; i < 100; ++i)
    {
        expected.emplace_back(fmt::format("message {}", i));
        ASSERT_TRUE(handler->send(fmt::format("message {}", i)).has_value());
    }

    std::vector<std::optional<std::string>> received;
    for (int i = 0; i < 100; ++i)
        received.push_back(client.recv_text());

    EXPECT_EQ(received, expected);
}

TEST_F(TestWebsocketHandler, test_websockethandler_full_queue_drops)
{
    ASSERT_TRUE(scope.server.set_websocket_max_pending(2));
    this->start<FloodWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("kept"));
    EXPECT_EQ(client.recv_text(), std::optional<std::string>("kept too"));
    EXPECT_TRUE(recorder.wait_call("third: websocket send queue full at 2 messages, dropped 1"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_close_sends_the_queue_first)
{
    this->start<ClosingWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    EXPECT_EQ(client.recv_text(), std::optional<std::string>("welcome"));
    EXPECT_EQ(client.recv_close_code(), std::optional<uint16_t>(1000));
    EXPECT_TRUE(recorder.wait_call("after close: websocket connection closed"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_closed_refuses_sends)
{
    this->start<LateSenderWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    client.send_close();

    EXPECT_TRUE(recorder.wait_call("on close: websocket connection closed, connected false"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_on_auth_refuses_the_upgrade)
{
    this->start<RefusingWebsocket>();
    SimpleWsClient client;
    ASSERT_TRUE(client.connect(loopback_port(3002)));

    EXPECT_EQ(client.upgrade("/", "test"), "");

    ASSERT_TRUE(recorder.wait_call("destroyed"));
    const std::vector<std::string> expected {"auth /", "destroyed"};
    EXPECT_EQ(recorder.snapshot(), expected);
}

TEST_F(TestWebsocketHandler, test_websockethandler_request_carries_the_handshake)
{
    this->start<SessionCookieWebsocket>();
    SimpleWsClient client;

    this->connect(client, "/chat?room=blue", "Cookie: session=abc\r\n");

    const std::vector<std::string> calls = recorder.snapshot();
    EXPECT_EQ(calls.at(0), "auth /chat room=blue session=abc ip=127.0.0.1");
    EXPECT_EQ(calls.at(1), "user alice");
}

TEST_F(TestWebsocketHandler, test_websockethandler_request_carries_the_authenticated_user)
{
    TestAuth auth;
    scope.server.set_authenticator(&auth);
    this->start<UserWebsocket>();
    SimpleWsClient client;

    this->connect(client, "/", "Authorization: Basic YWRtaW46c2VjcmV0\r\n");

    EXPECT_TRUE(recorder.wait_call("user admin"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_throwing_hook_closes_the_connection)
{
    this->start<ThrowingWebsocket>();
    SimpleWsClient client;
    this->connect(client);

    ASSERT_TRUE(client.send_text("boom"));

    ASSERT_TRUE(recorder.wait_call("destroyed"));
    EXPECT_TRUE(recorder.wait_call("close"));
}

TEST_F(TestWebsocketHandler, test_websockethandler_request_stop_from_on_close)
{
    ASSERT_TRUE(scope.server.add_websocket<StoppingWebsocket>("test", recorder, scope.server));
    scope.start(3002);
    SimpleWsClient client;
    this->connect(client);

    scope.stop();

    EXPECT_TRUE(recorder.wait_call("destroyed"));
}

} // namespace test
