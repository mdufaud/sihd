#ifndef __SIHD_SYS_SCREEN_MATH_HPP__
#define __SIHD_SYS_SCREEN_MATH_HPP__

namespace sihd::sys::screen::internal
{

// Pixels per inch from a pixel size and its physical size in millimeters.
inline double dpi_from(int pixels, int mm)
{
    if (pixels <= 0 || mm <= 0)
        return 0.0;
    return static_cast<double>(pixels) / (static_cast<double>(mm) / 25.4);
}

} // namespace sihd::sys::screen::internal

#endif
