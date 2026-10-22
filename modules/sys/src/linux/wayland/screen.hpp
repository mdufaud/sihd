#ifndef __SIHD_SYS_WAYLAND_SCREEN_HPP__
#define __SIHD_SYS_WAYLAND_SCREEN_HPP__

#include <sihd/sys/screen.hpp>

namespace sihd::sys::screen::wayland
{

#if defined(SIHD_COMPILE_WITH_WAYLAND)

sihd::util::Result<std::vector<sihd::sys::screen::Monitor>> monitors();

#else

inline sihd::util::Result<std::vector<sihd::sys::screen::Monitor>> monitors()
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "wayland backend not compiled in"));
}

#endif

} // namespace sihd::sys::screen::wayland

#endif
