#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

class TestDBusCall: public ::testing::Test
{
    protected:
        TestDBusCall() { LoggerManager::stream(); }

        virtual ~TestDBusCall() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!Connection::supported)
            {
                GTEST_SKIP() << "dbus not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto server_opened = _server.open(_bus.address());
            ASSERT_TRUE(server_opened.has_value()) << server_opened.error().message;
            auto client_opened = _client.open(_bus.address());
            ASSERT_TRUE(client_opened.has_value()) << client_opened.error().message;
            ASSERT_TRUE(_server.request_name(service).has_value());
        }

        virtual void TearDown() {}

        static std::vector<Message> pings(size_t count)
        {
            std::vector<Message> requests;
            for (size_t index = 0; index < count; ++index)
            {
                auto request = Message::method_call(service, "/org/sihd/Test", service, "Ping");
                EXPECT_TRUE(request.has_value());
                auto added = request->add(static_cast<uint32_t>(index));
                EXPECT_TRUE(added.has_value());
                requests.emplace_back(std::move(*request));
            }
            return requests;
        }

        // answers every call with its own argument, the last received first
        std::jthread echo_backwards(size_t count)
        {
            return std::jthread([this, count] {
                std::vector<Message> calls;
                for (size_t index = 0; index < count; ++index)
                {
                    auto call = receive_member(_server, "Ping");
                    ASSERT_TRUE(call.has_value());
                    calls.emplace_back(std::move(*call));
                }
                for (auto call = calls.rbegin(); call != calls.rend(); ++call)
                {
                    auto answer = call->method_return();
                    ASSERT_TRUE(answer.has_value()) << answer.error().message;
                    ASSERT_TRUE(answer->add(Iter(*call).get<uint32_t>().value_or(99)).has_value());
                    ASSERT_TRUE(answer->send(_server).has_value());
                }
            });
        }

        static constexpr ZStringView service = "org.sihd.Test";
        inline static const Duration timeout = Duration(time::sec(5));

        PrivateDBus _bus;
        Connection _server;
        Connection _client;
};

TEST_F(TestDBusCall, test_call_all_replies_in_request_order)
{
    auto server = this->echo_backwards(3);

    auto replies = Message::call_all(_client, pings(3), timeout);
    server.join();

    ASSERT_TRUE(replies.has_value()) << replies.error().message;
    std::vector<uint32_t> values;
    for (const Message & reply : *replies)
        values.emplace_back(Iter(reply).get<uint32_t>().value_or(99));
    EXPECT_EQ(values, (std::vector<uint32_t> {0, 1, 2}));
}

TEST_F(TestDBusCall, test_call_all_fails_on_an_error_reply)
{
    auto server = std::jthread([this] {
        auto first = receive_member(_server, "Ping");
        auto second = receive_member(_server, "Ping");
        ASSERT_TRUE(first.has_value());
        ASSERT_TRUE(second.has_value());
        auto answer = first->method_return();
        ASSERT_TRUE(answer.has_value()) << answer.error().message;
        ASSERT_TRUE(answer->send(_server).has_value());
        auto refusal = second->error_return("org.freedesktop.DBus.Error.AccessDenied", "not allowed");
        ASSERT_TRUE(refusal.has_value()) << refusal.error().message;
        ASSERT_TRUE(refusal->send(_server).has_value());
    });

    auto replies = Message::call_all(_client, pings(2), timeout);
    server.join();

    ASSERT_FALSE(replies.has_value());
    EXPECT_EQ(replies.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(replies.error().message, "org.freedesktop.DBus.Error.AccessDenied: not allowed");
}

TEST_F(TestDBusCall, test_unanswered_calls_time_out)
{
    const Duration short_timeout(time::ms(100));
    auto request = Message::method_call(service, "/org/sihd/Test", service, "Ping");
    ASSERT_TRUE(request.has_value()) << request.error().message;

    auto reply = request->call(_client, short_timeout);
    auto replies = Message::call_all(_client, pings(2), short_timeout);

    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().code, ErrorCode::timeout);
    EXPECT_TRUE(has_error_name(reply.error(), "org.freedesktop.DBus.Error.NoReply"));
    ASSERT_FALSE(replies.has_value());
    EXPECT_EQ(replies.error().code, ErrorCode::timeout);
    EXPECT_TRUE(has_error_name(replies.error(), "org.freedesktop.DBus.Error.NoReply"));
}

