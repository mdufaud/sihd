#include <gtest/gtest.h>

#include <sihd/usb/UsbContext.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;

class TestUsbContext: public ::testing::Test
{
    protected:
        TestUsbContext() { LoggerManager::stream(); }

        virtual ~TestUsbContext() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}

        static bool usb_available() { return UsbContext().scan().has_value(); }
};

TEST_F(TestUsbContext, test_move)
{
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    UsbContext context;
    UsbContext moved(std::move(context));
    UsbContext assigned;

    assigned = std::move(moved);

    auto moved_from = moved.scan();
    ASSERT_FALSE(moved_from.has_value());
    EXPECT_EQ(moved_from.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(moved_from.error().message, "no usb context");
    EXPECT_TRUE(assigned.scan().has_value());
}

TEST_F(TestUsbContext, test_scan)
{
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    UsbContext context;

    auto devices = context.scan();

    ASSERT_TRUE(devices.has_value()) << devices.error().message;
    for (const UsbDevice & device : *devices)
    {
        EXPECT_GE(device.bus, 1);
        EXPECT_LE(device.port_path.size(), 7u);
    }
}

} // namespace test
