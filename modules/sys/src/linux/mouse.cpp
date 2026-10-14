#include <sihd/sys/mouse.hpp>
#include <sihd/util/Logger.hpp>

// The x11 and wayland backends live under src/linux/{x11,wayland}/ as compile-time
// opts (inline no-ops when off); the session-matching backend dispatches first.

#include "internal/desktop_env.hpp"
#include "internal/dispatch.hpp"
#include "wayland/mouse.hpp"
#include "x11/mouse.hpp"

using sihd::sys::internal::try_first;

namespace sihd::sys::mouse
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::mouse");

Result<void> mouse_move(int x, int y)
{
    return try_first(internal::wayland_session(), wayland::mouse_move, x11::mouse_move, x, y);
}

Result<void> mouse_move_by(int dx, int dy)
{
    return try_first(internal::wayland_session(), wayland::mouse_move_by, x11::mouse_move_by, dx, dy);
}

Result<void> mouse_button(MouseButton button, bool down)
{
    return try_first(internal::wayland_session(), wayland::mouse_button, x11::mouse_button, button, down);
}

Result<void> mouse_click(MouseButton button)
{
    return try_first(internal::wayland_session(), wayland::mouse_click, x11::mouse_click, button);
}

Result<void> mouse_scroll(int x, int y)
{
    return try_first(internal::wayland_session(), wayland::mouse_scroll, x11::mouse_scroll, x, y);
}

} // namespace sihd::sys::mouse
