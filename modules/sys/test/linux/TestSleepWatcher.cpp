#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/sys/SleepWatcher.hpp>
#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Logger.hpp>

#include "../test_helper.hpp"
#include "private_dbus.hpp"

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;
using namespace sihd::sys::linux_::dbus;

class TestSleepWatcher: public ::testing::Test
{
    protected:
        TestSleepWatcher() { LoggerManager::stream(); }

        virtual ~TestSleepWatcher() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            if constexpr (!SleepWatcher::supported)
            {
                GTEST_SKIP() << "sleep notification not supported on this platform";
            }
            if (!PrivateDBus::available())
            {
                GTEST_SKIP() << "no dbus-daemon";
            }
            ASSERT_TRUE(_bus.start());
            auto opened = _server.open(_bus.address());
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
            auto owned = _server.request_name("org.freedesktop.login1");
            ASSERT_TRUE(owned.has_value()) << owned.error().message;
        }

        virtual void TearDown()
        {
            close_fd(_first_lock);
            close_fd(_second_lock);
        }

        void send_prepare_for_sleep(bool sleeping)
        {
            Message signal;
            auto built = signal.signal("/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep");
            ASSERT_TRUE(built.has_value()) << built.error().message;
            ASSERT_TRUE(signal.add(sleeping).has_value());
            ASSERT_TRUE(signal.send(_server).has_value());
        }

        PrivateDBus _bus;
        Connection _server;
        PrivateDBusClient _client;
        int _first_lock = -1;
        int _second_lock = -1;
};

TEST_F(TestSleepWatcher, test_sleep_waits_for_the_callback_then_resumes)
{
    ASSERT_TRUE(_client.start(_bus.address(), {"sleep-watch", "app_name=My App"}));

    const std::vector<std::string> args = answer_inhibit_with_pipe(_server, _first_lock);
    ASSERT_EQ(_client.next_line(), "started");
    EXPECT_EQ(args, (std::vector<std::string> {"sleep", "My App", "reacts before the system sleeps", "delay"}));

    this->send_prepare_for_sleep(true);
    EXPECT_EQ(_client.next_line(), "suspending");
    EXPECT_FALSE(closed_within(_first_lock, 50));

    ASSERT_TRUE(_client.proceed());
    EXPECT_TRUE(closed_within(_first_lock, 5000));

    this->send_prepare_for_sleep(false);
    const std::vector<std::string> args_again = answer_inhibit_with_pipe(_server, _second_lock);
    EXPECT_EQ(args_again, args);
    EXPECT_EQ(_client.next_line(), "resumed");

    EXPECT_EQ(_client.next_line(), "stopped");
    EXPECT_TRUE(closed_within(_second_lock, 5000));
    EXPECT_EQ(_client.finish(), 0);
}

TEST_F(TestSleepWatcher, test_started_twice)
{
    ASSERT_TRUE(_client.start(_bus.address(), {"sleep-watch", "twice=1"}));

    answer_inhibit_with_pipe(_server, _first_lock);

    EXPECT_EQ(_client.next_line(), "started");
    EXPECT_EQ(_client.next_line(), error_line(ErrorCode::already_exists, "sleep watcher is already running"));
}

TEST_F(TestSleepWatcher, test_delay_lock_refused)
{
    ASSERT_TRUE(_client.start(_bus.address(), {"sleep-watch"}));

    auto call = receive_member(_server, "Inhibit");
    ASSERT_TRUE(call.has_value());
    refuse(_server, *call, "org.freedesktop.DBus.Error.AccessDenied");

    EXPECT_EQ(_client.next_line(),
              error_line(ErrorCode::permission_denied, "org.freedesktop.DBus.Error.AccessDenied: denied"));
    EXPECT_EQ(_client.next_line(), "running=0");
}

TEST(TestSleepWatcherNoBus, test_empty_callback_and_idle_stop)
{
    SleepWatcher watcher;

    auto started = watcher.start({});

    ASSERT_FALSE(started.has_value());
    EXPECT_EQ(started.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(started.error().message, "a sleep watcher needs a callback");
    EXPECT_FALSE(watcher.is_running());
    EXPECT_TRUE(watcher.stop().has_value());
}

} // namespace test
