#ifndef __SIHD_SYS_SCREENSHOT_HPP__
#define __SIHD_SYS_SCREENSHOT_HPP__

#include <string_view>

#include <sihd/sys/Bitmap.hpp>
#include <sihd/sys/platform.hpp>

namespace sihd::sys::screenshot
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool supported = true;
#else
constexpr bool supported = false;
#endif

bool take_screen(Bitmap & bm);
bool take_under_cursor(Bitmap & bm);
bool take_focused(Bitmap & bm);
bool take_window_name(Bitmap & bm, std::string_view name);

} // namespace sihd::sys::screenshot

#endif
