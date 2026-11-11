#include <gtest/gtest.h>

#include <sihd/usb/UsbHandle.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/build.hpp>

#include "usb_available.hpp"

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;

class TestUsbHandle: public ::testing::Test
{
    protected:
        TestUsbHandle() { LoggerManager::stream(); }

        virtual ~TestUsbHandle() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUsbHandle, test_closed)
{
    UsbHandle handle;

    auto device = handle.device();

    EXPECT_FALSE(handle.is_open());
    ASSERT_FALSE(device.has_value());
    EXPECT_EQ(device.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(device.error().message, "no usb handle");
}

TEST_F(TestUsbHandle, test_open_absent)
{
    if (!usb_available())
        GTEST_SKIP() << "no usb subsystem";
    UsbDevice absent;
    absent.bus = 255;
    absent.address = 1;
    absent.vendor_id = 0x1234;
    absent.product_id = 0x5678;
    UsbHandle handle;

    auto opened = handle.open(absent);

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::not_found);
    EXPECT_EQ(opened.error().message, "cannot open usb device: no device 1234:5678 at 255:001");
    EXPECT_FALSE(handle.is_open());
}

TEST_F(TestUsbHandle, test_wrap_unsupported)
{
    if constexpr (!build::is_windows)
        GTEST_SKIP() << "file descriptors wrap on linux";
    UsbHandle handle;

    auto wrapped = handle.wrap(0);

    ASSERT_FALSE(wrapped.has_value());
    EXPECT_EQ(wrapped.error().code, ErrorCode::not_supported);
    EXPECT_EQ(wrapped.error().message,
              "cannot wrap usb device: Operation not supported or unimplemented on this platform");
    EXPECT_FALSE(handle.is_open());
}

} // namespace test
