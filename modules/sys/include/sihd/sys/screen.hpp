#ifndef __SIHD_SYS_SCREEN_HPP__
#define __SIHD_SYS_SCREEN_HPP__

#include <string>
#include <vector>

#include <sihd/util/Result.hpp>

namespace sihd::sys::screen
{

bool supported();

// One physical display; x, y, width and height are logical desktop units (raw pixels on X11)
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

sihd::util::Result<Monitor> containing(int x, int y);

} // namespace sihd::sys::screen

#endif
