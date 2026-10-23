#include <sihd/sys/keyboard.hpp>
#include <sihd/util/Logger.hpp>

// x11 and wayland backends (src/linux/{x11,wayland}/) are compile-time opts; the session-matching one dispatches first

#include "internal/desktop_env.hpp"
#include "internal/dispatch.hpp"
#include "wayland/keyboard.hpp"
#include "x11/keyboard.hpp"

using sihd::sys::internal::try_first;

namespace sihd::sys::keyboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::keyboard");

bool supported()
{
    return internal::x11_compiled() || internal::wayland_compiled();
}

Result<void> type_text(std::string_view utf8_text)
{
    return try_first(internal::wayland_session(), wayland::type_text, x11::type_text, utf8_text);
}

Result<void> key_press(Key key, Modifiers mods)
{
    return try_first(internal::wayland_session(), wayland::key_press, x11::key_press, key, mods);
}

Result<void> key_down(Key key)
{
    return try_first(internal::wayland_session(), wayland::key_down, x11::key_down, key);
}

Result<void> key_up(Key key)
{
    return try_first(internal::wayland_session(), wayland::key_up, x11::key_up, key);
}

} // namespace sihd::sys::keyboard
