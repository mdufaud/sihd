#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Logger.hpp>

// The x11 and wayland backends live under src/linux/{x11,wayland}/ as compile-time
// opts (inline no-ops when off); the session-matching backend dispatches first.

#include "internal/deadline.hpp"
#include "internal/desktop_env.hpp"
#include "wayland/clipboard.hpp"
#include "x11/clipboard.hpp"

#pragma message(                                                                                                       \
    "clipboard: image formats are exposed as raw bytes only - pixel decoding comes later in a dedicated image module")

namespace sihd::sys::clipboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

namespace
{

constexpr Duration serve_timeout = time::sec(10);

} // namespace

std::vector<RawContent> get_raw()
{
    return get_raw({});
}

std::vector<RawContent> get_raw(const std::vector<std::string_view> & wanted_mimes)
{
    if (internal::wayland_session())
    {
        auto res = wayland::get_raw(wanted_mimes);
        if (!res.empty())
            return res;
        return x11::get_raw(wanted_mimes);
    }
    auto res = x11::get_raw(wanted_mimes);
    if (!res.empty())
        return res;
    return wayland::get_raw(wanted_mimes);
}

bool set_raw(const std::vector<RawContentView> & contents)
{
    const Timestamp deadline = internal::deadline_from_now(serve_timeout);
    if (internal::wayland_session())
        return wayland::set_raw(contents, deadline) || x11::set_raw(contents, deadline);
    return x11::set_raw(contents, deadline) || wayland::set_raw(contents, deadline);
}

} // namespace sihd::sys::clipboard
