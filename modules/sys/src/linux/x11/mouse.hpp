#ifndef __SIHD_SYS_X11_MOUSE_HPP__
#define __SIHD_SYS_X11_MOUSE_HPP__

#include <sihd/sys/mouse.hpp>

namespace sihd::sys::mouse::x11
{

sihd::util::Result<void> mouse_move(int x, int y);

sihd::util::Result<void> mouse_move_by(int dx, int dy);

sihd::util::Result<void> mouse_button(MouseButton button, bool down);

sihd::util::Result<void> mouse_click(MouseButton button);

sihd::util::Result<void> mouse_scroll(int x, int y);

} // namespace sihd::sys::mouse::x11

#endif
