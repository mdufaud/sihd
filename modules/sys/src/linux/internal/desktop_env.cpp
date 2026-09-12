#include "desktop_env.hpp"

#include <sihd/sys/env.hpp>

namespace sihd::sys::internal
{

bool wayland_session()
{
    const std::optional<std::string> session_type = env::get("XDG_SESSION_TYPE");
    return env::get("WAYLAND_DISPLAY").has_value() || env::get("WAYLAND_SOCKET").has_value()
           || (session_type.has_value() && *session_type == "wayland");
}

} // namespace sihd::sys::internal
