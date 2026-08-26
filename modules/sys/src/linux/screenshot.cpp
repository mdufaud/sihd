#include <sihd/sys/screenshot.hpp>
#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>

// X11 and Wayland backends. They are compile-time options of this unix build
// (x11 / wayland scons opts); without them every operation is a no-op returning false.

#if defined(SIHD_COMPILE_WITH_X11)
# include <X11/Xlib.h>
# include <X11/Xutil.h>
#endif

#if defined(SIHD_COMPILE_WITH_WAYLAND)
# include <sihd/sys/File.hpp>
# include <sihd/sys/fs.hpp>
# include <sihd/sys/proc.hpp>
#endif

namespace sihd::sys::screenshot
{

using namespace sihd::util;

SIHD_NEW_LOGGER("sihd::sys::screenshot");

namespace
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

/**
 * Check if window can be captured via XGetImage.
 * XGetImage will fail/abort on:
 * - InputOnly windows (no pixmap backing)
 * - Unmapped windows (not visible)
 * - Unviewable windows (obscured or minimized)
 */
bool x11_is_window_readable(const XWindowAttributes & gwa)
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

bool x11_image_to_bitmap(Bitmap & bm, size_t width, size_t height, XImage *image)
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

                bm.set(x, height - y - 1, Pixel::rgb(red, green, blue));
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

bool x11_screenshot_window(Bitmap & bm, Display *display, Window window)
{
    if (window == 0)
        return false;

    XWindowAttributes gwa;
    if (XGetWindowAttributes(display, window, &gwa) == False)
    {
        return false;
    }

    if (!x11_is_window_readable(gwa))
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

    return x11_image_to_bitmap(bm, width, height, image);
}

#endif

#if defined(SIHD_COMPILE_WITH_WAYLAND) && !defined(SIHD_COMPILE_WITH_X11)

bool wayland_screenshot(Bitmap & bm, const std::string & output_name = "")
{
    proc::Options options;
    options.timeout = std::chrono::seconds(5);
    options.close_stderr = true;
    std::string tmp_path = fs::tmp_path() + "/sihd_screenshot.bmp";

    // Try different screenshot tools in order of preference
    std::vector<std::vector<std::string>> tools;

    // grim (wlroots compositors: Sway, etc.)
    {
        std::vector<std::string> args;
        args.emplace_back("grim");
        args.emplace_back("-t");
        args.emplace_back("bmp");
        if (!output_name.empty())
        {
            args.emplace_back("-o");
            args.emplace_back(output_name);
        }
        args.emplace_back(tmp_path);
        tools.push_back(args);
    }

    // spectacle (KDE Plasma)
    {
        std::vector<std::string> args = {"spectacle", "-b", "-n", "-f", "-o", tmp_path};
        tools.push_back(args);
    }

    // gnome-screenshot (GNOME)
    {
        std::vector<std::string> args = {"gnome-screenshot", "-f", tmp_path};
        tools.push_back(args);
    }

    for (const auto & args : tools)
    {
        auto exit_code_future = proc::execute(args, options);
        auto status = exit_code_future.wait_for(std::chrono::seconds(5));

        if (status == std::future_status::timeout || exit_code_future.get() != 0)
            continue;

        if (fs::is_file(tmp_path))
        {
            Defer defer_cleanup([&] { fs::remove_file(tmp_path); });
            if (bm.read_bmp(tmp_path))
                return true;
        }
    }

    SIHD_LOG(error, "No working screenshot tool found (tried: grim, spectacle, gnome-screenshot)");
    return false;
}

bool wayland_screenshot_focused(Bitmap & bm)
{
    proc::Options options;
    options.timeout = std::chrono::seconds(5);
    options.close_stderr = true;

    std::string tmp_path = fs::tmp_path() + "/sihd_screenshot.bmp";

    // Try tools that support active window capture
    std::vector<std::vector<std::string>> tools = {
        // spectacle (KDE) - active window
        {"spectacle", "-b", "-n", "-a", "-o", tmp_path},
        // gnome-screenshot - current window
        {"gnome-screenshot", "-w", "-f", tmp_path},
    };

    for (const auto & args : tools)
    {
        auto exit_code_future = proc::execute(args, options);
        auto status = exit_code_future.wait_for(std::chrono::seconds(5));

        if (status == std::future_status::timeout || exit_code_future.get() != 0)
            continue;

        if (fs::is_file(tmp_path))
        {
            Defer defer_cleanup([&] { fs::remove_file(tmp_path); });
            if (bm.read_bmp(tmp_path))
                return true;
        }
    }

    SIHD_LOG(warning, "Wayland: focused window capture not supported, falling back to full screen");
    return wayland_screenshot(bm);
}

bool wayland_screenshot_under_cursor(Bitmap & bm)
{
    // spectacle supports interactive region selection with -r but not "under cursor" specifically
    SIHD_LOG(warning, "Wayland: under cursor capture not supported, falling back to full screen");
    return wayland_screenshot(bm);
}

bool wayland_screenshot_window_name([[maybe_unused]] Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    // Wayland doesn't expose window list to clients for security
    SIHD_LOG(error, "Wayland: window name capture not supported");
    return false;
}

#endif

} // namespace

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
