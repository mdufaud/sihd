#ifndef __SIHD_SYS_BACKENDS_HPP__
#define __SIHD_SYS_BACKENDS_HPP__

// Internal declarations shared between the unix clipboard/screenshot dispatchers
// (src/linux/*.cpp) and their backends living in src/linux/x11/ and src/linux/wayland/.
//
// X11 and Wayland are compile-time options of this unix build (the x11 / wayland
// scons opts); when an option is disabled its backend is not even compiled and the
// inline no-op fallbacks below make every operation inert.

#include <optional>
#include <string>
#include <string_view>

#include <sihd/sys/Bitmap.hpp>

#if defined(SIHD_COMPILE_WITH_X11)
# include <X11/Xlib.h>
#endif

namespace sihd::sys::clipboard
{

#if defined(SIHD_COMPILE_WITH_X11)

bool x11_set_clipboard(std::string_view str);

std::optional<std::string> x11_get_clipboard();

bool x11_set_clipboard_image(const Bitmap & bitmap);

std::optional<Bitmap> x11_get_clipboard_image();

#else

inline bool x11_set_clipboard([[maybe_unused]] std::string_view str) { return false; }
inline std::optional<std::string> x11_get_clipboard() { return std::nullopt; }
inline bool x11_set_clipboard_image([[maybe_unused]] const Bitmap & bitmap) { return false; }
inline std::optional<Bitmap> x11_get_clipboard_image() { return std::nullopt; }

#endif

#if defined(SIHD_COMPILE_WITH_WAYLAND)

bool wayland_set_clipboard(std::string_view str);

std::optional<std::string> wayland_get_clipboard();

bool wayland_set_clipboard_image(const Bitmap & bitmap);

std::optional<Bitmap> wayland_get_clipboard_image();

#else

inline bool wayland_set_clipboard([[maybe_unused]] std::string_view str) { return false; }
inline std::optional<std::string> wayland_get_clipboard() { return std::nullopt; }
inline bool wayland_set_clipboard_image([[maybe_unused]] const Bitmap & bitmap) { return false; }
inline std::optional<Bitmap> wayland_get_clipboard_image() { return std::nullopt; }

#endif

} // namespace sihd::sys::clipboard

namespace sihd::sys::screenshot
{

#if defined(SIHD_COMPILE_WITH_X11)

struct X11Display
{
        X11Display() { this->display = XOpenDisplay(nullptr); };

        ~X11Display()
        {
            if (this->display != nullptr)
                XCloseDisplay(this->display);
        };

        Display *display;
};

bool x11_is_window_readable(const XWindowAttributes & gwa);

bool x11_screenshot_window(Bitmap & bm, Display *display, Window window);

#endif

#if defined(SIHD_COMPILE_WITH_WAYLAND)

bool wayland_screenshot(Bitmap & bm, const std::string & output_name = "");

bool wayland_screenshot_focused(Bitmap & bm);

bool wayland_screenshot_under_cursor(Bitmap & bm);

bool wayland_screenshot_window_name(Bitmap & bm, std::string_view name);

#endif

} // namespace sihd::sys::screenshot

#endif
