#include <gtest/gtest.h>

#include <sihd/bt/Device.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::bt;
using namespace sihd::util;

class TestBtDevice: public ::testing::Test
{
    protected:
        TestBtDevice(): _device("device") { LoggerManager::stream(); }

        virtual ~TestBtDevice() { LoggerManager::clear_loggers(); }

        virtual void SetUp() { ASSERT_TRUE(_device.set_address("00:11:22:33:44:55")); }

        Device _device;
};

TEST_F(TestBtDevice, test_trusted_not_supported)
{
    auto trusted = _device.set_trusted(true);

    ASSERT_FALSE(trusted.has_value());
    EXPECT_EQ(trusted.error().code, ErrorCode::not_supported);
}

TEST_F(TestBtDevice, test_other_adapter_not_supported)
{
    ASSERT_TRUE(_device.set_adapter(1));

    auto info = _device.info();
    auto paired = _device.pair();
    auto unpaired = _device.unpair();

    ASSERT_FALSE(info.has_value());
    EXPECT_EQ(info.error().code, ErrorCode::not_supported);
    ASSERT_FALSE(paired.has_value());
    EXPECT_EQ(paired.error().code, ErrorCode::not_supported);
    ASSERT_FALSE(unpaired.has_value());
    EXPECT_EQ(unpaired.error().code, ErrorCode::not_supported);
    EXPECT_FALSE(_device.start());
}

TEST_F(TestBtDevice, test_cancel_without_pairing)
{
    auto canceled = _device.cancel_pairing();

    ASSERT_FALSE(canceled.has_value());
    EXPECT_EQ(canceled.error().code, ErrorCode::not_found);
}

TEST_F(TestBtDevice, test_gatt_needs_connection)
{
    const GattCharacteristic characteristic {.uuid = "00002a00-0000-1000-8000-00805f9b34fb",
                                             .handle = 3,
                                             .properties = GattCharacteristic::read,
                                             .descriptors = {}};

    auto services = _device.services();
    auto read = _device.read_characteristic(characteristic);
    auto subscribed = _device.subscribe(characteristic);
    auto mtu = _device.mtu();

    ASSERT_FALSE(services.has_value());
    EXPECT_EQ(services.error().code, ErrorCode::closed);
    ASSERT_FALSE(read.has_value());
    EXPECT_EQ(read.error().code, ErrorCode::closed);
    ASSERT_FALSE(subscribed.has_value());
    EXPECT_EQ(subscribed.error().code, ErrorCode::closed);
    ASSERT_FALSE(mtu.has_value());
    EXPECT_EQ(mtu.error().code, ErrorCode::closed);
}

} // namespace test
