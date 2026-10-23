#ifndef __SIHD_SYS_WAYLAND_SCREEN_HPP__
#define __SIHD_SYS_WAYLAND_SCREEN_HPP__

#include <sihd/sys/screen.hpp>

namespace sihd::sys::screen::wayland
{

sihd::util::Result<std::vector<sihd::sys::screen::Monitor>> monitors();

} // namespace sihd::sys::screen::wayland

#endif
