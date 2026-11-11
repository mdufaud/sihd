#ifndef __SIHD_USB_LIBUSB_HPP__
#define __SIHD_USB_LIBUSB_HPP__

#include <memory>
#include <string_view>

#include <libusb-1.0/libusb.h>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Duration.hpp>
#include <sihd/util/Error.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::util
{
template <>
struct ErrorResolver<libusb_error>
{
        static ErrorCode code(libusb_error native);
};
} // namespace sihd::util

namespace sihd::usb::libusb
{

sihd::util::Error to_error(int rc, std::string_view operation);

struct ContextDeleter
{
        void operator()(libusb_context *context) const;
};

using Context = std::unique_ptr<libusb_context, ContextDeleter>;

// an android app cannot list /dev/bus/usb: its devices come from UsbManager file descriptors
sihd::util::Result<Context> init_context();

// cached by libusb, always succeeds since 1.0.16
libusb_device_descriptor descriptor_of(libusb_device *device);

UsbDevice to_device(libusb_device *device);

// libusb reads 0 as no timeout: a negative duration waits forever, zero is refused
sihd::util::Result<unsigned int> to_timeout(sihd::util::Duration timeout);

} // namespace sihd::usb::libusb

#endif
