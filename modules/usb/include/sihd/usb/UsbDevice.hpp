#ifndef __SIHD_USB_USBDEVICE_HPP__
#define __SIHD_USB_USBDEVICE_HPP__

#include <cstdint>
#include <string>
#include <vector>

namespace sihd::usb
{

enum class UsbSpeed
{
    unknown,
    low,
    full,
    high,
    super,
    super_plus
};

struct UsbDevice
{
        uint8_t bus = 0;
        uint8_t address = 0;
        // empty for a root hub
        std::vector<uint8_t> port_path;
        UsbSpeed speed = UsbSpeed::unknown;
        uint16_t usb_version_bcd = 0;
        uint16_t vendor_id = 0;
        uint16_t product_id = 0;
        uint16_t release_bcd = 0;
        uint8_t device_class = 0;
        uint8_t device_subclass = 0;
        uint8_t device_protocol = 0;
        uint8_t max_packet_size0 = 0;
        uint8_t configuration_count = 0;
        std::string vendor_name;
        std::string product_name;
        std::string class_name;
        std::string subclass_name;
        std::string protocol_name;
};

} // namespace sihd::usb

#endif
