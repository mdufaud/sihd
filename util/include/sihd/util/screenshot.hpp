#ifndef __SIHD_UTIL_SCREENSHOT_HPP__
#define __SIHD_UTIL_SCREENSHOT_HPP__

#include <string_view>

#include <sihd/util/Bitmap.hpp>

namespace sihd::util::screenshot
{

bool take_all(Bitmap & bm);
bool take_focus(Bitmap & bm);

bool take(Bitmap & bm, std::string_view name);

} // namespace sihd::util::screenshot

#endif
