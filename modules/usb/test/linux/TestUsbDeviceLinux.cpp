#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Logger.hpp>

#include "FakeUsbDevice.hpp"

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;

class TestUsbDeviceLinux: public ::testing::Test
{
    protected:
        TestUsbDeviceLinux() { LoggerManager::stream(); }

        virtual ~TestUsbDeviceLinux() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _fake = FakeUsbDevice::create();
            if (_fake == nullptr)
                GTEST_SKIP() << "no fake usb device: runs under umockdev-wrapper";
        }

        virtual void TearDown() {}

        // the scan order is unspecified: the keyboard sits at address 2, its read-only twin at 3
        static const UsbDevice & keyboard_of(const std::vector<UsbDevice> & devices)
        {
            return devices.front().address == 2 ? devices.front() : devices.back();
        }

        std::unique_ptr<FakeUsbDevice> _fake;
};

TEST_F(TestUsbDeviceLinux, test_scan_keyboard)
{
    auto devices = scan();

    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    ASSERT_EQ(devices->size(), 2u);
    const UsbDevice & keyboard = keyboard_of(*devices);
    EXPECT_EQ(keyboard.bus, 1);
    EXPECT_EQ(keyboard.address, 2);
    EXPECT_EQ(keyboard.port_path, std::vector<uint8_t>({1}));
    EXPECT_EQ(keyboard.speed, UsbSpeed::full);
    EXPECT_EQ(keyboard.usb_version_bcd, 0x0200);
    EXPECT_EQ(keyboard.vendor_id, FakeUsbDevice::vendor_id);
    EXPECT_EQ(keyboard.product_id, FakeUsbDevice::product_id);
    EXPECT_EQ(keyboard.release_bcd, 0x0100);
    EXPECT_EQ(keyboard.device_class, 0);
    EXPECT_EQ(keyboard.max_packet_size0, 64);
    EXPECT_EQ(keyboard.configuration_count, 1);
}

TEST_F(TestUsbDeviceLinux, test_scan_keyboard_configuration)
{
    auto devices = scan();

    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    ASSERT_EQ(devices->size(), 2u);
    const UsbDevice & keyboard = keyboard_of(*devices);
    ASSERT_EQ(keyboard.configurations.size(), 1u);
    const UsbConfiguration & configuration = keyboard.configurations.front();
    EXPECT_EQ(configuration.value, 1);
    // bus powered with remote wakeup
    EXPECT_EQ(configuration.attributes, 0xa0);
    // bMaxPower 50 in 2 mA units below SuperSpeed
    EXPECT_EQ(configuration.max_power_ma, 100);

    ASSERT_EQ(configuration.interfaces.size(), 1u);
    ASSERT_EQ(configuration.interfaces.front().alt_settings.size(), 1u);
    const UsbAltSetting & setting = configuration.interfaces.front().alt_settings.front();
    // hid class, boot subclass, keyboard protocol
    EXPECT_EQ(setting.interface_class, 3);
    EXPECT_EQ(setting.interface_subclass, 1);
    EXPECT_EQ(setting.interface_protocol, 1);

    ASSERT_EQ(setting.endpoints.size(), 1u);
    const UsbEndpoint & endpoint = setting.endpoints.front();
    EXPECT_EQ(endpoint.address, FakeUsbDevice::interrupt_endpoint);
    EXPECT_EQ(endpoint.direction, UsbDirection::in);
    EXPECT_EQ(endpoint.transfer_type, UsbTransferType::interrupt);
    EXPECT_EQ(endpoint.max_packet_size, 8);
    EXPECT_EQ(endpoint.interval, 10);
}

} // namespace test
