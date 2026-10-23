#ifndef __SIHD_SYS_X11_KEYBOARD_HPP__
#define __SIHD_SYS_X11_KEYBOARD_HPP__

// X11 keyboard backend (XTest fake key events); chars outside the keymap use a temporary keycode remap

#include <string_view>

#include <sihd/sys/keyboard.hpp>

namespace sihd::sys::keyboard::x11
{

sihd::util::Result<void> type_text(std::string_view utf8_text);

sihd::util::Result<void> key_press(Key key, Modifiers mods);

sihd::util::Result<void> key_down(Key key);

sihd::util::Result<void> key_up(Key key);

} // namespace sihd::sys::keyboard::x11

#endif
