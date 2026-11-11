#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <memory>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>

#include <sihd/sys/File.hpp>
#include <sihd/sys/fs.hpp>
#include <sihd/usb/UsbHandle.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/endian.hpp>

#include "../usb_available.hpp"
#include "FakeUsbDevice.hpp"

namespace test
{
using namespace sihd::usb;
using namespace sihd::util;
using namespace sihd::sys;

class TestUsbHandleLinux: public ::testing::Test
{
    protected:
        // GET_DESCRIPTOR: standard device-to-host request 6, descriptor type DEVICE (1) in the high byte of value
        static constexpr UsbHandle::ControlSetup get_device_descriptor {
            .request_type = 0x80,
            .request = 6,
            .value = 0x0100,
            .index = 0,
        };
        // device descriptor layout: bLength 18, idVendor at 8, idProduct at 10, string indexes at 14, 15 and 16
        static constexpr size_t device_descriptor_size = 18;
        static constexpr size_t vendor_offset = 8;
        static constexpr size_t product_offset = 10;
        static constexpr size_t manufacturer_index_offset = 14;
        static constexpr size_t product_index_offset = 15;
        static constexpr size_t serial_number_index_offset = 16;
        // bit 7 of a request type or an endpoint address is the direction: 0x00 and 0x01 are out, 0x81 is in
        static constexpr uint8_t out_request_type = 0x00;
        static constexpr uint8_t out_endpoint = 0x01;
        static constexpr uint8_t in_endpoint = 0x81;

        TestUsbHandleLinux() { LoggerManager::stream(); }

        virtual ~TestUsbHandleLinux() { LoggerManager::clear_loggers(); }

        virtual void SetUp()
        {
            _fake = FakeUsbDevice::create();
            if (!usb_available())
                GTEST_SKIP() << "no usb subsystem";
            auto devices = scan();
            ASSERT_TRUE(devices.has_value()) << devices.error().message;
            for (const UsbDevice & device : *devices)
            {
                const bool writable = fs::is_writable(device_path(device));
                // root writes the read-only twin too, and a root hub has no port path for sysfs_path
                const bool candidate = _fake != nullptr ? device.product_id == FakeUsbDevice::product_id
                                                        : !device.port_path.empty();
                if (writable && candidate && !_writable.has_value())
                    _writable = device;
                if (!writable && !_read_only.has_value())
                    _read_only = device;
            }
            if (!_writable.has_value())
                return;
            auto opened = _handle.open(*_writable);
            ASSERT_TRUE(opened.has_value()) << opened.error().message;
        }

        virtual void TearDown() {}

        static std::string device_path(const UsbDevice & device)
        {
            return fmt::format("/dev/bus/usb/{:03}/{:03}", device.bus, device.address);
        }

        static std::string sysfs_path(const UsbDevice & device, std::string_view attribute)
        {
            return fmt::format("/sys/bus/usb/devices/{}-{}/{}",
                               device.bus,
                               fmt::join(device.port_path, "."),
                               attribute);
        }

        static void expect_same_device(const UsbDevice & device, const UsbDevice & expected)
        {
            EXPECT_EQ(device.bus, expected.bus);
            EXPECT_EQ(device.address, expected.address);
            EXPECT_EQ(device.vendor_id, expected.vendor_id);
            EXPECT_EQ(device.product_id, expected.product_id);
            EXPECT_EQ(device.configurations.size(), expected.configurations.size());
        }

        static void expect_string(const Result<std::string> & text, uint8_t index, std::string_view name)
        {
            if (index == 0)
            {
                ASSERT_FALSE(text.has_value());
                EXPECT_EQ(text.error().code, ErrorCode::not_found);
                EXPECT_EQ(text.error().message, fmt::format("usb device has no {} string", name));
                return;
            }
            ASSERT_TRUE(text.has_value()) << text.error().message;
            EXPECT_FALSE(text->empty());
        }

        static void expect_refused(const Result<size_t> & transfer, std::string_view message)
        {
            ASSERT_FALSE(transfer.has_value());
            EXPECT_EQ(transfer.error().code, ErrorCode::invalid_argument);
            EXPECT_EQ(transfer.error().message, message);
        }

