#include <cstdint>
#include <cstring>
#include <string_view>

// sihd headers first: Xlib poisons common identifiers (None, Status, True,
// False, ...) with its macros.
#include <sihd/sys/Bitmap.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

#include "screenshot.hpp"
#include "x11_display.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// X11 screenshot backend - XGetImage based captures of root, focused,
// pointed-at or named windows.

namespace sihd::sys::screenshot::x11
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

namespace
{

int count_trailing_zeros(unsigned long mask)
{
    if (mask == 0)
        return 0;
    int count = 0;
    while ((mask & 1) == 0)
    {
        mask >>= 1;
        ++count;
    }
    return count;
}

uint8_t extract_channel(unsigned long pixel, unsigned long mask)
{
    if (mask == 0)
        return 0;
    const int shift = count_trailing_zeros(mask);
    const unsigned long extracted = (pixel & mask) >> shift;
    // Normalize to 8-bit (handle masks of different sizes)
    const unsigned long mask_max = mask >> shift;
    if (mask_max == 0)
        return 0;
    return static_cast<uint8_t>((extracted * 255) / mask_max);
}

bool image_to_bitmap(Bitmap & bm, size_t width, size_t height, XImage *image)
{
    try
    {
        // Always create 32-bit bitmap for consistency
        bm.create(width, height, 32);

        const unsigned long red_mask = image->red_mask;
        const unsigned long green_mask = image->green_mask;
        const unsigned long blue_mask = image->blue_mask;

        for (size_t y = 0; y < height; y++)
        {
            for (size_t x = 0; x < width; x++)
            {
                const unsigned long pixel = XGetPixel(image, x, y);

                const uint8_t red = extract_channel(pixel, red_mask);
                const uint8_t green = extract_channel(pixel, green_mask);
                const uint8_t blue = extract_channel(pixel, blue_mask);

                bm.set(x, height - y - 1, Color::rgb(red, green, blue));
            }
        }
        return true;
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "{}", e.what());
        return false;
    }
}

/**
 * Check if window can be captured via XGetImage.
 * XGetImage will fail/abort on:
 * - InputOnly windows (no pixmap backing)
 * - Unmapped windows (not visible)
 * - Unviewable windows (obscured or minimized)
 */
bool window_readable(const XWindowAttributes & gwa)
{
    // InputOnly windows have no drawable content
    if (gwa.c_class != InputOutput)
        return false;
    // Window must be mapped and viewable
    if (gwa.map_state != IsViewable)
        return false;
    // Sanity check dimensions
    if (gwa.width <= 0 || gwa.height <= 0)
        return false;
    return true;
}

bool screenshot_window(Bitmap & bm, Display *display, Window window)
{
    if (window == 0)
        return false;

    XWindowAttributes gwa;
    if (XGetWindowAttributes(display, window, &gwa) == False)
    {
        return false;
    }

    if (!window_readable(gwa))
    {
        return false;
    }

    const int width = gwa.width;
    const int height = gwa.height;

    XImage *image = XGetImage(display, window, 0, 0, width, height, AllPlanes, ZPixmap);
    if (image == nullptr)
    {
        return false;
    }

    Defer defer_destroy_image([&] { XDestroyImage(image); });

    return image_to_bitmap(bm, width, height, image);
}

} // namespace

bool take_screen(Bitmap & bm)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return false;

    return screenshot_window(bm, conn.display, DefaultRootWindow(conn.display));
}

bool take_focused(Bitmap & bm)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return false;

    Window child;
    int revert_to_return;
    XGetInputFocus(conn.display, &child, &revert_to_return);

    // None or PointerRoot hold no usable window.
    if (child == 0 || child == 1)
        return false;

    return screenshot_window(bm, conn.display, child);
}

bool take_under_cursor(Bitmap & bm)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return false;

    Window root = DefaultRootWindow(conn.display);

    Window child, root_win;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;
    if (!XQueryPointer(conn.display, root, &root_win, &child, &root_x, &root_y, &win_x, &win_y, &mask_return))
    {
        return false;
    }

    return screenshot_window(bm, conn.display, child);
}

bool take_window_name(Bitmap & bm, std::string_view name)
{
    sihd::sys::x11::DisplayConnection conn;
    if (conn.display == nullptr)
        return false;

    Window root = DefaultRootWindow(conn.display);

    XWindowAttributes gwa;
    Window root_win;
    Window parent_win;
    Window *list_win;
    unsigned int nchildren;
    if (XQueryTree(conn.display, root, &root_win, &parent_win, &list_win, &nchildren) == False)
    {
        return false;
    }

    Defer d([&list_win] { XFree(list_win); });

    for (unsigned int i = 0; i < nchildren; ++i)
    {
        Window tmp = list_win[i];

        if (XGetWindowAttributes(conn.display, tmp, &gwa) == False)
            continue;
        if (window_readable(gwa) == false)
            continue;

        char *window_name = nullptr;
        if (XFetchName(conn.display, tmp, &window_name) == True)
        {
            const bool found = window_name != nullptr && name == window_name;
            XFree(window_name);

            if (found && screenshot_window(bm, conn.display, tmp))
                return true;
        }
    }
    return false;
}

} // namespace sihd::sys::screenshot::x11
