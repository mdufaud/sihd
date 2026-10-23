#ifndef __SIHD_SYS_X11_SCREENSHOT_HPP__
#define __SIHD_SYS_X11_SCREENSHOT_HPP__

#include <string_view>

#include <sihd/sys/Bitmap.hpp>

namespace sihd::sys::screenshot::x11
{

bool take_screen(Bitmap & bm);

bool take_focused(Bitmap & bm);

bool take_under_cursor(Bitmap & bm);

bool take_window_name(Bitmap & bm, std::string_view name);

} // namespace sihd::sys::screenshot::x11

#endif
