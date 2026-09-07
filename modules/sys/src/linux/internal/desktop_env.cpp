#include "desktop_env.hpp"

#include <cstdlib>
#include <cstring>

namespace sihd::sys::internal
{

bool wayland_session()
{
    const char *session_type = getenv("XDG_SESSION_TYPE");
    return getenv("WAYLAND_DISPLAY") != nullptr || getenv("WAYLAND_SOCKET") != nullptr
           || (session_type != nullptr && strcmp(session_type, "wayland") == 0);
}

} // namespace sihd::sys::internal
