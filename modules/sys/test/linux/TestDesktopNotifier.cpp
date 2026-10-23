#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/DesktopNotifier.hpp>
#include <sihd/sys/Process.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

namespace
{

struct NotifyCall
{
        std::string app_name;
        uint32_t replaces_id = 0;
        std::string icon;
        std::string summary;
        std::string body;
        std::vector<std::string> actions;
        std::optional<uint8_t> urgency;
        std::optional<std::string> desktop_entry;
        int32_t timeout = 0;

        bool operator==(const NotifyCall &) const = default;
};

void read_hints(Iter & hints, NotifyCall & call)
{
    for (; hints.type() != '\0'; hints.next())
    {
        auto entry = hints.recurse();
        ASSERT_TRUE(entry.has_value()) << entry.error().message;
        const std::string key = entry->get<std::string>().value_or("");
        ASSERT_TRUE(entry->next());
        auto value = entry->recurse();
        ASSERT_TRUE(value.has_value()) << value.error().message;
        if (key == "urgency")
            call.urgency = value->get<uint8_t>().value_or(99);
        else if (key == "desktop-entry")
            call.desktop_entry = value->get<std::string>().value_or("");
    }
}

NotifyCall read_notify(const Message & message)
{
    NotifyCall call;
    Iter args(message);
    call.app_name = args.get<std::string>().value_or("");
    args.next();
    call.replaces_id = args.get<uint32_t>().value_or(99);
    args.next();
    call.icon = args.get<std::string>().value_or("");
    args.next();
    call.summary = args.get<std::string>().value_or("");
    args.next();
    call.body = args.get<std::string>().value_or("");

    args.next();
    call.actions = args.get_array<std::string>().value_or(std::vector<std::string> {"unreadable"});
    args.next();
    auto hints = args.recurse();
    if (hints.has_value())
        read_hints(*hints, call);
    args.next();
    call.timeout = args.get<int32_t>().value_or(99);
    return call;
}

} // namespace

class TestDesktopNotifier: public ::testing::Test
{
    protected:
        TestDesktopNotifier() { LoggerManager::stream(); }

        virtual ~TestDesktopNotifier() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!DesktopNotifier::supported)
            {
                GTEST_SKIP() << "desktop notifications not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto opened = _server.open(_bus.address());
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
        }

        virtual void TearDown()
        {
            _server.close();
            _client.terminate();
        }

        void own_notifications_name()
        {
            auto owned = _server.request_name("org.freedesktop.Notifications");
            ASSERT_TRUE(owned.has_value()) << owned.error().message;
        }

        void start_client(const std::vector<std::string> & args)
        {
            _client.add_argv(helper_path());
            _client.add_argv(args);
            _client.env_set("DBUS_SESSION_BUS_ADDRESS", _bus.address());
            _client.stdout_to(_client_output);
            _client.stderr_to(_client_error);
            ASSERT_TRUE(_client.execute().has_value());
        }

        int finish_client()
        {
            EXPECT_TRUE(_client.wait_any().value_or(false));
            EXPECT_TRUE(_client.terminate());
            return _client.return_code();
        }

        void answer_capabilities(const std::vector<std::string> & capabilities)
        {
            auto call = receive_member(_server, "GetCapabilities");
            ASSERT_TRUE(call.has_value());
            auto reply = call->method_return();
            ASSERT_TRUE(reply.has_value()) << reply.error().message;
            Appender body(*reply);
            auto list = body.open_array("s");
            ASSERT_TRUE(list.has_value()) << list.error().message;
            for (const std::string & capability : capabilities)
                ASSERT_TRUE(list->add(capability).has_value());
            ASSERT_TRUE(body.close(std::move(*list)).has_value());
            ASSERT_TRUE(reply->send(_server).has_value());
        }

        std::optional<NotifyCall> answer_notify(uint32_t id)
        {
            auto call = receive_member(_server, "Notify");
            if (!call.has_value())
                return std::nullopt;
            auto reply = call->method_return();
            if (!reply.has_value() || !reply->add(id).has_value() || !reply->send(_server).has_value())
                return std::nullopt;
            return read_notify(*call);
        }

        PrivateDBus _bus;
        Connection _server;
        Process _client;
        std::string _client_output;
        std::string _client_error;
};

