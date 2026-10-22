#include <algorithm>
#include <vector>

// sihd headers first: Xlib poisons common identifiers (None, Status, True, False, ...) with macros
#include <sihd/sys/screen.hpp>
#include <sihd/util/Logger.hpp>

#include "../../screen_math.hpp"
#include "screen.hpp"
#include "x11_display.hpp"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

using enum sihd::util::ErrorCode;

namespace sihd::sys::screen::x11
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screen::x11");

namespace
{

Monitor assemble(const char *name, int x, int y, int width, int height, int width_mm, int height_mm, bool primary)
{
    Monitor monitor;
    if (name != nullptr)
        monitor.name = name;
    monitor.x = x;
    monitor.y = y;
    monitor.width = width;
    monitor.height = height;
    monitor.width_mm = width_mm;
    monitor.height_mm = height_mm;
    monitor.primary = primary;
    // X11 positions and sizes are raw pixels: no compositor scaling.
    monitor.scale = 1.0;
    monitor.dpi = internal::dpi_from(width, width_mm);
    return monitor;
}

std::vector<Monitor> whole_screen(Display *display)
{
    const int screen = DefaultScreen(display);
    return {assemble("screen",
                     0,
                     0,
                     XDisplayWidth(display, screen),
                     XDisplayHeight(display, screen),
                     XDisplayWidthMM(display, screen),
                     XDisplayHeightMM(display, screen),
                     true)};
}

} // namespace

Result<std::vector<Monitor>> monitors()
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return std::unexpected(sihd::sys::x11::open_error());
    Display *display = conn.display;

    int event_base = 0;
    int error_base = 0;
    if (XRRQueryExtension(display, &event_base, &error_base) == 0)
        return whole_screen(display);

    int count = 0;
    XRRMonitorInfo *infos = XRRGetMonitors(display, DefaultRootWindow(display), True, &count);
    // Xvfb and extensionless servers report no monitors at all: the single X screen stands in
    if (infos == nullptr || count <= 0)
    {
        XRRFreeMonitors(infos);
        return whole_screen(display);
    }

    std::vector<Monitor> monitors;
    monitors.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i)
    {
        const XRRMonitorInfo & info = infos[i];
        char *name = XGetAtomName(display, info.name);
        monitors.push_back(
            assemble(name, info.x, info.y, info.width, info.height, info.mwidth, info.mheight, info.primary != 0));
        XFree(name);
    }
    XRRFreeMonitors(infos);
    // RandR may designate no primary output: the first one stands in, like the wayland backend
    if (std::ranges::contains(monitors, true, &Monitor::primary) == false)
        monitors.front().primary = true;
    return monitors;
}

} // namespace sihd::sys::screen::x11
