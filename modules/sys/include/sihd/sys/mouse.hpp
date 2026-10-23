#ifndef __SIHD_SYS_MOUSE_HPP__
#define __SIHD_SYS_MOUSE_HPP__

#include <cstdint>

#include <sihd/util/Result.hpp>

namespace sihd::sys::mouse
{

bool supported();

enum class MouseButton : uint8_t
{
    left,
    middle,
    right,
    back,
    forward,
};

sihd::util::Result<void> mouse_move(int x, int y);

sihd::util::Result<void> mouse_move_by(int dx, int dy);

sihd::util::Result<void> mouse_button(MouseButton button, bool down);

sihd::util::Result<void> mouse_click(MouseButton button = MouseButton::left);

// scrolls x detents horizontally and y vertically; positive is right and up
sihd::util::Result<void> mouse_scroll(int x, int y);

} // namespace sihd::sys::mouse

#endif
