#include <algorithm>
#include <optional>

#include <gtest/gtest.h>

#include <sihd/net/NetInterface.hpp>
#include <sihd/net/wifi.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>

#include "net_test_helpers.hpp"

namespace test
{
SIHD_LOGGER;
using namespace sihd::net;
using namespace sihd::util;
using namespace sihd::sys;
class TestWifi: public ::testing::Test
{
    protected:
        TestWifi() { LoggerManager::stream(); }

        virtual ~TestWifi() { LoggerManager::clear_loggers(); }
};

TEST_F(TestWifi, test_wifi_unknown_interface)
{
    auto link = wifi::link("sihd-no-such-interface");

    ASSERT_FALSE(link.has_value());
    EXPECT_EQ(link.error().code, ErrorCode::not_found);
}

TEST_F(TestWifi, test_wifi_loopback_is_no_station)
{
    const std::optional<NetInterface> loopback = find_loopback();
    ASSERT_TRUE(loopback.has_value());

    auto link = wifi::link(loopback->name());

    ASSERT_FALSE(link.has_value());
    if (link.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wifi stack: " << link.error().message;
    EXPECT_EQ(link.error().code, ErrorCode::not_found);
}

TEST_F(TestWifi, test_wifi_associated_station)
{
    if (os::is_run_by_wine())
        GTEST_SKIP() << "wine wlanapi lists no interface";

    auto interfaces = NetInterface::get_all_interfaces();
    ASSERT_TRUE(interfaces.has_value());
    auto station = std::ranges::find_if(*interfaces, [](const auto & entry) {
        return entry.second.kind() == NetInterface::Kind::wifi && entry.second.running();
    });
    if (station == interfaces->end())
        GTEST_SKIP() << "no running wifi interface";

    auto link = wifi::link(station->first);
    if (!link && (link.error().code == ErrorCode::not_supported || link.error().code == ErrorCode::permission_denied))
        GTEST_SKIP() << "wifi details unavailable: " << link.error().message;

    ASSERT_TRUE(link.has_value()) << link.error().message;
    EXPECT_FALSE(link->ssid.empty());
    EXPECT_EQ(link->bssid.size(), 17u);
    EXPECT_LT(link->signal_dbm, 0);
    EXPECT_GT(link->signal_dbm, -120);
    EXPECT_LT(link->tx_bitrate_bps, 100'000'000'000u);
    // windows reports no frequency when the bss list lacks the access point
    if (link->frequency_mhz != 0)
    {
        EXPECT_GT(link->frequency_mhz, 2000u);
        EXPECT_LT(link->frequency_mhz, 8000u);
    }
}

} // namespace test
