#include <algorithm>
#include <net_test_helpers.hpp>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/net/NetInterface.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/wifi/link.hpp>
#include <sihd/wifi/scan.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;
using sihd::net::NetInterface;

class TestAccessPoints: public ::testing::Test
{
    protected:
        TestAccessPoints() { LoggerManager::stream(); }

        virtual ~TestAccessPoints() { LoggerManager::clear_loggers(); }
};

TEST_F(TestAccessPoints, test_access_points_unknown_interface)
{
    auto unknown = access_points("sihd-no-such-interface");

    ASSERT_FALSE(unknown.has_value());
    if (unknown.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wifi stack: " << unknown.error().message;
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no wifi interface named sihd-no-such-interface");
}

TEST_F(TestAccessPoints, test_access_points_loopback)
{
    const std::optional<NetInterface> loopback = find_interface(NetInterface::Kind::loopback, false);
    ASSERT_TRUE(loopback.has_value());

    auto heard = access_points(loopback->name());

    ASSERT_FALSE(heard.has_value());
    if (heard.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wifi stack: " << heard.error().message;
    EXPECT_EQ(heard.error().code, ErrorCode::not_found);
    EXPECT_EQ(heard.error().message, fmt::format("no wifi interface named {}", loopback->name()));
}

TEST_F(TestAccessPoints, test_access_points_hold_the_associated_one)
{
    if (sihd::sys::os::is_run_by_wine())
        GTEST_SKIP() << "wine wlanapi lists no interface";
    const std::optional<NetInterface> station = find_interface(NetInterface::Kind::wifi, true);
    if (!station.has_value())
        GTEST_SKIP() << "no running wifi interface";
    auto station_link = link(station->name());
    if (!station_link.has_value())
        GTEST_SKIP() << "no association: " << station_link.error().message;

    auto heard = access_points(station->name());

    ASSERT_TRUE(heard.has_value()) << heard.error().message;
    const auto associated = std::ranges::find(*heard, station_link->bssid, &AccessPoint::bssid);
    ASSERT_NE(associated, heard->end());
    EXPECT_TRUE(associated->associated);
    EXPECT_EQ(associated->ssid, station_link->ssid);
    EXPECT_LT(associated->signal_dbm, 0);
    EXPECT_GT(associated->frequency_mhz, 2000u);
    EXPECT_NE(associated->security, Security::unknown);
}

} // namespace test
