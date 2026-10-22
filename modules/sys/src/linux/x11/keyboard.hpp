#ifndef __SIHD_SYS_X11_KEYBOARD_HPP__
#define __SIHD_SYS_X11_KEYBOARD_HPP__

// X11 keyboard backend (XTest fake key events); chars outside the keymap use a temporary keycode remap

#include <string_view>

#include <sihd/sys/keyboard.hpp>

namespace sihd::sys::keyboard::x11
{

#if defined(SIHD_COMPILE_WITH_X11)

sihd::util::Result<void> type_text(std::string_view utf8_text);

sihd::util::Result<void> key_press(Key key, Modifiers mods);

sihd::util::Result<void> key_down(Key key);

sihd::util::Result<void> key_up(Key key);

#else

inline sihd::util::Result<void> type_text([[maybe_unused]] std::string_view utf8_text)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "x11 backend not compiled in"));
}

inline sihd::util::Result<void> key_press([[maybe_unused]] Key key, [[maybe_unused]] Modifiers mods)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "x11 backend not compiled in"));
}

inline sihd::util::Result<void> key_down([[maybe_unused]] Key key)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "x11 backend not compiled in"));
}

inline sihd::util::Result<void> key_up([[maybe_unused]] Key key)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "x11 backend not compiled in"));
}

#endif

} // namespace sihd::sys::keyboard::x11

#endif