TEST_F(TestDesktopNotifier, test_notify_escapes_markup_body)
{
    this->own_notifications_name();

    this->start_client({"notify",
                        "app_name=My App",
                        "app_id=my.app",
                        "icon=dialog-information",
                        "urgency=2",
                        "timeout_ms=1500",
                        "summary=Summary",
                        "body=Tom & <Jerry>"});
    this->answer_capabilities({"body", "body-markup"});
    auto call = this->answer_notify(42);
    const int return_code = this->finish_client();

    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(*call,
              (NotifyCall {
                  .app_name = "My App",
                  .replaces_id = 0,
                  .icon = "dialog-information",
                  .summary = "Summary",
                  .body = "Tom &amp; &lt;Jerry&gt;",
                  .actions = {},
                  .urgency = 2,
                  .desktop_entry = "my.app",
                  .timeout = 1500,
              }));
    EXPECT_EQ(return_code, 0) << _client_error;
    EXPECT_EQ(_client_output, "42\n");
}

TEST_F(TestDesktopNotifier, test_notify_plain_body_and_defaults)
{
    this->own_notifications_name();

    this->start_client({"notify", "summary=Summary", "body=Tom & <Jerry>"});
    this->answer_capabilities({"body"});
    auto call = this->answer_notify(7);
    const int return_code = this->finish_client();

    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(*call,
              (NotifyCall {
                  .app_name = "",
                  .replaces_id = 0,
                  .icon = "",
                  .summary = "Summary",
                  .body = "Tom & <Jerry>",
                  .actions = {},
                  .urgency = 1,
                  .desktop_entry = std::nullopt,
                  .timeout = -1,
              }));
    EXPECT_EQ(return_code, 0) << _client_error;
    EXPECT_EQ(_client_output, "7\n");
}

TEST_F(TestDesktopNotifier, test_notify_low_urgency_and_clamped_timeout)
{
    this->own_notifications_name();

    this->start_client({"notify", "urgency=0", "timeout_ms=99999999999", "summary=Summary"});
    this->answer_capabilities({"body"});
    auto call = this->answer_notify(3);
    const int return_code = this->finish_client();

    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(call->urgency, 0);
    EXPECT_EQ(call->timeout, 2147483647);
    EXPECT_EQ(return_code, 0) << _client_error;
}

TEST_F(TestDesktopNotifier, test_notify_rejects_negative_timeout)
{
    this->start_client({"notify", "timeout_ms=-5", "summary=Summary"});
    const int return_code = this->finish_client();

    EXPECT_EQ(return_code, 1);
    EXPECT_EQ(_client_output, "error: a desktop notification timeout cannot be negative\n");
}

TEST_F(TestDesktopNotifier, test_close_sends_id)
{
    this->own_notifications_name();

    this->start_client({"notify-close", "id=42"});
    auto call = receive_member(_server, "CloseNotification");
    ASSERT_TRUE(call.has_value());
    auto reply = call->method_return();
    ASSERT_TRUE(reply.has_value()) << reply.error().message;
    ASSERT_TRUE(reply->send(_server).has_value());
    const int return_code = this->finish_client();

    EXPECT_EQ(Iter(*call).get<uint32_t>().value_or(0), 42u);
    EXPECT_EQ(return_code, 0) << _client_output;
}

TEST_F(TestDesktopNotifier, test_notify_without_server)
{
    this->start_client({"notify", "summary=Summary"});
    const int return_code = this->finish_client();

    EXPECT_EQ(return_code, 1);
    EXPECT_EQ(_client_output,
              "error: org.freedesktop.DBus.Error.ServiceUnknown: The name org.freedesktop.Notifications was not "
              "provided by any .service files\n");
}

TEST_F(TestDesktopNotifier, test_notify_reconnects_after_failed_call)
{
    this->own_notifications_name();

    this->start_client({"notify", "attempts=2", "summary=Summary"});
    auto failed_call = receive_member(_server, "GetCapabilities");
    ASSERT_TRUE(failed_call.has_value());
    auto error = failed_call->error_return("org.sihd.Error.Down", "server down");
    ASSERT_TRUE(error.has_value()) << error.error().message;
    ASSERT_TRUE(error->send(_server).has_value());
    this->answer_capabilities({"body"});
    auto call = this->answer_notify(9);
    const int return_code = this->finish_client();

    ASSERT_TRUE(call.has_value());
    EXPECT_EQ(return_code, 0) << _client_error;
    EXPECT_EQ(_client_output, "error: org.sihd.Error.Down: server down\n9\n");
}

} // namespace test
