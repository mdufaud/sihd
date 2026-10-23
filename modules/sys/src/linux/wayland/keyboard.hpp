#ifndef __SIHD_SYS_WAYLAND_KEYBOARD_HPP__
#define __SIHD_SYS_WAYLAND_KEYBOARD_HPP__

// zwp_virtual_keyboard_v1 with a generated keymap, wtype style

#include <string_view>

#include <sihd/sys/keyboard.hpp>

namespace sihd::sys::keyboard::wayland
{

sihd::util::Result<void> type_text(std::string_view utf8_text);

sihd::util::Result<void> key_press(Key key, Modifiers mods);

sihd::util::Result<void> key_down(Key key);

sihd::util::Result<void> key_up(Key key);

} // namespace sihd::sys::keyboard::wayland

#endif