        std::array<uint8_t, device_descriptor_size> read_device_descriptor()
        {
            std::array<uint8_t, device_descriptor_size> descriptor {};
            auto size = _handle.control_read(get_device_descriptor,
                                             descriptor.data(),
                                             descriptor.size(),
                                             std::chrono::seconds(1));
            EXPECT_TRUE(size.has_value()) << size.error().message;
            EXPECT_EQ(size.value_or(0), device_descriptor_size);
            return descriptor;
        }

        void claim_keyboard()
        {
            auto claimed = _handle.claim_interface(0);
            ASSERT_TRUE(claimed.has_value()) << claimed.error().message;
        }

        std::unique_ptr<FakeUsbDevice> _fake;
        std::optional<UsbDevice> _writable;
        std::optional<UsbDevice> _read_only;
        UsbHandle _handle;
        std::array<uint8_t, device_descriptor_size> _buffer {};
};

TEST_F(TestUsbHandleLinux, test_open_refused)
{
    if (!_read_only.has_value())
        GTEST_SKIP() << "no read-only usb device";

    UsbHandle handle;

    auto opened = handle.open(*_read_only);

    ASSERT_FALSE(opened.has_value());
    EXPECT_EQ(opened.error().code, ErrorCode::permission_denied);
    EXPECT_EQ(opened.error().message, "cannot open usb device: Access denied (insufficient permissions)");
    EXPECT_FALSE(handle.is_open());
}

TEST_F(TestUsbHandleLinux, test_open_twice)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto again = _handle.open(*_writable);

    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error().code, ErrorCode::already_exists);
    EXPECT_EQ(again.error().message, "the usb handle is already open");
    EXPECT_TRUE(_handle.device().has_value());
}

TEST_F(TestUsbHandleLinux, test_close)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    _handle.close();

    EXPECT_FALSE(_handle.is_open());
    auto closed = _handle.device();
    ASSERT_FALSE(closed.has_value());
    EXPECT_EQ(closed.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(closed.error().message, "no usb handle");

    auto reopened = _handle.open(*_writable);

    ASSERT_TRUE(reopened.has_value()) << reopened.error().message;
    EXPECT_TRUE(_handle.device().has_value());
}

TEST_F(TestUsbHandleLinux, test_close_releases_claimed_interface)
{
    if (_fake == nullptr)
        GTEST_SKIP() << "no fake usb device: runs under umockdev-wrapper";
    this->claim_keyboard();
    ASSERT_TRUE(_fake->is_claimed(0));

    _handle.close();

    EXPECT_FALSE(_fake->is_claimed(0));
}

TEST_F(TestUsbHandleLinux, test_open)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto opened = _handle.device();
    auto configuration = _handle.configuration();

    ASSERT_TRUE(opened.has_value()) << opened.error().message;
    expect_same_device(*opened, *_writable);
    ASSERT_TRUE(configuration.has_value()) << configuration.error().message;
    auto active = fs::read_all(sysfs_path(*_writable, "bConfigurationValue"));
    ASSERT_TRUE(active.has_value()) << active.error().message;
    EXPECT_EQ(*active, fmt::format("{}\n", *configuration));
}

TEST_F(TestUsbHandleLinux, test_max_power_matches_sysfs)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";
    auto configuration = _handle.configuration();
    ASSERT_TRUE(configuration.has_value()) << configuration.error().message;

    auto max_power = fs::read_all(sysfs_path(*_writable, "bMaxPower"));

    ASSERT_TRUE(max_power.has_value()) << max_power.error().message;
    size_t matched = 0;
    for (const UsbConfiguration & candidate : _writable->configurations)
    {
        if (candidate.value != *configuration)
            continue;
        ++matched;
        EXPECT_EQ(*max_power, fmt::format("{}mA\n", candidate.max_power_ma));
    }
    EXPECT_EQ(matched, 1u);
}

