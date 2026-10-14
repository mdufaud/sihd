#include <sihd/sys/screen.hpp>
#include <sihd/util/Logger.hpp>

// The x11 and wayland backends live under src/linux/{x11,wayland}/ as compile-time
// opts (inline no-ops when off); the session-matching backend dispatches first.

#include "internal/desktop_env.hpp"
#include "internal/dispatch.hpp"
#include "wayland/screen.hpp"
#include "x11/screen.hpp"

using sihd::sys::internal::try_first;

namespace sihd::sys::screen
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screen");

Result<std::vector<Monitor>> monitors()
{
    return try_first(internal::wayland_session(), wayland::monitors, x11::monitors);
}

} // namespace sihd::sys::screen
