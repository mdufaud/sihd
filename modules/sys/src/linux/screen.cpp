#include "wayland/screen.hpp"

#include <sihd/sys/screen.hpp>
#include <sihd/util/Logger.hpp>

#include "internal/desktop_env.hpp"
#include "internal/dispatch.hpp"
#include "x11/screen.hpp"

using sihd::sys::internal::try_first;

namespace sihd::sys::screen
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screen");

bool supported()
{
    return internal::x11_compiled() || internal::wayland_compiled();
}

Result<std::vector<Monitor>> monitors()
{
    return try_first(internal::wayland_session(), wayland::monitors, x11::monitors);
}

} // namespace sihd::sys::screen
