#ifndef __SIHD_SYS_X11_SCREEN_HPP__
#define __SIHD_SYS_X11_SCREEN_HPP__

// RandR monitors, one whole-screen entry when the extension is missing

#include <sihd/sys/screen.hpp>

namespace sihd::sys::screen::x11
{

#if defined(SIHD_COMPILE_WITH_X11)

sihd::util::Result<std::vector<sihd::sys::screen::Monitor>> monitors();

#else

inline sihd::util::Result<std::vector<sihd::sys::screen::Monitor>> monitors()
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "x11 backend not compiled in"));
}

#endif

} // namespace sihd::sys::screen::x11

#endif
