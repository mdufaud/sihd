#include <net_test_helpers.hpp>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/net/NetInterface.hpp>
#include <sihd/sys/os.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/wifi/radio.hpp>

namespace test
{
using namespace sihd::util;
using namespace sihd::wifi;
using sihd::net::NetInterface;

class TestRadio: public ::testing::Test
{
    protected:
        TestRadio() { LoggerManager::stream(); }

        virtual ~TestRadio() { LoggerManager::clear_loggers(); }
};

TEST_F(TestRadio, test_radio_loopback_is_no_wifi_interface)
{
    const std::optional<NetInterface> loopback = find_interface(NetInterface::Kind::loopback, false);
    ASSERT_TRUE(loopback.has_value());

    auto loopback_radio = radio(loopback->name());

    ASSERT_FALSE(loopback_radio.has_value());
    if (loopback_radio.error().code == ErrorCode::not_supported)
        GTEST_SKIP() << "no wifi stack: " << loopback_radio.error().message;
    EXPECT_EQ(loopback_radio.error().code, ErrorCode::not_found);
    EXPECT_EQ(loopback_radio.error().message, fmt::format("no wifi interface named {}", loopback->name()));
}

TEST_F(TestRadio, test_radio_of_a_running_interface)
{
    if (sihd::sys::os::is_run_by_wine())
        GTEST_SKIP() << "wine wlanapi lists no interface";
    const std::optional<NetInterface> wifi = find_interface(NetInterface::Kind::wifi, true);
    if (!wifi.has_value())
        GTEST_SKIP() << "no running wifi interface";

    auto wifi_radio = radio(wifi->name());
    if (!wifi_radio
        && (wifi_radio.error().code == ErrorCode::not_supported
            || wifi_radio.error().code == ErrorCode::permission_denied))
        GTEST_SKIP() << "wifi details unavailable: " << wifi_radio.error().message;

    ASSERT_TRUE(wifi_radio.has_value()) << wifi_radio.error().message;
    EXPECT_NE(wifi_radio->mode, Radio::Mode::unknown);
    EXPECT_TRUE(wifi_radio->frequency_mhz == 0 || wifi_radio->channel != 0);
}

TEST_F(TestRadio, test_radio_mode_str)
{
    EXPECT_EQ(mode_str(Radio::Mode::monitor), "monitor");
    EXPECT_EQ(mode_str(Radio::Mode::access_point), "access point");
    EXPECT_EQ(mode_str(Radio::Mode::unknown), "unknown");
}

} // namespace test
