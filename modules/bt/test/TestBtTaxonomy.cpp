#include <cstdint>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <sihd/bt/taxonomy.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::bt;
using namespace sihd::bt::taxonomy;
using namespace sihd::util;

class TestBtTaxonomy: public ::testing::Test
{
    protected:
        TestBtTaxonomy() { LoggerManager::stream(); }

        virtual ~TestBtTaxonomy() { LoggerManager::clear_loggers(); }

        static std::string base_uuid(std::string_view short_uuid)
        {
            return fmt::format("0000{}-0000-1000-8000-00805f9b34fb", short_uuid);
        }

        static ScanResult manufacturer_result(uint16_t company, std::vector<uint8_t> data)
        {
            ScanResult result;
            result.manufacturer_data[company] = std::move(data);
            return result;
        }

        static ScanResult service_data_result(std::string_view short_uuid, std::vector<uint8_t> data)
        {
            ScanResult result;
            result.service_data[base_uuid(short_uuid)] = std::move(data);
            return result;
        }
};

TEST_F(TestBtTaxonomy, test_bttaxonomy_appearance)
{
    // category 0x00f human interface device, sub-category 0x01 keyboard
    static_assert(kind_from_appearance(0x03c1) == Kind::keyboard);

    // sub-categories 0x02 mouse, 0x04 gamepad, 0x0a presentation remote
    EXPECT_EQ(kind_from_appearance(0x03c2), Kind::mouse);
    EXPECT_EQ(kind_from_appearance(0x03c4), Kind::gamepad);
    EXPECT_EQ(kind_from_appearance(0x03ca), Kind::hid);

    // categories 0x001 phone, 0x003 watch, 0x025 wearable audio device with sub-category 0x01 earbud
    EXPECT_EQ(kind_from_appearance(0x0040), Kind::phone);
    EXPECT_EQ(kind_from_appearance(0x00c0), Kind::watch);
    EXPECT_EQ(kind_from_appearance(0x0941), Kind::headphones);

    // category 0x000 unknown, 0x02c unassigned
    EXPECT_EQ(kind_from_appearance(0x0000), Kind::unknown);
    EXPECT_EQ(kind_from_appearance(0x0b00), Kind::unknown);
}

TEST_F(TestBtTaxonomy, test_bttaxonomy_class_of_device)
{
    // service bits 0x5a, major 0x02 phone, minor 3 smartphone
    static_assert(kind_from_class(0x5a020c) == Kind::phone);

    // major 0x05 peripheral: minor bits 7-6 keyboard, pointing device, then bits 5-2 gamepad, remote control
    EXPECT_EQ(kind_from_class(0x000540), Kind::keyboard);
    EXPECT_EQ(kind_from_class(0x000580), Kind::mouse);
    EXPECT_EQ(kind_from_class(0x000508), Kind::gamepad);
    EXPECT_EQ(kind_from_class(0x00050c), Kind::hid);

    // major 0x04 audio/video minor 6 headphones, major 0x07 wearable minor 1 wristwatch
    EXPECT_EQ(kind_from_class(0x240418), Kind::headphones);
    EXPECT_EQ(kind_from_class(0x000704), Kind::watch);

    // major 0x06 imaging: bit 7 printer, bit 4 display
    EXPECT_EQ(kind_from_class(0x000680), Kind::imaging);
    EXPECT_EQ(kind_from_class(0x000610), Kind::display);

    // major 0x1f uncategorized
    EXPECT_EQ(kind_from_class(0x001f00), Kind::unknown);
}

TEST_F(TestBtTaxonomy, test_bttaxonomy_uuid)
{
    // heart rate, human interface device
    EXPECT_EQ(kind_from_uuid(base_uuid("180d")), Kind::health);
    EXPECT_EQ(kind_from_uuid("00001812-0000-1000-8000-00805F9B34FB"), Kind::hid);

    // gap
    EXPECT_EQ(kind_from_uuid(base_uuid("1800")), Kind::unknown);
    // a vendor 128 bits uuid, a short form
    EXPECT_EQ(kind_from_uuid("6e400001-b5a3-f393-e0a9-e50e24dcca9e"), Kind::unknown);
    EXPECT_EQ(kind_from_uuid("180d"), Kind::unknown);
}

TEST_F(TestBtTaxonomy, test_bttaxonomy_beacon)
{
    // apple 0x004c: proximity beacon type 0x02 length 0x15, offline finding type 0x12 length 0x19
    EXPECT_EQ(beacon(manufacturer_result(0x004c, {0x02, 0x15, 0xe2, 0xc5})), Beacon::ibeacon);
    EXPECT_EQ(beacon(manufacturer_result(0x004c, {0x12, 0x19, 0x10})), Beacon::find_my);
    // microsoft 0x0006: beacon id 0x03, scenario 0x00, reserved rssi 0x80
    EXPECT_EQ(beacon(manufacturer_result(0x0006, {0x03, 0x00, 0x80})), Beacon::swift_pair);

    // eddystone url frame 0x10, fast pair model id, exposure notification rolling identifier
    EXPECT_EQ(beacon(service_data_result("feaa", {0x10})), Beacon::eddystone);
    EXPECT_EQ(beacon(service_data_result("fe2c", {0x00, 0x00, 0x01})), Beacon::fast_pair);
    EXPECT_EQ(beacon(service_data_result("fd6f", {0x01})), Beacon::exposure_notification);

    // apple nearby info type 0x10, an empty microsoft payload
    EXPECT_EQ(beacon(manufacturer_result(0x004c, {0x10, 0x05})), std::nullopt);
    EXPECT_EQ(beacon(manufacturer_result(0x0006, {})), std::nullopt);
}

TEST_F(TestBtTaxonomy, test_bttaxonomy_kind_order)
{
    ScanResult result = manufacturer_result(0x004c, {0x12, 0x19});
    EXPECT_EQ(kind(result), Kind::tag);

    result.service_data[base_uuid("180d")] = {};
    EXPECT_EQ(kind(result), Kind::health);

    result.uuids = {base_uuid("1812")};
    EXPECT_EQ(kind(result), Kind::hid);

    // major 0x04 audio/video minor 6 headphones
    result.class_of_device = 0x240418;
    EXPECT_EQ(kind(result), Kind::headphones);

    // an unknown appearance lets the class of device decide
    result.appearance = 0x0000;
    EXPECT_EQ(kind(result), Kind::headphones);

    // human interface device keyboard
    result.appearance = 0x03c1;
    EXPECT_EQ(kind(result), Kind::keyboard);

    EXPECT_EQ(kind(ScanResult {}), Kind::unknown);
}

TEST_F(TestBtTaxonomy, test_bttaxonomy_str)
{
    EXPECT_EQ(kind_str(Kind::hearing_aid), "hearing_aid");
    EXPECT_EQ(kind_str(Kind::unknown), "unknown");
    EXPECT_EQ(beacon_str(Beacon::find_my), "find_my");
    EXPECT_EQ(beacon_str(Beacon::exposure_notification), "exposure_notification");
}

} // namespace test
