#include <functional>
#include <map>
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

        static Result<void> add_string_entry(Appender & dict, const std::string & key, const std::string & value)
        {
            return dict.add_entry(key, "s", [&value](Appender & variant) { return variant.add(value); });
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

TEST_F(TestDBusServer, test_access_denied_reply)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());

    auto reply = this->call_answered_by([this](const Message & call) {
        auto answer = call.error_return("org.freedesktop.DBus.Error.AccessDenied", "not allowed");
        ASSERT_TRUE(answer.has_value()) << answer.error().message;
        ASSERT_TRUE(answer->send(_server).has_value());
    });

    ASSERT_FALSE(reply.has_value());
    EXPECT_EQ(reply.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(reply.error().message, "org.freedesktop.DBus.Error.AccessDenied: not allowed");
}

TEST_F(TestDBusServer, test_name_has_owner)
{
    auto before = _client.name_has_owner("org.sihd.Test");
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    auto after = _client.name_has_owner("org.sihd.Test");
    auto invalid = _client.name_has_owner("not a name");

    ASSERT_TRUE(before.has_value()) << before.error().message;
    EXPECT_FALSE(*before);
    ASSERT_TRUE(after.has_value()) << after.error().message;
    EXPECT_TRUE(*after);
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(invalid.error().message, "invalid dbus name: not a name");
}

TEST_F(TestDBusServer, test_sent_call_replies)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    auto succeeding = Message::method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    auto failing = Message::method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(succeeding.has_value()) << succeeding.error().message;
    ASSERT_TRUE(failing.has_value()) << failing.error().message;

    ASSERT_TRUE(succeeding->send(_client).has_value());
    auto succeeding_call = receive_member(_server, "Ping");
    ASSERT_TRUE(succeeding_call.has_value());
    auto answer = succeeding_call->method_return();
    ASSERT_TRUE(answer.has_value()) << answer.error().message;
    ASSERT_TRUE(answer->send(_server).has_value());
    auto success = receive_reply(_client);

    ASSERT_TRUE(failing->send(_client).has_value());
    auto failing_call = receive_member(_server, "Ping");
    ASSERT_TRUE(failing_call.has_value());
    auto refusal = failing_call->error_return("org.sihd.Error.Failed", "it failed");
    ASSERT_TRUE(refusal.has_value()) << refusal.error().message;
    ASSERT_TRUE(refusal->send(_server).has_value());
    auto failure = receive_reply(_client);

    EXPECT_EQ(succeeding_call->type(), Message::Type::method_call);
    ASSERT_TRUE(success.has_value());
    EXPECT_EQ(success->type(), Message::Type::method_return);
    EXPECT_EQ(success->error_name(), "");
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->type(), Message::Type::error);
    EXPECT_EQ(failure->error_name(), "org.sihd.Error.Failed");
    EXPECT_EQ(Iter(*failure).get<std::string>().value_or(""), "it failed");
}

TEST_F(TestDBusServer, test_object_path_argument)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    auto request = Message::method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(request.has_value()) << request.error().message;
    auto invalid = request->add_object_path("not a path");
    ASSERT_TRUE(request->add_object_path("/org/sihd/Device/1").has_value());

    ASSERT_TRUE(request->send(_client).has_value());
    auto call = receive_member(_server, "Ping");

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(invalid.error().message, "invalid dbus path: not a path");
    ASSERT_TRUE(call.has_value());
    Iter argument(*call);
    EXPECT_EQ(argument.type(), 'o');
    EXPECT_EQ(argument.get<std::string>().value_or(""), "/org/sihd/Device/1");
}

TEST_F(TestDBusServer, test_reply_dict_entries)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    auto reply = this->call_answered_by([this](const Message & call) {
        auto answer = call.method_return();
        ASSERT_TRUE(answer.has_value()) << answer.error().message;
        Appender body(*answer);
        auto properties = body.open_array("{sv}");
        ASSERT_TRUE(properties.has_value()) << properties.error().message;
        ASSERT_TRUE(add_string_entry(*properties, "Name", "wlan0").has_value());
        ASSERT_TRUE(add_string_entry(*properties, "Mode", "station").has_value());
        ASSERT_TRUE(body.close(std::move(*properties)).has_value());
        ASSERT_TRUE(answer->send(_server).has_value());
    });
    ASSERT_TRUE(reply.has_value()) << reply.error().message;

    std::map<std::string, std::string> properties;
    auto walked = Iter(*reply).for_each_entry<std::string>(
        [&properties](const std::string & key, Iter & value) -> Result<void> {
            auto text = value.get<std::string>();
            SIHD_UNEXPECTED_RETURN(text);
            properties.emplace(key, std::move(*text));
            return {};
        });
    auto stopped = Iter(*reply).for_each_entry<std::string>([](const std::string & key, Iter &) -> Result<void> {
        return std::unexpected(Error(ErrorCode::not_found, "no handler for {}", key));
    });

    ASSERT_TRUE(walked.has_value()) << walked.error().message;
    EXPECT_EQ(properties, (std::map<std::string, std::string> {{"Mode", "station"}, {"Name", "wlan0"}}));
    ASSERT_FALSE(stopped.has_value());
    EXPECT_EQ(stopped.error().code, ErrorCode::not_found);
    EXPECT_EQ(stopped.error().message, "no handler for Name");
}

} // namespace test
