#ifndef __SIHD_SYS_WAYLAND_SCREENSHOT_HPP__
#define __SIHD_SYS_WAYLAND_SCREENSHOT_HPP__

// native wlr-screencopy, else grim / spectacle / gnome-screenshot when no capture protocol is exposed

#include <string>
#include <string_view>

#include <sihd/sys/Bitmap.hpp>

namespace sihd::sys::screenshot::wayland
{

bool take_screen(Bitmap & bm, const std::string & output_name = "");
bool take_focused(Bitmap & bm);
bool take_under_cursor(Bitmap & bm);
bool take_window_name(Bitmap & bm, std::string_view name);

} // namespace sihd::sys::screenshot::wayland

#endif
