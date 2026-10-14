#ifndef __SIHD_SYS_SCREEN_HPP__
#define __SIHD_SYS_SCREEN_HPP__

#include <string>
#include <vector>

#include <sihd/sys/platform.hpp>
#include <sihd/util/Result.hpp>

namespace sihd::sys::screen
{

#if defined(SIHD_COMPILE_WITH_X11) || defined(SIHD_COMPILE_WITH_WAYLAND) || defined(__SIHD_WINDOWS__)
constexpr bool supported = true;
#else
constexpr bool supported = false;
#endif

// One physical display of the virtual desktop. x, y, width and height are
// logical desktop units; on X11 they are the raw pixels.
struct Monitor
{
        std::string name;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        int width_mm = 0;
        int height_mm = 0;
        double scale = 1.0;
        double dpi = 0.0;
        bool primary = false;
};

sihd::util::Result<std::vector<Monitor>> monitors();

sihd::util::Result<Monitor> primary();

// The monitor holding the desktop point, not_found when the point is off screen.
sihd::util::Result<Monitor> containing(int x, int y);

} // namespace sihd::sys::screen

#endif
