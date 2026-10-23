#ifndef __SIHD_SYS_LINUX_DESKTOP_ENV_HPP__
#define __SIHD_SYS_LINUX_DESKTOP_ENV_HPP__

namespace sihd::sys::internal
{

bool wayland_session();

bool x11_compiled();
bool wayland_compiled();

} // namespace sihd::sys::internal

#endif
