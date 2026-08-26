#include <sihd/sys/clipboard.hpp>
#include <sihd/util/Logger.hpp>

// X11 and Wayland backends. They are compile-time options of this unix build
// (x11 / wayland scons opts); the backend implementations live in src/linux/x11/
// and src/linux/wayland/. Without either option every operation is a no-op
// returning false/null.

#include "backends.hpp"

namespace sihd::sys::clipboard
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::clipboard");

bool set_text(std::string_view str)
{
    return x11_set_clipboard(str) || wayland_set_clipboard(str);
}

bool set_image(const Bitmap & bitmap)
{
    return x11_set_clipboard_image(bitmap) || wayland_set_clipboard_image(bitmap);
}

std::optional<std::string> get_text()
{
    std::optional<std::string> ret;

    ret = x11_get_clipboard();
    if (!ret.has_value())
        ret = wayland_get_clipboard();

    return ret;
}

std::optional<Bitmap> get_image()
{
    std::optional<Bitmap> ret;

    ret = x11_get_clipboard_image();
    if (!ret.has_value())
        ret = wayland_get_clipboard_image();

    return ret;
}

std::optional<Content> get_any()
{
    // Try image first
    if (auto img = get_image(); img.has_value())
        return Content {std::move(*img)};

    // Fall back to text
    if (auto txt = get_text(); txt.has_value())
        return Content {std::move(*txt)};

    return std::nullopt;
}

} // namespace sihd::sys::clipboard