TEST_F(TestUsbHandleLinux, test_strings)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";
    const std::array<uint8_t, device_descriptor_size> descriptor = this->read_device_descriptor();

    auto manufacturer = _handle.manufacturer();
    auto product = _handle.product();
    auto serial_number = _handle.serial_number();

    expect_string(manufacturer, descriptor[manufacturer_index_offset], "manufacturer");
    expect_string(product, descriptor[product_index_offset], "product");
    expect_string(serial_number, descriptor[serial_number_index_offset], "serial number");
    if (_fake != nullptr)
    {
        EXPECT_EQ(manufacturer.value_or(""), FakeUsbDevice::manufacturer);
        EXPECT_EQ(product.value_or(""), FakeUsbDevice::product);
    }
}

TEST_F(TestUsbHandleLinux, test_wrap)
{
    if (!_writable.has_value())
        GTEST_SKIP() << "no writable usb device";
    File file;
    auto file_opened = file.open(device_path(*_writable), "r+");
    ASSERT_TRUE(file_opened.has_value()) << file_opened.error().message;

    UsbHandle handle;

    auto wrapped_fd = handle.wrap(file.fd());

    ASSERT_TRUE(wrapped_fd.has_value()) << wrapped_fd.error().message;
    auto wrapped = handle.device();
    ASSERT_TRUE(wrapped.has_value()) << wrapped.error().message;
    UsbDevice expected = *_writable;
    // libusb reads the bus from the path of the descriptor: umockdev serves the node as a socket, which has none
    if (_fake != nullptr)
        expected.bus = 0;
    expect_same_device(*wrapped, expected);
}

TEST_F(TestUsbHandleLinux, test_wrap_invalid_fd)
{
    // once emulated nodes were closed, the umockdev preload takes descriptor -1 for one and blocks in its ioctl
    if (_fake != nullptr)
        GTEST_SKIP() << "umockdev blocks on ioctl(-1)";
    UsbHandle handle;

    auto wrapped = handle.wrap(-1);

    ASSERT_FALSE(wrapped.has_value());
    EXPECT_EQ(wrapped.error().code, ErrorCode::io_error);
    EXPECT_EQ(wrapped.error().message, "cannot wrap usb device: Input/Output Error");
    EXPECT_FALSE(handle.is_open());
}

TEST_F(TestUsbHandleLinux, test_control_read_device_descriptor)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    const std::array<uint8_t, device_descriptor_size> descriptor = this->read_device_descriptor();

    EXPECT_EQ(descriptor[0], device_descriptor_size);
    EXPECT_EQ(endian::read_le<uint16_t>(descriptor.data() + vendor_offset, sizeof(uint16_t)), _writable->vendor_id);
    EXPECT_EQ(endian::read_le<uint16_t>(descriptor.data() + product_offset, sizeof(uint16_t)), _writable->product_id);
}

TEST_F(TestUsbHandleLinux, test_set_auto_detach_kernel_driver)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto enabled = _handle.set_auto_detach_kernel_driver(true);
    auto disabled = _handle.set_auto_detach_kernel_driver(false);

    EXPECT_TRUE(enabled.has_value()) << enabled.error().message;
    EXPECT_TRUE(disabled.has_value()) << disabled.error().message;
}

TEST_F(TestUsbHandleLinux, test_control_read_refuses_out_request)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";
    UsbHandle::ControlSetup setup = get_device_descriptor;
    setup.request_type = out_request_type;

    auto transfer = _handle.control_read(setup, _buffer.data(), _buffer.size(), std::chrono::seconds(1));

    expect_refused(transfer, "usb control read needs an in request type, got 0x00");
}

TEST_F(TestUsbHandleLinux, test_control_write_refuses_in_request)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto transfer = _handle.control_write(get_device_descriptor, ArrByteView(), std::chrono::seconds(1));

    expect_refused(transfer, "usb control write needs an out request type, got 0x80");
}

TEST_F(TestUsbHandleLinux, test_transfer_refuses_zero_timeout)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto transfer = _handle.control_read(get_device_descriptor, _buffer.data(), _buffer.size(), Duration(0));

    expect_refused(transfer, "usb timeout cannot be zero");
}

