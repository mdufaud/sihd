#include <sihd/util/Defer.hpp>
#include <sihd/util/Logger.hpp>
#include <sihd/util/platform.hpp>
#include <sihd/util/screenshot.hpp>

#if defined(SIHD_COMPILE_WITH_X11)
# include <X11/Xlib.h>
# include <X11/Xutil.h>
#endif

#if defined(__SIHD_WINDOWS__)
# include <windows.h>
# include <wingdi.h>
# include <winuser.h>
#endif

namespace sihd::util::screenshot
{

SIHD_NEW_LOGGER("sihd::util::screenshot");

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

bool x11_is_window_readable(XWindowAttributes gwa)
{
    // SIHD_TRACEF(gwa.map_installed);
    // SIHD_TRACEF(InputOutput);
    // SIHD_TRACEF(InputOnly);
    // SIHD_TRACEF(gwa.c_class);
    // SIHD_TRACEF(gwa.x);
    // SIHD_TRACEF(gwa.y);
    // SIHD_TRACEF(gwa.width);
    // SIHD_TRACEF(gwa.height);
    // SIHD_TRACEF(gwa.backing_store);
    // SIHD_TRACEF(gwa.depth);
    // SIHD_TRACEF(gwa.root);
    // SIHD_TRACEF(gwa.map_state);
    return gwa.c_class == InputOutput && gwa.map_state == IsViewable && gwa.map_installed == 0;
}

bool x11_image_to_bitmap(Bitmap & bm, size_t width, size_t height, XImage *image)
{
    try
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

    return x11_image_to_bitmap(bm, width, height, image);
}

#endif

#if defined(__SIHD_WINDOWS__)

bool take_screen_from_window(Bitmap & bm, HWND window)
{
    bool ret = false;
    if (window)
    {
        HDC hdcScreen = GetDC(window);
        if (hdcScreen)
        {
            HDC hdcCompatible = CreateCompatibleDC(hdcScreen);
            if (hdcCompatible)
            {
                int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
                int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
                HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, nScreenWidth, nScreenHeight);
                if (hBmp)
                {
                    HGDIOBJ hOldBmp = SelectObject(hdcCompatible, hBmp);
                    BOOL bOK = BitBlt(hdcCompatible,
                                      0,
                                      0,
                                      nScreenWidth,
                                      nScreenHeight,
                                      hdcScreen,
                                      0,
                                      0,
                                      SRCCOPY | CAPTUREBLT);
                    if (bOK)
                    {
                        SelectObject(hdcCompatible, hOldBmp); // always select the previously selected object once done

                        BITMAPINFO MyBMInfo;
                        memset(&MyBMInfo, 0, sizeof(MyBMInfo));
                        MyBMInfo.bmiHeader.biSize = sizeof(MyBMInfo.bmiHeader);

                        // Get the BITMAPINFO structure from the bitmap
                        GetDIBits(hdcScreen, hBmp, 0, 0, NULL, &MyBMInfo, DIB_RGB_COLORS);

                        // create the bitmap buffer
                        char *lpPixels = new char[MyBMInfo.bmiHeader.biSizeImage];

                        MyBMInfo.bmiHeader.biCompression = BI_RGB;
                        MyBMInfo.bmiHeader.biBitCount = 24;

                        // get the actual bitmap buffer
                        GetDIBits(hdcScreen,
                                  hBmp,
                                  0,
                                  MyBMInfo.bmiHeader.biHeight,
                                  (LPVOID)lpPixels,
                                  &MyBMInfo,
                                  DIB_RGB_COLORS);

                        bm.create(nScreenWidth, nScreenHeight, 24);
                        bm.set((uint8_t *)lpPixels, MyBMInfo.bmiHeader.biSizeImage);

                        delete lpPixels;
                        ret = true;
                    }
                    DeleteObject(hBmp);
                }
                DeleteDC(hdcCompatible);
            }
        }
        ReleaseDC(window, hdcScreen);
    }
    return ret;
}

#endif

} // namespace

bool take_window_name(Bitmap & bm, [[maybe_unused]] std::string_view name)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;

    Window root = DefaultRootWindow(x11.display);

    XWindowAttributes gwa;
    Window named_window;
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

        char *window_name = NULL;
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

#elif defined(__SIHD_WINDOWS__)
    HWND active_window = FindWindowA(nullptr, name.data());
    return take_screen_from_window(bm, active_window);
#endif
    return false;
}

bool take_focused(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;

    Window child;
    int revert_to_return;
    XGetInputFocus(x11.display, &child, &revert_to_return);

    if (child == 0 || child == 1)
        return false;

    return x11_screenshot_window(bm, x11.display, child);
#elif defined(__SIHD_WINDOWS__)
    HWND foreground_window = GetForegroundWindow();
    return take_screen_from_window(bm, foreground_window);
#endif
    return false;
}

bool take_under_cursor(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;

    Window root = DefaultRootWindow(x11.display);

    Window child, root_win;
    int root_x, root_y, win_x, win_y;
    unsigned int mask_return;
    if (!XQueryPointer(x11.display, root, &root_win, &child, &root_x, &root_y, &win_x, &win_y, &mask_return))
    {
        return false;
    }

    return x11_screenshot_window(bm, x11.display, child);
#elif defined(__SIHD_WINDOWS__)
    POINT pt;
    HWND window;
    GetCursorPos(&pt);
    window = WindowFromPoint(pt);
    return take_screen_from_window(bm, window);
#endif
    return false;
}

bool take_screen(Bitmap & bm)
{
    bm.clear();
#if defined(SIHD_COMPILE_WITH_X11)
    X11Display x11;
    Window root = DefaultRootWindow(x11.display);
    return x11_screenshot_window(bm, x11.display, root);
#elif defined(__SIHD_WINDOWS__)
    HWND hDesktopWnd = GetDesktopWindow();
    return take_screen_from_window(bm, hDesktopWnd);
#endif
    return false;
}

} // namespace sihd::util::screenshot
