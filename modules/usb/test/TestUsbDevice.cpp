#include <gtest/gtest.h>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Logger.hpp>

#include "usb_available.hpp"

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;

class TestUsbDevice: public ::testing::Test
{
    protected:
        TestUsbDevice() { LoggerManager::stream(); }

        virtual ~TestUsbDevice() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        // bit 7 of an endpoint address is the direction, set for in
        static constexpr uint8_t endpoint_in_bit = 0x80;
        // usb class code of a hub
        static constexpr uint8_t hub_class = 9;
        // usb 2 and 3 cap an endpoint packet at 1024 bytes
        static constexpr uint16_t packet_size_max = 1024;
};

TEST_F(TestUsbDevice, test_scan)
{
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    auto devices = scan();

    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    for (const UsbDevice & device : *devices)
    {
        EXPECT_GE(device.bus, 1);
        EXPECT_LE(device.port_path.size(), 7u);
    }
}

TEST_F(TestUsbDevice, test_scan_configurations)
{
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    auto devices = scan();

    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    for (const UsbDevice & device : *devices)
    {
        EXPECT_EQ(device.configurations.size(), device.configuration_count);
        for (const UsbConfiguration & configuration : device.configurations)
        {
            EXPECT_GE(configuration.value, 1);
            for (const UsbInterface & usb_interface : configuration.interfaces)
            {
                ASSERT_FALSE(usb_interface.alt_settings.empty());
                for (const UsbAltSetting & setting : usb_interface.alt_settings)
                {
                    for (const UsbEndpoint & endpoint : setting.endpoints)
                    {
                        const bool in_bit = (endpoint.address & endpoint_in_bit) != 0;
                        EXPECT_EQ(endpoint.direction == UsbDirection::in, in_bit);
                        EXPECT_LE(endpoint.max_packet_size, packet_size_max);
                    }
                }
            }
        }
    }
}

TEST_F(TestUsbDevice, test_scan_hub_status_endpoint)
{
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    auto devices = scan();

    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    size_t hub_count = 0;
    for (const UsbDevice & device : *devices)
    {
        if (device.device_class != hub_class || device.configurations.empty())
            continue;
        ++hub_count;
        const UsbConfiguration & configuration = device.configurations.front();
        ASSERT_EQ(configuration.interfaces.size(), 1u);
        ASSERT_FALSE(configuration.interfaces.front().alt_settings.empty());
        const UsbAltSetting & setting = configuration.interfaces.front().alt_settings.front();
        EXPECT_EQ(setting.interface_class, hub_class);
        ASSERT_EQ(setting.endpoints.size(), 1u);
        EXPECT_EQ(setting.endpoints.front().direction, UsbDirection::in);
        EXPECT_EQ(setting.endpoints.front().transfer_type, UsbTransferType::interrupt);
    }
    if (hub_count == 0)
        GTEST_SKIP() << "no usb hub";
}

} // namespace test
