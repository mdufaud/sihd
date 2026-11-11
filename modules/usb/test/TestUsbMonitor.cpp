#include <algorithm>
#include <chrono>
#include <stdexcept>

#include <gtest/gtest.h>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/usb/UsbMonitor.hpp>
#include <sihd/util/Handler.hpp>
#include <sihd/util/Logger.hpp>

#include "usb_available.hpp"

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;

class TestUsbMonitor: public ::testing::Test
{
    protected:
        TestUsbMonitor() { LoggerManager::stream(); }

        virtual ~TestUsbMonitor() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUsbMonitor, test_unsupported)
{
    if constexpr (UsbMonitor::supported)
        GTEST_SKIP() << "usb hotplug is supported";

    EXPECT_THROW(UsbMonitor monitor, std::runtime_error);
}

TEST_F(TestUsbMonitor, test_present_devices_arrive)
{
    if constexpr (!UsbMonitor::supported)
        GTEST_SKIP() << "no usb hotplug";
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    UsbMonitor monitor(std::chrono::seconds(10));
    size_t notified = 0;
    Handler<UsbMonitor *> observer([&notified](UsbMonitor *) { ++notified; });
    monitor.add_observer(&observer);
    auto devices = scan();
    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    if (devices->empty())
        GTEST_SKIP() << "no usb device";

    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(monitor.run());
    const auto elapsed = std::chrono::steady_clock::now() - started;

    // the devices already plugged are queued at construction: the first run does not wait its timeout
    EXPECT_LT(elapsed, std::chrono::seconds(5));
    EXPECT_EQ(notified, 1u);
    ASSERT_EQ(monitor.events().size(), devices->size());
    for (const UsbMonitorEvent & event : monitor.events())
    {
        EXPECT_EQ(event.type, UsbMonitorEventType::arrived);
        const bool listed = std::ranges::any_of(*devices, [&event](const UsbDevice & device) {
            return device.bus == event.device.bus && device.address == event.device.address;
        });
        EXPECT_TRUE(listed) << "device " << int(event.device.bus) << ":" << int(event.device.address);
    }
}

TEST_F(TestUsbMonitor, test_quiet_run)
{
    if constexpr (!UsbMonitor::supported)
        GTEST_SKIP() << "no usb hotplug";
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    UsbMonitor monitor(std::chrono::milliseconds(1));
    ASSERT_TRUE(monitor.run());
    size_t notified = 0;
    Handler<UsbMonitor *> observer([&notified](UsbMonitor *) { ++notified; });
    monitor.add_observer(&observer);

    ASSERT_TRUE(monitor.run());

    EXPECT_EQ(notified, 0u);
    EXPECT_TRUE(monitor.events().empty());
}

} // namespace test
