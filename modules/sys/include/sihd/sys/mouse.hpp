#ifndef __SIHD_SYS_MOUSE_HPP__
#define __SIHD_SYS_MOUSE_HPP__

#include <cstdint>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::mouse
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool supported = true;
#else
constexpr bool supported = false;
#endif

enum class MouseButton : uint8_t
{
    left,
    middle,
    right,
    back,
    forward,
};

// Moves the cursor to the virtual desktop position.
sihd::util::Result<void> mouse_move(int x, int y);

// Moves the cursor relatively to its current position.
sihd::util::Result<void> mouse_move_by(int dx, int dy);

// Presses or releases `button` without releasing it automatically.
sihd::util::Result<void> mouse_button(MouseButton button, bool down);

sihd::util::Result<void> mouse_click(MouseButton button = MouseButton::left);

// Scrolls `x` detents horizontally and `y` detents vertically, positive
// scrolling right and up.
sihd::util::Result<void> mouse_scroll(int x, int y);

} // namespace sihd::sys::mouse

#endif
