#include <gtest/gtest.h>

#include <sihd/sys/DesktopNotifier.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::sys;

class TestDesktopNotifier: public ::testing::Test
{
    protected:
        TestDesktopNotifier() { LoggerManager::stream(); }

        virtual ~TestDesktopNotifier() { LoggerManager::clear_loggers(); }
};

TEST_F(TestDesktopNotifier, test_app_id_required)
{
    DesktopNotifier notifier;
    notifier.set_app_name("My App");

    DesktopNotifier::Notification notification;
    notification.summary = "Summary";

    auto notified = notifier.notify(notification);
    auto closed = notifier.close(1);

    ASSERT_FALSE(notified.has_value());
    EXPECT_EQ(notified.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(notified.error().message, "a windows desktop notification needs an app id");
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().message, "a windows desktop notification needs an app id");
}

TEST_F(TestDesktopNotifier, test_negative_timeout)
{
    DesktopNotifier notifier;
    notifier.set_app_id("org.sihd.Test");
    DesktopNotifier::Notification notification;
    notification.summary = "Summary";
    notification.timeout = Duration(time::milli(-5));

    auto notified = notifier.notify(notification);

    ASSERT_FALSE(notified.has_value());
    EXPECT_EQ(notified.error().code, ErrorCode::invalid_argument);
    EXPECT_EQ(notified.error().message, "a desktop notification timeout cannot be negative");
}

} // namespace test
