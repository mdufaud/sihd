#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/screenshot.hpp>

#if defined(__SIHD_LINUX__)

# if defined(SIHD_COMPILE_WITH_X11)
#  include <X11/Xlib.h>
#  include <X11/Xutil.h>
# endif

#endif

namespace sihd::util::screenshot
{

SIHD_NEW_LOGGER("sihd::util::screenshot");

namespace
{

#if defined(SIHD_COMPILE_WITH_X11)

bool check_x11_window_readable(XWindowAttributes gwa)
{
    SIHD_TRACEF(gwa.map_installed);
    SIHD_TRACEF(InputOutput);
    SIHD_TRACEF(InputOnly);
    SIHD_TRACEF(gwa.c_class);
    SIHD_TRACEF(gwa.x);
    SIHD_TRACEF(gwa.y);
    SIHD_TRACEF(gwa.width);
    SIHD_TRACEF(gwa.height);
    SIHD_TRACEF(gwa.backing_store);
    SIHD_TRACEF(gwa.depth);
    SIHD_TRACEF(gwa.root);
    SIHD_TRACEF(gwa.map_state);
    return gwa.c_class == InputOutput && gwa.map_state == IsViewable && gwa.map_installed == 0;
}

void create_x11_screenshot(Bitmap & bm, size_t width, size_t height, XImage *image)
{
    bm.create(width, height, image->bits_per_pixel);

    const unsigned long red_mask = image->red_mask;
    const unsigned long green_mask = image->green_mask;
    const unsigned long blue_mask = image->blue_mask;

    unsigned long pixel;
    unsigned char blue;
    unsigned char green;
    unsigned char red;

    for (size_t x = 0; x < width; x++)
    {
        for (size_t y = 0; y < height; y++)
        {
            pixel = XGetPixel(image, x, y);

            blue = pixel & blue_mask;
            green = (pixel & green_mask) >> 8;
            red = (pixel & red_mask) >> 16;

            bm.set(x, y, Pixel::rgb(red, green, blue));
        }
    }
}
#endif

} // namespace

bool take(Bitmap & bm, std::string_view name)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        SIHD_LOG(error, "could not open X display");
        return false;
    }
    Defer d([&display] { XCloseDisplay(display); });

    Window root = DefaultRootWindow(display);

    XWindowAttributes gwa;
    Window named_window;

    Window root_win;
    Window parent_win;
    Window *list_win;
    unsigned int nchildren;
    if (XQueryTree(display, root, &root_win, &parent_win, &list_win, &nchildren) == False)
    {
        return false;
    }

    bool found = false;
    for (unsigned int i = 0; i < nchildren; ++i)
    {
        Window tmp = list_win[i];

        if (XGetWindowAttributes(display, tmp, &gwa) == False)
        {
            continue;
        }

        if (!check_x11_window_readable(gwa))
        {
            continue;
        }

        char *window_name = NULL;
        if (XFetchName(display, tmp, &window_name) == True)
        {
            if (window_name != nullptr && name == window_name)
            {
                named_window = tmp;
                found = true;
            }
            XFree(window_name);
        }

        if (found)
        {
            break;
        }
    }

    if (!found)
    {
        return false;
    }

    const int width = gwa.width;
    const int height = gwa.height;

    XImage *image = XGetImage(display, named_window, 0, 0, width, height, AllPlanes, ZPixmap);
    if (image == nullptr)
    {
        SIHD_LOG(error, "could not get image");
        return false;
    }

    try
    {
        create_x11_screenshot(bm, width, height, image);
    }
    catch (const std::exception & e)
    {
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool take_focus(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        SIHD_LOG(error, "could not open X display");
        return false;
    }
    Defer d([&display] { XCloseDisplay(display); });

    Window root = DefaultRootWindow(display);

    Window child, root_win;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;
    if (!XQueryPointer(display, root, &root_win, &child, &root_x, &root_y, &win_x, &win_y, &mask_return))
    {
        SIHD_LOG(error, "XQueryPointer failed");
        return false;
    }

    if (child == 0)
    {
        SIHD_LOG(error, "no pointer");
    }

    // int revert_to_return;
    // XGetInputFocus(display, &child, &revert_to_return);

    if (child == 0)
    {
        SIHD_LOG(error, "no focus");
        return false;
    }

    XWindowAttributes gwa;
    if (XGetWindowAttributes(display, child, &gwa) == False)
    {
        SIHD_LOG(error, "could not open window attributes");
        return false;
    }

    if (!check_x11_window_readable(gwa))
    {
        SIHD_LOG(error, "could not window not readable");
        return false;
    }

    const int width = gwa.width;
    const int height = gwa.height;

    XImage *image = XGetImage(display, child, 0, 0, width, height, AllPlanes, ZPixmap);
    if (image == nullptr)
    {
        SIHD_LOG(error, "could not get image");
        return false;
    }

    try
    {
        create_x11_screenshot(bm, width, height, image);
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "{}", e.what());
        return false;
    }

    return true;
#else
    return false;
#endif
}

bool take_all(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        SIHD_LOG(error, "could not open X display");
        return false;
    }
    Defer d([&display] { XCloseDisplay(display); });

    Window root = DefaultRootWindow(display);

    XWindowAttributes gwa;
    if (XGetWindowAttributes(display, root, &gwa) == False)
    {
        SIHD_LOG(error, "could not open window attributes");
        return false;
    }

    if (!check_x11_window_readable(gwa))
    {
        return false;
    }

    const int width = gwa.width;
    const int height = gwa.height;

    XImage *image = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    if (image == nullptr)
    {
        SIHD_LOG(error, "could not get image");
        return false;
    }

    try
    {
        create_x11_screenshot(bm, width, height, image);
    }
    catch (const std::exception & e)
    {
        SIHD_LOG(error, "{}", e.what());
        return false;
    }

    return true;
#else
    return false;
#endif
}

} // namespace sihd::util::screenshot
