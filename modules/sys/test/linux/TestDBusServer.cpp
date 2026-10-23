#include <functional>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;

class TestDBusServer: public ::testing::Test
{
    protected:
        TestDBusServer() { LoggerManager::stream(); }

        virtual ~TestDBusServer() { LoggerManager::clear_loggers(); }

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
        }

        virtual void TearDown() {}

        // the client blocks in call() while the server answers with 'answer'
        Result<Message> call_answered_by(const std::function<void(const Message &)> & answer)
        {
            auto request = Message::method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
            SIHD_UNEXPECTED_RETURN(request);
            Result<Message> reply = std::unexpected(Error(ErrorCode::none, "no reply"));
            std::jthread client([this, &request, &reply] { reply = request->call(_client, Duration(time::sec(5))); });
            auto call = receive_member(_server, "Ping");
            EXPECT_TRUE(call.has_value());
            if (call.has_value())
                answer(*call);
            client.join();
            return reply;
        }

        PrivateDBus _bus;
        Connection _server;
        Connection _client;
};

TEST_F(TestDBusServer, test_request_name_taken)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());

    auto owned_again = _server.request_name("org.sihd.Test");
    auto taken = _client.request_name("org.sihd.Test");

    EXPECT_TRUE(owned_again.has_value());
    ASSERT_FALSE(taken.has_value());
    EXPECT_EQ(taken.error().code, ErrorCode::already_exists);
    EXPECT_EQ(taken.error().message, "dbus name org.sihd.Test is owned by another connection");
}

TEST_F(TestDBusServer, test_request_invalid_name)
{
    auto invalid = _server.request_name("not a name");

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(invalid.error().message, "invalid dbus name: not a name");
}

TEST_F(TestDBusServer, test_method_return)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());

    auto reply = this->call_answered_by([this](const Message & call) {
        auto answer = call.method_return();
        ASSERT_TRUE(answer.has_value()) << answer.error().message;
        ASSERT_TRUE(answer->add("pong").has_value());
        ASSERT_TRUE(answer->send(_server).has_value());
    });

    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    EXPECT_EQ(Iter(*reply).get<std::string>().value_or(""), "pong");
}

TEST_F(TestDBusServer, test_error_return)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());

    auto reply = this->call_answered_by([this](const Message & call) {
        auto invalid_name = call.error_return("no dots", "failed");
        auto answer = call.error_return("org.sihd.Error.Failed", "it failed");
        ASSERT_FALSE(invalid_name.has_value());
        EXPECT_EQ(invalid_name.error().message, "invalid dbus error name: no dots");
        ASSERT_TRUE(answer.has_value()) << answer.error().message;
        ASSERT_TRUE(answer->send(_server).has_value());
    });

    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().code, ErrorCode::io_error);
    EXPECT_EQ(reply.error().message, "org.sihd.Error.Failed: it failed");
}

} // namespace test
