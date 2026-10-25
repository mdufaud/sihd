#ifndef __SIHD_SYS_SCREENSHOT_HPP__
#define __SIHD_SYS_SCREENSHOT_HPP__

#include <sihd/sys/Bitmap.hpp>
#include <sihd/util/ZStringView.hpp>

namespace sihd::sys::screenshot
{

bool supported();

bool take_screen(Bitmap & bm);
bool take_under_cursor(Bitmap & bm);
bool take_focused(Bitmap & bm);
bool take_window_name(Bitmap & bm, sihd::util::ZStringView name);

} // namespace sihd::sys::screenshot

#endif
