#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/build.hpp>

#include "libusb.hpp"

using namespace sihd::util;
using enum sihd::util::ErrorCode;

namespace sihd::usb
{

Result<std::vector<UsbDevice>> scan()
{
    if constexpr (build::is_android)
        return std::unexpected(Error(not_supported, "cannot list usb devices on android"));
    auto context = libusb::init_context();
    SIHD_UNEXPECTED_RETURN(context);

    libusb_device **list = nullptr;
    const ssize_t count = libusb_get_device_list(context->get(), &list);
    if (count < 0)
        return std::unexpected(libusb::to_error(static_cast<int>(count), "list usb devices"));
    Defer free_list([list] { libusb_free_device_list(list, 1); });

    std::vector<UsbDevice> devices;
    devices.reserve(static_cast<size_t>(count));
    for (ssize_t i = 0; i < count; ++i)
        devices.emplace_back(libusb::to_device(list[i]));
    return devices;
}

} // namespace sihd::usb
