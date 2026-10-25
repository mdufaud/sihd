#include <gtest/gtest.h>

#include <sihd/util/Logger.hpp>
#include <sihd/util/time.hpp>
#include <sihd/wifi/Monitor.hpp>
#include <sihd/wifi/connection.hpp>
#include <sihd/wifi/scan.hpp>

namespace test
{
using namespace sihd::util;
namespace wifi = sihd::wifi;

class TestWlanService: public ::testing::Test
{
    protected:
        TestWlanService() { LoggerManager::stream(); }

        virtual ~TestWlanService() { LoggerManager::clear_loggers(); }

        static constexpr char unknown[] = "sihd-no-such-interface";
};

TEST_F(TestWlanService, test_unknown_interface_queries)
{
    auto selected = wifi::backend(unknown);
    auto current = wifi::state(unknown);
    auto known = wifi::known_networks(unknown);

    ASSERT_FALSE(selected.has_value());
    if (selected.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wlan service: " << selected.error().message;
    EXPECT_EQ(selected.error().message, "no wifi interface named sihd-no-such-interface");
    ASSERT_FALSE(current.has_value());
    EXPECT_EQ(current.error().code, ErrorCode::not_found);
    ASSERT_FALSE(known.has_value());
    EXPECT_EQ(known.error().code, ErrorCode::not_found);
}

TEST_F(TestWlanService, test_monitor_refuses_an_unknown_interface)
{
    wifi::Monitor monitor;

    auto opened = monitor.open(unknown);

    ASSERT_FALSE(opened.has_value());
    if (opened.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wlan service: " << opened.error().message;
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
    EXPECT_EQ(opened.error().message, "no wifi interface named sihd-no-such-interface");
    EXPECT_FALSE(monitor.run());
}

TEST_F(TestWlanService, test_unknown_interface_actions)
{
    auto scanned = wifi::scan(unknown, Duration(time::sec(1)));
    auto connected = wifi::connect(unknown, "home", "a long passphrase", Duration(time::sec(1)));
    auto disconnected = wifi::disconnect(unknown);
    auto forgotten = wifi::forget(unknown, "home");

    ASSERT_FALSE(scanned.has_value());
    if (scanned.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wlan service: " << scanned.error().message;
    EXPECT_EQ(scanned.error().code, ErrorCode::not_found);
    EXPECT_EQ(scanned.error().message, "no wifi interface named sihd-no-such-interface");
    ASSERT_FALSE(connected.has_value());
    EXPECT_EQ(connected.error().code, ErrorCode::not_found);
    EXPECT_EQ(connected.error().message, "no wifi interface named sihd-no-such-interface");
    ASSERT_FALSE(disconnected.has_value());
    EXPECT_EQ(disconnected.error().code, ErrorCode::not_found);
    EXPECT_EQ(disconnected.error().message, "no wifi interface named sihd-no-such-interface");
    ASSERT_FALSE(forgotten.has_value());
    EXPECT_EQ(forgotten.error().code, ErrorCode::not_found);
    EXPECT_EQ(forgotten.error().message, "no wifi interface named sihd-no-such-interface");
}

} // namespace test
