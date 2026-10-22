#ifndef __SIHD_SYS_WAYLAND_MOUSE_HPP__
#define __SIHD_SYS_WAYLAND_MOUSE_HPP__

// Wayland mouse backend: zwlr_virtual_pointer_v1, wlroots-style compositors only

#include <sihd/sys/mouse.hpp>

namespace sihd::sys::mouse::wayland
{

#if defined(SIHD_COMPILE_WITH_WAYLAND)

sihd::util::Result<void> mouse_move(int x, int y);

sihd::util::Result<void> mouse_move_by(int dx, int dy);

sihd::util::Result<void> mouse_button(MouseButton button, bool down);

sihd::util::Result<void> mouse_click(MouseButton button);

sihd::util::Result<void> mouse_scroll(int x, int y);

#else

inline sihd::util::Result<void> mouse_move([[maybe_unused]] int x, [[maybe_unused]] int y)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "wayland backend not compiled in"));
}

inline sihd::util::Result<void> mouse_move_by([[maybe_unused]] int dx, [[maybe_unused]] int dy)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "wayland backend not compiled in"));
}

inline sihd::util::Result<void> mouse_button([[maybe_unused]] MouseButton button, [[maybe_unused]] bool down)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "wayland backend not compiled in"));
}

inline sihd::util::Result<void> mouse_click([[maybe_unused]] MouseButton button)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "wayland backend not compiled in"));
}

inline sihd::util::Result<void> mouse_scroll([[maybe_unused]] int x, [[maybe_unused]] int y)
{
    return std::unexpected(sihd::util::Error(sihd::util::ErrorCode::not_supported, "wayland backend not compiled in"));
}

#endif

} // namespace sihd::sys::mouse::wayland

#endif
