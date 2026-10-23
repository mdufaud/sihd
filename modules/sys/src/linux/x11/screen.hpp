#ifndef __SIHD_SYS_X11_SCREEN_HPP__
#define __SIHD_SYS_X11_SCREEN_HPP__

// RandR monitors, one whole-screen entry when the extension is missing

#include <sihd/sys/screen.hpp>

namespace sihd::sys::screen::x11
{

sihd::util::Result<std::vector<sihd::sys::screen::Monitor>> monitors();

} // namespace sihd::sys::screen::x11

#endif