TEST_F(TestDBusCall, test_call_all_without_request)
{
    Connection closed;

    auto replies = Message::call_all(_client, {}, timeout);
    auto on_closed = Message::call_all(closed, pings(1), timeout);

    ASSERT_TRUE(replies.has_value()) << replies.error().message;
    EXPECT_TRUE(replies->empty());
    ASSERT_FALSE(on_closed.has_value());
    EXPECT_EQ(on_closed.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(on_closed.error().message, "dbus connection is not open");
}

TEST_F(TestDBusCall, test_receive_until)
{
    auto requests = pings(2);
    ASSERT_TRUE(requests[0].send(_client).has_value());
    ASSERT_TRUE(requests[1].send(_client).has_value());

    auto second = Message::receive_until(_server, timeout, [](const Message & message) -> Result<bool> {
        return message.member() == "Ping" && Iter(message).get<uint32_t>().value_or(99) == 1;
    });
    auto nothing_more = Message::receive_until(_server, Duration(time::ms(50)), [](const Message &) -> Result<bool> {
        return false;
    });
    auto refused = Message::receive_until(_client, timeout, [](const Message &) -> Result<bool> {
        return std::unexpected(Error(ErrorCode::invalid_argument, "unexpected message"));
    });

    ASSERT_TRUE(second.has_value()) << second.error().message;
    EXPECT_EQ(Iter(*second).get<uint32_t>().value_or(99), 1u);
    ASSERT_FALSE(nothing_more.has_value());
    EXPECT_EQ(nothing_more.error().code, ErrorCode::timeout);
    EXPECT_EQ(nothing_more.error().message, "dbus receive timed out");
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().message, "unexpected message");
}

TEST_F(TestDBusCall, test_to_error)
{
    auto requests = pings(2);
    ASSERT_TRUE(requests[0].send(_client).has_value());
    ASSERT_TRUE(requests[1].send(_client).has_value());
    auto first = receive_member(_server, "Ping");
    auto second = receive_member(_server, "Ping");
    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());

    auto denied = first->error_return("org.freedesktop.DBus.Error.AccessDenied", "not allowed");
    auto failed = second->error_return("org.sihd.Error.Failed", "it failed");
    ASSERT_TRUE(denied.has_value()) << denied.error().message;
    ASSERT_TRUE(failed.has_value()) << failed.error().message;

    const Error denied_error = denied->to_error();
    const Error failed_error = failed->to_error();
    const Error call_error = first->to_error();

    EXPECT_EQ(denied_error.code, ErrorCode::permission_denied);
    EXPECT_EQ(denied_error.message, "org.freedesktop.DBus.Error.AccessDenied: not allowed");
    EXPECT_EQ(failed_error.code, ErrorCode::io_error);
    EXPECT_EQ(failed_error.message, "org.sihd.Error.Failed: it failed");
    EXPECT_EQ(call_error.code, ErrorCode::invalid_argument);
    EXPECT_EQ(call_error.message, "dbus message is no error reply");
}

TEST_F(TestDBusCall, test_disable_auto_start)
{
    Connection no_auto_start;
    no_auto_start.disable_auto_start();
    ASSERT_TRUE(no_auto_start.open(_bus.address()).has_value());
    auto started = Message::method_call("org.sihd.Nobody", "/org/sihd/Test", service, "Ping");
    auto not_started = Message::method_call("org.sihd.Nobody", "/org/sihd/Test", service, "Ping");
    ASSERT_TRUE(started.has_value()) << started.error().message;
    ASSERT_TRUE(not_started.has_value()) << not_started.error().message;

    auto activation = started->call(_client, timeout);
    auto no_activation = not_started->call(no_auto_start, timeout);

    ASSERT_FALSE(activation.has_value());
    EXPECT_TRUE(has_error_name(activation.error(), "org.freedesktop.DBus.Error.ServiceUnknown"))
        << activation.error().message;
    ASSERT_FALSE(no_activation.has_value());
    EXPECT_TRUE(has_error_name(no_activation.error(), "org.freedesktop.DBus.Error.NameHasNoOwner"))
        << no_activation.error().message;
}

} // namespace test
