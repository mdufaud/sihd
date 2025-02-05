#ifndef __SIHD_UTIL_SCREENSHOT_HPP__
#define __SIHD_UTIL_SCREENSHOT_HPP__

#include <string_view>

#include <sihd/util/Bitmap.hpp>
#include <sihd/util/platform.hpp>

namespace sihd::util::screenshot
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(__SIHD_WINDOWS__)
constexpr bool usable = true;
#else
constexpr bool usable = false;
#endif

bool take_screen(Bitmap & bm);
bool take_under_cursor(Bitmap & bm);
bool take_focused(Bitmap & bm);
bool take_window_name(Bitmap & bm, std::string_view name);

} // namespace sihd::util::screenshot

#endif