TEST_F(TestUsbHandleLinux, test_bulk_read_refuses_out_endpoint)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto transfer = _handle.bulk_read(out_endpoint, _buffer.data(), _buffer.size(), std::chrono::seconds(1));

    expect_refused(transfer, "usb bulk read needs an in endpoint, got 0x01");
}

TEST_F(TestUsbHandleLinux, test_interrupt_write_refuses_in_endpoint)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    auto transfer = _handle.interrupt_write(in_endpoint, ArrByteView(), std::chrono::seconds(1));

    expect_refused(transfer, "usb interrupt write needs an out endpoint, got 0x81");
}

TEST_F(TestUsbHandleLinux, test_moved_from)
{
    if (!_handle.is_open())
        GTEST_SKIP() << "no writable usb device";

    UsbHandle moved(std::move(_handle));
    auto moved_from = _handle.product();

    ASSERT_FALSE(moved_from.has_value());
    EXPECT_EQ(moved_from.error().code, ErrorCode::not_initialized);
    EXPECT_EQ(moved_from.error().message, "no usb handle");
    EXPECT_TRUE(moved.device().has_value());
}

TEST_F(TestUsbHandleLinux, test_interrupt_read)
{
    if (_fake == nullptr)
        GTEST_SKIP() << "no fake usb device: runs under umockdev-wrapper";
    this->claim_keyboard();
    std::array<uint8_t, FakeUsbDevice::report.size()> report {};

    auto size = _handle.interrupt_read(FakeUsbDevice::interrupt_endpoint,
                                       report.data(),
                                       report.size(),
                                       std::chrono::seconds(1));

    ASSERT_TRUE(size.has_value()) << size.error().message;
    EXPECT_EQ(*size, report.size());
    EXPECT_EQ(report, FakeUsbDevice::report);
}

TEST_F(TestUsbHandleLinux, test_interrupt_read_stalled)
{
    if (_fake == nullptr)
        GTEST_SKIP() << "no fake usb device: runs under umockdev-wrapper";
    this->claim_keyboard();
    _fake->set_interrupt_status(-EPIPE);

    auto size = _handle.interrupt_read(FakeUsbDevice::interrupt_endpoint,
                                       _buffer.data(),
                                       FakeUsbDevice::report.size(),
                                       std::chrono::seconds(1));

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error().code, ErrorCode::io_error);
    EXPECT_EQ(size.error().message, "cannot complete usb interrupt read: Pipe error");
}

TEST_F(TestUsbHandleLinux, test_interrupt_read_partial_timeout)
{
    if (_fake == nullptr)
        GTEST_SKIP() << "no fake usb device: runs under umockdev-wrapper";
    this->claim_keyboard();
    constexpr size_t partial_size = 3;
    _fake->hold_interrupt(partial_size);

    auto size = _handle.interrupt_read(FakeUsbDevice::interrupt_endpoint,
                                       _buffer.data(),
                                       FakeUsbDevice::report.size(),
                                       std::chrono::milliseconds(50));

    ASSERT_TRUE(size.has_value()) << size.error().message;
    EXPECT_EQ(*size, partial_size);
    EXPECT_TRUE(std::equal(_buffer.begin(), _buffer.begin() + partial_size, FakeUsbDevice::report.begin()));
}

TEST_F(TestUsbHandleLinux, test_interrupt_read_timeout)
{
    if (_fake == nullptr)
        GTEST_SKIP() << "no fake usb device: runs under umockdev-wrapper";
    this->claim_keyboard();
    _fake->hold_interrupt(0);

    auto size = _handle.interrupt_read(FakeUsbDevice::interrupt_endpoint,
                                       _buffer.data(),
                                       FakeUsbDevice::report.size(),
                                       std::chrono::milliseconds(50));

    ASSERT_FALSE(size.has_value());
    EXPECT_EQ(size.error().code, ErrorCode::timeout);
    EXPECT_EQ(size.error().message, "cannot complete usb interrupt read: Operation timed out");
}

} // namespace test
