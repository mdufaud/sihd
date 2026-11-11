#ifndef __SIHD_USB_USBDEVICE_HPP__
#define __SIHD_USB_USBDEVICE_HPP__

#include <cstdint>
#include <string>
#include <vector>

#include <sihd/util/Result.hpp>

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

enum class UsbDirection
{
    out,
    in
};

enum class UsbTransferType
{
    control,
    isochronous,
    bulk,
    interrupt
};

struct UsbEndpoint
{
        // bit 7 set for an in endpoint
        uint8_t address = 0;
        UsbDirection direction = UsbDirection::out;
        UsbTransferType transfer_type = UsbTransferType::control;
        uint16_t max_packet_size = 0;
        uint8_t interval = 0;
};

struct UsbAltSetting
{
        uint8_t number = 0;
        uint8_t interface_class = 0;
        uint8_t interface_subclass = 0;
        uint8_t interface_protocol = 0;
        std::vector<UsbEndpoint> endpoints;
};

struct UsbInterface
{
        uint8_t number = 0;
        std::vector<UsbAltSetting> alt_settings;
};

struct UsbConfiguration
{
        uint8_t value = 0;
        uint8_t attributes = 0;
        uint16_t max_power_ma = 0;
        std::vector<UsbInterface> interfaces;
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
        // smaller than configuration_count when the system refused to read one
        std::vector<UsbConfiguration> configurations;
        std::string vendor_name;
        std::string product_name;
        std::string class_name;
        std::string subclass_name;
        std::string protocol_name;
};

// lists the connected devices without opening them, not_supported on android: an app cannot list /dev/bus/usb
sihd::util::Result<std::vector<UsbDevice>> scan();

} // namespace sihd::usb

#endif
