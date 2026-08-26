#include <sihd/sys/screenshot.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

// X11 and Wayland backends. They are compile-time options of this unix build
// (x11 / wayland scons opts); the backend implementations live in src/linux/x11/
// and src/linux/wayland/. Without either option every operation returns false.
//
// Same dispatch rule as before the split: when X11 is compiled in, a failed
// display open falls through to false without trying Wayland; the Wayland
// backends are only dispatched when X11 is not part of this build.

#include "backends.hpp"

namespace sihd::sys::screenshot
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

bool take_window_name(Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;
    if (x11.display)
    {
        Window root = DefaultRootWindow(x11.display);

        XWindowAttributes gwa;
        Window named_window = 0;
        Window root_win;
        Window parent_win;
        Window *list_win;
        unsigned int nchildren;
        if (XQueryTree(x11.display, root, &root_win, &parent_win, &list_win, &nchildren) == False)
        {
            return false;
        }

        Defer d([&list_win] { XFree(list_win); });

        bool found = false;
        for (unsigned int i = 0; i < nchildren; ++i)
        {
            Window tmp = list_win[i];

            if (XGetWindowAttributes(x11.display, tmp, &gwa) == False)
                continue;
            if (x11_is_window_readable(gwa) == false)
                continue;

            char *window_name = nullptr;
            if (XFetchName(x11.display, tmp, &window_name) == True)
            {
                if (window_name != nullptr && name == window_name)
                {
                    named_window = tmp;
                    found = true;
                }
                XFree(window_name);
            }

            if (found)
                break;
        }

        if (found)
            return x11_screenshot_window(bm, x11.display, named_window);
    }
#elif defined(SIHD_COMPILE_WITH_WAYLAND) && !defined(SIHD_COMPILE_WITH_X11)
    return wayland_screenshot_window_name(bm, name);
#endif
    return false;
}

bool take_focused(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;
    if (x11.display)
    {
        Window child;
        int revert_to_return;
        XGetInputFocus(x11.display, &child, &revert_to_return);

        if (child == 0 || child == 1)
            return false;

        return x11_screenshot_window(bm, x11.display, child);
    }
#elif defined(SIHD_COMPILE_WITH_WAYLAND) && !defined(SIHD_COMPILE_WITH_X11)
    return wayland_screenshot_focused(bm);
#endif
    return false;
}

bool take_under_cursor(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;
    if (x11.display)
    {
        Window root = DefaultRootWindow(x11.display);

        Window child, root_win;
        int root_x, root_y, win_x, win_y;
        unsigned int mask_return;
        if (!XQueryPointer(x11.display,
                           root,
                           &root_win,
                           &child,
                           &root_x,
                           &root_y,
                           &win_x,
                           &win_y,
                           &mask_return))
        {
            return false;
        }

        return x11_screenshot_window(bm, x11.display, child);
    }
#elif defined(SIHD_COMPILE_WITH_WAYLAND) && !defined(SIHD_COMPILE_WITH_X11)
    return wayland_screenshot_under_cursor(bm);
#endif
    return false;
}

bool take_screen(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;
    if (x11.display)
    {
        Window root = DefaultRootWindow(x11.display);
        return x11_screenshot_window(bm, x11.display, root);
    }
#elif defined(SIHD_COMPILE_WITH_WAYLAND) && !defined(SIHD_COMPILE_WITH_X11)
    return wayland_screenshot(bm);
#endif
    return false;
}

} // namespace sihd::sys::screenshot
