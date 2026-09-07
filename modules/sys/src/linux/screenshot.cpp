#include <sihd/sys/screenshot.hpp>
#include <sihd/util/Logger.hpp>

// The x11 and wayland backends live under src/linux/{x11,wayland}/ as compile-time
// opts (inline no-ops when off); the session-matching backend dispatches first.

#include "internal/desktop_env.hpp"
#include "wayland/screenshot.hpp"
#include "x11/screenshot.hpp"

namespace sihd::sys::screenshot
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

bool take_screen(Bitmap & bm)
{
    bm.clear();
    if (internal::wayland_session())
        return wayland::take_screen(bm) || x11::take_screen(bm);
    return x11::take_screen(bm) || wayland::take_screen(bm);
}

bool take_focused(Bitmap & bm)
{
    bm.clear();
    if (internal::wayland_session())
        return wayland::take_focused(bm) || x11::take_focused(bm);
    return x11::take_focused(bm) || wayland::take_focused(bm);
}

bool take_under_cursor(Bitmap & bm)
{
    bm.clear();
    if (internal::wayland_session())
        return wayland::take_under_cursor(bm) || x11::take_under_cursor(bm);
    return x11::take_under_cursor(bm) || wayland::take_under_cursor(bm);
}

bool take_window_name(Bitmap & bm, std::string_view name)
{
    bm.clear();
    if (internal::wayland_session())
        return wayland::take_window_name(bm, name) || x11::take_window_name(bm, name);
    return x11::take_window_name(bm, name) || wayland::take_window_name(bm, name);
}

} // namespace sihd::sys::screenshot
