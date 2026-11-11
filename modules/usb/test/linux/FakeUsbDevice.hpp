#ifndef __SIHD_USB_TEST_FAKEUSBDEVICE_HPP__
#define __SIHD_USB_TEST_FAKEUSBDEVICE_HPP__

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

namespace test
{

// a usb boot keyboard at 001:002 and its read-only twin at 001:003, served by umockdev while the object lives
class FakeUsbDevice
{
    public:
        // pid.codes test vendor
        static constexpr uint16_t vendor_id = 0x1209;
        static constexpr uint16_t product_id = 0x0001;
        static constexpr uint16_t read_only_product_id = 0x0002;
        static constexpr uint8_t interrupt_endpoint = 0x81;
        // string descriptors 1 and 2, the device declares no serial number
        static constexpr std::string_view manufacturer = "sihd";
        static constexpr std::string_view product = "fake keyboard";
        // boot keyboard report: modifiers, reserved, then the 'a' key (usage 0x04) pressed
        static constexpr std::array<uint8_t, 8> report {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};

        // nullptr outside umockdev-wrapper or without umockdev: sysfs and usbfs stay the real ones
        static std::unique_ptr<FakeUsbDevice> create();

        ~FakeUsbDevice();

        // the next interrupt transfers end with this urb status (-EPIPE: stalled endpoint)
        void set_interrupt_status(int status);
        // the next interrupt transfers stay pending until cancelled, then deliver size bytes
        void hold_interrupt(int size);
        // set by CLAIMINTERFACE, cleared by RELEASEINTERFACE only: closing the node keeps it
        bool is_claimed(uint8_t interface_number) const;

    private:
        struct Impl;

        explicit FakeUsbDevice(std::unique_ptr<Impl> impl);

        std::unique_ptr<Impl> _impl;
};

} // namespace test

#endif
