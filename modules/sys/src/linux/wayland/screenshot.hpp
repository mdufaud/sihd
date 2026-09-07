#ifndef __SIHD_SYS_WAYLAND_SCREENSHOT_HPP__
#define __SIHD_SYS_WAYLAND_SCREENSHOT_HPP__

// Wayland screenshot backend of the dispatcher in src/linux/screenshot.cpp:
// native wlr-screencopy capture, grim / spectacle / gnome-screenshot when the
// compositor exposes no capture protocol (KWin, Mutter).

#include <string>
#include <string_view>

#include <sihd/sys/Bitmap.hpp>

namespace sihd::sys::screenshot::wayland
{

#if defined(SIHD_COMPILE_WITH_WAYLAND)

bool take_screen(Bitmap & bm, const std::string & output_name = "");
bool take_focused(Bitmap & bm);
bool take_under_cursor(Bitmap & bm);
bool take_window_name(Bitmap & bm, std::string_view name);

#else

// The backend is not compiled in: inert inline fallbacks.

inline bool take_screen([[maybe_unused]] Bitmap & bm, [[maybe_unused]] const std::string & output_name = "")
{
    return false;
}
inline bool take_focused([[maybe_unused]] Bitmap & bm)
{
    return false;
}
inline bool take_under_cursor([[maybe_unused]] Bitmap & bm)
{
    return false;
}
inline bool take_window_name([[maybe_unused]] Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    return false;
}

#endif

} // namespace sihd::sys::screenshot::wayland

#endif
