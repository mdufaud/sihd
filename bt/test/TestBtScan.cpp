#include <gtest/gtest.h>
#include <iostream>
#include <sihd/bt/BtUtils.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/term.hpp>

namespace test
{
SIHD_NEW_LOGGER("test");
using namespace sihd::bt;
using namespace sihd::util;
class TestBtScan: public ::testing::Test
{
    protected:
        TestBtScan() { sihd::util::LoggerManager::stream(); }

        virtual ~TestBtScan() { sihd::util::LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestBtScan, test_btscan_bluetooth_enabled)
{
    try
    {
        bool enabled = BtUtils::bluetooth_enabled();
        SIHD_LOG(info, "Bluetooth enabled: {}", enabled);
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(warning, "Bluetooth check failed (no dbus/bluetooth stack?): {}", e.what());
        GTEST_SKIP();
    }
}

TEST_F(TestBtScan, test_btscan_scan)
{
    bool enabled = false;
    try
    {
        enabled = BtUtils::bluetooth_enabled();
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(warning, "Bluetooth check failed (no dbus/bluetooth stack?): {}", e.what());
        GTEST_SKIP();
    }
    if (!enabled)
    {
        SIHD_LOG(warning, "Bluetooth not enabled, skipping scan test");
        GTEST_SKIP();
    }
    auto devices = BtUtils::scan(3000);
    for (const auto & dev : devices)
    {
        SIHD_LOG(info,
                 "Device: {} [{}] {} dBm connectable={}",
                 dev.identifier,
                 dev.address,
                 dev.rssi,
                 dev.connectable);
    }
}
} // namespace test