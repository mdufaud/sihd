#ifndef __SIHD_USB_USBCONTEXT_HPP__
#define __SIHD_USB_USBCONTEXT_HPP__

#include <memory>
#include <vector>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::usb
{

class UsbContext
{
    public:
        UsbContext();
        ~UsbContext();

        UsbContext(const UsbContext &) = delete;
        UsbContext & operator=(const UsbContext &) = delete;
        UsbContext(UsbContext && other) noexcept;
        UsbContext & operator=(UsbContext && other) noexcept;

        sihd::util::Result<std::vector<UsbDevice>> scan() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;
};

} // namespace sihd::usb

#endif
