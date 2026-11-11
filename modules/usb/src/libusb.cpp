#include "libusb.hpp"

#include <algorithm>
#include <array>

#include <sihd/util/Defer.hpp>
#include <sihd/util/build.hpp>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::util
{

ErrorCode ErrorResolver<libusb_error>::code(libusb_error native)
{
    switch (native)
    {
        case LIBUSB_ERROR_IO:
        case LIBUSB_ERROR_PIPE:
        case LIBUSB_ERROR_BUSY:
            return io_error;
        case LIBUSB_ERROR_INVALID_PARAM:
            return invalid_argument;
        case LIBUSB_ERROR_ACCESS:
            return permission_denied;
        case LIBUSB_ERROR_NO_DEVICE:
        case LIBUSB_ERROR_NOT_FOUND:
            return not_found;
        case LIBUSB_ERROR_TIMEOUT:
            return timeout;
        case LIBUSB_ERROR_OVERFLOW:
            return overflow;
        case LIBUSB_ERROR_INTERRUPTED:
            return interrupted;
        case LIBUSB_ERROR_NO_MEM:
            return out_of_memory;
        case LIBUSB_ERROR_NOT_SUPPORTED:
            return not_supported;
        default:
            return unknown;
    }
}

} // namespace sihd::util

namespace sihd::usb::libusb
{

namespace
{

// bMaxPower unit in mA
constexpr uint16_t power_unit_superspeed = 8;
constexpr uint16_t power_unit = 2;
// wMaxPacketSize bits 10:0, bits 12:11 count the extra transactions per microframe of a high-speed periodic endpoint
constexpr uint16_t packet_size_mask = 0x07ff;

UsbSpeed to_speed(int speed)
{
    switch (speed)
    {
        case LIBUSB_SPEED_LOW:
            return UsbSpeed::low;
        case LIBUSB_SPEED_FULL:
            return UsbSpeed::full;
        case LIBUSB_SPEED_HIGH:
            return UsbSpeed::high;
        case LIBUSB_SPEED_SUPER:
            return UsbSpeed::super;
        case LIBUSB_SPEED_SUPER_PLUS:
            return UsbSpeed::super_plus;
        default:
            return UsbSpeed::unknown;
    }
}

UsbTransferType to_transfer_type(uint8_t attributes)
{
    switch (attributes & LIBUSB_TRANSFER_TYPE_MASK)
    {
        case LIBUSB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS:
            return UsbTransferType::isochronous;
        case LIBUSB_ENDPOINT_TRANSFER_TYPE_BULK:
            return UsbTransferType::bulk;
        case LIBUSB_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
            return UsbTransferType::interrupt;
        default:
            return UsbTransferType::control;
    }
}

UsbAltSetting to_alt_setting(const libusb_interface_descriptor & descriptor)
{
    UsbAltSetting ret;
    ret.number = descriptor.bAlternateSetting;
    ret.interface_class = descriptor.bInterfaceClass;
    ret.interface_subclass = descriptor.bInterfaceSubClass;
    ret.interface_protocol = descriptor.bInterfaceProtocol;
    ret.endpoints.reserve(descriptor.bNumEndpoints);
    for (uint8_t i = 0; i < descriptor.bNumEndpoints; ++i)
    {
        const libusb_endpoint_descriptor & endpoint = descriptor.endpoint[i];
        ret.endpoints.emplace_back(UsbEndpoint {
            .address = endpoint.bEndpointAddress,
            .direction = (endpoint.bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN
                             ? UsbDirection::in
                             : UsbDirection::out,
            .transfer_type = to_transfer_type(endpoint.bmAttributes),
            .max_packet_size = static_cast<uint16_t>(endpoint.wMaxPacketSize & packet_size_mask),
            .interval = endpoint.bInterval,
        });
    }
    return ret;
}

UsbConfiguration to_configuration(const libusb_config_descriptor & descriptor, UsbSpeed speed)
{
    const bool superspeed = speed == UsbSpeed::super || speed == UsbSpeed::super_plus;

    UsbConfiguration ret;
    ret.value = descriptor.bConfigurationValue;
    ret.attributes = descriptor.bmAttributes;
    ret.max_power_ma = descriptor.MaxPower * (superspeed ? power_unit_superspeed : power_unit);
    ret.interfaces.reserve(descriptor.bNumInterfaces);
    for (uint8_t i = 0; i < descriptor.bNumInterfaces; ++i)
    {
        const libusb_interface & entry = descriptor.interface[i];
        UsbInterface usb_interface;
        if (entry.num_altsetting > 0)
            usb_interface.number = entry.altsetting[0].bInterfaceNumber;
        for (int alt = 0; alt < entry.num_altsetting; ++alt)
            usb_interface.alt_settings.emplace_back(to_alt_setting(entry.altsetting[alt]));
        ret.interfaces.emplace_back(std::move(usb_interface));
    }
    return ret;
}

} // namespace

Error to_error(int rc, std::string_view operation)
{
    return Error(static_cast<libusb_error>(rc), "cannot {}: {}", operation, libusb_strerror(rc));
}

void ContextDeleter::operator()(libusb_context *context) const
{
    libusb_exit(context);
}

Result<Context> init_context()
{
    libusb_context *context = nullptr;
    int rc;
    if constexpr (build::is_android)
    {
        const libusb_init_option no_discovery {.option = LIBUSB_OPTION_NO_DEVICE_DISCOVERY, .value = {.ival = 0}};
        rc = libusb_init_context(&context, &no_discovery, 1);
    }
    else
    {
        rc = libusb_init_context(&context, nullptr, 0);
    }
    if (rc != LIBUSB_SUCCESS)
        return std::unexpected(Error(not_initialized, "cannot init usb context: {}", libusb_strerror(rc)));
    return Context(context);
}

libusb_device_descriptor descriptor_of(libusb_device *device)
{
    libusb_device_descriptor descriptor {};
    libusb_get_device_descriptor(device, &descriptor);
    return descriptor;
}

UsbDevice to_device(libusb_device *device)
{
    const libusb_device_descriptor descriptor = descriptor_of(device);

    // 7 tiers is the usb 3 spec maximum: the call cannot overflow
    std::array<uint8_t, 7> ports {};
    const int port_count = libusb_get_port_numbers(device, ports.data(), static_cast<int>(ports.size()));

    UsbDevice ret;
    ret.bus = libusb_get_bus_number(device);
    ret.address = libusb_get_device_address(device);
    ret.port_path.assign(ports.begin(), ports.begin() + std::max(port_count, 0));
    ret.speed = to_speed(libusb_get_device_speed(device));

    ret.usb_version_bcd = descriptor.bcdUSB;
    ret.vendor_id = descriptor.idVendor;
    ret.product_id = descriptor.idProduct;
    ret.release_bcd = descriptor.bcdDevice;
    ret.device_class = descriptor.bDeviceClass;
    ret.device_subclass = descriptor.bDeviceSubClass;
    ret.device_protocol = descriptor.bDeviceProtocol;
    ret.max_packet_size0 = descriptor.bMaxPacketSize0;
    ret.configuration_count = descriptor.bNumConfigurations;

    ret.configurations.reserve(descriptor.bNumConfigurations);
    for (uint8_t index = 0; index < descriptor.bNumConfigurations; ++index)
    {
        libusb_config_descriptor *config = nullptr;
        if (libusb_get_config_descriptor(device, index, &config) != LIBUSB_SUCCESS)
            continue;
        Defer free_config([config] { libusb_free_config_descriptor(config); });
        ret.configurations.emplace_back(to_configuration(*config, ret.speed));
    }
    return ret;
}

Result<unsigned int> to_timeout(Duration timeout)
{
    if (timeout.nanoseconds() == 0)
        return std::unexpected(Error(invalid_argument, "usb timeout cannot be zero"));
    if (timeout.nanoseconds() < 0)
        return 0u;
    // below one millisecond would become 0, the libusb infinite wait
    return static_cast<unsigned int>(std::max(timeout.milliseconds_int(), 1));
}

} // namespace sihd::usb::libusb
