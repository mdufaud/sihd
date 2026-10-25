#include <net_test_helpers.hpp>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/net/NetInterface.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/wifi/link.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;
using sihd::net::NetInterface;

class TestLink: public ::testing::Test
{
    protected:
        TestLink() { LoggerManager::stream(); }

        virtual ~TestLink() { LoggerManager::clear_loggers(); }
};

TEST_F(TestLink, test_link_unknown_interface)
{
    auto unknown = link("sihd-no-such-interface");

    ASSERT_FALSE(unknown.has_value());
    if (unknown.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wifi stack: " << unknown.error().message;
    EXPECT_EQ(unknown.error().code, ErrorCode::not_found);
    EXPECT_EQ(unknown.error().message, "no wifi interface named sihd-no-such-interface");
}

TEST_F(TestLink, test_link_loopback_is_no_wifi_interface)
{
    const std::optional<NetInterface> loopback = find_interface(NetInterface::Kind::loopback, false);
    ASSERT_TRUE(loopback.has_value());

    auto loopback_link = link(loopback->name());

    ASSERT_FALSE(loopback_link.has_value());
    if (loopback_link.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wifi stack: " << loopback_link.error().message;
    EXPECT_EQ(loopback_link.error().code, ErrorCode::not_found);
    EXPECT_EQ(loopback_link.error().message, fmt::format("no wifi interface named {}", loopback->name()));
}

TEST_F(TestLink, test_link_associated_station)
{
    if (sihd::sys::os::is_run_by_wine())
        GTEST_SKIP() << "wine wlanapi lists no interface";
    const std::optional<NetInterface> station = find_interface(NetInterface::Kind::wifi, true);
    if (!station.has_value())
        GTEST_SKIP() << "no running wifi interface";

    auto station_link = link(station->name());
    if (!station_link
        && (station_link.error().code == ErrorCode::not_supported
            || station_link.error().code == ErrorCode::permission_denied))
        GTEST_SKIP() << "wifi details unavailable: " << station_link.error().message;

    ASSERT_TRUE(station_link.has_value()) << station_link.error().message;
    EXPECT_FALSE(station_link->ssid.empty());
    EXPECT_EQ(station_link->bssid.size(), 17u);
    EXPECT_LT(station_link->signal_dbm, 0);
    EXPECT_GT(station_link->signal_dbm, -120);
    EXPECT_LT(station_link->tx_bitrate_bps, 100'000'000'000u);
    // windows never reports a frequency when the bss list lacks the access point
    if (station_link->frequency_mhz != 0)
    {
        EXPECT_GT(station_link->frequency_mhz, 2000u);
        EXPECT_LT(station_link->frequency_mhz, 8000u);
    }
}

} // namespace test
