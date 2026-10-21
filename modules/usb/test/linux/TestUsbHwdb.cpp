#include <algorithm>

#include <gtest/gtest.h>

#include <sihd/usb/UsbContext.hpp>
#include <sihd/usb/linux/hwdb.hpp>
#include <sihd/util/Logger.hpp>

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;

class TestUsbHwdb: public ::testing::Test
{
    protected:
        TestUsbHwdb() { LoggerManager::stream(); }

        virtual ~TestUsbHwdb() { LoggerManager::clear_loggers(); }

        virtual void SetUp() {}

        virtual void TearDown() {}
};

TEST_F(TestUsbHwdb, test_enrich)
{
    auto scanned = UsbContext().scan();
    if (!scanned.has_value())
        GTEST_SKIP() << "no usb subsystem: " << scanned.error().message;
    std::vector<UsbDevice> devices = *scanned;

    auto enriched = linux_::hwdb::enrich(devices);
    if (!enriched.has_value())
    {
        ASSERT_EQ(enriched.error().code, ErrorCode::not_found) << enriched.error().message;
        GTEST_SKIP() << enriched.error().message;
    }

    ASSERT_EQ(devices.size(), scanned->size());
    for (size_t i = 0; i < devices.size(); ++i)
    {
        EXPECT_EQ(devices[i].bus, (*scanned)[i].bus);
        EXPECT_EQ(devices[i].address, (*scanned)[i].address);
        EXPECT_EQ(devices[i].vendor_id, (*scanned)[i].vendor_id);
        EXPECT_EQ(devices[i].product_id, (*scanned)[i].product_id);
    }

    auto root_hub = std::ranges::find_if(devices, [](const UsbDevice & device) {
        return device.vendor_id == 0x1d6b && device.product_id == 0x0002;
    });
    if (root_hub == devices.end())
        GTEST_SKIP() << "no linux usb 2.0 root hub";
    EXPECT_EQ(root_hub->vendor_name, "Linux Foundation");
    EXPECT_EQ(root_hub->product_name, "2.0 root hub");
    EXPECT_EQ(root_hub->class_name, "Hub");
}

} // namespace test
