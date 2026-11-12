#include <algorithm>
#include <functional>
#include <map>
#include <optional>
#include <thread>

#include <gtest/gtest.h>

#include <sihd/sys/linux/EventFd.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/sys/user.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys::linux_::dbus;
using sihd::sys::linux_::EventFd;

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
            Message request;
            auto built = request.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
            SIHD_UNEXPECTED_RETURN(built);
            Result<Message> reply = std::unexpected(Error(ErrorCode::none, "no reply"));
            std::jthread client([this, &request, &reply] { reply = request.call(_client, Duration(time::sec(5))); });
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
    Message succeeding;
    Message failing;
    auto succeeding_built = succeeding.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    auto failing_built = failing.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(succeeding_built.has_value()) << succeeding_built.error().message;
    ASSERT_TRUE(failing_built.has_value()) << failing_built.error().message;

    ASSERT_TRUE(succeeding.send(_client).has_value());
    auto succeeding_call = receive_member(_server, "Ping");
    ASSERT_TRUE(succeeding_call.has_value());
    auto answer = succeeding_call->method_return();
    ASSERT_TRUE(answer.has_value()) << answer.error().message;
    ASSERT_TRUE(answer->send(_server).has_value());
    auto success = receive_reply(_client);

    ASSERT_TRUE(failing.send(_client).has_value());
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
    Message request;
    auto built = request.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    auto invalid = request.add_object_path("not a path");
    ASSERT_TRUE(request.add_object_path("/org/sihd/Device/1").has_value());

    ASSERT_TRUE(request.send(_client).has_value());
    auto call = receive_member(_server, "Ping");

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(invalid.error().message, "invalid dbus path: not a path");
    ASSERT_TRUE(call.has_value());
    Iter argument(*call);
    EXPECT_EQ(argument.type(), 'o');
    EXPECT_EQ(argument.get<std::string>().value_or(""), "/org/sihd/Device/1");
}

TEST_F(TestDBusServer, test_received_call_metadata)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    Message request;
    auto built = request.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(request.add("hello").has_value());
    ASSERT_TRUE(request.add_signature("s").has_value());
    ASSERT_TRUE(request.set_no_reply().has_value());

    ASSERT_TRUE(request.send(_client).has_value());
    auto call = receive_member(_server, "Ping");
    auto locked = request.set_no_reply();

    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(call->destination(), "org.sihd.Test");
    EXPECT_EQ(call->signature(), "sg");
    EXPECT_FALSE(call->expects_reply());
    EXPECT_EQ(code_of(locked), ErrorCode::invalid_argument);
    EXPECT_EQ(error_of(locked), "dbus message was already sent or received");
}

TEST_F(TestDBusServer, test_request_name_denied_by_policy)
{
    auto denied = _server.request_name("org.sihd.Denied");

    ASSERT_EQ(code_of(denied), ErrorCode::permission_denied);
    EXPECT_TRUE(has_error_name(denied.error(), "org.freedesktop.DBus.Error.AccessDenied")) << denied.error().message;
}

TEST_F(TestDBusServer, test_queued_name)
{
    auto owned = _server.request_name("org.sihd.Test");
    auto queued = _client.request_name("org.sihd.Test", {.queue = true});

    auto released = _server.release_name("org.sihd.Test");
    auto acquired = receive_accepted(_client, [](const Message & message) {
        return message.member() == "NameAcquired" && Iter(message).get<std::string>().value_or("") == "org.sihd.Test";
    });
    auto owned_now = _client.request_name("org.sihd.Test");

    ASSERT_TRUE(owned.has_value()) << owned.error().message;
    EXPECT_EQ(*owned, NameRequest::primary_owner);
    ASSERT_TRUE(queued.has_value()) << queued.error().message;
    EXPECT_EQ(*queued, NameRequest::queued);
    EXPECT_TRUE(released.has_value());
    EXPECT_TRUE(acquired.has_value());
    ASSERT_TRUE(owned_now.has_value()) << owned_now.error().message;
    EXPECT_EQ(*owned_now, NameRequest::primary_owner);
}

