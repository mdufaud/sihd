#ifndef __SIHD_SYS_LINUX_INTERNAL_LOGIN1_HPP__
#define __SIHD_SYS_LINUX_INTERNAL_LOGIN1_HPP__

#include <sihd/sys/linux/dbus.hpp>
#include <sihd/util/Result.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::internal
{

// the lock lasts until the returned fd closes
sihd::util::Result<linux_::dbus::UnixFd> login1_inhibit_sleep(linux_::dbus::Connection & connection,
                                                              sihd::util::ZStringView who,
                                                              sihd::util::ZStringView why,
                                                              sihd::util::ZStringView mode);

} // namespace sihd::sys::internal

#endif
