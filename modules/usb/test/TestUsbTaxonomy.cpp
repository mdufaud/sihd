#include <array>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include <sihd/usb/taxonomy.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::usb;
using namespace sihd::usb::taxonomy;
using namespace sihd::util;

class TestUsbTaxonomy: public ::testing::Test
{
    protected:
        TestUsbTaxonomy() { LoggerManager::stream(); }

        virtual ~TestUsbTaxonomy() { LoggerManager::clear_loggers(); }

        // one configuration, one interface per class, subclass and protocol
        static UsbDevice device_with(uint8_t device_class, const std::vector<std::array<uint8_t, 3>> & interfaces)
        {
            UsbDevice device;
            device.device_class = device_class;
            UsbConfiguration configuration;
            for (const auto & [class_code, subclass, protocol] : interfaces)
            {
                UsbAltSetting setting;
                setting.interface_class = class_code;
                setting.interface_subclass = subclass;
                setting.interface_protocol = protocol;
                configuration.interfaces.push_back(UsbInterface {.number = 0, .alt_settings = {setting}});
            }
            device.configurations.push_back(configuration);
            return device;
        }
};

TEST_F(TestUsbTaxonomy, test_usbtaxonomy_kind_from_class)
{
    // hid boot interface: protocol 0x01 keyboard, 0x02 mouse; a protocol outside the boot subclass means nothing
    static_assert(kind_from_class(0x03, 0x01, 0x01) == Kind::keyboard);
    EXPECT_EQ(kind_from_class(0x03, 0x01, 0x02), Kind::mouse);
    EXPECT_EQ(kind_from_class(0x03, 0x00, 0x01), Kind::hid);

    // mass storage scsi bulk only, hub
    EXPECT_EQ(kind_from_class(0x08, 0x06, 0x50), Kind::storage);
    EXPECT_EQ(kind_from_class(0x09, 0x00, 0x00), Kind::hub);

    // cdc acm with at commands, acm with the vendor protocol of rndis, ethernet control model, cdc data
    EXPECT_EQ(kind_from_class(0x02, 0x02, 0x01), Kind::serial);
    EXPECT_EQ(kind_from_class(0x02, 0x02, 0xff), Kind::network);
    EXPECT_EQ(kind_from_class(0x02, 0x06, 0x00), Kind::network);
    EXPECT_EQ(kind_from_class(0x0a, 0x00, 0x00), Kind::unknown);

    // bluetooth programming interface, device firmware upgrade, vendor specific
    EXPECT_EQ(kind_from_class(0xe0, 0x01, 0x01), Kind::bluetooth);
    EXPECT_EQ(kind_from_class(0xfe, 0x01, 0x02), Kind::firmware_update);
    EXPECT_EQ(kind_from_class(0xff, 0x42, 0x01), Kind::vendor_specific);

    // unassigned base class
    EXPECT_EQ(kind_from_class(0x04, 0x00, 0x00), Kind::unknown);
}

TEST_F(TestUsbTaxonomy, test_usbtaxonomy_kind_of_device)
{
    // the hub device class decides
    EXPECT_EQ(kind(device_with(0x09, {{0x09, 0x00, 0x00}})), Kind::hub);

    // per interface 0x00: vendor specific then hid boot mouse
    EXPECT_EQ(kind(device_with(0x00, {{0xff, 0x00, 0x00}, {0x03, 0x01, 0x02}})), Kind::mouse);

    // miscellaneous 0xef interface association: cdc data, video, audio
    EXPECT_EQ(kind(device_with(0xef, {{0x0a, 0x00, 0x00}, {0x0e, 0x01, 0x00}, {0x01, 0x01, 0x00}})), Kind::video);

    EXPECT_EQ(kind(UsbDevice {}), Kind::unknown);
}

TEST_F(TestUsbTaxonomy, test_usbtaxonomy_str)
{
    EXPECT_EQ(kind_str(Kind::firmware_update), "firmware_update");
    EXPECT_EQ(kind_str(Kind::unknown), "unknown");
}

} // namespace test