TEST_F(TestDBusServer, test_replaced_name)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test", {.allow_replacement = true}).has_value());

    auto replaced = _client.request_name("org.sihd.Test", {.replace_existing = true});
    auto lost = receive_member(_server, "NameLost");

    ASSERT_TRUE(replaced.has_value()) << replaced.error().message;
    EXPECT_EQ(*replaced, NameRequest::primary_owner);
    ASSERT_TRUE(lost.has_value());
    EXPECT_EQ(lost->type(), Message::Type::signal);
    EXPECT_EQ(Iter(*lost).get<std::string>().value_or(""), "org.sihd.Test");
}

TEST_F(TestDBusServer, test_release_name_not_owned)
{
    auto unowned = _server.release_name("org.sihd.Test");
    ASSERT_TRUE(_client.request_name("org.sihd.Test").has_value());
    auto foreign = _server.release_name("org.sihd.Test");
    auto invalid = _server.release_name("not a name");

    EXPECT_EQ(code_of(unowned), ErrorCode::not_found);
    EXPECT_EQ(error_of(unowned), "dbus name org.sihd.Test has no owner");
    EXPECT_EQ(code_of(foreign), ErrorCode::invalid_argument);
    EXPECT_EQ(error_of(foreign), "dbus name org.sihd.Test is owned by another connection");
    EXPECT_EQ(error_of(invalid), "invalid dbus name: not a name");
}

TEST_F(TestDBusServer, test_caller_credentials)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    Message request;
    auto built = request.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(request.send(_client).has_value());
    auto call = receive_member(_server, "Ping");
    ASSERT_TRUE(call.has_value());

    auto credentials = _server.credentials(call->sender(), Duration(time::sec(5)));
    auto unknown = _server.credentials(":1.9999", Duration(time::sec(5)));

    ASSERT_TRUE(credentials.has_value()) << credentials.error().message;
    EXPECT_EQ(credentials->user, sihd::sys::user::effective_user());
    EXPECT_EQ(credentials->pid, sihd::sys::os::pid());
    for (const sihd::sys::user::GroupId & group : sihd::sys::user::groups())
        EXPECT_NE(std::ranges::find(credentials->groups, group), credentials->groups.end());
    ASSERT_FALSE(unknown.has_value());
    EXPECT_TRUE(has_error_name(unknown.error(), "org.freedesktop.DBus.Error.NameHasNoOwner"))
        << unknown.error().message;
}

TEST_F(TestDBusServer, test_unix_fd_argument)
{
    ASSERT_TRUE(_server.request_name("org.sihd.Test").has_value());
    EventFd event;
    ASSERT_TRUE(event.open().has_value());
    Message request;
    auto built = request.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(request.add_unix_fd(event.fd()).has_value());

    ASSERT_TRUE(request.send(_client).has_value());
    auto call = receive_member(_server, "Ping");
    ASSERT_TRUE(call.has_value());
    auto received = Iter(*call).get_unix_fd();
    auto not_a_fd = Iter(request).get<int32_t>();
    ASSERT_TRUE(received.has_value()) << received.error().message;
    ASSERT_TRUE(event.notify(3).has_value());

    uint64_t count = 0;
    const ssize_t read_size = read_fd(received->fd(), &count, sizeof(count));

    EXPECT_EQ(call->signature(), "h");
    EXPECT_NE(received->fd(), event.fd());
    EXPECT_EQ(read_size, static_cast<ssize_t>(sizeof(count)));
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(error_of(not_a_fd), "dbus type mismatch: expected 'i', got 'h'");
}

TEST_F(TestDBusServer, test_unix_fd_invalid)
{
    Message request;
    auto built = request.method_call("org.sihd.Test", "/org/sihd/Test", "org.sihd.Test", "Ping");
    ASSERT_TRUE(built.has_value()) << built.error().message;
    ASSERT_TRUE(request.add("not a fd").has_value());

    auto invalid = request.add_unix_fd(-1);
    auto mismatch = Iter(request).get_unix_fd();

    EXPECT_EQ(code_of(invalid), ErrorCode::invalid_argument);
    EXPECT_EQ(error_of(invalid), "cannot fstat a negative fd");
    EXPECT_EQ(error_of(mismatch), "dbus type mismatch: expected 'h', got 's'");
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
