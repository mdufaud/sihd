#ifndef __SIHD_SYS_LINUX_INTERNAL_DESKTOP_BUS_HPP__
#define __SIHD_SYS_LINUX_INTERNAL_DESKTOP_BUS_HPP__

#include <sihd/util/Duration.hpp>
#include <sihd/util/ZStringView.hpp>
#include <sihd/util/time.hpp>

namespace sihd::sys::internal
{

inline constexpr sihd::util::Duration dbus_call_timeout(sihd::util::time::sec(5));
inline constexpr sihd::util::ZStringView portal_service = "org.freedesktop.portal.Desktop";
inline constexpr sihd::util::ZStringView portal_path = "/org/freedesktop/portal/desktop";

} // namespace sihd::sys::internal

#endif
