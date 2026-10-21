#ifndef __SIHD_USB_LINUX_HWDB_HPP__
#define __SIHD_USB_LINUX_HWDB_HPP__

#include <vector>

#include <sihd/usb/UsbDevice.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/build.hpp>

#if !defined(__SIHD_LINUX__)
# error "sihd/usb/linux headers are linux only"
#endif

namespace sihd::usb::linux_::hwdb
{

sihd::util::Result<void> enrich(std::vector<UsbDevice> & devices);

} // namespace sihd::usb::linux_::hwdb

#endif
