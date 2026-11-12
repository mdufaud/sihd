#include <chrono>

#include <gtest/gtest.h>

#include <sihd/net/NetMonitor.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
class TestNetMonitor: public ::testing::Test
{
    protected:
        TestNetMonitor() { LoggerManager::stream(); }

        virtual ~TestNetMonitor() { LoggerManager::clear_loggers(); }
};

TEST_F(TestNetMonitor, test_netmonitor_run_returns_true_when_idle)
{
    NetMonitor monitor(std::chrono::milliseconds(10));
    ASSERT_TRUE(monitor.open());

    EXPECT_TRUE(monitor.run());
}

TEST_F(TestNetMonitor, test_netmonitor_open_once)
{
    NetMonitor monitor;
    monitor.set_run_timeout(std::chrono::milliseconds(10));

    EXPECT_FALSE(monitor.is_open());
    EXPECT_FALSE(monitor.run());

    ASSERT_TRUE(monitor.open());
    auto reopened = monitor.open();

    EXPECT_TRUE(monitor.is_open());
    ASSERT_FALSE(reopened.has_value());
    EXPECT_EQ(reopened.error().code, ErrorCode::already_exists);
    EXPECT_EQ(reopened.error().message, "net monitor already open");
    EXPECT_TRUE(monitor.run());
}

TEST_F(TestNetMonitor, test_netmonitor_event_type_str)
{
    NetMonitorEvent event;

    EXPECT_EQ(event.type_str(), "unknown");
    event.type = NetMonitorEventType::link;
    EXPECT_EQ(event.type_str(), "link");
    event.type = NetMonitorEventType::address;
    EXPECT_EQ(event.type_str(), "address");
    event.type = NetMonitorEventType::route;
    EXPECT_EQ(event.type_str(), "route");
    event.type = NetMonitorEventType::overflow;
    EXPECT_EQ(event.type_str(), "overflow");
}

} // namespace test
