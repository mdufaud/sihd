#ifndef __SIHD_SYS_X11_SCREENSHOT_HPP__
#define __SIHD_SYS_X11_SCREENSHOT_HPP__

// X11 screenshot backend - XGetImage based captures. The dispatcher in
// src/linux/screenshot.cpp is its only user.

#include <string_view>

#include <sihd/sys/Bitmap.hpp>

namespace sihd::sys::screenshot::x11
{

#if defined(SIHD_COMPILE_WITH_X11)

bool take_screen(Bitmap & bm);

bool take_focused(Bitmap & bm);

bool take_under_cursor(Bitmap & bm);

bool take_window_name(Bitmap & bm, std::string_view name);

#else

// The backend is not compiled in: inert inline fallbacks.

inline bool take_screen([[maybe_unused]] Bitmap & bm)
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

} // namespace sihd::sys::screenshot::x11

#endif
