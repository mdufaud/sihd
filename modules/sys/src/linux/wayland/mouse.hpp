#ifndef __SIHD_SYS_WAYLAND_MOUSE_HPP__
#define __SIHD_SYS_WAYLAND_MOUSE_HPP__

// Wayland mouse backend: zwlr_virtual_pointer_v1, wlroots-style compositors only

#include <sihd/sys/mouse.hpp>

namespace sihd::sys::mouse::wayland
{

sihd::util::Result<void> mouse_move(int x, int y);

sihd::util::Result<void> mouse_move_by(int dx, int dy);

sihd::util::Result<void> mouse_button(MouseButton button, bool down);

sihd::util::Result<void> mouse_click(MouseButton button);

sihd::util::Result<void> mouse_scroll(int x, int y);

} // namespace sihd::sys::mouse::wayland

#endif
