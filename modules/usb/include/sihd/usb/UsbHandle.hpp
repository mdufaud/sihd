#ifndef __SIHD_USB_USBHANDLE_HPP__
#define __SIHD_USB_USBHANDLE_HPP__

#include <cstdint>
#include <memory>
#include <string>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/ArrayView.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::usb
{

class UsbHandle
{
    public:
        // the 8 setup bytes of a control transfer, bit 7 of request_type gives its direction
        struct ControlSetup
        {
                uint8_t request_type = 0;
                uint8_t request = 0;
                uint16_t value = 0;
                uint16_t index = 0;
        };

        UsbHandle();
        ~UsbHandle();

        UsbHandle(UsbHandle && other) noexcept;
        UsbHandle & operator=(UsbHandle && other) noexcept;

        UsbHandle(const UsbHandle &) = delete;
        UsbHandle & operator=(const UsbHandle &) = delete;

        // not_found once the device was unplugged, got a new address or another device took its address
        sihd::util::Result<void> open(const UsbDevice & device);
        // the caller keeps owning fd and closes it after the handle
        sihd::util::Result<void> wrap(int fd);
        void close();
        bool is_open() const;

        sihd::util::Result<UsbDevice> device() const;

        // not_found when the device declares no such string
        sihd::util::Result<std::string> manufacturer() const;
        sihd::util::Result<std::string> product() const;
        sihd::util::Result<std::string> serial_number() const;

        sihd::util::Result<uint8_t> configuration() const;
        sihd::util::Result<void> set_configuration(uint8_t value);
        sihd::util::Result<void> claim_interface(uint8_t interface_number);
        sihd::util::Result<void> release_interface(uint8_t interface_number);
        sihd::util::Result<void> set_alt_setting(uint8_t interface_number, uint8_t alt_setting);
        sihd::util::Result<void> set_auto_detach_kernel_driver(bool active);
        sihd::util::Result<void> clear_halt(uint8_t endpoint);
        // the device may enumerate again: the handle then answers not_found
        sihd::util::Result<void> reset();

        // a negative timeout waits forever, zero is refused
        sihd::util::Result<size_t>
            control_read(const ControlSetup & setup, void *data, size_t size, sihd::util::Duration timeout);
        sihd::util::Result<size_t>
            control_write(const ControlSetup & setup, sihd::util::ArrByteView data, sihd::util::Duration timeout);
        // bulk and interrupt: a timeout after a partial transfer returns its size
        sihd::util::Result<size_t> bulk_read(uint8_t endpoint, void *data, size_t size, sihd::util::Duration timeout);
        sihd::util::Result<size_t>
            bulk_write(uint8_t endpoint, sihd::util::ArrByteView data, sihd::util::Duration timeout);
        sihd::util::Result<size_t>
            interrupt_read(uint8_t endpoint, void *data, size_t size, sihd::util::Duration timeout);
        sihd::util::Result<size_t>
            interrupt_write(uint8_t endpoint, sihd::util::ArrByteView data, sihd::util::Duration timeout);

    private:
        struct Impl;

        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::usb

#endif
