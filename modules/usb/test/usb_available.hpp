#ifndef __SIHD_USB_TEST_USB_AVAILABLE_HPP__
#define __SIHD_USB_TEST_USB_AVAILABLE_HPP__

#include <sihd/usb/UsbDevice.hpp>

namespace test
{

// false without usbfs, for example in a container
inline bool usb_available()
{
    return sihd::usb::scan().has_value();
}

} // namespace test

#endif
