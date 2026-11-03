#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/PowerInhibitor.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

class TestPowerInhibitor: public ::testing::Test
{
    protected:
        TestPowerInhibitor() { LoggerManager::stream(); }

        virtual ~TestPowerInhibitor() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!PowerInhibitor::supported)
            {
                GTEST_SKIP() << "power inhibition not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto opened = _server.open(_bus.address());
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
        }

        virtual void TearDown() { close_fd(_lock_read); }

        void own_name(const std::string & name)
        {
            auto owned = _server.request_name(name);
            ASSERT_TRUE(owned.has_value()) << owned.error().message;
        }

        PrivateDBus _bus;
        Connection _server;
        PrivateDBusClient _client;
        int _lock_read = -1;
};

TEST_F(TestPowerInhibitor, test_sleep_lock_held_until_release)
{
    this->own_name("org.freedesktop.login1");
    ASSERT_TRUE(_client.start(_bus.address(), {"power-inhibit", "what=sleep", "app_name=My App", "reason=Recording"}));

    const std::vector<std::string> args = answer_inhibit_with_pipe(_server, _lock_read);
    const auto held = _client.next_line();
    const bool closed_while_held = closed_within(_lock_read, 50);

    ASSERT_TRUE(_client.proceed());
    const auto released = _client.next_line();
    const bool closed_after_release = closed_within(_lock_read, 5000);

    EXPECT_EQ(args, (std::vector<std::string> {"sleep", "My App", "Recording", "block"}));
    EXPECT_EQ(held, "held");
    EXPECT_FALSE(closed_while_held);
    EXPECT_EQ(released, "released");
    EXPECT_TRUE(closed_after_release);
    EXPECT_EQ(_client.finish(), 0);
}

TEST_F(TestPowerInhibitor, test_sleep_lock_ends_with_the_object)
{
    this->own_name("org.freedesktop.login1");
    ASSERT_TRUE(_client.start(_bus.address(), {"power-inhibit", "what=sleep", "end=destroy"}));

    answer_inhibit_with_pipe(_server, _lock_read);
    ASSERT_EQ(_client.next_line(), "held");
    ASSERT_TRUE(_client.proceed());
    const auto released = _client.next_line();

    EXPECT_EQ(released, "released");
    EXPECT_TRUE(closed_within(_lock_read, 5000));
    EXPECT_EQ(_client.finish(), 0);
}

TEST_F(TestPowerInhibitor, test_sleep_inhibited_twice)
{
    this->own_name("org.freedesktop.login1");
    ASSERT_TRUE(_client.start(_bus.address(), {"power-inhibit", "what=sleep", "twice=1"}));

    answer_inhibit_with_pipe(_server, _lock_read);

    EXPECT_EQ(_client.next_line(), "held");
    EXPECT_EQ(_client.next_line(), error_line(ErrorCode::already_exists, "sleep is already inhibited"));
    EXPECT_FALSE(closed_within(_lock_read, 50));
}

TEST_F(TestPowerInhibitor, test_sleep_refused_leaves_nothing_held)
{
    this->own_name("org.freedesktop.login1");
    ASSERT_TRUE(_client.start(_bus.address(), {"power-inhibit", "what=sleep"}));

    auto call = receive_member(_server, "Inhibit");
    ASSERT_TRUE(call.has_value());
    refuse(_server, *call, "org.freedesktop.DBus.Error.AccessDenied");

    EXPECT_EQ(_client.next_line(),
              error_line(ErrorCode::permission_denied, "org.freedesktop.DBus.Error.AccessDenied: denied"));
    EXPECT_EQ(_client.next_line(), "inhibited=0");
}

TEST_F(TestPowerInhibitor, test_display_held_until_release)
{
    this->own_name("org.freedesktop.portal.Desktop");
    ASSERT_TRUE(_client.start(_bus.address(), {"power-inhibit", "what=display", "reason=Slideshow"}));

    auto inhibit = receive_member(_server, "Inhibit");
    ASSERT_TRUE(inhibit.has_value());
    EXPECT_EQ(inhibit->interface(), "org.freedesktop.portal.Inhibit");
    EXPECT_EQ(inhibit->path(), "/org/freedesktop/portal/desktop");
    EXPECT_EQ(inhibit->signature(), "sua{sv}");
    Iter args(*inhibit);
    EXPECT_EQ(args.get<std::string>().value_or("unreadable"), "");
    args.next();
    EXPECT_EQ(args.get<uint32_t>().value_or(0), 8u);
    args.next();

    std::string reason;
    auto read = args.for_each_entry<std::string>([&reason](const std::string & key, Iter & value) -> Result<void> {
        if (key == "reason")
            reason = value.get<std::string>().value_or("unreadable");
        return {};
    });
    ASSERT_TRUE(read.has_value()) << read.error().message;
    EXPECT_EQ(reason, "Slideshow");

    auto reply = inhibit->method_return();
    ASSERT_TRUE(reply.has_value());
    ASSERT_TRUE(reply->add_object_path(portal_handle).has_value());
    ASSERT_TRUE(reply->send(_server).has_value());
    EXPECT_EQ(_client.next_line(), "held");

    ASSERT_TRUE(_client.proceed());
    auto close = receive_member(_server, "Close");
    ASSERT_TRUE(close.has_value());
    EXPECT_EQ(close->path(), portal_handle);
    EXPECT_EQ(close->interface(), "org.freedesktop.portal.Request");
    auto closed = close->method_return();
    ASSERT_TRUE(closed.has_value());
    ASSERT_TRUE(closed->send(_server).has_value());

    EXPECT_EQ(_client.next_line(), "released");
    EXPECT_EQ(_client.finish(), 0);
}

TEST_F(TestPowerInhibitor, test_release_without_inhibition)
{
    PowerInhibitor inhibitor;

    EXPECT_TRUE(inhibitor.release().has_value());
    EXPECT_FALSE(inhibitor.is_sleep_inhibited());
    EXPECT_FALSE(inhibitor.is_display_inhibited());
}

} // namespace test
