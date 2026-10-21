#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>
#include <libusb-1.0/libusb.h>

#include <sihd/usb/UsbContext.hpp>
#include <sihd/util/Defer.hpp>

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::util
{
template <>
struct ErrorResolver<libusb_error>
{
        static ErrorCode code(libusb_error native)
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
};
} // namespace sihd::util

namespace sihd::usb
{

namespace
{

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

UsbDevice to_device(libusb_device *device)
{
    // cached by libusb, always succeeds since 1.0.16
    libusb_device_descriptor descriptor {};
    libusb_get_device_descriptor(device, &descriptor);

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
    return ret;
}

} // namespace

struct UsbContext::Impl
{
        libusb_context *context = nullptr;
        std::string init_error;
};

UsbContext::UsbContext(): _impl(std::make_unique<Impl>())
{
    const int rc = libusb_init_context(&_impl->context, nullptr, 0);
    if (rc != LIBUSB_SUCCESS)
    {
        _impl->context = nullptr;
        _impl->init_error = fmt::format("cannot init usb context: {}", libusb_strerror(rc));
    }
}

UsbContext::~UsbContext()
{
    if (_impl != nullptr && _impl->context != nullptr)
        libusb_exit(_impl->context);
}

UsbContext::UsbContext(UsbContext && other) noexcept = default;

UsbContext & UsbContext::operator=(UsbContext && other) noexcept
{
    if (this != &other)
    {
        if (_impl != nullptr && _impl->context != nullptr)
            libusb_exit(_impl->context);
        _impl = std::move(other._impl);
    }
    return *this;
}

Result<std::vector<UsbDevice>> UsbContext::scan() const
{
    if (_impl == nullptr || _impl->context == nullptr)
    {
        const bool has_init_error = _impl != nullptr && !_impl->init_error.empty();
        return std::unexpected(Error(not_initialized, has_init_error ? _impl->init_error : "no usb context"));
    }

    libusb_device **list = nullptr;
    const ssize_t count = libusb_get_device_list(_impl->context, &list);
    if (count < 0)
    {
        const int rc = static_cast<int>(count);
        return std::unexpected(
            Error(static_cast<libusb_error>(rc), "cannot list usb devices: {}", libusb_strerror(rc)));
    }
    Defer free_list([list] { libusb_free_device_list(list, 1); });

    std::vector<UsbDevice> devices;
    devices.reserve(static_cast<size_t>(count));
    for (ssize_t i = 0; i < count; ++i)
        devices.emplace_back(to_device(list[i]));
    return devices;
}

} // namespace sihd::usb
